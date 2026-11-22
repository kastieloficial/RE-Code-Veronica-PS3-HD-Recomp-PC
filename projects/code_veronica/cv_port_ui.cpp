// The port's shared menu look: see cv_port_ui.h. Values come from the approved mockups
// (docs/mockup-inventario/mock.css, docs/mockup-voltar-titulo/mock.css).
#include "cv_port_ui.h"
#include "cv_port_glyphs.h"
#include "cv_srt_i18n.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <initializer_list>
#include <map>
#include <mutex>
#include <tuple>
#include <utility>
#pragma comment(lib,"gdiplus.lib")
using namespace Gdiplus;

namespace cv_port_ui {
namespace {
// The achievement card is drawn on its own thread while the F6/F7/F2 windows draw on the launcher's: the lazy font loading and the font cache share this lock.
std::recursive_mutex& font_lock() {static std::recursive_mutex m;return m;}
// ---- Embedded fonts -----------------------------------------------------------
constexpr int ROLES=6;
const wchar_t* const k_resource[ROLES]={L"CV_FONT_CINZEL_700",L"CV_FONT_CINZEL_600",L"CV_FONT_BARLOW_400",L"CV_FONT_BARLOW_500",L"CV_FONT_BARLOW_600",L"CV_FONT_BARLOW_700"};
const wchar_t* const k_expected[ROLES]={L"Cinzel",L"Cinzel SemiBold",L"Barlow Semi Condensed",L"Barlow Semi Condensed Medium",L"Barlow Semi Condensed SemiBold",L"Barlow Semi Condensed"};
// The family GDI+ reports for each file ("Cinzel" holds Bold and SemiBold, "Barlow Semi Condensed" Regular and Bold) and the style of the role in it.
const wchar_t* const k_family[ROLES]={L"Cinzel",L"Cinzel",L"Barlow Semi Condensed",L"Barlow Semi Condensed Medium",L"Barlow Semi Condensed SemiBold",L"Barlow Semi Condensed"};
const int k_style[ROLES]={FontStyleBold,FontStyleRegular,FontStyleRegular,FontStyleRegular,FontStyleRegular,FontStyleBold};
std::unique_ptr<PrivateFontCollection> collection;
std::vector<std::unique_ptr<PrivateFontCollection>> collections_extra;
const FontFamily* families[ROLES]={};   // point into family_arrays: the originals GDI+ listed, never copies
std::vector<std::unique_ptr<FontFamily[]>> family_arrays;
std::vector<std::vector<unsigned char>> font_bytes;   // the collection reads these: they stay alive
bool loaded=false,load_failed=false,gdi_done=false;
bool resource(const wchar_t* name,const void*& data,DWORD& size) {
    const HMODULE module=GetModuleHandleW(nullptr);const HRSRC res=FindResourceW(module,name,MAKEINTRESOURCEW(10));
    if(!res)return false;size=SizeofResource(module,res);data=LockResource(LoadResource(module,res));return data&&size;
}
// The embedded fonts go in one GDI+ PrivateFontCollection. The bytes are copied to the heap first: the resource pages
// are read-only and GDI+ then listed only the first family. If it still does not list them all, the files are written to
// the temporary folder and added from there. Fonts registered with AddFontMemResourceEx (the F7 search box) are
// invisible to GDI+.
void add_gdi_fonts();
int match_families(PrivateFontCollection& pfc,std::wstring& log,int first=0,int last=ROLES-1) {
    const int count=pfc.GetFamilyCount();if(count<=0)return 0;
    std::unique_ptr<FontFamily[]> all(new FontFamily[count]);int found=0;
    if(pfc.GetFamilies(count,all.get(),&found)!=Ok)return 0;
    int matched=0;
    for(int f=0;f<found;f++) {
        WCHAR name[LF_FACESIZE]={};all[f].GetFamilyName(name);log+=std::wstring(f?L"; ":L"")+name;
        for(int i=first;i<=last;i++)if(!families[i]&&_wcsicmp(name,k_family[i])==0&&all[f].IsStyleAvailable(k_style[i])){families[i]=&all[f];matched++;}
    }
    family_arrays.push_back(std::move(all));
    return matched;
}
bool load_fonts() {
    if(loaded)return true;if(load_failed)return false;
    {Bitmap probe(1,1,PixelFormat32bppARGB);if(probe.GetLastStatus()==GdiplusNotInitialized)return false;}   // too early: try again later
    std::wstring log;int matched=0;
    // One collection with all six files: several collections mixed their faces up in GDI+, and a collection that listed only
    // some of the families (AddMemoryFont did, now and then) is dropped whole and the next source tried. "Cinzel" holds Bold and
    // SemiBold (Regular).
    for(int source=0;source<2&&matched<ROLES;source++) {
        for(auto& f:families)f=nullptr;
        family_arrays.clear();collection.reset();font_bytes.clear();log.clear();matched=0;
        auto pfc=std::make_unique<PrivateFontCollection>();
        WCHAR tmp[MAX_PATH]={};const bool from_files=source==0;
        std::wstring dir;
        if(from_files) {   // the files, from the temporary folder
            if(!GetTempPathW(MAX_PATH,tmp))continue;
            dir=std::wstring(tmp)+L"CodeVeronicaFonts"+L"\\";CreateDirectoryW(dir.c_str(),nullptr);
        } else font_bytes.reserve(ROLES);
        for(int i=0;i<ROLES;i++) {
            const void* data=nullptr;DWORD size=0;if(!resource(k_resource[i],data,size))continue;
            if(from_files) {
                const std::wstring path=dir+k_resource[i]+L".ttf";
                HANDLE h=CreateFileW(path.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_TEMPORARY,nullptr);
                if(h!=INVALID_HANDLE_VALUE){DWORD w=0;WriteFile(h,data,size,&w,nullptr);CloseHandle(h);pfc->AddFontFile(path.c_str());}
            } else {
                font_bytes.emplace_back(size_t(size)+65536,0);std::memcpy(font_bytes.back().data(),data,size);   // zero padding: GDI+ may read past the end
                pfc->AddMemoryFont(font_bytes.back().data(),INT(size));
            }
        }
        matched=match_families(*pfc,log,0,ROLES-1);collection=std::move(pfc);
        std::fprintf(stderr,"[PORT-UI] fonts from %s: %d of %d roles\n",from_files?"files":"memory",matched,ROLES);
    }
    if(!matched){load_failed=true;std::fprintf(stderr,"[PORT-UI] embedded fonts unavailable: using the system faces\n");return false;}
    // A weight that did not register borrows the nearest one that did.
    auto borrow=[&](int to,int from){if(!families[to]&&families[from])families[to]=families[from];};
    borrow(1,0);borrow(0,1);borrow(3,2);borrow(4,5);borrow(3,4);borrow(4,3);borrow(2,3);borrow(5,4);
    std::fprintf(stderr,"[PORT-UI] fonts registered: %d of %d (%ls)\n",matched,ROLES,log.c_str());
    loaded=true;return true;
}
void add_gdi_fonts() {
    if(gdi_done)return;gdi_done=true;
    for(int i=0;i<ROLES;i++){const void* data=nullptr;DWORD size=0,n=0;if(resource(k_resource[i],data,size))AddFontMemResourceEx(const_cast<void*>(data),size,nullptr,&n);}
}
bool bold_role(Role r) {return r==Role::TITLE||r==Role::TITLE_SEMI||r==Role::BODY600||r==Role::BODY700;}
int role_index(Role r) {return int(r);}

// ---- Brushes ------------------------------------------------------------------
using Stop=std::pair<float,Color>;
// A CSS-style linear gradient over `r`; `css_deg`: 180 = top to bottom, 90 = left to right, 160 = mostly down.
std::unique_ptr<LinearGradientBrush> lin(RectF r,float css_deg,std::initializer_list<Stop> stops) {
    if(r.Width<1)r.Width=1;if(r.Height<1)r.Height=1;
    auto brush=std::make_unique<LinearGradientBrush>(r,stops.begin()->second,(stops.end()-1)->second,css_deg-90.f,FALSE);
    std::vector<Color> colors;std::vector<REAL> positions;
    for(const auto& s:stops){colors.push_back(s.second);positions.push_back(s.first);}
    brush->SetInterpolationColors(colors.data(),positions.data(),INT(colors.size()));
    return brush;
}
Color alpha(Color c,int a) {return Color(BYTE(a),c.GetR(),c.GetG(),c.GetB());}
void fill_round(Graphics& g,RectF r,float radius,const Brush& b) {auto p=round_path(r,radius);g.FillPath(&b,p.get());}
void stroke_round(Graphics& g,RectF r,float radius,Color c,float width) {auto p=round_path(r,radius);Pen pen(c,width);g.DrawPath(&pen,p.get());}
RectF inset(RectF r,float d) {return RectF(r.X+d,r.Y+d,std::max(0.f,r.Width-2*d),std::max(0.f,r.Height-2*d));}
// A rounded fill with a gradient border `bw` thick (CSS "padding-box / border-box" pair).
void bordered(Graphics& g,RectF r,float radius,float bw,const Brush& border,const Brush& fill) {
    fill_round(g,r,radius,border);fill_round(g,inset(r,bw),std::max(0.f,radius-bw*0.5f),fill);
}
void top_highlight(Graphics& g,RectF r,int a) {Pen p(Color(BYTE(a),255,255,255),1);g.DrawLine(&p,r.X+2,r.Y+0.5f,r.X+r.Width-2,r.Y+0.5f);}
Color tone(Color c,ButtonState st) {
    auto mix=[&](float t,int to){return Color(c.GetA(),BYTE(c.GetR()+(to-int(c.GetR()))*t),BYTE(c.GetG()+(to-int(c.GetG()))*t),BYTE(c.GetB()+(to-int(c.GetB()))*t));};
    switch(st){case ButtonState::HOVER:return mix(.16f,255);case ButtonState::PRESSED:return mix(.28f,0);
    case ButtonState::DISABLED:{const BYTE y=BYTE((c.GetR()+c.GetG()+c.GetB())/3*0.6f);return Color(c.GetA(),y,y,y);}default:return c;}
}
RectF pressed_rect(RectF r,ButtonState st) {return st==ButtonState::PRESSED?RectF(r.X+1,r.Y+1,r.Width,r.Height):r;}
float keycap_width(const std::wstring& label,float size) {const float n=float(label.size());return n<=1?size:std::max(size,size*(0.5f+0.37f*n));}
}

bool fonts_available() {std::lock_guard<std::recursive_mutex> l(font_lock());return load_fonts();}
const wchar_t* family_name(Role role) {
    std::lock_guard<std::recursive_mutex> l(font_lock());
    if(!load_fonts()||!families[role_index(role)])return nullptr;
    static WCHAR names[ROLES][LF_FACESIZE];WCHAR* out=names[role_index(role)];families[role_index(role)]->GetFamilyName(out);return out;
}
void register_gdi_fonts() {add_gdi_fonts();}
namespace {
std::map<std::wstring,std::unique_ptr<FontFamily>>& system_families() {static std::map<std::wstring,std::unique_ptr<FontFamily>> m;return m;}
}
bool native_fonts(int lang) {return lang>=0&&lang<13&&lang!=3&&lang!=9&&lang!=10&&lang!=11&&lang!=12;}
std::unique_ptr<Font> font(int lang,Role role,float px) {
    std::lock_guard<std::recursive_mutex> l(font_lock());
    const int i=role_index(role);lang=std::clamp(lang,0,12);
    if(native_fonts(lang)&&load_fonts()) {
        // The role's own face first, then the nearest weight that exists.
        static const int order[ROLES][ROLES]={{0,1,5,4,3,2},{1,0,5,4,3,2},{2,3,4,5,1,0},{3,2,4,5,1,0},{4,5,3,2,1,0},{5,4,3,2,1,0}};
        for(int k=0;k<ROLES;k++) {
            const int f=order[i][k];if(!families[f])continue;
            auto made=std::make_unique<Font>(families[f],px,k_style[f],UnitPixel);
            if(made->IsAvailable())return made;
        }
    }
    const bool bold=bold_role(role);
    // The family outlives the Font (a cached Font keeps pointing at it): one object per face name, kept until release_fonts().
    auto& family=system_families()[cv_srt_i18n::font_face(cv_srt_i18n::Language(lang),bold)];
    if(!family)family=std::make_unique<FontFamily>(cv_srt_i18n::font_face(cv_srt_i18n::Language(lang),bold));
    return std::make_unique<Font>(family.get(),px,bold?FontStyleBold:FontStyleRegular,UnitPixel);
}

namespace {
std::map<std::tuple<int,int,int>,std::unique_ptr<Font>>& font_cache() {static std::map<std::tuple<int,int,int>,std::unique_ptr<Font>> cache;return cache;}
}
const Font& font_cached(int lang,Role role,float px) {
    std::lock_guard<std::recursive_mutex> l(font_lock());
    auto& cache=font_cache();
    const auto key=std::make_tuple(std::clamp(lang,0,12),int(role),int(std::lround(px*4)));
    auto found=cache.find(key);if(found!=cache.end())return *found->second;
    return *(cache[key]=font(lang,role,float(std::get<2>(key))/4));
}
void release_fonts() {
    std::lock_guard<std::recursive_mutex> l(font_lock());
    font_cache().clear();system_families().clear();   // every Font and family goes before GdiplusShutdown (at exit they would be freed after it)
    for(auto& f:families)f=nullptr;
    family_arrays.clear();collection.reset();font_bytes.clear();loaded=false;load_failed=false;
}

// ---- Primitives ---------------------------------------------------------------
std::unique_ptr<GraphicsPath> round_path(RectF r,float radius) {
    auto p=std::make_unique<GraphicsPath>();radius=std::min(radius,std::min(r.Width,r.Height)*0.5f);
    if(radius<=0.1f){p->AddRectangle(r);return p;}
    const float d=radius*2;
    p->AddArc(r.X,r.Y,d,d,180,90);p->AddArc(r.X+r.Width-d,r.Y,d,d,270,90);
    p->AddArc(r.X+r.Width-d,r.Y+r.Height-d,d,d,0,90);p->AddArc(r.X,r.Y+r.Height-d,d,d,90,90);p->CloseFigure();return p;
}
void diamond(Graphics& g,float cx,float cy,float size) {
    const float h=size*0.7071f;PointF pts[4]={PointF(cx,cy-h),PointF(cx+h,cy),PointF(cx,cy+h),PointF(cx-h,cy)};
    auto b=lin(RectF(cx-h,cy-h,2*h,2*h),180,{{0,col::GOLD_HI},{1,Color(255,138,102,32)}});g.FillPolygon(b.get(),pts,4);
}
void gold_frame(Graphics& g,RectF outer) {
    const auto state=g.Save();g.SetSmoothingMode(SmoothingModeAntiAlias);
    {SolidBrush shadow(Color(110,0,0,0));fill_round(g,RectF(outer.X-1,outer.Y-1,outer.Width+2,outer.Height+2),4,shadow);}
    auto ring=lin(outer,160,{{0,col::GOLD_HI},{.45f,col::GOLD},{1,col::GOLD_LO}});fill_round(g,outer,3,*ring);
    const RectF in=inset(outer,3);
    GraphicsPath p;p.AddRectangle(in);PathGradientBrush bg(&p);
    bg.SetCenterPoint(PointF(in.X+in.Width*0.3f,in.Y+in.Height*0.06f));
    const Color colors[3]={col::BG2,col::BG1,col::BG0};const REAL pos[3]={0,.6f,1};bg.SetInterpolationColors(colors,pos,3);
    g.FillRectangle(&bg,in);
    Pen line(col::GOLD_DARK,1);g.DrawRectangle(&line,in.X+0.5f,in.Y+0.5f,in.Width-1,in.Height-1);
    g.Restore(state);
}
void header_band(Graphics& g,RectF r) {
    auto b=lin(r,180,{{0,col::BAND0},{1,col::BAND1}});g.FillRectangle(b.get(),r);
    SolidBrush edge(col::GOLD_LO);g.FillRectangle(&edge,r.X,r.Y+r.Height-1,r.Width,1.f);
    SolidBrush dark(col::GOLD_DARK);g.FillRectangle(&dark,r.X,r.Y+r.Height,r.Width,1.f);
    SolidBrush sheen(Color(38,246,231,168));g.FillRectangle(&sheen,r.X,r.Y+r.Height+1,r.Width,1.f);
}
void panel_blue(Graphics& g,RectF r) {
    const auto state=g.Save();g.SetSmoothingMode(SmoothingModeAntiAlias);
    stroke_round(g,RectF(r.X-1,r.Y-1,r.Width+2,r.Height+2),4,col::GOLD_DARK,1);
    auto fill=lin(r,180,{{0,col::PANEL0},{1,col::PANEL1}});fill_round(g,r,3,*fill);
    stroke_round(g,inset(r,0.5f),3,col::BLUE_LINE,1);stroke_round(g,inset(r,1.5f),2,Color(255,5,11,28),1);
    g.Restore(state);
}
void cell_blue(Graphics& g,RectF r,bool selected,ButtonState st) {
    const auto state=g.Save();g.SetSmoothingMode(SmoothingModeAntiAlias);
    GraphicsPath p;p.AddRectangle(r);PathGradientBrush bg(&p);bg.SetCenterPoint(PointF(r.X+r.Width*0.5f,r.Y+r.Height*0.38f));
    const Color centre=tone(selected?Color(255,37,78,156):col::CELL0,st),edge=tone(selected?Color(255,20,48,112):col::CELL1,st);
    const Color colors[3]={edge,edge,centre};const REAL pos[3]={0,.3f,1};bg.SetInterpolationColors(colors,pos,3);
    if(selected)for(int i=3;i>=1;i--){Pen glow(Color(BYTE(18+i*4),255,216,106),float(i*3));g.DrawRectangle(&glow,r.X-i,r.Y-i,r.Width+2*i,r.Height+2*i);}
    fill_round(g,r,2,bg);
    SolidBrush hi(Color(46,160,190,255));g.FillRectangle(&hi,r.X+1,r.Y+1,r.Width-2,1.f);
    if(selected) {
        SolidBrush tint(Color(40,255,216,106));g.FillRectangle(&tint,inset(r,2));
        stroke_round(g,inset(r,1),2,col::SEL,2);
        PointF tri[3]={PointF(r.X,r.Y),PointF(r.X+8,r.Y),PointF(r.X,r.Y+8)};SolidBrush corner(col::SEL);g.FillPolygon(&corner,tri,3);
    } else stroke_round(g,inset(r,0.5f),2,col::BLUE_LINE,1);
    g.Restore(state);
}
void plate(Graphics& g,RectF r,bool active,ButtonState st) {
    const auto state=g.Save();g.SetSmoothingMode(SmoothingModeAntiAlias);r=pressed_rect(r,st);
    const auto T=[&](Color x){return tone(x,st);};
    if(active) {
        const int glow_a=st==ButtonState::HOVER?6:0;
        for(int i=3;i>=1;i--){Pen glow(Color(BYTE(10+i*3+glow_a*i),255,216,106),float(i*3));auto p=round_path(RectF(r.X-i,r.Y-i,r.Width+2*i,r.Height+2*i),4);g.DrawPath(&glow,p.get());}
        auto border=lin(r,180,{{0,T(col::GOLD_HI)},{1,T(Color(255,138,102,32))}});
        auto fill=lin(r,180,{{0,T(Color(255,255,240,180))},{.5f,T(Color(255,227,184,78))},{1,T(Color(255,176,131,42))}});
        bordered(g,r,3,1.5f,*border,*fill);top_highlight(g,inset(r,1.5f),st==ButtonState::PRESSED?60:200);
    } else {
        if(st==ButtonState::HOVER){for(int i=2;i>=1;i--){Pen glow(Color(BYTE(14*i),240,200,100),float(i*3));auto p=round_path(RectF(r.X-i,r.Y-i,r.Width+2*i,r.Height+2*i),4);g.DrawPath(&glow,p.get());}}
        auto border=lin(r,180,{{0,T(col::GOLD)},{1,T(col::GOLD_LO)}});
        auto fill=lin(r,180,{{0,T(Color(255,28,34,50))},{1,T(Color(255,13,17,28))}});
        bordered(g,r,3,1.5f,*border,*fill);top_highlight(g,inset(r,1.5f),st==ButtonState::PRESSED?6:18);
    }
    g.Restore(state);
}
void plate_label(Graphics& g,RectF r,bool active,const std::wstring& label,int lang,float px,float tracking,ButtonState st) {
    plate(g,r,active,st);const Font* f=&font_cached(lang,Role::BODY600,px);r=pressed_rect(r,st);
    text(g,label,inset(r,4),*f,active?col::INK:tone(Color(255,201,208,226),st==ButtonState::HOVER?ButtonState::HOVER:ButtonState::NORMAL),StringAlignmentCenter,StringAlignmentCenter,false,tracking);
}
void badge_gold(Graphics& g,RectF r,const std::wstring& label,int lang,float px,float tracking) {
    const auto state=g.Save();g.SetSmoothingMode(SmoothingModeAntiAlias);
    SolidBrush shadow(Color(120,0,0,0));fill_round(g,RectF(r.X,r.Y+1,r.Width,r.Height),2,shadow);
    auto fill=lin(r,180,{{0,Color(255,255,240,180)},{.6f,Color(255,217,170,62)},{1,Color(255,176,131,42)}});fill_round(g,r,2,*fill);
    const Font* f=&font_cached(lang,Role::BODY700,px);text(g,label,r,*f,col::INK,StringAlignmentCenter,StringAlignmentCenter,false,tracking);
    g.Restore(state);
}
void stepper_arrow(Graphics& g,RectF r,bool left,ButtonState st) {
    const auto state=g.Save();g.SetSmoothingMode(SmoothingModeAntiAlias);r=pressed_rect(r,st);
    SolidBrush shadow(Color(160,0,0,0));if(st!=ButtonState::PRESSED)fill_round(g,RectF(r.X,r.Y+1,r.Width,r.Height),3,shadow);
    auto fill=lin(r,180,{{0,tone(Color(255,241,217,138),st)},{1,tone(Color(255,176,131,42),st)}});fill_round(g,r,3,*fill);top_highlight(g,r,st==ButtonState::PRESSED?40:136);
    const float cx=r.X+r.Width/2,cy=r.Y+r.Height/2,s=std::min(r.Width,r.Height)*0.2f*(st==ButtonState::PRESSED?0.94f:1.f),d=left?-1.f:1.f;
    Pen pen(Color(255,26,18,6),std::max(1.6f,s*0.55f));pen.SetLineJoin(LineJoinRound);pen.SetStartCap(LineCapRound);pen.SetEndCap(LineCapRound);
    PointF pts[3]={PointF(cx-d*s*0.6f,cy-s*1.3f),PointF(cx+d*s*0.7f,cy),PointF(cx-d*s*0.6f,cy+s*1.3f)};g.DrawLines(&pen,pts,3);
    g.Restore(state);
}
void button_disabled(Graphics& g,RectF r,const std::wstring& label,int lang,float px,float tracking);
void primary_button_red(Graphics& g,RectF r,const std::wstring& label,int lang,float px,float tracking,ButtonState st) {
    if(st==ButtonState::DISABLED){button_disabled(g,r,label,lang,px,tracking);return;}
    const auto state=g.Save();g.SetSmoothingMode(SmoothingModeAntiAlias);r=pressed_rect(r,st);
    const int boost=st==ButtonState::HOVER?10:0;
    if(st!=ButtonState::PRESSED)for(int i=3;i>=1;i--){Pen glow(Color(BYTE(8+i*4+boost),190,30,36),float(i*3));auto p=round_path(RectF(r.X-i,r.Y-i,r.Width+2*i,r.Height+2*i),4);g.DrawPath(&glow,p.get());}
    auto border=lin(r,135,{{0,tone(col::GOLD_HI,st)},{.5f,tone(col::GOLD,st)},{1,tone(col::GOLD_LO,st)}});
    auto fill=lin(r,180,{{0,tone(Color(255,167,30,36),st)},{.55f,tone(Color(255,122,20,24),st)},{1,tone(Color(255,90,13,17),st)}});
    bordered(g,r,3,2,*border,*fill);top_highlight(g,inset(r,2),st==ButtonState::PRESSED?20:64);
    const Font* f=&font_cached(lang,Role::BODY600,px*(st==ButtonState::PRESSED?0.97f:1.f));RectF t=inset(r,6);
    text(g,label,RectF(t.X,t.Y+1,t.Width,t.Height),*f,Color(170,0,0,0),StringAlignmentCenter,StringAlignmentCenter,false,tracking);
    text(g,label,t,*f,Color(255,255,255,255),StringAlignmentCenter,StringAlignmentCenter,false,tracking);
    g.Restore(state);
}
// The gold-ring round close button: HOVER brighter ring (#FFE9A8) with a soft red glow, PRESSED darker ring (#9C7424),
// darker fill, the X shifted 1px and scaled 0.94.
void close_button(Graphics& g,RectF r,ButtonState st) {
    const auto state=g.Save();g.SetSmoothingMode(SmoothingModeAntiAlias);
    const bool dis=st==ButtonState::DISABLED;
    if(st==ButtonState::HOVER)for(int i=3;i>=1;i--){Pen glow(Color(BYTE(16+i*6),200,40,40),float(i)*2.5f);g.DrawEllipse(&glow,r.X-i,r.Y-i,r.Width+2*i,r.Height+2*i);}
    Color ring_a=col::GOLD_HI,ring_b=col::GOLD_LO,in_a(255,42,20,24),in_b(255,18,10,12);
    if(st==ButtonState::HOVER){ring_a=Color(255,255,233,168);ring_b=col::GOLD;in_a=Color(255,86,28,32);in_b=Color(255,44,14,18);}
    else if(st==ButtonState::PRESSED){ring_a=Color(255,156,116,36);ring_b=Color(255,90,64,16);in_a=Color(255,22,10,12);in_b=Color(255,10,5,6);}
    else if(dis){ring_a=tone(ring_a,st);ring_b=tone(ring_b,st);in_a=tone(in_a,st);in_b=tone(in_b,st);}
    auto ring=lin(r,135,{{0,ring_a},{.5f,st==ButtonState::PRESSED?ring_a:col::GOLD},{1,ring_b}});g.FillEllipse(ring.get(),r);
    const RectF in=inset(r,2);GraphicsPath p;p.AddEllipse(in);PathGradientBrush fill(&p);fill.SetCenterColor(in_a);fill.SetCenterPoint(PointF(in.X+in.Width/2,in.Y+in.Height/2));
    const Color sur[1]={in_b};INT n=1;fill.SetSurroundColors(sur,&n);g.FillEllipse(&fill,in);
    const float o=st==ButtonState::PRESSED?1.f:0.f,k=(st==ButtonState::PRESSED?0.94f:1.f)*std::min(r.Width,r.Height)*0.2f;
    const float cx=r.X+r.Width/2+o,cy=r.Y+r.Height/2+o;
    Pen x(dis?Color(255,120,112,100):Color(255,246,231,168),std::max(1.6f,k*0.42f));x.SetStartCap(LineCapRound);x.SetEndCap(LineCapRound);
    g.DrawLine(&x,cx-k,cy-k,cx+k,cy+k);g.DrawLine(&x,cx-k,cy+k,cx+k,cy-k);
    g.Restore(state);
}
void button_disabled(Graphics& g,RectF r,const std::wstring& label,int lang,float px,float tracking) {
    const auto state=g.Save();g.SetSmoothingMode(SmoothingModeAntiAlias);
    auto border=lin(r,135,{{0,Color(255,122,112,84)},{1,Color(255,60,50,30)}});
    auto fill=lin(r,180,{{0,Color(255,70,60,60)},{1,Color(255,44,38,38)}});
    bordered(g,r,3,2,*border,*fill);
    const Font* f=&font_cached(lang,Role::BODY600,px);text(g,label,inset(r,6),*f,Color(255,150,146,146),StringAlignmentCenter,StringAlignmentCenter,false,tracking);
    g.Restore(state);
}
void warning_box(Graphics& g,RectF r,const std::wstring& message,int lang,float px) {
    const auto state=g.Save();g.SetSmoothingMode(SmoothingModeAntiAlias);
    SolidBrush bg(Color(40,240,180,76));fill_round(g,r,3,bg);stroke_round(g,inset(r,0.5f),3,Color(110,240,180,76),1);
    SolidBrush bar(col::WARN);g.FillRectangle(&bar,r.X,r.Y+1,5.f,r.Height-2);
    const Font* f=&font_cached(lang,Role::BODY500,px);text(g,message,RectF(r.X+20,r.Y,r.Width-34,r.Height),*f,Color(255,247,217,160),StringAlignmentNear,StringAlignmentCenter,true);
    g.Restore(state);
}
void panel_gold(Graphics& g,RectF r) {
    const auto state=g.Save();g.SetSmoothingMode(SmoothingModeAntiAlias);
    auto border=lin(r,135,{{0,col::GOLD_HI},{.5f,col::GOLD},{1,col::GOLD_LO}});
    auto fill=lin(r,180,{{0,Color(255,14,23,48)},{1,Color(255,8,12,26)}});
    bordered(g,r,3,2,*border,*fill);
    g.Restore(state);
}

// ---- Text -----------------------------------------------------------------------
namespace {
void draw_text(Graphics& g,const std::wstring& s,RectF r,const Font& f,const Brush& b,Color box_color,StringAlignment h,StringAlignment v,bool wrap,float tracking) {
    if(s.empty())return;
    const bool rtl=cv_srt_i18n::rtl_text(s);
    // In a right-to-left format GDI+ swaps near and far: ask for the opposite so Near is still the left edge and Far the right one.
    StringFormat sf;sf.SetAlignment(rtl&&h==StringAlignmentNear?StringAlignmentFar:rtl&&h==StringAlignmentFar?StringAlignmentNear:h);sf.SetLineAlignment(v);sf.SetTrimming(StringTrimmingEllipsisCharacter);
    INT flags=StringFormatFlagsLineLimit;if(!wrap)flags|=StringFormatFlagsNoWrap;if(rtl)flags|=StringFormatFlagsDirectionRightToLeft;sf.SetFormatFlags(flags);
    const size_t sq=s.find(L'□');
    // Right-aligned single lines are placed by their measured width: GDI+ drew nothing for Far-aligned Japanese text.
    if(h==StringAlignmentFar&&!wrap&&!rtl&&sq==std::wstring::npos&&tracking<0.5f) {
        StringFormat tf(StringFormat::GenericTypographic());tf.SetFormatFlags(StringFormatFlagsNoWrap|StringFormatFlagsMeasureTrailingSpaces);
        RectF m;g.MeasureString(s.c_str(),-1,&f,PointF(0,0),&tf,&m);
        if(m.Width<=r.Width) {
            const float lh=f.GetHeight(&g),y=v==StringAlignmentCenter?r.Y+(r.Height-lh)/2:v==StringAlignmentFar?r.Y+r.Height-lh:r.Y;
            g.DrawString(s.c_str(),-1,&f,PointF(r.X+r.Width-m.Width,y),&tf,&b);return;
        }
    }
    // Letter-spaced small caps: one glyph at a time (the fonts here kern little at these sizes).
    if(tracking>=0.5f&&!wrap&&!rtl&&sq==std::wstring::npos) {
        StringFormat tf(StringFormat::GenericTypographic());tf.SetFormatFlags(StringFormatFlagsNoWrap|StringFormatFlagsMeasureTrailingSpaces);
        std::vector<float> adv;float total=0;
        for(wchar_t c:s){RectF m;g.MeasureString(&c,1,&f,PointF(0,0),&tf,&m);adv.push_back(m.Width);total+=m.Width+tracking;}
        total-=tracking;const float lh=f.GetHeight(&g);
        float x=h==StringAlignmentCenter?r.X+(r.Width-total)/2:h==StringAlignmentFar?r.X+r.Width-total:r.X;
        const float y=v==StringAlignmentCenter?r.Y+(r.Height-lh)/2:v==StringAlignmentFar?r.Y+r.Height-lh:r.Y;
        // Without grid fitting: with it each glyph drawn alone was hinted to a different whole-pixel height, so the letters
        // of a spaced title sat unevenly. Every glyph shares one top (y), hence one baseline, whatever its accents.
        const TextRenderingHint hint=g.GetTextRenderingHint();g.SetTextRenderingHint(TextRenderingHintAntiAlias);
        for(size_t i=0;i<s.size();i++){g.DrawString(&s[i],1,&f,PointF(x,y),&tf,&b);x+=adv[i]+tracking;}
        g.SetTextRenderingHint(hint);
        return;
    }
    // The fonts draw the PlayStation square tiny: draw it as a box between the text around it.
    if(sq!=std::wstring::npos&&!rtl&&!wrap&&h==StringAlignmentNear) {
        StringFormat tf(StringFormat::GenericTypographic());tf.SetFormatFlags(StringFormatFlagsNoWrap|StringFormatFlagsMeasureTrailingSpaces);tf.SetLineAlignment(v);
        const float line=f.GetHeight(&g),side=line*0.55f;float cx=r.X;size_t from=0;
        const float top=v==StringAlignmentCenter?r.Y+(r.Height-line)/2:r.Y;
        for(size_t at=sq;;at=s.find(L'□',from)) {
            const std::wstring part=s.substr(from,at==std::wstring::npos?std::wstring::npos:at-from);
            if(!part.empty()){RectF m;g.MeasureString(part.c_str(),-1,&f,PointF(0,0),&tf,&m);
                g.DrawString(part.c_str(),-1,&f,RectF(cx,r.Y,std::max(0.f,r.X+r.Width-cx),r.Height),&tf,&b);cx+=m.Width;}
            if(at==std::wstring::npos)break;
            Pen pen(box_color,1.2f);g.DrawRectangle(&pen,cx+2,top+(line-side)/2,side,side);cx+=side+4;from=at+1;
        }
        return;
    }
    g.DrawString(s.c_str(),-1,&f,r,&sf,&b);
}
}
void text(Graphics& g,const std::wstring& s,RectF r,const Font& f,Color c,StringAlignment h,StringAlignment v,bool wrap,float tracking) {
    SolidBrush b(c);draw_text(g,s,r,f,b,c,h,v,wrap,tracking);
}
float text_width(Graphics& g,const std::wstring& s,const Font& f,float tracking) {
    if(s.empty())return 0;
    StringFormat tf(StringFormat::GenericTypographic());tf.SetFormatFlags(StringFormatFlagsNoWrap|StringFormatFlagsMeasureTrailingSpaces);
    RectF m;g.MeasureString(s.c_str(),-1,&f,PointF(0,0),&tf,&m);return m.Width+(s.size()>1?tracking*float(s.size()-1):0);
}
void title_gold(Graphics& g,const std::wstring& s,RectF r,int lang,float px,float tracking,StringAlignment h,StringAlignment v) {
    const Font* f=&font_cached(lang,Role::TITLE,px);const float lh=f->GetHeight(&g);
    const float top=v==StringAlignmentCenter?r.Y+(r.Height-lh)/2:v==StringAlignmentFar?r.Y+r.Height-lh:r.Y;
    text(g,s,RectF(r.X,r.Y+2,r.Width,r.Height),*f,Color(178,0,0,0),h,v,false,tracking);   // the 2px dark shadow
    auto grad=lin(RectF(r.X,top,r.Width,lh),180,{{.1f,Color(255,255,243,192)},{.55f,Color(255,224,182,80)},{1,Color(255,156,116,36)}});
    draw_text(g,s,r,*f,*grad,Color(255,224,182,80),h,v,false,tracking);
}

// ---- Glyphs and footer ------------------------------------------------------------
float draw_glyph(Graphics& g,const Glyph& glyph,int kind,float x,float y,float size) {
    switch(glyph.kind) {
    case GlyphKind::KEY:case GlyphKind::TEXT: {
        const std::wstring label=!glyph.label.empty()?glyph.label:glyph.value>0?cv_key_cap_label(glyph.value):std::wstring();
        const float w=keycap_width(label,size);cv_keycap::draw(g,label,x,y,w,size);return w;
    }
    case GlyphKind::ARROWS: {
        static const wchar_t* arrows[4]={L"↑",L"↓",L"←",L"→"};float cx=x;int n=0;
        for(int i=0;i<4;i++)if(glyph.value&(1<<i)){if(n++)cx+=3;cv_keycap::draw(g,arrows[i],cx,y,size,size);cx+=size;}
        return cx-x;
    }
    case GlyphKind::SLASH: {const Font* f=&font_cached(1,Role::BODY500,17);const float w=text_width(g,L"/",*f)+8;
        text(g,L"/",RectF(x,y,w,size),*f,Color(255,136,146,174),StringAlignmentCenter,StringAlignmentCenter);return w;}
    case GlyphKind::DPAD:cv_port_glyphs::btn_glyph(g,glyph_layout(kind),cv_port_glyphs::BTN_DPAD_NEUTRAL,x,y,size);return size;
    case GlyphKind::PAD:cv_port_glyphs::btn_glyph(g,glyph_layout(kind),glyph.value,x,y,size);return size;
    }
    return 0;
}
namespace {
constexpr float GLYPH=28,GLYPH_GAP=3,LABEL_GAP=9,SEP_MARGIN=18,SEP=6;
const std::vector<Glyph>& glyphs_for(const FooterGroup& group,int kind) {return kind==PAD_INPUT_KEYBOARD?group.keyboard:group.pad;}
float slash_width() {static const float w=[]{Bitmap b(1,1,PixelFormat32bppARGB);Graphics g(&b);return text_width(g,L"/",font_cached(1,Role::BODY500,17))+8;}();return w;}
// A note is smaller and dimmer than an action label.
const Font& label_font(int lang,const FooterGroup& group) {return font_cached(lang,Role::BODY500,group.note?16.f:17.f);}
// The widths draw_glyph returns, without drawing.
float glyphs_width(const std::vector<Glyph>& list) {
    float w=0;int n=0;
    for(const auto& glyph:list){if(n++)w+=GLYPH_GAP;
        if(glyph.kind==GlyphKind::KEY||glyph.kind==GlyphKind::TEXT){const std::wstring label=!glyph.label.empty()?glyph.label:glyph.value>0?cv_key_cap_label(glyph.value):std::wstring();w+=keycap_width(label,GLYPH);}
        else if(glyph.kind==GlyphKind::ARROWS){int c=0;for(int i=0;i<4;i++)if(glyph.value&(1<<i))c++;w+=c?c*GLYPH+(c-1)*3:0;}
        else if(glyph.kind==GlyphKind::SLASH)w+=slash_width();
        else w+=GLYPH;}
    return w;
}
}
float footer_width(Graphics& g,int lang,int kind,const std::vector<FooterGroup>& groups) {
    float total=0;int n=0;
    for(const auto& group:groups) {
        if(n++)total+=SEP_MARGIN*2+SEP;
        const float gw=glyphs_width(glyphs_for(group,kind));
        total+=gw+(gw>0?LABEL_GAP:0)+text_width(g,group.label,label_font(lang,group),0.3f);
    }
    return total;
}
void draw_footer(Graphics& g,RectF r,int lang,int kind,const std::vector<FooterGroup>& groups,std::vector<RectF>* rects,int hover,int pressed) {
    if(rects)rects->clear();
    if(groups.empty())return;
    const auto state=g.Save();g.SetSmoothingMode(SmoothingModeAntiAlias);
    SolidBrush line(Color(255,58,46,16));g.FillRectangle(&line,r.X,r.Y,r.Width,1.f);
    auto sheen=lin(RectF(r.X,r.Y+1,r.Width,r.Height-1),90,{{0,Color(0,255,255,255)},{.5f,Color(8,255,255,255)},{1,Color(0,255,255,255)}});
    g.FillRectangle(sheen.get(),RectF(r.X,r.Y+1,r.Width,r.Height-1));
    const float total=footer_width(g,lang,kind,groups),avail=std::max(1.f,r.Width-24),s=total>avail?avail/total:1.f;
    const float cy=r.Y+1+(r.Height-1)/2,mid=r.X+r.Width/2;
    if(s<1){g.TranslateTransform(mid,cy);g.ScaleTransform(s,s);g.TranslateTransform(-mid,-cy);}
    float x=r.X+(r.Width-total)/2;int n=0,index=0;
    for(const auto& group:groups) {
        if(n++){x+=SEP_MARGIN;diamond(g,x+SEP/2,cy,SEP);x+=SEP+SEP_MARGIN;}
        const Font* f=&label_font(lang,group);
        const auto& list=glyphs_for(group,kind);
        const float gw=glyphs_width(list),lw=text_width(g,group.label,*f,0.3f),start=x,span=gw+(gw>0?LABEL_GAP:0)+lw;
        const bool hot=index==hover,down=hot&&index==pressed;
        if(rects){RectF box(start-6,cy-18,span+12,36);box.X=mid+(box.X-mid)*s;box.Y=cy+(box.Y-cy)*s;box.Width*=s;box.Height*=s;rects->push_back(box);}
        if(hot){SolidBrush hi(Color(BYTE(down?60:34),255,216,106));auto p=round_path(RectF(start-6,cy-18,span+12,36),5);g.FillPath(&hi,p.get());
            Pen edge(Color(BYTE(down?150:90),240,200,100),1);g.DrawPath(&edge,p.get());}
        int k=0;
        for(const auto& glyph:list){if(k++)x+=GLYPH_GAP;x+=draw_glyph(g,glyph,kind,x,cy-GLYPH/2,GLYPH);}
        if(!list.empty())x+=LABEL_GAP;
        const Color ink=group.note?Color(255,136,146,174):hot?Color(255,255,255,255):col::FOOT;
        text(g,group.label,RectF(x,cy-16,lw+8,32),*f,ink,StringAlignmentNear,StringAlignmentCenter,false,0.3f);x+=lw;
        index++;
    }
    g.Restore(state);
}

// ---- Input helpers ------------------------------------------------------------------
int normalize_kind(int kind) {
    return kind==PAD_KIND_XINPUT||kind==PAD_KIND_DS4||kind==PAD_KIND_DUALSENSE||kind==PAD_KIND_SWITCH?kind:PAD_INPUT_KEYBOARD;
}
// Match the launcher's explicit icon preference; Auto follows local menu input.
int hint_kind(int input_kind) {
    const char* mode=std::getenv("CV_BUTTON_ICONS");
    if(mode&&std::strcmp(mode,"keyboard")==0)return PAD_INPUT_KEYBOARD;
    if(mode&&std::strcmp(mode,"xbox")==0)return PAD_KIND_XINPUT;
    if(mode&&std::strcmp(mode,"ps")==0)return PAD_KIND_DS4;
    if(mode&&std::strcmp(mode,"switch")==0)return PAD_KIND_SWITCH;
    return input_kind==PAD_KIND_XINPUT||input_kind==PAD_KIND_SWITCH?input_kind:(input_kind==PAD_KIND_DS4||input_kind==PAD_KIND_DUALSENSE)?PAD_KIND_DS4:PAD_INPUT_KEYBOARD;
}
int glyph_layout(int kind) {return kind==PAD_KIND_SWITCH?2:kind==PAD_KIND_XINPUT?1:0;}
void menu_pad(PadPhys& p) {
    // Undo gameplay's Xbox 360 shoulder remapping only for this host menu.
    if(pad_phys_360_layout(p.kind)){const auto bits=p.buttons;auto swap=[&](int a,int b){p.buttons=(p.buttons&~((1u<<a)|(1u<<b)))|((bits&(1u<<a))?(1u<<b):0)|((bits&(1u<<b))?(1u<<a):0);};swap(PB_L1,PB_L2);swap(PB_R1,PB_R2);}
    if(p.ly<64)p.buttons|=1u<<PB_UP;else if(p.ly>192)p.buttons|=1u<<PB_DOWN;
    if(p.lx<64)p.buttons|=1u<<PB_LEFT;else if(p.lx>192)p.buttons|=1u<<PB_RIGHT;
}
// Switch names by position, from the PlayStation ones: B bottom, A right, Y left, X top; ZL/ZR are L2/R2.
std::wstring switch_hint(std::wstring t) {
    const wchar_t* from[]={L"L2",L"R2",L"L1",L"R1",L"✕",L"○",L"□",L"△"};
    const wchar_t* to[]={L"ZL",L"ZR",L"L",L"R",L"B",L"A",L"Y",L"X"};
    for(int i=0;i<8;i++)for(size_t at=0;(at=t.find(from[i],at))!=std::wstring::npos;at+=wcslen(to[i]))t.replace(at,wcslen(from[i]),to[i]);
    return t;
}

// ---- Window placement ------------------------------------------------------------------
namespace {
RECT client_screen(HWND game) {
    RECT area{};
    if(game&&GetClientRect(game,&area)&&area.right>0){POINT o{0,0};ClientToScreen(game,&o);OffsetRect(&area,o.x,o.y);}
    else SystemParametersInfoW(SPI_GETWORKAREA,0,&area,0);
    return area;
}
}
float scale_for(HWND game,int w,int h) {
    const RECT a=client_screen(game);const float cw=float(a.right-a.left),ch=float(a.bottom-a.top);
    float s=std::clamp(std::min(cw/1920.f,ch/1080.f),0.55f,2.5f);
    if(w>0&&h>0&&cw>0&&ch>0)s=std::min(s,std::min(cw/float(w),ch/float(h)));
    return s;
}
RECT center_on_game(HWND game,int w,int h) {
    const RECT a=client_screen(game);const int x=a.left+((a.right-a.left)-w)/2,y=a.top+((a.bottom-a.top)-h)/2;
    return RECT{x,y,x+w,y+h};
}
}
