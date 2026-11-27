// Native scene interpolation; included inside the frame pacing namespace.

bool native_scene_ready() {
    const uint32_t system=vm_read32(CV_SCENE_SYSTEM);
    // Match the active character's live-room signature, as SRT and save tools do.
    // Claire (including alternate outfit), Chris, Steve and Battle Game Wesker.
    const unsigned character=vm_read8(CV_SRT_CHARACTER);
    static constexpr uint32_t live_markers[]={0x3D650,0x3C4FC,0x3A4D0,0x3E814};
    if(!system || character>=4 || vm_read32(CV_SRT_RDX)!=live_markers[character] || int32_t(vm_read32(CV_SRT_HEALTH))<=0)return false;
    // Game Over ("YOU DIED" / "Retry? Yes/No", system+0x2A6D6/+0x2A6D8) can run
    // with HP > 0 (e.g. instant-death fall off the Antarctica helipad), and
    // room/disc-swap monitor (system+0x1B108 == 4) also draws 2D prompts.
    // Rebuilding the 3D scene via 000155D0 would free their 2D text/sprite lists.
    if(vm_read16(system+0x2A6D6)!=0 || vm_read32(system+0x2A6D8)!=0 || vm_read8(system+0x1B108)==4)return false;
    // The game's 2D screen fader (fade_an at +0x2A5D8, 0..255): keep the native
    // frame whenever the screen is fading/black so the extra 3D pass never draws
    // over a cutscene-skip black fade and exposes character model swaps.
    if(read_float(system+0x2A5D8u)>0.5f)return false;
    const uint32_t tasks=vm_read32(system+0x64)&~vm_read32(system+0x68);
    const uint32_t camera_mode=vm_read32(system+0x74),control=vm_read32(system+0x70);
    // Script command 0012C864 mode 3 sets 0x44 and control bit 4. The
    // connecting staircase uses 0x44/0x46 while the same room is drawable.
    // The recorded in-room event uses 0x204 with the same script control bit;
    // its ordinary scene path remains drawable (no special-render flags).
    // 0x2004 is the scripted furniture sequence; it uses the same scene path.
    // Pure in-room scripted event ((camera_mode & 7) == 4, no fade/load) when
    // not in a skippable story cutscene (+78 bit 0x10000000): allow any room
    // upper camera bits (0x40, 0x100, 0x200, 0x1000, 0x2000...) so unskippable
    // camera pans/mechanisms without dialogue interpolate at 60 FPS everywhere.
    const bool skippable_cutscene=(vm_read32(system+0x78)&0x10000000u)!=0||(camera_mode&0x10000000u)!=0;
    const bool scripted_camera=!skippable_cutscene&&((camera_mode&7)==4||camera_mode==0x46)&&(control&4);
    // 001AF3E0/00146F90 call message update/draw routine 001611C8 for bit 9.
    // Scene preparation would free its text lists. Keep the original frame
    // until the message closes, without repeating its input/timer logic.
    // Sniper Rifle / Linear Launcher scope view: control 0x00800000 and
    // 6C 0x000800C0 are set only while the scope is up (player state 0x47).
    // Its overlay is not rebuilt by the extra scene pass; keep original frames.
    const bool scope_view=(control&0x00800000)||(vm_read32(system+0x6C)&0x000800C0);
    // 6C bits 0x100/0x200 mark rooms with special render paths and stay
    // excluded, except in one room: rm_1050 (stage 1, room 5, any variant),
    // where the alarm keeps 0x200 set while it flashes and the whole room fell
    // back to 30 Hz. Only 0x200 is let through there; every other guard above
    // (Game Over, monitor, scope, messages, loads) still applies.
    const bool alarm_room=vm_read8(CV_SRT_ROOM)==1&&vm_read8(CV_SRT_ROOM+1)==5;
    const uint32_t special_render=alarm_room?0x100u:0x300u;
    return (tasks&0x80)&&!(tasks&0x1803F)&&(!(camera_mode&7)||scripted_camera)&&
        !(control&0x1C040308)&&!(vm_read32(system+0x6C)&special_render)&&!scope_view;
}
// Room numbers and camera IDs can be reused after loading. Match the actual
// scene resources as well, and never carry render endpoints across a transition.
static constexpr unsigned scene_words=6;
bool scene_read(uint32_t* scene) {
    scene[0]=(uint32_t(vm_read8(CV_SRT_ROOM))<<16)|(uint32_t(vm_read8(CV_SRT_ROOM+1))<<8)|vm_read8(CV_SRT_ROOM+2);
    scene[1]=vm_read32(CV_SCENE_SYSTEM);scene[2]=vm_read32(CV_SCENE_CONFIG);scene[3]=vm_read32(CV_SCENE_RENDER);
    const uint32_t manager=vm_read32(CV_SCENE_ROOM_MANAGER);
    scene[4]=manager?vm_read32(manager+0x60):0;
    // Never blend across a character switch even if actor/model storage is reused.
    scene[5]=vm_read8(CV_SRT_CHARACTER);
    return scene[1]&&scene[2]&&scene[3]&&scene[4];
}
bool scene_matches(const uint32_t* scene) {
    uint32_t current[scene_words];
    return scene_read(current)&&std::memcmp(current,scene,sizeof current)==0;
}
static uint32_t scene_previous[10],scene_cut[3],scene_scene[scene_words];
static bool scene_previous_valid=false,scene_scripted=false;
// Scene preparation may retain camera pointers until the later draw submission.
// Restore gameplay camera products only when the next BioUpdate begins.
static uint32_t scene_derived[5],scene_derived_saved[5][16];
static constexpr unsigned scene_derived_words[]={3,3,16,16,16};
static bool scene_derived_pending=false;
static bool scene_cut_view_valid=false;
static uint32_t scene_cut_view[16];
// Type-0 matrix palettes shared by Claire, attachments and normal enemies.
struct ProbeModel {
    uint32_t actor=0, model=0, root=0, palette=0, count=0, parent_bone=0, identity=0, instance=0;
    float position[3]{}, root_matrix[16]{}, bones[64][16]{};
};
struct ProbePose {
    bool scripted=false, camera_cut=false;
    uint32_t camera[10]{}, cut[3]{}, scene[scene_words]{}, count=0;
    ProbeModel models[5]{}; // Claire and at most four direct attachments.
    uint32_t enemy_count=0;
    ProbeModel enemies[128]{}; // Original actor pool includes enemy accessories.
    uint32_t object_count=0;
    ProbeModel objects[128]{}; // At most 64 entries in each original object pool.
};
static ProbePose scene_pose_previous,scene_pose_current,scene_pose_saved;
static bool scene_pose_have=false,scene_pose_pair=false,scene_pose_pending=false;
void scene_model_write(const ProbeModel& current,const ProbeModel& previous,float alpha) {
    const auto mix=[&](float from,float to) {return &current==&previous?to:from+(to-from)*alpha;};
    for(unsigned i=0;i<3;i++)write_float(current.actor+0x10+4*i,
        mix(previous.position[i],current.position[i]));
    for(unsigned i=0;i<16;i++)write_float(current.root+4*i,
        mix(previous.root_matrix[i],current.root_matrix[i]));
    for(unsigned b=0;b<current.count;b++)for(unsigned i=0;i<16;i++)
        write_float(current.palette+b*0x48+8+4*i,
            mix(previous.bones[b][i],current.bones[b][i]));
}
void scene_pose_restore() {
    if(!scene_pose_pending)return;
    for(unsigned m=0;m<scene_pose_saved.count;m++) {
        const auto& p=scene_pose_saved.models[m];scene_model_write(p,p,1.0f);
    }
    for(unsigned m=0;m<scene_pose_saved.enemy_count;m++) {
        const auto& p=scene_pose_saved.enemies[m];scene_model_write(p,p,1.0f);
    }
    for(unsigned m=0;m<scene_pose_saved.object_count;m++) {
        const auto& p=scene_pose_saved.objects[m];scene_model_write(p,p,1.0f);
    }
    scene_pose_pending=false;
}
void native_scene_restore() {
    scene_cut_view_valid=false;
    scene_pose_restore();
    if(!scene_derived_pending)return;
    for(unsigned i=0;i<5;i++)if(scene_derived[i])
        for(unsigned j=0;j<scene_derived_words[i];j++)
            vm_write32(scene_derived[i]+4*j,scene_derived_saved[i][j]);
    scene_derived_pending=false;
}
static constexpr uint32_t scene_fields[]={0xC,0x10,0x14,0x48,0x4C,0x50,0x6C,0x54,0x58,0x5C};
bool scene_camera_matches(const uint32_t* cut) {
    return vm_read32((CV_SCENE_CAMERA+0x4))==cut[0]&&vm_read32((CV_SCENE_CAMERA+0x8C))==cut[1]&&vm_read32((CV_SCENE_CAMERA+0xA4))==cut[2];
}
#include "cv_inspection_cut.h"
// CV_SMOOTH_LOG=1: one line whenever the camera/player state changes.
void native_scene_log() {
    static const bool on=[] {const char* e=std::getenv("CV_SMOOTH_LOG");return e&&std::strcmp(e,"1")==0;}();
    if(!on)return;
    const uint32_t system=vm_read32(CV_SCENE_SYSTEM),actor=vm_read32(CV_SCENE_ACTOR);
    if(!system||!actor)return;
    uint32_t now[10]={vm_read32(system+0x64),vm_read32(system+0x68),vm_read32(system+0x6C),vm_read32(system+0x70),
        vm_read32(system+0x74),vm_read32(actor+0xC),vm_read32(actor+0x8C),vm_read32(CV_SCENE_CAMERA+0x4),
        vm_read32(CV_SCENE_CAMERA+0x8C),vm_read32(CV_SCENE_CAMERA+0xA4)};
    static uint32_t last[10];
    if(std::memcmp(now,last,sizeof now)==0)return;
    std::memcpy(last,now,sizeof now);
    // Own file, flushed per line: the game's stderr is not always captured.
    static std::FILE* file=std::fopen("smooth-log.txt","w");
    if(!file)return;
    std::fprintf(file,"[SMOOTH-LOG] t=%ld ready=%d room=%02X%02X%02X",(long)std::clock(),native_scene_ready()?1:0,
        vm_read8(CV_SRT_ROOM),vm_read8(CV_SRT_ROOM+1),vm_read8(CV_SRT_ROOM+2));
    std::fprintf(file," tasks=%08X/%08X 6C=%08X ctl=%08X cam=%08X actor_c=%08X actor_8c=%08X cut=%08X/%08X/%08X\n",
        now[0],now[1],now[2],now[3],now[4],now[5],now[6],now[7],now[8],now[9]);
    std::fflush(file);
}
// CV_EVENT_LOG=1 (dev only): structured diagnostics for mapping unskippable transitions (lifts, submarine, etc.).
struct EventDoorRec {
    uint32_t kind,word;uint8_t entry;
    bool operator==(const EventDoorRec& o) const {return kind==o.kind&&word==o.word&&entry==o.entry;}
};
struct EventLogState {
    std::FILE* file=nullptr;
    std::FILE* marks=nullptr;
    std::chrono::steady_clock::time_point t_start{};
    bool started=false;
    unsigned mark_count=0;
    // Active span tracking (while +74 & 7 != 0)
    bool busy=false,span_marked=false,span_menu=false;
    std::chrono::steady_clock::time_point span_t0{};
    uint32_t span_room_in=0,span_f74_seen=0,span_f70_seen=0,span_floor_val=0xFFFFFFFFu;
    uint8_t span_char_in=0,span_entry_in=0;
    float span_pos_in[3]={};
    std::vector<uint32_t> snap;
    std::vector<EventDoorRec> span_doors;
    // Last completed span (so pressing F3 right after a scene still captures its summary)
    std::chrono::steady_clock::time_point last_span_end{};
    bool has_last_span=false;
    std::string last_summary,last_flags;
    double elapsed_s() {
        const auto now=std::chrono::steady_clock::now();
        if(!started){t_start=now;started=true;return 0.0;}
        return std::chrono::duration<double>(now-t_start).count();
    }
    static const char* char_name(unsigned c) {
        static constexpr const char* k[]={"Claire","Chris","Steve","Wesker"};
        return c<4?k[c]:"?";
    }
    static void fmt_room(uint32_t rm,char* out,size_t n) {
        std::snprintf(out,n,"rm_%X%02X v%u",(rm>>16)&0xFF,(rm>>8)&0xFF,rm&0xFF);
    }
    static void fmt_wall_time(char* out,size_t n) {
        std::time_t now=std::time(nullptr);std::tm tm{};
#ifdef _WIN32
        localtime_s(&tm,&now);
#else
        localtime_r(&now,&tm);
#endif
        std::strftime(out,n,"%Y-%m-%d %H:%M:%S",&tm);
    }
    std::FILE* ensure_file() {
        if(!file) {
            file=std::fopen("event-log.txt","w");
            if(file) {
                char ts[32];fmt_wall_time(ts,sizeof ts);
                std::fprintf(file,"# [EVENT-LOG] title=%s skip_events=%d started=%s\n",
                    CV_TITLE_ID,cv_options::enabled("CV_SKIP_EVENTS")?1:0,ts);
                std::fprintf(file,"# Pressione F3 durante o jogo para marcar uma cena (grava resumo em event-marks.txt).\n");
                std::fflush(file);
            }
        }
        return file;
    }
    std::FILE* ensure_marks() {
        if(!marks) {
            marks=std::fopen("event-marks.txt","w");
            if(marks) {
                char ts[32];fmt_wall_time(ts,sizeof ts);
                std::fprintf(marks,"# [EVENT-MARKS] title=%s skip_events=%d started=%s\n",
                    CV_TITLE_ID,cv_options::enabled("CV_SKIP_EVENTS")?1:0,ts);
                std::fflush(marks);
            }
        }
        return marks;
    }
};
EventLogState& event_log_state() {static EventLogState s;return s;}
bool event_log_enabled() {
    static const bool on=[] {const char* e=std::getenv("CV_EVENT_LOG");return e&&std::strcmp(e,"1")==0;}();
    return on;
}
void native_event_log_note(const char* text) {
    if(!event_log_enabled()||!text||!*text)return;
    auto& st=event_log_state();
    if(std::FILE* f=st.ensure_file()) {
        std::fprintf(f,"[NOTE t=%.2fs] %s\n",st.elapsed_s(),text);
        std::fflush(f);
    }
    if(st.span_marked&&st.marks) {
        std::fprintf(st.marks,"  [NOTE t=%.2fs] %s\n",st.elapsed_s(),text);
        std::fflush(st.marks);
    }
}
void native_event_mark_poll(uint32_t system,uint32_t room) {
#ifdef _WIN32
    static bool was_down=false;
    const bool down=(GetAsyncKeyState(VK_F3)&0x8000)!=0;
    const bool edge=down&&!was_down;
    was_down=down;
    if(!edge)return;
    HWND fg=GetForegroundWindow();DWORD pid=0;
    if(fg)GetWindowThreadProcessId(fg,&pid);
    if(pid!=GetCurrentProcessId())return;
    auto& st=event_log_state();
    std::FILE* f=st.ensure_file();
    std::FILE* mf=st.ensure_marks();
    const unsigned id=++st.mark_count;
    const double t=st.elapsed_s();
    const unsigned chr=vm_read8(CV_SRT_CHARACTER);
    const uint8_t entry=vm_read8(system+0x97);
    const uint32_t f50=vm_read32(system+0x50),f64=vm_read32(system+0x64),f68=vm_read32(system+0x68);
    const uint32_t f70=vm_read32(system+0x70),f74=vm_read32(system+0x74);
    const uint32_t d0=vm_read32(system+0x2A6A0),d1=vm_read32(system+0x2A6A4),d2=vm_read32(system+0x2A6A8);
    const uint32_t actor=vm_read32(CV_SCENE_ACTOR);
    const float px=actor?read_float(actor+0x10):0.0f,py=actor?read_float(actor+0x14):0.0f,pz=actor?read_float(actor+0x18):0.0f;
    char rbuf[32];EventLogState::fmt_room(room,rbuf,sizeof rbuf);
    char line[512];
    std::snprintf(line,sizeof line,
        "[USER-MARK #%u] t=%.2fs %s (entry=%u) char=%s in_span=%d pos=(%.1f,%.1f,%.1f) +50=%08X +64=%08X +68=%08X +70=%08X +74=%08X door=%08X/%08X/%08X",
        id,t,rbuf,unsigned(entry),EventLogState::char_name(chr),st.busy?1:0,px,py,pz,f50,f64,f68,f70,f74,d0,d1,d2);
    if(f){std::fprintf(f,"%s\n",line);std::fflush(f);}
    if(mf) {
        std::fprintf(mf,"\n%s\n",line);
        if(st.busy) {
            st.span_marked=true;
            std::fprintf(mf,"  (cena em andamento — o resumo sera anexado assim que ela terminar)\n");
        } else if(st.has_last_span) {
            const double ago=std::chrono::duration<double>(std::chrono::steady_clock::now()-st.last_span_end).count();
            if(ago<=20.0) {
                std::fprintf(mf,"  (ultima cena encerrada ha %.1fs):\n  %s\n  %s\n",ago,st.last_summary.c_str(),st.last_flags.c_str());
            }
        }
        // Dump active enemies' interpolation eligibility (type +8C, model, root, palette, bone count).
        const uint32_t ecount=vm_read32(system+0x1B140);
        if(ecount&&ecount<=128) {
            for(unsigned i=0;i<ecount;i++) {
                const uint32_t e=CV_SRT_ENEMIES+i*0x578;
                const uint32_t fl=vm_read32(e);
                if((fl&0x80000001u)!=1)continue;
                const uint32_t eid=vm_read32(e+4),t8c=vm_read32(e+0x8C),mdl=vm_read32(e+0x2D8),rt=vm_read32(e+0x340);
                const uint32_t pal=mdl?vm_read32(mdl+0x14):0,bones=mdl?vm_read32(mdl+4):0;
                const char* why=t8c!=0?"skip:8C!=0":(!mdl?"skip:no_model":(!rt?"skip:no_root":(!pal?"skip:no_palette":(bones==0?"skip:bones==0":(bones>64?"skip:bones>64":"OK")))));
                std::fprintf(mf,"  [ENEMY #%u] id=%08X fl=%08X +8C=%08X model=%08X root=%08X pal=%08X bones=%u pos=(%.1f,%.1f,%.1f) -> %s\n",
                    i,eid,fl,t8c,mdl,rt,pal,bones,read_float(e+0x10),read_float(e+0x14),read_float(e+0x18),why);
            }
        }
        // Dump 3D inventory render lists (0x100, 0x114, 0x128, 0x13C) when in menu to inspect translucent items.
        if((f64&~f68)&0x200u) {
            const uint32_t obj=vm_read32(CV_SCENE_RENDER);
            if(obj) {
                static constexpr uint32_t offs[]={0x100,0x114,0x128,0x13C};
                std::fprintf(mf,"  [INV-3D] active=%d paired=%d item=%u obj=%08X mode=%d\n",
                    cv_item_interpolation::active()?1:0,cv_item_interpolation::paired?1:0,
                    vm_read32(CV_SCENE_INVENTORY+0x54),obj,int(vm_read32(obj+0x14C)));
                for(uint32_t off:offs) {
                    const uint32_t cnt=vm_read32(obj+off),arr=vm_read32(obj+off+0xC);
                    if(!cnt||cnt>128||!arr)continue;
                    std::fprintf(mf,"  [INV-LIST +%03X] cnt=%u:",off,cnt);
                    for(unsigned i=0;i<cnt&&i<16;i++) {
                        const uint32_t cmd=vm_read32(arr+4*i);
                        const uint32_t vt=cmd?vm_read32(cmd):0,mdl=cmd?vm_read32(cmd+0x30):0,vc=cmd?vm_read32(cmd+0x10):0;
                        std::fprintf(mf," [%u:vt=%08X%s mdl=%08X vc=%u]",i,vt,vt==CV_SCENE_MESH_VTABLE?"(MESH)":"",mdl,vc);
                    }
                    std::fputc('\n',mf);
                }
            }
        }
        std::fflush(mf);
    }
    wchar_t toast[96];
    swprintf(toast,96,L"Cena marcada #%u (rm_%X%02X v%u)",id,(room>>16)&0xFF,(room>>8)&0xFF,room&0xFF);
    perf_overlay_toast(toast,1);
#else
    (void)system;(void)room;
#endif
}
void native_event_log() {
    if(!event_log_enabled())return;
    const uint32_t system=vm_read32(CV_SCENE_SYSTEM);
    if(!system)return;
    const uint32_t room_cur=(vm_read8(CV_SRT_ROOM)<<16)|(vm_read8(CV_SRT_ROOM+1)<<8)|vm_read8(CV_SRT_ROOM+2);
    native_event_mark_poll(system,room_cur);
    auto& st=event_log_state();
    // Track live span metadata even on frames where the watched state words don't change.
    const uint32_t f70_cur=vm_read32(system+0x70),f74_cur=vm_read32(system+0x74);
    if(st.busy) {
        st.span_f70_seen|=f70_cur;
        st.span_f74_seen|=f74_cur;
        if(f70_cur&0x00080000u)st.span_menu=true;
        if(st.span_menu&&(f74_cur&0x1000u))st.span_floor_val=vm_read32(system+0x289DC);
        const uint32_t dk=vm_read32(system+0x2A6A4),dw=vm_read32(system+0x2A6A8);
        if(dw) {
            EventDoorRec rec{dk,dw,vm_read8(system+0x97)};
            if(st.span_doors.empty()||!(st.span_doors.back()==rec)) {
                if(st.span_doors.size()<8)st.span_doors.push_back(rec);
            }
        }
    }
    static constexpr uint32_t watch[]={0x50,0x64,0x68,0x6C,0x70,0x74,0x78,0x7C,0x94,0x98,0x2A6A0,0x2A6A4,0x2A6A8,
        0xB4,0x270,0x28C,0x8A0,0xA4,0xB8,0xBC,0xCC,0xD0,0x13C,0x26C,0x274};
    uint32_t now[sizeof watch/sizeof watch[0]+1];
    for(unsigned i=0;i<sizeof watch/sizeof watch[0];i++)now[i]=vm_read32(system+watch[i]);
    now[sizeof watch/sizeof watch[0]]=room_cur;
    static uint32_t last[sizeof now/sizeof now[0]];
    if(std::memcmp(now,last,sizeof now)==0)return;
    std::memcpy(last,now,sizeof now);
    std::FILE* file=st.ensure_file();
    if(!file)return;
    const double t_s=st.elapsed_s();
    char rbuf[32];EventLogState::fmt_room(room_cur,rbuf,sizeof rbuf);
    std::fprintf(file,"t=%.2fs %s chr=%u(%s)",t_s,rbuf,unsigned(vm_read8(CV_SRT_CHARACTER)),EventLogState::char_name(vm_read8(CV_SRT_CHARACTER)));
    for(uint32_t off=0x40;off<0xA0;off+=4)std::fprintf(file," %02X=%08X",off,vm_read32(system+off));
    // The door request the room change reads (written by the warp function at +0x2A6A0..+0x2A6AB).
    std::fprintf(file," door=%08X/%08X/%08X",vm_read32(system+0x2A6A0),vm_read32(system+0x2A6A4),vm_read32(system+0x2A6A8));
    std::fprintf(file," A4=%08X B4=%08X B8=%08X BC=%08X CC=%08X D0=%08X 13C=%08X 26C=%08X 270=%08X 274=%08X 28C=%08X 8A0=%08X",
        vm_read32(system+0xA4),vm_read32(system+0xB4),vm_read32(system+0xB8),vm_read32(system+0xBC),
        vm_read32(system+0xCC),vm_read32(system+0xD0),vm_read32(system+0x13C),vm_read32(system+0x26C),
        vm_read32(system+0x270),vm_read32(system+0x274),vm_read32(system+0x28C),vm_read32(system+0x8A0));
    const uint32_t actor=vm_read32(CV_SCENE_ACTOR);
    if(actor)std::fprintf(file," pos=%.1f,%.1f,%.1f rot=%08X",read_float(actor+0x10),read_float(actor+0x14),read_float(actor+0x18),
        vm_read32(actor+0x1C));
    std::fputc('\n',file);
    // Busy span (74 & 7: script, fade, room change): snapshot the system block when it starts, summarize & diff when it ends.
    static constexpr uint32_t span_words=0x2B000/4;   // the whole state block (live_character maps up to +0x2ACA4)
    const bool busy_now=(now[5]&7)!=0;
    if(busy_now&&!st.busy) {
        st.snap.resize(span_words);
        for(uint32_t i=0;i<span_words;i++)st.snap[i]=vm_read32(system+4*i);
        st.span_t0=std::chrono::steady_clock::now();
        st.span_room_in=room_cur;
        st.span_char_in=vm_read8(CV_SRT_CHARACTER);
        st.span_entry_in=vm_read8(system+0x97);
        st.span_f70_seen=f70_cur;
        st.span_f74_seen=f74_cur;
        st.span_menu=(f70_cur&0x00080000u)!=0;
        st.span_floor_val=0xFFFFFFFFu;
        st.span_marked=false;
        st.span_doors.clear();
        if(actor){st.span_pos_in[0]=read_float(actor+0x10);st.span_pos_in[1]=read_float(actor+0x14);st.span_pos_in[2]=read_float(actor+0x18);}
        else {st.span_pos_in[0]=st.span_pos_in[1]=st.span_pos_in[2]=0.0f;}
        std::fprintf(file,"[SPAN-BEGIN] t=%.2fs %s system=%08X\n",t_s,rbuf,system);
    } else if(!busy_now&&st.busy&&!st.snap.empty()) {
        const auto t_end=std::chrono::steady_clock::now();
        const double dur=std::chrono::duration<double>(t_end-st.span_t0).count();
        char rin[32],rout[32];
        EventLogState::fmt_room(st.span_room_in,rin,sizeof rin);
        EventLogState::fmt_room(room_cur,rout,sizeof rout);
        const uint8_t entry_out=vm_read8(system+0x97);
        const float px=actor?read_float(actor+0x10):0.0f,py=actor?read_float(actor+0x14):0.0f,pz=actor?read_float(actor+0x18):0.0f;
        // 1) Persistent save/event flags diff (+0x9C .. +0x8C0, excluding +0x8B0 play-time counter)
        std::string flags_line="[SCENE-FLAGS]";
        unsigned flag_changes=0;
        for(uint32_t off=0x9C;off<0x8C0;off+=4) {
            if(off==0x8B0)continue;   // play-time ticks every frame
            const uint32_t before=st.snap[off/4],after=vm_read32(system+off);
            if(before==after)continue;
            const uint32_t set_bits=after&~before,clr_bits=before&~after;
            char item[96];
            if(set_bits&&clr_bits)std::snprintf(item,sizeof item," +%03X:%08X>%08X(+%08X/-%08X)",off,before,after,set_bits,clr_bits);
            else if(set_bits)std::snprintf(item,sizeof item," +%03X:%08X>%08X(+%08X)",off,before,after,set_bits);
            else std::snprintf(item,sizeof item," +%03X:%08X>%08X(-%08X)",off,before,after,clr_bits);
            if(flag_changes++<40)flags_line+=item;
        }
        if(flag_changes==0)flags_line+=" (nenhuma flag persistente alterada em +9C..+8C0)";
        else if(flag_changes>40){char tail[32];std::snprintf(tail,sizeof tail," ...(%u total)",flag_changes);flags_line+=tail;}
        // 2) Decoded door requests seen during the span (ready for call_door(ctx, kind, area, room, variant, anim))
        std::string doors_str;
        for(const auto& d:st.span_doors) {
            char dbuf[80];
            std::snprintf(dbuf,sizeof dbuf,"%skind=%02X rm_%X%02X v%u entry=%u anim=%02X",
                doors_str.empty()?"":"; ",d.kind,(d.word>>24)&0xFF,(d.word>>16)&0xFF,(d.word>>8)&0xFF,unsigned(d.entry),d.word&0xFF);
            doors_str+=dbuf;
        }
        char sum_buf[1024];
        char floor_part[40]="";
        if(st.span_floor_val!=0xFFFFFFFFu)std::snprintf(floor_part,sizeof floor_part," floor=%u",st.span_floor_val);
        std::snprintf(sum_buf,sizeof sum_buf,
            "[SCENE-SUMMARY] t=%.2fs dur=%.2fs char=%s %s -> %s (entry=%u) f74_seen=%08X f70_seen=%08X menu=%d%s pos=(%.1f,%.1f,%.1f)->(%.1f,%.1f,%.1f) doors=[%s]",
            t_s,dur,EventLogState::char_name(vm_read8(CV_SRT_CHARACTER)),rin,rout,unsigned(entry_out),
            st.span_f74_seen,st.span_f70_seen,st.span_menu?1:0,floor_part,
            st.span_pos_in[0],st.span_pos_in[1],st.span_pos_in[2],px,py,pz,doors_str.c_str());
        std::fprintf(file,"%s\n%s\n",sum_buf,flags_line.c_str());
        // Save last span for post-scene F3 marks, and flush to event-marks.txt if marked during span.
        st.last_span_end=t_end;st.has_last_span=true;
        st.last_summary=sum_buf;st.last_flags=flags_line;
        if(st.span_marked&&st.marks) {
            std::fprintf(st.marks,"  %s\n  %s\n",sum_buf,flags_line.c_str());
            std::fflush(st.marks);
        }
        st.span_marked=false;
        // 3) Remaining system-block diff (+0x8C0..+0x2B000) for auxiliary state like floor menus (+289DC)
        unsigned changed=0;
        std::fprintf(file,"[SPAN-END] other changed words:");
        for(uint32_t i=0x8C0/4;i<span_words;i++) {
            const uint32_t v=vm_read32(system+4*i);
            if(v!=st.snap[i]&&changed++<200)std::fprintf(file," %05X:%08X>%08X",4*i,st.snap[i],v);
        }
        std::fprintf(file," (total %u)\n",changed);
    }
    st.busy=busy_now;
    // Elevator menu: snapshot guest 0x00800000..0x00C00000 when the menu opens (70 & 0x80000), diff when the choice is
    // confirmed (74 & 0x1000), to find where the chosen floor is kept.
    static constexpr uint32_t wide_base=0x00800000,wide_words=0x00400000/4;
    static std::vector<uint32_t> wide;
    static bool menu=false,chosen=false;
    const bool menu_now=(now[4]&0x00080000)!=0,chosen_now=(now[5]&0x1000)!=0;
    if(menu_now&&!menu) {
        wide.resize(wide_words);
        for(uint32_t i=0;i<wide_words;i++)wide[i]=vm_read32(wide_base+4*i);
        std::fprintf(file,"[MENU-OPEN] t=%.2fs\n",t_s);
    }
    if(chosen_now&&!chosen&&!wide.empty()) {
        unsigned changed=0;
        std::fprintf(file,"[MENU-CHOICE] t=%.2fs changed words:",t_s);
        for(uint32_t i=0;i<wide_words;i++) {
            const uint32_t v=vm_read32(wide_base+4*i);
            if(v!=wide[i]&&changed++<600)std::fprintf(file," %08X:%08X>%08X",wide_base+4*i,wide[i],v);
        }
        std::fprintf(file," (total %u)\n",changed);
        wide.clear();
    }
    menu=menu_now;chosen=chosen_now;
    // Whole-guest memory scan (opt-in via CV_SCAN=1).
    static const bool scan=[] {const char* e=std::getenv("CV_SCAN");return e&&std::strcmp(e,"1")==0;}();
    if(scan) {
        static int phase=0;static std::clock_t t0=0;
        const std::clock_t t=std::clock();const uint32_t f70=now[4];
        const bool here=room_cur==0x031700u&&(now[5]&7);
        if(!here&&phase>0&&phase<5){phase=0;cv_scan_free();}
        if(here) {
            if(phase==0&&(f70&0x01000000u)&&!(f70&0x200u)){phase=1;t0=t;std::fprintf(file,"[SCAN] ride start\n");}
            else if(phase==1&&t-t0>=CLOCKS_PER_SEC){cv_scan_snapshot(0);phase=2;std::fprintf(file,"[SCAN] snapshot 0\n");}
            else if(phase==2&&t-t0>=3*CLOCKS_PER_SEC&&!(f70&0x200u)){cv_scan_snapshot(1);phase=3;std::fprintf(file,"[SCAN] snapshot 1\n");}
            else if(phase==3&&(f70&0x200u)){cv_scan_report(file);cv_scan_free();phase=4;t0=t;}
            else if(phase==4&&t-t0>=CLOCKS_PER_SEC){cv_scan_followup(file,"PLUS1S");phase=5;}
            else if(phase==5&&t-t0>=3*CLOCKS_PER_SEC){cv_scan_followup(file,"PLUS3S");phase=6;}
            else if(phase==2&&(f70&0x200u)){phase=0;cv_scan_free();std::fprintf(file,"[SCAN] ride too short\n");}
        }
    }
    std::fflush(file);
}
// CV_SCRIPT_TRACE=1 (dev, US): script trace, every logic frame for rm_317 v0.
void native_script_trace() {
    static const bool on=[] {const char* e=std::getenv("CV_SCRIPT_TRACE");return e&&std::strcmp(e,"1")==0;}();
    if(!on)return;
    const uint32_t system=vm_read32(CV_SCENE_SYSTEM);
    if(!system)return;
    static std::FILE* file=std::fopen("script-trace.txt","w");
    if(!file)return;
    const uint32_t room=(vm_read8(CV_SRT_ROOM)<<16)|(vm_read8(CV_SRT_ROOM+1)<<8)|vm_read8(CV_SRT_ROOM+2);
    const uint32_t f74=vm_read32(system+0x74);uint32_t now[6]={0,0,0,0,vm_read32(system+0x70),f74};
    if(room==0x031700u&&(f74&7)) {
        static uint32_t prev[0x680/4];static uint32_t last_pc[6]={};
        std::fprintf(file,"[TRACE] t=%ld 70=%08X 74=%08X",(long)std::clock(),now[4],now[5]);
        for(uint32_t i=0;i<0x680/4;i++) {
            const uint32_t v=vm_read32(0x00CA75C0+4*i);
            if(v!=prev[i]){std::fprintf(file," %04X=%08X",0x75C0+4*i,v);prev[i]=v;}
        }
        std::fputc('\n',file);
        for(unsigned k=0;k<6;k++) {
            const uint32_t pc=vm_read32(0x00CA75D4+0x134*k+8);
            if(pc==last_pc[k]||pc<0x40000000u||pc>=0x50000000u)continue;
            last_pc[k]=pc;
            std::fprintf(file,"[TRACE-PC%u] %08X:",k,pc);
            for(uint32_t a=pc-0x20;a<pc+0x30;a++)std::fprintf(file,"%s%02X",a==pc?" [":(a==pc+1?"] ":" "),unsigned(vm_read8(a)));
            std::fputc('\n',file);
        }
    }
    std::fflush(file);
}
void native_scene_before() {
    native_scene_log();
    native_event_log();
    native_script_trace();
    const char* on=std::getenv("CV_SMOOTH_ANIMATION");
    inspection_before(on&&std::strcmp(on,"1")==0);
    scene_previous_valid=on&&std::strcmp(on,"1")==0&&native_scene_ready()&&scene_read(scene_scene);
    if(!scene_previous_valid)return;
    scene_scripted=(vm_read32(vm_read32(CV_SCENE_SYSTEM)+0x74)&4)!=0;
    for(unsigned i=0;i<10;i++)scene_previous[i]=vm_read32(CV_SCENE_CAMERA+scene_fields[i]);
    scene_cut[0]=vm_read32((CV_SCENE_CAMERA+0x4));scene_cut[1]=vm_read32((CV_SCENE_CAMERA+0x8C));scene_cut[2]=vm_read32((CV_SCENE_CAMERA+0xA4));
}
bool scene_model_read(uint32_t actor,ProbeModel& next) {
    next.actor=actor;
    if(!actor||vm_read32(actor+0x8C)!=0)return false;
    next.model=vm_read32(actor+0x2D8);next.root=vm_read32(actor+0x340);
    next.parent_bone=vm_read32(actor+0x2E0);
    next.identity=vm_read32(actor+4);next.instance=vm_read32(actor+0x39C);
    if(!next.model||!next.root)return false;
    next.palette=vm_read32(next.model+0x14);next.count=vm_read32(next.model+4);
    if(!next.palette||!next.count||next.count>64)return false;
    for(unsigned i=0;i<3;i++) {
        next.position[i]=read_float(actor+0x10+4*i);
        if(!std::isfinite(next.position[i]))return false;
    }
    for(unsigned i=0;i<16;i++) {
        next.root_matrix[i]=read_float(next.root+4*i);
        if(!std::isfinite(next.root_matrix[i]))return false;
    }
    for(unsigned b=0;b<next.count;b++)for(unsigned i=0;i<16;i++) {
        next.bones[b][i]=read_float(next.palette+b*0x48+8+4*i);
        if(!std::isfinite(next.bones[b][i]))return false;
    }
    return true;
}
// 001BE1A0 queues the HD room mask; 00204208 normally commits it two
// engine updates later. A newly rendered cut must use that mask immediately.
void scene_room_cut() {
    rsx_d3d12_camera_cut();
    // 0022C108 selects the HD FOV using cached IDs BEFORE updating them.
    // Seed both IDs at a cut so its first new view also gets the new FOV.
    const uint32_t cameras=vm_read32(CV_SCENE_CAMERA_MANAGER);
    const uint32_t hd_camera=cameras?vm_read32(cameras+0x44):0;
    if(hd_camera) {
        vm_write32(hd_camera+0x174,vm_read32((CV_SCENE_CAMERA+0x8C)));
        vm_write32(hd_camera+0x178,vm_read32((CV_SCENE_CAMERA+0xA4)));
    }
    const uint32_t manager=vm_read32(CV_SCENE_ROOM_MANAGER);
    if(!manager)return;
    const uint32_t room=vm_read32(manager+0x60);
    if(!room)return;
    const uint32_t delay=vm_read32(room+0x198);
    if(delay!=2)return;
    for(unsigned i=0;i<16;i++) {
        vm_write32(room+0x118+4*i,vm_read32(room+0x158+4*i));
        vm_write32(room+0x158+4*i,0xFFFFFFFFu);
    }
    vm_write32(room+0x198,0xFFFFFFFFu);
}
void native_scene_after() {
    const bool inspection_cut=inspection_after();
    // A cut changes both visibility and resource state after original drawing.
    // Rebuild that frame from the NEW camera; never blend across the cut.
    if(!scene_previous_valid||!native_scene_ready()||!scene_matches(scene_scene)) {
        scene_pose_have=scene_pose_pair=false;return;
    }
    const bool scripted=(vm_read32(vm_read32(CV_SCENE_SYSTEM)+0x74)&4)!=0;
    // Script entry/exit can change the view without changing the room camera IDs.
    // Treat that ownership change as a cut, including the first post-update image.
    const bool cut=inspection_cut||!scene_camera_matches(scene_cut)||scripted!=scene_scripted||
        (scene_pose_have&&scripted!=scene_pose_current.scripted);
    // Avoid a large stack frame now that the bounded enemy pool is included.
    static ProbePose next;
    next.count=next.enemy_count=next.object_count=0;
    next.scripted=scripted;next.camera_cut=cut;
    std::memcpy(next.scene,scene_scene,sizeof next.scene);
    const uint32_t actor=vm_read32(CV_SCENE_ACTOR);
    if(!scene_model_read(actor,next.models[0])) {scene_pose_have=scene_pose_pair=false;return;}
    next.count=1;
    // Original object pools from 0016A75C; attached head/hair is a separate
    // type-0 actor. Follow verified parent links, never a fixed heap address.
    const uint32_t system=vm_read32(CV_SCENE_SYSTEM),config=vm_read32(CV_SCENE_CONFIG);
    if(config)for(unsigned pool=0;pool<2;pool++) {
        const uint32_t base=vm_read32(system+0x1B1D8+4*pool);
        const uint32_t count=pool==0&&(vm_read32(system+0x74)&0x40000000u)?4:vm_read32(config+0x8C+4*pool);
        if(!base)continue;
        if(count>64) {scene_pose_have=scene_pose_pair=false;return;}
        for(unsigned i=0;i<count;i++) {
            const uint32_t child=base+i*0x4E4;
            // Standalone type-0 scene models share the same root/palette path:
            // scripted furniture, moving scenery and the physical box lid.
            // Identity includes the dispatch variant, so changes still snap.
            if((vm_read32(child)&0x80000081u)==1 &&
               (vm_read32(child+4)>>16)<1200 &&
               scene_model_read(child,next.objects[next.object_count]))next.object_count++;
            if(!(vm_read32(child)&0x80)||vm_read32(child+0x2E4)!=actor)continue;
            if(next.count==5||!scene_model_read(child,next.models[next.count])) {
                scene_pose_have=scene_pose_pair=false;return;
            }
            next.count++;
        }
    }
    // 00129D70: active enemy slots use bit 0, with bit 31 excluding drawing.
    // Unsupported palettes are left to the original renderer; never drop Claire.
    // The SRT/config count excludes accessories; 00129D70 uses this extent.
    const uint32_t enemies=vm_read32(system+0x1B140);
    if(enemies<=128)for(unsigned i=0;i<enemies;i++) {
        const uint32_t enemy=CV_SRT_ENEMIES+i*0x578;
        if((vm_read32(enemy)&0x80000001u)!=1)continue;
        if(scene_model_read(enemy,next.enemies[next.enemy_count]))next.enemy_count++;
    }
    if(cut) {
        scene_room_cut();
        for(unsigned i=0;i<10;i++)next.camera[i]=vm_read32(CV_SCENE_CAMERA+scene_fields[i]);
        next.cut[0]=vm_read32((CV_SCENE_CAMERA+0x4));next.cut[1]=vm_read32((CV_SCENE_CAMERA+0x8C));next.cut[2]=vm_read32((CV_SCENE_CAMERA+0xA4));
    }else {
        std::memcpy(next.camera,scene_previous,sizeof next.camera);
        std::memcpy(next.cut,scene_cut,sizeof next.cut);
    }
    const auto& old=scene_pose_current;
    scene_pose_pair=!cut&&scene_pose_have&&old.count==next.count&&std::memcmp(old.cut,next.cut,sizeof next.cut)==0&&std::memcmp(old.scene,next.scene,sizeof next.scene)==0;
    if(scene_pose_pair)for(unsigned m=0;m<next.count;m++) {
        const auto& a=old.models[m];const auto& b=next.models[m];
        if(a.actor!=b.actor||a.model!=b.model||a.root!=b.root||a.palette!=b.palette||
           a.count!=b.count||a.parent_bone!=b.parent_bone)scene_pose_pair=false;
    }
    scene_pose_previous=old;scene_pose_current=next;scene_pose_have=true;
}
void scene_models_blend(const ProbeModel* models,unsigned count,const ProbeModel* old_models,
                        unsigned previous_count,bool blend,float alpha) {
    for(unsigned m=0;m<count;m++) {
        const auto& current=models[m];
        const ProbeModel* previous=&current;
        // Match each actor independently: a spawn/despawn must not reset camera motion.
        if(blend)for(unsigned j=0;j<previous_count;j++) {
            const auto& candidate=old_models[j];
            if(current.actor==candidate.actor&&current.identity==candidate.identity&&
               current.instance==candidate.instance&&current.model==candidate.model&&
               current.root==candidate.root&&current.palette==candidate.palette&&
               current.count==candidate.count&&current.parent_bone==candidate.parent_bone) {
                previous=&candidate;break;
            }
        }
        scene_model_write(current,*previous,alpha);
    }
}
static std::atomic<bool> scene_replacing_active{false};
// EU-only diagnostic: repeat scene preparation without the task dispatcher.
void render_scene_between_ticks(ppu_context* ctx, unsigned count, double phase) {
    static const bool on=[] {const char* e=std::getenv("CV_SMOOTH_ANIMATION");return e&&std::strcmp(e,"1")==0;}();
    static thread_local unsigned consecutive_ticks=0;
    if(!on || !scene_pose_have || !native_scene_ready()) {
        consecutive_ticks=0;
        scene_replacing_active.store(false,std::memory_order_relaxed);
        return;
    }
    if(!scene_camera_matches(scene_pose_current.cut)||!scene_matches(scene_pose_current.scene)||
       scene_pose_current.scripted!=((vm_read32(vm_read32(CV_SCENE_SYSTEM)+0x74)&4)!=0)) {
        scene_pose_have=scene_pose_pair=false;
        consecutive_ticks=0;
        scene_replacing_active.store(false,std::memory_order_relaxed);
        return;
    }
    if(count==0) {
        consecutive_ticks=0;
    } else {
        if(++consecutive_ticks>=2) {
            // Heavy scene (running <= ~35 FPS): every frame is already a real
            // 30 Hz logic tick with no in-between frames to interpolate. Keep
            // CV_BIO_UPDATE's freshly built scene instead of discarding and
            // rebuilding it a second time on the same frame.
            scene_replacing_active.store(false,std::memory_order_relaxed);
            return;
        }
        if(!scene_pose_pair && !scene_pose_current.camera_cut) {
            // CV_BIO_UPDATE just drew current_pose at alpha=1.0; no blend needed.
            scene_replacing_active.store(false,std::memory_order_relaxed);
            return;
        }
    }
    const uint32_t system=vm_read32(CV_SCENE_SYSTEM), object=vm_read32(CV_SCENE_RENDER);
    if(!system || !object) {
        scene_replacing_active.store(false,std::memory_order_relaxed);
        return;
    }
    scene_replacing_active.store(true,std::memory_order_relaxed);
    const ppu_context incoming=*ctx;
    const uint32_t camera=CV_SCENE_CAMERA;
    const uint32_t counter=vm_read32(camera+0x4B0),last=vm_read32(camera+0x4AC);
    ctx->gpr[3]=object;CV_SCENE_FN_000155D0(ctx);drain_scene(ctx);
    vm_write32(camera+0x4B0,counter);vm_write32(camera+0x4AC,last);
    ctx->gpr[3]=0;CV_SCENE_FN_0022B9F0(ctx);drain_scene(ctx);
    // Preserve gameplay camera products, but keep the render camera alive
    // through submission after this function returns.
    const uint32_t derived[]={camera+0x18,camera+0x24,vm_read32(camera+0x9C),vm_read32(camera+0xA0),CV_SCENE_VIEW_MATRIX};
    for(unsigned i=0;i<5;i++) {
        scene_derived[i]=derived[i];
        if(derived[i])for(unsigned j=0;j<scene_derived_words[i];j++)
            scene_derived_saved[i][j]=vm_read32(derived[i]+4*j);
    }
    scene_derived_pending=true;
    uint32_t saved[10];
    const bool blend=scene_pose_pair;
    const float alpha=blend?static_cast<float>(phase):1.0f;
    const auto& previous_pose=scene_pose_previous;
    const auto& current_pose=scene_pose_current;
    // Original and extra images now use the SAME previous/current endpoints.
    // No extrapolation and no additional gameplay/animation update.
    scene_pose_saved=current_pose;scene_pose_pending=true;
    for(unsigned m=0;m<current_pose.count;m++)
        scene_model_write(current_pose.models[m],blend?previous_pose.models[m]:current_pose.models[m],alpha);
    scene_models_blend(current_pose.enemies,current_pose.enemy_count,
                       previous_pose.enemies,previous_pose.enemy_count,blend,alpha);
    scene_models_blend(current_pose.objects,current_pose.object_count,
                       previous_pose.objects,previous_pose.object_count,blend,alpha);
    for(unsigned i=0;i<10;i++) {
        saved[i]=vm_read32(camera+scene_fields[i]);
        if(!blend) {vm_write32(camera+scene_fields[i],current_pose.camera[i]);continue;}
        if(i<7) {
            float previous,current;std::memcpy(&previous,&previous_pose.camera[i],4);std::memcpy(&current,&current_pose.camera[i],4);
            write_float(camera+scene_fields[i],previous+(current-previous)*static_cast<float>(phase));
        }else {
            const int delta=int((current_pose.camera[i]-previous_pose.camera[i]+0x8000u)&0xFFFFu)-0x8000;
            vm_write32(camera+scene_fields[i],(previous_pose.camera[i]+static_cast<int>(std::lround(delta*phase)))&0xFFFFu);
        }
    }
    CV_SCENE_FN_00034478(ctx);drain_scene(ctx);
    if(current_pose.camera_cut) {
        // 0022C108 consumes the previous HD eye/direction before caching the
        // newly derived camera. A hard cut must not retain the old view here.
        const uint32_t cameras=vm_read32(CV_SCENE_CAMERA_MANAGER);
        const uint32_t hd=cameras?vm_read32(cameras+0x44):0;
        if(hd) {
            for(unsigned i=0;i<3;i++) {
                vm_write32(hd+0x140+4*i,vm_read32(camera+0x24+4*i));
                vm_write32(hd+0x160+4*i,vm_read32(camera+0x18+4*i));
            }
            vm_write32(hd+0x14C,0);vm_write32(hd+0x16C,0);
            for(unsigned i=0;i<4;i++)vm_write32(hd+0x150+4*i,0);
            // The model preparation below also consumes the active HD view.
            // Updating only its history leaves that first draw on the old view
            // until the later HD camera pass. Refresh it before rebuilding.
            ctx->gpr[3]=hd;CV_SCENE_FN_0022C108(ctx);drain_scene(ctx);
            for(unsigned i=0;i<16;i++)scene_cut_view[i]=vm_read32(object+0x30+4*i);
            scene_cut_view_valid=true;
        }
    }
    CV_SCENE_FN_00158620(ctx);drain_scene(ctx);
    CV_SCENE_FN_00146BE0(ctx);drain_scene(ctx);
    for(unsigned i=0;i<10;i++)vm_write32(camera+scene_fields[i],saved[i]);
    *ctx=incoming;
}
void native_prepare_cut(ppu_context* ctx) {
    // Map/door cameras can overwrite the shared view after scene rebuilding.
    // Worker bounds must use the same camera space as this cut's model packets.
    if(scene_cut_view_valid&&scene_pose_have&&native_scene_ready()&&
       scene_camera_matches(scene_pose_current.cut)&&scene_matches(scene_pose_current.scene)) {
        const uint32_t object=vm_read32(CV_SCENE_RENDER);
        for(unsigned i=0;i<16;i++)vm_write32(object+0x30+4*i,scene_cut_view[i]);
    }
    CV_SCENE_FN_001F4C40(ctx);
}
