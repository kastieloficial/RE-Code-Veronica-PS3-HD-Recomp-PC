#include "cv_scene_region.h"
#pragma once
#include <chrono>
#include <cstdint>

// The EU controller produces edges at display rate (00519534). BioUpdate's
// 001E3318 reads controller+3C+164 at logic rate. Preserve intervening edges,
// not held buttons/axes or the producer's previous-button history.
namespace cv_fps_input {
struct Latch {
    uint32_t owner=0,pending[4]{},saved[4]{};
    std::chrono::steady_clock::time_point last{};
    bool borrowed=false;
    void reset() {owner=0;for(auto& bits:pending)bits=0;}
    bool valid(uint32_t object) const {
        return object>=0x10000&&object<0xBFFFF000&&vm_read32(object)==CV_SCENE_CONTROLLER_VTABLE&&
            vm_read8(object+0x3C+5)!=0;
    }
    void capture() {
        const auto now=std::chrono::steady_clock::now();
        const uint32_t object=vm_read32(CV_SCENE_CONTROLLER);
        // Never replay buffered input after a pause, device loss or replacement.
        if(object!=owner||now-last>std::chrono::milliseconds(100))reset();
        last=now;
        if(!valid(object)){reset();return;}
        owner=object;
        for(unsigned i=0;i<4;i++)pending[i]|=vm_read32(owner+0x1A0+4*i);
    }
    void begin_tick(bool first) {
        if(!owner||!valid(owner)||vm_read32(CV_SCENE_CONTROLLER)!=owner){reset();return;}
        for(unsigned i=0;i<4;i++) {
            const uint32_t address=owner+0x1A0+4*i;
            saved[i]=vm_read32(address);
            vm_write32(address,first?pending[i]:0);
            pending[i]=0;
        }
        borrowed=true;
        // 001E3318 caches its mapped result for a display frame. Catch-up ticks
        // must not confirm a second menu/dialog with that same button edge.
        if(!first)for(unsigned port=0;port<4;port++) {
            vm_write32(CV_MENU_INPUT+60*port,0);
            vm_write32(CV_MENU_REPEAT+60*port,0);
        }
    }
    void end_tick() {
        if(borrowed&&vm_read32(CV_SCENE_CONTROLLER)==owner&&valid(owner))
            for(unsigned i=0;i<4;i++)vm_write32(owner+0x1A0+4*i,saved[i]);
        borrowed=false;
    }
};
}
