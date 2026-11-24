// The F7 archives window's drawing: see cv_archives_ui.h. Values come from docs/mockup-inventario/arq.css + mock.css.
#include "cv_archives_ui.h"
#include "cv_archives_i18n.h"
#include "cv_port_ui_i18n.h"
#include "cv_test_tools_i18n.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <memory>
#include <tuple>
#pragma comment(lib,"gdiplus.lib")
using namespace Gdiplus;

namespace cv_archives_ui {
namespace {
namespace ui=cv_port_ui;
namespace col=cv_port_ui::col;
namespace lay=layout;
using ui::ButtonState;using ui::Role;
using ui::Glyph;using ui::GlyphKind;

// Cached Consolas family/fonts + pre-scaled 32bppPARGB thumbnails (all freed in release_caches()).
std::unique_ptr<FontFamily> mono_family;
std::map<int,std::unique_ptr<Font>> mono_fonts;
std::map<std::tuple<const uint32_t*,int,int>,std::unique_ptr<Bitmap>> thumb_cache;
float thumb_scale_key=0;

const Font& mono_font(float px) {
    if(!mono_family)mono_family=std::make_unique<FontFamily>(L"Consolas");
    const int k=int(std::lround(px*4));
    auto it=mono_fonts.find(k);
    if(it!=mono_fonts.end())return *it->second;
    return *(mono_fonts[k]=std::make_unique<Font>(mono_family.get(),px,FontStyleBold,UnitPixel));
}
float graphics_scale(Graphics& g) {
    Matrix m;g.GetTransform(&m);REAL e[6]={};m.GetElements(e);
    return (e[0]>0.05f&&e[0]<10.f)?e[0]:1.f;
}

const wchar_t* ts(const View& v,cv_archives_i18n::Id id) {return cv_archives_i18n::strings[std::clamp(v.language,0,12)][id];}
const wchar_t* ps(const View& v,cv_port_ui_i18n::Id id) {return cv_port_ui_i18n::strings[std::clamp(v.language,0,12)][id];}
const wchar_t* mt(const View& v,cv_test_i18n::Id id) {return cv_test_i18n::k_text[std::clamp(v.language,0,12)][id];}
const Font& F(const View& v,Role role,float px) {return ui::font_cached(v.language,role,px);}
const Color C_DIM(255,159,168,192),C_LIB(255,159,168,192),C_FIELD(255,109,123,163),C_WHO(255,242,245,252),C_WHERE(255,180,189,214),
    C_WHEN(255,136,146,174),C_AMBER(255,240,180,76),C_GOLD_TXT(255,227,184,78),C_LABEL(255,183,191,214),C_DETAIL(255,230,236,248);

ButtonState state(const View& v,int kind,int value=0,bool enabled=true) {return v.mouse.state(hit_id(kind,value),enabled);}

std::unique_ptr<LinearGradientBrush> lin(RectF r,float css_deg,Color a,Color b,Color mid=Color(0,0,0,0),bool has_mid=false,float at=0.5f) {
    if(r.Width<1)r.Width=1;if(r.Height<1)r.Height=1;
    auto brush=std::make_unique<LinearGradientBrush>(r,a,b,css_deg-90.f,FALSE);
    if(has_mid){const Color cs[3]={a,mid,b};const REAL ps[3]={0,at,1};brush->SetInterpolationColors(cs,ps,3);}
    return brush;
}
void fill_round(Graphics& g,RectF r,float radius,const Brush& b) {auto p=ui::round_path(r,radius);g.FillPath(&b,p.get());}
void stroke_round(Graphics& g,RectF r,float radius,Color c,float width) {
    Pen pen(c,width);auto p=ui::round_path(RectF(r.X+width/2,r.Y+width/2,r.Width-width,r.Height-width),std::max(0.f,radius-width/2));g.DrawPath(&pen,p.get());
}
RectF inset(RectF r,float d) {return RectF(r.X+d,r.Y+d,std::max(0.f,r.Width-2*d),std::max(0.f,r.Height-2*d));}
Color tone_color(Tone t) {return t==Tone::BAD?col::BAD:t==Tone::GOOD?col::OK:t==Tone::WARN?C_AMBER:Color(255,184,191,214);}

// ui::text with a line box tall enough for the language's face: GDI+ drops a line that does not fit its rectangle (LineLimit),
// and the fallback faces of Arabic, Japanese, Chinese and Korean are taller than Barlow.
void txt(Graphics& g,const std::wstring& s,RectF r,const Font& f,Color c,StringAlignment h=StringAlignmentNear,StringAlignment v=StringAlignmentNear,bool wrap=false,float tracking=0) {
    if(!wrap&&v==StringAlignmentCenter){const float need=f.GetSize()*1.8f;if(r.Height<need){r.Y-=(need-r.Height)/2;r.Height=need;}}
    ui::text(g,s,r,f,c,h,v,wrap,tracking);
}

// ---- Icons (vectors; the fonts lack them) --------------------------------------
void search_icon(Graphics& g,float x,float y) {   // 18x18: a magnifier
    const auto st=g.Save();g.SetSmoothingMode(SmoothingModeAntiAlias);
    Pen p(Color(255,109,123,163),2.f);p.SetStartCap(LineCapRound);p.SetEndCap(LineCapRound);
    g.DrawEllipse(&p,x+2.3f,y+2.3f,10.4f,10.4f);Pen q(Color(255,109,123,163),2.2f);q.SetStartCap(LineCapRound);q.SetEndCap(LineCapRound);
    g.DrawLine(&q,x+11.5f,y+11.5f,x+16.f,y+16.f);g.Restore(st);
}
void info_icon(Graphics& g,float x,float y,Color c) {   // 20x20: a circled i
    const auto st=g.Save();g.SetSmoothingMode(SmoothingModeAntiAlias);
    Pen p(c,1.6f);g.DrawEllipse(&p,x+1.8f,y+1.8f,16.4f,16.4f);Pen q(c,2.f);q.SetStartCap(LineCapRound);q.SetEndCap(LineCapRound);
    g.DrawLine(&q,x+10.f,y+9.f,x+10.f,y+14.5f);SolidBrush b(c);g.FillEllipse(&b,x+8.8f,y+4.8f,2.4f,2.4f);g.Restore(st);
}
void warn_icon(Graphics& g,float x,float y,Color c) {   // 20x20: a triangle with a mark
    const auto st=g.Save();g.SetSmoothingMode(SmoothingModeAntiAlias);
    SolidBrush b(c);const PointF t[3]={PointF(x+10,y+1.5f),PointF(x+19,y+17.5f),PointF(x+1,y+17.5f)};g.FillPolygon(&b,t,3);
    Pen p(Color(255,26,18,6),2.f);p.SetStartCap(LineCapRound);p.SetEndCap(LineCapRound);
    g.DrawLine(&p,x+10.f,y+7.f,x+10.f,y+12.f);g.DrawLine(&p,x+10.f,y+14.f,x+10.f,y+15.2f);g.Restore(st);
}
void plus_icon(Graphics& g,RectF r) {   // the "New save" cross, gold gradient
    const auto st=g.Save();g.SetSmoothingMode(SmoothingModeAntiAlias);
    const float cx=r.X+r.Width/2,cy=r.Y+r.Height/2,a=13,t=3;
    auto b=lin(RectF(cx-a,cy-a,2*a,2*a),180,Color(255,255,243,192),Color(255,156,116,36),Color(255,224,182,80),true,.55f);
    g.FillRectangle(b.get(),RectF(cx-a,cy-t/2,2*a,t));g.FillRectangle(b.get(),RectF(cx-t/2,cy-a,t,2*a));g.Restore(st);
}
void pause_bars(Graphics& g,float x,float cy) {
    SolidBrush b(Color(255,227,184,78));
    for(int i=0;i<2;i++)fill_round(g,RectF(x+i*6.5f,cy-7,3.5f,14),1,b);
}
void side_glyph(Graphics& g,const View& v,int kind,RectF r,int hit_kind,bool left) {
    const bool kb=kind==PAD_INPUT_KEYBOARD;
    const ButtonState st=state(v,hit_kind);
    ui::draw_glyph(g,kb?Glyph{GlyphKind::KEY,left?'Q':'E',L""}:Glyph{GlyphKind::PAD,left?PB_L1:PB_R1,L""},kind,r.X,r.Y,r.Width<28?28.f:r.Width);
    if(st==ButtonState::HOVER||st==ButtonState::PRESSED) {   // a gold ring: the glyph is clickable (previous / next tab)
        const auto s=g.Save();g.SetSmoothingMode(SmoothingModeAntiAlias);Pen p(Color(BYTE(st==ButtonState::PRESSED?255:200),255,216,106),1.6f);
        auto path=ui::round_path(RectF(r.X-2,r.Y-2,r.Width+4,r.Height+4),7);g.DrawPath(&p,path.get());g.Restore(s);
    }
}
// The width of a glyph (as the footer measures it): a one-glyph group has the glyph, then the label gap.
float glyph_width(Graphics& g,int lang,int kind,const Glyph& glyph) {
    ui::FooterGroup gr{{glyph},{glyph},L""};return std::max(0.f,ui::footer_width(g,lang,kind,{gr})-9.f);
}

Color mixc(Color c,float t,int to) {return Color(c.GetA(),BYTE(c.GetR()+(to-int(c.GetR()))*t),BYTE(c.GetG()+(to-int(c.GetG()))*t),BYTE(c.GetB()+(to-int(c.GetB()))*t));}
Color shade(Color c,ButtonState st) {return st==ButtonState::HOVER?mixc(c,.14f,255):st==ButtonState::PRESSED?mixc(c,.25f,0):c;}
// A list row (.rw in arq.css): a flat blue card; the chosen one has the gold border, glow and corner mark.
void row_card(Graphics& g,RectF r,bool selected,ButtonState st) {
    const auto state=g.Save();g.SetSmoothingMode(SmoothingModeAntiAlias);
    if(selected)for(int i=3;i>=1;i--){Pen glow(Color(BYTE(16+i*8),255,216,106),float(i*3));g.DrawRectangle(&glow,r.X-i,r.Y-i,r.Width+2*i,r.Height+2*i);}
    auto fill=selected?lin(r,180,shade(Color(255,31,68,142),st),shade(Color(255,20,48,112),st)):lin(r,180,shade(Color(255,20,44,102),st),shade(Color(255,13,32,80),st));
    fill_round(g,r,2,*fill);
    SolidBrush hi(Color(selected?30:41,160,190,255));g.FillRectangle(&hi,r.X+1,r.Y+1,r.Width-2,1.f);
    if(selected){SolidBrush tint(Color(10,255,216,106));g.FillRectangle(&tint,inset(r,2));stroke_round(g,r,2,col::SEL,2);
        const PointF tri[3]={PointF(r.X,r.Y),PointF(r.X+10,r.Y),PointF(r.X,r.Y+10)};SolidBrush corner(col::SEL);g.FillPolygon(&corner,tri,3);}
    else stroke_round(g,r,2,col::BLUE_LINE,1);
    g.Restore(state);
}
// The entry stepper's gold square with a minus or a plus (the F7 look of arq.css .vstp .ar).
void step_button(Graphics& g,RectF r,bool minus,ButtonState st) {
    const auto state=g.Save();g.SetSmoothingMode(SmoothingModeAntiAlias);
    if(st==ButtonState::PRESSED)r=RectF(r.X+1,r.Y+1,r.Width,r.Height);
    SolidBrush shadow(Color(160,0,0,0));if(st!=ButtonState::PRESSED)fill_round(g,RectF(r.X,r.Y+1,r.Width,r.Height),3,shadow);
    auto fill=lin(r,180,shade(Color(255,241,217,138),st),shade(Color(255,176,131,42),st));fill_round(g,r,3,*fill);
    {Pen hi(Color(BYTE(st==ButtonState::PRESSED?40:136),255,255,255),1);g.DrawLine(&hi,r.X+2,r.Y+0.5f,r.X+r.Width-2,r.Y+0.5f);}
    const float cx=r.X+r.Width/2,cy=r.Y+r.Height/2,k=r.Width*0.2f;Pen pen(Color(255,26,18,6),2.6f);pen.SetStartCap(LineCapRound);pen.SetEndCap(LineCapRound);
    g.DrawLine(&pen,cx-k,cy,cx+k,cy);if(!minus)g.DrawLine(&pen,cx,cy-k,cx,cy+k);
    g.Restore(state);
}

// ---- Pieces ----------------------------------------------------------------------
void header(Graphics& g,const View& v) {
    ui::header_band(g,lay::HEADER_BAND);
    // "CODE: VERONICA" (small caps, spaced) over the Cinzel title; "PAUSED" with the bars at the right.
    txt(g,L"CODE: VERONICA",RectF(24,8.5f,500,16),F(v,Role::BODY600,13),C_DIM,StringAlignmentNear,StringAlignmentCenter,false,3);
    ui::title_gold(g,ts(v,cv_archives_i18n::ARCHIVES),RectF(24,12.5f,700,60),v.language,30,2);   // (centre 42.5: a tall box, the fallback faces are tall)
    const std::wstring paused=ps(v,cv_port_ui_i18n::PAUSED_CAPS);const Font& pf=F(v,Role::BODY600,16);
    const float pw=ui::text_width(g,paused,pf,2.5f),right=W-28.f,cy=34.f;
    txt(g,paused,RectF(right-pw-4,cy-14,pw+8,28),pf,Color(255,200,206,221),StringAlignmentNear,StringAlignmentCenter,false,2.5f);
    pause_bars(g,right-pw-10-10,cy);
}
void tabs(Graphics& g,const View& v,int kind) {
    const wchar_t* names[3]={ts(v,cv_archives_i18n::LOAD_TAB),ts(v,cv_archives_i18n::SAVE_TAB),ts(v,cv_archives_i18n::MAPS_TAB)};
    for(int i=0;i<3;i++)ui::plate_label(g,lay::tab(i),v.page==i,names[i],v.language,19,2,state(v,HIT_TAB,i));
    side_glyph(g,v,kind,lay::TAB_PREV,HIT_TAB_PREV,true);
    side_glyph(g,v,kind,lay::TAB_NEXT,HIT_TAB_NEXT,false);
}
void picture(Graphics& g,RectF box,const uint32_t* pixels) {   // 16:9 picture, "cover" fit, cached 32bppPARGB
    const float sc=graphics_scale(g);
    const float qsc=std::round(sc*100.f);
    if(thumb_scale_key!=0&&thumb_scale_key!=qsc)thumb_cache.clear();
    thumb_scale_key=qsc;
    const int pw=std::max(1,int(std::ceil(box.Width*sc)));
    const int ph=std::max(1,int(std::ceil(box.Height*sc)));
    const auto key=std::make_tuple(pixels,pw,ph);
    auto it=thumb_cache.find(key);
    if(it==thumb_cache.end()) {
        Bitmap src(int(THUMB_W),int(THUMB_H),int(THUMB_W*4),PixelFormat32bppARGB,reinterpret_cast<BYTE*>(const_cast<uint32_t*>(pixels)));
        auto dst=std::make_unique<Bitmap>(pw,ph,PixelFormat32bppPARGB);
        Graphics bg(dst.get());
        bg.SetInterpolationMode(InterpolationModeHighQualityBicubic);
        bg.SetPixelOffsetMode(PixelOffsetModeHalf);
        const float s=std::max(float(pw)/THUMB_W,float(ph)/THUMB_H),w=THUMB_W*s,h=THUMB_H*s;
        bg.DrawImage(&src,RectF((pw-w)/2,(ph-h)/2,w,h),0,0,float(THUMB_W),float(THUMB_H),UnitPixel);
        if(thumb_cache.size()>160)thumb_cache.clear();
        it=thumb_cache.emplace(key,std::move(dst)).first;
    }
    const auto st=g.Save();g.SetInterpolationMode(InterpolationModeLowQuality);
    g.DrawImage(it->second.get(),box);g.Restore(st);
}
// A picture in the gold frame (the preview) or a plain placeholder text.
void preview(Graphics& g,const View& v,RectF r) {
    const auto st=g.Save();g.SetSmoothingMode(SmoothingModeAntiAlias);
    auto border=lin(r,135,col::GOLD_HI,col::GOLD_LO,col::GOLD,true,.5f);fill_round(g,r,3,*border);
    const RectF in=inset(r,2);SolidBrush bg(Color(255,13,14,15));fill_round(g,in,2,bg);
    if(v.image) {
        picture(g,in,v.image);
        if(v.page==PAGE_MAPS){SolidBrush shade(Color(110,0,0,0));g.FillRectangle(&shade,in);}   // the map picture is a stand-in: dimmed
    } else if(!v.placeholder.empty())
        txt(g,v.placeholder,in,F(v,Role::BODY400,19),Color(255,157,148,137),StringAlignmentCenter,StringAlignmentCenter,true,0);
    if(!v.tag.empty()) {   // the room code, bottom left
        const Font& mono=mono_font(17);
        const float tw=ui::text_width(g,v.tag,mono,1)+18;const RectF tag(in.X+12,in.Y+in.Height-10-26,tw,26);
        SolidBrush b(Color(179,10,15,30));fill_round(g,tag,2,b);stroke_round(g,tag,2,col::GOLD_LO,1);
        txt(g,v.tag,tag,mono,Color(255,246,231,168),StringAlignmentCenter,StringAlignmentCenter,false,1);
    }
    g.Restore(st);
}
void diamond_small(Graphics& g,float x,float cy,Color c) {SolidBrush b(c);const PointF p[4]={PointF(x+3.5f,cy-3.5f-1.5f),PointF(x+7.f+1.5f,cy),PointF(x+3.5f,cy+3.5f+1.5f),PointF(x-1.5f,cy)};g.FillPolygon(&b,p,4);}
void list_row(Graphics& g,const View& v,int slot,const Row& row,int index) {
    const RectF r=lay::row(slot);const bool on=index==v.selected;
    row_card(g,r,on,state(v,HIT_ROW,index));
    const float ox=r.X,oy=r.Y;
    if(row.kind==Row::MAP) {
        const Font& mono=mono_font(32);
        txt(g,row.title,RectF(ox+24,oy+14,600,38),mono,C_WHO,StringAlignmentNear,StringAlignmentCenter,false,1);
        txt(g,row.where,RectF(ox+24,oy+57.5f,760,22),F(v,Role::BODY400,18),C_DIM,StringAlignmentNear,StringAlignmentCenter);
        if(!row.event.empty()) {
            diamond_small(g,ox+24,oy+87,C_AMBER);
            txt(g,row.event,RectF(ox+38,oy+77,700,20),F(v,Role::BODY600,16),C_AMBER,StringAlignmentNear,StringAlignmentCenter);
        }
        return;
    }
    if(row.kind==Row::NEW_SAVE) {
        plus_icon(g,RectF(ox+24,oy+12,80,80));
        txt(g,row.title,RectF(ox+128,oy+19,670,28),F(v,Role::BODY600,23),C_WHO,StringAlignmentNear,StringAlignmentCenter);
        txt(g,row.where,RectF(ox+128,oy+53,670,44),F(v,Role::BODY400,17),C_DIM,StringAlignmentNear,StringAlignmentNear,true);
        return;
    }
    const RectF th(ox+7,oy+6,160,90);   // the picture; a text when there is none
    SolidBrush bg(Color(255,13,14,15));g.FillRectangle(&bg,th);
    if(row.thumb)picture(g,inset(th,1),row.thumb);
    else txt(g,row.original?ts(v,cv_archives_i18n::ORIGINAL):ts(v,cv_archives_i18n::NO_PREVIEW),th,F(v,Role::BODY400,15),Color(255,157,148,137),StringAlignmentCenter,StringAlignmentCenter,true);
    {Pen edge(Color(255,61,54,46),1);g.DrawRectangle(&edge,th.X+0.5f,th.Y+0.5f,th.Width-1,th.Height-1);}
    txt(g,row.title,RectF(ox+185,oy+12.5f,620,26),F(v,Role::BODY600,21),C_WHO,StringAlignmentNear,StringAlignmentCenter);
    txt(g,row.where,RectF(ox+185,oy+43,620,22),F(v,Role::BODY400,17.5f),C_WHERE,StringAlignmentNear,StringAlignmentCenter);
    txt(g,row.when,RectF(ox+185,oy+70.5f,620,22),F(v,Role::BODY400,16.5f),C_WHEN,StringAlignmentNear,StringAlignmentCenter,false,0.3f);
}
void list(Graphics& g,const View& v) {
    for(int slot=0;slot<int(v.rows.size())&&slot<ROWS_VISIBLE;slot++)list_row(g,v,slot,v.rows[size_t(slot)],v.scroll+slot);
    if(v.count>ROWS_VISIBLE) {   // where the visible rows are in the list
        const RectF t=lay::SCROLL_TRACK;const float bar=std::max(30.f,t.Height*ROWS_VISIBLE/v.count);
        SolidBrush back(Color(255,18,35,79));fill_round(g,t,2,back);
        const float y=t.Y+(t.Height-bar)*v.scroll/float(std::max(1,v.count-ROWS_VISIBLE));
        auto b=lin(RectF(t.X,y,t.Width,bar),180,Color(255,241,217,138),Color(255,176,131,42));fill_round(g,RectF(t.X,y,t.Width,bar),2,*b);
    }
    if(v.count==0)txt(g,v.page==PAGE_MAPS?L"":ts(v,cv_archives_i18n::EMPTY),RectF(24,lay::LIST_Y+110,820,120),F(v,Role::BODY500,20),C_DIM,StringAlignmentCenter,StringAlignmentCenter,true);
}
void filter_and_now(Graphics& g,const View& v) {   // Maps: the search field's frame, and the room the character is in
    const auto st=g.Save();g.SetSmoothingMode(SmoothingModeAntiAlias);
    const RectF f=lay::FILTER_BOX;
    SolidBrush bg(Color(255,7,11,22));fill_round(g,f,3,bg);
    {SolidBrush inner(Color(90,0,0,0));g.FillRectangle(&inner,RectF(f.X+1,f.Y+1,f.Width-2,2.f));}
    stroke_round(g,f,3,v.search_focus?col::SEL:col::BLUE_LINE,v.search_focus?1.5f:1);
    search_icon(g,f.X+14,f.Y+11);
    if(!v.search_cue.empty())txt(g,v.search_cue,lay::FILTER_EDIT,F(v,Role::BODY400,18),C_FIELD,StringAlignmentNear,StringAlignmentCenter);   // (the window's EDIT shows its own cue)
    g.Restore(st);
    {
        const std::wstring label=ts(v,cv_archives_i18n::CURRENT);
        const Font& mono=mono_font(21);
        const std::wstring code=v.now_room.empty()?L"—":v.now_room;
        const float cw=ui::text_width(g,code,mono,1),right=lay::NOW_ROOM.X+lay::NOW_ROOM.Width;
        txt(g,code,RectF(right-cw-8,lay::NOW_ROOM.Y,cw+8,lay::NOW_ROOM.Height),mono,Color(255,227,184,78),StringAlignmentFar,StringAlignmentCenter,false,0);
        txt(g,label,RectF(right-cw-12-400,lay::NOW_ROOM.Y,400,lay::NOW_ROOM.Height),F(v,Role::BODY400,17),C_DIM,StringAlignmentFar,StringAlignmentCenter);
    }
}
void library_header(Graphics& g,const View& v) {
    txt(g,ts(v,cv_archives_i18n::LIBRARY),lay::LIBRARY_LABEL,F(v,Role::BODY600,16),C_LIB,StringAlignmentNear,StringAlignmentCenter,false,2.5f);
    txt(g,std::to_wstring(v.files)+L" "+ts(v,cv_archives_i18n::FILES),lay::LIBRARY_LABEL,F(v,Role::BODY500,17),C_LIB,StringAlignmentFar,StringAlignmentCenter);
}
void detail(Graphics& g,const View& v) {
    const bool maps=v.page==PAGE_MAPS;
    preview(g,v,maps?lay::PREVIEW_MAPS:lay::PREVIEW);
    txt(g,v.heading,maps?RectF(878,lay::HEADING_MAPS.Y,496,29):RectF(878,lay::HEADING.Y,496,29),F(v,Role::BODY600,23),C_WHO,StringAlignmentNear,StringAlignmentCenter,false,0);
    if(maps)txt(g,v.note,RectF(878,lay::NOTE_MAPS.Y,496,23),F(v,Role::BODY400,17),C_WHEN,StringAlignmentNear,StringAlignmentCenter,false,0);
    else if(!v.character.empty()) {
        const RectF a=lay::DETAIL_CHARACTER,b=lay::DETAIL_TIME;const Font& lf=F(v,Role::BODY400,17),& vf=F(v,Role::BODY600,17);
        txt(g,ts(v,cv_archives_i18n::CHARACTER),a,lf,C_DIM,StringAlignmentNear,StringAlignmentCenter);
        txt(g,v.character,RectF(a.X+150,a.Y,a.Width-150,a.Height),vf,C_DETAIL,StringAlignmentNear,StringAlignmentCenter);
        txt(g,ts(v,cv_archives_i18n::TIME),b,lf,C_DIM,StringAlignmentNear,StringAlignmentCenter);
        txt(g,v.play_time,RectF(b.X+150,b.Y,b.Width-150,b.Height),vf,C_DETAIL,StringAlignmentNear,StringAlignmentCenter);
    } else if(!v.note.empty())txt(g,v.note,lay::NOTE_NEW,F(v,Role::BODY400,17),C_WHEN,StringAlignmentNear,StringAlignmentNear,true);
}
void entry_stepper(Graphics& g,const View& v,int kind) {
    const auto st=g.Save();g.SetSmoothingMode(SmoothingModeAntiAlias);
    const RectF r=lay::STEPPER;
    auto fill=lin(r,180,Color(255,28,34,50),Color(255,13,17,28));fill_round(g,r,3,*fill);stroke_round(g,r,3,col::GOLD_LO,1);
    {SolidBrush hi(Color(15,255,255,255));g.FillRectangle(&hi,r.X+1,r.Y+1,r.Width-2,1.f);}
    txt(g,mt(v,cv_test_i18n::ENTRY),RectF(r.X+16,r.Y,200,r.Height),F(v,Role::BODY500,18),Color(255,183,191,214),StringAlignmentNear,StringAlignmentCenter);
    step_button(g,lay::STEP_MINUS,true,state(v,HIT_STEP_MINUS));
    step_button(g,lay::STEP_PLUS,false,state(v,HIT_STEP_PLUS));
    {SolidBrush bg(Color(255,7,11,22));fill_round(g,lay::STEP_VALUE,2,bg);stroke_round(g,lay::STEP_VALUE,2,Color(255,58,68,102),1);}
    txt(g,std::to_wstring(v.spawn),lay::STEP_VALUE,F(v,Role::BODY700,20),Color(255,255,255,255),StringAlignmentCenter,StringAlignmentCenter);
    // The left / right hint, aligned to the right of its fixed slot.
    const Glyph arrows=kind==PAD_INPUT_KEYBOARD?Glyph{GlyphKind::ARROWS,12,L""}:Glyph{GlyphKind::DPAD,0,L""};
    const float w=glyph_width(g,v.language,kind,arrows);
    ui::draw_glyph(g,arrows,kind,lay::STEP_HINT.X+lay::STEP_HINT.Width-w,lay::STEP_HINT.Y+(lay::STEP_HINT.Height-28)/2,28);
    g.Restore(st);
    // What "entry" means, under the stepper.
    info_icon(g,lay::ENTRY_HINT.X,lay::ENTRY_HINT.Y,C_DIM);
    txt(g,ts(v,cv_archives_i18n::ENTRY_HINT),RectF(lay::ENTRY_HINT.X+29,lay::ENTRY_HINT.Y,lay::ENTRY_HINT.Width-29,lay::ENTRY_HINT.Height),F(v,Role::BODY400,15),C_DIM,StringAlignmentNear,StringAlignmentNear,true);
}
// A button with the action's glyph and its label, centred together.
void button_content(Graphics& g,const View& v,int kind,RectF r,const Glyph& glyph,const std::wstring& label,float px,float tracking,Color ink,bool shadow,bool pressed) {
    const Font& f=F(v,Role::BODY600,px);const float gw=glyph_width(g,v.language,kind,glyph),tw=ui::text_width(g,label,f,tracking),total=gw+12+tw;
    const float o=pressed?1.f:0.f,x=r.X+(r.Width-total)/2+o,cy=r.Y+r.Height/2+o;
    ui::draw_glyph(g,glyph,kind,x,cy-14,28);
    if(shadow)txt(g,label,RectF(x+gw+12,cy-16+1,tw+10,32),f,Color(170,0,0,0),StringAlignmentNear,StringAlignmentCenter,false,tracking);
    txt(g,label,RectF(x+gw+12,cy-16,tw+10,32),f,ink,StringAlignmentNear,StringAlignmentCenter,false,tracking);
}
void delete_plate(Graphics& g,RectF r,ButtonState st) {
    const auto s=g.Save();g.SetSmoothingMode(SmoothingModeAntiAlias);
    if(st==ButtonState::PRESSED)r=RectF(r.X+1,r.Y+1,r.Width,r.Height);
    if(st==ButtonState::HOVER){for(int i=2;i>=1;i--){Pen gl(Color(BYTE(18*i),230,90,90),float(i*3));auto p=ui::round_path(RectF(r.X-i,r.Y-i,r.Width+2*i,r.Height+2*i),4);g.DrawPath(&gl,p.get());}}
    auto fill=lin(r,180,shade(Color(255,58,20,24),st),shade(Color(255,34,12,16),st));
    fill_round(g,r,3,*fill);
    stroke_round(g,r,3,shade(Color(255,196,86,86),st),1.5f);
    {SolidBrush hi(Color(28,255,190,190));g.FillRectangle(&hi,r.X+2,r.Y+1.5f,r.Width-4,1.f);}
    g.Restore(s);
}
void buttons(Graphics& g,const View& v,int kind) {
    const bool kb=kind==PAD_INPUT_KEYBOARD,maps=v.page==PAGE_MAPS;
    const RectF pr=maps?lay::PRIMARY_MAPS:lay::PRIMARY;
    const std::wstring label=maps?ts(v,cv_archives_i18n::LOAD_MAP_BTN):v.page==PAGE_SAVE?ts(v,cv_archives_i18n::NEW_SAVE):ts(v,cv_archives_i18n::LOAD_SAVE_BTN);
    const bool enabled=v.primary_enabled&&!v.busy;const ButtonState st=enabled?state(v,HIT_PRIMARY):ButtonState::DISABLED;
    ui::primary_button_red(g,pr,L"",v.language,19,1.4f,st);
    if(enabled)button_content(g,v,kind,pr,kb?Glyph{GlyphKind::KEY,VK_RETURN,L""}:Glyph{GlyphKind::PAD,PB_SOUTH,L""},label,19,1.4f,Color(255,255,255,255),true,st==ButtonState::PRESSED);
    else txt(g,label,inset(pr,6),F(v,Role::BODY600,19),Color(255,150,146,146),StringAlignmentCenter,StringAlignmentCenter,false,1.4f);
    const bool show_over=v.page==PAGE_SAVE&&v.can_overwrite;
    const bool show_del=(v.page==PAGE_LOAD||v.page==PAGE_SAVE)&&v.can_delete;
    if(show_over&&show_del) {
        const ButtonState s2=state(v,HIT_SECONDARY);
        ui::plate(g,lay::SECONDARY_LEFT,false,s2);
        button_content(g,v,kind,lay::SECONDARY_LEFT,kb?Glyph{GlyphKind::KEY,'O',L""}:Glyph{GlyphKind::PAD,PB_WEST,L""},ts(v,cv_archives_i18n::OVERWRITE),16.5f,0.6f,Color(255,230,236,248),false,s2==ButtonState::PRESSED);
        const ButtonState sd=state(v,HIT_DELETE);
        delete_plate(g,lay::SECONDARY_RIGHT,sd);
        button_content(g,v,kind,lay::SECONDARY_RIGHT,kb?Glyph{GlyphKind::KEY,VK_DELETE,L"Del"}:Glyph{GlyphKind::PAD,PB_SELECT,L""},ts(v,cv_archives_i18n::DELETE_SAVE),16.5f,0.6f,Color(255,255,212,212),false,sd==ButtonState::PRESSED);
    } else if(show_over) {
        const ButtonState s2=state(v,HIT_SECONDARY);
        ui::plate(g,lay::SECONDARY,false,s2);
        button_content(g,v,kind,lay::SECONDARY,kb?Glyph{GlyphKind::KEY,'O',L""}:Glyph{GlyphKind::PAD,PB_WEST,L""},ts(v,cv_archives_i18n::OVERWRITE),18,1,Color(255,230,236,248),false,s2==ButtonState::PRESSED);
    } else if(show_del) {
        const ButtonState sd=state(v,HIT_DELETE);
        delete_plate(g,lay::SECONDARY,sd);
        button_content(g,v,kind,lay::SECONDARY,kb?Glyph{GlyphKind::KEY,VK_DELETE,L"Del"}:Glyph{GlyphKind::PAD,PB_WEST,L""},ts(v,cv_archives_i18n::DELETE_SAVE),18,1,Color(255,255,212,212),false,sd==ButtonState::PRESSED);
    }
    // The message under it: a result or refusal, else the page's own note (Maps: when it works).
    std::wstring msg=v.status;Tone tone=v.tone;
    const RectF mr=maps?lay::MESSAGE_MAPS:lay::MESSAGE;
    if(!msg.empty()) {
        const Color c=tone_color(tone);
        if(tone==Tone::WARN||tone==Tone::BAD)warn_icon(g,mr.X,mr.Y+1,tone==Tone::BAD?col::BAD:C_AMBER);
        txt(g,msg,RectF(mr.X+(tone==Tone::WARN||tone==Tone::BAD?30:0),mr.Y,mr.Width-(tone==Tone::WARN||tone==Tone::BAD?30:0),mr.Height),F(v,Role::BODY500,16.5f),c,StringAlignmentNear,StringAlignmentNear,true);
    }
}
void footer(Graphics& g,const View& v,int kind) {
    namespace pt=cv_port_ui_i18n;
    auto kbd=[](int vk){return Glyph{GlyphKind::KEY,vk,L""};};auto pad=[](int pb){return Glyph{GlyphKind::PAD,pb,L""};};
    const Glyph slash{GlyphKind::SLASH,0,L""};
    std::vector<ui::FooterGroup> groups={
        {{kbd('Q'),kbd('E')},{pad(PB_L1),pad(PB_R1)},ps(v,pt::TABS)},
        {{Glyph{GlyphKind::ARROWS,3,L""},slash,kbd(VK_PRIOR),kbd(VK_NEXT)},{Glyph{GlyphKind::DPAD,0,L""},slash,pad(PB_L2),pad(PB_R2)},ps(v,pt::SCROLL_LIST)},
        {{},{},ps(v,pt::HOLD_FAST),true},
        {{kbd(VK_ESCAPE)},{pad(PB_EAST)},ps(v,pt::BACK)},
    };
    std::vector<RectF> rects;
    int hover=-1,pressed=-1;const int back_id=hit_id(HIT_BACK);
    if(v.mouse.hover==back_id)hover=3;if(v.mouse.pressed==back_id&&hover==3)pressed=3;
    ui::draw_footer(g,lay::FOOTER,v.language,kind,groups,&rects,hover,pressed);
    v.back_rect=rects.size()>3?rects[3]:RectF(0,0,0,0);
}
void dialog(Graphics& g,const View& v) {
    SolidBrush dim(Color(158,3,6,14));g.FillRectangle(&dim,RectF(0,0,W,H));
    ui::gold_frame(g,lay::DIALOG);
    const auto title_id=v.confirm_delete?cv_archives_i18n::DELETE_TITLE:cv_archives_i18n::OVERWRITE;
    const auto text_id=v.confirm_delete?cv_archives_i18n::DELETE_QUESTION:cv_archives_i18n::QUESTION;
    ui::title_gold(g,ts(v,title_id),RectF(lay::DIALOG_TITLE.X,lay::DIALOG_TITLE.Y-15,lay::DIALOG_TITLE.Width,lay::DIALOG_TITLE.Height+30),v.language,32,1.5f,StringAlignmentCenter,StringAlignmentCenter);
    txt(g,ts(v,text_id),lay::DIALOG_TEXT,F(v,Role::BODY400,23),C_DETAIL,StringAlignmentCenter,StringAlignmentNear,true);
    ui::plate_label(g,lay::DIALOG_CANCEL,!v.confirm_yes,ts(v,cv_archives_i18n::CANCEL),v.language,24,1.5f,state(v,HIT_DIALOG_CANCEL));
    if(v.confirm_delete&&v.confirm_yes) {
        ui::primary_button_red(g,lay::DIALOG_YES,ts(v,cv_archives_i18n::YES),v.language,24,1.5f,state(v,HIT_DIALOG_YES));
    } else {
        ui::plate_label(g,lay::DIALOG_YES,v.confirm_yes,ts(v,cv_archives_i18n::YES),v.language,24,1.5f,state(v,HIT_DIALOG_YES));
    }
}
}  // namespace

std::vector<Hit> hits(const View& v) {
    std::vector<Hit> out;
    if(v.confirming){out.push_back({lay::DIALOG_CANCEL,HIT_DIALOG_CANCEL,0});out.push_back({lay::DIALOG_YES,HIT_DIALOG_YES,0});return out;}
    if(v.busy)return out;
    for(int i=0;i<3;i++)out.push_back({lay::tab(i),HIT_TAB,i});
    out.push_back({lay::TAB_PREV,HIT_TAB_PREV,0});out.push_back({lay::TAB_NEXT,HIT_TAB_NEXT,0});
    for(int slot=0;slot<int(v.rows.size())&&slot<ROWS_VISIBLE;slot++)out.push_back({lay::row(slot),HIT_ROW,v.scroll+slot});
    if(v.page==PAGE_MAPS){
        out.push_back({lay::STEP_MINUS,HIT_STEP_MINUS,0});out.push_back({lay::STEP_PLUS,HIT_STEP_PLUS,0});
    }
    if(v.primary_enabled)out.push_back({v.page==PAGE_MAPS?lay::PRIMARY_MAPS:lay::PRIMARY,HIT_PRIMARY,0});
    const bool show_over=v.page==PAGE_SAVE&&v.can_overwrite;
    const bool show_del=(v.page==PAGE_LOAD||v.page==PAGE_SAVE)&&v.can_delete;
    if(show_over&&show_del){out.push_back({lay::SECONDARY_LEFT,HIT_SECONDARY,0});out.push_back({lay::SECONDARY_RIGHT,HIT_DELETE,0});}
    else if(show_over)out.push_back({lay::SECONDARY,HIT_SECONDARY,0});
    else if(show_del)out.push_back({lay::SECONDARY,HIT_DELETE,0});
    if(v.back_rect.Width>0)out.push_back({v.back_rect,HIT_BACK,0});
    return out;
}
const Hit* hit_at(const std::vector<Hit>& list,float x,float y) {
    for(const auto& h:list)if(x>=h.rect.X&&x<h.rect.X+h.rect.Width&&y>=h.rect.Y&&y<h.rect.Y+h.rect.Height)return &h;
    return nullptr;
}

void release_caches() {
    thumb_cache.clear();
    mono_fonts.clear();
    mono_family.reset();
    thumb_scale_key=0;
}
void draw_backdrop(Graphics& g,const View& v) {
    g.SetSmoothingMode(SmoothingModeAntiAlias);g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
    g.SetPixelOffsetMode(PixelOffsetModeHalf);
    ui::gold_frame(g,RectF(0,0,W,H));
    header(g,v);
}
void draw_foreground(Graphics& g,const View& v) {
    g.SetSmoothingMode(SmoothingModeAntiAlias);g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
    g.SetInterpolationMode(InterpolationModeHighQualityBicubic);g.SetPixelOffsetMode(PixelOffsetModeHalf);
    const int kind=ui::hint_kind(v.input);
    tabs(g,v,kind);
    if(v.page==PAGE_MAPS)filter_and_now(g,v);else library_header(g,v);
    list(g,v);
    detail(g,v);
    if(v.page==PAGE_MAPS)entry_stepper(g,v,kind);
    buttons(g,v,kind);
    footer(g,v,kind);
    if(v.confirming)dialog(g,v);
}
void draw(Graphics& g,const View& v) {
    draw_backdrop(g,v);
    draw_foreground(g,v);
}
}
