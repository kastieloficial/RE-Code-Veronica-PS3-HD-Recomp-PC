// The return-to-title box's drawing: see cv_title_return_ui.h. Values come from docs/mockup-inventario/gen2.py (titulo) + arq.css + mock.css.
#include "cv_title_return_ui.h"
#include "cv_title_return_i18n.h"
#include "cv_port_ui_i18n.h"
#include <algorithm>
#include <cmath>
#include <memory>
#pragma comment(lib,"gdiplus.lib")
using namespace Gdiplus;

namespace cv_title_return_ui {
namespace {
namespace ui=cv_port_ui;
namespace col=cv_port_ui::col;
namespace lay=layout;
using ui::ButtonState;using ui::Role;
using ui::Glyph;using ui::GlyphKind;

const wchar_t* ts(const View& v,cv_title_return_i18n::Id id) {return cv_title_return_i18n::tr(v.language,id);}
const Font& F(const View& v,Role role,float px) {return ui::font_cached(v.language,role,px);}
const Color C_EYEBROW(255,159,168,192),C_LABEL(255,201,208,226),C_LABEL_HOVER(255,230,236,248);
constexpr float TRACK_EYEBROW=3,TRACK_QUESTION=1.5f,TRACK_LABEL=1.5f;

ButtonState state(const View& v,int id) {return v.mouse.state(id);}

// ui::text with a line box tall enough for the language's face: GDI+ drops a line that does not fit its rectangle (LineLimit),
// and the fallback faces of Arabic, Japanese, Chinese and Korean are taller than Barlow.
void txt(Graphics& g,const std::wstring& s,RectF r,const Font& f,Color c,StringAlignment h=StringAlignmentNear,StringAlignment v=StringAlignmentNear,bool wrap=false,float tracking=0) {
    if(!wrap&&v==StringAlignmentCenter){const float need=f.GetSize()*1.8f;if(r.Height<need){r.Y-=(need-r.Height)/2;r.Height=need;}}
    ui::text(g,s,r,f,c,h,v,wrap,tracking);
}
// The largest size from `start` down to Metrics::MIN_PX at which the text fits `max_w`; `width` receives the width at that size.
float fit(Graphics& g,const std::wstring& s,const View& v,Role role,float start,float max_w,float tracking,float& width) {
    float px=start;
    for(;;){width=ui::text_width(g,s,F(v,role,px),tracking);if(width<=max_w||px<=Metrics::MIN_PX)break;px=std::max(Metrics::MIN_PX,px-1);}
    return px;
}
// "CODE: VERONICA · GAME PAUSED" is drawn in two parts around a gold dot; a text without the separator is one part.
struct Eyebrow {std::wstring left,right;bool split=false;};
Eyebrow split_header(const std::wstring& s) {
    const size_t at=s.find(L" · ");
    if(at==std::wstring::npos)return {s,L"",false};
    return {s.substr(0,at),s.substr(at+3),true};
}
constexpr float DOT_MARGIN=8;
float eyebrow_width(Graphics& g,const View& v,const Eyebrow& e,float px) {
    const Font& f=F(v,Role::BODY600,px);
    if(!e.split)return ui::text_width(g,e.left,f,TRACK_EYEBROW);
    return ui::text_width(g,e.left,f,TRACK_EYEBROW)+2*DOT_MARGIN+ui::text_width(g,L"·",f,TRACK_EYEBROW)+ui::text_width(g,e.right,f,TRACK_EYEBROW);
}

RectF warning_text_rect() {   // the box: 4px bar, 18px padding, the 30px mark, 14px gap, 18px padding
    const RectF r=lay::WARNING;const float x=r.X+4+18+30+14;return RectF(x,r.Y,r.X+r.Width-18-x,r.Height);
}
float label_max_width() {return lay::YES.Width-2*14-28;}   // padding, and the room of the arrow that marks the chosen button

// ---- Pieces ----------------------------------------------------------------------
void header(Graphics& g,const View& v,const Metrics& m) {
    ui::header_band(g,lay::BAND);
    const Eyebrow e=split_header(ts(v,cv_title_return_i18n::HEADER));
    const Font& f=F(v,Role::BODY600,m.header_px);
    const float cy=lay::BAND.Y+lay::BAND.Height/2,h=22;
    float x=(W-m.header_w)/2;
    const float lw=ui::text_width(g,e.left,f,TRACK_EYEBROW);
    txt(g,e.left,RectF(x,cy-h/2,lw+6,h),f,C_EYEBROW,StringAlignmentNear,StringAlignmentCenter,false,TRACK_EYEBROW);
    if(e.split){
        x+=lw+DOT_MARGIN;
        const float dw=ui::text_width(g,L"·",f,TRACK_EYEBROW);
        txt(g,L"·",RectF(x,cy-h/2,dw+6,h),f,col::GOLD,StringAlignmentNear,StringAlignmentCenter,false,TRACK_EYEBROW);
        x+=dw+DOT_MARGIN;
        txt(g,e.right,RectF(x,cy-h/2,m.header_w-(x-(W-m.header_w)/2)+6,h),f,C_EYEBROW,StringAlignmentNear,StringAlignmentCenter,false,TRACK_EYEBROW);
    }
}
void warning(Graphics& g,const View& v,const Metrics& m) {
    const auto saved=g.Save();g.SetSmoothingMode(SmoothingModeAntiAlias);
    const RectF r=lay::WARNING;
    {   // .warnbox: amber wash fading to the right, a thin dark-amber border and the 4px amber bar
        LinearGradientBrush fill(r,Color(41,240,180,76),Color(10,240,180,76),LinearGradientModeHorizontal);
        auto path=ui::round_path(r,2);g.FillPath(&fill,path.get());
        Pen border(Color(255,122,90,30),1);auto edge=ui::round_path(RectF(r.X+.5f,r.Y+.5f,r.Width-1,r.Height-1),2);g.DrawPath(&border,edge.get());
        SolidBrush bar(col::WARN);g.FillRectangle(&bar,r.X,r.Y,4.f,r.Height);
    }
    {   // the "!" mark: a 30px amber disc
        const RectF d(r.X+4+18,r.Y+(r.Height-30)/2,30,30);SolidBrush disc(col::WARN);g.FillEllipse(&disc,d);
        txt(g,L"!",d,F(v,Role::BODY700,21),col::INK,StringAlignmentCenter,StringAlignmentCenter);
    }
    txt(g,ts(v,cv_title_return_i18n::WARNING),warning_text_rect(),F(v,Role::BODY600,m.warning_px),col::WARN,StringAlignmentNear,StringAlignmentCenter,false);
    g.Restore(saved);
}
void arrow_mark(Graphics& g,float x,float cy,Color c) {   // the 16x18 triangle that marks the chosen button
    const auto saved=g.Save();g.SetSmoothingMode(SmoothingModeAntiAlias);
    SolidBrush b(c);const PointF p[3]={PointF(x+3,cy-7.5f),PointF(x+14,cy),PointF(x+3,cy+7.5f)};g.FillPolygon(&b,p,3);g.Restore(saved);
}
void choice(Graphics& g,const View& v,const Metrics& m,RectF r,bool chosen,const std::wstring& label,float label_w,int id) {
    const ButtonState st=state(v,id);
    ui::plate(g,r,chosen,st);
    if(st==ButtonState::PRESSED)r=RectF(r.X+1,r.Y+1,r.Width-2,r.Height-2);
    const float arrow=chosen?16.f+12.f:0.f,total=arrow+label_w,x=r.X+(r.Width-total)/2,cy=r.Y+r.Height/2;
    if(chosen)arrow_mark(g,x,cy,col::INK);
    const Color c=chosen?col::INK:st==ButtonState::HOVER?C_LABEL_HOVER:C_LABEL;
    txt(g,label,RectF(x+arrow,cy-16,label_w+8,32),F(v,Role::BODY600,m.label_px),c,StringAlignmentNear,StringAlignmentCenter,false,TRACK_LABEL);
}
void footer(Graphics& g,const View& v,int kind) {
    namespace pt=cv_port_ui_i18n;
    const std::vector<ui::FooterGroup> groups={
        {{Glyph{GlyphKind::ARROWS,4|8,L""},Glyph{GlyphKind::SLASH,0,L""},Glyph{GlyphKind::KEY,'A',L""},Glyph{GlyphKind::KEY,'D',L""}},{Glyph{GlyphKind::DPAD,0,L""}},pt::strings[std::clamp(v.language,0,12)][pt::CHOOSE]},
        {{Glyph{GlyphKind::KEY,VK_RETURN,L""}},{Glyph{GlyphKind::PAD,PB_SOUTH,L""}},pt::strings[std::clamp(v.language,0,12)][pt::CONFIRM]},
        {{Glyph{GlyphKind::KEY,VK_ESCAPE,L""}},{Glyph{GlyphKind::PAD,PB_EAST,L""}},pt::strings[std::clamp(v.language,0,12)][pt::BACK]},
    };
    std::vector<RectF> rects;
    int hover=-1,pressed=-1;
    if(v.mouse.hover==HIT_BACK)hover=2;if(v.mouse.pressed==HIT_BACK&&hover==2)pressed=2;
    ui::draw_footer(g,lay::FOOTER,v.language,kind,groups,&rects,hover,pressed);
    v.back_rect=rects.size()>2?rects[2]:RectF(0,0,0,0);
}
}  // namespace

Metrics measure(Graphics& g,const View& v) {
    Metrics m;
    const Eyebrow e=split_header(ts(v,cv_title_return_i18n::HEADER));
    m.header_max=W-60;
    for(m.header_px=15;;m.header_px=std::max(Metrics::MIN_PX,m.header_px-1)){m.header_w=eyebrow_width(g,v,e,m.header_px);if(m.header_w<=m.header_max||m.header_px<=Metrics::MIN_PX)break;}
    m.question_max=lay::QUESTION.Width;
    m.question_px=fit(g,ts(v,cv_title_return_i18n::QUESTION),v,Role::TITLE,36,m.question_max,TRACK_QUESTION,m.question_w);
    m.warning_max=warning_text_rect().Width;
    m.warning_px=fit(g,ts(v,cv_title_return_i18n::WARNING),v,Role::BODY600,20,m.warning_max,0,m.warning_w);
    m.label_max=label_max_width();
    // Both buttons share a size: the one that fits the longer label.
    float yw=0,nw=0;
    const float a=fit(g,ts(v,cv_title_return_i18n::YES),v,Role::BODY600,24,m.label_max,TRACK_LABEL,yw);
    const float b=fit(g,ts(v,cv_title_return_i18n::NO),v,Role::BODY600,24,m.label_max,TRACK_LABEL,nw);
    m.label_px=std::min(a,b);
    m.yes_w=ui::text_width(g,ts(v,cv_title_return_i18n::YES),F(v,Role::BODY600,m.label_px),TRACK_LABEL);
    m.no_w=ui::text_width(g,ts(v,cv_title_return_i18n::NO),F(v,Role::BODY600,m.label_px),TRACK_LABEL);
    return m;
}

std::vector<Hit> hits(const View& v) {
    std::vector<Hit> out;
    out.push_back({lay::YES,HIT_YES});out.push_back({lay::NO,HIT_NO});
    if(v.back_rect.Width>0)out.push_back({v.back_rect,HIT_BACK});
    return out;
}
const Hit* hit_at(const std::vector<Hit>& list,float x,float y) {
    const Hit* found=nullptr;
    for(const auto& h:list)if(x>=h.rect.X&&x<h.rect.X+h.rect.Width&&y>=h.rect.Y&&y<h.rect.Y+h.rect.Height)found=&h;
    return found;
}

void draw(Graphics& g,const View& v) {
    g.SetSmoothingMode(SmoothingModeAntiAlias);g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
    g.SetInterpolationMode(InterpolationModeHighQualityBicubic);g.SetPixelOffsetMode(PixelOffsetModeHalf);
    const int kind=ui::hint_kind(v.input);
    const Metrics m=measure(g,v);
    ui::gold_frame(g,RectF(0,0,W,H));
    header(g,v,m);
    // The question in Cinzel gold; a tall box because the fallback faces are tall.
    ui::title_gold(g,ts(v,cv_title_return_i18n::QUESTION),RectF(lay::QUESTION.X,lay::QUESTION.Y-15,lay::QUESTION.Width,lay::QUESTION.Height+30),v.language,m.question_px,TRACK_QUESTION,StringAlignmentCenter,StringAlignmentCenter);
    warning(g,v,m);
    choice(g,v,m,lay::YES,v.selected==CHOICE_YES,ts(v,cv_title_return_i18n::YES),m.yes_w,HIT_YES);
    choice(g,v,m,lay::NO,v.selected==CHOICE_NO,ts(v,cv_title_return_i18n::NO),m.no_w,HIT_NO);
    footer(g,v,kind);
}
}
