// The F6 item menu against synthetic guest memory, for each region's own
// addresses: the catalog, the entry's word, the placement rules, the one
// write from the game frame, and the menu's navigation. With an output
// folder (and a game copy for the item art) it also draws the menu to PNGs.
#include "../cv_items.h"
#include "../cv_port_ui.h"
#include "../cv_item_icons_hd.h"
#include "../cv_srt_icons.h"
#include "../cv_item_menu.h"
#include "../cv_items_i18n.h"
#include "../cv_srt_i18n.h"
#include "../cv_version.h"
#include "../cv_launcher_controls.h"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <set>
#include <string>
#include <vector>
static std::vector<uint8_t> memory(16*1024*1024);
extern "C" {
uint8_t* vm_base=memory.data();
uint8_t vm_read8(uint64_t a) {assert(a<memory.size());return memory[a];}
uint16_t vm_read16(uint64_t a) {return uint16_t(vm_read8(a))<<8|vm_read8(a+1);}
uint32_t vm_read32(uint64_t a) {return uint32_t(vm_read16(a))<<16|vm_read16(a+2);}
void vm_write8(uint64_t a,uint8_t v) {assert(a<memory.size());memory[a]=v;}
void vm_write16(uint64_t a,uint16_t v) {vm_write8(a,uint8_t(v>>8));vm_write8(a+1,uint8_t(v));}
void vm_write32(uint64_t a,uint32_t v) {vm_write16(a,uint16_t(v>>16));vm_write16(a+2,uint16_t(v));}
int g_pad_block=0;
void* rsx_d3d12_window(void) {return nullptr;}
int pad_phys_poll(int,PadPhys*) {return 0;}
void cv_focus_pause_hold(int) {}
void perf_overlay_toast(const wchar_t*,int) {}
int pad_phys_360_layout(int) {return 0;}   // cv_port_ui::menu_pad
int pad_last_input(void) {return PAD_INPUT_KEYBOARD;}   // the menu window
}
std::wstring cv_key_cap_label(int vk) {   // button_icons.cpp: the keycaps the footer draws
    if(vk==0x0D)return L"Enter";if(vk==0x1B)return L"Esc";if(vk>=0x70&&vk<=0x87)return L"F"+std::to_wstring(vk-0x6F);
    return vk>0?std::wstring(1,wchar_t(vk)):std::wstring();
}
using namespace cv_items;
static const uint32_t state=CV_SRT_CHARACTER-0x94;
static uint32_t entry_address(int character,int entry) {return CV_SRT_INVENTORY+character*0x40+4+entry*4;}
// A live character in plain gameplay: the room's marker, positive health,
// the room task running and no screen, door or event in the task words.
static void begin(int character,bool side_pack=false) {
    std::fill(memory.begin(),memory.end(),0);
    static const uint32_t markers[]={0x3D650,0x3C4FC,0x3A4D0,0x3E814};
    vm_write8(CV_SRT_CHARACTER,uint8_t(character));vm_write32(CV_SRT_RDX,markers[character]);
    vm_write32(CV_SRT_HEALTH,150);vm_write32(state+0x64,0x80);
    if(side_pack)vm_write32(state+0x6C,0x08000000u);
    set_enabled(false);set_enabled(true);   // an empty queue
}
static void fill(int character,std::initializer_list<uint32_t> words,int equipped=0) {
    vm_write32(CV_SRT_INVENTORY+character*0x40,uint32_t(equipped));
    int i=0;for(uint32_t w:words)vm_write32(entry_address(character,i++),w);
}
// The game's inventory screen, as a JP run logged it.
static void inventory_screen() {
    vm_write32(state+0x64,0x0073DFC0);vm_write32(state+0x68,0x0003DC80);
    vm_write32(state+0x70,0x00015408);vm_write32(state+0x74,0x40);
}
static unsigned seen_serial=0;
static Outcome last() {const auto all=outcomes(seen_serial);assert(!all.empty());return all.back();}
// Plan on the live inventory as the menu does, queue, run one game frame.
static Result run(int character,int id,int quantity,int target) {
    const auto inv=read();Result why;
    const int entry=place(inv,id,target,why);if(entry<0)return why;
    const Result asked=request(character,id,quantity,entry,inv.entry[entry]);
    if(asked!=R_QUEUED)return asked;
    frame();return last().result;
}
static void check_catalog() {
    std::set<int> seen;int per[CATEGORY_COUNT]={};
    for(const auto& i:catalog()) {
        assert(i.id>0&&i.id<=0x99);assert(seen.insert(i.id).second);per[i.category]++;
        assert(i.id!=0x8D&&i.id!=0x92&&i.id!=0x93&&i.id!=0x94&&i.id!=0x97);
        for(int l=0;l<13;l++) {   // every item has a real name in every language
            const auto n=cv_srt_i18n::item(i.id,cv_srt_i18n::Language(l));
            assert(!n.empty()&&n.find(L"Item ")!=0&&n.find(L"Unknown")==std::wstring::npos&&n.find(L"desconhecido")==std::wstring::npos);
        }
        assert(i.counted?(i.quantity>=1&&i.quantity<=i.maximum):i.maximum==i.quantity);
    }
    for(int c=0;c<CATEGORY_COUNT;c++)assert(per[c]>0);
    // Only items with a real icon on the game pages; the Side Pack and "File" are gone.
    for(const auto& i:catalog()) {cv_srt_icons::Sprite sp;assert(cv_srt_icons::sprite(i.id,sp));}
    assert(!info(0x39)&&!info(0x7D)&&!info(0x4A)&&!info(0x26)&&!info(0x6D)&&!info(0x6E)&&!info(0x2F)&&!info(0x3A)&&!info(0x89));
    assert(items(OTHER)==std::vector<int>({0x91,0x1F,0x6A,0x3F,0x80,0x81}));   // manual, ink ribbon, security file, 3 briefcases
    assert(cv_srt_i18n::item(0x91,cv_srt_i18n::EN)==L"Playing Manual"&&cv_srt_i18n::item(0x1F,cv_srt_i18n::EN)==L"Ink Ribbon"&&cv_srt_i18n::item(0x6A,cv_srt_i18n::EN)==L"Security File");
    for(int id:{0x3F,0x80,0x81})assert(cv_srt_i18n::item(id,cv_srt_i18n::EN).find(L"Case:")==0);
    {   // the weapons, in the names of the translation
        const wchar_t* pt[]={L"Pistola",L"Pistola aprimorada",L"Pistola personalizada",L"Glock 17",L"Lugers douradas",L"Espingarda",L"Magnum",L"Lança-granadas",
            L"Besta",L"Rifle de precisão",L"Fuzil de assalto",L"Submetralhadoras",L"M1P",L"Lança-foguetes",L"Canhão linear",L"Faca de combate"};
        const wchar_t* en[]={L"Handgun",L"Enhanced handgun",L"Custom handgun",L"Glock 17",L"Gold Lugers",L"Shotgun",L"Magnum",L"Grenade launcher",
            L"Bow gun",L"Sniper rifle",L"Assault rifle",L"Submachine guns",L"M1P",L"Rocket launcher",L"Linear launcher",L"Combat knife"};
        const auto weapons=items(WEAPONS);assert(weapons.size()==16);
        for(size_t i=0;i<16;i++) {
            assert(cv_srt_i18n::item(weapons[i],cv_srt_i18n::PT)==pt[i]);assert(cv_srt_i18n::item(weapons[i],cv_srt_i18n::EN)==en[i]);
        }
    }
    assert(info(9)->category==WEAPONS&&info(0x0C)->category==AMMO&&info(0x15)->category==HEALING);
    assert(info(0x43)->category==KEY_ITEMS&&info(0x1F)->category==OTHER&&!info(0x8D));
    assert(items(AMMO).size()==14&&items(HEALING).size()==10);
    for(int l=0;l<13;l++)for(int s=0;s<cv_items_i18n::COUNT;s++)assert(cv_items_i18n::k_text[l][s]&&*cv_items_i18n::k_text[l][s]);
}
static void check_encoding() {
    assert(encode(9,15)==0x0009000Fu);           // handgun, 15 loaded
    assert(encode(0x0C,45)==0x000C002Du);        // handgun bullets
    assert(encode(0x15,7)==0x00150001u);         // a herb is always one
    assert(encode(8,9)==0x00080000u);            // the knife has no count, as in saves
    assert(encode(0x43,3)==0x00430001u);         // a key
    assert(encode(2,150)==0x00020064u);          // assault rifle: 100 %
    assert(encode(6,99)==0x00060032u);           // grenade launcher: type bits stay clear
    assert(encode(0x0C,0)==0x000C0001u&&encode(0x0C,9999)==0x000C00FAu);
    assert(encode(0x78,4)==0x00780004u);         // the wing objects are counted
}
static void check_read() {
    for(int c=0;c<4;c++) {
        begin(c);fill(c,{0x00370001,0x00090010,0x00080000},2);
        auto inv=read();
        assert(inv.valid&&inv.playing&&inv.character==c&&inv.capacity==8&&inv.equipped==2);
        assert(inv.id(0)==0x37&&inv.id(1)==9&&inv.quantity(1)==16&&inv.used()==2&&inv.first_empty()==3);
        assert(inv.is_equipped(1)&&!inv.is_equipped(2));
        begin(c,true);assert(read().capacity==10);
    }
    begin(0);vm_write32(CV_SRT_RDX,0x12345);assert(!read().valid);          // no room loaded
    begin(0);vm_write8(CV_SRT_CHARACTER,7);assert(!read().valid);
    begin(1);vm_write32(CV_SRT_RDX,0x3D650);assert(!read().valid);          // Claire's room marker, Chris
    // Screens (map, file, options, save), no room, death: valid, nothing may be written.
    const struct {uint32_t offset,value;} busy[]={{0x64,0x81},{0x64,0x8080},{0x64,0x10080},{0x64,0},{0x70,0x10000000},{0x70,0x08000000}};
    for(const auto& b:busy){begin(0);vm_write32(state+b.offset,b.value);const auto inv=read();assert(inv.valid&&!inv.writable&&!inv.can_remove&&!inv.playing);}
    // The game's inventory screen (JP log): the room task blocked, task 0x200
    // and +70 0x8. Put in and taken out, not plain gameplay.
    begin(0);inventory_screen();{const auto inv=read();assert(inv.writable&&inv.can_remove&&!inv.playing);}
    begin(0);inventory_screen();vm_write32(state+0x70,0x00015400);assert(!read().writable);   // 0x200 without the screen's bit
    begin(0);vm_write32(state+0x64,0x280);vm_write32(state+0x70,0x8);assert(!read().can_remove);   // the bit while the room runs: an event
    // Events, check points (typewriters), doors, fades: an item may be put
    // in, nothing removed.
    const struct {uint32_t offset,value;} event[]={{0x74,1},{0x74,2},{0x74,4},{0x70,0x08},{0x70,0x00040000},{0x70,0x04000000}};
    for(const auto& b:event){begin(0);vm_write32(state+b.offset,b.value);const auto inv=read();assert(inv.valid&&inv.writable&&!inv.can_remove&&!inv.playing);}
    begin(0);assert(read().playing&&read().can_remove);
    begin(0);vm_write32(state+0x68,0x80);assert(!read().writable);          // the room task blocked
    begin(0);vm_write32(state+0x64,0x81);vm_write32(state+0x68,0x01);assert(read().playing&&read().writable);   // blocked bits do not count
    begin(0);vm_write32(CV_SRT_HEALTH,0);assert(!read().writable);
    begin(0);vm_write32(CV_SRT_HEALTH,0xFFFFFFF0u);assert(!read().writable);
}
static void check_place() {
    Result why;
    begin(0);fill(0,{0x00370001,0x00090010,0x000C001E},2);auto inv=read();
    assert(place(inv,0x15,0,why)==3&&why==R_NONE);           // first free entry
    assert(place(inv,0x15,2,why)==2);                         // replace the bullets
    assert(place(inv,0x15,1,why)<0&&why==R_EQUIPPED);         // never the equipped weapon
    assert(place(inv,0x15,6,why)==3);                         // an empty entry past the end: the list stays packed
    assert(place(inv,0x15,9,why)<0&&why==R_BAD);              // beyond 8 without the Side Pack
    assert(place(inv,0x8D,0,why)<0&&why==R_BAD);              // unknown item
    // Full: no first free entry; a chosen one still works.
    begin(1);fill(1,{0,0x00080000,0x0005000F,0x000C001E,0x00140001,0x00150001,0x00160001,0x00170001,0x00430001},2);inv=read();
    assert(inv.used()==8&&inv.first_empty()<0);
    assert(place(inv,0x15,0,why)<0&&why==R_FULL);
    assert(place(inv,0x15,5,why)==5);
    begin(1,true);fill(1,{0,0x00080000,0x0005000F,0x000C001E,0x00140001,0x00150001,0x00160001,0x00170001,0x00430001},2);
    assert(place(read(),0x15,0,why)==9);                      // the Side Pack's two entries
    begin(0);vm_write32(state+0x74,2);assert(place(read(),0x15,0,why)==1&&why==R_NONE);           // a fade: still placed
    begin(0);vm_write32(state+0x70,0x08000000);assert(place(read(),0x15,0,why)<0&&why==R_NOT_PLAYING);   // the save screen
}
static void check_wide() {
    for(int id:{1,2,3,0x21,0x22,0x8E})assert(wide(id)&&info(id)->category==WEAPONS);
    for(int id:{9,0x83,0x0A,5,4,0x20,6,7,8,0x0B,0x15,0x0C})assert(!wide(id));
    Result why;
    // 8 spaces: handgun, shotgun, rifle (2), lugers (2), magnum: 7 used, one free, entry 6 is empty.
    const uint32_t full7[]={0x00370001,0x0009000F,0x00040007,0x00030007,0x00210014,0x00200006};
    begin(0);fill(0,{full7[0],full7[1],full7[2],full7[3],full7[4],full7[5]},2);
    auto inv=read();
    assert(inv.used()==5&&inv.spaces_used()==7&&inv.free_spaces()==1&&inv.first_empty()==6);
    assert(place(inv,3,0,why)<0&&why==R_NO_ROOM_WIDE);                 // the rifle needs two
    assert(place(inv,0x8E,0,why)<0&&why==R_NO_ROOM_WIDE);
    assert(place(inv,0x20,0,why)==6&&why==R_NONE);                      // a single one fits
    assert(place(inv,3,2,why)==2);                                      // replacing the shotgun (1 free + 1 back = 2)
    assert(place(inv,3,3,why)==3);                                      // replacing the rifle itself
    assert(place(inv,3,1,why)<0&&why==R_EQUIPPED);
    assert(place(inv,0x15,2,why)==2);                                   // normal for wide: always fits
    {   // change() on the game thread refuses it too: nothing is written
        assert(request(0,3,7,6,inv.entry[6])==R_QUEUED);
        const auto before=memory;frame();assert(memory==before&&last().result==R_NO_ROOM_WIDE);
        assert(request(0,3,7,2,inv.entry[2])==R_QUEUED);frame();      // replacing a normal item: fits
        assert(last().result==R_DONE&&vm_read32(entry_address(0,2))==0x00030007u);
        // That made it 8 of 8: the empty entry 6 takes no one.
        const auto now=read();assert(now.free_spaces()==0&&now.first_empty()==6);
        assert(place(now,0x15,0,why)<0&&why==R_FULL);
        assert(request(0,0x15,1,6,0)==R_QUEUED);const auto b2=memory;frame();assert(memory==b2&&last().result==R_FULL);
    }
    // Two free spaces: a wide weapon goes in.
    begin(0);fill(0,{full7[0],full7[1],full7[2],full7[3],full7[4]},2);inv=read();
    assert(inv.spaces_used()==6&&inv.free_spaces()==2);
    assert(place(inv,2,0,why)==5&&run(0,2,100,0)==R_DONE&&vm_read32(entry_address(0,5))==0x00020064u);
    assert(read().free_spaces()==0);
    // Side Pack: 10 spaces.
    begin(0,true);fill(0,{full7[0],full7[1],full7[2],full7[3],full7[4],0x00220064,0x00010004},2);inv=read();
    assert(inv.capacity==10&&inv.used()==6&&inv.spaces_used()==10&&inv.free_spaces()==0&&inv.first_empty()==7);   // 4 wide entries
    assert(place(inv,0x15,0,why)<0&&why==R_FULL&&place(inv,3,0,why)<0&&why==R_NO_ROOM_WIDE);
    begin(0,true);fill(0,{full7[0],full7[1],full7[2],full7[3],full7[4]},2);inv=read();
    assert(inv.spaces_used()==6&&inv.free_spaces()==4&&place(inv,3,0,why)==5);
    // Replacing a wide by a normal item frees a space.
    begin(0);fill(0,{full7[0],full7[1],full7[2],full7[3],full7[4],full7[5]},2);
    assert(run(0,0x15,1,3)==R_DONE&&read().free_spaces()==2);
    while(pending())frame();
    // The menu: with one free space the rifle's default is out; only the entries that give enough room are offered.
    begin(0);fill(0,{full7[0],full7[1],full7[2],full7[3],full7[4],full7[5]},2);
    cv_item_menu::View v;refresh(v,read());v.category=WEAPONS;v.selected=9;   // the rifle
    assert(v.item()==3);
    cv_item_menu::apply(v,cv_item_menu::CONFIRM);
    assert(v.focus==1&&v.target==2&&cv_item_menu::targets(v)==std::vector<int>({2,3,4,5}));
    v.target=0;assert(cv_item_menu::submit(v)==R_NO_ROOM_WIDE&&v.tone==cv_item_menu::BAD&&pending()==0);
    assert(v.status==cv_items_i18n::k_text[v.language][cv_items_i18n::NO_TWO_SLOTS]);
    v.category=WEAPONS;v.selected=13;v.focus=0;    // the rocket launcher: same
    assert(v.item()==1);cv_item_menu::apply(v,cv_item_menu::CONFIRM);assert(!cv_item_menu::targets(v).empty()&&cv_item_menu::targets(v).front()!=0);
    v.selected=6;v.focus=0;assert(v.item()==0x20);cv_item_menu::apply(v,cv_item_menu::CONFIRM);assert(cv_item_menu::targets(v).front()==0);   // the magnum fits
}
static void check_layout() {
    auto count=[](const std::vector<Cell>& l,Cell::Kind k){int n=0;for(const auto& c:l)n+=c.kind==k;return n;};
    auto at=[](const std::vector<Cell>& l,int entry)->const Cell*{for(const auto& c:l)if(c.kind==Cell::ITEM&&c.entry==entry)return &c;return nullptr;};
    auto cells=[](const std::vector<Cell>& l){int n=0;for(const auto& c:l)if(c.kind!=Cell::LOCKED)n+=c.span;return n;};
    // The preview inventory: two ordinary, two wide: rows [1 2] [rifle] [lugers] [empty empty] [locked locked].
    begin(0);fill(0,{0x00370001,0x0009000F,0x00040007,0x00030007,0x00210014},2);
    auto l=layout(read());
    assert(count(l,Cell::ITEM)==4&&count(l,Cell::EMPTY)==2&&count(l,Cell::LOCKED)==2&&cells(l)==8);
    assert(at(l,1)->row==0&&at(l,1)->col==0&&at(l,1)->span==1&&at(l,2)->row==0&&at(l,2)->col==1);
    assert(at(l,3)->row==1&&at(l,3)->col==0&&at(l,3)->span==2&&at(l,4)->row==2&&at(l,4)->span==2);
    for(const auto& c:l)if(c.kind==Cell::EMPTY)assert(c.row==3);else if(c.kind==Cell::LOCKED)assert(c.row==4);
    // A wide that would start in the right column moves down and leaves that cell open.
    begin(0);fill(0,{0x00370001,0x0009000F,0x00030007},2);l=layout(read());
    assert(at(l,1)->row==0&&at(l,1)->col==0&&at(l,2)->row==1&&at(l,2)->span==2);
    {int holes=0;for(const auto& c:l)if(c.kind==Cell::EMPTY&&c.row==0&&c.col==1)holes++;assert(holes==1);}
    assert(cells(l)==8&&count(l,Cell::EMPTY)==5);
    // ... which the next ordinary item takes.
    begin(0);fill(0,{0x00370001,0x0009000F,0x00030007,0x00200006},2);l=layout(read());
    assert(at(l,3)->row==0&&at(l,3)->col==1&&at(l,2)->row==1&&cells(l)==8&&count(l,Cell::EMPTY)==4);
    // Wide after a wide: no hole.
    begin(0);fill(0,{0x00370001,0x00030007,0x00210014,0x0009000F},2);l=layout(read());
    assert(at(l,1)->row==0&&at(l,2)->row==1&&at(l,3)->row==2&&at(l,3)->col==0&&cells(l)==8);
    // Side Pack: ten cells, no locked ones. Nothing: eight empty and two locked.
    begin(0,true);fill(0,{0x00370001,0x00090001,0x00090001,0x00090001});l=layout(read());
    assert(count(l,Cell::LOCKED)==0&&cells(l)==10&&count(l,Cell::ITEM)==3&&count(l,Cell::EMPTY)==7);
    {Inventory none;l=layout(none);assert(count(l,Cell::EMPTY)==8&&count(l,Cell::LOCKED)==2);}
    // Every row is at most two cells wide, no cell is used twice.
    begin(0,true);fill(0,{0x00370001,0x00030007,0x00090001,0x00210014,0x00090001,0x00090001,0x00220064,0x00090001},2);l=layout(read());
    {int grid[16][2]={};for(const auto& c:l){assert(c.row<16&&c.col+c.span<=2);for(int k=0;k<c.span;k++)assert(++grid[c.row][c.col+k]==1);}}
}
#ifdef _WIN32
static uint64_t region_hash(Gdiplus::Bitmap& b,int x,int y,int w,int h) {
    Gdiplus::BitmapData d{};Gdiplus::Rect r(x,y,w,h);const auto ok=b.LockBits(&r,Gdiplus::ImageLockModeRead,PixelFormat32bppARGB,&d);assert(ok==Gdiplus::Ok);(void)ok;
    uint64_t hash=1469598103934665603ull;
    for(int j=0;j<h;j++){const uint8_t* row=static_cast<const uint8_t*>(d.Scan0)+j*d.Stride;for(int i=0;i<w*4;i++){hash^=row[i];hash*=1099511628211ull;}}
    b.UnlockBits(&d);return hash;
}
static const cv_item_menu::Hit* find_hit(const cv_item_menu::View& v,int kind,int value) {
    for(const auto& h:v.hits)if(h.kind==kind&&h.value==value)return &h;
    return nullptr;
}
// Hover and pressed feedback on every kind of clickable control; HD icons with the atlas as fallback; every language draws.
static void check_drawing() {
    using namespace cv_item_menu;
    for(const auto& i:catalog())assert(cv_item_icons_hd::get(i.id));            // every offered item has its HD icon
    assert(cv_item_icons_hd::get(0x26)==nullptr&&cv_item_icons_hd::get(0x39)==nullptr);   // none for an id without a file: the atlas, else the name
    begin(0);fill(0,{0x00370001,0x0009000F,0x00040007,0x00030007,0x00210014,0x00200006},2);
    View v;v.language=0;refresh(v,read());v.selected=6;
    Gdiplus::Bitmap bmp(W,H,PixelFormat32bppARGB);
    auto render=[&](){Gdiplus::Graphics g(&bmp);draw(g,v,L"");};
    render();
    // Every kind of control: normal, hover and pressed look different.
    const struct {int kind,value;} controls[]={{HIT_CLOSE,0},{HIT_TAB,2},{HIT_ITEM,1},{HIT_SLOT,2},{HIT_QTY_PLUS,0},{HIT_TARGET_PREV,0}};
    for(const auto& ctl:controls) {
        v.mouse.leave();render();const auto* hit=find_hit(v,ctl.kind,ctl.value);assert(hit);
        const int x=int(hit->rect.X),y=int(hit->rect.Y),w=int(hit->rect.Width),h=int(hit->rect.Height);
        const auto normal=region_hash(bmp,x,y,w,h);
        v.mouse.move(hit_id(ctl.kind,ctl.value));render();const auto hover=region_hash(bmp,x,y,w,h);
        v.mouse.down(hit_id(ctl.kind,ctl.value));render();const auto pressed=region_hash(bmp,x,y,w,h);
        assert(normal!=hover&&hover!=pressed&&normal!=pressed);
        v.mouse.leave();
    }
    {   // the main button, enabled: three looks
        v.mouse.leave();render();const auto* b=find_hit(v,HIT_PLACE,0);assert(b);
        const int x=int(b->rect.X),y=int(b->rect.Y),w=int(b->rect.Width),h=int(b->rect.Height);
        const auto normal=region_hash(bmp,x,y,w,h);v.mouse.move(hit_id(HIT_PLACE,0));render();const auto hover=region_hash(bmp,x,y,w,h);
        v.mouse.down(hit_id(HIT_PLACE,0));render();assert(normal!=hover&&hover!=region_hash(bmp,x,y,w,h));v.mouse.leave();
    }
    {   // the footer follows the input device
        v.input=PAD_INPUT_KEYBOARD;render();const auto kb=region_hash(bmp,24,776,1352,40);
        v.input=PAD_KIND_DS4;render();const auto ps=region_hash(bmp,24,776,1352,40);
        v.input=PAD_KIND_XINPUT;render();const auto xb=region_hash(bmp,24,776,1352,40);
        v.input=PAD_KIND_SWITCH;render();const auto sw=region_hash(bmp,24,776,1352,40);
        assert(kb!=ps&&ps!=xb&&xb!=sw&&kb!=xb&&ps!=sw);v.input=PAD_INPUT_KEYBOARD;
    }
    {   // the hits hold the geometry of the design: close, tabs, 16 cells, button
        render();const auto* close=find_hit(v,HIT_CLOSE,0);assert(close&&close->rect.X==1334&&close->rect.Y==14&&close->rect.Width==40&&close->rect.Height==40);
        for(int i=0;i<5;i++)assert(find_hit(v,HIT_TAB,i));
        for(int i=0;i<16;i++)assert(find_hit(v,HIT_ITEM,i));
        const auto* cell0=find_hit(v,HIT_ITEM,0);assert(cell0&&cell0->rect.Width==220&&cell0->rect.Height==104&&cell0->rect.X==37&&cell0->rect.Y==143);
        const auto* place=find_hit(v,HIT_PLACE,0);assert(place&&place->rect.X==976&&place->rect.Y==700&&place->rect.Width==400&&place->rect.Height==42);
        assert(hit_at(v,close->rect.X+5,close->rect.Y+5)==close&&!hit_at(v,2,2));
    }
    for(int l=0;l<13;l++)for(int c=0;c<CATEGORY_COUNT;c++){v.language=l;v.category=c;v.selected=0;v.mouse.leave();render();}
}
#endif
static void check_writes() {
    seen_serial=outcome_serial();
    for(int c=0;c<4;c++) {   // Claire, Chris, Steve, Wesker: each their own entries
        begin(c);fill(c,{0x00370001,0x00090010},2);
        const auto before=memory;
        assert(run(c,0x0C,45,0)==R_DONE);
        assert(vm_read32(entry_address(c,2))==0x000C002Du);
        size_t changed=0;for(size_t i=0;i<memory.size();i++)changed+=memory[i]!=before[i];
        assert(changed==2);                                    // 00 0C 00 2D over zeros: one word, nothing else
        for(int other=0;other<4;other++)if(other!=c)assert(vm_read32(entry_address(other,2))==0);
    }
    // Replacing keeps the rest, flags of the new entry clear, equipped untouched.
    begin(0);fill(0,{0x00370001,0x20070063,0x000C001E,0x00140001},2);
    assert(run(0,0x15,1,2)==R_DONE);
    assert(vm_read32(entry_address(0,1))==0x20070063u&&vm_read32(entry_address(0,2))==0x00150001u&&vm_read32(entry_address(0,3))==0x00140001u);
    assert(vm_read32(CV_SRT_INVENTORY)==2);
    assert(run(0,0x15,1,1)==R_EQUIPPED);
    // Several choices while paused: the menu plans each on the inventory it
    // shows (the live one with the queue in place); one frame puts them all.
    begin(1);fill(1,{0,0x00080000,0x0005000F},3);
    Inventory shown=read();
    for(int id:{0x15,0x16,0x0C}) {
        Result why;const int entry=place(shown,id,0,why);assert(entry>0);
        assert(request(1,id,30,entry,shown.entry[entry])==R_QUEUED);
        shown=read();apply_pending(shown);
        assert(shown.id(entry)==id);
    }
    assert(pending()==3&&vm_read32(entry_address(1,3))==0);   // nothing written before the frame
    frame();assert(pending()==0);
    assert(vm_read32(entry_address(1,3))==0x00150001u&&vm_read32(entry_address(1,4))==0x00160001u&&vm_read32(entry_address(1,5))==0x000C001Eu);
    {const auto all=outcomes(seen_serial);assert(all.size()==3);for(const auto& o:all)assert(o.result==R_DONE);}
    // The queue holds ten.
    begin(0,true);fill(0,{0x00370001});
    for(int i=0;i<kQueue;i++)assert(request(0,0x15,1,i+1,0)==R_QUEUED);
    assert(request(0,0x16,1,1,0)==R_BUSY);
    frame();for(int e=1;e<=10;e++)assert(vm_read32(entry_address(0,e))==0x00150001u);
    outcomes(seen_serial);
    // The option off: nothing is queued; turned off while waiting: dropped.
    begin(0);fill(0,{0x00370001});set_enabled(false);auto before=memory;
    assert(request(0,0x15,1,1,0)==R_DISABLED);frame();assert(memory==before);
    begin(0);fill(0,{0x00370001});assert(request(0,0x15,1,1,0)==R_QUEUED);set_enabled(false);
    assert(pending()==0);before=memory;frame();assert(memory==before);
    // A screen opened before the frame: no write.
    begin(0);fill(0,{0x00370001});assert(request(0,0x15,1,1,0)==R_QUEUED);
    vm_write32(state+0x64,0x82);before=memory;frame();assert(memory==before&&last().result==R_NOT_PLAYING);
    // By a typewriter or in an event: the item goes in, a removal waits.
    begin(0);fill(0,{0x00370001,0x000C001E});vm_write32(state+0x70,0x00040008);vm_write32(state+0x74,4);
    assert(run(0,0x15,1,0)==R_DONE&&vm_read32(entry_address(0,2))==0x00150001u);
    assert(request_remove(0,1,0x000C001E)==R_QUEUED);before=memory;frame();assert(memory==before&&last().result==R_NOT_PLAYING);
    // With the game's inventory screen open: both.
    begin(0);fill(0,{0x00370001,0x000C001E});inventory_screen();
    assert(run(0,0x15,1,0)==R_DONE&&vm_read32(entry_address(0,2))==0x00150001u);
    assert(request_remove(0,1,0x000C001E)==R_QUEUED);frame();
    assert(last().result==R_DONE&&vm_read32(entry_address(0,1))==0x00150001u&&vm_read32(entry_address(0,2))==0);
    // Another character by the frame, or the entry changed meanwhile: no write.
    begin(0);fill(0,{0x00370001});assert(request(1,0x15,1,1,0)==R_QUEUED);before=memory;frame();assert(memory==before&&last().result==R_CHANGED);
    begin(0);fill(0,{0x00370001,0x000C001E});
    assert(request(0,0x15,1,1,0x000C001E)==R_QUEUED);vm_write32(entry_address(0,1),0x000C001D);   // a shot fired
    before=memory;frame();assert(memory==before&&last().result==R_CHANGED);
    begin(0);fill(0,{0x00370001});
    assert(request(0,0x15,1,2,0)==R_QUEUED);before=memory;frame();   // empty, but not the list's end
    assert(memory==before&&last().result==R_CHANGED);
    begin(0);fill(0,{0x00370001});assert(request(0,0x15,1,9,0)==R_QUEUED);before=memory;frame();   // no Side Pack
    assert(memory==before&&last().result==R_BAD);
    // Full inventory with no entry chosen.
    begin(0);fill(0,{0x00370001,1<<16,2<<16,3<<16,4<<16,5<<16,6<<16,7<<16,9<<16});
    before=memory;assert(run(0,0x15,1,0)==R_FULL&&memory==before);
    assert(request(0,0x8D,1,1,0)==R_BAD&&request(5,0x15,1,1,0)==R_BAD&&request(0,0x15,1,0,0)==R_BAD);
    // Removing, as the game takes a used-up item out: the entries below move
    // up and the equipped number follows its weapon.
    begin(0);fill(0,{0x00370001,0x000C001E,0x00150001,0x00090010,0x00140001},4);   // the handgun (entry 3) equipped
    assert(request_remove(0,1,0x000C001E)==R_QUEUED);frame();
    {const auto o=last();assert(o.result==R_DONE&&o.removed&&o.entry==1&&o.id==0x0C);}
    assert(vm_read32(entry_address(0,0))==0x00370001u&&vm_read32(entry_address(0,1))==0x00150001u&&vm_read32(entry_address(0,2))==0x00090010u);
    assert(vm_read32(entry_address(0,3))==0x00140001u&&vm_read32(entry_address(0,4))==0&&vm_read32(CV_SRT_INVENTORY)==3);
    assert(request_remove(0,2,0x00090010)==R_QUEUED);before=memory;frame();assert(memory==before&&last().result==R_EQUIPPED);
    assert(request_remove(0,3,0x00140001)==R_QUEUED);frame();
    assert(vm_read32(entry_address(0,3))==0&&vm_read32(CV_SRT_INVENTORY)==3);   // after the weapon: its number stays
    assert(request_remove(0,1,0x00160001)==R_QUEUED);before=memory;frame();assert(memory==before&&last().result==R_CHANGED);
    assert(request_remove(0,0,0x00370001)==R_BAD&&request_remove(0,1,0)==R_BAD);   // never the fixed slot; nothing to remove
    // A full list: the last entry empties.
    begin(0,true);fill(0,{0x00370001,1<<16,2<<16,3<<16,4<<16,5<<16,6<<16,7<<16,9<<16,0x000C0001,0x00150001});
    assert(request_remove(0,5,5<<16)==R_QUEUED);frame();
    assert(vm_read32(entry_address(0,5))==(6u<<16)&&vm_read32(entry_address(0,9))==0x00150001u&&vm_read32(entry_address(0,10))==0);
    // Removals and additions mixed in one queue, planned on the shown inventory.
    begin(1);fill(1,{0,0x00080000,0x0005000F},3);
    shown=read();
    assert(request_remove(1,1,shown.entry[1])==R_QUEUED);shown=read();apply_pending(shown);
    assert(shown.entry[1]==0x0005000Fu&&shown.equipped==2&&shown.first_empty()==2);
    {Result why;const int e=place(shown,0x15,0,why);assert(e==2&&request(1,0x15,1,e,shown.entry[e])==R_QUEUED);}
    frame();
    assert(vm_read32(entry_address(1,1))==0x0005000Fu&&vm_read32(entry_address(1,2))==0x00150001u&&vm_read32(CV_SRT_INVENTORY+0x40)==2);
    outcomes(seen_serial);
}
static void check_navigation() {
    using namespace cv_item_menu;
    begin(0);fill(0,{0x00370001,0x00090010,0x000C001E},2);
    View v;v.serial=outcome_serial();refresh(v,read());
    assert(v.category==WEAPONS&&v.item()==items(WEAPONS)[0]);
    apply(v,RIGHT);apply(v,DOWN);assert(v.selected==5);
    apply(v,NEXT);assert(v.category==AMMO&&v.selected==0);
    apply(v,PREV);apply(v,PREV);assert(v.category==OTHER);
    apply(v,NEXT);apply(v,NEXT);assert(v.category==AMMO);
    assert(!apply(v,CONFIRM)&&v.focus==1&&v.row==0&&v.quantity==info(v.item())->quantity&&v.target==0);
    apply(v,RIGHT);assert(v.quantity==info(v.item())->quantity+1);
    apply(v,NEXT);assert(v.quantity==info(v.item())->quantity+11);
    apply(v,DOWN);apply(v,RIGHT);assert(v.target==2);           // entry 1 is equipped: skipped
    apply(v,RIGHT);assert(v.target==0);
    assert(targets(v)==std::vector<int>({0,2}));
    apply(v,LEFT);assert(v.target==2);
    const uint32_t word=uint32_t(items(AMMO)[0])<<16|uint32_t(info(items(AMMO)[0])->quantity+11);
    assert(submit(v)==R_QUEUED&&v.tone==GOOD&&v.focus==0);
    assert(v.inventory.entry[2]==word&&vm_read32(entry_address(0,2))==0x000C001Eu);   // shown at once, written later
    refresh(v,read());assert(v.inventory.entry[2]==word);         // still shown: the queue is applied
    frame();poll_result(v);assert(v.tone==GOOD&&vm_read32(entry_address(0,2))==word);
    assert(apply(v,BACK));                                        // Esc on the grid closes
    bool ok=false;
    assert(summary(0,{{R_DONE,3,0x15}},ok)==L"Colocado no espaço 3: Erva verde"&&ok);
    assert(summary(1,{{R_DONE,3,0x15},{R_DONE,4,0x16}},ok)==L"2 items put in the inventory"&&ok);
    assert(summary(0,{{R_DONE,3,0x15},{R_CHANGED,4,0x16}},ok)==cv_items_i18n::k_text[0][cv_items_i18n::CHANGED]&&!ok);
    // Herbs have no quantity row; full inventory proposes a space to replace.
    begin(1);fill(1,{0,0x00080000,0x0005000F,0x000C001E,0x00140001,0x00150001,0x00160001,0x00170001,0x00430001},2);
    View full;refresh(full,read());full.category=HEALING;apply(full,CONFIRM);
    assert(full.row==1&&full.target==2);                         // entry 1 holds the equipped knife
    // Another character in between closes the panel.
    begin(0);fill(0,{0x00370001});refresh(full,read());assert(full.focus==0);
    // Removing from the menu: R, the space, confirm; never the equipped weapon.
    begin(0);fill(0,{0x00370001,0x00090010,0x000C001E},2);
    View r;r.language=0;refresh(r,read());
    apply(r,REMOVE);assert(r.focus==2&&r.slot==2);
    apply(r,LEFT);assert(r.slot==1);
    assert(submit(r)==R_EQUIPPED&&r.tone==BAD&&pending()==0);
    apply(r,RIGHT);assert(r.slot==2);
    assert(submit(r)==R_QUEUED&&r.tone==GOOD&&r.inventory.empty(2)&&vm_read32(entry_address(0,2))==0x000C001Eu);
    apply(r,DOWN);apply(r,BACK);assert(r.focus==0);
    frame();assert(vm_read32(entry_address(0,2))==0);
    assert(summary(0,{{R_DONE,2,0x0C,true}},ok)==L"Removido do espaço 2: Munição de pistola"&&ok);
    assert(summary(1,{{R_DONE,2,0x0C,true},{R_DONE,2,0x15}},ok)==L"2 changes made to the inventory");
    while(pending())frame();
}
#ifdef _WIN32
#pragma comment(lib,"gdiplus.lib")
// Draws the window to a PNG: `backdrop` centres it on a dark game-like canvas (1920x1080 as the approved previews), else the
// window fills the image.
static void export_png(const std::string& file,cv_item_menu::View& v,const std::wstring& game,int w,int h,bool backdrop=false) {
    using namespace Gdiplus;
    Bitmap image(w,h,PixelFormat32bppARGB);
    {Graphics g(&image);
     if(backdrop) {
         LinearGradientBrush bg(Point(0,0),Point(w,h),Color(255,40,18,14),Color(255,8,8,12));g.FillRectangle(&bg,0,0,w,h);
         const float s=std::min(w/1920.f,h/1080.f);
         g.TranslateTransform((w-cv_item_menu::W*s)/2,(h-cv_item_menu::H*s)/2+5*s);g.ScaleTransform(s,s);
     } else g.ScaleTransform(float(w)/cv_item_menu::W,float(h)/cv_item_menu::H);
     cv_item_menu::draw(g,v,game);}
    UINT n=0,size=0;GetImageEncodersSize(&n,&size);std::vector<uint8_t> buffer(size);
    auto* codecs=reinterpret_cast<ImageCodecInfo*>(buffer.data());GetImageEncoders(n,size,codecs);
    CLSID png{};for(UINT i=0;i<n;i++)if(!wcscmp(codecs[i].MimeType,L"image/png"))png=codecs[i].Clsid;
    const std::wstring name(file.begin(),file.end());
    const auto saved=image.Save(name.c_str(),&png);assert(saved==Ok);(void)saved;
}
static void previews(const char* folder,const char* game_path) {
    using namespace cv_item_menu;
    const std::string dir=folder;std::wstring game;if(game_path)for(const char* c=game_path;*c;c++)game+=wchar_t(*c);
    auto file=[&](const char* tag){return dir+"/items-"+CV_TITLE_ID+"-"+tag+".png";};
    // The approved previews: Claire with the pistol equipped, the shotgun, the rifle and the gold Lugers.
    const uint32_t base[]={0x00370001,0x0009000F,0x00040007,0x00030007,0x00210014};
    begin(0);fill(0,{base[0],base[1],base[2],base[3],base[4]},2);
    View a;a.language=0;a.selected=6;a.input=PAD_INPUT_KEYBOARD;refresh(a,read());     // inventario-a: PT, keyboard, Magnum
    export_png(file("pt-kb"),a,game,1920,1080,true);
    begin(0);fill(0,{base[0],base[1],base[2],base[3],base[4],0x00200006},2);
    View b;b.language=0;b.selected=9;b.input=PAD_KIND_DS4;refresh(b,read());           // inventario-b: PT, PlayStation, the rifle refused
    export_png(file("pt-ps"),b,game,1920,1080,true);
    begin(0);fill(0,{base[0],base[1],base[2],base[3],base[4]},2);
    View c;c.language=1;c.selected=6;c.input=PAD_KIND_XINPUT;refresh(c,read());       // inventario-c: EN, Xbox
    export_png(file("en-xbox"),c,game,1920,1080,true);
    // Switch glyphs, the panel open with the quantity chosen, and the mouse over / on controls.
    View sw=c;sw.input=PAD_KIND_SWITCH;sw.mouse.leave();export_png(file("en-switch"),sw,game,1400,820);
    View panel=a;panel.input=PAD_INPUT_KEYBOARD;apply(panel,CONFIRM);apply(panel,RIGHT);apply(panel,RIGHT);apply(panel,DOWN);
    export_png(file("pt-panel"),panel,game,1400,820);
    // A wide weapon with the panel open: its two spaces are shown where it would go.
    begin(0);fill(0,{base[0],base[1],base[2]},2);
    View wide_panel;wide_panel.language=0;wide_panel.selected=13;refresh(wide_panel,read());apply(wide_panel,CONFIRM);
    export_png(file("pt-wide-panel"),wide_panel,game,1400,820);
    View hover=a;hover.mouse.move(hit_id(HIT_ITEM,2));export_png(file("pt-hover-cell"),hover,game,1400,820);
    View tab=a;tab.mouse.move(hit_id(HIT_TAB,3));export_png(file("pt-hover-tab"),tab,game,1400,820);
    View close=a;close.mouse.down(hit_id(HIT_CLOSE,0));export_png(file("pt-pressed-close"),close,game,1400,820);
    View button=a;button.mouse.down(hit_id(HIT_PLACE,0));export_png(file("pt-pressed-button"),button,game,1400,820);
    // Removing, a key item, a full inventory with the Side Pack, a screen open in the game.
    begin(0);fill(0,{base[0],base[1],base[2],base[3],base[4],0x00200006},2);
    View remove;remove.language=0;refresh(remove,read());apply(remove,REMOVE);apply(remove,DOWN);
    export_png(file("pt-remove"),remove,game,1400,820);
    View key;key.language=0;refresh(key,read());key.category=KEY_ITEMS;key.selected=9;apply(key,DOWN);apply(key,DOWN);
    export_png(file("pt-key-item"),key,game,1400,820);
    begin(1,true);fill(1,{0,0x00080000,0x0005000F,0x000C001E,0x00140001,0x00150001,0x00160001,0x00170001,0x00430001,0x00210014,0x000F0006},3);
    View full;full.language=3;refresh(full,read());full.category=HEALING;full.selected=1;apply(full,CONFIRM);
    export_png(file("ja-full"),full,game,1400,820);
    full.language=1;full.status.clear();export_png(file("en-full"),full,game,1400,820);
    begin(0);fill(0,{base[0],base[1]},2);vm_write32(state+0x64,0x8080);
    View busy;busy.language=0;refresh(busy,read());busy.category=AMMO;apply(busy,CONFIRM);
    export_png(file("pt-not-playing"),busy,game,1400,820);
    for(int l=0;l<13;l++) {   // every language's texts in place
        begin(0);fill(0,{base[0],base[1],base[2],base[3],base[4]},2);
        View each;each.language=l;refresh(each,read());each.category=KEY_ITEMS;each.selected=0;apply(each,CONFIRM);
        each.status=cv_items_i18n::k_text[l][cv_items_i18n::NO_TWO_SLOTS];each.tone=cv_item_menu::BAD;each.input=PAD_KIND_XINPUT;
        char tag[32];std::snprintf(tag,sizeof tag,"lang-%02d",l);export_png(file(tag),each,game,1400,820);
    }
}
#endif
int main(int argc,char** argv) {
    check_catalog();check_encoding();check_read();check_place();check_wide();check_layout();check_writes();check_navigation();
#ifdef _WIN32
    Gdiplus::GdiplusStartupInput input;ULONG_PTR token=0;Gdiplus::GdiplusStartup(&token,&input,nullptr);
    cv_port_ui::fonts_available();   // loaded before any Graphics exists, as the menu window does when it opens
    check_drawing();
    if(argc>1)previews(argv[1],argc>2?argv[2]:nullptr);
    cv_item_menu::release();cv_item_icons_hd::release();
    // No GdiplusShutdown: the keycap cache (cv_keycap::draw) holds GDI+ images in a function-static map that is destroyed at exit.
    (void)token;
#endif
    std::printf("cv_items tests passed (%s, %zu items)\n",CV_TITLE_ID,catalog().size());
    return 0;
}
