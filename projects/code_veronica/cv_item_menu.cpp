/* The F6 item menu: choose an item, its quantity and a space; the game frame
 * puts it in the live inventory (cv_items.cpp). The window follows the approved
 * previews (docs/previas-v1.7/inventario-a/b/c.png): gold frame, Cinzel / Barlow Semi Condensed,
 * HD item icons embedded in the program (cv_item_icons_hd), the icons of the selected game copy
 * (as the SRT reads them) only as a fallback. */
#include "cv_item_menu.h"
#ifdef _WIN32
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cwctype>
#include <map>
#include <memory>
#include <tuple>
#include <dwmapi.h>
#include "cv_item_icons_hd.h"
#include "cv_items_i18n.h"
#include "cv_port_ui_i18n.h"
#include "cv_srt_i18n.h"
#include "cv_srt_icons.h"
#include "cv_launcher_controls.h"
#include "cv_port_modal.h"
#include "cv_port_ui.h"
#pragma comment(lib,"gdiplus.lib")
#pragma comment(lib,"dwmapi.lib")
using namespace Gdiplus;
extern "C" void* rsx_d3d12_window(void);
extern "C" void perf_overlay_toast(const wchar_t* text,int on);    // the notice in the game's corner

namespace cv_item_menu {
namespace {
namespace tx=cv_items_i18n;
namespace ui=cv_port_ui;
namespace col=cv_port_ui::col;
using ui::ButtonState;using ui::Role;
const wchar_t* t(const View& v,tx::Id id) {return tx::k_text[std::clamp(v.language,0,12)][id];}
cv_srt_i18n::Language lang(const View& v) {return cv_srt_i18n::Language(std::clamp(v.language,0,12));}
std::wstring name(const View& v,int id) {return cv_srt_i18n::item(id,lang(v));}
std::wstring format(const wchar_t* pattern,int n,const std::wstring& s=L"") {
    wchar_t out[256];swprintf(out,256,pattern,n,s.c_str());return out;
}
std::wstring upper(std::wstring s) {for(auto& c:s)c=wchar_t(std::towupper(c));return s;}
const wchar_t* character_name(int id) {
    static const wchar_t* names[]={L"CLAIRE REDFIELD",L"CHRIS REDFIELD",L"STEVE BURNSIDE",L"ALBERT WESKER"};
    return id>=0&&id<4?names[id]:L"—";
}
int need_of(int id) {return cv_items::wide(id)?2:1;}
// Room for the item in the first free entry; and for it replacing an occupied entry (which gives its own spaces back).
bool fits_free(const cv_items::Inventory& inv,int id) {return inv.first_empty()>0&&inv.free_spaces()>=need_of(id);}
bool fits_replace(const cv_items::Inventory& inv,int id,int entry) {return inv.free_spaces()+need_of(inv.id(entry))>=need_of(id);}

// ---- Images ---------------------------------------------------------------
// The icons read from the selected game copy: the fallback of an id without an HD file.
struct Images {
    cv_srt_icons::Library library;std::map<int,std::unique_ptr<Bitmap>> cache;std::wstring root;
    Bitmap* get(const std::wstring& game,int id) {
        if(root!=game) {root=game;cache.clear();}
        library.load(game);
        auto found=cache.find(id);if(found!=cache.end())return found->second.get();
        auto& slot=cache[id];
        const auto* icon=library.get(id);if(!icon)return nullptr;
        auto image=std::make_unique<Bitmap>(icon->width,icon->height,PixelFormat32bppARGB);
        Rect r(0,0,icon->width,icon->height);BitmapData d{};
        if(image->LockBits(&r,ImageLockModeWrite,PixelFormat32bppARGB,&d)!=Ok)return nullptr;
        for(int y=0;y<icon->height;y++)std::memcpy(static_cast<uint8_t*>(d.Scan0)+y*d.Stride,icon->argb.data()+size_t(y)*icon->width,size_t(icon->width)*4);
        image->UnlockBits(&d);slot=std::move(image);return slot.get();
    }
} images;
// The square portraits the SRT uses (launcher_art/srt_<name>.png, RCDATA).
std::map<int,std::unique_ptr<Bitmap>> portraits;
Bitmap* portrait(int character) {
    auto& cache=portraits;
    if(character<0||character>3)return nullptr;
    auto found=cache.find(character);if(found!=cache.end())return found->second.get();
    auto& image=cache[character];
    const wchar_t* names[]={L"SRT_CLAIRE",L"SRT_CHRIS",L"SRT_STEVE",L"SRT_WESKER"};
    const HMODULE module=GetModuleHandleW(nullptr);const HRSRC res=FindResourceW(module,names[character],MAKEINTRESOURCEW(10));
    if(!res)return nullptr;const DWORD size=SizeofResource(module,res);
    const void* data=LockResource(LoadResource(module,res));if(!data||!size||size>4*1024*1024)return nullptr;
    HGLOBAL block=GlobalAlloc(GMEM_MOVEABLE,size);if(!block)return nullptr;
    void* bytes=GlobalLock(block);if(!bytes){GlobalFree(block);return nullptr;}
    std::memcpy(bytes,data,size);GlobalUnlock(block);
    IStream* stream=nullptr;if(CreateStreamOnHGlobal(block,TRUE,&stream)!=S_OK){GlobalFree(block);return nullptr;}
    {Bitmap source(stream);if(source.GetLastStatus()==Ok&&source.GetWidth()&&source.GetWidth()<=1024)
        image.reset(source.Clone(0,0,int(source.GetWidth()),int(source.GetHeight()),PixelFormat32bppARGB));}
    stream->Release();return image.get();
}

// ---- Layout (mock.css, window coordinates) -----------------------------------------
constexpr int COLS=4,ROWS=4;
constexpr float CAT_X=24,CAT_Y=136,CAT_W=928,CAT_H=452;           // the catalog panel
constexpr float GRID_X=37,GRID_Y=143,CELL_W=220,CELL_H=104,GAP=8;  // 4x4 cells
constexpr float BAN_X=24,BAN_Y=600,BAN_W=928,BAN_H=132;            // the banner of the selected item
constexpr float RX=976,RW=400;                                      // the right column
constexpr float CARD_Y=216,CARD_H=68,CARD_STEP=76,CARD_NARROW=194,CARD_GAP2=206;
constexpr float STEP1_Y=604,STEP2_Y=650,STEP_H=38,BTN_Y=700,BTN_H=42,FOOT_Y=776;
const Color C_MUTED2(255,136,146,174),C_MUTED3(255,125,138,174),C_LABEL(255,183,191,214);

// ---- Drawing helpers ------------------------------------------------------------------
struct Ctx {Graphics& g;View& v;const std::wstring& game;int lang() const {return v.language;}};
const Font* F(const Ctx& c,Role role,float px) {return &ui::font_cached(c.v.language,role,px);}
// The font shrunk (to `min_px`) until `s` fits `max_w`.
const Font* fit(const Ctx& c,Role role,float px,const std::wstring& s,float max_w,float tracking=0,float min_px=10) {
    const Font* f=F(c,role,px);
    while(px>min_px&&ui::text_width(c.g,s,*f,tracking)>max_w){px-=0.5f;f=F(c,role,px);}
    return f;
}
Color mix(Color c,float t,int to) {
    return Color(c.GetA(),BYTE(c.GetR()+(to-int(c.GetR()))*t),BYTE(c.GetG()+(to-int(c.GetG()))*t),BYTE(c.GetB()+(to-int(c.GetB()))*t));
}
Color shade(Color c,ButtonState st) {return st==ButtonState::HOVER?mix(c,.14f,255):st==ButtonState::PRESSED?mix(c,.25f,0):c;}
void fill_v(Graphics& g,RectF r,Color a,Color b,float radius) {
    LinearGradientBrush br(PointF(r.X,r.Y),PointF(r.X,r.Y+r.Height+0.01f),a,b);
    auto p=ui::round_path(r,radius);g.FillPath(&br,p.get());
}
void stroke(Graphics& g,RectF r,float radius,Color c,float width,DashStyle dash=DashStyleSolid) {
    Pen pen(c,width);pen.SetDashStyle(dash);
    auto p=ui::round_path(RectF(r.X+width/2,r.Y+width/2,r.Width-width,r.Height-width),std::max(0.f,radius-width/2));g.DrawPath(&pen,p.get());
}
void glow(Graphics& g,RectF r,float radius,Color c,int steps=3) {
    for(int i=steps;i>=1;i--){Pen p(Color(BYTE(c.GetA()*(steps+1-i)/(steps+1)),c.GetR(),c.GetG(),c.GetB()),float(i)*2.5f);
        auto path=ui::round_path(RectF(r.X-i,r.Y-i,r.Width+2*i,r.Height+2*i),radius+i);g.DrawPath(&p,path.get());}
}
// An item's art, fitted inside `r` keeping its proportions: the HD icon, else the game's own, else its name.
void icon(Ctx& c,int id,RectF r,float alpha=1) {
    Bitmap* image=cv_item_icons_hd::get(id);
    if(!image)image=images.get(c.game,id);
    if(!image){auto f=F(c,Role::BODY500,12);ui::text(c.g,name(c.v,id),r,*f,col::MUTED,StringAlignmentCenter,StringAlignmentCenter,true);return;}
    const float iw=float(image->GetWidth()),ih=float(image->GetHeight());
    const float s=std::min(r.Width/iw,r.Height/ih),w=iw*s,h=ih*s;
    const RectF to(r.X+(r.Width-w)/2,r.Y+(r.Height-h)/2,w,h);
    const auto state=c.g.Save();c.g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
    ColorMatrix shadow={0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0, 0,0,0,alpha*0.45f,0, 0,0,0,0,1};   // the soft drop shadow
    ImageAttributes sa;sa.SetColorMatrix(&shadow);
    c.g.DrawImage(image,RectF(to.X,to.Y+3,to.Width,to.Height),0,0,iw,ih,UnitPixel,&sa);
    ColorMatrix m={1,0,0,0,0, 0,1,0,0,0, 0,0,1,0,0, 0,0,0,alpha,0, 0,0,0,0,1};
    ImageAttributes a;a.SetColorMatrix(&m);
    c.g.DrawImage(image,to,0,0,iw,ih,UnitPixel,&a);
    c.g.Restore(state);
}
// Text with a black outline, white inside (the quantity on a card).
void outlined(Ctx& c,const std::wstring& s,float x,RectF band,const Font& f,Color fill) {
    // Drawn as text (GraphicsPath::AddString crashed on the embedded private-collection fonts): a black ring of copies, then the fill.
    StringFormat sf(StringFormat::GenericTypographic());sf.SetFormatFlags(StringFormatFlagsNoWrap);
    const float lh=f.GetHeight(&c.g),y=band.Y+(band.Height-lh)/2;
    SolidBrush black(Color(255,0,0,0)),white(fill);
    for(int i=0;i<16;i++){const float a=float(i)*0.3927f,dx=std::cos(a)*2.f,dy=std::sin(a)*2.f;c.g.DrawString(s.c_str(),-1,&f,PointF(x+dx,y+dy),&sf,&black);}
    c.g.DrawString(s.c_str(),-1,&f,PointF(x,y),&sf,&white);
}
// ---- Tab icons: vectors (docs/mockup-inventario/gen.py TABICON, pistol.svg.txt) ----------------------------
void tab_icon(Graphics& g,int i,float x,float y,Color c) {
    const auto state=g.Save();g.SetSmoothingMode(SmoothingModeAntiAlias);
    SolidBrush b(c);
    g.TranslateTransform(x,y);
    if(i==0) {   // the 1911 pistol, 64x40 view box drawn 30x19
        g.ScaleTransform(30.f/64,19.f/40);
        GraphicsPath p(FillModeAlternate);
        p.StartFigure();
        const PointF a[]={{6.5f,6},{7.6f,3.8f},{12.6f,3.8f},{13.4f,6},{50,6},{51,3.8f},{54.4f,3.8f},{55.2f,6},{58,6},{58,7.5f},{61,7.5f},{61,12.7f},{58,12.7f},{58,14.6f},{46,14.6f},{46,18.5f},{40.6f,18.5f}};
        p.AddLines(a,17);p.AddBezier(PointF(40.6f,18.5f),PointF(41,25),PointF(38.4f,28),PointF(32,28));
        const PointF b2[]={{32,28},{22.6f,28},{20,37.6f},{4,36.2f},{9.4f,16.2f},{8.4f,14.4f},{6,14.2f},{1.8f,11.2f},{2.4f,8.2f},{6,9.4f}};
        p.AddLines(b2,10);p.CloseFigure();
        for(float rx:{9.5f,12.7f,15.9f,19.1f}){p.StartFigure();p.AddRectangle(RectF(rx,7.6f,1.5f,5.f));p.CloseFigure();}
        p.StartFigure();p.AddRectangle(RectF(30,8,9.5f,3.4f));p.CloseFigure();
        p.StartFigure();p.AddRectangle(RectF(24,16.1f,7.5f,1));p.CloseFigure();
        p.StartFigure();p.AddLine(26.8f,21.f,38.2f,21.f);p.AddBezier(PointF(38.2f,21),PointF(38.2f,24.4f),PointF(36.6f,26.2f),PointF(33,26.2f));
        p.AddLine(33.f,26.2f,28.4f,26.2f);p.AddBezier(PointF(28.4f,26.2f),PointF(27.2f,26.2f),PointF(26.8f,25.6f),PointF(26.8f,24.4f));p.CloseFigure();
        {const PointF q[]={{31,21.2f},{33.6f,21.2f},{32,25.4f},{30,25.4f}};p.StartFigure();p.AddPolygon(q,4);}
        {const PointF q[]={{10.5f,22},{22,22},{18.85f,33.5f},{7.3f,33.5f}};p.StartFigure();p.AddPolygon(q,4);}
        {const PointF q[]={{11.3f,23.1f},{20.9f,23.1f},{17.9f,32.4f},{8.6f,32.4f}};p.StartFigure();p.AddPolygon(q,4);}
        g.FillPath(&b,&p);
    } else {     // 16x16 view box drawn 19x19
        g.ScaleTransform(19.f/16,19.f/16);
        if(i==1) {
            GraphicsPath p;p.AddLine(8.f,0.8f,8.f,0.8f);
            p.AddBezier(PointF(8,0.8f),PointF(10,2.4f),PointF(11,4.2f),PointF(11,6.4f));p.AddLine(11.f,6.4f,11.f,8.7f);p.AddLine(11.f,8.7f,5.f,8.7f);p.AddLine(5.f,8.7f,5.f,6.4f);
            p.AddBezier(PointF(5,6.4f),PointF(5,4.2f),PointF(6,2.4f),PointF(8,0.8f));p.CloseFigure();g.FillPath(&b,&p);
            g.FillRectangle(&b,RectF(4.6f,9.6f,6.8f,5.4f));
            SolidBrush dark(Color(102,0,0,0));g.FillRectangle(&dark,RectF(4.6f,11,6.8f,0.9f));
        } else if(i==2) {
            const PointF q[]={{6,2},{10,2},{10,6},{14,6},{14,10},{10,10},{10,14},{6,14},{6,10},{2,10},{2,6},{6,6}};g.FillPolygon(&b,q,12);
        } else if(i==3) {
            Pen p(c,2.f);g.DrawEllipse(&p,RectF(1.8f,4.8f,6.4f,6.4f));
            g.DrawLine(&p,8.f,8.f,15.f,8.f);g.DrawLine(&p,12.f,8.f,12.f,11.f);g.DrawLine(&p,14.5f,8.f,14.5f,10.5f);
        } else {
            Pen p(c,1.6f);const PointF d[]={{3,1.5f},{10,1.5f},{13,4.5f},{13,14.5f},{3,14.5f}};g.DrawPolygon(&p,d,5);
            g.DrawLine(&p,5.5f,8.f,10.5f,8.f);g.DrawLine(&p,5.5f,11.f,10.5f,11.f);
        }
    }
    g.Restore(state);
}
void lock_icon(Graphics& g,float x,float y) {   // 18x20 view box drawn 21x23
    const auto state=g.Save();g.SetSmoothingMode(SmoothingModeAntiAlias);g.TranslateTransform(x,y);g.ScaleTransform(21.f/18,23.f/20);
    SolidBrush body(Color(255,138,131,114));auto r=ui::round_path(RectF(2.5f,8.5f,13,10),1.5f);g.FillPath(&body,r.get());
    Pen p(Color(255,138,131,114),2.2f);g.DrawArc(&p,RectF(5,2,8,8),180.f,180.f);g.DrawLine(&p,5.f,6.f,5.f,8.5f);g.DrawLine(&p,13.f,6.f,13.f,8.5f);
    SolidBrush hole(Color(255,23,27,40));g.FillEllipse(&hole,RectF(7.5f,11.7f,3,3));
    g.Restore(state);
}
// ---- Cards of the right column ---------------------------------------------------------------------------
struct CardOpts {bool equipped=false,ghost=false,picked=false,fixed=false;float icon_alpha=1;ButtonState st=ButtonState::NORMAL;};
void card(Ctx& c,RectF r,int id,int quantity,bool counted,bool percent,const CardOpts& o) {
    Graphics& g=c.g;const auto state=g.Save();g.SetSmoothingMode(SmoothingModeAntiAlias);
    const bool wide_card=r.Width>300;
    const bool pressed=o.st==ButtonState::PRESSED;
    const RectF body=pressed?RectF(r.X+1,r.Y+1,r.Width,r.Height):r;
    const bool gold=o.equipped||o.ghost;
    if(gold)glow(g,body,2,Color(90,255,216,106));
    else if(o.st==ButtonState::HOVER)glow(g,body,2,Color(70,240,200,100),2);
    fill_v(g,body,shade(o.fixed?Color(255,27,35,56):Color(255,20,44,102),o.st),shade(o.fixed?Color(255,15,20,36):Color(255,13,32,80),o.st),2);
    if(gold){SolidBrush tint(Color(o.ghost?44:30,255,216,106));g.FillRectangle(&tint,RectF(body.X+2,body.Y+2,body.Width-4,body.Height-4));}
    {SolidBrush hi(Color(41,160,190,255));g.FillRectangle(&hi,body.X+1,body.Y+1,body.Width-2,1.f);}
    if(o.picked)stroke(g,body,2,col::BAD,2);
    else if(o.ghost)stroke(g,body,2,col::SEL,2);
    else if(o.equipped)stroke(g,body,2,Color(255,227,184,78),1.5f);
    else stroke(g,body,2,o.fixed?col::GOLD_LO:o.st==ButtonState::HOVER?Color(255,120,160,230):col::BLUE_LINE,1);
    const float iw=o.fixed?110:wide_card?170:92;
    const RectF ii(body.X+4,body.Y+4,iw,body.Height-8);
    {   // the soft light behind the art
        GraphicsPath p;p.AddEllipse(RectF(ii.X-ii.Width*0.18f,ii.Y-ii.Height*0.2f,ii.Width*1.36f,ii.Height*1.4f));PathGradientBrush pb(&p);
        pb.SetCenterColor(o.fixed?Color(90,120,130,170):Color(140,46,100,205));const Color sur[1]={o.fixed?Color(0,22,28,48):Color(0,16,40,94)};INT n=1;pb.SetSurroundColors(sur,&n);
        const auto clip=g.Save();g.SetClip(ii,CombineModeIntersect);g.FillPath(&pb,&p);g.Restore(clip);
    }
    icon(c,id,RectF(ii.X+6,ii.Y+3,ii.Width-12,ii.Height-6),o.icon_alpha);
    const float tx0=body.X+iw+10,right=body.X+body.Width-9;
    const bool wide_item=cv_items::wide(id);
    if(counted&&!o.fixed) {
        auto f=F(c,Role::BODY700,21.5f);
        outlined(c,std::to_wstring(quantity)+(percent?L"%":L""),body.X+9,RectF(body.X,body.Y+body.Height-27,80,24),*f,Color(255,255,255,255));
    }
    float name_top=13;
    if(o.fixed)name_top=34;
    else if(o.equipped)name_top=wide_item?26:31;
    {
        const std::wstring n=name(c.v,id);const float max_w=std::max(40.f,right-tx0);
        auto f=fit(c,Role::BODY600,17,n,max_w,0.2f,11);
        ui::text(g,n,RectF(tx0-14,body.Y+name_top,max_w+14,21),*f,col::TXT,StringAlignmentFar,StringAlignmentCenter,false,0.2f);   // the rect is wider than the text: DrawString pads it
    }
    if(o.fixed) {   // "● Fixo"
        auto f=F(c,Role::BODY700,12);const std::wstring s=t(c.v,tx::FIXED);
        const float w=ui::text_width(g,s,*f,1)+14+11,x=right-w;
        const RectF b(x,body.Y+6,w,19);
        {SolidBrush bg(Color(255,18,13,4));auto p=ui::round_path(b,2);g.FillPath(&bg,p.get());}
        stroke(g,b,2,col::GOLD_LO,1);
        SolidBrush dot(Color(255,227,184,78));g.FillEllipse(&dot,RectF(x+7,b.Y+b.Height/2-3,6,6));
        ui::text(g,s,RectF(x+18,b.Y,w-18,b.Height),*f,Color(255,227,184,78),StringAlignmentNear,StringAlignmentCenter,false,1);
    }
    if(o.equipped) {
        auto f=F(c,Role::BODY700,12);const std::wstring s=t(c.v,tx::EQUIPPED_BADGE);
        const float w=ui::text_width(g,s,*f,1)+14;
        ui::badge_gold(g,RectF(body.X+body.Width-6-w,body.Y+5,w,18),s,c.v.language,12,1);
        PointF tri[3]={PointF(body.X,body.Y),PointF(body.X+10,body.Y),PointF(body.X,body.Y+10)};SolidBrush corner(col::SEL);g.FillPolygon(&corner,tri,3);
    }
    if(wide_item&&!o.fixed) {
        auto f=F(c,Role::BODY700,13);const std::wstring s=t(c.v,tx::TWO_SLOTS);
        const float w=ui::text_width(g,s,*f,0.3f)+14;
        ui::badge_gold(g,RectF(body.X+body.Width-9-w,body.Y+(o.equipped?47.f:38.f),w,18),s,c.v.language,13,0.3f);
    }
    if(o.picked) {   // to be removed: a red cross
        Pen x(col::BAD,2.f);x.SetStartCap(LineCapRound);x.SetEndCap(LineCapRound);
        const float cx=body.X+body.Width-14,cy=body.Y+14;g.DrawLine(&x,cx-5,cy-5,cx+5,cy+5);g.DrawLine(&x,cx-5,cy+5,cx+5,cy-5);
    }
    g.Restore(state);
}
void empty_card(Ctx& c,RectF r,ButtonState st) {
    Graphics& g=c.g;const auto state=g.Save();g.SetSmoothingMode(SmoothingModeAntiAlias);
    const bool hot=st==ButtonState::HOVER||st==ButtonState::PRESSED;
    SolidBrush bg(hot?Color(170,22,40,86):Color(140,8,16,40));auto p=ui::round_path(r,2);g.FillPath(&bg,p.get());
    stroke(g,r,2,hot?Color(255,227,184,78):col::BLUE_LINE,1,DashStyleDash);
    auto f=F(c,Role::BODY500,17);ui::text(g,t(c.v,tx::EMPTY),r,*f,hot?col::MUTED:Color(255,109,130,184),StringAlignmentCenter,StringAlignmentCenter,false,1);
    g.Restore(state);
}
void locked_card(Ctx& c,RectF r) {
    Graphics& g=c.g;const auto state=g.Save();g.SetSmoothingMode(SmoothingModeAntiAlias);
    SolidBrush base(Color(255,14,18,29));g.FillRectangle(&base,r);
    g.SetClip(r,CombineModeIntersect);
    Pen stripe(Color(255,23,27,40),6.f*1.4142f);   // 6px stripes along 135 degrees, 12px period
    for(float k=-r.Height;k<r.Width+r.Height;k+=12*1.4142f){g.DrawLine(&stripe,r.X+k,r.Y+r.Height+9,r.X+k+r.Height+18,r.Y-9);}
    g.ResetClip();
    SolidBrush shade_(Color(70,0,0,0));g.FillRectangle(&shade_,RectF(r.X+1,r.Y+1,r.Width-2,5));
    stroke(g,r,2,Color(255,74,64,48),1);
    auto f=fit(c,Role::BODY600,17,t(c.v,tx::LOCKED),r.Width-60,0.3f,11);
    const float tw=ui::text_width(g,t(c.v,tx::LOCKED),*f,0.3f),total=21+10+tw,x=r.X+(r.Width-total)/2;
    lock_icon(g,x,r.Y+(r.Height-23)/2);
    ui::text(g,t(c.v,tx::LOCKED),RectF(x+31,r.Y,tw+8,r.Height),*f,Color(255,154,146,127),StringAlignmentNear,StringAlignmentCenter,false,0.3f);
    g.Restore(state);
}
void stepper(Ctx& c,int index,float y,const wchar_t* label,bool bullet,const std::wstring& value,bool enabled,bool focused,bool compact,int minus,int plus) {
    Graphics& g=c.g;View& v=c.v;const auto state=g.Save();g.SetSmoothingMode(SmoothingModeAntiAlias);
    const RectF r(RX,y,RW,STEP_H);
    if(focused)glow(g,r,3,Color(70,255,216,106));
    fill_v(g,r,Color(255,28,34,50),Color(255,13,17,28),3);
    stroke(g,r,3,focused?col::SEL:col::GOLD_LO,focused?1.5f:1);
    {SolidBrush hi(Color(15,255,255,255));g.FillRectangle(&hi,r.X+1,r.Y+1,r.Width-2,1.f);}
    {   // the label (a bullet before the ammunition ones), shrunk to fit before the left arrow
        const Color ink=enabled?C_LABEL:Color(255,90,96,112);
        float x=r.X+17;
        if(bullet){tab_icon(g,1,x,y+(STEP_H-19)/2,enabled?col::GOLD_HI:ink);x+=19+6;}
        const float max_w=1105-4-x;
        const Font* lf=fit(c,Role::BODY500,18,label,max_w,0,10.5f);
        ui::text(g,label,RectF(x,r.Y,max_w+4,r.Height),*lf,ink,StringAlignmentNear,StringAlignmentCenter);
    }
    const RectF left(1105,y+5,28,28),right(1341,y+5,28,28),box(1139,y+5,196,28);
    ui::stepper_arrow(g,left,true,enabled?v.mouse.state(hit_id(minus,0)):ButtonState::DISABLED);
    ui::stepper_arrow(g,right,false,enabled?v.mouse.state(hit_id(plus,0)):ButtonState::DISABLED);
    {SolidBrush bg(Color(255,7,11,22));auto p=ui::round_path(box,2);g.FillPath(&bg,p.get());stroke(g,box,2,Color(255,58,68,102),1);}
    auto vf=fit(c,compact?Role::BODY600:Role::BODY700,compact?18.f:19.f,value,box.Width-16,0.3f,11);
    ui::text(g,value,box,*vf,enabled?Color(255,255,255,255):Color(255,100,106,122),StringAlignmentCenter,StringAlignmentCenter,false,0.3f);
    if(enabled) {
        v.hits.push_back({RectF(1101,y,36,STEP_H),minus,0});
        v.hits.push_back({RectF(1337,y,40,STEP_H),plus,0});
    }
    (void)index;
    g.Restore(state);
}
std::vector<ui::FooterGroup> footer_groups(const View& v,int kind) {
    using ui::Glyph;using ui::GlyphKind;namespace pt=cv_port_ui_i18n;
    const int l=v.language;
    auto kb=[](int vk){return Glyph{GlyphKind::KEY,vk,L""};};
    auto pad=[](int pb){return Glyph{GlyphKind::PAD,pb,L""};};
    std::vector<ui::FooterGroup> out={
        {{Glyph{GlyphKind::ARROWS,15,L""}},{Glyph{GlyphKind::DPAD,0,L""}},pt::tr(l,pt::NAVIGATE)},
        {{kb('Q'),kb('E')},{pad(PB_L1),pad(PB_R1)},pt::tr(l,pt::CATEGORY)},
        {{kb(VK_RETURN)},{pad(PB_SOUTH)},pt::tr(l,pt::PUT)},
        {{kb('R')},{pad(PB_WEST)},pt::tr(l,pt::REMOVE)},
        {{kb(VK_ESCAPE)},{pad(PB_EAST)},pt::tr(l,pt::BACK)},
        {{kb(VK_F6)},{pad(PB_START)},pt::tr(l,pt::CLOSE)},
    };
    // A PlayStation pad has no Close button of its own here (Start is for the game's chords): only the keyboard, Xbox and Switch show one.
    if(kind==PAD_KIND_DS4||kind==PAD_KIND_DUALSENSE)out.pop_back();
    return out;
}
// The current inventory as the menu will show it once the item chosen goes in (or an empty/ghost cell).
struct Ghost {cv_items::Inventory inventory;int entry=0;};
Ghost with_ghost(const View& v,int id,bool panel_open) {
    Ghost out;out.inventory=v.inventory;
    if(!panel_open||!id||!v.inventory.valid)return out;
    cv_items::Result why;const int entry=cv_items::place(v.inventory,id,v.target,why);
    if(entry<=0)return out;
    out.inventory.entry[entry]=cv_items::encode(id,v.quantity);out.entry=entry;
    return out;
}
}  // namespace

void release() {images.cache.clear();portraits.clear();cv_item_icons_hd::release();cv_port_ui::release_fonts();}
int View::item() const {
    const auto list=cv_items::items(cv_items::Category(category));
    return selected>=0&&selected<int(list.size())?list[size_t(selected)]:0;
}
std::vector<int> targets(const View& view) {
    std::vector<int> out;const auto& inv=view.inventory;const int id=view.item();
    if(fits_free(inv,id))out.push_back(0);
    for(int i=1;i<=inv.capacity;i++)if(!inv.empty(i)&&!inv.is_equipped(i)&&fits_replace(inv,id,i))out.push_back(i);
    return out;
}
const Hit* hit_at(const View& view,float x,float y) {
    for(const auto& hit:view.hits)if(hit.rect.Contains(x,y))return &hit;
    return nullptr;
}
namespace {
void fix_target(View& v) {
    const auto list=targets(v);
    if(list.empty()){v.target=0;return;}
    if(std::find(list.begin(),list.end(),v.target)==list.end())v.target=list.front();
}
void open_panel(View& v) {
    const auto* info=cv_items::info(v.item());if(!info)return;
    v.focus=1;v.quantity=info->quantity;v.row=info->counted?0:1;
    v.target=fits_free(v.inventory,v.item())?0:-1;fix_target(v);
}
}
void refresh(View& view,const cv_items::Inventory& live) {
    const bool changed=live.character!=view.inventory.character;
    view.inventory=live;cv_items::apply_pending(view.inventory);   // as it will be once the menu closes
    if(changed&&view.focus!=0)view.focus=0;
    fix_target(view);
}
namespace {
// Removing: the occupied entries, but not the equipped weapon.
bool removable(const cv_items::Inventory& inv,int e) {return e>=1&&e<=inv.capacity&&!inv.empty(e)&&!inv.is_equipped(e);}
void open_remove(View& v) {
    v.focus=2;v.status=t(v,tx::REMOVE_PICK);v.tone=NEUTRAL;
    if(removable(v.inventory,v.slot))return;
    for(int e=1;e<=v.inventory.capacity;e++)if(removable(v.inventory,e)){v.slot=e;return;}
    v.slot=1;
}
// The next occupied cell of the two-column layout in a direction (wide weapons take a whole row).
int move_slot(const cv_items::Inventory& inv,int slot,Action a) {
    const auto cells=cv_items::layout(inv);
    const cv_items::Cell* now=nullptr;
    for(const auto& c:cells)if(c.kind==cv_items::Cell::ITEM&&c.entry==slot)now=&c;
    if(!now){for(const auto& c:cells)if(c.kind==cv_items::Cell::ITEM)return c.entry;return slot;}
    const float cx=now->col+now->span/2.f;
    const cv_items::Cell* best=nullptr;float best_score=1e9f;
    for(const auto& c:cells) {
        if(c.kind!=cv_items::Cell::ITEM||&c==now)continue;
        const float x=c.col+c.span/2.f;float score;
        if(a==LEFT||a==RIGHT) {if(c.row!=now->row||(a==LEFT?x>=cx:x<=cx))continue;score=std::fabs(x-cx);}
        else {
            const int dr=a==UP?now->row-c.row:c.row-now->row;if(dr<=0)continue;
            score=dr*10+std::fabs(x-cx);
        }
        if(score<best_score){best_score=score;best=&c;}
    }
    return best?best->entry:slot;
}
}
bool apply(View& v,Action a) {
    const auto list=cv_items::items(cv_items::Category(v.category));
    const int n=int(list.size());
    if(a==REMOVE) {if(v.focus==2)v.focus=0;else open_remove(v);return false;}
    if(v.focus==2) {   // an item of the inventory, by the cards' own layout
        switch(a) {
        case LEFT:case RIGHT:case UP:case DOWN:v.slot=move_slot(v.inventory,v.slot,a);break;
        case CONFIRM:submit(v);break;
        case BACK:v.focus=0;v.status.clear();break;
        default:break;
        }
        return false;
    }
    if(v.focus==0) {
        switch(a) {
        case LEFT:if(v.selected%COLS)v.selected--;break;
        case RIGHT:if(v.selected%COLS<COLS-1&&v.selected+1<n)v.selected++;break;
        case UP:if(v.selected>=COLS)v.selected-=COLS;break;
        case DOWN:if(v.selected+COLS<n)v.selected+=COLS;else if(v.selected/COLS<(n-1)/COLS)v.selected=n-1;break;
        case PREV:case NEXT:
            v.category=(v.category+(a==NEXT?1:cv_items::CATEGORY_COUNT-1))%cv_items::CATEGORY_COUNT;
            v.selected=0;v.scroll=0;break;
        case CONFIRM:open_panel(v);break;
        case BACK:return true;
        default:break;
        }
        const int row=v.selected/COLS;
        if(row<v.scroll)v.scroll=row;if(row>=v.scroll+ROWS)v.scroll=row-ROWS+1;
        return false;
    }
    const auto* info=cv_items::info(v.item());
    if(!info){v.focus=0;return false;}
    const int first_row=info->counted?0:1;
    switch(a) {
    case UP:v.row=std::max(first_row,v.row-1);break;
    case DOWN:v.row=std::min(2,v.row+1);break;
    case LEFT:case RIGHT:case PREV:case NEXT: {
        const int sign=(a==LEFT||a==PREV)?-1:1;
        if(v.row==0)v.quantity=cv_items::clamp_quantity(info->id,v.quantity+sign*((a==PREV||a==NEXT)?10:1));
        else if(v.row==1) {
            const auto list=targets(v);if(list.empty())break;
            auto at=std::find(list.begin(),list.end(),v.target);
            const int i=at==list.end()?0:int(at-list.begin());
            v.target=list[size_t((i+sign+int(list.size()))%int(list.size()))];
        }
        break;}
    case CONFIRM:submit(v);break;
    case BACK:v.focus=0;break;
    default:break;
    }
    return false;
}
cv_items::Result submit(View& v) {
    if(v.focus==2) {
        const auto& inv=v.inventory;
        if(!inv.valid||!inv.can_remove){cv_items::report_refusal("removal");v.status=t(v,tx::NOT_PLAYING);v.tone=BAD;return cv_items::R_NOT_PLAYING;}
        if(inv.is_equipped(v.slot)){v.status=t(v,tx::NO_REMOVE_EQUIPPED);v.tone=BAD;return cv_items::R_EQUIPPED;}
        if(!removable(inv,v.slot)){v.status=t(v,tx::REMOVE_PICK);v.tone=WARN;return cv_items::R_BAD;}
        const int gone=inv.id(v.slot);
        const auto r=cv_items::request_remove(inv.character,v.slot,inv.entry[v.slot]);
        if(r==cv_items::R_QUEUED) {
            const std::wstring said=format(t(v,tx::REMOVE_QUEUED_FMT),v.slot,name(v,gone));
            refresh(v,cv_items::read());   // the entries below move up at once
            if(!removable(v.inventory,v.slot))open_remove(v);   // the list's end: another item, if any
            v.status=said;v.tone=GOOD;
        } else if(r==cv_items::R_BUSY){v.status=t(v,tx::QUEUE_FULL);v.tone=WARN;}
        else {v.status=t(v,tx::NOT_PLAYING);v.tone=BAD;}
        return r;
    }
    const int id=v.item();const auto& inv=v.inventory;
    cv_items::Result why;
    const int entry=v.target<0?-1:cv_items::place(inv,id,v.target,why);
    if(entry<0) {
        if(v.target<0)why=cv_items::R_FULL;
        if(why==cv_items::R_NOT_PLAYING)cv_items::report_refusal("item");
        v.status=t(v,why==cv_items::R_FULL?tx::FULL:why==cv_items::R_EQUIPPED?tx::NO_EQUIPPED:why==cv_items::R_NO_ROOM_WIDE?tx::NO_TWO_SLOTS:tx::NOT_PLAYING);
        v.tone=BAD;return why;
    }
    const auto r=cv_items::request(inv.character,id,v.quantity,entry,inv.entry[entry]);
    if(r==cv_items::R_QUEUED) {
        v.status=format(t(v,tx::QUEUED_FMT),entry,name(v,id));v.tone=GOOD;v.focus=0;
        v.inventory.entry[entry]=cv_items::encode(id,v.quantity);fix_target(v);
    } else if(r==cv_items::R_BUSY){v.status=t(v,tx::QUEUE_FULL);v.tone=WARN;}
    else {v.status=t(v,tx::NOT_PLAYING);v.tone=BAD;}
    return r;
}
std::wstring summary(int language,const std::vector<cv_items::Outcome>& done,bool& ok) {
    View v;v.language=language;ok=true;int placed=0;const cv_items::Outcome* last=nullptr;
    for(const auto& o:done) {
        if(o.result==cv_items::R_DONE){placed++;last=&o;continue;}
        ok=false;
    }
    if(!ok) {
        for(const auto& o:done)if(o.result==cv_items::R_CHANGED||o.result==cv_items::R_EQUIPPED)return t(v,tx::CHANGED);
        for(const auto& o:done)if(o.result==cv_items::R_NO_ROOM_WIDE)return t(v,tx::NO_TWO_SLOTS);
        for(const auto& o:done)if(o.result==cv_items::R_FULL)return t(v,tx::FULL);
        return t(v,tx::NOT_PLAYING);
    }
    if(placed==1)return format(t(v,last->removed?tx::REMOVED_FMT:tx::DONE_FMT),last->entry,name(v,last->id));
    for(const auto& o:done)if(o.removed)return format(t(v,tx::CHANGES_N_FMT),placed);
    return format(t(v,tx::ADDED_N_FMT),placed);
}
void poll_result(View& v) {
    // Outcomes come while the game runs: the menu shows them if still open.
    const auto done=cv_items::outcomes(v.serial);
    if(done.empty())return;
    bool ok;v.status=summary(v.language,done,ok);v.tone=ok?GOOD:WARN;
}

void draw(Graphics& g,View& v,const std::wstring& game) {
    v.hits.clear();
    Ctx c{g,v,game};
    g.SetSmoothingMode(SmoothingModeAntiAlias);g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
    g.SetInterpolationMode(InterpolationModeHighQualityBicubic);g.SetPixelOffsetMode(PixelOffsetModeHalf);
    const auto& inv=v.inventory;
    const int kind=ui::hint_kind(v.input);
    ui::gold_frame(g,RectF(0,0,W,H));
    ui::header_band(g,RectF(3,3,W-6,65));

    // ---- Header: the title and "Game paused" on the left; the character, the spaces and the close button on the right.
    {
        const std::wstring title=t(v,tx::TITLE);
        auto tf=&ui::font_cached(v.language,Role::TITLE,35);const float tw=ui::text_width(g,title,*tf,2);
        ui::title_gold(g,title,RectF(24,0,tw+40,68),v.language,35,2);
        float x=24+tw+22;const float cy=37.5f;
        ui::diamond(g,x+2.5f,cy,5);x+=5+2+9;
        SolidBrush bar(col::MUTED);
        for(int i=0;i<2;i++){auto p=ui::round_path(RectF(x+i*6.5f,cy-6.5f,3.5f,13),1);g.FillPath(&bar,p.get());}
        x+=10+9;
        auto pf=&ui::font_cached(v.language,Role::BODY500,17);
        ui::text(g,t(v,tx::PAUSED),RectF(x,cy-14,260,28),*pf,col::MUTED,StringAlignmentNear,StringAlignmentCenter,false,0.3f);
    }
    {
        const RectF close(1334,14,40,40);
        ui::close_button(g,close,v.mouse.state(hit_id(HIT_CLOSE,0)));
        v.hits.push_back({close,HIT_CLOSE,0});
        const float right=1310;
        if(inv.valid) {
            const std::wstring who=character_name(inv.character);
            auto nf=&ui::font_cached(v.language,Role::TITLE_SEMI,21);
            wchar_t spaces[96];swprintf(spaces,96,t(v,tx::SPACES_FMT),inv.spaces_used(),inv.capacity);
            auto sf=&ui::font_cached(v.language,Role::BODY500,16.5f);
            const float nw=ui::text_width(g,who,*nf,1),sw=ui::text_width(g,spaces,*sf,0);
            const float pw=std::max(nw,sw),px=right-pw-16-54;
            // The portrait: a gold gradient ring around the square art.
            const RectF pr(px,7,54,54);
            {LinearGradientBrush ring(PointF(pr.X,pr.Y),PointF(pr.X+pr.Width,pr.Y+pr.Height),col::GOLD_HI,col::GOLD_LO);
             auto p=ui::round_path(pr,3);g.FillPath(&ring,p.get());
             SolidBrush bg(Color(255,10,16,32));auto q=ui::round_path(RectF(pr.X+2,pr.Y+2,50,50),2);g.FillPath(&bg,q.get());}
            if(Bitmap* face=portrait(inv.character))g.DrawImage(face,RectF(pr.X+2,pr.Y+2,50,50));
            ui::text(g,who,RectF(right-pw-4,10,pw+8,28),*nf,col::IVORY,StringAlignmentFar,StringAlignmentCenter,false,1);
            // "6 of 8 spaces": the count in gold.
            const std::wstring digits=std::to_wstring(inv.spaces_used());const std::wstring all=spaces;
            const size_t at=all.find(digits);
            if(at==std::wstring::npos||cv_srt_i18n::rtl_text(all)||!ui::native_fonts(v.language))   // (the gold digit needs the embedded font's metrics)
                ui::text(g,all,RectF(right-pw-8,38,pw+16,20),*sf,col::MUTED,StringAlignmentFar,StringAlignmentCenter);
            else {
                const std::wstring a=all.substr(0,at),d=digits,b=all.substr(at+digits.size());
                float x=right-sw;const float wa=ui::text_width(g,a,*sf),wd=ui::text_width(g,d,*sf);
                ui::text(g,a,RectF(x,38,wa+14,20),*sf,col::MUTED,StringAlignmentNear,StringAlignmentCenter);x+=wa;
                ui::text(g,d,RectF(x,38,wd+14,20),*sf,col::GOLD_HI,StringAlignmentNear,StringAlignmentCenter);x+=wd;
                ui::text(g,b,RectF(x,38,sw+14,20),*sf,col::MUTED,StringAlignmentNear,StringAlignmentCenter);
            }
        } else {
            auto wf=&ui::font_cached(v.language,Role::BODY500,17);
            ui::text(g,t(v,tx::WAITING),RectF(right-400,14,400,40),*wf,col::MUTED,StringAlignmentFar,StringAlignmentCenter);
        }
    }

    // ---- Category tabs, with the Q/E (L1/R1, LB/RB) glyphs at their sides.
    const tx::Id cats[]={tx::WEAPONS,tx::TAB_AMMO,tx::HEALING,tx::KEY_ITEMS,tx::TAB_OTHER};
    for(int i=0;i<5;i++) {
        const RectF r(70+i*169.2f,80,159,44);const bool on=v.category==i;
        const ButtonState st=v.mouse.state(hit_id(HIT_TAB,i));
        ui::plate(g,r,on,st);
        const RectF cr=st==ButtonState::PRESSED?RectF(r.X+1,r.Y+1,r.Width,r.Height):r;
        const float iw=i==0?30.f:19.f;
        const std::wstring label=t(v,cats[i]);
        auto f=fit(c,Role::BODY600,18.5f,label,cr.Width-iw-8-16,0.3f,12);
        const float tw=ui::text_width(g,label,*f,0.3f),total=iw+8+tw,x=cr.X+(cr.Width-total)/2;
        const Color ink=on?col::INK:shade(Color(255,201,208,226),st==ButtonState::HOVER?st:ButtonState::NORMAL);
        tab_icon(g,i,x,cr.Y+(cr.Height-19)/2,ink);
        ui::text(g,label,RectF(x+iw+8,cr.Y,tw+10,cr.Height),*f,ink,StringAlignmentNear,StringAlignmentCenter,false,0.3f);
        v.hits.push_back({r,HIT_TAB,i});
    }
    {
        using ui::Glyph;using ui::GlyphKind;
        const bool kb=kind==PAD_INPUT_KEYBOARD;
        ui::draw_glyph(g,kb?Glyph{GlyphKind::KEY,'Q',L""}:Glyph{GlyphKind::PAD,PB_L1,L""},kind,24,88,28);
        ui::draw_glyph(g,kb?Glyph{GlyphKind::KEY,'E',L""}:Glyph{GlyphKind::PAD,PB_R1,L""},kind,922,88,28);
    }

    // ---- The catalog: 4x4 cells that scroll by rows with the selection.
    ui::panel_blue(g,RectF(CAT_X,CAT_Y,CAT_W,CAT_H));
    const auto list=cv_items::items(cv_items::Category(v.category));
    const int rows=(int(list.size())+COLS-1)/COLS;
    for(int i=v.scroll*COLS;i<int(list.size())&&i<(v.scroll+ROWS)*COLS;i++) {
        const int cc=i%COLS,rr=i/COLS-v.scroll;
        const RectF cell(GRID_X+cc*(CELL_W+GAP),GRID_Y+rr*(CELL_H+GAP),CELL_W,CELL_H);
        const bool on=i==v.selected;const int id=list[size_t(i)];
        ui::cell_blue(g,cell,on,v.mouse.state(hit_id(HIT_ITEM,i)));
        icon(c,id,RectF(cell.X+12,cell.Y+8,196,62));
        const std::wstring n=name(v,id);
        auto f=fit(c,Role::BODY600,15.5f,n,cell.Width-12,0.2f,10.5f);
        ui::text(g,n,RectF(cell.X+4,cell.Y+76,cell.Width-8,22),*f,on?Color(255,255,243,200):col::TXT,StringAlignmentCenter,StringAlignmentCenter,false,0.2f);
        if(cv_items::wide(id)) {
            auto bf=F(c,Role::BODY700,12);const std::wstring s=t(v,tx::TWO_SLOTS);
            const float w=ui::text_width(g,s,*bf,0.3f)+12;
            ui::badge_gold(g,RectF(cell.X+cell.Width-(on?5.f:4.f)-w,cell.Y+(on?5.f:4.f),w,17),s,v.language,12,0.3f);
        }
        v.hits.push_back({cell,HIT_ITEM,i});
    }
    if(rows>ROWS) {   // where the visible rows are in the list
        const float top=GRID_Y,track=ROWS*(CELL_H+GAP)-GAP,bar=track*ROWS/rows;
        SolidBrush back(Color(120,16,40,94));g.FillRectangle(&back,RectF(CAT_X+CAT_W-9,top,4,track));
        SolidBrush fore(col::GOLD);g.FillRectangle(&fore,RectF(CAT_X+CAT_W-9,top+(track-bar)*v.scroll/float(rows-ROWS),4,bar));
    }

    // ---- The banner: the selected item, what it takes, and the state of the room for it.
    const int id=v.item();const auto* info=cv_items::info(id);
    const bool panel_open=v.focus==1&&info;
    const bool removing=v.focus==2;
    ui::panel_gold(g,RectF(BAN_X,BAN_Y,BAN_W,BAN_H));
    if(info) {
        const RectF pic(36,612,132,108);
        ui::cell_blue(g,pic,false);
        icon(c,id,RectF(pic.X+6,pic.Y+12,120,84));
        const float tx0=186,tw=BAN_X+BAN_W-2-14-tx0;
        const std::wstring n=name(v,id);
        auto nf=fit(c,Role::TITLE_SEMI,26,n,tw,0.8f,14);
        ui::text(g,n,RectF(tx0,610,tw,34),*nf,Color(255,255,255,255),StringAlignmentNear,StringAlignmentCenter,false,0.8f);
        const tx::Id kinds[]={tx::KIND_WEAPON,tx::KIND_AMMO,tx::KIND_HEALING,tx::KIND_KEY,tx::KIND_OTHER};
        const std::wstring s=std::wstring(t(v,kinds[info->category]))+L"  ·  "+t(v,cv_items::wide(id)?tx::TAKES_2:tx::TAKES_1);
        auto sf=fit(c,Role::BODY500,18,s,tw,0,12);
        ui::text(g,s,RectF(tx0,644,tw,23),*sf,col::MUTED,StringAlignmentNear,StringAlignmentCenter);
        const bool key_item=info->category==cv_items::KEY_ITEMS;
        const bool ammo_item=info->category==cv_items::AMMO;
        const std::wstring note=key_item?std::wstring(L"⚠  ")+t(v,tx::WARN_KEY):ammo_item?std::wstring(t(v,tx::QTY_HINT_AMMO)):std::wstring(t(v,tx::NOTE_SAVE));
        auto nof=fit(c,Role::BODY400,16.5f,note,tw,0,11);
        ui::text(g,note,RectF(tx0,670,tw,22),*nof,key_item?col::WARN:C_MUTED2,StringAlignmentNear,StringAlignmentCenter);
        // The state line: the last request's outcome, else what the room allows.
        std::wstring line;Color tone=col::MUTED;
        const Color tones[]={col::MUTED,col::OK,col::WARN,col::BAD};
        const bool fits=fits_free(inv,id);
        if(!v.status.empty()){line=v.status;tone=tones[v.tone];}
        else if(inv.valid&&!inv.writable){line=t(v,tx::NOT_PLAYING);tone=col::WARN;}
        else if(inv.valid&&!fits&&!panel_open){
            if(inv.first_empty()>0&&need_of(id)==2){line=t(v,tx::NO_TWO_SLOTS);tone=col::BAD;}
            else{line=t(v,tx::FULL);tone=col::WARN;}
        }
        else if(inv.valid&&fits){line=inv.free_spaces()==1?std::wstring(t(v,tx::FREE_1)):format(t(v,tx::FREE_N_FMT),inv.free_spaces());}
        else if(inv.valid&&panel_open&&v.target>0)line=format(t(v,tx::REPLACE_FMT),v.target);
        if(!line.empty()) {
            const float cy=708;
            for(int i=3;i>=1;i--){SolidBrush gl(Color(BYTE(24-i*6),tone.GetR(),tone.GetG(),tone.GetB()));g.FillEllipse(&gl,RectF(tx0+4-4-i*1.5f,cy-4-i*1.5f,8+i*3.f,8+i*3.f));}
            SolidBrush dot(tone);g.FillEllipse(&dot,RectF(tx0,cy-4,8,8));
            auto lf=fit(c,Role::BODY600,17,line,tw-16,0,11);
            ui::text(g,line,RectF(tx0+16,696,tw-16,24),*lf,tone,StringAlignmentNear,StringAlignmentCenter);
        }
    }
    // The tab's name and how many items it has, and the page of the grid.
    {
        auto f=F(c,Role::BODY500,15);
        const std::wstring head=upper(t(v,cats[v.category]));
        const float hw=ui::text_width(g,head,*f,0.5f);
        ui::text(g,head,RectF(24,742,hw+10,30),*f,C_MUTED3,StringAlignmentNear,StringAlignmentCenter,false,0.5f);
        ui::text(g,L"·",RectF(24+hw+5,742,10,30),*f,col::GOLD,StringAlignmentCenter,StringAlignmentCenter);
        ui::text(g,std::to_wstring(list.size()),RectF(24+hw+16,742,60,30),*f,C_MUTED3,StringAlignmentNear,StringAlignmentCenter,false,0.5f);
        const int pages=std::max(1,(rows+ROWS-1)/ROWS),page=std::clamp(v.selected/(COLS*ROWS),0,pages-1)+1;
        ui::text(g,std::to_wstring(page)+L" / "+std::to_wstring(pages),RectF(BAN_X+BAN_W-200,742,200,30),*f,C_MUTED3,StringAlignmentFar,StringAlignmentCenter,false,0.5f);
    }

    // ---- The right column: "current inventory", the fixed slot and the cards.
    {
        const RectF head(RX,80,RW,44);ui::plate(g,head,true);
        auto hf=&ui::font_cached(v.language,Role::TITLE,18.5f);
        ui::text(g,t(v,tx::CURRENT),RectF(RX+19.5f,80,RW-120,44),*hf,col::INK,StringAlignmentNear,StringAlignmentCenter,false,1.5f);
        if(inv.valid) {
            wchar_t count[32];swprintf(count,32,L"%d/%d",inv.spaces_used(),inv.capacity);
            auto cf=&ui::font_cached(v.language,Role::BODY700,22);const float w=ui::text_width(g,count,*cf,0.5f)+24;
            const RectF b(RX+RW-1.5f-8-w,87,w,30);
            SolidBrush bg(col::INK);auto p=ui::round_path(b,3);g.FillPath(&bg,p.get());
            ui::text(g,count,b,*cf,col::GOLD_HI,StringAlignmentCenter,StringAlignmentCenter,false,0.5f);
        }
    }
    const Ghost ghost=with_ghost(v,id,panel_open);
    if(inv.valid&&!inv.empty(0)) {
        CardOpts o;o.fixed=true;card(c,RectF(RX,136,RW,CARD_H),inv.id(0),0,false,false,o);
    } else {
        empty_card(c,RectF(RX,136,RW,CARD_H),ButtonState::NORMAL);
    }
    for(const auto& cell:cv_items::layout(ghost.inventory)) {
        const RectF r(RX+cell.col*CARD_GAP2,CARD_Y+cell.row*CARD_STEP,cell.span==2?RW:CARD_NARROW,CARD_H);
        if(cell.kind==cv_items::Cell::LOCKED){locked_card(c,r);continue;}
        if(cell.kind==cv_items::Cell::EMPTY) {
            empty_card(c,r,panel_open?v.mouse.state(hit_id(HIT_SLOT,0)):ButtonState::NORMAL);
            if(panel_open)v.hits.push_back({r,HIT_SLOT,0});
            continue;
        }
        const int e=cell.entry;const auto& words=ghost.inventory;
        const auto* held=cv_items::info(words.id(e));
        CardOpts o;o.ghost=e==ghost.entry;o.equipped=inv.is_equipped(e)&&!o.ghost;o.picked=removing&&e==v.slot;
        o.st=v.mouse.state(hit_id(HIT_SLOT,e));
        if(o.ghost&&inv.empty(e))o.icon_alpha=0.7f;
        card(c,r,words.id(e),words.quantity(e),held&&held->counted,held&&held->percent,o);
        v.hits.push_back({r,HIT_SLOT,e});
    }

    // ---- The placement: quantity, space and the button.
    const bool counted=info&&info->counted&&!removing;
    // The label says what the number is: rounds in a weapon, % charge, the amount of ammunition, or a plain quantity.
    tx::Id qty_label=tx::QTY_LABEL_SINGLE;bool bullet=false;
    if(info&&!removing) {
        if(info->category==cv_items::WEAPONS&&info->counted){qty_label=info->percent?tx::QTY_LABEL_CHARGE:tx::QTY_LABEL_WEAPON;bullet=true;}
        else if(info->category==cv_items::AMMO){qty_label=tx::QTY_LABEL_AMMO;bullet=true;}
    }
    // Items that come one at a time (herbs, keys, the knife) show their fixed quantity, arrows off.
    const std::wstring qty_value=counted?std::to_wstring(panel_open?v.quantity:info->quantity)+(info->percent?L"%":L""):info&&!removing&&info->category!=cv_items::WEAPONS?L"1":L"—";
    stepper(c,0,STEP1_Y,t(v,qty_label),bullet,qty_value,counted,panel_open&&v.row==0,false,HIT_QTY_MINUS,HIT_QTY_PLUS);
    const auto tlist=targets(v);
    // Closed, the panel shows where the item would go by default.
    const int shown=panel_open?v.target:inv.first_empty()>0?0:tlist.empty()?-1:tlist.front();
    const std::wstring where=removing?L"—":shown==0?t(v,tx::FIRST_FREE):shown>0?format(t(v,tx::REPLACE_FMT),shown):L"—";
    stepper(c,1,STEP2_Y,t(v,tx::SLOT),false,where,info&&!removing&&!tlist.empty(),panel_open&&v.row==1,true,HIT_TARGET_PREV,HIT_TARGET_NEXT);
    {
        // Removing turns the button into the removal's, on the chosen space. A wide weapon without two free spaces
        // side by side keeps it off until a space to replace is chosen.
        const bool ready=removing?inv.valid&&inv.can_remove&&removable(inv,v.slot)
            :info&&inv.valid&&inv.writable&&v.target>=0&&!tlist.empty()&&(v.focus==1||fits_free(inv,id));
        const RectF r(RX,BTN_Y,RW,BTN_H);
        ui::primary_button_red(g,r,t(v,removing?tx::REMOVE:tx::PLACE),v.language,19,1.4f,ready?v.mouse.state(hit_id(HIT_PLACE,0)):ButtonState::DISABLED);
        if(ready)v.hits.push_back({r,HIT_PLACE,0});
        if(panel_open&&v.row==2&&ready){stroke(g,RectF(r.X-3,r.Y-3,r.Width+6,r.Height+6),4,col::SEL,1.5f);}
    }

    // ---- The footer: the glyphs of the last input device.
    ui::draw_footer(g,RectF(24,FOOT_Y,1352,40),v.language,kind,footer_groups(v,kind));
}
}  // namespace cv_item_menu

