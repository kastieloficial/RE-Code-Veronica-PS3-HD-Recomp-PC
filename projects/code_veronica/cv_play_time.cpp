#include "ppu_recomp.h"
#include "cv_scene_region.h"
#include "cv_options.h"
#include "cv_play_time.h"

void CV_SCENE_FN_TIMER(ppu_context*);
extern "C" bool cv_fps_elapsed_ticks(float*);

// Replaces the original adjacent pair, not the game logic or SRT formatter.
void cv_play_time_update(ppu_context* ctx) {
    static thread_local cv_play_time::Clock clock;
    const uint32_t stateAddress=vm_read32(CV_SCENE_SYSTEM);
    const uint32_t before=stateAddress?vm_read32(stateAddress+0x8B0):0;
    ctx->lr=0x00028A88;CV_SCENE_FN_TIMER(ctx);
    ctx->lr=0x00028A8C;CV_SCENE_FN_TIMER(ctx);
    float elapsed=0;
    if(!cv_options::enabled("CV_SMOOTH_ANIMATION")||!stateAddress||
       vm_read32(CV_SCENE_SYSTEM)!=stateAddress||!cv_fps_elapsed_ticks(&elapsed)) {
        clock.reset();return;
    }
    const uint32_t after=vm_read32(stateAddress+0x8B0);
    const uint32_t corrected=clock.advance(stateAddress,before,after,elapsed);
    if(corrected!=after)vm_write32(stateAddress+0x8B0,corrected);
}
