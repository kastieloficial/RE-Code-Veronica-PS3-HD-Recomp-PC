// Return to the title screen: the box's window and input (launcher thread) and the game-thread carry-out. See cv_title_return.h.
#include "cv_title_return.h"
#ifdef _WIN32
#include "ppu_recomp.h"
#include "cv_version.h"
#include "cv_title_return_rules.h"
#include "cv_title_return_ui.h"
#include "cv_title_return_i18n.h"
#include "cv_port_modal.h"
#include "cv_port_ui.h"
#include "cv_launcher_controls.h"
#include <windowsx.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
extern "C" uint8_t* vm_base;
extern "C" PPU_THREAD_LOCAL ppu_context* g_active_ctx;
extern "C" PPU_THREAD_LOCAL void (*g_trampoline_fn)(void*);
extern "C" void* rsx_d3d12_window(void);
extern "C" void perf_overlay_toast(const wchar_t*,int);
void CV_FN_TITLE_RETURN_TASK(ppu_context*);   // the Game Over return task of this region (cv_version.h)
void CV_FN_STOP_BANK(ppu_context*);           // (sound manager, bank): stops that bank's voices
namespace {
namespace rules=cv_title_return;
namespace ui=cv_title_return_ui;
namespace tr=cv_title_return_i18n;
constexpr uint32_t state=CV_SRT_CHARACTER-0x94;
constexpr uint32_t F2A70C=state+0x2A70C;   // the Game Over task's "fade started" halfword
const wchar_t* text(int lang,tr::Id id) {return tr::tr(lang,id);}

bool mapped(uint32_t a,size_t n) {
    if(!vm_base||!a||n>UINT32_MAX-a)return false;
    const uint8_t* p=vm_base+a,*end=p+n;
    while(p<end){MEMORY_BASIC_INFORMATION m{};if(!VirtualQuery(p,&m,sizeof m)||m.State!=MEM_COMMIT||(m.Protect&(PAGE_NOACCESS|PAGE_GUARD)))return false;
        const auto* next=static_cast<const uint8_t*>(m.BaseAddress)+m.RegionSize;if(next<=p)return false;p=next;}
    return true;
}
// A snapshot of the system block, read-only (the box opens from the UI thread; Yes is checked again on the game thread).
rules::State read_state() {
    rules::State s;
    if(!mapped(state,0x2A710)||!mapped(CV_SRT_HEALTH,4)||!mapped(CV_SRT_RDX,4)||!mapped(CV_SRT_CHARACTER,1))return s;
    s.mapped=true;
    static constexpr uint32_t rdx[]={0x3D650,0x3C4FC,0x3A4D0,0x3E814};
    const unsigned character=vm_read8(CV_SRT_CHARACTER);
    s.health=int32_t(vm_read32(CV_SRT_HEALTH));
    s.live_character=character<4&&vm_read32(CV_SRT_RDX)==rdx[character];
    s.tasks=vm_read32(state+0x64);s.blocked=vm_read32(state+0x68);s.flags70=vm_read32(state+0x70);s.flags74=vm_read32(state+0x74);
    return s;
}
const char* verdict_name(rules::Verdict v) {
    switch(v){
    case rules::Verdict::GAME:return "game";case rules::Verdict::INVENTORY_SCREEN:return "inventory screen";
    case rules::Verdict::UNREADABLE:return "guest not readable";case rules::Verdict::MENU_OR_TITLE:return "title or menu";
    case rules::Verdict::SAVE_LOAD:return "saving or loading";case rules::Verdict::GAME_OVER:return "game over / no character";
    case rules::Verdict::TRANSITION:return "door or room change";case rules::Verdict::EVENT:return "cutscene or event";
    case rules::Verdict::OTHER_SCREEN:return "another screen";case rules::Verdict::INVENTORY_REFUSED:return "inventory screen (refused)";
    }
    return "?";
}

// ---- Game thread ----------------------------------------------------------------------
std::atomic<bool> request{false};
std::atomic<int> request_language{1};
rules::Machine machine;             // game thread only
bool from_inventory=false;
// A guest call from the game thread: the same helper as cv_save_library.cpp (own stack, drained trampoline).
struct Call {ppu_context c;ppu_context* previous;void(*tail)(void*);Call(ppu_context* p):c(*p),previous(g_active_ctx),tail(g_trampoline_fn){c.gpr[1]-=0x2000;vm_write64(c.gpr[1],p->gpr[1]);c.gpr[2]=CV_TOC;g_active_ctx=&c;g_trampoline_fn=nullptr;}
 ~Call(){g_active_ctx=previous;g_trampoline_fn=tail;}void run(void(*fn)(ppu_context*),uint32_t a=0,uint32_t b=0){c.gpr[3]=a;c.gpr[4]=b;fn(&c);while(g_trampoline_fn){auto next=g_trampoline_fn;g_trampoline_fn=nullptr;next(&c);}}};
void log_state(const char* what) {
    std::fprintf(stderr,"[TITLE-RETURN] %s: tasks=%08X blocked=%08X flags50=%08X flags70=%08X flags74=%08X\n",what,
        vm_read32(state+0x64),vm_read32(state+0x68),vm_read32(state+0x50),vm_read32(state+0x70),vm_read32(state+0x74));
}
void finish(ppu_context* ctx) {
    // The sound bank the way a load stops it, then the typewriter's flags the Game Over path may have left.
    {Call call(ctx);if(const uint32_t sound=vm_read32(CV_SOUND_MANAGER))call.run(CV_FN_STOP_BANK,sound,9);}
    const uint32_t f70=vm_read32(state+0x70),f74=vm_read32(state+0x74);
    if(f70&rules::F70_TYPEWRITER)vm_write32(state+0x70,f70&~rules::F70_TYPEWRITER);
    if(f74&rules::F74_SAVE_REQUEST)vm_write32(state+0x74,f74&~rules::F74_SAVE_REQUEST);
    log_state(from_inventory?"back at the main menu (from the inventory screen)":"back at the main menu");
}

// ---- The window (launcher thread) --------------------------------------------------------
HWND window=nullptr;ui::View view;float scale=1;int painted_kind=PAD_INPUT_KEYBOARD;
uint32_t previous_pad[PAD_PHYS_MAX]={};
void repaint() {if(window)InvalidateRect(window,nullptr,FALSE);}
void note_input(int kind) {kind=cv_port_ui::normalize_kind(kind);if(view.input!=kind){view.input=kind;repaint();}}
void close_box() {if(window)cv_port_modal::close(cv_port_modal::TITLE_RETURN);view.mouse.leave();}
void paint(HWND h) {
    painted_kind=cv_port_ui::hint_kind(view.input);
    PAINTSTRUCT ps;HDC dc=BeginPaint(h,&ps);RECT c;GetClientRect(h,&c);
    const int w=std::max<int>(1,c.right),hh=std::max<int>(1,c.bottom);
    {Gdiplus::Bitmap back(w,hh,PixelFormat32bppARGB);
     {Gdiplus::Graphics g(&back);g.ScaleTransform(scale,scale);ui::draw(g,view);}
     Gdiplus::Graphics out(dc);out.DrawImage(&back,Gdiplus::Rect(0,0,w,hh),0,0,w,hh,Gdiplus::UnitPixel);}
    EndPaint(h,&ps);
}
int id_at(int px,int py) {   // the last painted control under the point (back_rect comes from the paint)
    const auto list=ui::hits(view);const ui::Hit* hit=ui::hit_at(list,px/scale,py/scale);return hit?hit->kind:-1;
}
void select(int choice) {if(view.selected!=choice){view.selected=choice;repaint();}}
// Yes: the box closes first (the game runs again and gets the pad back), then the game thread is asked.
void confirm() {
    if(view.selected!=ui::CHOICE_YES){close_box();return;}
    const int lang=view.language;
    close_box();
    request_language=lang;request=true;
    perf_overlay_toast(text(lang,tr::RETURNING),1);
    std::fprintf(stderr,"[TITLE-RETURN] confirmed in the box: request filed for the game thread\n");
}
void activate(int id) {
    if(id==ui::HIT_YES){view.selected=ui::CHOICE_YES;confirm();}
    else if(id==ui::HIT_NO||id==ui::HIT_BACK){view.selected=ui::CHOICE_NO;close_box();}
}
LRESULT CALLBACK proc(HWND h,UINT msg,WPARAM wp,LPARAM lp) {
    switch(msg) {
    case WM_ERASEBKGND:return 1;
    case WM_PAINT:paint(h);return 0;
    case WM_CLOSE:close_box();return 0;
    case WM_KEYDOWN:
        note_input(PAD_INPUT_KEYBOARD);
        switch(wp) {
        case VK_LEFT:case 'A':select(ui::CHOICE_YES);break;
        case VK_RIGHT:case 'D':select(ui::CHOICE_NO);break;
        case VK_TAB:select(view.selected==ui::CHOICE_YES?ui::CHOICE_NO:ui::CHOICE_YES);break;
        case VK_RETURN:case VK_SPACE:if(!cv_port_modal::key_repeat(lp))confirm();break;
        case VK_ESCAPE:case VK_F2:if(!cv_port_modal::key_repeat(lp)){view.selected=ui::CHOICE_NO;close_box();}break;   // F2 again = No
        }
        return 0;
    case WM_MOUSEMOVE:view.mouse.arm(h);if(view.mouse.move(id_at(GET_X_LPARAM(lp),GET_Y_LPARAM(lp))))repaint();return 0;
    case WM_MOUSELEAVE:if(view.mouse.leave())repaint();return 0;
    case WM_LBUTTONDOWN:{
        note_input(PAD_INPUT_KEYBOARD);
        const int id=id_at(GET_X_LPARAM(lp),GET_Y_LPARAM(lp));
        if(id>=0){SetCapture(h);if(view.mouse.down(id))repaint();}
        return 0;}
    case WM_LBUTTONUP:
        if(view.mouse.pressed>=0){
            ReleaseCapture();int fired=-1;
            const bool changed=view.mouse.up(id_at(GET_X_LPARAM(lp),GET_Y_LPARAM(lp)),fired);
            if(fired>=0){activate(fired);repaint();}   // the action fires on release over the same control
            else if(changed)repaint();
        }
        return 0;
    case WM_TIMER:{
        cv_port_modal::tick(cv_port_modal::TITLE_RETURN,h);   // the game window took the focus: win it back
        if(!IsWindowVisible(h))return 0;
        if(painted_kind!=cv_port_ui::hint_kind(view.input))repaint();
        if(GetForegroundWindow()!=h)return 0;
        for(int i=0;i<PAD_PHYS_MAX;i++){
            PadPhys p{};if(!pad_phys_poll(i,&p)){previous_pad[i]=0;continue;}
            cv_port_ui::menu_pad(p);   // Xbox 360 layout, left stick -> d-pad
            const uint32_t edge=p.buttons&~previous_pad[i];previous_pad[i]=p.buttons;
            // The shortcut chords (R1+R2+L1+L2 + buttons, this box's own included) belong to the launcher: nothing here while both triggers are held.
            if(cv_launcher_controls::chord_held(p.buttons))continue;
            if(edge)note_input(p.kind);
            if(edge&(1u<<PB_EAST)){view.selected=ui::CHOICE_NO;close_box();return 0;}
            if(edge&(1u<<PB_LEFT))select(ui::CHOICE_YES);
            if(edge&(1u<<PB_RIGHT))select(ui::CHOICE_NO);
            if(edge&(1u<<PB_SOUTH)){confirm();return 0;}
        }
        return 0;}
    case WM_DESTROY:window=nullptr;return 0;
    }
    return DefWindowProcW(h,msg,wp,lp);
}
void place_window() {
    HWND game=static_cast<HWND>(rsx_d3d12_window());
    scale=cv_port_ui::scale_for(game,ui::W,ui::H);
    const int w=int(std::lround(ui::W*scale)),h=int(std::lround(ui::H*scale));
    const RECT r=cv_port_ui::center_on_game(game,w,h);
    SetWindowPos(window,nullptr,r.left,r.top,w,h,SWP_NOACTIVATE|SWP_NOZORDER);   // owned by the game, never TOPMOST
}
}  // namespace