// ---- The window ---------------------------------------------------------------
namespace {
using namespace cv_item_menu;
HWND window=nullptr;View view;std::wstring game_root;float scale=1;ULONGLONG closed_at=0;
uint32_t previous_pad[PAD_PHYS_MAX]={};ULONGLONG repeat_at[PAD_PHYS_MAX]={};
std::atomic<bool> allowed{false};
void paint(HWND h) {
    PAINTSTRUCT ps;HDC dc=BeginPaint(h,&ps);RECT c;GetClientRect(h,&c);
    const int w=std::max<int>(1,c.right),hh=std::max<int>(1,c.bottom);
    Bitmap back(w,hh,PixelFormat32bppARGB);
    {Graphics g(&back);g.ScaleTransform(scale,scale);draw(g,view,game_root);}
    Graphics out(dc);out.DrawImage(&back,0,0);
    EndPaint(h,&ps);
}
void redraw() {if(window)InvalidateRect(window,nullptr,FALSE);}
void note_input(int kind) {
    kind=cv_port_ui::normalize_kind(kind);
    if(view.input!=kind){view.input=kind;redraw();}
}
void act(Action a) {if(apply(view,a))cv_item_menu_close();else redraw();}
int id_at(int x,int y) {const Hit* hit=hit_at(view,x/scale,y/scale);return hit?hit_id(hit->kind,hit->value):-1;}
// The action of a control, on the button's release over it.
void activate(int id) {
    const Hit* found=nullptr;
    for(const auto& hit:view.hits)if(hit_id(hit.kind,hit.value)==id){found=&hit;break;}
    if(!found)return;
    const Hit hit=*found;
    switch(hit.kind) {
    case HIT_CLOSE:cv_item_menu_close();return;
    case HIT_TAB:view.category=hit.value;view.selected=0;view.scroll=0;view.focus=0;break;
    case HIT_ITEM:if(view.selected==hit.value&&view.focus==0)apply(view,CONFIRM);else{view.selected=hit.value;view.focus=0;}break;
    case HIT_SLOT:if(view.focus==1){const auto l=targets(view);if(std::find(l.begin(),l.end(),hit.value)!=l.end()){view.target=hit.value;view.row=1;}}
                  else if(hit.value>0&&!view.inventory.empty(hit.value)){if(view.focus!=2)apply(view,REMOVE);view.slot=hit.value;}   // the item to take out
                  break;
    case HIT_QTY_MINUS:case HIT_QTY_PLUS:case HIT_TARGET_PREV:case HIT_TARGET_NEXT:
        if(view.focus==0)apply(view,CONFIRM);
        view.row=hit.kind<=HIT_QTY_PLUS?0:1;apply(view,(hit.kind==HIT_QTY_MINUS||hit.kind==HIT_TARGET_PREV)?LEFT:RIGHT);break;
    case HIT_PLACE:if(view.focus==0)apply(view,CONFIRM);submit(view);break;
    default:break;
    }
}
void place_window() {
    HWND game=static_cast<HWND>(rsx_d3d12_window());
    scale=cv_port_ui::scale_for(game,W,H);
    const int w=int(std::lround(W*scale)),h=int(std::lround(H*scale));
    const RECT r=cv_port_ui::center_on_game(game,w,h);
    SetWindowPos(window,nullptr,r.left,r.top,w,h,SWP_NOACTIVATE|SWP_NOZORDER);   // owned by the game, never TOPMOST
}
LRESULT CALLBACK proc(HWND h,UINT msg,WPARAM wp,LPARAM lp) {
    switch(msg) {
    case WM_ERASEBKGND:return 1;
    case WM_PAINT:paint(h);return 0;
    case WM_KEYDOWN:
        note_input(PAD_INPUT_KEYBOARD);
        switch(wp) {
        case VK_UP:act(UP);break;case VK_DOWN:act(DOWN);break;case VK_LEFT:act(LEFT);break;case VK_RIGHT:act(RIGHT);break;
        case 'Q':case VK_PRIOR:act(PREV);break;case 'E':case VK_NEXT:case VK_TAB:act(NEXT);break;
        case VK_RETURN:case VK_SPACE:act(CONFIRM);break;case VK_ESCAPE:case VK_BACK:act(BACK);break;
        case 'R':case VK_DELETE:act(REMOVE);break;
        case VK_F6:if(!cv_port_modal::key_repeat(lp))cv_item_menu_close();break;
        }
        return 0;
    case WM_MOUSEMOVE:
        view.mouse.arm(h);
        if(view.mouse.move(id_at(short(LOWORD(lp)),short(HIWORD(lp)))))redraw();
        return 0;
    case WM_MOUSELEAVE:if(view.mouse.leave())redraw();return 0;
    case WM_LBUTTONDOWN: {
        note_input(PAD_INPUT_KEYBOARD);
        const int id=id_at(short(LOWORD(lp)),short(HIWORD(lp)));
        if(id>=0){SetCapture(h);if(view.mouse.down(id))redraw();}
        return 0;}
    case WM_LBUTTONUP:
        if(view.mouse.pressed>=0) {
            ReleaseCapture();int fired=-1;
            const bool changed=view.mouse.up(id_at(short(LOWORD(lp)),short(HIWORD(lp))),fired);
            if(fired>=0){activate(fired);redraw();}   // the action fires on release over the same control
            else if(changed)redraw();
        }
        return 0;
    case WM_MOUSEWHEEL:note_input(PAD_INPUT_KEYBOARD);act(GET_WHEEL_DELTA_WPARAM(wp)>0?UP:DOWN);return 0;
    case WM_TIMER: {
        cv_port_modal::tick(cv_port_modal::ITEMS,h);   // the game window took the focus: win it back
        if(!IsWindowVisible(h)) {
            // Closed with items queued: the game, running again, puts them in
            // on its next frame; say so in its corner, then stop looking.
            const auto done=cv_items::outcomes(view.serial);
            if(!done.empty()){bool ok;perf_overlay_toast(summary(view.language,done,ok).c_str(),ok);}
            if(!cv_items::pending()||GetTickCount64()>=closed_at+5000)KillTimer(h,1);
            return 0;
        }
        const auto before=view.inventory;refresh(view,cv_items::read());
        const unsigned serial=view.serial;poll_result(view);
        bool dirty=serial!=view.serial||std::memcmp(&before,&view.inventory,sizeof before)!=0;
        if(GetForegroundWindow()==h) for(int i=0;i<PAD_PHYS_MAX;i++) {
            PadPhys pad{};if(!pad_phys_poll(i,&pad)){previous_pad[i]=0;continue;}
            cv_port_ui::menu_pad(pad);   // Xbox 360 layout: LB/RB back to L1/R1 (the game's remap is not for this menu)
            const uint32_t edge=pad.buttons&~previous_pad[i];previous_pad[i]=pad.buttons;
            const auto pressed=[&](int b){return (edge>>b)&1u;};
            // The shortcut chords (R1+R2+L1+L2 + a button) belong to the
            // launcher: nothing here while both triggers are held.
            if(cv_launcher_controls::chord_held(pad.buttons))continue;
            if(edge)note_input(pad.kind);
            if(pressed(PB_EAST)){act(BACK);return 0;}
            if(pressed(PB_START)){cv_item_menu_close();return 0;}
            if(pressed(PB_SOUTH))act(CONFIRM);
            if(pressed(PB_WEST))act(REMOVE);
            if(pressed(PB_L1))act(PREV);if(pressed(PB_R1))act(NEXT);
            const int dirs[]={PB_UP,PB_DOWN,PB_LEFT,PB_RIGHT};const Action as[]={UP,DOWN,LEFT,RIGHT};
            for(int d=0;d<4;d++) if((pad.buttons>>dirs[d])&1u) {
                if(((edge>>dirs[d])&1u)||GetTickCount64()>=repeat_at[i]){act(as[d]);repeat_at[i]=GetTickCount64()+((edge>>dirs[d])&1u?350:110);}
                break;
            }
        }
        if(dirty)redraw();
        return 0;}
    case WM_CLOSE:cv_item_menu_close();return 0;
    case WM_DESTROY:window=nullptr;return 0;
    }
    return DefWindowProcW(h,msg,wp,lp);
}
}  // namespace

