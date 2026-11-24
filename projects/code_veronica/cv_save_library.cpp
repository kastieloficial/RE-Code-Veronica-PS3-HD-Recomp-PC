// Native progress library: independent files, never modifies DATA0.DAT.
#include "cv_save_library.h"
#ifdef _WIN32
#include "ppu_recomp.h"
#include "cv_version.h"
#include "cv_save_rooms.h"
#include "cv_launcher_controls.h"
#include "cv_test_tools.h"
#include "cv_test_tools_i18n.h"
#include "cv_archives_i18n.h"
#include "cv_srt_i18n.h"
#include "cv_port_modal.h"
#include "cv_port_ui.h"
#include "cv_archives_ui.h"
#include "cv_title_flow.h"
#include "cv_title_return.h"
#include <cwctype>
#include <algorithm>
#include <array>
#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <thread>
#include <vector>
#include <chrono>
#include <cmath>
#include <dwmapi.h>
#include <windowsx.h>
#include <commctrl.h>
#include <cstdlib>
extern "C" uint8_t* vm_base;
extern "C" PPU_THREAD_LOCAL ppu_context* g_active_ctx;
extern "C" PPU_THREAD_LOCAL void (*g_trampoline_fn)(void*);
extern "C" void* rsx_d3d12_window(void);
extern "C" void perf_overlay_toast(const wchar_t*,int);
extern "C" int rsx_d3d12_request_thumbnail(void (*)(void*,const uint32_t*,unsigned,unsigned),void*);

