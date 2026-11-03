// Native scene interpolation; included inside the frame pacing namespace.

bool native_scene_ready() {
    const uint32_t system=vm_read32(CV_SCENE_SYSTEM);
    if(!system || vm_read32(CV_SRT_RDX)!=0x3D650 || vm_read8(CV_SRT_CHARACTER)!=0 || int32_t(vm_read32(CV_SRT_HEALTH))<=0)return false;
    const uint32_t tasks=vm_read32(system+0x64)&~vm_read32(system+0x68);
    const uint32_t camera_mode=vm_read32(system+0x74),control=vm_read32(system+0x70);
    // Script command 0012C864 mode 3 sets 0x44 and control bit 4. The
    // connecting staircase uses 0x44/0x46 while the same room is drawable.
    // The recorded in-room event uses 0x204 with the same script control bit;
    // its ordinary scene path remains drawable (no special-render flags).
    // 0x2004 is the scripted furniture sequence; it uses the same scene path.
    // Keep other event modes excluded; task/resource guards still reject loads.
    const bool scripted_camera=(camera_mode==0x44||camera_mode==0x46||camera_mode==0x204||camera_mode==0x2004)&&(control&4);
    // 001AF3E0/00146F90 call message update/draw routine 001611C8 for bit 9.
    // Scene preparation would free its text lists. Keep the original frame
    // until the message closes, without repeating its input/timer logic.
    return (tasks&0x80)&&!(tasks&0x1803F)&&(!(camera_mode&7)||scripted_camera)&&
        !(control&0x1C040308)&&!(vm_read32(system+0x6C)&0x300);
}
// Room numbers and camera IDs can be reused after loading. Match the actual
// scene resources as well, and never carry render endpoints across a transition.
static constexpr unsigned scene_words=5;
bool scene_read(uint32_t* scene) {
    scene[0]=(uint32_t(vm_read8(CV_SRT_ROOM))<<16)|(uint32_t(vm_read8(CV_SRT_ROOM+1))<<8)|vm_read8(CV_SRT_ROOM+2);
    scene[1]=vm_read32(CV_SCENE_SYSTEM);scene[2]=vm_read32(CV_SCENE_CONFIG);scene[3]=vm_read32(CV_SCENE_RENDER);
    const uint32_t manager=vm_read32(CV_SCENE_ROOM_MANAGER);
    scene[4]=manager?vm_read32(manager+0x60):0;
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
void native_scene_before() {
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
// EU-only diagnostic: repeat scene preparation without the task dispatcher.
void render_scene_between_ticks(ppu_context* ctx, unsigned count, double phase) {
    static const bool on=[] {const char* e=std::getenv("CV_SMOOTH_ANIMATION");return e&&std::strcmp(e,"1")==0;}();
    if(!on || !scene_pose_have || !native_scene_ready())return;
    if(!scene_camera_matches(scene_pose_current.cut)||!scene_matches(scene_pose_current.scene)||
       scene_pose_current.scripted!=((vm_read32(vm_read32(CV_SCENE_SYSTEM)+0x74)&4)!=0)) {
        scene_pose_have=scene_pose_pair=false;return;
    }
    const uint32_t system=vm_read32(CV_SCENE_SYSTEM), object=vm_read32(CV_SCENE_RENDER);
    if(!system || !object)return;
    (void)count;
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
