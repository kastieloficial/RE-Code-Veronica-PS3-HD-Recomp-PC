#include "ppu_recomp.h"
#include "../cv_play_time.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <vector>

void cv_play_time_update(ppu_context*);
static uint32_t stateAddress=0x10000,ticks=0,task=0x80,flags=0x80000000;
static float elapsed=0.5f;
static bool have_timing=true;
static unsigned calls=0;
uint32_t vm_read32(uint64_t address) {
    if(address==0xA89BD0)return stateAddress;
    assert(address==stateAddress+0x8B0);return ticks;
}
void vm_write32(uint64_t address,uint32_t value) {assert(address==stateAddress+0x8B0);ticks=value;}
extern "C" bool cv_fps_elapsed_ticks(float* out) {*out=elapsed;return have_timing;}
void func_001D027C(ppu_context* ctx) {
    // Original EU guard and limit; two calls still provide original clobbers.
    assert(ctx->lr==(calls%2?0x00028A8C:0x00028A88));++calls;
    if((task&0x80)&&(flags&0x80000000)&&ticks<cv_play_time::limit)++ticks;
    ctx->gpr[3]=ticks;ctx->gpr[4]=stateAddress;ctx->cr=123;
}
static void option(bool on) {
#ifdef _WIN32
    _putenv_s("CV_SMOOTH_ANIMATION",on?"1":"0");
#else
    setenv("CV_SMOOTH_ANIMATION",on?"1":"0",1);
#endif
}
static void reset(uint32_t value=0) {stateAddress+=0x1000;ticks=value;task=0x80;flags=0x80000000;have_timing=true;}
static void frame(double seconds) {
    elapsed=static_cast<float>(seconds*30);ppu_context ctx{};ctx.gpr[20]=42;
    const auto before=calls;cv_play_time_update(&ctx);
    assert(calls==before+2&&ctx.gpr[20]==42&&ctx.cr==123&&ctx.lr==0x00028A8C);
}
int main() {
    option(true);
    for(int fps: {15,30,35,40,59,60,120,144,240}) {
        reset();for(int i=0;i<fps*10;i++)frame(1.0/fps);
        assert(ticks>=599&&ticks<=601);
    }
    // Same elapsed time, different frame partitions (including a long frame).
    for(const auto& frames:std::vector<std::vector<double>>{{.5,.5},{.01,.09,.2,.7},{1.0}}) {
        reset();for(double delta:frames)frame(delta);assert(ticks==60);
    }
    reset();frame(1.0/120);assert(ticks==0);frame(1.0/120);assert(ticks==1);
    // Pause discards fractions and cannot turn paused wall time into game time.
    reset();frame(1.0/120);task=0;frame(5);assert(ticks==0);task=0x80;
    frame(1.0/120);assert(ticks==0);frame(1.0/120);assert(ticks==1);
    flags=0;frame(5);assert(ticks==1);flags=0x80000000;frame(1.0/60);assert(ticks==2);
    // Loading / replacing the counter must not inherit an old fractional tick.
    reset();frame(1.0/120);ticks=100;frame(1.0/120);assert(ticks==100);
    frame(1.0/120);assert(ticks==101);
    reset();frame(1.0/120);stateAddress+=0x1000;frame(1.0/120);assert(ticks==0);
    reset(cv_play_time::limit-1);frame(10);assert(ticks==cv_play_time::limit);
    frame(10);assert(ticks==cv_play_time::limit);
    reset();frame(0);assert(ticks==0);
    have_timing=false;frame(1.0/60);assert(ticks==2);have_timing=true;
    frame(std::numeric_limits<double>::quiet_NaN());assert(ticks==4);
    frame(-1);assert(ticks==6);
    option(false);reset();for(int i=0;i<60;i++)frame(1.0/60);assert(ticks==120);
    std::puts("PASS: elapsed time at 15-240 FPS, uneven frames, pause, load, limit, fallback and opt-out");
}