#if defined(CV_REGION_JP)
#define SAVE_DATA_PTR 0x00AB2AD4u
#define PLAYER_PTR 0x00A79C3Cu
#define STOP_BANK func_00259C4C
#define LOAD_BEGIN func_001CE9E0
#define SOUND_PTR 0x00AB5F64u
#elif defined(CV_REGION_US)
#define SAVE_DATA_PTR 0x00AB2A18u
#define PLAYER_PTR 0x00A79BD8u
#define STOP_BANK func_0025935C
#define LOAD_BEGIN func_001CE860
#define SOUND_PTR 0x00AB5EA8u
#else
#define SAVE_DATA_PTR 0x00AC2D64u
#define PLAYER_PTR 0x00A89BD8u
#define STOP_BANK func_0025B32C
#define LOAD_BEGIN func_001D0680
#define SOUND_PTR 0x00AC61F4u
#endif
void STOP_BANK(ppu_context*);void LOAD_BEGIN(ppu_context*);
namespace {
namespace fs=std::filesystem;
constexpr uint32_t state=CV_SRT_CHARACTER-0x94;
constexpr size_t slot_size=0x838,common_size=0x34,head_size=128;
constexpr unsigned tw=320,th=180;

constexpr UINT WM_DONE=WM_APP+432;
struct Record {fs::path path;uint64_t time=0;unsigned stage=0,room=0,variant=0,character=0,seconds=0;int legacy=-1;};
struct Data {std::array<uint8_t,slot_size> slot{};std::array<uint8_t,common_size> common{};uint32_t option=0;};
struct Pending {bool save=false;fs::path replace;Data data;};
std::mutex queue_lock,photo_lock;std::condition_variable photo_ready;Pending queued;bool have_request=false;
std::atomic<bool> busy{false},available{false};std::atomic<HWND> notification{nullptr};
std::atomic<uintptr_t> sequence{0};uintptr_t photo_id=0;std::vector<uint32_t> photo;
HWND window=nullptr;bool saving=false;std::atomic<int> language{1};int selected=0,scroll=0;
std::vector<Record> entries;std::map<std::wstring,std::vector<uint32_t>> thumbnails;
std::wstring status;
uint32_t previous_pad[PAD_PHYS_MAX]={};ULONGLONG repeat_at[PAD_PHYS_MAX]={};

uint32_t be(const uint8_t* p){return uint32_t(p[0])<<24|uint32_t(p[1])<<16|uint32_t(p[2])<<8|p[3];}
void putbe(uint8_t* p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=uint8_t(v>>(24-i*8));}
uint32_t le(const uint8_t* p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
void put(std::vector<uint8_t>& b,size_t p,uint32_t v){for(unsigned i=0;i<4;i++)b[p+i]=uint8_t(v>>(i*8));}
uint64_t time_value(){FILETIME t;GetSystemTimeAsFileTime(&t);return uint64_t(t.dwLowDateTime)|(uint64_t(t.dwHighDateTime)<<32);}
uint32_t crc(const uint8_t* p,size_t n){uint32_t h=~0u;for(size_t i=0;i<n;i++){h^=p[i];for(unsigned j=0;j<8;j++)h=(h>>1)^(0xedb88320u&uint32_t(-int(h&1)));}return ~h;}
bool mapped(uint32_t a,size_t n){if(!vm_base||!a||n>UINT32_MAX-a)return false;const uint8_t* p=vm_base+a,*end=p+n;
 while(p<end){MEMORY_BASIC_INFORMATION m{};if(!VirtualQuery(p,&m,sizeof m)||m.State!=MEM_COMMIT||(m.Protect&(PAGE_NOACCESS|PAGE_GUARD)))return false;
 const auto* next=static_cast<const uint8_t*>(m.BaseAddress)+m.RegionSize;if(next<=p)return false;p=next;}return true;}
fs::path root(){const char* env=std::getenv("PS3_SAVE_ROOT");return env&&*env?fs::u8path(env):fs::path(L"./gamedata/dev_hdd0/home/00000001/savedata");}
fs::path library(){return root()/L"CodeVeronicaLibrary"/CV_TITLE_ID;}
bool valid_slot(const Data& d){
 if(be(d.slot.data())!=10||d.slot[8]>=4||d.slot[9]>=10||d.slot[10]>=100||d.slot[11]>=10)return false;
 for(unsigned axis=0;axis<3;axis++){uint32_t bits=be(d.slot.data()+0x804+axis*4);float f;std::memcpy(&f,&bits,4);if(!std::isfinite(f)||std::abs(f)>1000000)return false;}
 return true;
}
Record metadata(const Data& d){Record r;r.time=time_value();r.character=d.slot[8];r.stage=d.slot[9];r.room=d.slot[10];r.variant=d.slot[11];r.seconds=be(d.slot.data()+CV_SRT_TIME-state-0x8c)/60;return r;}
bool header(const uint8_t* h){return !std::memcmp(h,"CVSAVE1",8)&&le(h+8)==1&&le(h+12)==head_size&&!std::memcmp(h+16,CV_TITLE_ID,std::strlen(CV_TITLE_ID))&&le(h+40)==slot_size+common_size+4&&le(h+44)==tw*th*4;}
bool read(const Record& r,Data& d,std::vector<uint32_t>* image=nullptr){
 std::ifstream f(r.path,std::ios::binary|std::ios::ate);if(!f)return false;const auto n=f.tellg();
 if(r.legacy>=0){if(n!=0x7c84)return false;f.seekg(8+size_t(r.legacy)*slot_size);f.read((char*)d.slot.data(),slot_size);f.seekg(0x7b50);f.read((char*)d.common.data(),common_size);f.seekg(0x7b84);uint8_t opt[4]{};f.read((char*)opt,4);d.option=be(opt);return bool(f)&&valid_slot(d);}
 const size_t expected=head_size+slot_size+common_size+4+tw*th*4;if(n!=std::streamoff(expected))return false;
 std::vector<uint8_t> b(expected);f.seekg(0);f.read((char*)b.data(),b.size());if(!f||!header(b.data()))return false;
 const uint32_t check=le(b.data()+48);std::fill(b.begin()+48,b.begin()+52,0);if(crc(b.data(),b.size())!=check)return false;
 std::memcpy(d.slot.data(),b.data()+head_size,slot_size);std::memcpy(d.common.data(),b.data()+head_size+slot_size,common_size);d.option=le(b.data()+head_size+slot_size+common_size);
 if(image){image->resize(tw*th);std::memcpy(image->data(),b.data()+head_size+slot_size+common_size+4,tw*th*4);}return valid_slot(d);
}
bool read_thumbnail_only(const Record& r,std::vector<uint32_t>& image){
 if(r.legacy>=0)return false;
 std::ifstream f(r.path,std::ios::binary|std::ios::ate);if(!f)return false;
 const size_t expected=head_size+slot_size+common_size+4+tw*th*4;
 if(f.tellg()!=std::streamoff(expected))return false;
 std::array<uint8_t,head_size> h{};f.seekg(0);f.read((char*)h.data(),h.size());
 if(!f||!header(h.data()))return false;
 image.resize(tw*th);
 f.seekg(std::streamoff(head_size+slot_size+common_size+4));
 f.read((char*)image.data(),std::streamsize(tw*th*4));
 return bool(f);
}
void refresh(bool keep_pos=false){
 const int prev_sel=selected,prev_scr=scroll;
 entries.clear();thumbnails.clear();cv_archives_ui::release_caches();std::error_code ec;
 for(fs::directory_iterator it(library(),ec),end;!ec&&it!=end;it.increment(ec)){
  if(!it->is_regular_file(ec)||it->path().extension()!=L".cvsav")continue;
  std::ifstream f(it->path(),std::ios::binary);std::array<uint8_t,head_size> h{};f.read((char*)h.data(),h.size());if(!f||!header(h.data()))continue;
  Record r;r.path=it->path();r.time=uint64_t(le(h.data()+28))|(uint64_t(le(h.data()+32))<<32);r.character=h[52];r.stage=h[53];r.room=h[54];r.variant=h[55];r.seconds=le(h.data()+56);entries.push_back(r);
 }
 // Original slots are listed read-only. Never copy over, delete, or switch DATA0.DAT.
 ec.clear();for(fs::directory_iterator it(root(),ec),end;!ec&&it!=end;it.increment(ec)){
  if(!it->is_directory(ec)||it->path().filename().wstring().find(fs::path(CV_TITLE_ID).wstring())!=0)continue;
  const auto path=it->path()/L"DATA0.DAT";std::ifstream f(path,std::ios::binary|std::ios::ate);if(!f||f.tellg()!=0x7c84)continue;
  WIN32_FILE_ATTRIBUTE_DATA info{};uint64_t written=0;if(GetFileAttributesExW(path.c_str(),GetFileExInfoStandard,&info))written=uint64_t(info.ftLastWriteTime.dwLowDateTime)|(uint64_t(info.ftLastWriteTime.dwHighDateTime)<<32);
  for(int i=0;i<15;i++){Data d;f.seekg(8+i*slot_size);f.read((char*)d.slot.data(),slot_size);if(!f||!valid_slot(d))continue;auto r=metadata(d);r.path=path;r.time=written;r.legacy=i;entries.push_back(r);}
 }
 std::sort(entries.begin(),entries.end(),[](const auto&a,const auto&b){return a.time>b.time;});
 const int max_sel=std::max(0,int(entries.size())-(saving?0:1));
 selected=std::clamp(keep_pos?prev_sel:selected,0,max_sel);
 if(keep_pos){
  scroll=std::clamp(prev_scr,0,std::max(0,max_sel-cv_archives_ui::ROWS_VISIBLE+1));
  if(selected<scroll)scroll=selected;
  if(selected>=scroll+cv_archives_ui::ROWS_VISIBLE)scroll=selected-cv_archives_ui::ROWS_VISIBLE+1;
 }else scroll=0;
}
std::wstring date(uint64_t value){FILETIME ft{DWORD(value),DWORD(value>>32)},local{};SYSTEMTIME t{};FileTimeToLocalFileTime(&ft,&local);FileTimeToSystemTime(&local,&t);wchar_t out[64];swprintf(out,64,L"%04u-%02u-%02u  %02u:%02u:%02u",t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond);return out;}
void thumbnail(void* user,const uint32_t* rgba,unsigned w,unsigned h){
 if(!rgba||!w||!h)return;std::lock_guard<std::mutex> lock(photo_lock);if(uintptr_t(user)!=sequence.load())return;
 photo.assign(tw*th,0xff111111);const double scale=std::min(double(tw)/w,double(th)/h);unsigned dw=std::max(1u,unsigned(w*scale)),dh=std::max(1u,unsigned(h*scale));
 for(unsigned y=0;y<dh;y++)for(unsigned x=0;x<dw;x++){uint32_t c=rgba[size_t(std::min(h-1,unsigned(y/scale)))*w+std::min(w-1,unsigned(x/scale))];photo[size_t(y+(th-dh)/2)*tw+x+(tw-dw)/2]=0xff000000u|((c&255)<<16)|(c&0xff00)|((c>>16)&255);}
 photo_id=uintptr_t(user);photo_ready.notify_all();
}
void publish(const wchar_t* msg,bool ok){perf_overlay_toast(msg,ok);if(HWND h=notification.load())PostMessageW(h,WM_DONE,ok,0);}
bool write_save(const Data& d,const fs::path& replace,uintptr_t id){
 std::vector<uint32_t> pixels(tw*th,0xff191616);
 {std::unique_lock<std::mutex> lock(photo_lock);photo_ready.wait_for(lock,std::chrono::seconds(3),[&]{return photo_id==id;});if(photo_id==id)pixels=photo;}
 auto r=metadata(d);std::vector<uint8_t> b(head_size+slot_size+common_size+4+pixels.size()*4);
 std::memcpy(b.data(),"CVSAVE1",8);put(b,8,1);put(b,12,head_size);std::memcpy(b.data()+16,CV_TITLE_ID,std::strlen(CV_TITLE_ID));put(b,28,uint32_t(r.time));put(b,32,uint32_t(r.time>>32));
 put(b,40,slot_size+common_size+4);put(b,44,tw*th*4);b[52]=uint8_t(r.character);b[53]=uint8_t(r.stage);b[54]=uint8_t(r.room);b[55]=uint8_t(r.variant);put(b,56,r.seconds);
 std::memcpy(b.data()+head_size,d.slot.data(),slot_size);std::memcpy(b.data()+head_size+slot_size,d.common.data(),common_size);put(b,head_size+slot_size+common_size,d.option);
 std::memcpy(b.data()+head_size+slot_size+common_size+4,pixels.data(),pixels.size()*4);put(b,48,crc(b.data(),b.size()));
 std::error_code ec;fs::create_directories(library(),ec);if(ec)return false;
 wchar_t name[96];swprintf(name,96,L"%020llu-%u-%llu.cvsav",(unsigned long long)r.time,GetCurrentProcessId(),(unsigned long long)id);
 const auto target=replace.empty()?library()/name:replace;const auto temp=library()/(std::wstring(name)+L".pending");
 HANDLE file=CreateFileW(temp.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);if(file==INVALID_HANDLE_VALUE)return false;
 DWORD done=0;bool ok=WriteFile(file,b.data(),DWORD(b.size()),&done,nullptr)&&done==b.size()&&FlushFileBuffers(file);CloseHandle(file);
 if(ok)ok=MoveFileExW(temp.c_str(),target.c_str(),MOVEFILE_WRITE_THROUGH|(replace.empty()?0:MOVEFILE_REPLACE_EXISTING))!=FALSE;
 if(!ok)DeleteFileW(temp.c_str());return ok;
}
struct Call {ppu_context c;ppu_context* previous;void(*tail)(void*);Call(ppu_context* p):c(*p),previous(g_active_ctx),tail(g_trampoline_fn){c.gpr[1]-=0x2000;vm_write64(c.gpr[1],p->gpr[1]);c.gpr[2]=CV_TOC;g_active_ctx=&c;g_trampoline_fn=nullptr;}
 ~Call(){g_active_ctx=previous;g_trampoline_fn=tail;}void run(void(*fn)(ppu_context*),uint32_t a=0,uint32_t b=0){c.gpr[3]=a;c.gpr[4]=b;fn(&c);while(g_trampoline_fn){auto next=g_trampoline_fn;g_trampoline_fn=nullptr;next(&c);}}};
bool restore(ppu_context* ctx,const Data& d){
 Call call(ctx);
 // System options belong to the active profile. The native load helper that
 // updates DATA0's achievement flag requires a live mutex/header; no fake
 // savedata manager or scratch lock is constructed for a library entry.
 // Same native slot copy as the load screen; no UI object or virtual slot
 // is constructed. Then enter its confirmed restart path, without UI cleanup.
 std::memcpy(vm_base+state+0x8c,d.slot.data(),slot_size);
 if(uint32_t sound=vm_read32(SOUND_PTR))call.run(STOP_BANK,sound,9);
 call.run(LOAD_BEGIN);
 // Keep the native confirmed-load flag (0x200). Room startup uses it
 // to restore saved position/health rather than initialize a new game.
 vm_write32(state+0x50,(vm_read32(state+0x50)&~0x00030040u)|0x200u);
 vm_write32(state+0x70,vm_read32(state+0x70)&~0x08000000u);vm_write32(state+0x74,vm_read32(state+0x74)&~0x00200000u);
 vm_write32(state+0x68,0x80);vm_write32(state+0x64,0x00300050);vm_write32(state+0x58,0);vm_write16(state+0x5c,0);vm_write16(state+0x5e,0);
 vm_write32(state+0x1b18c,vm_read32(state+0x1b1d4));
 std::fprintf(stderr,"[SAVE-LIBRARY] native load requested room=%u/%u/%u character=%u\n",d.slot[9],d.slot[10],d.slot[11],d.slot[8]);return true;
}
bool capture(Data& out){
 // Mirrors the PS3's native snapshot (EU 00190BA8 / JP 0018F1A8 /
 // US 0018F210), but writes only our local copy. The snapshot refreshes
 // live actor values that the persistent state does not update every tick.
 const uint32_t actor=vm_read32(PLAYER_PTR);const unsigned character=vm_read8(state+0x94);
 if(character>=4||!mapped(actor,0x420))return false;
 std::memcpy(out.slot.data(),vm_base+state+0x8c,slot_size);
 auto put16=[&](size_t at,unsigned v){out.slot[at]=uint8_t(v>>8);out.slot[at+1]=uint8_t(v);};
 putbe(out.slot.data(),10);out.slot[0x9a-0x8c]=vm_read8(actor+8);
 putbe(out.slot.data()+0x880-0x8c+character*4,vm_read32(actor+0x88)&0x78280000u);
 for(unsigned axis=0;axis<3;axis++)putbe(out.slot.data()+0x890-0x8c+axis*4,vm_read32(actor+0x10+axis*4));
 putbe(out.slot.data()+0x89c-0x8c,vm_read32(actor+0x20));
 out.slot[0x8a0-0x8c+character]=uint8_t(vm_read32(actor+0x3e4));
 put16(0x8a4-0x8c+character*2,vm_read32(actor+0x41c));
 std::memcpy(out.common.data(),vm_base+state+0x34,common_size);
 return valid_slot(out);
}
// Unified host UI. Native save capture/restore remains above; the window's drawing is cv_archives_ui.cpp.
namespace ui=cv_archives_i18n;
namespace au=cv_archives_ui;
namespace tf=cv_title_flow;
const wchar_t* tr(ui::Id id){return ui::strings[language.load()][id];}
const wchar_t* mt(cv_test_i18n::Id id){return cv_test_i18n::k_text[language.load()][id];}
constexpr int rows_visible=au::ROWS_VISIBLE;
int page=au::PAGE_LOAD,spawn=0,input_kind=PAD_INPUT_KEYBOARD;
int painted_hint_kind=PAD_INPUT_KEYBOARD;POINT last_mouse{};bool stick_active[PAD_PHYS_MAX]={};
int page_selected[3]={},page_scroll[3]={};
bool confirming=false,confirm_yes=false,confirm_delete=false,double_click=false,search_focus=false;
bool view_dirty=true;
au::Tone status_tone=au::Tone::NEUTRAL;
std::atomic<int> context{tf::CTX_NONE};   // what the game thread says F7 may do: nothing, gameplay, the title menu
HWND search=nullptr;
HFONT edit_font=nullptr;HBRUSH edit_brush=nullptr;
float scale=1;
au::View view;                               // the drawn state, and the mouse's hover / pressed
std::vector<cv_test::Room> maps;std::vector<size_t> filtered_maps;
unsigned current_room=~0u;
bool (*travel_to)(unsigned,unsigned)=nullptr;
ULONGLONG held_since[PAD_PHYS_MAX]={};
uint32_t held_direction[PAD_PHYS_MAX]={};
void repaint(bool dirty=true){if(dirty)view_dirty=true;if(window)InvalidateRect(window,nullptr,FALSE);}
void set_status(const std::wstring& s,au::Tone t=au::Tone::NEUTRAL){status=s;status_tone=t;view_dirty=true;}
void close_window(){if(busy)return;confirming=false;confirm_delete=false;cv_port_modal::close(cv_port_modal::ARCHIVES);}   // hides, the game runs again and gets the focus
int count(){return page==au::PAGE_MAPS?int(filtered_maps.size()):int(entries.size())+(saving?1:0);}
void move(int delta){if(busy||confirming||!count())return;selected=std::clamp(selected+delta,0,count()-1);if(selected<scroll)scroll=selected;if(selected>=scroll+rows_visible)scroll=selected-rows_visible+1;repaint();}
void filter_maps(){wchar_t value[128]={};if(search)GetWindowTextW(search,value,128);std::wstring query=value;for(auto& c:query)c=wchar_t(towlower(c));filtered_maps.clear();for(size_t i=0;i<maps.size();i++)if(query.empty()||maps[i].code().find(query)!=std::wstring::npos)filtered_maps.push_back(i);selected=scroll=0;repaint();}
// The search field's font: the embedded Barlow where it exists (registered for GDI first), else the language's own face.
void make_edit_font(){
 if(!search)return;cv_port_ui::register_gdi_fonts();
 const int lang=language.load();
 const wchar_t* face=cv_port_ui::native_fonts(lang)?L"Barlow Semi Condensed":cv_srt_i18n::font_face(cv_srt_i18n::Language(lang));
 LOGFONTW lf{};lf.lfHeight=-std::max(10,int(18*scale+0.5f));lf.lfWeight=FW_NORMAL;lf.lfCharSet=DEFAULT_CHARSET;lf.lfQuality=ANTIALIASED_QUALITY;
 wcsncpy_s(lf.lfFaceName,face,_TRUNCATE);
 HFONT next=CreateFontIndirectW(&lf);SendMessageW(search,WM_SETFONT,reinterpret_cast<WPARAM>(next),TRUE);if(edit_font)DeleteObject(edit_font);edit_font=next;
}
void search_layout(){if(!search||!window)return;const auto r=au::layout::FILTER_EDIT;MoveWindow(search,int(r.X*scale),int(r.Y*scale),int(r.Width*scale),int(std::ceil(r.Height*scale)),TRUE);ShowWindow(search,page==au::PAGE_MAPS?SW_SHOW:SW_HIDE);}
void switch_tab(int next){if(busy||confirming)return;page_selected[page]=selected;page_scroll[page]=scroll;page=(next+3)%3;saving=page==au::PAGE_SAVE;selected=page_selected[page];scroll=page_scroll[page];status.clear();view.mouse.leave();search_layout();SetFocus(window);move(0);repaint();}
const Record* selection(){int i=selected-(saving?1:0);return page!=au::PAGE_MAPS&&i>=0&&i<int(entries.size())?&entries[i]:nullptr;}
std::wstring room_label(unsigned stage,unsigned room,unsigned variant){
 // Descriptive English room names are optional metadata. Other languages
 // use the localized area/room label rather than leaking English UI strings.
 if(language==1)return cv_save_room_name(stage,room,variant);
 wchar_t out[128];swprintf(out,128,mt(cv_test_i18n::WHERE),stage,room);wchar_t code[32];swprintf(code,32,L"  ·  rm_%u%02u%u",stage,room,variant);return std::wstring(out)+code;
}
void activate(bool overwrite=false){
 if(busy)return;const int ctx=context.load();
 if(!tf::load_allowed(ctx)){set_status(tr(ui::UNAVAILABLE),au::Tone::WARN);repaint();return;}
 if(page==au::PAGE_MAPS){if(selected>=0&&selected<int(filtered_maps.size())&&travel_to&&travel_to(maps[filtered_maps[selected]].key(),unsigned(spawn)))close_window();else{set_status(mt(cv_test_i18n::REFUSED),au::Tone::BAD);repaint();}return;}
 if(saving&&!tf::save_allowed(ctx)){set_status(tr(ui::SAVE_TITLE_BLOCKED),au::Tone::WARN);repaint();return;}   // no save at the title
 Pending p;p.save=saving;const int index=selected-(saving?1:0);
 if(saving){if(index>=0&&!overwrite){confirming=true;confirm_delete=false;confirm_yes=false;repaint();return;}
  if(index>=0){if(index>=int(entries.size())||entries[index].legacy>=0)return;p.replace=entries[index].path;}}
 else if(index<0||index>=int(entries.size())||!read(entries[index],p.data)){set_status(tr(ui::CORRUPT),au::Tone::BAD);repaint();return;}
 {std::lock_guard<std::mutex> lock(queue_lock);if(have_request)return;queued=std::move(p);have_request=true;busy=true;}
 set_status(tr(ui::WORKING));if(!saving)cv_port_modal::close(cv_port_modal::ARCHIVES);else cv_port_modal::set_working(cv_port_modal::ARCHIVES,true);   // a save is written with the game running, the window still shown
 repaint();
}
void request_overwrite(){const auto* e=selection();if(busy||!tf::save_allowed(context.load())||!saving||!e||e->legacy>=0)return;confirming=true;confirm_delete=false;confirm_yes=false;repaint();}
void request_delete(){const auto* e=selection();if(busy||page==au::PAGE_MAPS||!e||e->legacy>=0)return;confirming=true;confirm_delete=true;confirm_yes=false;repaint();}
void delete_selected(){
 const auto* e=selection();
 if(busy||page==au::PAGE_MAPS||!e||e->legacy>=0)return;
 const fs::path target=e->path;
 std::error_code ec;
 const bool ok=fs::remove(target,ec)&&!ec;
 if(ok){refresh(true);set_status(tr(ui::DELETED),au::Tone::GOOD);}
 else{set_status(tr(ui::DELETE_FAILED),au::Tone::BAD);}
 repaint();
}
void accept(){
 if(confirming){
  const bool yes=confirm_yes,del=confirm_delete;
  confirming=false;confirm_delete=false;
  if(yes){if(del)delete_selected();else activate(true);}
  repaint();
 }else{if(saving)selected=scroll=0;activate();}
}
void back(){if(confirming){confirming=false;confirm_delete=false;repaint();}else close_window();}
const wchar_t* chars[]={L"Claire Redfield",L"Chris Redfield",L"Steve Burnside",L"Albert Wesker"};
std::wstring clock_text(unsigned seconds){wchar_t tm[32];swprintf(tm,32,L"%02u:%02u:%02u",seconds/3600,seconds/60%60,seconds%60);return tm;}
// The thumbnail of a library file (320x180), read once and kept; null for an original slot or an unreadable file.
const uint32_t* thumbnail_of(const Record* e){
 if(!e||e->legacy>=0)return nullptr;const auto key=e->path.wstring();auto at=thumbnails.find(key);
 if(at==thumbnails.end()){std::vector<uint32_t> px;if(!read_thumbnail_only(*e,px)||px.size()!=size_t(tw)*th)return nullptr;at=thumbnails.emplace(key,std::move(px)).first;}
 return at->second.data();
}
// The input helpers live in cv_port_ui (shared with the other port windows).
int hint_kind(){return cv_port_ui::hint_kind(input_kind);}
void note_input(int kind){kind=cv_port_ui::normalize_kind(kind);if(input_kind!=kind){input_kind=kind;repaint();}}
using cv_port_ui::menu_pad;
// Fills `view` for a paint or a hit test: only what is visible, the pictures held in `thumbnails` until the next sync.
void sync_view(){
 if(!view_dirty)return;
 view_dirty=false;
 au::View& v=view;const int ctx=context.load();const int total=count();
 if(thumbnails.size()>64){thumbnails.clear();au::release_caches();}   // before this batch: the pointers below stay valid until the next sync
 v.language=language.load();v.page=page;v.input=input_kind;v.unavailable=!tf::load_allowed(ctx);
 v.count=total;v.scroll=scroll;v.selected=selected;v.spawn=spawn;v.files=int(entries.size());
 v.confirming=confirming;v.confirm_yes=confirm_yes;v.confirm_delete=confirm_delete;v.busy=busy;v.search_focus=search_focus;
 v.rows.clear();
 for(int item=scroll;item<total&&item<scroll+rows_visible;item++){
  au::Row row;
  if(page==au::PAGE_MAPS){
   const auto& m=maps[filtered_maps[size_t(item)]];row.kind=au::Row::MAP;row.title=m.code();row.where=room_label(m.stage,m.number,m.variant);
   if(m.variant){wchar_t e[100];swprintf(e,100,mt(cv_test_i18n::EVENT),m.variant);row.event=e;}
  }else if(saving&&item==0){row.kind=au::Row::NEW_SAVE;row.title=tr(ui::NEW_SAVE);row.where=tr(ui::NEW_HINT);}
  else{
   const auto& e=entries[size_t(item-(saving?1:0))];row.kind=au::Row::SAVE;row.title=chars[std::min(e.character,3u)];row.where=room_label(e.stage,e.room,e.variant);
   row.when=date(e.time);row.thumb=thumbnail_of(&e);row.original=e.legacy>=0;
  }
  v.rows.push_back(std::move(row));
 }
 // The right column.
 v.heading.clear();v.note.clear();v.tag.clear();v.image=nullptr;v.placeholder.clear();v.character.clear();v.play_time.clear();
 v.now_room.clear();if(current_room!=~0u)v.now_room=cv_test::Room{int(current_room>>16),int((current_room>>8)&255),int(current_room&255)}.code();
 if(page==au::PAGE_MAPS){
  if(selected>=0&&selected<int(filtered_maps.size())){
   const auto& m=maps[filtered_maps[size_t(selected)]];v.heading=room_label(m.stage,m.number,m.variant);v.note=tr(ui::MAP_NOTE);v.tag=m.code();
   for(const auto& record:entries)if(record.legacy<0&&record.stage==m.stage&&record.room==m.number&&record.variant==m.variant){v.image=thumbnail_of(&record);break;}   // a picture of that room, if a save has one
  }
  if(!v.image)v.placeholder=tr(ui::NO_PREVIEW);
 }else if(const Record* e=selection()){
  v.heading=room_label(e->stage,e->room,e->variant);v.character=chars[std::min(e->character,3u)];v.play_time=clock_text(e->seconds);
  v.image=thumbnail_of(e);if(!v.image)v.placeholder=e->legacy>=0?tr(ui::ORIGINAL):tr(ui::NO_PREVIEW);
 }else if(saving&&selected==0){v.heading=tr(ui::NEW_SAVE);v.note=tr(ui::NEW_HINT);v.placeholder=std::wstring(L"+  ")+tr(ui::NEW_SAVE);}
 else v.placeholder=tr(ui::NO_PREVIEW);
 v.primary_enabled=tf::load_allowed(ctx)&&!busy&&(page==au::PAGE_SAVE?tf::save_allowed(ctx):total>0);
 v.can_overwrite=saving&&tf::save_allowed(ctx)&&selection()&&selection()->legacy<0;
 v.can_delete=!busy&&page!=au::PAGE_MAPS&&selection()&&selection()->legacy<0;
 // Under the main button: a result or refusal first, else why the page may not work, else the page's own note.
 if(!status.empty()){v.status=status;v.tone=status_tone;}
 else if(!tf::load_allowed(ctx)){v.status=tr(ui::UNAVAILABLE);v.tone=au::Tone::WARN;}
 else if(page==au::PAGE_SAVE&&!tf::save_allowed(ctx)){v.status=tr(ui::SAVE_TITLE_BLOCKED);v.tone=au::Tone::WARN;}
 else if(page==au::PAGE_MAPS){v.status=ctx==tf::CTX_TITLE?tr(ui::MAP_TITLE_NOTE):tr(ui::UNAVAILABLE);v.tone=au::Tone::WARN;}
 else{v.status.clear();v.tone=au::Tone::NEUTRAL;}
}
struct ArchivesBuffers{
 HDC memdc=nullptr;HBITMAP dib=nullptr;HGDIOBJ old_bmp=nullptr;uint8_t* bits=nullptr;
 std::vector<uint8_t> backdrop;int w=0,h=0,lang=-1;float sc=0;
 void free_all(){
  if(memdc&&old_bmp){SelectObject(memdc,old_bmp);old_bmp=nullptr;}
  if(dib){DeleteObject(dib);dib=nullptr;}
  if(memdc){DeleteDC(memdc);memdc=nullptr;}
  bits=nullptr;backdrop.clear();backdrop.shrink_to_fit();w=h=0;lang=-1;sc=0;
 }
 bool ensure(HDC ref_dc,int nw,int nh,float nsc,const au::View& v){
  if(dib&&w==nw&&h==nh&&sc==nsc&&lang==v.language)return true;
  free_all();
  BITMAPINFO bmi{};bmi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bmi.bmiHeader.biWidth=nw;bmi.bmiHeader.biHeight=-nh;
  bmi.bmiHeader.biPlanes=1;bmi.bmiHeader.biBitCount=32;bmi.bmiHeader.biCompression=BI_RGB;
  void* raw=nullptr;memdc=CreateCompatibleDC(ref_dc);if(!memdc)return false;
  dib=CreateDIBSection(memdc,&bmi,DIB_RGB_COLORS,&raw,nullptr,0);if(!dib||!raw){free_all();return false;}
  old_bmp=SelectObject(memdc,dib);bits=static_cast<uint8_t*>(raw);w=nw;h=nh;sc=nsc;lang=v.language;
  backdrop.assign(size_t(nw)*size_t(nh)*4,0);
  {Gdiplus::Bitmap bg_bmp(nw,nh,nw*4,PixelFormat32bppPARGB,backdrop.data());
   Gdiplus::Graphics bg(&bg_bmp);bg.ScaleTransform(nsc,nsc);au::draw_backdrop(bg,v);}
  return true;
 }
} abuf;
void paint(HWND h){painted_hint_kind=hint_kind();PAINTSTRUCT ps;HDC dc=BeginPaint(h,&ps);RECT c;GetClientRect(h,&c);
 const int w=std::max<int>(1,c.right),hh=std::max<int>(1,c.bottom);sync_view();
 if(abuf.ensure(dc,w,hh,scale,view)){
  std::memcpy(abuf.bits,abuf.backdrop.data(),abuf.backdrop.size());
  {Gdiplus::Bitmap back(w,hh,w*4,PixelFormat32bppPARGB,abuf.bits);
   Gdiplus::Graphics g(&back);g.ScaleTransform(scale,scale);au::draw_foreground(g,view);}
  BitBlt(dc,0,0,w,hh,abuf.memdc,0,0,SRCCOPY);
 }else{
  Gdiplus::Bitmap back(w,hh,PixelFormat32bppPARGB);
  {Gdiplus::Graphics g(&back);g.ScaleTransform(scale,scale);au::draw(g,view);}
  Gdiplus::Graphics out(dc);out.DrawImage(&back,Gdiplus::Rect(0,0,w,hh),0,0,w,hh,Gdiplus::UnitPixel);
 }
 EndPaint(h,&ps);
}
int id_at(int px,int py){sync_view();const auto list=au::hits(view);const au::Hit* hit=au::hit_at(list,px/scale,py/scale);return hit?au::hit_id(hit->kind,hit->value):-1;}
void reveal(int index){selected=std::clamp(index,0,std::max(0,count()-1));if(selected<scroll)scroll=selected;if(selected>=scroll+rows_visible)scroll=selected-rows_visible+1;view_dirty=true;}
// The action of a control, on the button's release over it.
void fire(int id,bool dbl){
 const int kind=id/65536,value=id%65536;
 switch(kind){
 case au::HIT_TAB:switch_tab(value);break;
 case au::HIT_TAB_PREV:switch_tab(page-1);break;
 case au::HIT_TAB_NEXT:switch_tab(page+1);break;
 case au::HIT_ROW:if(value<count()){reveal(value);repaint();if(dbl){if(saving&&selection())request_overwrite();else accept();}}break;
 case au::HIT_STEP_MINUS:case au::HIT_STEP_PLUS:spawn=std::clamp(spawn+(kind==au::HIT_STEP_MINUS?-1:1),0,255);repaint();break;
 case au::HIT_PRIMARY:if(saving)selected=scroll=0;activate();break;
 case au::HIT_SECONDARY:request_overwrite();break;
 case au::HIT_DELETE:request_delete();break;
 case au::HIT_BACK:back();break;
 case au::HIT_DIALOG_CANCEL:confirm_yes=false;accept();break;
 case au::HIT_DIALOG_YES:confirm_yes=true;accept();break;
 }
}
void keyboard(WPARAM key){note_input(PAD_INPUT_KEYBOARD);if(busy)return;
 if(key==VK_ESCAPE||key==VK_F7){back();return;}if(confirming){if(key==VK_LEFT||key==VK_RIGHT||key==VK_TAB){confirm_yes=!confirm_yes;repaint();}if(key==VK_RETURN)accept();return;}
 if(key==VK_TAB||key=='Q'||key=='E'){switch_tab(page+((key=='Q'||(key==VK_TAB&&(GetKeyState(VK_SHIFT)&0x8000)))?-1:1));return;}
 if(key==VK_UP)move(-1);if(key==VK_DOWN)move(1);if(key==VK_PRIOR)move(-rows_visible);if(key==VK_NEXT)move(rows_visible);if(key==VK_HOME)move(-count());if(key==VK_END)move(count());
 if(page==au::PAGE_MAPS&&(key==VK_LEFT||key==VK_RIGHT)){spawn=std::clamp(spawn+(key==VK_LEFT?-1:1),0,255);repaint();}
 if(key==VK_RETURN)accept();if(saving&&key=='O')request_overwrite();if(saving&&key=='N'){selected=scroll=0;activate();}
 if(key==VK_DELETE&&page!=au::PAGE_MAPS)request_delete();
 if(page==au::PAGE_MAPS&&key=='F'){SetFocus(search);SendMessageW(search,EM_SETSEL,0,-1);}
}
LRESULT CALLBACK proc(HWND h,UINT msg,WPARAM wp,LPARAM lp){switch(msg){
 case WM_PAINT:paint(h);return 0;case WM_ERASEBKGND:return 1;
 case WM_CLOSE:back();return 0;
 case WM_SIZE:search_layout();return 0;
 case WM_KEYDOWN:if(wp==VK_F7&&cv_port_modal::key_repeat(lp))return 0;keyboard(wp);return 0;
 case WM_COMMAND:{const auto from=reinterpret_cast<HWND>(lp);
  if(from==search){if(HIWORD(wp)==EN_CHANGE)filter_maps();else if(HIWORD(wp)==EN_SETFOCUS||HIWORD(wp)==EN_KILLFOCUS){search_focus=HIWORD(wp)==EN_SETFOCUS;repaint();}}
  return 0;}
 case WM_CTLCOLOREDIT:if(!edit_brush)edit_brush=CreateSolidBrush(RGB(7,11,22));SetTextColor(reinterpret_cast<HDC>(wp),RGB(230,236,248));SetBkColor(reinterpret_cast<HDC>(wp),RGB(7,11,22));return reinterpret_cast<LRESULT>(edit_brush);
 case WM_MOUSEWHEEL:note_input(PAD_INPUT_KEYBOARD);move(GET_WHEEL_DELTA_WPARAM(wp)>0?-3:3);return 0;
 case WM_MOUSEMOVE:view.mouse.arm(h);if(view.mouse.move(id_at(GET_X_LPARAM(lp),GET_Y_LPARAM(lp))))repaint(false);return 0;
 case WM_MOUSELEAVE:if(view.mouse.leave())repaint(false);return 0;
 case WM_LBUTTONDOWN:case WM_LBUTTONDBLCLK:{note_input(PAD_INPUT_KEYBOARD);if(busy)return 0;
  if(!confirming)SetFocus(h);
  const int id=id_at(GET_X_LPARAM(lp),GET_Y_LPARAM(lp));double_click=msg==WM_LBUTTONDBLCLK;
  if(id>=0){SetCapture(h);if(view.mouse.down(id))repaint(false);}return 0;}
 case WM_LBUTTONUP:if(view.mouse.pressed>=0){ReleaseCapture();int fired=-1;const bool changed=view.mouse.up(id_at(GET_X_LPARAM(lp),GET_Y_LPARAM(lp)),fired);
   const bool dbl=double_click;double_click=false;
   if(fired>=0){fire(fired,dbl);repaint();}else if(changed)repaint(false);}
  return 0;
 case WM_DONE:busy=false;if(IsWindowVisible(h)){cv_port_modal::set_working(cv_port_modal::ARCHIVES,false);refresh();set_status(wp?tr(ui::SAVED):tr(ui::FAILED),wp?au::Tone::GOOD:au::Tone::BAD);repaint();}return 0;
 case WM_TIMER:cv_port_modal::tick(cv_port_modal::ARCHIVES,h);if(painted_hint_kind!=hint_kind())repaint();if(IsWindowVisible(h)&&(GetForegroundWindow()==h||IsChild(h,GetForegroundWindow())))for(int i=0;i<PAD_PHYS_MAX;i++){
  PadPhys p{};if(!pad_phys_poll(i,&p)){previous_pad[i]=held_direction[i]=0;stick_active[i]=false;continue;}
  menu_pad(p);
  const bool motion=p.rx<64||p.rx>192||p.ry<64||p.ry>192;
  const bool new_motion=motion&&!stick_active[i];stick_active[i]=motion;
  uint32_t edge=p.buttons&~previous_pad[i];previous_pad[i]=p.buttons;
  if(cv_launcher_controls::chord_held(p.buttons))continue;if(edge||new_motion){note_input(p.kind);SetFocus(h);}if(busy)continue;
  if(edge&(1u<<PB_EAST)){back();return 0;}
  if(confirming){if(edge&((1u<<PB_LEFT)|(1u<<PB_RIGHT))){confirm_yes=!confirm_yes;repaint();}if(edge&(1u<<PB_SOUTH))accept();continue;}
  if(edge&(1u<<PB_L1))switch_tab(page-1);if(edge&(1u<<PB_R1))switch_tab(page+1);
  if(edge&(1u<<PB_SOUTH))accept();
  if(saving&&(edge&(1u<<PB_WEST))){if(tf::save_allowed(context.load()))request_overwrite();else request_delete();}
  if(saving&&(edge&(1u<<PB_SELECT)))request_delete();
  if(!saving&&page==au::PAGE_LOAD&&(edge&(1u<<PB_WEST)))request_delete();
  if(saving&&(edge&(1u<<PB_NORTH))){selected=scroll=0;activate();}
  if(page==au::PAGE_MAPS&&(edge&((1u<<PB_LEFT)|(1u<<PB_RIGHT)))){spawn=std::clamp(spawn+((edge&(1u<<PB_LEFT))?-1:1),0,255);repaint();}
  uint32_t dir=p.buttons&((1u<<PB_UP)|(1u<<PB_DOWN)|(1u<<PB_L2)|(1u<<PB_R2));auto now=GetTickCount64();if(!dir){held_direction[i]=0;continue;}if(dir!=held_direction[i]){held_direction[i]=dir;held_since[i]=now;repeat_at[i]=0;}
  if(now>=repeat_at[i]){note_input(p.kind);int delta=(dir&((1u<<PB_UP)|(1u<<PB_L2)))?-1:1;if(dir&((1u<<PB_L2)|(1u<<PB_R2)))delta*=rows_visible;move(delta);repeat_at[i]=now+((now==held_since[i])?340:(now-held_since[i]>1100?55:115));}
 }return 0;
 case WM_DESTROY:KillTimer(h,1);abuf.free_all();au::release_caches();notification=nullptr;window=nullptr;search=nullptr;return 0;}
 return DefWindowProcW(h,msg,wp,lp);
}

}
void cv_save_library_frame(ppu_context* ctx,bool gameplay,bool title){
 const int now=tf::context_of(gameplay,title&&!gameplay);context=now;available=now!=tf::CTX_NONE;
 Pending p;{std::lock_guard<std::mutex> lock(queue_lock);if(!have_request)return;p=std::move(queued);have_request=false;}
 if(now==tf::CTX_NONE){busy=false;publish(tr(ui::UNAVAILABLE),false);return;}
 if(!mapped(state+0x8c,slot_size)||!mapped(state+0x34,common_size)){busy=false;publish(tr(ui::UNAVAILABLE),false);return;}
 // A load works in game and at the title menu (the same native slot copy and restart tasks); a save only in game.
 if(!p.save){bool ok=valid_slot(p.data)&&restore(ctx,p.data);if(ok&&now==tf::CTX_TITLE){cv_title_shortcut_started("library save");std::fprintf(stderr,"[TITLE-LOAD] library entry loaded from the title menu\n");}busy=false;publish(ok?tr(ui::LOADING):tr(ui::CORRUPT),ok);return;}
 if(now!=tf::CTX_GAMEPLAY){busy=false;publish(tr(ui::SAVE_TITLE_BLOCKED),false);return;}
 if(!capture(p.data)){busy=false;publish(tr(ui::UNAVAILABLE),false);return;}
 const uint32_t original=vm_read32(SAVE_DATA_PTR);if(mapped(original,0x7cc0))p.data.option=vm_read32(original+0x7bac);
 if(!valid_slot(p.data)){busy=false;publish(tr(ui::UNAVAILABLE),false);return;}
 const uintptr_t id=++sequence;{std::lock_guard<std::mutex> lock(photo_lock);photo_id=0;photo.clear();}
 rsx_d3d12_request_thumbnail(thumbnail,reinterpret_cast<void*>(id));
 try{std::thread([p=std::move(p),id](){bool ok=false;try{ok=write_save(p.data,p.replace,id);}catch(...){ok=false;}
  // The menu consumes completion before accepting another write, so an old
  // completion cannot pause a newer request between its snapshot and picture.
  publish(ok?tr(ui::SAVED):tr(ui::FAILED),ok);
 }).detach();}
 catch(...){busy=false;publish(tr(ui::FAILED),false);}
}
void cv_save_library_show(bool mode,int lang){cv_archives_show(mode?au::PAGE_SAVE:au::PAGE_LOAD,lang);}
void cv_archives_show(int page_,int lang,int opening_input){if(busy)return;language=std::clamp(lang,0,12);page=std::clamp(page_,0,2);saving=page==au::PAGE_SAVE;selected=scroll=0;confirming=false;confirm_delete=false;view_dirty=true;status.clear();refresh();
 cv_port_ui::fonts_available();   // the embedded fonts load before any Graphics of this window exists
 if(!window){WNDCLASSW wc{};wc.style=CS_DBLCLKS;wc.lpfnWndProc=proc;wc.hInstance=GetModuleHandleW(nullptr);wc.hCursor=LoadCursorW(nullptr,MAKEINTRESOURCEW(32512));wc.lpszClassName=L"CVSaveLibrary";wc.hIcon=LoadIconW(wc.hInstance,MAKEINTRESOURCEW(1));RegisterClassW(&wc);
  // Owned by the game window, never TOPMOST: above the game, below other programs.
  window=CreateWindowExW(WS_EX_TOOLWINDOW,wc.lpszClassName,L"Code: Veronica",WS_POPUP|WS_CLIPCHILDREN,0,0,au::W,au::H,(HWND)rsx_d3d12_window(),nullptr,wc.hInstance,nullptr);
  if(!window)return;
  search=CreateWindowExW(0,L"EDIT",L"",WS_CHILD|ES_AUTOHSCROLL,0,0,100,25,window,reinterpret_cast<HMENU>(INT_PTR(400)),wc.hInstance,nullptr);
 }
 HWND game=(HWND)rsx_d3d12_window();
 scale=cv_port_ui::scale_for(game,au::W,au::H);
 const int width=int(std::lround(au::W*scale)),height=int(std::lround(au::H*scale));const RECT where=cv_port_ui::center_on_game(game,width,height);
 SetWindowPos(window,nullptr,where.left,where.top,width,height,SWP_NOACTIVATE|SWP_NOZORDER);
 make_edit_font();
 SetWindowTextW(search,L"");SendMessageW(search,EM_SETCUEBANNER,0,reinterpret_cast<LPARAM>(tr(ui::SEARCH)));
 std::fill(std::begin(page_selected),std::end(page_selected),0);std::fill(std::begin(page_scroll),std::end(page_scroll),0);
 view.mouse.leave();search_focus=false;
 filter_maps();if(page==au::PAGE_MAPS)for(size_t i=0;i<filtered_maps.size();i++)if(maps[filtered_maps[i]].key()==current_room){selected=int(i);scroll=std::max(0,selected-rows_visible+1);break;}
 search_layout();notification=window;cv_port_modal::register_close(cv_port_modal::ARCHIVES,cv_save_library_close);
 if(!cv_port_modal::open(cv_port_modal::ARCHIVES,window))return;   // owner = the game, game paused and pad blocked; refused when the game is not in front
 SetFocus(window);SetTimer(window,1,16,nullptr);note_input(opening_input==PAD_KIND_NONE?pad_last_input():opening_input);GetCursorPos(&last_mouse);
 for(int i=0;i<PAD_PHYS_MAX;i++){PadPhys p{};pad_phys_poll(i,&p);menu_pad(p);previous_pad[i]=p.buttons;held_direction[i]=0;stick_active[i]=p.rx<64||p.rx>192||p.ry<64||p.ry>192;}InvalidateRect(window,nullptr,FALSE);
}
void cv_save_library_close(){if(!busy)close_window();}
void cv_archives_release(){   // before GdiplusShutdown / at exit
 if(window)DestroyWindow(window);
 abuf.free_all();au::release_caches();
 if(edit_font){DeleteObject(edit_font);edit_font=nullptr;}if(edit_brush){DeleteObject(edit_brush);edit_brush=nullptr;}
 cv_port_ui::release_fonts();
}
bool cv_save_library_message(MSG* msg){if(!window||!IsWindowVisible(window)||(msg->hwnd!=window&&!IsChild(window,msg->hwnd)))return false;
 // Input in child edits must update hints too. Stationary/synthetic mouse
 // messages do not steal the device from the controller.
 if(msg->message==WM_KEYDOWN||msg->message==WM_SYSKEYDOWN||msg->message==WM_CHAR||msg->message==WM_LBUTTONDOWN||msg->message==WM_RBUTTONDOWN||msg->message==WM_MOUSEWHEEL)note_input(PAD_INPUT_KEYBOARD);
 if(msg->message==WM_MOUSEMOVE){POINT point{};GetCursorPos(&point);if(point.x!=last_mouse.x||point.y!=last_mouse.y){last_mouse=point;note_input(PAD_INPUT_KEYBOARD);}}
 if(msg->hwnd==search&&msg->message==WM_KEYDOWN){
  if(msg->wParam==VK_F7&&cv_port_modal::key_repeat(msg->lParam))return true;
  if(msg->wParam==VK_TAB||msg->wParam==VK_UP||msg->wParam==VK_DOWN||msg->wParam==VK_RETURN||msg->wParam==VK_ESCAPE||msg->wParam==VK_F7||msg->wParam==VK_PRIOR||msg->wParam==VK_NEXT){SetFocus(window);keyboard(msg->wParam);return true;}
 }
 TranslateMessage(msg);DispatchMessageW(msg);return true;}
bool cv_archives_visible(){return window&&IsWindowVisible(window);}
void cv_archives_maps(const std::vector<cv_test::Room>& rooms,unsigned current,bool (*travel)(unsigned,unsigned)){maps=rooms;current_room=current;travel_to=travel;}

#endif
