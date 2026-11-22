#include "../cv_test_tools.cpp"
#include <cassert>
static std::array<unsigned char,16*1024*1024> memory{};
static int frames=0,warps=0,serializations=0;
static bool last_gameplay=false,last_title=false;
static int toasts=0,toast_ok=0;static std::wstring last_toast;
static ppu_context* caller_context;
extern "C" {
uint8_t* vm_base=memory.data();
PPU_THREAD_LOCAL ppu_context* g_active_ctx=nullptr;
PPU_THREAD_LOCAL void (*g_trampoline_fn)(void*)=nullptr;
int g_pad_block=0;
uint8_t vm_read8(uint64_t a){assert(a<memory.size());return memory[a];}
uint16_t vm_read16(uint64_t a){return uint16_t(vm_read8(a))<<8|vm_read8(a+1);}
uint32_t vm_read32(uint64_t a){return uint32_t(vm_read16(a))<<16|vm_read16(a+2);}
void vm_write8(uint64_t a,uint8_t v){assert(a<memory.size());memory[a]=v;}
void vm_write16(uint64_t a,uint16_t v){vm_write8(a,uint8_t(v>>8));vm_write8(a+1,uint8_t(v));}
void vm_write32(uint64_t a,uint32_t v){for(int i=0;i<4;i++)vm_write8(a+i,uint8_t(v>>(24-i*8)));}
void vm_write64(uint64_t a,uint64_t v){vm_write32(a,uint32_t(v>>32));vm_write32(a+4,uint32_t(v));}
uint64_t vm_read64(uint64_t a){return uint64_t(vm_read32(a))<<32|vm_read32(a+4);}
void* rsx_d3d12_window(){return nullptr;}
void perf_overlay_toast(const wchar_t* text,int ok){++toasts;toast_ok=ok;last_toast=text;}
int pad_phys_poll(int,PadPhys*){return 0;}
}
// The host windows and per-frame helpers cv_test_tools.cpp calls are not part of this test (it never linked without them).
bool cv_save_library_message(MSG*){return false;}
bool cv_archives_visible(){return false;}
void cv_save_library_close(){}
void cv_save_library_show(bool,int){}
void cv_archives_maps(const std::vector<cv_test::Room>&,unsigned,bool (*)(unsigned,unsigned)){}
void cv_archives_show(int,int,int){}
void cv_save_library_frame(ppu_context*,bool gameplay,bool title){last_gameplay=gameplay;last_title=title;}
namespace cv_quick {void frame(ppu_context*){}bool consumes_stick(){return false;}}
void cv_manual_reload_frame(ppu_context*){}
void cv_title_return_frame(ppu_context*){}   // cv_title_return.cpp
void ORIGINAL_FRAME(ppu_context*){++frames;}
void ORIGINAL_SERIALIZE(ppu_context*){++serializations;}
static int save_screens=0,load_screens=0,finishes=0;
static uint32_t finish_flags=0;
void ORIGINAL_SAVE_SCREEN(ppu_context*){++save_screens;}
static bool run_native_finish=false;
static unsigned fade_calls=0;
static void native_finish_service(ppu_context* c,uint32_t address) {
 if(address==0x001999A8u)c->gpr[3]=0; // asynchronous read/fade wait completed
 if(address==0x001910F0u) { // isolate drawing; record the native fade command
  ++fade_calls;vm_write32(state_base+0x2A604,0xFF000000);
  vm_write32(state_base+0x2A5FC,0x437F0000); // menu black, alpha 255
  vm_write32(state_base+0x74,vm_read32(state_base+0x74)|2u);
 }
}
#define DRAIN_TRAMPOLINE(c) do { while(g_trampoline_fn) { auto next=g_trampoline_fn;g_trampoline_fn=nullptr;next(c); } } while(0)
#include "native_save_finish_fixture.h"
#include "native_load_visual_fixture.h"
#undef DRAIN_TRAMPOLINE
static unsigned visual_returns=0;
void NATIVE_LOAD_VISUAL_RETURN(ppu_context* c) {
 ++visual_returns;
 if(run_native_finish)native_ui_return_fixture(c);
}
static unsigned sound_resets=0;
void NATIVE_STOP_BANK(ppu_context* c){++sound_resets;assert(c->gpr[3]==0x2C0000&&c->gpr[4]==9);}
void ORIGINAL_SAVE_FINISH(ppu_context* c){
 ++finishes;finish_flags=vm_read32(state_base+0x50);
 if(run_native_finish)native_finish_fixture(c);
}
void NATIVE_LOAD_SCREEN(ppu_context* c){
 ++load_screens;assert(c->gpr[3]==LOAD_STORAGE&&c->gpr[4]==0);
 assert((vm_read32(state_base+0x50)&0x200)!=0);
 // The native constructor returns the UI storage pointer, as the save one does.
 c->gpr[3]=LOAD_STORAGE;
}
static int rows=0;static std::vector<uint16_t> row_text;
void ORIGINAL_ROW(ppu_context* c){   // reads the location as func_001D8090 does
 ++rows;const uint32_t row=uint32_t(c->gpr[3]),table=vm_read32(state_base+0x1B3F0);
 const int id=int8_t(vm_read8(row+0xD));row_text.clear();
 if(c->gpr[1]>=0x9000)return;   // a slot without the label: nothing borrowed
 assert(id!=0&&c->gpr[1]==0x9000-0x200&&vm_read64(c->gpr[1])==0x9000);
 for(uint32_t t=table+vm_read32(table+4+id*4);;t+=2){uint16_t v=vm_read16(t);row_text.push_back(v);if(v==0xFFFF)break;}
}
static void tail(void* raw){assert(raw==g_active_ctx);static_cast<ppu_context*>(raw)->gpr[3]=999;}
void TEST_WARP(ppu_context* c){
 ++warps;assert(c==g_active_ctx&&c!=caller_context);assert(c->gpr[1]==caller_context->gpr[1]-0x400&&c->gpr[2]==CV_TOC);
 assert(c->gpr[4]==2&&c->gpr[5]==14&&c->gpr[6]==3&&c->gpr[7]==255);
 assert(vm_read8(state_base+0x97)==1);g_trampoline_fn=tail;
}
int main(){
 cv_test::Room parsed;
 for(auto name:{L"rm_0000.arc",L"rm_0021.arc",L"rm_2141.arc",L"rm_9999.arc"})assert(cv_test::parse_room(name,parsed));
 assert(parsed.stage==9&&parsed.number==99&&parsed.variant==9);
 for(auto name:{L"rm_00.arc",L"rm_0020.arc.bak",L"../rm_0020.arc",L"rm_0A20.arc",L"rm_0020.png"})assert(!cv_test::parse_room(name,parsed));
 assert(!cv_launcher_controls::chord_active(cv_launcher_controls::menu_chord,cv_launcher_controls::test_chord));
 assert(!cv_launcher_controls::chord_active(cv_launcher_controls::srt_chord,cv_launcher_controls::test_chord));
 ppu_context c{};caller_context=&c;c.gpr[1]=0x9000;c.gpr[2]=111;c.gpr[3]=222;g_active_ctx=&c;
 constexpr uint32_t packet=0x80000000u|(2<<24)|(14<<16)|(1<<8)|3;
 cv_test_tools_enable(true);
 request=packet;TEST_FRAME(&c);assert(warps==0&&result==4); // title/uninitialized state
 vm_write32(CV_SRT_RDX,0x3D650);vm_write8(CV_SRT_CHARACTER,0);vm_write32(CV_SRT_HEALTH,160);   // health is 32-bit
 vm_write32(state_base+0x64,0x18080);vm_write32(state_base+0x68,0x18000); // menus enabled but blocked
 assert(active_game());
 request=packet;TEST_FRAME(&c);assert(warps==1&&request==0&&result==2);
 assert(c.gpr[1]==0x9000&&c.gpr[2]==111&&c.gpr[3]==222&&g_active_ctx==&c&&g_trampoline_fn==nullptr);
 TEST_FRAME(&c);assert(warps==1); // consumed once
 vm_write32(state_base+0x68,0x10000);request=packet;TEST_FRAME(&c);assert(warps==1&&result==4); // save dialog active
 vm_write32(state_base+0x68,0x18000);vm_write32(CV_SRT_HEALTH,0);request=packet;TEST_FRAME(&c);assert(warps==1); // dead
 // "Save here": the typewriter's request bit, only on a live game; the serializer
 // then keeps the inventory (ribbon) and the save count.
 vm_write32(CV_SRT_HEALTH,160);save_request=true;TEST_FRAME(&c);
 assert((vm_read32(state_base+0x74)&0x00200000u)!=0&&tool_save&&result==6);
 const uint32_t inventory=CV_SRT_INVENTORY;vm_write32(inventory+4,0x001F0002u);vm_write32(state_base+0x90,7);
 TEST_SERIALIZE(&c);assert(serializations==1&&vm_read32(inventory+4)==0x001F0002u&&vm_read32(state_base+0x90)==7);
 // A room with no typewriter: once the file is written, the slot list's display wait is ended.
 const uint32_t object=0x200000,manager=0x210000,ui=0x220000,slots=0x230000;
 vm_write32(SAVE_OBJECT,object);vm_write32(SAVE_MANAGER,manager);vm_write32(manager+0x4E8,1);
 vm_write32(object+0x30,2);vm_write32(object+0x58,4);vm_write8(object+0x2B,1);vm_write32(object+0xA0,ui);vm_write32(ui+0x34,slots);
 vm_write32(slots,1);vm_write16(slots+0x14,3);vm_write32(slots+0x20,8);
 vm_write32(state_base+0x68,0x10000);TEST_FRAME(&c);assert(vm_read32(slots+0x20)==8&&vm_read32(slots)==1); // still writing
 vm_write32(manager+0x4E8,0);TEST_FRAME(&c);assert(vm_read32(slots+0x20)==0&&vm_read32(slots)==0&&vm_read16(slots+0x14)==0xFFFA);
 // The chosen slot, once the state is copied into it (step 5), is remembered;
 // its row then shows "F7: rm_XXXX" where the location goes.
 const uint32_t data=0x240000,slot=data+0x30+2*0x838,table=0x260000,row=0x270000;
 f7_file=(fs::temp_directory_path()/L"cv_f7_saves_test.txt").wstring();_wremove(f7_file.c_str());
 vm_write32(SAVE_DATA,data);vm_write32(slot,0xA);vm_write8(slot+9,2);vm_write8(slot+0xA,14);vm_write8(slot+0xB,0);
 vm_write32(state_base+0x1B3F0,table);vm_write32(table+8,0x40);
 vm_write32(row,3);vm_write8(row+0xD,0);c.gpr[3]=row;c.gpr[1]=0x9000;
 TEST_ROW(&c);assert(rows==1&&row_text.empty()); // not a tool save yet
 vm_write32(ui+0x10,2);vm_write32(object+0x54,5);TEST_FRAME(&c);assert(f7_saves.size()==1);
 TEST_ROW(&c);assert(rows==2&&c.gpr[1]==0x9000&&vm_read32(table+8)==0x40&&vm_read8(row+0xD)==0);
 const std::vector<uint16_t> label=f7_label(2,14,0);
 assert(row_text==label&&label.back()==0xFFFF);
#if !defined(CV_REGION_JP)
 {std::string shown;for(uint16_t v:label)if(v!=0xFFFF)shown+=v==0xFF01?' ':char(v+0x20);assert(shown=="F7: rm_2140");}
#endif
 f7_saves.clear();f7_loaded=false;TEST_ROW(&c);assert(rows==3&&!row_text.empty()); // read back from the file
 vm_write8(slot+0x20,1);TEST_ROW(&c);assert(rows==4&&row_text.empty()); // overwritten by a normal save
 _wremove(f7_file.c_str());
 vm_write32(object+0x54,0);vm_write32(state_base+0x68,0x18000);
 cv_test_tools_enable(false);TEST_SERIALIZE(&c);assert(serializations==2);TEST_FRAME(&c);assert(warps==1&&frames==10);
 // New load action: localization is complete, ordinary save/title paths still
 // delegate, and no guest state changes until a live-game request is consumed.
 for(auto& lang:cv_test_i18n::k_text)for(auto text:lang)assert(text&&*text);
 TEST_SAVE_SCREEN(&c);TEST_SAVE_FINISH(&c);
 assert(save_screens==1&&load_screens==0&&finishes==1);
 cv_test_tools_enable(true);vm_write32(state_base+0x74,0);tool_save=false;
 vm_write32(CV_SRT_HEALTH,0);load_request=true;TEST_FRAME(&c);
 assert(!tool_load&&!load_request&&result==4&&vm_read32(state_base+0x74)==0);
 vm_write32(CV_SRT_HEALTH,160);vm_write32(state_base+0x50,0x10040);
 const uint32_t original_flags=vm_read32(state_base+0x50);
 const int old_serializations=serializations;
 load_request=true;TEST_FRAME(&c);
 assert(tool_load&&!load_opened&&!tool_save&&!ready&&result==7&&!load_request);
 assert(vm_read32(state_base+0x74)==0x00200000&&vm_read32(state_base+0x50)==original_flags);
 TEST_FRAME(&c);assert(load_screens==0); // wait for native asset preparation
 TEST_SAVE_SCREEN(&c);
 assert(load_screens==1&&save_screens==1&&load_opened&&vm_read32(LOAD_UI)==LOAD_STORAGE);
 assert(serializations==old_serializations); // loading must never serialize/write a save
 assert(vm_read32(inventory+4)==0x001F0002u&&vm_read32(state_base+0x90)==7);
 // Cancelling uses the room return path and preserves every unrelated flag.
 vm_write32(state_base+0x58,0);TEST_SAVE_FINISH(&c);
 assert(tool_load&&load_opened&&load_cancelled&&finish_flags==original_flags&&finishes==2);
 // Confirmation preserves the native full-load branch and selection result.
 tool_load=true;load_opened=true;load_cancelled=false;load_visual_returned=false;vm_write32(state_base+0x50,original_flags|0x200);
 vm_write32(state_base+0x58,0x10);TEST_SAVE_FINISH(&c);
 assert(tool_load&&load_opened&&finish_flags==(original_flags|0x200)&&vm_read32(state_base+0x58)==0x10);
 vm_write32(state_base+0x68,0x10000);TEST_FRAME(&c); // native load active
 vm_write32(state_base+0x64,0x300050);TEST_FRAME(&c); // native room restart
 assert(!tool_load&&!load_opened);
 // Disabling tools cancels queued requests; a later native save is untouched.
 load_request=true;save_request=true;request=packet;cv_test_tools_enable(false);
 assert(!load_request&&!save_request&&!request);
 const int old_screens=save_screens;TEST_SAVE_SCREEN(&c);assert(save_screens==old_screens+1);
 ready=true;send_load_save();assert(!load_request); // disabled UI action
 cv_test_tools_enable(true);ready=false;send_load_save();assert(!load_request);
 ready=true;request=packet;send_load_save();assert(!load_request);request=0;
 save_request=true;send_load_save();assert(!load_request);save_request=false;
 cv_test_tools_enable(false);
 // Reproduce the actual native finalizer against the reported live-game
 // states (not the old no-op finalizer stub). EU control flow is identical in
 // JP/US; point its global at the region's synthetic state.
 vm_write32(0xA89BD0,state_base);vm_write32(0xA89BD4,0x290000);
 vm_write32(state_base+0x60,0x280000);
 const uint32_t visual_controller=0x2A0000,visual_sprite=0x2B0000;
 vm_write32(0xAC0968,visual_controller);vm_write32(SCREEN_FADE_CONTROLLER,visual_controller);
 vm_write32(visual_controller+0x30,visual_sprite);
 vm_write32(SOUND_MANAGER,0x2C0000);
 run_native_finish=true;
 auto prepare_finish=[&](bool confirm) {
  tool_load=true;load_opened=true;load_cancelled=false;load_visual_returned=false;
  vm_write32(state_base+0x50,0x200);
  vm_write32(state_base+0x58,confirm?0x10:0);
  vm_write16(state_base+0x5C,2);vm_write16(state_base+0x5E,0);
  vm_write32(state_base+0x64,0x73DFC0);vm_write32(state_base+0x68,0x35E00);
  vm_write32(state_base+0x70,0x08005040);vm_write32(state_base+0x74,0x00200100);
  // Exact native load UI completion call, before its +4C=1 store.
  c.gpr[3]=visual_controller;c.gpr[4]=0x40;c.gpr[5]=0;c.gpr[6]=uint64_t(-1);
  native_ui_fade_fixture(&c);vm_write8(visual_controller+0x4C,1);
  assert(vm_read32(visual_sprite+0x300)==0&&vm_read32(visual_controller+0x40)==0);
 };
 // Show why native title-load cleanup alone is insufficient when entered via
 // the typewriter: both transition flags survive it.
 prepare_finish(true);native_finish_fixture(&c);
 assert(vm_read32(state_base+0x64)==0x300050);
 assert((vm_read32(state_base+0x70)&0x08000000)&& (vm_read32(state_base+0x74)&0x00200000));
 assert(vm_read8(visual_controller+0x4C)==1&&vm_read32(visual_sprite+0x300)==0);
 const unsigned resets_before_confirm=sound_resets;
 prepare_finish(true);TEST_SAVE_FINISH(&c);
 assert(sound_resets==resets_before_confirm+1); // old room's looping bank-9 voices stopped before the restart
 assert(vm_read32(state_base+0x64)==0x300050&&vm_read32(state_base+0x70)==0x5040&&vm_read32(state_base+0x74)==0x100);
 assert(vm_read32(state_base+0x68)==0x80); // restart tasks see the title-screen load's blocked mask
 assert(vm_read32(state_base+0x50)&0x200); // full saved-state restart remains selected
 assert(vm_read8(visual_controller+0x4C)==0&&vm_read32(visual_controller+0x40)==255&&vm_read32(visual_sprite+0x300)==0xFF000000);
 // A cancellation takes two native phases. The room fade is restored only
 // after cleanup, including its saved colour and the fade-active flag.
 std::array<uint32_t,24> expected_fade{};
 for(size_t i=0;i<expected_fade.size();++i) {expected_fade[i]=0x3F000000u+uint32_t(i);vm_write32(state_base+LoadReturnState::fade_offset+uint32_t(i*4),expected_fade[i]);}
 vm_write32(state_base+0x74,0x100);load_return.capture();
 prepare_finish(false);const unsigned returns_before_cancel=visual_returns,resets_before_cancel=sound_resets;TEST_SAVE_FINISH(&c);
 assert(tool_load&&load_cancelled&&fade_calls==1&&vm_read16(state_base+0x5E)==1);
 assert(vm_read32(state_base+0x74)&2); // native return currently uses the menu's black
 assert(vm_read8(visual_controller+0x4C)==0&&vm_read32(visual_sprite+0x300)==0xFF000000);
 TEST_SAVE_FINISH(&c);
 assert(!tool_load&&!load_opened&&!load_cancelled&&vm_read16(state_base+0x5C)==0);
 assert(visual_returns==returns_before_cancel+1);
 assert(sound_resets==resets_before_cancel); // cancelling keeps the room's sounds
 assert(vm_read32(state_base+0x68)&0x8000);
 assert(!(vm_read32(state_base+0x70)&0x08000000)&&vm_read32(state_base+0x74)==0x100);
 for(size_t i=0;i<expected_fade.size();++i)assert(vm_read32(state_base+LoadReturnState::fade_offset+uint32_t(i*4))==expected_fade[i]);
 run_native_finish=false;
 // ---- F7 from the title: the pure rules, then the whole flow on synthetic memory ----
 {
  using namespace cv_title_flow;
  Snapshot snap;snap.mapped=true;snap.tasks=0x300048;snap.blocked=0;snap.flags74=0;snap.title_state=0xFFFFFFFFu;
  assert(title_menu_ready(snap));                                   // the main menu, idle, past the intro
  {auto s=snap;s.mapped=false;assert(!title_menu_ready(s));}
  {auto s=snap;s.tasks=0x300040;assert(!title_menu_ready(s));}      // no menu task
  {auto s=snap;s.tasks=0x300048|0x80;assert(!title_menu_ready(s));} // a room is running
  {auto s=snap;s.tasks=0x300048|0x8000;assert(!title_menu_ready(s));}   // a save/load screen
  {auto s=snap;s.tasks=0x300048|0x10;assert(!title_menu_ready(s));}     // new game / load setup
  {auto s=snap;s.blocked=0x8;assert(!title_menu_ready(s));}         // the menu task is blocked
  {auto s=snap;s.flags74=2;assert(!title_menu_ready(s));}           // a fade is running
  for(uint32_t st=7;st<=9;st++){auto s=snap;s.title_state=st;assert(!title_menu_ready(s));}   // the intro movie
  {auto s=snap;s.title_state=3;assert(title_menu_ready(s));}
  assert(context_of(true,true)==CTX_GAMEPLAY&&context_of(false,true)==CTX_TITLE&&context_of(false,false)==CTX_NONE);
  assert(load_allowed(CTX_GAMEPLAY)&&load_allowed(CTX_TITLE)&&!load_allowed(CTX_NONE));
  assert(maps_allowed(CTX_TITLE)&&!maps_allowed(CTX_NONE));
  assert(save_allowed(CTX_GAMEPLAY)&&!save_allowed(CTX_TITLE)&&!save_allowed(CTX_NONE));   // Save only in game
  assert(new_game_writes(0x400200).flags50==0x400000&&new_game_writes(0).tasks==0x300010);
  // The machine: refuse when the menu is not ready, start, wait for 30 frames of control, warp once.
  Machine m;assert(!m.request(packet,false,0)&&!m.busy());
  assert(m.request(packet,true,1000)&&m.busy()&&!m.request(packet,true,1000));
  Inputs in;in.now_ms=1000;in.title_ready=false;
  for(int i=0;i<5;i++)assert(m.step(in)==Step::NONE);              // waiting for the menu
  in.now_ms=1000+Machine::START_TIMEOUT_MS+1;assert(m.step(in)==Step::ABORT_NOT_READY&&!m.busy());
  assert(m.request(packet,true,5000));in.now_ms=5000;in.title_ready=true;assert(m.step(in)==Step::START_NEW_GAME);
  in.title_ready=false;in.active=0x300010;in.game_ready=false;
  for(int i=0;i<100;i++)assert(m.step(in)==Step::NONE);            // setup, opening: no control yet
  in.game_ready=true;in.active=0x80;
  for(unsigned i=1;i<Machine::CONTROL_FRAMES;i++)assert(m.step(in)==Step::NONE);
  in.game_ready=false;assert(m.step(in)==Step::NONE);              // control lost (a cutscene): the count restarts
  in.game_ready=true;
  for(unsigned i=1;i<Machine::CONTROL_FRAMES;i++)assert(m.step(in)==Step::NONE);
  assert(m.step(in)==Step::WARP&&m.pending==packet&&!m.busy());     // after the 30th frame in a row
  assert(m.step(in)==Step::NONE);                                  // once
  // Aborts: the menu comes back (after the grace frames), and the 240 s limit.
  assert(m.request(packet,true,0));in.title_ready=true;in.now_ms=0;assert(m.step(in)==Step::START_NEW_GAME);
  in.title_ready=false;in.game_ready=false;in.active=0x8;
  for(unsigned i=0;i<Machine::GRACE_FRAMES;i++)assert(m.step(in)==Step::NONE);   // the tasks may still show it right after the writes
  assert(m.step(in)==Step::ABORT_MENU&&!m.busy());
  assert(m.request(packet,true,0));in.title_ready=true;assert(m.step(in)==Step::START_NEW_GAME);
  in.title_ready=false;in.active=0x300010;in.now_ms=Machine::TIMEOUT_MS;assert(m.step(in)==Step::NONE);
  in.now_ms=Machine::TIMEOUT_MS+1;assert(m.step(in)==Step::ABORT_TIMEOUT&&!m.busy());
 }
 {
  // TEST_FRAME with the main menu on screen (EU memory map, synthetic): contexts, the request, the new game, the warp.
  static uint64_t fake_now=1000;clock_ms=[]()->uint64_t{return fake_now;};
  for(auto& b:memory)b=0;
  cv_test_tools_enable(true);
  vm_write32(CV_TITLE_STATE,0xFFFFFFFFu);vm_write32(state_base+0x64,0x300048);vm_write32(state_base+0x68,0);vm_write32(state_base+0x74,0);vm_write32(state_base+0x50,0x400200);
  const int warps_before=warps;
  TEST_FRAME(&c);
  assert(last_title&&!last_gameplay&&archives_context==cv_title_flow::CTX_TITLE&&title_ready&&!ready);
  assert(cv_archives_travel(0x00020E01u>>0,3));                    // accepted at the title menu (the room key is unused by the stub warp)
  request=packet;   // the window's request, as cv_archives_travel packs it
  TEST_FRAME(&c);
  assert(title_flow.busy()&&flow_busy&&toasts==1&&toast_ok==1&&last_toast==cv_archives_i18n::strings[language][cv_archives_i18n::STARTING]);
  assert(vm_read32(state_base+0x64)==0x300010&&(vm_read32(state_base+0x50)&0x200)==0&&vm_read32(state_base+0x50)==0x400000);   // the menu's New Game writes
  assert(!cv_archives_travel(0x00020E01u,3));                      // one flow at a time
  for(int i=0;i<10;i++){fake_now+=16;TEST_FRAME(&c);}
  assert(warps==warps_before&&title_flow.busy()&&!last_title);     // loading the new game: nothing yet
  // Control reached (the same fixture the other tests use for a controllable game).
  vm_write32(CV_SRT_RDX,0x3D650);vm_write8(CV_SRT_CHARACTER,0);vm_write32(CV_SRT_HEALTH,160);vm_write32(state_base+0x64,0x18080);vm_write32(state_base+0x68,0x18000);
  for(unsigned i=0;i<cv_title_flow::Machine::CONTROL_FRAMES-1;i++){fake_now+=16;TEST_FRAME(&c);assert(warps==warps_before);}
  fake_now+=16;TEST_FRAME(&c);
  assert(warps==warps_before+1&&!title_flow.busy()&&!flow_busy&&last_gameplay);
  assert(c.gpr[1]==0x9000&&g_active_ctx==&c&&g_trampoline_fn==nullptr);
  // A menu that is not ready (loading) accepts nothing; Save stays out of reach at the title.
  vm_write32(state_base+0x64,0x300058);vm_write32(state_base+0x68,0);vm_write32(CV_SRT_HEALTH,0);TEST_FRAME(&c);
  assert(!last_title&&archives_context==cv_title_flow::CTX_NONE&&!title_ready&&!cv_archives_travel(0x00020E01u,3));
  // The menu comes back during the wait: nothing more is written, the user is told.
  vm_write32(state_base+0x64,0x300048);vm_write32(state_base+0x50,0x400200);TEST_FRAME(&c);assert(title_ready);
  request=packet;const int t0=toasts;TEST_FRAME(&c);assert(title_flow.busy()&&toasts==t0+1);
  vm_write32(state_base+0x64,0x300048);   // the menu task is back after the writes
  for(int i=0;i<6;i++){fake_now+=16;TEST_FRAME(&c);}
  assert(!title_flow.busy()&&!flow_busy&&toasts==t0+2&&toast_ok==0&&last_toast==cv_archives_i18n::strings[language][cv_archives_i18n::TITLE_FLOW_FAILED]);
  // The 240 s limit, and turning F7 off mid-flow.
  vm_write32(state_base+0x64,0x300048);TEST_FRAME(&c);request=packet;TEST_FRAME(&c);assert(title_flow.busy());
  vm_write32(state_base+0x64,0x300010);fake_now+=cv_title_flow::Machine::TIMEOUT_MS+1;TEST_FRAME(&c);assert(!title_flow.busy()&&toast_ok==0);
  vm_write32(state_base+0x64,0x300048);TEST_FRAME(&c);request=packet;TEST_FRAME(&c);assert(title_flow.busy());
  cv_test_tools_enable(false);TEST_FRAME(&c);assert(!title_flow.busy()&&!flow_busy&&archives_context==cv_title_flow::CTX_NONE);
  // i18n of the new strings is complete (the static_assert, and no cell left as the English one in another language except proper names).
  using namespace cv_archives_i18n;
  for(int l=2;l<13;l++)for(Id id:{ENTRY_HINT,SAVE_TITLE_BLOCKED,MAP_TITLE_NOTE,STARTING,TITLE_FLOW_FAILED})assert(std::wcscmp(strings[l][id],strings[1][id])!=0);
 }
 std::puts("[map-tools-test] PASS: maps/save regression; native finalizer reproduces stale load flags, F7 clears them; independent native UI opacity reset on load and cancel; multi-phase room fade return (" CV_TITLE_ID ")");
}
