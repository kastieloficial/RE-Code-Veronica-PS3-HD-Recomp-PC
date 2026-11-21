// Win32 side of cv_port_modal.h: the window ownership, foreground rules and the two game-facing writes.
#include "cv_port_modal.h"
#ifdef _WIN32
extern "C" int g_pad_block;
extern "C" void* rsx_d3d12_window(void);
extern "C" void cv_focus_pause_hold(int on);   // cv_focus_pause.cpp
namespace cv_port_modal {
namespace {
Core core;HWND windows[8]={};void (*closers[8])()={};bool switching=false;
HWND game() {return static_cast<HWND>(rsx_d3d12_window());}
bool ours(HWND h) {if(!h)return false;for(HWND w:windows)if(w&&w==h)return true;return false;}
// The only writer of the game-facing state.
void recompute() {g_pad_block=core.pad_block()?1:0;cv_focus_pause_hold(core.pause()?1:0);}
}
void register_close(unsigned bit,void (*close_fn)()) {const int i=Core::index(bit);if(i>=0)closers[i]=close_fn;}
bool open(unsigned bit,HWND w) {
    const int i=Core::index(bit);if(i<0||!w)return false;
    const uint64_t now=GetTickCount64();HWND front=GetForegroundWindow(),g=game();
    // The launcher counts as in front: its own button opens F7.
    if(!front||!(front==g||ours(front)||(core.mask&LAUNCHER)))return false;
    if(!(core.mask&bit)&&core.debounced(bit,now))return false;
    if(const unsigned others=core.exclusive_conflict(bit)) {
        switching=true;
        for(int k=0;k<8;k++)if((others&(1u<<k))&&closers[k])closers[k]();
        switching=false;
        if(core.exclusive_conflict(bit))return false;   // one of them could not close (a save being written)
    }
    if(!core.acquire(bit,now))return false;
    windows[i]=w;
    if(g)SetWindowLongPtrW(w,GWLP_HWNDPARENT,reinterpret_cast<LONG_PTR>(g));   // owner = the game: above it, below other programs
    recompute();
    ShowWindow(w,SW_SHOW);SetForegroundWindow(w);
    if(GetForegroundWindow()!=w) {   // the focus did not come: undo everything
        ShowWindow(w,SW_HIDE);core.release(bit,now);recompute();return false;
    }
    return true;
}
void close(unsigned bit) {
    const int i=Core::index(bit);if(i<0)return;
    const bool was=(core.mask&bit)!=0;HWND w=windows[i];
    const bool focused=w&&GetForegroundWindow()==w;
    core.release(bit,was?GetTickCount64():0);recompute();
    if(w)ShowWindow(w,SW_HIDE);
    if(was&&!switching&&!(core.mask&LAUNCHER)&&(focused||!GetForegroundWindow()))if(HWND g=game())SetForegroundWindow(g);
}
void tick(unsigned bit,HWND w) {
    if(!w||!(core.mask&bit)||!IsWindowVisible(w))return;
    HWND g=game();if(g&&GetForegroundWindow()==g)SetForegroundWindow(w);
}
void set_launcher(bool active) {core.set_launcher(active);recompute();}
void set_working(unsigned bit,bool on) {core.set_working(bit,on);recompute();}
unsigned mask() {return core.mask;}
}
#endif
