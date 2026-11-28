// Win32 side of cv_port_modal.h: the window ownership, foreground rules and the two game-facing writes.
#include "cv_port_modal.h"
#ifdef _WIN32
#include <atomic>
#include <vector>
#include "../../libs/video/rsx_ui_batch.h"
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

// OBS / Window-Capture mirror state: holds the last painted RGBA bitmap of the
// active modal popup (F6 items, F7 archives/maps, F2 title return) and its
// normalized position inside the game window's client area.
SRWLOCK mirror_lock=SRWLOCK_INIT;
HWND mirror_hwnd=nullptr;
bool mirror_active=false;
uint32_t mirror_w=0,mirror_h=0;
float mirror_nx=0,mirror_ny=0,mirror_nw=0,mirror_nh=0;
uint64_t mirror_gen=0;
std::vector<uint32_t> mirror_rgba;
std::atomic<int> mirror_dirty{0};

std::vector<uint32_t> render_rgba;
uint32_t render_w=0,render_h=0;
float render_nx=0,render_ny=0,render_nw=0,render_nh=0;
uint64_t render_gen=0;
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
    if(w){mirror_clear(w);ShowWindow(w,SW_HIDE);}
    if(was&&!switching&&!(core.mask&LAUNCHER)&&(focused||!GetForegroundWindow()))if(HWND g=game())SetForegroundWindow(g);
}
void tick(unsigned bit,HWND w) {
    if(!w||!(core.mask&bit)||!IsWindowVisible(w))return;
    HWND g=game();if(g&&GetForegroundWindow()==g)SetForegroundWindow(w);
}
void set_launcher(bool active) {core.set_launcher(active);recompute();}
void set_working(unsigned bit,bool on) {core.set_working(bit,on);recompute();}
unsigned mask() {return core.mask;}

void mirror_update(HWND h,const uint32_t* bgra,int w,int h_px,int stride_bytes) {
    if(!h||!bgra||w<=0||h_px<=0||w>4096||h_px>4096)return;
    HWND g=game();if(!g)return;
    RECT cr{};if(!GetClientRect(g,&cr)||cr.right<=0||cr.bottom<=0)return;
    RECT wr{};if(!GetWindowRect(h,&wr))return;
    POINT tl{wr.left,wr.top};ScreenToClient(g,&tl);
    const float cw=float(cr.right),ch=float(cr.bottom);
    const float nx=float(tl.x)/cw,ny=float(tl.y)/ch;
    const float nw=float(wr.right-wr.left)/cw,nh=float(wr.bottom-wr.top)/ch;
    const int stride=stride_bytes>0?stride_bytes:w*4;
    AcquireSRWLockExclusive(&mirror_lock);
    mirror_rgba.resize(size_t(w)*size_t(h_px));
    const auto* src_bytes=reinterpret_cast<const uint8_t*>(bgra);
    uint32_t* dst=mirror_rgba.data();
    for(int y=0;y<h_px;y++) {
        const auto* row=reinterpret_cast<const uint32_t*>(src_bytes+size_t(y)*size_t(stride));
        uint32_t* out=dst+size_t(y)*size_t(w);
        for(int x=0;x<w;x++) {
            const uint32_t p=row[x];
            out[x]=0xFF000000u|((p&0xFFu)<<16)|(p&0xFF00u)|((p>>16)&0xFFu);
        }
    }
    mirror_hwnd=h;mirror_active=true;
    mirror_w=uint32_t(w);mirror_h=uint32_t(h_px);
    mirror_nx=nx;mirror_ny=ny;mirror_nw=nw;mirror_nh=nh;
    mirror_gen++;
    ReleaseSRWLockExclusive(&mirror_lock);
    mirror_dirty.store(1,std::memory_order_release);
}
void mirror_clear(HWND h) {
    bool cleared=false;
    AcquireSRWLockExclusive(&mirror_lock);
    if(mirror_active&&(!h||mirror_hwnd==h)) {
        mirror_active=false;mirror_hwnd=nullptr;mirror_gen++;cleared=true;
    }
    ReleaseSRWLockExclusive(&mirror_lock);
    if(cleared)mirror_dirty.store(1,std::memory_order_release);
}
bool mirror_take_dirty() {
    return mirror_dirty.exchange(0,std::memory_order_acq_rel)!=0;
}
}

extern "C" const rsx_ui_sprite* cv_port_modal_mirror_hook(
    uint32_t w,uint32_t h,const uint32_t** rgba,uint32_t* aw,uint32_t* ah,uint64_t* generation) {
    using namespace cv_port_modal;
    AcquireSRWLockShared(&mirror_lock);
    if(!mirror_active||!mirror_w||!mirror_h||mirror_rgba.empty()) {
        ReleaseSRWLockShared(&mirror_lock);
        return nullptr;
    }
    if(render_gen!=mirror_gen) {
        render_rgba=mirror_rgba;
        render_w=mirror_w;render_h=mirror_h;
        render_nx=mirror_nx;render_ny=mirror_ny;render_nw=mirror_nw;render_nh=mirror_nh;
        render_gen=mirror_gen;
    }
    ReleaseSRWLockShared(&mirror_lock);
    static rsx_ui_sprite sp;
    sp.x=render_nx*float(w);sp.y=render_ny*float(h);
    sp.w=render_nw*float(w);sp.h=render_nh*float(h);
    sp.u0=0;sp.v0=0;sp.u1=1;sp.v1=1;
    sp.argb=0xFFFFFFFFu;sp.after=0;
    *rgba=render_rgba.data();*aw=render_w;*ah=render_h;*generation=render_gen;
    return &sp;
}
#endif