void cv_title_return_toggle(int language,int opening_input) {
    language=std::clamp(language,0,12);
    if(cv_title_return_visible()){cv_title_return_close();return;}
    const rules::Verdict verdict=rules::evaluate(read_state());
    if(!rules::allowed(verdict)) {
        perf_overlay_toast(text(language,tr::BLOCKED),0);
        std::fprintf(stderr,"[TITLE-RETURN] not available now (%s)\n",verdict_name(verdict));
        return;
    }
    if(!window) {
        WNDCLASSW wc{};wc.lpfnWndProc=proc;wc.hInstance=GetModuleHandleW(nullptr);wc.hCursor=LoadCursorW(nullptr,MAKEINTRESOURCEW(32512));
        wc.lpszClassName=L"CVTitleReturn";wc.hIcon=LoadIconW(wc.hInstance,MAKEINTRESOURCEW(1));RegisterClassW(&wc);
        // Owned by the game window, never TOPMOST: above the game, below other programs.
        window=CreateWindowExW(WS_EX_TOOLWINDOW,wc.lpszClassName,L"Code: Veronica",WS_POPUP,0,0,ui::W,ui::H,static_cast<HWND>(rsx_d3d12_window()),nullptr,wc.hInstance,nullptr);
        if(!window)return;
    }
    cv_port_ui::fonts_available();   // the embedded fonts load before any Graphics of this window exists
    view.language=language;view.selected=ui::CHOICE_NO;view.mouse.leave();
    view.input=cv_port_ui::normalize_kind(opening_input==PAD_KIND_NONE?PAD_INPUT_KEYBOARD:opening_input);
    cv_port_modal::register_close(cv_port_modal::TITLE_RETURN,cv_title_return_close);
    place_window();
    // Owner = the game, shown and focused, the game stands still and ignores the pad; closes F6 / F7; refused when the game is not in front.
    if(!cv_port_modal::open(cv_port_modal::TITLE_RETURN,window))return;
    from_inventory=verdict==rules::Verdict::INVENTORY_SCREEN;
    SetFocus(window);
    for(int i=0;i<PAD_PHYS_MAX;i++){PadPhys p{};pad_phys_poll(i,&p);cv_port_ui::menu_pad(p);previous_pad[i]=p.buttons;}   // a chord still held opens nothing
    SetTimer(window,1,16,nullptr);painted_kind=cv_port_ui::hint_kind(view.input);repaint();
}
void cv_title_return_close() {if(cv_title_return_visible())close_box();}
bool cv_title_return_visible() {return window&&IsWindowVisible(window);}
void cv_title_return_release() {if(window)DestroyWindow(window);cv_port_ui::release_fonts();}

