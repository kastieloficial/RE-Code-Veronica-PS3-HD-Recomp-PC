// Optional host-side test window. Guest calls are consumed only by the game dispatcher.
#include "cv_test_tools.h"
#include "cv_save_library.h"
#include "cv_archives_ui.h"
#include "cv_archives_i18n.h"
#include "cv_title_flow.h"
#include "cv_port_modal.h"
#include "cv_title_return.h"
#include "cv_version.h"
#include "cv_options.h"
#include "cv_scene_region.h"
#include "ppu_recomp.h"
#include <atomic>
#include <algorithm>
#include <array>
#include <filesystem>
#include <vector>
#include <set>
#include <cstdio>
#include <cstring>
#include <string_view>
#include <cwctype>
#include "cv_launcher_controls.h"
#include "cv_test_tools_i18n.h"
#include "cv_items.h"
#include "cv_manual_reload.h"
#include "cv_quick_select.h"
#ifdef _WIN32
#include <commctrl.h>
#include <dwmapi.h>
#include <uxtheme.h>
#pragma comment(lib,"uxtheme.lib")
#pragma comment(lib,"comctl32.lib")
#pragma comment(lib,"dwmapi.lib")
#endif
#ifdef _WIN32
extern "C" uint8_t* vm_base;
extern "C" PPU_THREAD_LOCAL ppu_context* g_active_ctx;
extern "C" PPU_THREAD_LOCAL void (*g_trampoline_fn)(void*);
extern "C" void* rsx_d3d12_window(void);
extern "C" void perf_overlay_toast(const wchar_t*,int);   // the notice in the game's corner
extern "C" void cv_event_log_note(const char*);           // appends a note to event-log.txt when CV_EVENT_LOG=1
// JP/US: the same functions as EU, matched by their normalized lifted bodies;
// they differ only in global addresses, with the same state and save-object
// offsets (CONTINUAR-MAPAS-E-SAVES.md).
#if defined(CV_REGION_JP)
#define TEST_FRAME func_00032F80
#define TITLE_TASK_TABLE 0x00A79C40u   /* 0xA80000-25536, from the lift of TEST_FRAME (not seen in game) */
#define ORIGINAL_FRAME cv_original_00032F80
#define TEST_WARP func_0018ED00
#define TEST_SERIALIZE func_001D76D0
#define ORIGINAL_SERIALIZE cv_original_001D76D0
#define TEST_SAVE_SCREEN func_001D788C
#define ORIGINAL_SAVE_SCREEN cv_original_001D788C
#define TEST_SAVE_FINISH func_00033B78
#define ORIGINAL_SAVE_FINISH cv_original_00033B78
#define NATIVE_LOAD_SCREEN func_001BFCE0
#define NATIVE_LOAD_VISUAL_RETURN func_001D6174
#define NATIVE_STOP_BANK func_00259C4C
#define SOUND_MANAGER 0x00AB5F64u
#define SCREEN_FADE_CONTROLLER 0x00AB06D8u
#define LOAD_STORAGE 0x00CA7048u
#define LOAD_UI 0x00CA7084u
#define SAVE_OBJECT 0x00AAEE44u    /* EU 0x00ABF0D4: written by func_001D788C (EU func_001D952C) */
#define SAVE_MANAGER 0x00AE9548u   /* EU 0x00AF97C8: read by the AutoSave entry func_00272718 */
#define SAVE_DATA 0x00AB2AD4u      /* EU 0x00AC2D64 */
#define TEST_ROW func_001D63F0
#define ORIGINAL_ROW cv_original_001D63F0
#elif defined(CV_REGION_US)
#define TEST_FRAME func_00032FA0
#define TITLE_TASK_TABLE 0x00A79BDCu   /* 0xA80000-25636, from the lift of TEST_FRAME (not seen in game) */
#define ORIGINAL_FRAME cv_original_00032FA0
#define TEST_WARP func_0018ED68
#define TEST_SERIALIZE func_001D7550
#define ORIGINAL_SERIALIZE cv_original_001D7550
#define TEST_SAVE_SCREEN func_001D770C
#define ORIGINAL_SAVE_SCREEN cv_original_001D770C
#define TEST_SAVE_FINISH func_00033BD0
#define ORIGINAL_SAVE_FINISH cv_original_00033BD0
#define NATIVE_LOAD_SCREEN func_001BFA48
#define NATIVE_LOAD_VISUAL_RETURN func_001D5FF4
#define NATIVE_STOP_BANK func_0025935C
#define SOUND_MANAGER 0x00AB5EA8u
#define SCREEN_FADE_CONTROLLER 0x00AB061Cu
#define LOAD_STORAGE 0x00CA6FC8u
#define LOAD_UI 0x00CA7004u
#define SAVE_OBJECT 0x00AAED88u    /* EU 0x00ABF0D4: written by func_001D770C (EU func_001D952C) */
#define SAVE_MANAGER 0x00AE94C8u   /* EU 0x00AF97C8: read by the AutoSave entry func_00271E28 */
#define SAVE_DATA 0x00AB2A18u      /* EU 0x00AC2D64 */
#define TEST_ROW func_001D6270
#define ORIGINAL_ROW cv_original_001D6270
#else
#define TEST_FRAME func_000333A0
#define TITLE_TASK_TABLE 0x00A89BDCu   /* the task dispatcher's table of OPDs, bits 0..25 */
#define ORIGINAL_FRAME cv_original_000333A0
#define TEST_WARP func_00190700
#define TEST_SERIALIZE func_001D9370
#define ORIGINAL_SERIALIZE cv_original_001D9370
#define TEST_SAVE_SCREEN func_001D952C
#define ORIGINAL_SAVE_SCREEN cv_original_001D952C
#define TEST_SAVE_FINISH func_00033FD0
#define ORIGINAL_SAVE_FINISH cv_original_00033FD0
#define NATIVE_LOAD_SCREEN func_001C1668
#define NATIVE_LOAD_VISUAL_RETURN func_001D7E14
#define NATIVE_STOP_BANK func_0025B32C        /* (manager, bank): stops that bank's voices */
#define SOUND_MANAGER 0x00AC61F4u
#define SCREEN_FADE_CONTROLLER 0x00AC0968u
#define LOAD_STORAGE 0x00CB72C8u
#define LOAD_UI 0x00CB7304u
#define SAVE_OBJECT 0x00ABF0D4u    /* the save screen's object (func_001D952C) */
#define SAVE_MANAGER 0x00AF97C8u   /* the savedata thread's manager */
#define SAVE_DATA 0x00AC2D64u      /* DATA0.DAT's buffer: 15 slots of 0x838 bytes from +30 */
#define TEST_ROW func_001D8090      /* draws one row of the save/load slot list */
#define ORIGINAL_ROW cv_original_001D8090
#endif
void ORIGINAL_FRAME(ppu_context*);
void ORIGINAL_SERIALIZE(ppu_context*);
void ORIGINAL_ROW(ppu_context*);
void ORIGINAL_SAVE_SCREEN(ppu_context*);
void ORIGINAL_SAVE_FINISH(ppu_context*);
namespace {
namespace fs=std::filesystem;
constexpr uint32_t state_base=CV_SRT_CHARACTER-0x94;
std::atomic<bool> enabled{false},ready{false};
// The title menu is ready (F7 Load / Maps may act there) and the Maps-from-title flow is running; both set by TEST_FRAME.
std::atomic<bool> title_ready{false},flow_busy{false};
std::atomic<int> archives_context{cv_title_flow::CTX_NONE};
cv_title_flow::Machine title_flow;   // game thread
uint64_t (*clock_ms)()=[]()->uint64_t{return uint64_t(GetTickCount64());};   // tests replace it
// Zero means no request; bit 31 is set on every published request, including rm_0000.
std::atomic<uint32_t> request{0};
std::atomic<int> result{0}; // 0 idle, 1 queued, 2 native request issued, 3 busy, 4 wrong state, 5 timeout
std::atomic<uint32_t> room_now{0};   // area<<16 | room<<8 | variant, from state +95..+97
std::atomic<bool> room_known{false};  // a live character: room_now is the room she is in
// "Save here": the game's own save screen, opened as the typewriter opens it.
std::atomic<bool> save_request{false},load_request{false};
bool load_cancelled=false,load_visual_returned=false;
unsigned load_watch_frames=0;
struct LoadReturnState {
    static constexpr uint32_t fade_offset=0x2A5CC;
    std::array<uint32_t,24> fade{};
    uint32_t fade_active=0;
    void capture() {
        for(size_t i=0;i<fade.size();++i)fade[i]=vm_read32(state_base+fade_offset+uint32_t(i*4));
        fade_active=vm_read32(state_base+0x74)&2u;
    }
    void restore() const {
        for(size_t i=0;i<fade.size();++i)vm_write32(state_base+fade_offset+uint32_t(i*4),fade[i]);
        vm_write32(state_base+0x74,(vm_read32(state_base+0x74)&~2u)|fade_active);
    }
} load_return;
bool tool_load=false,load_opened=false; // game thread; scoped to F7, including cancellation
bool tool_save=false;   // game thread: the open save screen came from this window
// Saves made from this window, known by a hash of their slot: the slot list
// shows "F7: rm_XXXX" as their location. Nothing is written into the save.
std::set<uint64_t> f7_saves;bool f7_loaded=false;
std::wstring f7_file;   // empty: cv_f7_saves.txt beside the executable
HWND window=nullptr,filter=nullptr,list=nullptr,entry=nullptr,travel=nullptr,save_button=nullptr,load_save_button=nullptr;
std::vector<cv_test::Room> rooms;
std::vector<size_t> visible_rooms;
std::wstring game_root;
uint32_t previous_pad[PAD_PHYS_MAX]={};
ULONGLONG repeat_at[PAD_PHYS_MAX]={};
int language=1;
const wchar_t* tx(cv_test_i18n::Id id) {return cv_test_i18n::k_text[language][id];}
bool mapped(uint32_t address,size_t size) {
    if(!vm_base||size>UINT32_MAX-address)return false;
    uintptr_t at=reinterpret_cast<uintptr_t>(vm_base)+address,end=at+size;
    while(at<end) {MEMORY_BASIC_INFORMATION m{};if(!VirtualQuery(reinterpret_cast<void*>(at),&m,sizeof m)||m.State!=MEM_COMMIT||(m.Protect&(PAGE_NOACCESS|PAGE_GUARD)))return false;
        uintptr_t next=reinterpret_cast<uintptr_t>(m.BaseAddress)+m.RegionSize;if(next<=at)return false;at=next;}
    return true;
}
bool live_character() {
    if(!mapped(state_base,0x2ACA4)||!mapped(CV_SRT_HEALTH,4)||!mapped(CV_SRT_RDX,4))return false;
    // The room header identifies a live character; stale title/game-over data is insufficient.
    static constexpr uint32_t rdx[]={0x3D650,0x3C4FC,0x3A4D0,0x3E814};
    unsigned character=vm_read8(CV_SRT_CHARACTER);
    return character<4&&vm_read32(CV_SRT_RDX)==rdx[character]&&int32_t(vm_read32(CV_SRT_HEALTH))>0;   // 32-bit, as the SRT reads it
}
bool active_game() {
    if(!live_character())return false;
    uint32_t tasks=vm_read32(state_base+0x64),blocked=vm_read32(state_base+0x68);
    return ((tasks&~blocked)&0x80)&&!((tasks&~blocked)&0x1803F)&&!(vm_read32(state_base+0x74)&7)&&!(vm_read32(state_base+0x70)&0x1C040008);
}
// A slot is the game state from +8C: +4 the save count, +8 the character,
// +9/+A/+B the room (area, number, variant); +0 is 0xA in a used slot.
uint32_t slot_address(uint32_t slot) {
    const uint32_t data=mapped(SAVE_DATA,4)?vm_read32(SAVE_DATA):0;
    const uint32_t at=data+0x30+slot*0x838;
    return data&&slot<15&&mapped(at,0x838)&&vm_read32(at)==0xA?at:0;
}
uint64_t slot_hash(uint32_t at) {
    uint64_t h=1469598103934665603ull;
    for(uint32_t i=0;i<0x838;i++){h^=vm_read8(at+i);h*=1099511628211ull;}
    return h;
}
std::wstring f7_path() {
    if(!f7_file.empty())return f7_file;
    wchar_t exe[MAX_PATH]={};GetModuleFileNameW(nullptr,exe,MAX_PATH);
    return (fs::path(exe).parent_path()/L"cv_f7_saves.txt").wstring();
}
void f7_load() {
    if(f7_loaded)return;
    f7_loaded=true;
    if(FILE* f=_wfopen(f7_path().c_str(),L"r")) {
        char id[16];unsigned long long h;
        while(std::fscanf(f,"%15s %llx",id,&h)==2)if(!std::strcmp(id,CV_TITLE_ID))f7_saves.insert(h);
        std::fclose(f);
    }
}
void f7_record(uint32_t slot) {
    const uint32_t at=slot_address(slot);if(!at)return;
    f7_load();const uint64_t h=slot_hash(at);
    if(!f7_saves.insert(h).second)return;
    if(FILE* f=_wfopen(f7_path().c_str(),L"a")){std::fprintf(f,"%s %016llx\n",CV_TITLE_ID,(unsigned long long)h);std::fclose(f);}
    std::printf("[CVTest] tool save in slot %u marked for the slot list\n",slot+1);std::fflush(stdout);
}
// "F7: rm_XXXX" in the font's codes: ASCII - 0x20 (EU/US); the Japanese font
// has only upper case (A = 0) and its digits at 0xD8. FF01 is a space.
std::vector<uint16_t> f7_label(unsigned area,unsigned room,unsigned variant) {
    const unsigned digits[4]={area%10,room/10%10,room%10,variant%10};
    std::vector<uint16_t> text;
#if defined(CV_REGION_JP)
    text={uint16_t('F'-'A'),0xDF,0xFF01,uint16_t('R'-'A'),uint16_t('M'-'A')};
    for(unsigned d:digits)text.push_back(uint16_t(0xD8+d));
#else
    for(char c:std::string_view("F7:"))text.push_back(uint16_t(c-0x20));
    text.push_back(0xFF01);
    for(char c:std::string_view("rm_"))text.push_back(uint16_t(c-0x20));
    for(unsigned d:digits)text.push_back(uint16_t('0'+d-0x20));
#endif
    text.push_back(0xFFFF);
    return text;
}
void call_warp(ppu_context* caller,uint32_t packed) {
    ppu_context work=*caller;
    const auto old_active=g_active_ctx;const auto old_tail=g_trampoline_fn;
    work.gpr[1]-=0x400;vm_write64(work.gpr[1],caller->gpr[1]);work.gpr[2]=CV_TOC;
    work.gpr[3]=0;work.gpr[4]=(packed>>24)&15;work.gpr[5]=(packed>>16)&255;
    work.gpr[6]=packed&255;work.gpr[7]=255; // no door animation
    g_active_ctx=&work;g_trampoline_fn=nullptr;
    vm_write8(state_base+0x97,(packed>>8)&15);
    TEST_WARP(&work);
    while(g_trampoline_fn) {auto next=g_trampoline_fn;g_trampoline_fn=nullptr;next(&work);}
    g_active_ctx=old_active;g_trampoline_fn=old_tail;
    std::printf("[CVTest] warp requested: rm_%u%02u%u entry=%u, on game thread\n",(packed>>24)&15,(packed>>16)&255,(packed>>8)&15,packed&255);std::fflush(stdout);
}
// The warp function with every argument as a script door gives it: r3 the request kind (+2A6A4), r4 area, r5 room,
// r6 the variant of the new room (+2A6AA, read back as +98 and the room's variant), r7 the door animation (+2A6AB).
void call_door(ppu_context* caller,uint32_t kind,uint32_t area,uint32_t room,uint32_t variant,uint32_t animation) {
    ppu_context work=*caller;
    const auto old_active=g_active_ctx;const auto old_tail=g_trampoline_fn;
    work.gpr[1]-=0x400;vm_write64(work.gpr[1],caller->gpr[1]);work.gpr[2]=CV_TOC;
    work.gpr[3]=kind;work.gpr[4]=area;work.gpr[5]=room;work.gpr[6]=variant;work.gpr[7]=animation;
    g_active_ctx=&work;g_trampoline_fn=nullptr;
    TEST_WARP(&work);
    while(g_trampoline_fn) {auto next=g_trampoline_fn;g_trampoline_fn=nullptr;next(&work);}
    g_active_ctx=old_active;g_trampoline_fn=old_tail;
}
std::wstring lower(std::wstring s) {for(auto& c:s)c=wchar_t(towlower(c));return s;}
// The list row of the room the character is in (its variant, else the same
// area and room), or the first row.
int current_row() {
    if(!room_known)return 0;
    const unsigned now=room_now.load();int same_room=-1;
    for(size_t i=0;i<visible_rooms.size();i++) {const unsigned key=rooms[visible_rooms[i]].key();
        if(key==now)return int(i);
        if(same_room<0&&(key>>8)==(now>>8))same_room=int(i);}
    return same_room<0?0:same_room;
}
void refresh_list() {
    wchar_t value[128]={};GetWindowTextW(filter,value,128);std::wstring query=lower(value);
    SendMessageW(list,WM_SETREDRAW,FALSE,0);SendMessageW(list,LB_RESETCONTENT,0,0);visible_rooms.clear();
    for(size_t i=0;i<rooms.size();i++) {auto label=rooms[i].code();
        if(!query.empty()&&lower(label).find(query)==std::wstring::npos)continue;
        SendMessageW(list,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(label.c_str()));visible_rooms.push_back(i);}
    if(!visible_rooms.empty())SendMessageW(list,LB_SETCURSEL,current_row(),0);
    SendMessageW(list,WM_SETREDRAW,TRUE,0);InvalidateRect(list,nullptr,TRUE);
}
void catalog(const std::wstring& game) {
    rooms.clear();std::set<unsigned> seen;std::error_code ec;
    fs::path root=fs::path(game)/L"USRDIR"/L"BHCV"/L"nativePS3"/L"biocv_disc";
    // Prefer English, then add any region-specific rooms absent there. No recursive scan.
    std::vector<fs::path> dirs{root/L"eng"/L"rdx_lnk"};
    for(const auto& d:fs::directory_iterator(root,ec))if(d.is_directory(ec)&&d.path().filename()!=L"eng")dirs.push_back(d.path()/L"rdx_lnk");
    for(const auto& dir:dirs) {ec.clear();for(const auto& item:fs::directory_iterator(dir,ec)) {
        cv_test::Room room;if(!item.is_regular_file(ec)||!cv_test::parse_room(item.path().filename().wstring(),room)||!seen.insert(room.key()).second)continue;
        rooms.push_back(room);}}
    std::sort(rooms.begin(),rooms.end(),[](auto& a,auto& b){return a.key()<b.key();});
    std::printf("[CVTest] map catalog: %zu rooms, %s\n",rooms.size(),CV_TITLE_ID);
}
// ---- The window, in the launcher's colours -------------------------------
// Everything but the two text fields and the list is painted here; the list
// and the buttons are owner-drawn.
constexpr int W=680,MARGIN=24,DY=48,H=640+DY;   // DY: the current-room banner under the title
constexpr COLORREF C_BG=RGB(21,17,17),C_FIELD=RGB(26,22,22),C_FIELD_BD=RGB(70,60,58),C_LINE=RGB(70,28,28),
    C_TEXT=RGB(225,218,210),C_DIM=RGB(150,140,132),C_RED=RGB(196,28,34),C_RED_HI=RGB(230,50,50),C_OK=RGB(120,190,110),C_HOVER=RGB(38,30,30);
enum {ID_FILTER=10,ID_ENTRY,ID_LIST,ID_LOAD,ID_BACK,ID_SAVE,ID_LOAD_SAVE};
const RECT filter_box{MARGIN,122+DY,MARGIN+400,154+DY},entry_box{MARGIN+424,122+DY,W-MARGIN,154+DY};
const RECT list_box{MARGIN,234+DY,W-MARGIN,438+DY},status_box{MARGIN,496+DY,W-MARGIN,520+DY},here_box{MARGIN,80,W-MARGIN,80+36};
HFONT title_font=nullptr,body_font=nullptr,small_font=nullptr,code_font=nullptr;
HBRUSH field_brush=nullptr;
std::wstring status_text;COLORREF status_color=C_DIM;
int hover_button=0;
HFONT font(int px,int weight,const wchar_t* face) {
    return CreateFontW(-px,0,0,0,weight,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,face);
}
void fill(HDC dc,RECT r,COLORREF c) {HBRUSH b=CreateSolidBrush(c);FillRect(dc,&r,b);DeleteObject(b);}
void frame(HDC dc,RECT r,COLORREF c) {HBRUSH b=CreateSolidBrush(c);FrameRect(dc,&r,b);DeleteObject(b);}
void write(HDC dc,const std::wstring& s,RECT r,HFONT f,COLORREF c,UINT format=DT_LEFT|DT_TOP|DT_WORDBREAK) {
    SelectObject(dc,f);SetTextColor(dc,c);SetBkMode(dc,TRANSPARENT);DrawTextW(dc,s.c_str(),-1,&r,format|DT_NOPREFIX);
}
void set_status(const std::wstring& s,COLORREF c) {
    if(s==status_text&&c==status_color)return;
    status_text=s;status_color=c;if(window)InvalidateRect(window,&status_box,FALSE);
}
void send_selected() {
    if(!enabled||!ready||request.load()||save_request.load()||load_request.load())return;
    auto i=SendMessageW(list,LB_GETCURSEL,0,0);if(i==LB_ERR||size_t(i)>=visible_rooms.size())return;
    wchar_t v[12];GetWindowTextW(entry,v,12);wchar_t* end=nullptr;long spawn=wcstol(v,&end,10);
    if(!*v||*end||spawn<0||spawn>255) {set_status(tx(cv_test_i18n::BAD_ENTRY),C_RED_HI);return;}
    const auto& room=rooms[visible_rooms[size_t(i)]];
    uint32_t packed=0x80000000u|(room.stage<<24)|(room.number<<16)|(room.variant<<8)|unsigned(spawn);
    uint32_t empty=0;result=1;if(!request.compare_exchange_strong(empty,packed)){result=3;return;}
    ShowWindow(window,SW_HIDE);cv_port_modal::close(cv_port_modal::TEST_TOOLS);   // legacy window: its own modal bit
}
void send_save() {
    if(!enabled||!ready||request.load()||save_request.load()||load_request.load())return;
    ShowWindow(window,SW_HIDE);cv_save_library_show(true,language);
}
void send_load_save() {
    if(!enabled||!ready||request.load()||save_request.load()||load_request.load())return;
    ShowWindow(window,SW_HIDE);cv_save_library_show(false,language);
}
// Enter and the pad's confirm button must activate the focused action, rather
// than warping to the selected map while the new load-save button has focus.
void confirm_action() {
    const int id=GetDlgCtrlID(GetFocus());
    if(id==ID_LOAD_SAVE)send_load_save();
    else if(id==ID_SAVE)send_save();
    else if(id==ID_BACK)SendMessageW(window,WM_CLOSE,0,0);
    else send_selected();
}
HWND child(HWND parent,const wchar_t* cls,const wchar_t* text,DWORD style,RECT r,int id,HFONT f) {
    HWND c=CreateWindowExW(0,cls,text,WS_CHILD|WS_VISIBLE|style,r.left,r.top,r.right-r.left,r.bottom-r.top,parent,reinterpret_cast<HMENU>(INT_PTR(id)),GetModuleHandleW(nullptr),nullptr);
    SendMessageW(c,WM_SETFONT,reinterpret_cast<WPARAM>(f),TRUE);return c;
}
// A text field sits inside a painted frame, vertically centred.
RECT inside(RECT r) {return RECT{r.left+10,r.top+6,r.right-10,r.bottom-5};}
// "You are in: rm_XXXX · area A · room NN", read from the game every frame.
unsigned here_shown=~0u;   // what the banner last drew; ~0u: no live character
void paint_here(HDC dc) {
    fill(dc,here_box,C_FIELD);fill(dc,RECT{here_box.left,here_box.top,here_box.left+3,here_box.bottom},C_RED_HI);
    RECT r{here_box.left+14,here_box.top,here_box.right-12,here_box.bottom};
    SelectObject(dc,body_font);const std::wstring label=tx(cv_test_i18n::HERE);
    SIZE size{};GetTextExtentPoint32W(dc,label.c_str(),int(label.size()),&size);
    write(dc,label,r,body_font,C_DIM,DT_LEFT|DT_VCENTER|DT_SINGLELINE);
    r.left+=size.cx+10;
    if(here_shown==~0u) {write(dc,L"—",r,code_font,C_DIM,DT_LEFT|DT_VCENTER|DT_SINGLELINE);return;}
    const cv_test::Room room{int(here_shown>>16),int((here_shown>>8)&255),int(here_shown&255)};
    const std::wstring code=room.code();SelectObject(dc,code_font);
    GetTextExtentPoint32W(dc,code.c_str(),int(code.size()),&size);
    write(dc,code,r,code_font,C_TEXT,DT_LEFT|DT_VCENTER|DT_SINGLELINE);
    r.left+=size.cx+12;
    wchar_t where[128];int n=swprintf(where,128,L"· ");
    n+=swprintf(where+n,128-n,tx(cv_test_i18n::WHERE),room.stage,room.number);
    if(room.variant){n+=swprintf(where+n,128-n,L" · ");swprintf(where+n,128-n,tx(cv_test_i18n::EVENT),room.variant);}
    write(dc,where,r,body_font,C_TEXT,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
}
void paint(HWND h) {
    PAINTSTRUCT ps;HDC screen=BeginPaint(h,&ps);
    RECT all{0,0,W,H};HDC dc=CreateCompatibleDC(screen);HBITMAP bmp=CreateCompatibleBitmap(screen,W,H);HGDIOBJ old=SelectObject(dc,bmp);
    fill(dc,all,C_BG);
    fill(dc,RECT{0,0,W,4},C_RED);
    write(dc,tx(cv_test_i18n::TITLE),RECT{MARGIN,20,W-MARGIN,56},title_font,C_RED_HI,DT_LEFT|DT_SINGLELINE);
    write(dc,tx(cv_test_i18n::SUBTITLE),RECT{MARGIN+138,34,W-MARGIN,56},small_font,C_DIM,DT_LEFT|DT_SINGLELINE);
    fill(dc,RECT{MARGIN,66,W-MARGIN,67},C_LINE);
    paint_here(dc);
    write(dc,tx(cv_test_i18n::FILTER),RECT{MARGIN,94+DY,MARGIN+400,118+DY},body_font,C_TEXT,DT_LEFT|DT_SINGLELINE);
    write(dc,tx(cv_test_i18n::ENTRY),RECT{entry_box.left,94+DY,W-MARGIN,118+DY},body_font,C_TEXT,DT_LEFT|DT_SINGLELINE);
    for(RECT r:{filter_box,entry_box}) {fill(dc,r,C_FIELD);frame(dc,r,GetFocus()==(r.left==filter_box.left?filter:entry)?C_RED:C_FIELD_BD);}
    write(dc,tx(cv_test_i18n::ENTRY_HELP),
          RECT{MARGIN,164+DY,W-MARGIN,222+DY},small_font,C_DIM);
    frame(dc,RECT{list_box.left-1,list_box.top-1,list_box.right+1,list_box.bottom+1},C_FIELD_BD);
    write(dc,tx(cv_test_i18n::NOTE),
          RECT{MARGIN,448+DY,W-MARGIN,492+DY},small_font,C_DIM);
    write(dc,status_text,status_box,body_font,status_color,DT_LEFT|DT_SINGLELINE|DT_END_ELLIPSIS);
    BitBlt(screen,0,0,W,H,dc,0,0,SRCCOPY);
    SelectObject(dc,old);DeleteObject(bmp);DeleteDC(dc);EndPaint(h,&ps);
}
void draw_button(const DRAWITEMSTRUCT* d) {
    const bool primary=d->CtlID!=ID_BACK,disabled=(d->itemState&ODS_DISABLED)!=0,hot=hover_button==int(d->CtlID)&&!disabled;
    RECT r=d->rcItem;
    COLORREF bg=primary?(disabled?RGB(70,40,40):hot?C_RED_HI:C_RED):(hot?C_HOVER:C_BG);
    fill(d->hDC,r,bg);if(!primary)frame(d->hDC,r,C_FIELD_BD);
    if(d->itemState&ODS_FOCUS) {RECT f{r.left+3,r.top+3,r.right-3,r.bottom-3};frame(d->hDC,f,primary?RGB(255,200,200):C_RED);}
    write(d->hDC,tx(d->CtlID==ID_LOAD?cv_test_i18n::LOAD:d->CtlID==ID_SAVE?cv_test_i18n::SAVE:d->CtlID==ID_LOAD_SAVE?cv_test_i18n::LOAD_SAVE:cv_test_i18n::BACK),r,body_font,
          disabled?RGB(150,120,120):RGB(245,240,235),DT_CENTER|DT_VCENTER|DT_SINGLELINE);
}
void draw_room(const DRAWITEMSTRUCT* d) {
    if(d->itemID==UINT(-1))return;
    RECT r=d->rcItem;const bool selected=(d->itemState&ODS_SELECTED)!=0;
    fill(d->hDC,r,selected?RGB(70,24,26):(d->itemID&1)?RGB(24,20,20):C_FIELD);
    if(selected)fill(d->hDC,RECT{r.left,r.top,r.left+3,r.bottom},C_RED_HI);
    if(d->itemID>=visible_rooms.size())return;
    const auto& room=rooms[visible_rooms[d->itemID]];
    write(d->hDC,room.code(),RECT{r.left+14,r.top,r.left+130,r.bottom},code_font,C_TEXT,DT_LEFT|DT_VCENTER|DT_SINGLELINE);
    wchar_t where[64];swprintf(where,64,tx(cv_test_i18n::WHERE),room.stage,room.number);
    write(d->hDC,where,RECT{r.left+140,r.top,r.left+330,r.bottom},small_font,C_DIM,DT_LEFT|DT_VCENTER|DT_SINGLELINE);
    if(room.variant) {
        wchar_t tag[64];swprintf(tag,64,tx(cv_test_i18n::EVENT),room.variant);
        write(d->hDC,tag,RECT{r.left+330,r.top,r.right-12,r.bottom},small_font,RGB(214,120,96),DT_RIGHT|DT_VCENTER|DT_SINGLELINE);
    }
}
LRESULT CALLBACK button_proc(HWND b,UINT msg,WPARAM wp,LPARAM lp,UINT_PTR,DWORD_PTR) {
    if(msg==WM_MOUSEMOVE&&hover_button!=GetDlgCtrlID(b)) {
        hover_button=GetDlgCtrlID(b);InvalidateRect(b,nullptr,FALSE);
        TRACKMOUSEEVENT t{sizeof t,TME_LEAVE,b,0};TrackMouseEvent(&t);
    } else if(msg==WM_MOUSELEAVE) {hover_button=0;InvalidateRect(b,nullptr,FALSE);}
    return DefSubclassProc(b,msg,wp,lp);
}
LRESULT CALLBACK proc(HWND h,UINT msg,WPARAM wp,LPARAM lp) {
    switch(msg) {
    case WM_CREATE: {
        filter=child(h,L"EDIT",L"",WS_TABSTOP|ES_AUTOHSCROLL,inside(filter_box),ID_FILTER,body_font);
        entry=child(h,L"EDIT",L"0",WS_TABSTOP|ES_NUMBER|ES_AUTOHSCROLL,inside(entry_box),ID_ENTRY,body_font);
        SendMessageW(entry,EM_LIMITTEXT,3,0);
        list=child(h,L"LISTBOX",L"",WS_TABSTOP|WS_VSCROLL|LBS_NOTIFY|LBS_NOINTEGRALHEIGHT|LBS_OWNERDRAWFIXED|LBS_HASSTRINGS,list_box,ID_LIST,body_font);
        HWND load=child(h,L"BUTTON",L"",WS_TABSTOP|BS_OWNERDRAW,RECT{MARGIN,534+DY,MARGIN+308,574+DY},ID_LOAD,body_font);
        save_button=child(h,L"BUTTON",L"",WS_TABSTOP|BS_OWNERDRAW,RECT{MARGIN+324,534+DY,W-MARGIN,574+DY},ID_SAVE,body_font);
        load_save_button=child(h,L"BUTTON",L"",WS_TABSTOP|BS_OWNERDRAW,RECT{MARGIN,584+DY,MARGIN+308,624+DY},ID_LOAD_SAVE,body_font);
        HWND back=child(h,L"BUTTON",L"",WS_TABSTOP|BS_OWNERDRAW,RECT{MARGIN+324,584+DY,W-MARGIN,624+DY},ID_BACK,body_font);
        travel=load;
        SetWindowTheme(list,L"DarkMode_Explorer",nullptr);   // a dark scroll bar
        for(HWND b:{load,save_button,load_save_button,back})SetWindowSubclass(b,button_proc,1,0);
        SetTimer(h,1,50,nullptr);return 0;}
    case WM_MEASUREITEM:reinterpret_cast<MEASUREITEMSTRUCT*>(lp)->itemHeight=32;return TRUE;
    case WM_DRAWITEM: {
        auto* d=reinterpret_cast<const DRAWITEMSTRUCT*>(lp);
        if(d->CtlType==ODT_LISTBOX)draw_room(d);else draw_button(d);
        return TRUE;}
    case WM_ERASEBKGND:return 1;
    case WM_PAINT:paint(h);return 0;
    case WM_COMMAND:
        if(LOWORD(wp)==ID_FILTER&&HIWORD(wp)==EN_CHANGE)refresh_list();
        if((LOWORD(wp)==ID_FILTER||LOWORD(wp)==ID_ENTRY)&&(HIWORD(wp)==EN_SETFOCUS||HIWORD(wp)==EN_KILLFOCUS)) {
            RECT r=LOWORD(wp)==ID_FILTER?filter_box:entry_box;InflateRect(&r,1,1);InvalidateRect(h,&r,FALSE);}
        if(LOWORD(wp)==ID_LOAD||(LOWORD(wp)==ID_LIST&&HIWORD(wp)==LBN_DBLCLK))send_selected();
        if(LOWORD(wp)==IDOK)confirm_action();
        if(LOWORD(wp)==ID_SAVE)send_save();
        if(LOWORD(wp)==ID_LOAD_SAVE)send_load_save();
        if(LOWORD(wp)==ID_BACK||LOWORD(wp)==IDCANCEL)SendMessageW(h,WM_CLOSE,0,0);return 0;
    case WM_TIMER: {
        if(!IsWindowVisible(h))return 0;
        int r=result.load();auto code=room_now.load();wchar_t line[256];
        if(const unsigned here=room_known?code:~0u;here!=here_shown){here_shown=here;InvalidateRect(h,&here_box,FALSE);}
        if(r==7)set_status(tx(cv_test_i18n::LOADING),C_OK);
        else if(r==6)set_status(tx(cv_test_i18n::SAVING),C_OK);
        else if(r==2) {swprintf(line,256,tx(cv_test_i18n::DONE),(code>>16)&255,(code>>8)&255);set_status(line,C_OK);}
        else if(r==3||r==4)set_status(tx(cv_test_i18n::REFUSED),C_RED_HI);
        else if(ready.load()) {swprintf(line,256,tx(cv_test_i18n::READY),visible_rooms.size());set_status(line,C_OK);}
        else set_status(tx(cv_test_i18n::WAIT),C_DIM);
        const bool available=enabled&&ready&&!request.load()&&!save_request.load()&&!load_request.load();
        EnableWindow(travel,available&&!visible_rooms.empty());
        EnableWindow(load_save_button,available);
        EnableWindow(save_button,SAVE_OBJECT&&available);   // only where the save screen can be released
        if(GetForegroundWindow()!=h)return 0;
        for(int i=0;i<PAD_PHYS_MAX;i++) {PadPhys p{};if(!pad_phys_poll(i,&p)){previous_pad[i]=0;continue;}
            uint32_t edge=p.buttons&~previous_pad[i];previous_pad[i]=p.buttons;
            if(cv_launcher_controls::chord_held(p.buttons))continue;   // a shortcut chord: the launcher acts on it
            if(edge&(1u<<PB_EAST)){SendMessageW(h,WM_CLOSE,0,0);break;}
            if(edge&(1u<<PB_SOUTH)){confirm_action();break;}
            int delta=(p.buttons&(1u<<PB_DOWN))?1:(p.buttons&(1u<<PB_UP))?-1:0;
            if(delta&&((edge&((1u<<PB_DOWN)|(1u<<PB_UP)))||GetTickCount64()>=repeat_at[i])) {int n=int(SendMessageW(list,LB_GETCOUNT,0,0)),sel=int(SendMessageW(list,LB_GETCURSEL,0,0));if(n)SendMessageW(list,LB_SETCURSEL,std::clamp(sel+delta,0,n-1),0);repeat_at[i]=GetTickCount64()+160;}}
        return 0;}
    case WM_KEYDOWN:if(wp==VK_F7||wp==VK_ESCAPE)SendMessageW(h,WM_CLOSE,0,0);else if(wp==VK_RETURN)confirm_action();return 0;
    case WM_CLOSE:ShowWindow(h,SW_HIDE);cv_port_modal::close(cv_port_modal::TEST_TOOLS);return 0;
    case WM_CTLCOLOREDIT:case WM_CTLCOLORLISTBOX:
        SetTextColor(reinterpret_cast<HDC>(wp),C_TEXT);SetBkColor(reinterpret_cast<HDC>(wp),C_FIELD);return reinterpret_cast<LRESULT>(field_brush);
    case WM_DESTROY:window=nullptr;KillTimer(h,1);return 0;
    }
    return DefWindowProcW(h,msg,wp,lp);
}
}
bool cv_test_tools_message(MSG* m) {
    if(cv_save_library_message(m))return true;
    if(!cv_test_tools_visible() || (m->hwnd!=window&&!IsChild(window,m->hwnd)))return false;
    if(m->message==WM_KEYDOWN&&(m->wParam==VK_ESCAPE||m->wParam==VK_F7)){cv_test_tools_close();return true;}
    if(m->message==WM_KEYDOWN&&m->wParam==VK_RETURN){confirm_action();return true;}
    return IsDialogMessageW(window,m)!=FALSE;
}
void cv_test_tools_enable(bool on) {enabled=on;if(!on){request=0;save_request=false;load_request=false;cv_test_tools_close();}}
bool cv_test_tools_visible() {return cv_archives_visible()||(window&&IsWindowVisible(window));}
void cv_test_tools_close() {cv_save_library_close();if(window)SendMessageW(window,WM_CLOSE,0,0);}
// Publish only a room request; the existing game-thread dispatcher performs the warp.
// At the title menu the same request starts a new game and warps once there is control (the game thread decides).
bool cv_archives_travel(unsigned key,unsigned spawn) {
 if(!enabled||(!ready&&!title_ready)||spawn>255||save_request||load_request||flow_busy)return false;
 const uint32_t packed=0x80000000u|(((key>>16)&15)<<24)|(((key>>8)&255)<<16)|((key&15)<<8)|spawn;
 uint32_t empty=0;return request.compare_exchange_strong(empty,packed);
}
void cv_test_tools_show(HWND owner,const std::wstring& game,int lang,int opening_input) {
 if(!enabled)return;if(cv_test_tools_visible()){cv_test_tools_close();return;}(void)owner;
 language=std::clamp(lang,0,12);
 if(game_root!=game||rooms.empty()){game_root=game;catalog(game);}
 cv_archives_maps(rooms,room_known?room_now.load():~0u,cv_archives_travel);
 cv_archives_show(cv_archives_ui::PAGE_LOAD,language,opening_input);   // F7 opens on Load
}
namespace {
const wchar_t* flow_text(cv_archives_i18n::Id id) {return cv_archives_i18n::strings[std::clamp(language,0,12)][id];}
cv_title_flow::Snapshot title_snapshot() {
    cv_title_flow::Snapshot s;
    s.mapped=mapped(state_base,0x84)&&mapped(CV_TITLE_STATE,4);
    if(!s.mapped)return s;
    s.tasks=vm_read32(state_base+0x64);s.blocked=vm_read32(state_base+0x68);s.flags74=vm_read32(state_base+0x74);s.title_state=vm_read32(CV_TITLE_STATE);
    return s;
}
#ifndef CV_DISTRIBUTION
// Dev only (CV_TITLE_PROBE=1): logs every frame where the watched title/menu state changes, and once the task table.
// Capture one natural New Game and one natural Load Game from the main menu to confirm the menu's writes.
void title_probe() {
    static int on=-1;
    if(on<0){const char* e=std::getenv("CV_TITLE_PROBE");on=e&&*e&&*e!='0';}
    if(!on||!mapped(state_base,0x84))return;
    static bool first=true;static uint32_t last[9]={};static unsigned frame=0;++frame;
    if(first) {
        first=false;
        for(unsigned i=0;i<26;i++) {
            const uint32_t opd=mapped(TITLE_TASK_TABLE+i*4,4)?vm_read32(TITLE_TASK_TABLE+i*4):0;
            const uint32_t entry=opd&&mapped(opd,4)?vm_read32(opd):0;
            std::printf("[TITLE-PROBE] task bit %2u (0x%08X) -> opd=%08X func=%08X\n",i,1u<<i,opd,entry);
        }
        std::printf("[TITLE-PROBE] watching +50 +58 +5C +64 +68 +70 +74, difficulty, title state; tasks are +64 & ~+68\n");std::fflush(stdout);
        last[0]=~vm_read32(state_base+0x50);   // forces the first line
    }
    const uint32_t cur[9]={vm_read32(state_base+0x50),vm_read32(state_base+0x58),vm_read16(state_base+0x5C),vm_read32(state_base+0x64),vm_read32(state_base+0x68),
        vm_read32(state_base+0x70),vm_read32(state_base+0x74),vm_read8(CV_SRT_DIFFICULTY),mapped(CV_TITLE_STATE,4)?vm_read32(CV_TITLE_STATE):0};
    if(std::memcmp(cur,last,sizeof cur)==0)return;
    std::memcpy(last,cur,sizeof cur);
    std::printf("[TITLE-PROBE] f=%u +50=%08X +58=%08X +5C=%04X +64=%08X +68=%08X active=%08X +70=%08X +74=%08X difficulty=%u titleState=%08X\n",
        frame,cur[0],cur[1],cur[2],cur[3],cur[4],cur[3]&~cur[4],cur[5],cur[6],cur[7],cur[8]);std::fflush(stdout);
}
#else
inline void title_probe() {}
#endif
// CV_SKIP_EVENTS=1: the elevator of area 7 (rm_701, the car) goes straight to the chosen floor once the floor menu is confirmed.
// Mapped with CV_EVENT_LOG (US, 2026-11-26): the car room is area 7 room 1; the menu keeps the floor at state+0x289DC
// (0 B1F, 1 1F, 2 2F; before the choice it holds the floor of origin) and +74 gains 0x1000 on the confirm, then the ride
// runs 5-7 s and the script changes room. A ride writes no lasting state (system block diff before/after each of the six
// rides), so the same door request the script makes at the end of the ride gives the same result. The script's request,
// logged at +2A6A0 on natural rides: kind 2, area 7, the floor's room and variant, door animation 0x29.
void elevator_skip(ppu_context* ctx) {
    static const bool on=cv_options::enabled("CV_SKIP_EVENTS");
    if(!on||!mapped(state_base,0x289E0))return;
    static bool confirmed=false;
    const uint32_t f74=vm_read32(state_base+0x74);
    const bool now=(f74&0x1000)!=0;
    const bool edge=now&&!confirmed;
    confirmed=now;
    // The same elevator in two maps: Chris's car rm_701 (area 7) and Claire's car rm_316 (area 3, mapped the same way on
    // 2026-11-26: same floor menu and floor word, its floors at rm_315, rm_303 and rm_30C, all at arrival point 0).
    const uint32_t area=vm_read8(state_base+0x95),room=vm_read8(state_base+0x96);
    const bool chris=area==7&&room==1,claire=area==3&&room==0x16;
    if(!edge||(!chris&&!claire)||(f74&7)!=4||!live_character())return;
    // entry: the arrival point, the byte +97 the script writes beside its door request (+94 reads 01 area room entry);
    // Chris's 2F has its elevator door at entry 1, the other floors at 0. Without it the character lands at 0,0,0.
    struct Arrival {uint8_t room,variant,entry;};
    static constexpr Arrival chris_floors[3]={{0x06,2,0},{0x13,2,0},{0x03,1,1}};    // B1F rm_706, 1F rm_713, 2F rm_703
    static constexpr Arrival claire_floors[3]={{0x15,1,0},{0x03,0,0},{0x0C,1,0}};   // B1F rm_315, 1F rm_303, 2F rm_30C
    const uint32_t floor=vm_read32(state_base+0x289DC);
    if(floor>2) {std::printf("[EVENT-SKIP] elevator: floor value %u unknown, ride left as is\n",floor);std::fflush(stdout);return;}
    const Arrival& a=(chris?chris_floors:claire_floors)[floor];
    uint8_t entry=a.entry;
    // Chris 1F (rm_713 v2): rm_7010 runs `04 01 01 5d 00 01` (RE:CVX bytecode: 00 01 = bit clear, 00 00 = bit set)
    // on event flag 0x15D (state+0xC8 bit 0x04, set by Wesker scene rm_70B/rm_710, cleared by rm_7191):
    // 0 -> rm_7190 (entry 0, zombies), 1 -> rm_7191 (entry 1, Seeker/Hunter).
    if(chris&&floor==1)entry=(vm_read32(state_base+0xC8)&0x00000004u)?1:0;
    // At the end of a natural ride the script clears bit 4 of +70 and +74 in the same frame as its door request.
    vm_write32(state_base+0x70,vm_read32(state_base+0x70)&~4u);
    vm_write32(state_base+0x74,vm_read32(state_base+0x74)&~4u);
    vm_write8(state_base+0x97,entry);
    call_door(ctx,2,area,a.room,a.variant,0x29);
    std::printf("[EVENT-SKIP] elevator: floor %u chosen, ride skipped\n",floor);std::fflush(stdout);
    char note[96];std::snprintf(note,sizeof note,"[EVENT-SKIP] elevator (%s): floor %u -> rm_%X%02X v%u entry=%u",chris?"Chris":"Claire",floor,area,a.room,a.variant,unsigned(entry));cv_event_log_note(note);
}
// CV_SKIP_EVENTS=1: the submarine between area 1 and area 4. Inside it (rm_10E at Rockfort, rm_405 at the far dock) its
// panel asks for the other side's inside in variant 1 (door request kind 0: 04 05 01 FF or 01 0E 01 FF), whose room script
// plays the ~10 s voyage and flips the submarine's two lasting flags on its first frame (state+B4 bit 0x00200000 "at
// Rockfort", state+270 bit 0x00800000 "at the far dock"); its ladder then leads out to that side's dock (04 03 01 10 or
// 01 0D 01 10). Mapped with CV_EVENT_LOG (US, Claire, 2026-11-26). Sending the panel straight to the dock set both flags
// right, yet the far dock's hatch stayed shut: the voyage room sets more than those two. So the voyage room is loaded as the
// game loads it, and once its script has started (+2A6A0 3, the flags already flipped) the same inside is loaded again in
// variant 0 (no voyage; its arrival point is the ladder's foot), and the character is put at the panel (15, 0, 36.5), where
// the voyage leaves her. Variant 0 at the far side has its ladder lead to Rockfort (01 0D 01 10); while the submarine is at
// the far dock (+270 bit 0x00800000) that request is turned into the voyage room's own (04 03 01 10).
// CV_SKIP_EVENTS=1: the area 8 lift (rm_804, Chris; mapped with CV_EVENT_LOG, US, 2026-11-26). Its car is the same room in
// variant 0 going up and variant 3 going down; the car's script plays the ride (~16 s up, ~7 s down, +74 bit 4) and ends
// with a door request kind 0, animation 0x21, arrival point 0: up to rm_70D variant 0, down to rm_804 variant 1. The car's
// script sets state+A4 bit 0x08000000 on its first frame both ways (+74 becomes 0x44; +2A6A0 stays 1 in this room) and
// clears it again with its end request going down: going up the bit stays set. Once the script has started (the bit set),
// its own end request is made at once, the bit cleared first going down, as its end does.
// Claire's copy is rm_404 (area 4, mapped 2026-11-26): same car variants and A4 bit; its script's end requests are kind
// 0x60, animation 0x21: up to rm_309 variant 1 at arrival point 1, down to rm_404 variant 1 at arrival point 0.
void lift_skip(ppu_context* ctx) {
    static const bool on=cv_options::enabled("CV_SKIP_EVENTS");
    if(!on||!mapped(state_base,0x2A6AC)||!live_character())return;
    const uint32_t area=vm_read8(state_base+0x95);
    if((area!=8&&area!=4)||vm_read8(state_base+0x96)!=4)return;
    const uint32_t variant=vm_read8(state_base+0x98),f74=vm_read32(state_base+0x74),a4=vm_read32(state_base+0xA4);
    if((variant!=0&&variant!=3)||vm_read32(state_base+0x2A6A0)==0||(f74&7)!=4||!(a4&0x08000000u))return;
    const bool up=variant==0,chris=area==8;
    if(!up)vm_write32(state_base+0xA4,a4&~0x08000000u);
    vm_write32(state_base+0x70,vm_read32(state_base+0x70)&~4u);
    vm_write32(state_base+0x74,f74&~4u);
    vm_write8(state_base+0x97,chris||!up?0:1);
    if(chris) {if(up)call_door(ctx,0,7,0x0D,0,0x21);else call_door(ctx,0,8,0x04,1,0x21);}
    else {if(up)call_door(ctx,0x60,3,0x09,1,0x21);else call_door(ctx,0x60,4,0x04,1,0x21);}
    std::printf("[EVENT-SKIP] airport lift (%s): ride %s skipped\n",chris?"Chris":"Claire",up?"up":"down");std::fflush(stdout);
    char note[96];std::snprintf(note,sizeof note,"[EVENT-SKIP] airport lift (%s): ride %s skipped",chris?"Chris":"Claire",up?"up":"down");cv_event_log_note(note);
}
// CV_SKIP_EVENTS=1: the area 7 cave lift (rm_717, Chris; mapped with CV_EVENT_LOG, US, 2026-11-26). Two scenes:
//  - calling it at the bottom ("Call the elevator? Yes"): +74 becomes 0x3044 and ~8 s later the script sends the character
//    into the car (kind 8, rm_717 variant 2, animation 0x35, arrival 0); the call sets state+CC bit 0x1000 for good.
//  - the ride in the car (rm_717 variant 2 up, variant 3 down; +74 bit 4, ~7 s) ends with kind 0x0A: up to rm_70C variant 4,
//    down to rm_717 variant 1 (animation 0x35, arrival 0); state+274 bit 0x10 ends set at the top, clear at the bottom.
// Each scene's end request is made as soon as it starts, with those flags at the values its end leaves.
void cave_lift_skip(ppu_context* ctx) {
    static const bool on=cv_options::enabled("CV_SKIP_EVENTS");
    if(!on||!mapped(state_base,0x2A6AC)||!live_character())return;
    if(vm_read8(state_base+0x95)!=7||vm_read8(state_base+0x96)!=0x17)return;
    const uint32_t variant=vm_read8(state_base+0x98),f74=vm_read32(state_base+0x74);
    if(vm_read32(state_base+0x2A6A0)==0||(f74&7)!=4)return;
    const bool call=(variant==0||variant==1)&&(f74&0x3040u)==0x3040u,ride=variant==2||variant==3;
    if(!call&&!ride)return;   // any other scene of this room is left alone
    vm_write32(state_base+0x70,vm_read32(state_base+0x70)&~4u);
    vm_write8(state_base+0x97,0);
    if(call) {
        vm_write32(state_base+0xCC,vm_read32(state_base+0xCC)|0x1000u);
        vm_write32(state_base+0x74,f74&~4u);
        call_door(ctx,8,7,0x17,2,0x35);
        std::printf("[EVENT-SKIP] cave lift: call skipped\n");std::fflush(stdout);
        cv_event_log_note("[EVENT-SKIP] cave lift: call skipped");
    } else {
        const bool up=variant==2;const uint32_t f274=vm_read32(state_base+0x274);
        vm_write32(state_base+0x274,up?f274|0x10u:f274&~0x10u);
        vm_write32(state_base+0x74,f74&~4u);
        if(up)call_door(ctx,0x0A,7,0x0C,4,0x35);
        else call_door(ctx,0x0A,7,0x17,1,0x35);
        std::printf("[EVENT-SKIP] cave lift: ride %s skipped\n",up?"up":"down");std::fflush(stdout);
        cv_event_log_note(up?"[EVENT-SKIP] cave lift: ride up skipped":"[EVENT-SKIP] cave lift: ride down skipped");
    }
}
// CV_SKIP_EVENTS=1: the area 9 lift (rm_90A, Chris; mapped with CV_EVENT_LOG, US, 2026-11-26). Its car is rm_90A variant 2,
// entered from either floor; the car's script plays the ride (+74 bit 4, ~5 s) and ends with kind 8, animation 0x43,
// arrival 0, to the other floor: entered from rm_90D it goes to rm_90A variant 3, entered from rm_90A variant 3 it goes to
// rm_90D variant 2. A ride toggles state+D0 bit 0x2000 (clear at the variant 3 floor, set at the rm_90D floor). The room
// the character came from is kept here; once the car's script runs, its end request is made with that flag at its end value.
void area9_lift_skip(ppu_context* ctx,uint32_t previous_room) {
    static const bool on=cv_options::enabled("CV_SKIP_EVENTS");
    if(!on||!mapped(state_base,0x2A6AC)||!live_character())return;
    if(vm_read8(state_base+0x95)!=9||vm_read8(state_base+0x96)!=0x0A||vm_read8(state_base+0x98)!=2)return;
    const uint32_t f74=vm_read32(state_base+0x74);
    if(vm_read32(state_base+0x2A6A0)==0||(f74&7)!=4)return;
    const bool from_90d=(previous_room>>8)==0x090D,from_top=(previous_room>>8)==0x090A&&(previous_room&0xFF)!=2;
    if(!from_90d&&!from_top)return;   // came in some other way: the ride plays as in the original
    const uint32_t d0=vm_read32(state_base+0xD0);
    vm_write32(state_base+0xD0,from_90d?d0&~0x2000u:d0|0x2000u);
    vm_write32(state_base+0x70,vm_read32(state_base+0x70)&~4u);
    vm_write32(state_base+0x74,f74&~4u);
    vm_write8(state_base+0x97,0);
    if(from_90d)call_door(ctx,8,9,0x0A,3,0x43);
    else call_door(ctx,8,9,0x0D,2,0x43);
    std::printf("[EVENT-SKIP] area 9 lift: ride skipped\n");std::fflush(stdout);
    cv_event_log_note(from_90d?"[EVENT-SKIP] area 9 lift: ride to rm_90A v3 skipped":"[EVENT-SKIP] area 9 lift: ride to rm_90D v2 skipped");
}
// CV_SKIP_EVENTS=1: the second area 9 lift (rm_910, Chris; mapped with CV_EVENT_LOG, US, 2026-11-26). Its car is rm_910
// variant 3 going up and variant 2 going down; the car's script plays the ride (+74 bit 4, ~6 s) and ends with kind 8,
// animation 0x4C, arrival 0: up to rm_911 variant 0, down to rm_910 variant 4. A ride changes no lasting state. Once the
// car's script runs, its end request is made at once.
void area9_second_lift_skip(ppu_context* ctx) {
    static const bool on=cv_options::enabled("CV_SKIP_EVENTS");
    if(!on||!mapped(state_base,0x2A6AC)||!live_character())return;
    if(vm_read8(state_base+0x95)!=9||vm_read8(state_base+0x96)!=0x10)return;
    const uint32_t variant=vm_read8(state_base+0x98),f74=vm_read32(state_base+0x74);
    if((variant!=2&&variant!=3)||vm_read32(state_base+0x2A6A0)==0||(f74&7)!=4)return;
    const bool up=variant==3;
    vm_write32(state_base+0x70,vm_read32(state_base+0x70)&~4u);
    vm_write32(state_base+0x74,f74&~4u);
    vm_write8(state_base+0x97,0);
    if(up)call_door(ctx,8,9,0x11,0,0x4C);
    else call_door(ctx,8,9,0x10,4,0x4C);
    std::printf("[EVENT-SKIP] second area 9 lift: ride %s skipped\n",up?"up":"down");std::fflush(stdout);
    cv_event_log_note(up?"[EVENT-SKIP] second area 9 lift: ride up skipped":"[EVENT-SKIP] second area 9 lift: ride down skipped");
}
// CV_SKIP_EVENTS=1: Claire's story ride in the floor elevator (rm_316, after the scene with Steve in rm_315; mapped with
// CV_EVENT_LOG, US, 2026-11-26). The scene's door puts her in the car with +74 bit 0x80 (a scene's door, not a walk-in);
// the car's script then rides up by itself (no floor menu, ~8 s): on its first frame it sets state+BC bit 0x00010000 and
// room bits in +26C, and with its end request (kind 2, rm_30C variant 1, animation 0x29, arrival 0) it clears state+A4
// bit 0x200. The arrival in rm_30C sets the rest itself. Once the car's script has started (the BC bit set), the A4 bit
// is cleared and its end request made at once.
void story_ride_skip(ppu_context* ctx) {
    static const bool on=cv_options::enabled("CV_SKIP_EVENTS");
    static bool from_scene=false;
    if(!on||!mapped(state_base,0x2A6AC)||!live_character())return;
    const bool in_car=vm_read8(state_base+0x95)==3&&vm_read8(state_base+0x96)==0x16&&vm_read8(state_base+0x98)==0;
    const uint32_t f74=vm_read32(state_base+0x74);
    if(!in_car){from_scene=false;return;}
    if(f74&0x80)from_scene=true;   // arrived by the scene's door
    // +74 & 7 is 4 (script) or 6 (script and the fade in): the cut may land while the screen is still dark.
    if(!from_scene||vm_read32(state_base+0x2A6A0)==0||((f74&7)!=4&&(f74&7)!=6))return;
    const uint32_t a4=vm_read32(state_base+0xA4);
    if(!(vm_read32(state_base+0xBC)&0x00010000u)||!(a4&0x200u))return;   // the ride's script not started, or already ended
    // The script's opening (fade in, ~0.8 s) finishes switching the character from Steve to Claire: cutting on its first
    // frame left Claire with Steve's voice (user's test, 2026-11-26); waiting for the end of the fade and y >= 3 fixed it but
    // showed the start of the ride; cutting at the car's first move (y >= 0.05) kept the voice but let the lift's sound
    // start. Now: the first frame of the fade in (+74 bit 2), the car still at rest and the screen dark.
    // Fallbacks if Steve's voice comes back: y >= 0.05 (kept the voice), then (f74&2)==0 and y >= 3.
    if(!(f74&2))return;
    vm_write32(state_base+0xA4,a4&~0x200u);
    vm_write32(state_base+0x70,vm_read32(state_base+0x70)&~4u);
    vm_write32(state_base+0x74,f74&~6u);   // the script bit and, when the cut lands in it, the fade in
    vm_write8(state_base+0x97,0);
    call_door(ctx,2,3,0x0C,1,0x29);
    from_scene=false;
    std::printf("[EVENT-SKIP] Claire's story ride up skipped\n");std::fflush(stdout);
    cv_event_log_note("[EVENT-SKIP] Claire's story ride up skipped");
}
// CV_SKIP_EVENTS=1: the story voyage with Steve (mapped with CV_EVENT_LOG, US, 2026-11-26). After Steve's wheel scene at
// the Rockfort dock, a scene door (kind 0x41) takes Claire and Steve into the submarine (rm_10E variant 0); its script plays
// the voyage (~13 s), flipping the submarine's flags (state+B4, +270) on its first frame, and ends with kind 0x40 to the far
// dock (rm_403 variant 1, animation 0x10, arrival 0), where the next scene plays as usual. Once the voyage's script runs
// (+2A6A0 3) and the far-dock flag is set, its end request is made at once.
void steve_voyage_skip(ppu_context* ctx) {
    static const bool on=cv_options::enabled("CV_SKIP_EVENTS");
    if(!on||!mapped(state_base,0x2A6AC)||!live_character())return;
    if(vm_read8(state_base+0x95)!=1||vm_read8(state_base+0x96)!=0x0E||vm_read8(state_base+0x98)!=0)return;
    const uint32_t f74=vm_read32(state_base+0x74);
    if(vm_read32(state_base+0x2A6A4)!=0x41||vm_read32(state_base+0x2A6A0)!=3||(f74&7)!=4)return;   // came by the scene's door
    if(!(vm_read32(state_base+0x270)&0x00800000u))return;   // the voyage's first frame not run yet
    vm_write32(state_base+0x70,vm_read32(state_base+0x70)&~4u);
    vm_write32(state_base+0x74,f74&~4u);
    vm_write8(state_base+0x97,0);
    call_door(ctx,0x40,4,0x03,1,0x10);
    std::printf("[EVENT-SKIP] Steve's submarine voyage skipped\n");std::fflush(stdout);
    cv_event_log_note("[EVENT-SKIP] Steve's submarine voyage skipped");
}
// CV_SKIP_EVENTS=1: Claire's self-destruct airport lift (rm_404 -> rm_317 -> rm_309; keeps only Alfred's skippable scene).
//  1) Warehouse countdown intro (rm_404 v1/v2): sets state+A4 bit 0x08000000 on its first frame -> send kind 0x62 to rm_317 v0 at once.
//  2) Pre-Alfred lift ride in rm_317 v0: fast-forwarded inside a single frame in cv_fps_experimental.cpp (all regions, no thread-address hacks).
//  3) Post-Alfred lift ride in rm_317 v0: once Alfred's skippable scene (+78 bit 0x10000000) finishes or is skipped (+70 has 0x01004000,
//     no 0x0200, and +78 bit 0x10000000 is gone), send its end request at once (kind 0x60 -> rm_309 v1, anim 0x21, entry 1).
void countdown_lift_skip(ppu_context* ctx) {
    static const bool on=cv_options::enabled("CV_SKIP_EVENTS");
    static bool a4_was=true,alfred_seen=false,pending_c0=false;
    if(!on||!mapped(state_base,0x2A6AC)||!live_character()){a4_was=true;alfred_seen=false;return;}
    const uint32_t area=vm_read8(state_base+0x95),room=vm_read8(state_base+0x96),variant=vm_read8(state_base+0x98);
    const uint32_t f74=vm_read32(state_base+0x74),f70=vm_read32(state_base+0x70),f78=vm_read32(state_base+0x78);
    const bool a4=(vm_read32(state_base+0xA4)&0x08000000u)!=0,started=a4&&!a4_was;
    a4_was=a4;
    // Phase 1: cut the ~3 s unskippable warehouse countdown scene straight into rm_317 v0.
    if(area==4&&room==4&&(variant==1||variant==2)&&started&&(f74&7)==4) {
        vm_write32(state_base+0x70,f70&~4u);vm_write32(state_base+0x74,f74&~4u);vm_write8(state_base+0x97,0);
        call_door(ctx,0x62,3,0x17,0,0x21);
        std::printf("[EVENT-SKIP] countdown lift: warehouse scene skipped\n");std::fflush(stdout);
        cv_event_log_note("[EVENT-SKIP] countdown lift: warehouse scene skipped");
        return;
    }
    // Ensure the story flag (+C0 bit 0x08000000) is present after arriving at rm_309 v1.
    if(pending_c0&&area==3&&room==9&&(f74&7)==0) {
        vm_write32(state_base+0xC0,vm_read32(state_base+0xC0)|0x08000000u);
        pending_c0=false;
    }
    if(area!=3||room!=0x17||variant!=0){alfred_seen=false;return;}
    if(vm_read32(state_base+0x2A6A4)!=0x62||vm_read32(state_base+0x2A6A0)==0)return;
    if(f78&0x10000000u){alfred_seen=true;return;}
    if(!alfred_seen)return;
    if((f74&7)!=4&&(f74&7)!=6)return;
    if((f70&0x01004000u)!=0x01004000u||(f70&0x0200u))return;
    // Phase 3: Alfred's scene ended or was skipped -> cut the final lift ride straight to rm_309 v1.
    alfred_seen=false;pending_c0=true;
    vm_write32(state_base+0x70,f70&~4u);vm_write32(state_base+0x74,f74&~0x10000006u);vm_write8(state_base+0x97,1);
    call_door(ctx,0x60,3,0x09,1,0x21);
    std::printf("[EVENT-SKIP] countdown lift: post-Alfred ride skipped\n");std::fflush(stdout);
    cv_event_log_note("[EVENT-SKIP] countdown lift: post-Alfred ride skipped");
}
void write_guest_float(uint32_t address,float value) {uint32_t bits;std::memcpy(&bits,&value,4);vm_write32(address,bits);}
void submarine_skip(ppu_context* ctx) {
    static const bool on=cv_options::enabled("CV_SKIP_EVENTS");
    static bool place=false;   // variant 0 was loaded by this skip: put the character at the panel once it is there
    if(!on||!mapped(state_base,0x2A6AC)||!live_character())return;
    const uint32_t area=vm_read8(state_base+0x95),room=vm_read8(state_base+0x96),variant=vm_read8(state_base+0x98);
    const bool far_side=area==4&&room==0x05,rockfort=area==1&&room==0x0E;
    const uint32_t step=vm_read32(state_base+0x2A6A0),f74=vm_read32(state_base+0x74);
    if(!far_side&&!rockfort){place=false;return;}
    if(variant==1) {
        if(step!=3||(f74&7)!=4)return;   // the voyage's script running
        // The flags the voyage flips in its first frame: wait for them, so the rest is reached with its own values.
        const bool flipped=far_side?!(vm_read32(state_base+0xB4)&0x00200000u)&&(vm_read32(state_base+0x270)&0x00800000u)
                                   :(vm_read32(state_base+0xB4)&0x00200000u)&&!(vm_read32(state_base+0x270)&0x00800000u);
        if(!flipped)return;
        vm_write32(state_base+0x70,vm_read32(state_base+0x70)&~4u);
        vm_write32(state_base+0x74,f74&~4u);
        vm_write8(state_base+0x97,0);
        call_door(ctx,0,area,room,0,0xFF);
        place=true;
        std::printf("[EVENT-SKIP] submarine: voyage skipped, same inside without it\n");std::fflush(stdout);
        cv_event_log_note(far_side?"[EVENT-SKIP] submarine: voyage to far dock skipped":"[EVENT-SKIP] submarine: voyage to Rockfort skipped");
        return;
    }
    if(place&&step==3&&(f74&7)==0) {   // arrived in variant 0 and in control: to the panel, as after the voyage
        const uint32_t actor=vm_read32(CV_SCENE_ACTOR);
        if(actor&&mapped(actor,0x1C)) {write_guest_float(actor+0x10,15.0f);write_guest_float(actor+0x14,0.0f);write_guest_float(actor+0x18,36.5f);}
        place=false;
    }
    // The far side's ladder in variant 0 while the submarine is there: out to the far dock, as the voyage room's ladder.
    if(far_side&&step==0&&vm_read32(state_base+0x2A6A4)==0&&(f74&1)&&vm_read32(state_base+0x2A6A8)==0x010D0110u&&
       (vm_read32(state_base+0x270)&0x00800000u))
        vm_write32(state_base+0x2A6A8,0x04030110u);
}
// One game frame of "Maps" from the title: accept the request, start the new game, wait for control, warp.
void title_flow_frame(ppu_context* ctx,bool title,bool game_ready) {
    using cv_title_flow::Step;
    if(!title_flow.busy())return;
    cv_title_flow::Inputs in;in.title_ready=title;in.game_ready=game_ready;
    in.active=cv_title_flow::active_tasks(vm_read32(state_base+0x64),vm_read32(state_base+0x68));in.now_ms=clock_ms();
    switch(title_flow.step(in)) {
    case Step::START_NEW_GAME: {
        const auto w=cv_title_flow::new_game_writes(vm_read32(state_base+0x50));
#ifndef CV_DISTRIBUTION
        if(const char* d=std::getenv("CV_TITLE_DIFFICULTY"))if(*d)vm_write8(CV_SRT_DIFFICULTY,uint8_t(std::atoi(d)));   // dev: the probe may show the menu sets it first
#endif
        vm_write32(state_base+0x50,w.flags50);vm_write32(state_base+0x64,w.tasks);
        cv_title_shortcut_started("new game");   // the menu object as its exit leaves it; the gate bits of the game's reset follow
        std::printf("[TITLE-FLOW] new game started from the title menu (+50=%08X +64=%08X); warp after control\n",w.flags50,w.tasks);std::fflush(stdout);
        break;}
    case Step::WARP:
        call_warp(ctx,title_flow.pending);result=2;ready=false;
        std::printf("[TITLE-FLOW] control reached: warp done\n");std::fflush(stdout);break;
    case Step::ABORT_NOT_READY:case Step::ABORT_TIMEOUT:case Step::ABORT_MENU:
        result=4;perf_overlay_toast(flow_text(cv_archives_i18n::TITLE_FLOW_FAILED),0);
        std::printf("[TITLE-FLOW] aborted (%s): nothing more is written\n",in.active&0xC?"menu or title is back":"timeout / not ready");std::fflush(stdout);break;
    default:break;
    }
    flow_busy=title_flow.busy();
}
#if defined(CV_REGION_JP)
constexpr uint32_t kOptMatStatePtr = 0x00AAEB24u;
constexpr uint32_t kOptTexIdAddr   = 0x00ADC968u;
constexpr uint32_t kOptBlendBytes  = 0x00ADCA2Cu;
#elif defined(CV_REGION_US)
constexpr uint32_t kOptMatStatePtr = 0x00AAEA68u;
constexpr uint32_t kOptTexIdAddr   = 0x00ADC8E8u;
constexpr uint32_t kOptBlendBytes  = 0x00ADC9ACu;
#else
constexpr uint32_t kOptMatStatePtr = 0x00ABEDB4u;
constexpr uint32_t kOptTexIdAddr   = 0x00AECBE8u;
constexpr uint32_t kOptBlendBytes  = 0x00AECCACu;
#endif

// Native RECVX HD bug: when the in-game OPTION menu (adv/opt.arc) closes,
// func_0003F768 -> func_001BFF18(base_obj) resets sys+0x1ED24 (the count of
// entries in the default effect texture list sys+0x1ED20) and sys+0x21980 back
// to base_obj+0x64, truncating away the current room's appended particle/effect
// texture slots. Their entries remain valid in memory, so TEX_LOOKUP's loop
// (for i = base_obj+0x64; i < cnt; ++i) runs 0 iterations -> returns 0 ->
// DRAW_EFFECT falls back to PACK_VTX_NOTEX (solid opaque squares) until the
// next room load re-registers them. Preserve and restore both counts (and the
// global njSetTexture state) when returning from OPTION to the same room.
void option_menu_texture_guard() {
    constexpr uint32_t kMenuActive = (CV_MENU_CURSOR - 0x18u) + 0x94u;
    constexpr uint32_t kOffTexCnt  = 0x1ED24u;
    constexpr uint32_t kOffTexCnt2 = 0x21980u;
    if (!mapped(kMenuActive, 1) || !mapped(state_base + kOffTexCnt, 4) ||
        !mapped(state_base + kOffTexCnt2, 4) || !mapped(kOptMatStatePtr, 4) ||
        !mapped(kOptTexIdAddr - 4u, 8) || !mapped(kOptBlendBytes, 2) ||
        !mapped(CV_SRT_ROOM, 2))
        return;

    static bool     was_open      = false;
    static bool     saved_valid   = false;
    static uint16_t saved_room    = 0;
    static uint32_t saved_cnt     = 0;
    static uint32_t saved_cnt2    = 0;
    static uint32_t saved_mat     = 0;
    static uint32_t saved_list    = 0;
    static uint32_t saved_tex     = 0;
    static uint8_t  saved_b0      = 0;
    static uint8_t  saved_b1      = 0;

    const bool open = vm_read8(kMenuActive) != 0u;
    if (open) {
        was_open = true;
        return;
    }

    if (was_open) {
        was_open = false;
        if (saved_valid && vm_read16(CV_SRT_ROOM) == saved_room) {
            const uint32_t cur_cnt  = vm_read32(state_base + kOffTexCnt);
            const uint32_t cur_cnt2 = vm_read32(state_base + kOffTexCnt2);
            const uint32_t cur_mat  = vm_read32(kOptMatStatePtr);
            const uint32_t cur_tex  = vm_read32(kOptTexIdAddr);
            const uint8_t  cur_b0   = vm_read8(kOptBlendBytes);
            const uint8_t  cur_b1   = vm_read8(kOptBlendBytes + 1u);
            bool restored = false;
            if (cur_cnt < saved_cnt) {
                vm_write32(state_base + kOffTexCnt, saved_cnt);
                restored = true;
            }
            if (cur_cnt2 < saved_cnt2) {
                vm_write32(state_base + kOffTexCnt2, saved_cnt2);
                restored = true;
            }
            if (saved_mat != 0u && (cur_mat != saved_mat || cur_tex != saved_tex || cur_b0 != saved_b0 || cur_b1 != saved_b1)) {
                vm_write32(kOptMatStatePtr, saved_mat);
                vm_write32(kOptTexIdAddr - 4u, saved_list);
                vm_write32(kOptTexIdAddr, saved_tex);
                vm_write8(kOptBlendBytes, saved_b0);
                vm_write8(kOptBlendBytes + 1u, saved_b1);
                restored = true;
            }
            if (restored) {
                std::fprintf(stderr,
                    "[OPTION-TEX-FIX] restored room effect textures after OPTION menu "
                    "(room=%04X cnt=%u->%u cnt2=%u->%u mat=%08X->%08X)\n",
                    unsigned(saved_room), cur_cnt, saved_cnt, cur_cnt2, saved_cnt2, cur_mat, saved_mat);
            }
        }
        saved_valid = false;
        return;
    }

    if (live_character()) {
        const uint32_t cnt = vm_read32(state_base + kOffTexCnt);
        if (cnt != 0u) {
            saved_room  = vm_read16(CV_SRT_ROOM);
            saved_cnt   = cnt;
            saved_cnt2  = vm_read32(state_base + kOffTexCnt2);
            saved_mat   = vm_read32(kOptMatStatePtr);
            saved_list  = vm_read32(kOptTexIdAddr - 4u);
            saved_tex   = vm_read32(kOptTexIdAddr);
            saved_b0    = vm_read8(kOptBlendBytes);
            saved_b1    = vm_read8(kOptBlendBytes + 1u);
            saved_valid = true;
        }
    } else {
        saved_valid = false;
    }
}
}
void TEST_FRAME(ppu_context* ctx) {
    cv_title_return_frame(ctx);   // the return to the title after the box's "Yes" (cv_title_return.cpp): before anything else of this frame
    option_menu_texture_guard();
    const bool gameplay=enabled&&active_game()&&!tool_load&&!tool_save;
    const bool title=enabled&&!gameplay&&!tool_load&&cv_title_flow::title_menu_ready(title_snapshot());
    archives_context=cv_title_flow::context_of(gameplay,title);title_ready=title;
    title_probe();
    cv_save_library_frame(ctx,gameplay,title);
    cv_quick::frame(ctx);
    if(!cv_quick::consumes_stick())cv_manual_reload_frame(ctx);
    cv_items::frame();   // the F6 item menu's one write, between game frames
    elevator_skip(ctx);
    submarine_skip(ctx);
    lift_skip(ctx);
    cave_lift_skip(ctx);
    {   // the room before the current one (area, room, variant), for the lifts whose car serves both floors
        static uint32_t current=0,previous=0;
        if(mapped(state_base,0x9C)) {
            const uint32_t key=(uint32_t(vm_read8(state_base+0x95))<<16)|(uint32_t(vm_read8(state_base+0x96))<<8)|vm_read8(state_base+0x98);
            if(key!=current){previous=current;current=key;}
        }
        area9_lift_skip(ctx,previous);
    }
    area9_second_lift_skip(ctx);
    story_ride_skip(ctx);
    steve_voyage_skip(ctx);
    countdown_lift_skip(ctx);
    if(enabled) {
        bool valid=active_game()&&!tool_load;ready=valid;
        room_known=live_character();
        if(room_known)room_now=(vm_read8(state_base+0x95)<<16)|(vm_read8(state_base+0x96)<<8)|vm_read8(state_base+0x97);
        uint32_t pending=request.exchange(0);
        if(pending) {
            if(valid){call_warp(ctx,pending);result=2;ready=false;valid=false;}
            else if(title&&title_flow.request(pending,true,clock_ms())) {   // the title menu: a new game first
                flow_busy=true;result=1;perf_overlay_toast(flow_text(cv_archives_i18n::STARTING),1);
                std::printf("[TITLE-FLOW] request accepted at the title menu\n");std::fflush(stdout);}
            else{result=4;std::printf("[CVTest] warp refused: no controllable live game\n");}
        }
        title_flow_frame(ctx,title,valid);
        // The request the typewriter makes once its ribbon is accepted:
        // func_001AEED4 (EU; JP func_001AD524, US func_001AD47C) sees +74 bit 0x00200000 and opens the save screen.
        if(save_request.exchange(false)) {
            if(valid) {vm_write32(state_base+0x74,vm_read32(state_base+0x74)|0x00200000u);tool_save=true;result=6;ready=false;valid=false;
                std::printf("[CVTest] save screen requested\n");std::fflush(stdout);}
            else {result=4;std::printf("[CVTest] save refused: no controllable live game\n");}
        }
        if(load_request.exchange(false)) {
            if(valid) {
                // First enter the typewriter flow: it loads the system messages
                // and UI assets. Switching to load earlier would skip those assets.
                load_return.capture();load_cancelled=false;load_visual_returned=false;
                tool_load=true;load_opened=false;tool_save=false;
                vm_write32(state_base+0x74,vm_read32(state_base+0x74)|0x00200000u);
                result=7;ready=false;
                std::printf("[CVTest] load screen requested (15 native slots)\n");std::fflush(stdout);
            } else {result=4;std::printf("[CVTest] load refused: no controllable live game\n");}
        }
        // Record natural save entry/exit only while these optional tools are enabled.
        if(mapped(state_base,0x84)) {static uint32_t last_tasks=0;uint32_t tasks=vm_read32(state_base+0x64),active=tasks&~vm_read32(state_base+0x68);
            if((last_tasks&0x8000)&&!(active&0x8000)) {tool_save=false;if(load_opened&&!load_cancelled){tool_load=false;load_opened=false;result=0;}}   // the save screen closed
            // A save from this window in a room with no typewriter: the room has no
            // location name, so the "Saving content" text leaves the slot list's
            // display limit (+1C) as garbage and the minimum display time
            // (func_001D8D70) never ends. Once the file is written (the manager idle,
            // the save object waiting at state 2/4), end it as that timeout does.
            if(tool_save&&(active&0x8000)&&SAVE_OBJECT) {
                const uint32_t o=vm_read32(SAVE_OBJECT),mgr=vm_read32(SAVE_MANAGER);
                // Step 5 follows the copy of the state into the chosen slot (+A0 -> +10),
                // before the file is written: remember that slot as made here.
                if(o&&mapped(o,0xA4)&&vm_read32(o+0x54)==5) {const uint32_t ui=vm_read32(o+0xA0);if(ui&&mapped(ui,0x14))f7_record(vm_read32(ui+0x10));}
                if(o&&mapped(o,0xA4)&&mgr&&mapped(mgr,0x4F0)&&vm_read32(o+0x30)==2&&vm_read32(o+0x58)==4&&vm_read8(o+0x2B)==1&&vm_read32(mgr+0x4E8)==0) {
                    const uint32_t ui=vm_read32(o+0xA0),list=ui&&mapped(ui,0x38)?vm_read32(ui+0x34):0;
                    if(list&&mapped(list,0x28)&&vm_read32(list)&&int32_t(vm_read32(list+0x20))>=8) {
                        vm_write32(list+0x20,0);vm_write32(list,0);vm_write16(list+0x14,0xFFFA);
                        std::printf("[CVTest] tool save written; save screen released\n");std::fflush(stdout);
                    }
                }
            }
            if(((active^last_tasks)&0x18000)!=0)std::printf("[CVTest] native menu/save transition: tasks=%08X blocked=%08X flags=%08X/%08X/%08X saveMode=%u helper=%08X\n",tasks,vm_read32(state_base+0x68),vm_read32(state_base+0x50),vm_read32(state_base+0x70),vm_read32(state_base+0x74),vm_read16(state_base+0x5C),vm_read32(state_base+0x60));last_tasks=active;}
        if(load_watch_frames&&--load_watch_frames%60==0) {
            std::printf("[CVTest] load progress: room=%04X char=%u tasks=%08X blocked=%08X flags=%08X/%08X/%08X roomStep=%08X fade=%08X/%08X/%08X/%08X fadeTime=%08X/%08X delay=%u\n",
                vm_read16(CV_SRT_ROOM),vm_read8(CV_SRT_CHARACTER),vm_read32(state_base+0x64),vm_read32(state_base+0x68),
                vm_read32(state_base+0x50),vm_read32(state_base+0x70),vm_read32(state_base+0x74),vm_read32(state_base+0x1B104),
                vm_read32(state_base+0x2A5D8),vm_read32(state_base+0x2A5E4),vm_read32(state_base+0x2A5F0),vm_read32(state_base+0x2A5FC),vm_read32(state_base+0x2A5CC),vm_read32(state_base+0x2A5D0),vm_read32(state_base+0x2A608));std::fflush(stdout);
        }
    }else{ready=false;if(title_flow.busy()){title_flow.reset();flow_busy=false;}}
    ORIGINAL_FRAME(ctx);
}
// Redirect only an F7 load request, after the native save-mode initializer
// has prepared the shared UI assets. Normal saving still calls its original
// constructor. The mode dispatcher subsequently drives the native load screen.
void TEST_SAVE_SCREEN(ppu_context* ctx) {
    if(!tool_load||load_opened) {ORIGINAL_SAVE_SCREEN(ctx);return;}
    vm_write32(state_base+0x50,vm_read32(state_base+0x50)|0x200u);
    ctx->gpr[3]=LOAD_STORAGE;ctx->gpr[4]=0;
    NATIVE_LOAD_SCREEN(ctx);
    while(g_trampoline_fn) {auto next=g_trampoline_fn;g_trampoline_fn=nullptr;next(ctx);}
    vm_write32(LOAD_UI,uint32_t(ctx->gpr[3]));
    load_opened=true;
    std::printf("[CVTest] native load screen opened\n");std::fflush(stdout);
}
void TEST_SAVE_FINISH(ppu_context* ctx) {
    if(tool_load&&load_opened) {
        if(!load_visual_returned) {
            // Load UI completion (EU 001C1F78) also enables the engine UI
            // fader and sets its sprite to zero opacity. It is independent
            // of the room fader at +2A5CC. Normal save UI never does this.
            // Run the game's matching return routine before either exit path.
            const uint32_t visual=vm_read32(SCREEN_FADE_CONTROLLER);
            const unsigned before=visual&&mapped(visual,0x50)?vm_read8(visual+0x4C):0;
            NATIVE_LOAD_VISUAL_RETURN(ctx);
            while(g_trampoline_fn) {auto next=g_trampoline_fn;g_trampoline_fn=nullptr;next(ctx);}
            load_visual_returned=true;
            std::printf("[CVTest] native load UI visual return: controller=%08X mode=%u -> %u opacity=%u\n",
                visual,before,visual&&mapped(visual,0x50)?vm_read8(visual+0x4C):0,
                visual&&mapped(visual,0x50)?vm_read32(visual+0x40):0);std::fflush(stdout);
        }
        if(!(vm_read32(state_base+0x58)&0x10u)) {
            // Keep ownership until the multi-frame typewriter return finishes.
            // Loading UI ran while the screen was black; its fades must not
            // replace the pre-menu room fade on cancellation.
            vm_write32(state_base+0x50,vm_read32(state_base+0x50)&~0x200u);
            if(!load_cancelled) {load_cancelled=true;
                std::printf("[CVTest] load cancelled; waiting for native room return\n");std::fflush(stdout);}
        } else if(!load_cancelled) {
            // The title-screen load finalizer never clears typewriter flags:
            // it was not entered from gameplay. They block updates of the new
            // room when F7 reuses that finalizer. Clear only these two flags,
            // retaining the confirmed-load bit and the selected slot/state.
            vm_write32(state_base+0x70,vm_read32(state_base+0x70)&~0x08000000u);
            vm_write32(state_base+0x74,vm_read32(state_base+0x74)&~0x00200000u);
            // The title-screen load starts its restart tasks (00300050) with
            // only 0x80 blocked; the restart then builds the gameplay mask.
            // From gameplay, +68 still holds the typewriter's mask and the
            // restart keeps 0x80/0x100 blocked: the room never updates and its
            // fade stays black. Start from the title-screen mask instead.
            vm_write32(state_base+0x68,0x80u);
            // The title screen has no room sounds; gameplay does. The new room's
            // entry (EU func_001B0CB0 step 4/0A, func_001E2314) waits until no
            // voice of sound bank 9 plays, and the old room's looping ambience
            // never ends: the screen stays black. Stop that bank's voices.
            if(const uint32_t sound=mapped(SOUND_MANAGER,4)?vm_read32(SOUND_MANAGER):0) {
                ctx->gpr[3]=sound;ctx->gpr[4]=9;
                NATIVE_STOP_BANK(ctx);
                while(g_trampoline_fn) {auto next=g_trampoline_fn;g_trampoline_fn=nullptr;next(ctx);}
            }
            load_watch_frames=600;
            std::printf("[CVTest] confirmed load: typewriter transition released\n");std::fflush(stdout);
        }
    }
    ORIGINAL_SAVE_FINISH(ctx);
    // The original has finished releasing the UI and resuming gameplay. Restore
    // its actual previous fade, rather than forcing a colour every frame.
    if(tool_load&&load_cancelled&&vm_read16(state_base+0x5C)==0&&
       (vm_read32(state_base+0x68)&0x8000u)&&!(vm_read32(state_base+0x74)&0x00200000u)) {
        load_return.restore();tool_load=false;load_opened=false;load_cancelled=false;result=0;
        std::printf("[CVTest] load cancelled: previous room fade restored, flags=%08X/%08X\n",
            vm_read32(state_base+0x70),vm_read32(state_base+0x74));std::fflush(stdout);
    }
}
void TEST_SERIALIZE(ppu_context* ctx) {
    if(enabled)std::printf("[CVTest] original save serializer: object=%08X room=%04X flags=%08X/%08X/%08X\n",uint32_t(ctx->gpr[3]),vm_read16(CV_SRT_ROOM),vm_read32(state_base+0x50),vm_read32(state_base+0x70),vm_read32(state_base+0x74));
    // A save made from this window spends no ink ribbon and adds nothing to the
    // save count: func_001D90A8 does both after the data was copied, so the
    // file is the same; only the live inventory and counter are put back.
    const uint32_t inventory=CV_SRT_INVENTORY+vm_read8(CV_SRT_CHARACTER)*0x40;
    const bool restore=enabled&&tool_save&&mapped(inventory,48)&&mapped(state_base+0x90,4);
    uint32_t items[12]={},saves=0;
    if(restore) {for(int i=0;i<12;i++)items[i]=vm_read32(inventory+i*4);saves=vm_read32(state_base+0x90);}
    ORIGINAL_SERIALIZE(ctx);
    if(restore) {for(int i=0;i<12;i++)vm_write32(inventory+i*4,items[i]);vm_write32(state_base+0x90,saves);
        std::printf("[CVTest] tool save: ribbon and save count kept\n");std::fflush(stdout);}
}
// A slot saved from this window shows "F7: rm_XXXX" where the location goes.
// The row's location message is drawn from a borrowed entry of the system
// message table (state +1B3F0: text = table + entry[id + 1]), pointed for this
// call at the label, which sits in a stack block above the callee's frames.
void TEST_ROW(ppu_context* ctx) {
    const uint32_t row=uint32_t(ctx->gpr[3]);
    const uint32_t at=mapped(row,0x10)?slot_address(vm_read32(row)-1):0;
    const uint32_t table=at&&mapped(state_base+0x1B3F0,4)?vm_read32(state_base+0x1B3F0):0;
    if(table&&mapped(table,8)) {f7_load();
        if(f7_saves.count(slot_hash(at))) {
            constexpr int8_t id=1;   // any message; restored below
            const auto text=f7_label(vm_read8(at+9),vm_read8(at+0xA),vm_read8(at+0xB));
            const uint64_t sp=ctx->gpr[1],block=sp-0x200;
            const uint32_t label=uint32_t(block+0x100),entry=table+4+id*4;
            vm_write64(block,sp);
            for(size_t i=0;i<text.size();i++)vm_write16(label+uint32_t(i*2),text[i]);
            const uint32_t old_entry=vm_read32(entry);const uint8_t old_id=vm_read8(row+0xD);
            vm_write32(entry,label-table);vm_write8(row+0xD,uint8_t(id));
            ctx->gpr[1]=block;ORIGINAL_ROW(ctx);ctx->gpr[1]=sp;
            vm_write32(entry,old_entry);vm_write8(row+0xD,old_id);
            return;
        }
    }
    ORIGINAL_ROW(ctx);
}
#endif
