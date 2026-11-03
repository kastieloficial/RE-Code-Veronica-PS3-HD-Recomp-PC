#include "../cv_scene_region.h"
#include "../cv_fps_experimental.cpp"
#include <cassert>
#ifdef _WIN32
#define setenv(name,value,overwrite) _putenv_s(name,value)
#endif
#include <vector>
static std::vector<unsigned char> memory(0x1100000);
uint32_t vm_read32(uint64_t a) {assert(a+4<=memory.size());return uint32_t(memory[a])<<24|uint32_t(memory[a+1])<<16|uint32_t(memory[a+2])<<8|memory[a+3];}
uint8_t vm_read8(uint64_t a) {assert(a<memory.size());return memory[a];}
void vm_write8(uint64_t a,uint8_t b) {assert(a<memory.size());memory[a]=b;}
void vm_write32(uint64_t a,uint32_t b) {assert(a+4<=memory.size());for(int i=0;i<4;i++)memory[a+i]=uint8_t(b>>(24-i*8));}
extern "C" {PPU_THREAD_LOCAL void (*g_trampoline_fn)(void*)=nullptr;}
extern "C" int rsx_d3d12_pose_replay_enabled() {return 0;}
extern "C" void rsx_d3d12_pose_scene(uint32_t) {}
static unsigned cut_notifications=0;
static bool camera_held=false;
extern "C" void rsx_d3d12_camera_hold(int hold) {camera_held=hold!=0;}
extern "C" void rsx_d3d12_camera_cut() {++cut_notifications;}
extern "C" void ppu_register_function(uint64_t,void (*)(ppu_context*)) {}
void CV_OG_FRAME_PACE(ppu_context*) {}
void CV_BIO_UPDATE(ppu_context*) {}
void CV_SCENE_FN_001F4C40(ppu_context*) {}
void CV_SCENE_FN_000155D0(ppu_context*) {}
void CV_SCENE_FN_0022B9F0(ppu_context*) {}
static bool derive_hd=false;
void CV_SCENE_FN_00034478(ppu_context*) {
    if(!derive_hd)return;
    for(unsigned i=0;i<3;i++) {
        write_float(CV_SCENE_CAMERA+0x24+4*i,100.0f+i);
        write_float(CV_SCENE_CAMERA+0x18+4*i,200.0f+i);
    }
}
static unsigned hd_updates=0,hd_calls=0;
static bool check_inspection_hold=false;
void CV_SCENE_FN_0022C108(ppu_context* ctx) {
    ++hd_calls;
    if(check_inspection_hold) {
        assert((vm_read32(0x200070)&1)==0);
        assert(vm_read8(0x310180)==0);
        for(unsigned i=0;i<3;i++) {
            assert(vm_read32(0x310140+4*i)==vm_read32((CV_SCENE_CAMERA+0x24)+4*i));
            assert(vm_read32(0x310160+4*i)==vm_read32((CV_SCENE_CAMERA+0x18)+4*i));
        }
    }
    if(!derive_hd)return;
    const auto hd=static_cast<uint32_t>(ctx->gpr[3]);
    assert(hd==0x310000);
    ++hd_updates;
    for(unsigned i=0;i<16;i++)vm_write32(vm_read32(CV_SCENE_RENDER)+0x30+4*i,1000+i);
    for(unsigned i=0;i<3;i++) {
        assert(read_float(hd+0x140+4*i)==100.0f+i);
        assert(read_float(hd+0x160+4*i)==200.0f+i);
        vm_write32(hd+0x120+4*i,vm_read32(hd+0x140+4*i));
    }
}
void CV_SCENE_FN_00158620(ppu_context*) {
    if(derive_hd&&scene_pose_current.camera_cut)
        for(unsigned i=0;i<3;i++)assert(read_float(0x310120+4*i)==100.0f+i);
}
static float expected;
static unsigned draws;
static bool check_enemy=false;
static float expected_enemy;
static bool check_hat=false;
static bool check_box=false;
static float expected_box;
void CV_SCENE_FN_00146BE0(ppu_context*) {
    ++draws;
    if(check_box) {
        assert(std::abs(read_float(0x503000)-expected_box)<1e-5f);
        assert(std::abs(read_float(0x504008)-expected_box)<1e-5f);
    }
    if(check_hat)assert(std::abs(read_float(0x405038)-expected_enemy-5)<1e-5f);
    if(check_enemy) {
        assert(std::abs(read_float(CV_SRT_ENEMIES+0x10)-expected_enemy)<1e-5f);
        assert(std::abs(read_float(0x401000+0x30)-expected_enemy)<1e-5f);
        assert(std::abs(read_float(0x402000+8+0x30)-expected_enemy)<1e-5f);
    }
    assert(std::abs(read_float(CV_SCENE_CAMERA+0xC)-expected)<1e-5f);
    assert(std::abs(read_float(0x104000+0x10)-(expected+3))<1e-5f);
    assert(std::abs(read_float(0x107000+8+0x30)-(expected+3))<1e-5f);
    assert(std::abs(read_float(0x100000+0x10)-expected)<1e-5f);
    assert(std::abs(read_float(0x102000+0x30)-expected)<1e-5f);
    assert(std::abs(read_float(0x103000+8+0x30)-expected)<1e-5f);
}
static void pose(float x) {
    write_float(0x104000+0x10,x+3);write_float(0x106000+0x30,x+3);write_float(0x107000+8+0x30,x+3);
    write_float(CV_SCENE_CAMERA+0xC,x);write_float(0x100000+0x10,x);
    write_float(0x102000+0x30,x);write_float(0x103000+8+0x30,x);
}
int main(int argc,char**) {
    if(argc>1){
        setenv("CV_SMOOTH_ANIMATION","0",1);
        native_scene_before();native_scene_after();
        ppu_context context{};render_scene_between_ticks(&context,1,.5);
        assert(draws==0&&!scene_pose_have);puts("PASS: disabled scene enhancement leaves rendering unchanged");return 0;
    }
    setenv("CV_SMOOTH_ANIMATION","1",1);
    vm_write32(CV_SCENE_ROOM_MANAGER,0x250000);vm_write32(0x250060,0x260000);
    vm_write32(CV_SCENE_SYSTEM,0x200000);vm_write32(CV_SCENE_RENDER,0x210000);
    vm_write32(0x200000+0x64,0x80);vm_write32(CV_SRT_RDX,0x3D650);
    vm_write32(CV_SRT_HEALTH,100);memory[CV_SRT_ROOM]=1;memory[CV_SRT_ROOM+1]=8;
    vm_write32(CV_SCENE_ACTOR,0x100000);vm_write32(0x100000+0x2D8,0x101000);vm_write32(0x100000+0x340,0x102000);
    vm_write32(0x101000+4,1);vm_write32(0x101000+0x14,0x103000);
    vm_write32(CV_SCENE_CONFIG,0x220000);vm_write32(0x220000+0x8c,1);vm_write32(0x200000+0x1b1d8,0x104000);
    vm_write32(0x104000,0x80);vm_write32(0x104000+0x2e4,0x100000);
    vm_write32(0x104000+0x2d8,0x105000);vm_write32(0x104000+0x340,0x106000);
    vm_write32(0x105000+4,1);vm_write32(0x105000+0x14,0x107000);
    vm_write32(0x102000+4,0x80000000);
    pose(0);native_scene_before();native_scene_after();
    pose(10);native_scene_before();native_scene_after();
    // The gameplay camera has advanced; rendering uses the matched endpoints.
    write_float(CV_SCENE_CAMERA+0xC,20);
    ppu_context ctx{};
    for(unsigned count: {0u,1u,3u})for(double phase: {0.0,0.25,0.5,0.9}) {
        auto before=memory;expected=float(phase*10);
        render_scene_between_ticks(&ctx,count,phase);
        assert(read_float(CV_SCENE_CAMERA+0xC)==20);
        native_scene_restore();assert(memory==before);
    }
    assert(draws==12);
    // Enemy bones and position share the camera phase, including catch-up draws.
    vm_write32(CV_SRT_ENEMY_COUNT,1);vm_write32(0x21B140u,1);vm_write32(CV_SRT_ENEMIES,1);
    vm_write32(CV_SRT_ENEMIES+4,0x10000);vm_write32(CV_SRT_ENEMIES+0x39c,7);
    vm_write32(CV_SRT_ENEMIES+0x2d8,0x400000);vm_write32(CV_SRT_ENEMIES+0x340,0x401000);
    vm_write32(0x400004,1);vm_write32(0x400014,0x402000);
    const auto enemy_pose=[](float x) {
        write_float(CV_SRT_ENEMIES+0x10,x);write_float(0x401000+0x30,x);write_float(0x402000+8+0x30,x);
    };
    pose(0);enemy_pose(100);native_scene_before();native_scene_after();
    pose(10);enemy_pose(120);native_scene_before();native_scene_after();check_enemy=true;
    for(unsigned count: {0u,1u,3u})for(double phase: {0.0,.25,.5,.9}) {
        expected=float(phase*10);expected_enemy=100+float(phase*20);auto before=memory;
        render_scene_between_ticks(&ctx,count,phase);native_scene_restore();assert(memory==before);
    }
    // Render extent includes accessory slots beyond the SRT count (four here).
    vm_write32(CV_SRT_ENEMY_COUNT,4);
    vm_write32(0x21B140,128);
    const uint32_t hat=CV_SRT_ENEMIES+127*0x578;
    vm_write32(hat,0x8081);vm_write32(hat+4,0x220000);
    vm_write32(hat+0x2e4,CV_SRT_ENEMIES);vm_write32(hat+0x2e0,11);
    vm_write32(hat+0x2d8,0x404000);vm_write32(hat+0x340,0x406000);
    vm_write32(0x404004,1);vm_write32(0x404014,0x405000);
    pose(0);enemy_pose(100);write_float(0x405038,105);native_scene_before();native_scene_after();
    pose(10);enemy_pose(120);write_float(0x405038,125);native_scene_before();native_scene_after();
    assert(scene_pose_current.enemy_count==2);check_hat=true;
    for(double phase: {0.0,.25,.5,.9}) {
        expected=float(phase*10);expected_enemy=100+float(phase*20);auto before=memory;
        render_scene_between_ticks(&ctx,0,phase);native_scene_restore();assert(memory==before);
    }
    check_hat=false;vm_write32(hat,0);vm_write32(0x21B140,1);
    // Reused slot with another identity must snap only that enemy, not the camera.
    vm_write32(CV_SRT_ENEMIES+0x39c,8);pose(20);enemy_pose(300);native_scene_before();native_scene_after();
    assert(scene_pose_pair);expected=15;expected_enemy=300;
    auto enemy_before=memory;render_scene_between_ticks(&ctx,0,.5);native_scene_restore();assert(memory==enemy_before);
    // A different model/palette in a reused slot also starts at its own endpoint.
    vm_write32(0x400014,0x403000);write_float(0x403008+0x30,400);
    enemy_pose(400);pose(30);native_scene_before();native_scene_after();
    assert(scene_pose_pair);expected=25;expected_enemy=400;
    // The fixture's draw callback tracks the first palette; keep it aligned too.
    enemy_before=memory;render_scene_between_ticks(&ctx,0,.5);native_scene_restore();assert(memory==enemy_before);
    vm_write32(0x400014,0x402000);
    // A camera cut snaps enemies along with the new view, never across the boundary.
    native_scene_before();vm_write32(CV_SCENE_CAMERA+4,9);pose(40);enemy_pose(600);native_scene_after();
    assert(!scene_pose_pair);expected=40;expected_enemy=600;enemy_before=memory;
    render_scene_between_ticks(&ctx,0,.25);native_scene_restore();assert(memory==enemy_before);
    // A visible death pose (zero HP) remains eligible; rendering does not advance AI.
    vm_write32(CV_SRT_ENEMIES+0x41c,0);pose(20);enemy_pose(700);native_scene_before();native_scene_after();
    expected=30;expected_enemy=650;enemy_before=memory;
    render_scene_between_ticks(&ctx,0,.5);native_scene_restore();assert(memory==enemy_before);
    // Inactive/unsupported/malformed enemies do not disable the valid scene.
    check_enemy=false;
    for(const auto [a,value]: {std::pair{CV_SRT_ENEMIES,0u},std::pair{CV_SRT_ENEMIES,0x80000001u},
        std::pair{CV_SRT_ENEMIES+0x8c,1u},std::pair{0x400004u,65u},std::pair{0x402008u,0x7fc00000u},
        std::pair{0x21B140u,129u}}) {
        const uint32_t old=vm_read32(a);vm_write32(a,value);
        pose(20);native_scene_before();native_scene_after();assert(scene_pose_have&&scene_pose_pair);
        expected=20;enemy_before=memory;render_scene_between_ticks(&ctx,0,.5);native_scene_restore();assert(memory==enemy_before);
        vm_write32(a,old);
    }
    // Reappearing actor starts from its own pose, even at the same address.
    vm_write32(0x21B140u,0);native_scene_before();native_scene_after();
    vm_write32(0x21B140u,1);enemy_pose(500);native_scene_before();native_scene_after();
    check_enemy=true;expected=20;expected_enemy=500;enemy_before=memory;
    render_scene_between_ticks(&ctx,0,.5);native_scene_restore();assert(memory==enemy_before);
    check_enemy=false;vm_write32(0x21B140u,0);
    // Box lid uses ordinary pool 1, independently of Claire/enemy attachments.
    vm_write32(0x220090,1);vm_write32(0x21b1dc,0x501000);
    vm_write32(0x501000,1);vm_write32(0x501004,0x1780064);
    vm_write32(0x5012d8,0x502000);vm_write32(0x501340,0x503000);
    vm_write32(0x502004,1);vm_write32(0x502014,0x504000);
    const auto box_pose=[](float x) {write_float(0x503000,x);write_float(0x504008,x);};
    box_pose(0);native_scene_before();native_scene_after();
    box_pose(10);native_scene_before();native_scene_after();check_box=true;expected=20;
    for(unsigned count: {0u,1u,3u})for(double phase: {0.,.25,.5,.9}) {
        expected_box=float(10*phase);auto before=memory;
        render_scene_between_ticks(&ctx,count,phase);native_scene_restore();assert(memory==before);
    }
    // Original close removes dispatch variant 100: no borrowed/intermediate pose.
    vm_write32(0x501004,0x1780000);box_pose(0);
    native_scene_before();native_scene_after();expected_box=0;
    auto box_before=memory;render_scene_between_ticks(&ctx,0,.5);native_scene_restore();assert(memory==box_before);
    // Reopening and model replacement cannot reuse old endpoints.
    vm_write32(0x501004,0x1780064);box_pose(30);native_scene_before();native_scene_after();
    expected_box=30;render_scene_between_ticks(&ctx,0,.5);native_scene_restore();
    vm_write32(0x50139c,1);box_pose(50);native_scene_before();native_scene_after();
    expected_box=50;render_scene_between_ticks(&ctx,0,.5);native_scene_restore();
    // Malformed/inactive objects are left untouched without disabling the scene.
    for(const auto [address,value]: {std::pair{0x501000u,0u},std::pair{0x501000u,0x80000001u},
        std::pair{0x50108cu,1u},std::pair{0x502004u,65u}}) {
        const auto old=vm_read32(address);vm_write32(address,value);
        native_scene_before();native_scene_after();assert(scene_pose_have);
        box_before=memory;render_scene_between_ticks(&ctx,0,.5);native_scene_restore();assert(memory==box_before);
        vm_write32(address,old);
    }
    // Recorded cabinet motion, and another ordinary identity/dispatch variant.
    // Preserve the original lateral shake while adding intermediate positions.
    for(uint32_t identity: {0x006b0000u,0x003a0002u}) {
        vm_write32(0x501004,identity);vm_write32(0x50139c,7);
        const auto scenery_pose=[&](float x,float z) {
            write_float(0x501010,x);write_float(0x501018,z);box_pose(z);
        };
        scenery_pose(45,22);native_scene_before();native_scene_after();
        scenery_pose(44.9f,22.1f);native_scene_before();native_scene_after();
        for(unsigned count: {0u,1u,3u})for(double phase: {0.,.25,.5,.9}) {
            expected_box=22+float(phase)*.1f;const auto before=memory;
            render_scene_between_ticks(&ctx,count,phase);
            assert(std::abs(read_float(0x501010)-(45-float(phase)*.1f))<1e-5f);
            assert(std::abs(read_float(0x501018)-expected_box)<1e-5f);
            native_scene_restore();assert(memory==before);
        }
        // The next tick moves sideways back; do not average the shake away.
        scenery_pose(45,22.2f);native_scene_before();native_scene_after();
        expected_box=22.15f;render_scene_between_ticks(&ctx,0,.5);
        assert(std::abs(read_float(0x501010)-44.95f)<1e-5f);native_scene_restore();
        // Spawn/instance reuse and camera cuts keep the exact new endpoint.
        vm_write32(0x50139c,8);scenery_pose(40,30);native_scene_before();native_scene_after();
        expected_box=30;render_scene_between_ticks(&ctx,0,.5);native_scene_restore();
        native_scene_before();vm_write32((CV_SCENE_CAMERA+0x8C),vm_read32((CV_SCENE_CAMERA+0x8C))+1);
        scenery_pose(50,40);native_scene_after();expected_box=40;
        render_scene_between_ticks(&ctx,0,.5);assert(read_float(0x501010)==50);native_scene_restore();
    }
    check_box=false;vm_write32(0x220090,0);
    // Cuts and changed palettes cannot mix unrelated states.
    vm_write32(CV_SCENE_CAMERA+4,123);pose(30);native_scene_before();native_scene_after();
    assert(!scene_pose_pair);expected=30;render_scene_between_ticks(&ctx,1,0.2);native_scene_restore();
    // Fixed-camera switch AFTER the original scene, inside one BioUpdate.
    // Rebuild a complete new-camera scene, including the frame without a BioUpdate.
    vm_write32(0x220000+4,0x230000);vm_write32(0x220000+0x84,5);
    vm_write32(0x220000+0x1AC,0x240000);vm_write32(0x220000+0x1A4,2);
    const uint32_t mesh=0x230000+4*0xE0;
    vm_write32(mesh,0x10);vm_write32(0x240000,0x21);vm_write32(0x240034,0x29);
    vm_write32(CV_SCENE_ROOM_MANAGER,0x250000);vm_write32(0x250060,0x260000);
    for(unsigned i=0;i<16;i++) {
        vm_write32(0x260118+4*i,0x11110000u+i);
        vm_write32(0x260158+4*i,0xAAAA0000u+i);
    }
    // Do not consume absent work or unrelated resource transitions.
    for(uint32_t delay: {0u,1u,3u,0xFFFFFFFFu}) {
        vm_write32(0x260198,delay);auto before=memory;
        scene_room_cut();assert(memory==before);
    }
    vm_write32(CV_SCENE_CAMERA_MANAGER,0x300000);vm_write32(0x300044,0x310000);
    vm_write32(0x310174,6);vm_write32(0x310178,88);
    vm_write32(0x310170,0x3f400000); // Other HD camera fields remain owned by its update.
    vm_write32(0x260198,2);
    pose(40);native_scene_before();
    vm_write32(mesh,0x42);vm_write32(0x240000,0x49);vm_write32(0x240034,0x41);
    vm_write32(CV_SCENE_CAMERA+0x8C,1);pose(80);native_scene_after();
    assert(scene_pose_have&&!scene_pose_pair);
    assert(vm_read32(0x310174)==1&&vm_read32(0x310178)==0);
    assert(vm_read32(0x310170)==0x3f400000);
    for(unsigned i=0;i<16;i++) {
        assert(vm_read32(0x260118+4*i)==0xAAAA0000u+i);
        assert(vm_read32(0x260158+4*i)==0xFFFFFFFFu);
    }
    assert(vm_read32(0x260198)==0xFFFFFFFFu);
    // Preserve the complete new-camera visibility without rewriting its flags.
    assert(vm_read32(mesh)==0x42&&vm_read32(0x240000)==0x49&&vm_read32(0x240034)==0x41);
    unsigned at_cut=draws;auto cut_memory=memory;
    expected=80;
    for(unsigned count: {1u,0u,0u}) {
        render_scene_between_ticks(&ctx,count,.5);native_scene_restore();
        assert(draws==++at_cut&&memory==cut_memory);
    }
    // The next update stays on the new view without a stale endpoint.
    vm_write32(mesh,vm_read32(mesh)|0x100); // A later renderer-owned flag remains unchanged.
    native_scene_before();
    assert(vm_read32(mesh)==0x142&&vm_read32(0x240000)==0x49&&vm_read32(0x240034)==0x41);
    native_scene_after();assert(scene_pose_have&&scene_pose_pair);
    for(double phase: {0.0,.25,.5,.9}) {
        expected=80;render_scene_between_ticks(&ctx,1,phase);native_scene_restore();
    }
    // Continuous camera movement within the new view still interpolates.
    pose(90);native_scene_before();native_scene_after();assert(scene_pose_pair);
    expected=85;render_scene_between_ticks(&ctx,1,.5);native_scene_restore();
    // Also reject a camera identity changed between update and extra frame.
    vm_write32(CV_SCENE_CAMERA+0x8C,2);at_cut=draws;cut_memory=memory;
    render_scene_between_ticks(&ctx,0,.75);native_scene_restore();
    assert(!scene_pose_have&&!scene_pose_pair&&draws==at_cut&&memory==cut_memory);
    // A second normal room uses the same scene path, without a room allowlist.
    memory[CV_SRT_ROOM+1]=9;
    pose(100);native_scene_before();native_scene_after();
    assert(scene_pose_have&&!scene_pose_pair);
    pose(110);native_scene_before();native_scene_after();assert(scene_pose_pair);
    expected=105;at_cut=draws;render_scene_between_ticks(&ctx,0,.5);native_scene_restore();assert(draws==at_cut+1);
    // Any room variant or resource change during BioUpdate discards the frame.
    const uint32_t addresses[]={CV_SRT_ROOM,CV_SRT_ROOM+1,CV_SRT_ROOM+2,CV_SCENE_SYSTEM,CV_SCENE_CONFIG,CV_SCENE_RENDER,0x250060};
    for(unsigned i=0;i<7;i++) {
        const uint32_t a=addresses[i],old=i<3?vm_read8(a):vm_read32(a);
        native_scene_before();
        if(i<3)vm_write8(a,uint8_t(old+1));else vm_write32(a,old+0x10000);
        native_scene_after();assert(!scene_pose_have&&!scene_pose_pair);
        if(i<3)vm_write8(a,uint8_t(old));else vm_write32(a,old);
        native_scene_before();native_scene_after();assert(scene_pose_have&&!scene_pose_pair);
    }
    // Even unchanged numeric camera IDs cannot authorize drawing stale resources.
    vm_write32(0x250060,0x270000);at_cut=draws;auto before_room=memory;
    render_scene_between_ticks(&ctx,0,.5);native_scene_restore();
    assert(draws==at_cut&&!scene_pose_have&&memory==before_room);
    native_scene_before();native_scene_after();assert(scene_pose_have&&!scene_pose_pair);
    vm_write32(0x250060,0);native_scene_before();native_scene_after();assert(!scene_pose_have);
    vm_write32(0x250060,0x260000);
    // Scripted in-room camera mode observed on the connecting stairs.
    for(uint32_t flags: {0x44u,0x46u,0x204u,0x2004u}) {
        vm_write32(0x200074,flags);vm_write32(0x200070,flags==0x2004?0x1105005:(flags==0x204?0x1010015:0x10004));
        vm_write32(0x200064,0x73dfc0);vm_write32(0x200068,0x3de00);
        assert(native_scene_ready());
        pose(100);native_scene_before();native_scene_after();
        pose(110);native_scene_before();native_scene_after();assert(scene_pose_pair);
        for(double phase: {0.0,.25,.5,.9}) {
            expected=100+float(10*phase);auto before=memory;
            render_scene_between_ticks(&ctx,0,phase);native_scene_restore();assert(memory==before);
        }
        // Subtitles and special rendering must remain protected during events.
        const auto control=vm_read32(0x200070);
        vm_write32(0x200070,control|0x200);assert(!native_scene_ready());
        vm_write32(0x200070,control);
        vm_write32(0x20006c,0x100);assert(!native_scene_ready());vm_write32(0x20006c,0);
        // A pending load still excludes redraw even with scripted camera flags.
        vm_write32(0x200064,0x73dfc1);assert(!native_scene_ready());
        vm_write32(0x200064,0x73dfc0);
        vm_write32(0x200068,0x13d600);vm_write32(0x200074,3);assert(!native_scene_ready());
        vm_write32(0x200068,0x3de00);vm_write32(0x200074,flags);
        native_scene_before();memory[CV_SRT_ROOM+2]++;native_scene_after();assert(!scene_pose_have);
        memory[CV_SRT_ROOM+2]--;
        native_scene_before();vm_write32(0x250060,0x270000);native_scene_after();assert(!scene_pose_have);
        vm_write32(0x250060,0x260000);
    }
    for(uint32_t flags: {1u,2u,4u,6u,0x45u,0x47u,0x144u}) {
        vm_write32(0x200074,flags);assert(!native_scene_ready());
    }
    vm_write32(0x200074,0x44);vm_write32(0x200070,0);assert(!native_scene_ready());
    vm_write32(0x200074,0);vm_write32(0x200064,0x80);vm_write32(0x200068,0);
    // Event boundaries can jump cameras without changing any of the cut IDs.
    for(bool enter: {true,false}) {
        vm_write32(0x200070,0x10004);
        vm_write32(0x200074,enter?0x200:0x204);
        pose(200);native_scene_before();native_scene_after();
        pose(210);native_scene_before();native_scene_after();
        native_scene_before();
        vm_write32(0x200074,enter?0x204:0x200);pose(800);
        native_scene_after();assert(scene_pose_have&&!scene_pose_pair);
        for(double phase: {0.,.25,.5,.9}) {
            expected=800;auto before=memory;
            render_scene_between_ticks(&ctx,0,phase);native_scene_restore();assert(memory==before);
        }
        // Movement resumes from the NEW endpoint, never from the old event.
        pose(810);native_scene_before();native_scene_after();assert(scene_pose_pair);
        expected=805;render_scene_between_ticks(&ctx,0,.5);native_scene_restore();
    }
    // Transition already visible before the next capture also breaks the pair.
    vm_write32(0x200074,0x204);pose(900);native_scene_before();native_scene_after();
    assert(!scene_pose_pair);expected=900;render_scene_between_ticks(&ctx,0,.25);native_scene_restore();
    vm_write32(0x200074,0);vm_write32(0x200070,0);
    for(uint32_t view_flags: {0x100u,0x200u,0x300u}) {
    // Inspection changes ownership without changing camera IDs, with text active.
    vm_write32(0x200074,(0x2000|view_flags));vm_write32(0x200070,0x15000);pose(100);
    inspection_before(true);
    vm_write32(0x200074,view_flags);vm_write32(CV_SCENE_CAMERA,2);vm_write32(0x200070,0x12285);pose(500);
    for(unsigned i=0;i<3;i++) {
        write_float((CV_SCENE_CAMERA+0xC)+4*i,500+i);write_float((CV_SCENE_CAMERA+0x24)+4*i,100+i);write_float((CV_SCENE_CAMERA+0x18)+4*i,10+i);
        write_float(0x310140+4*i,-1);
    }
    assert(!native_scene_ready());assert(inspection_after());
    const unsigned prior_notifications=cut_notifications,prior_hd_calls=hd_calls;
    check_inspection_hold=true;
    ctx.gpr[3]=0x310000;inspection_hd_update(&ctx);
    check_inspection_hold=false;
    assert(vm_read32(0x200070)==0x12285);
    assert(cut_notifications==prior_notifications);
    assert(hd_calls==prior_hd_calls+1);
    assert(camera_held);
    assert(read_float(0x310140)==100); // Update the old scene without consuming the cut.
    inspection_before(true);assert(!inspection_after());
    for(unsigned i=0;i<3;i++)write_float((CV_SCENE_CAMERA+0x24)+4*i,500+i);
    inspection_hd_update(&ctx);
    for(unsigned i=0;i<3;i++)assert(read_float(0x310140+4*i)==500+i);
    assert(cut_notifications==prior_notifications+1);
    assert(!camera_held);
    assert(vm_read8(0x310180)==1);
    inspection_hd_update(&ctx);assert(cut_notifications==prior_notifications+1);
    // Exit changes ownership one tick BEFORE restoring the original viewpoint.
    inspection_before(true);vm_write32(0x200074,(0x2000|view_flags));assert(!inspection_after());
    inspection_before(true);vm_write32(CV_SCENE_CAMERA,0);pose(100);assert(inspection_after());
    vm_write32(0x200070,0x15000);
    for(unsigned i=0;i<3;i++)vm_write32((CV_SCENE_CAMERA+0x24)+4*i,vm_read32((CV_SCENE_CAMERA+0xC)+4*i));
    ctx.gpr[3]=0x310000;inspection_hd_update(&ctx);assert(vm_read8(0x310180)==0);
    inspection_before(true);pose(101);assert(!inspection_after());
    // A scene replacement or pending load cancels the boundary.
    inspection_before(true);vm_write32(0x200074,view_flags);pose(500);
    vm_write32(0x200064,0x81);assert(!inspection_after());vm_write32(0x200064,0x80);
    inspection_before(true);vm_write32(0x200074,(0x2000|view_flags));pose(100);
    vm_write32(0x250060,0x270000);assert(!inspection_after());vm_write32(0x250060,0x260000);
    vm_write32(0x200074,0);vm_write32(0x200070,0);
    }
    // Running camera moves after ownership changes but before detail activation.
    vm_write32(0x200074,0x2100);vm_write32(CV_SCENE_CAMERA,0);pose(100);
    inspection_before(true);vm_write32(0x200074,0x128);pose(101);
    assert(!inspection_after());
    inspection_before(true);vm_write32(0x200074,0x100);vm_write32(CV_SCENE_CAMERA,2);pose(500);
    assert(inspection_after());
    assert(inspection.target[0]==vm_read32((CV_SCENE_CAMERA+0xC)));
    ctx.gpr[3]=0x310000;inspection_hd_update(&ctx);assert(camera_held);
    // A scene replacement must release a held presentation immediately.
    vm_write32(0x250060,0x270000);assert(!inspection_after());assert(!camera_held);
    vm_write32(0x250060,0x260000);
    vm_write32(CV_SCENE_CAMERA,0);vm_write32(0x200074,0);vm_write32(0x200070,0);
    // First inspection after load has no ownership edge (0x100 -> 0x100).
    inspection={};vm_write32(0x200074,0x100);pose(100);
    inspection_before(true);vm_write32(0x200070,0x12285);
    vm_write32(CV_SCENE_CAMERA,2);pose(500);assert(inspection_after());
    ctx.gpr[3]=0x310000;inspection_hd_update(&ctx);assert(camera_held);
    vm_write32(0x250060,0x270000);assert(!inspection_after());assert(!camera_held);
    vm_write32(0x250060,0x260000);
    // A camera type change without text must not create an inspection cut.
    inspection={};vm_write32(CV_SCENE_CAMERA,0);vm_write32(0x200070,0);pose(100);
    inspection_before(true);vm_write32(CV_SCENE_CAMERA,2);pose(500);assert(!inspection_after());
    vm_write32(CV_SCENE_CAMERA,0);vm_write32(0x200074,0);
    // Room messages are updated and drawn together by 001611C8 when bit 9 is set.
    // Keep their original display lists on logic ticks AND intervening frames.
    pose(120);native_scene_before();native_scene_after();
    expected=120;unsigned before_message=draws;
    vm_write32(0x200070,0x17204);vm_write32(0x200074,0x100);
    auto message_memory=memory;
    for(unsigned count: {1u,0u,3u}) {
        render_scene_between_ticks(&ctx,count,.5);native_scene_restore();
        assert(draws==before_message&&memory==message_memory);
    }
    native_scene_before();native_scene_after();assert(!scene_pose_have);
    // Closing a message must discard its old camera endpoint before resuming.
    native_scene_before();
    vm_write32(0x200070,0);vm_write32(0x200074,0);
    native_scene_after();assert(!scene_pose_have);
    pose(130);native_scene_before();native_scene_after();
    assert(scene_pose_have&&!scene_pose_pair);
    expected=130;render_scene_between_ticks(&ctx,0,.5);native_scene_restore();
    assert(draws==before_message+1);
    pose(140);native_scene_before();native_scene_after();
    expected=135;render_scene_between_ticks(&ctx,0,.5);native_scene_restore();
    // The HD renderer consumes history before caching the new derived eye.
    // Both directions of a hard cut must seed that history in its first image.
    derive_hd=true;
    for(unsigned id: {3u,2u}) {
        pose(150);native_scene_before();
        vm_write32((CV_SCENE_CAMERA+0x8C),id);pose(160);native_scene_after();
        expected=160;
        for(double phase: {0.0,.5,.9}) {
            for(unsigned i=0;i<12;i++)write_float(0x310140+4*i,-999.0f);
            for(unsigned i=0;i<3;i++)write_float(0x310120+4*i,-999.0f);
            const unsigned before_hd=hd_updates;
            render_scene_between_ticks(&ctx,0,phase);assert(hd_updates==before_hd+1);
            for(unsigned i=0;i<3;i++) {
                assert(read_float(0x310140+4*i)==100.0f+i);
                assert(read_float(0x310160+4*i)==200.0f+i);
            }
            for(unsigned offset: {0x14Cu,0x150u,0x154u,0x158u,0x15Cu,0x16Cu})
                assert(vm_read32(0x310000+offset)==0);
            const uint32_t view=vm_read32(CV_SCENE_RENDER)+0x30;
            for(unsigned i=0;i<16;i++)vm_write32(view+4*i,2000+i);
            native_prepare_cut(&ctx);
            for(unsigned i=0;i<16;i++)assert(vm_read32(view+4*i)==1000+i);
            native_scene_restore();
            vm_write32(view,3000);native_prepare_cut(&ctx);assert(vm_read32(view)==3000);
        }
        // Continuous movement must retain the original HD history behavior.
        pose(170);native_scene_before();native_scene_after();
        write_float(0x310140,-123);expected=165;const unsigned before_hd=hd_updates;
        render_scene_between_ticks(&ctx,0,.5);assert(read_float(0x310140)==-123&&hd_updates==before_hd);
        native_scene_restore();
    }
    derive_hd=false;
    // Loading tasks, special render paths and missing live-character state stay excluded.
    for(const auto [offset,mask]: {std::pair{0x64u,1u},std::pair{0x6Cu,0x100u},std::pair{0x6Cu,0x200u},std::pair{0x70u,0x100u},std::pair{0x74u,1u}}) {
        const uint32_t old=vm_read32(0x200000+offset);vm_write32(0x200000+offset,old|mask);
        native_scene_before();native_scene_after();assert(!scene_pose_have);
        vm_write32(0x200000+offset,old);
    }
    vm_write32(0x101000+4,65);native_scene_before();native_scene_after();assert(!scene_pose_have);
    unsigned prior=draws;render_scene_between_ticks(&ctx,1,.5);assert(draws==prior);
    vm_write32(0x101000+4,1);vm_write32(0x107000+8,0x7fc00000);
    native_scene_before();native_scene_after();assert(!scene_pose_have);
    // The item renderer borrows its own vertices, never the sprite/text lists.
    using namespace cv_item_interpolation;
    vm_write32(0x200064,0x200);vm_write32(0x200068,0);vm_write32(0x200070,8);
    vm_write32(menu+0x44,0x20000);vm_write32(menu+0x48,0x10);vm_write32(menu+0x54,73);
    vm_write32(0x210128,1);vm_write32(0x210134,0x500000);vm_write32(0x500000,0x510000);
    vm_write32(0x510000,CV_SCENE_MESH_VTABLE);vm_write32(0x510030,0x540000);
    vm_write32(0x510004,0x520000);vm_write32(0x51000C,0x530000);
    vm_write32(0x510010,2);vm_write32(0x510014,2);
    auto vertices=[](float x) {
        for(uint32_t a: {0x520000u,0x530000u})for(unsigned i=0;i<8;i++)
            write_float(a+4*i,i%4==3?0.0f:x+float(i));
    };
    reset();vertices(0);capture();assert(!paired);
    vertices(10);capture();assert(paired);
    const auto original_item=memory;
    for(double phase: {0.0,.25,.5,.9}) {
        render(phase);assert(std::abs(read_float(0x520000)-float(phase*10))<1e-5f);
        assert(std::abs(read_float(0x530004)-float(1+phase*10))<1e-5f);
        restore();assert(memory==original_item);
    }
    // Repeated render-only frames use the same endpoints, without accumulating.
    render(.25);restore();render(.75);assert(read_float(0x520000)==7.5f);restore();
    // New item, changed mesh topology, unsupported commands and menu exit snap.
    vm_write32(menu+0x54,74);capture();assert(!paired);
    vertices(20);capture();assert(paired);
    vm_write32(0x510030,0x540100);capture();assert(!paired);
    vm_write32(0x510010,9000);capture();assert(current.meshes.empty());
    vm_write32(0x510010,2);vm_write32(0x510000,0x8F0724);capture();assert(current.meshes.empty());
    vm_write32(0x510000,CV_SCENE_MESH_VTABLE);vertices(0);capture();vertices(10);capture();
    vm_write32(0x210128,0);auto changed_list=memory;render(.5);assert(memory==changed_list);
    vm_write32(0x210128,1);vm_write32(0x200070,0);capture();assert(current.meshes.empty());
    puts("PASS: item positions, normals, padding, exact restore, repeated intermediate frames, identity, bounds, menu exit");
    puts("PASS: enemies, identity/slot changes, death poses, shared phase, original/extra/catch-up frames, exact restore, intra-update cuts, new-view redraws, HD mask commit, visibility preservation, restart, bounds");
}