// One game frame, before the game's own dispatcher: the request after "Yes", then the Game Over return task each frame until the
// game is at its main menu. The task is the one the game runs after a Game Over (fade out, clean the room up, then
// system+0x64 = 0x300048): the same ending, started by us instead of by dying.
void cv_title_return_frame(ppu_context* ctx) {
    const bool requested=!machine.running&&request.exchange(false);
    if(!machine.running&&!requested)return;
    const rules::State now=read_state();
    const uint32_t tasks=now.mapped?now.tasks:0;
    const rules::Verdict verdict=rules::evaluate(now);
    switch(machine.step(requested,rules::allowed(verdict),tasks)) {
    case rules::Step::IDLE:return;
    case rules::Step::ABORT:
        if(requested){perf_overlay_toast(text(request_language.load(),tr::BLOCKED),0);std::fprintf(stderr,"[TITLE-RETURN] refused on the game thread (%s)\n",verdict_name(verdict));}
        else {std::fprintf(stderr,"[TITLE-RETURN] gave up after %u frames without reaching the main menu\n",rules::MAX_FRAMES);log_state("aborted");}
        return;
    case rules::Step::START:
        from_inventory=verdict==rules::Verdict::INVENTORY_SCREEN;
        vm_write16(F2A70C,0);   // the task's "fade started" mark is only reset when a new game starts: clear it once
        log_state(from_inventory?"started (from the inventory screen)":"started");
        break;
    case rules::Step::DONE:finish(ctx);return;
    case rules::Step::CALL:break;
    }
    {Call call(ctx);call.run(CV_FN_TITLE_RETURN_TASK);}
    if(rules::returned(vm_read32(state+0x64))){machine.finish();finish(ctx);}
}
#endif