void cv_item_menu_enable(bool on) {
    allowed=on;cv_items::set_enabled(on);
    if(!on)cv_item_menu_close();
}
bool cv_item_menu_visible() {return window&&IsWindowVisible(window);}
void cv_item_menu_close() {
    if(!cv_item_menu_visible())return;
    closed_at=GetTickCount64();
    if(!cv_items::pending())KillTimer(window,1);   // else it stays to report them
    cv_port_modal::close(cv_port_modal::ITEMS);   // hides, the game runs again (its next frame takes the queue) and gets the focus back
}
void cv_item_menu_toggle(const std::wstring& game,int language) {
    if(!allowed)return;
    if(cv_item_menu_visible()){cv_item_menu_close();return;}
    if(!window) {
        WNDCLASSW wc{};wc.lpfnWndProc=proc;wc.hInstance=GetModuleHandleW(nullptr);wc.hCursor=LoadCursorW(nullptr,MAKEINTRESOURCEW(32512));
        wc.lpszClassName=L"CVItemMenu";wc.hIcon=LoadIconW(wc.hInstance,MAKEINTRESOURCEW(1));RegisterClassW(&wc);
        // Owned by the game window (not the launcher), no TOPMOST: above the game, below other programs.
        window=CreateWindowExW(WS_EX_TOOLWINDOW,L"CVItemMenu",L"Code: Veronica",WS_POPUP,
                               0,0,W,H,static_cast<HWND>(rsx_d3d12_window()),nullptr,wc.hInstance,nullptr);
        if(!window)return;
    }
    cv_port_ui::fonts_available();   // the embedded fonts load here, before any Graphics of the menu exists (GDI+ private fonts must be added first)
    game_root=game;
    view.language=std::clamp(language,0,12);view.focus=0;view.status.clear();view.tone=NEUTRAL;
    view.serial=cv_items::outcome_serial();view.mouse.leave();
    {const int last=pad_last_input();view.input=cv_port_ui::normalize_kind(last==PAD_KIND_NONE?PAD_INPUT_KEYBOARD:last);}
    cv_port_modal::register_close(cv_port_modal::ITEMS,cv_item_menu_close);
    place_window();
    // Owner = the game, shown and focused, the game stands still and ignores the pad; refused when the game is not in front.
    if(!cv_port_modal::open(cv_port_modal::ITEMS,window))return;
    refresh(view,cv_items::read());
    SetFocus(window);
    for(int i=0;i<PAD_PHYS_MAX;i++){PadPhys pad{};pad_phys_poll(i,&pad);cv_port_ui::menu_pad(pad);previous_pad[i]=pad.buttons;}
    SetTimer(window,1,50,nullptr);redraw();
}
#endif
