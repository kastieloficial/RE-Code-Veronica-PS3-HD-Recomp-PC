#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace cv_play_time {
// The original counter stores 60 ticks/second, independently of render rate.
constexpr uint32_t limit=0x0CDFE600;
struct Clock {
    uint32_t stateAddress=0,expected=0;
    double remainder=0;
    void reset() {stateAddress=expected=0;remainder=0;}
    uint32_t advance(uint32_t identity,uint32_t before,uint32_t after,double ticks30) {
        // Observe the original routine: it owns pause/scene/limit conditions.
        if(!identity||after<=before||after-before>2||before>=limit||
           !std::isfinite(ticks30)||ticks30<0) {reset();return after;}
        if(stateAddress!=identity||expected!=before)remainder=0;
        stateAddress=identity;
        const double pending=remainder+ticks30*2;
        const uint32_t remaining=limit-before;
        const uint32_t whole=pending>=remaining?remaining:static_cast<uint32_t>(std::floor(pending+1e-7));
        remainder=std::max(0.0,pending-whole);
        if(whole==remaining)remainder=0;
        expected=before+whole;
        return expected;
    }
};
}
