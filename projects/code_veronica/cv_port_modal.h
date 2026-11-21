#pragma once
/* One owner of the "a port window is open" state: F5 launcher, F6 items, F7 archives, the legacy
 * test window and the return-to-title box. Each window holds one bit; recompute() below is the only
 * writer of g_pad_block (the game ignores the pad while any bit is set) and of the focus-pause hold
 * (the game stands still while a non-launcher window is open and not working). The decision logic is
 * the pure Core (tests/test_port_modal.cpp); the Win32 glue is in cv_port_modal.cpp. UI thread only. */
#include <cstdint>
namespace cv_port_modal {
enum Bit : unsigned { LAUNCHER=1, ITEMS=2, ARCHIVES=4, TEST_TOOLS=8, TITLE_RETURN=16 };
constexpr uint64_t DEBOUNCE_MS=250;
struct Core {
    unsigned mask=0;        // bits of the windows that are open
    unsigned working=0;     // open windows that let the game run (a save being written)
    uint64_t last_toggle[8]={};   // time of the last open/close of each bit; 0 = never
    static int index(unsigned bit) {for(int i=0;i<8;i++)if(bit==(1u<<i))return i;return -1;}
    // Opening or closing the same window again within DEBOUNCE_MS is ignored (key chatter, two inputs at once).
    bool debounced(unsigned bit,uint64_t now) const {const int i=index(bit);return i>=0&&last_toggle[i]&&now-last_toggle[i]<DEBOUNCE_MS;}
    // The other windows that must close before `bit` opens (the launcher coexists: it is the options menu).
    unsigned exclusive_conflict(unsigned bit) const {return mask&~bit&~unsigned(LAUNCHER);}
    bool acquire(unsigned bit,uint64_t now) {
        const int i=index(bit);if(i<0)return false;
        if(mask&bit)return true;
        if(debounced(bit,now))return false;
        mask|=bit;last_toggle[i]=now?now:1;return true;
    }
    // Clears only its own bit; `now` (when given) starts the debounce of the next opening.
    void release(unsigned bit,uint64_t now=0) {
        const int i=index(bit);if(i<0)return;
        mask&=~bit;working&=~bit;if(now)last_toggle[i]=now;
    }
    void set_launcher(bool on) {if(on)mask|=LAUNCHER;else mask&=~unsigned(LAUNCHER);}
    void set_working(unsigned bit,bool on) {if(!(mask&bit))return;if(on)working|=bit;else working&=~bit;}
    bool pad_block() const {return mask!=0;}
    bool pause() const {return (mask&~unsigned(LAUNCHER)&~working)!=0;}
};
inline bool key_repeat(intptr_t lparam) {return (lparam&(1<<30))!=0;}   // WM_KEYDOWN lp bit 30: auto-repeat
}
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
namespace cv_port_modal {
void register_close(unsigned bit,void (*close_fn)());   // how another window closes this one
// Opens `w` for `bit`: only with the game (or one of our windows) in front, not within 250 ms of the last
// toggle; closes the other windows, makes the game the owner (no TOPMOST), shows and focuses; undone and
// false when the focus did not come.
bool open(unsigned bit,HWND w);
void close(unsigned bit);                  // hides, releases, gives the focus back to the game
void tick(unsigned bit,HWND w);            // from the window's timer: win the focus back from the game
void set_launcher(bool active);            // F5 launcher activated/deactivated
void set_working(unsigned bit,bool on);    // the game may run under this window (a save being written)
unsigned mask();
}
#endif
