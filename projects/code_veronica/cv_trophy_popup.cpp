#define NOMINMAX
#include "cv_trophy_popup.h"
#include "cv_trophy_i18n.h"
#include "cv_port_ui.h"
#include <windows.h>
#include <mmsystem.h>
#include <gdiplus.h>
#include <objidl.h>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <cstring>
#pragma comment(lib,"gdiplus.lib")
#pragma comment(lib,"winmm.lib")
extern "C" const rsx_ui_sprite* (*g_rsx_trophy_hook)(uint32_t,uint32_t,const uint32_t**,uint32_t*,uint32_t*,uint64_t*);
// The achievement card (docs/PLANO-MELHORIAS-v1.7.md section 2, docs/mockup-voltar-titulo/mock.css ".ach"): 640x112 design units, drawn
// at the output's own scale (never a 1080p bitmap stretched), the trophy's own picture as a square with slightly rounded corners and a
// tier-coloured border, and a 2px line along the bottom edge that shrinks with the time left.
// The atlas the renderer gets holds the card (with a soft shadow all around it, `pad` pixels) and, under it, a solid 4-row strip the
// renderer draws as the progress line; frame_at returns two sprites (see rsx_ui_batch.h).
namespace cv_trophy_popup {
namespace {
using namespace Gdiplus;
constexpr float design_w=640,design_h=112,design_margin=32,min_scale=.3f,max_scale=1.5f;
constexpr unsigned strip_rows=4;
constexpr uint64_t enter_ms=280,hold_ms=5000,exit_ms=300,total_ms=enter_ms+hold_ms+exit_ms;
struct Card {std::vector<uint32_t> rgba;unsigned w=0,h=0,pad=0;float scale=1;uint64_t generation=0;};
// One toast: what to draw (kept so that it can be drawn again at another scale) and the picture made for it.
struct Entry {cv_trophies::Trophy trophy;int language=1;std::shared_ptr<Card> card;};
struct State {
 std::mutex lock;std::deque<std::shared_ptr<Entry>> queue;std::shared_ptr<Entry> active;std::vector<std::shared_ptr<Card>> retired;
 std::atomic<int> position{1};std::atomic<bool> show{true},work{false};std::atomic<uint64_t> canceled{0};uint64_t observed_cancel=0,start=0,next_generation=1;
 std::atomic<float> output_scale{0};   // written by frame_at (the render thread), read by whoever prepares a card
 ULONG_PTR gdip=0;HANDLE sound_event=nullptr;void(*sound_observer)()=nullptr;rsx_ui_sprite sprites[2]{};
 std::mutex render_lock;   // one picture at a time (GDI+ text, the shared fonts)
 std::mutex job_lock;std::condition_variable job_cv;std::shared_ptr<Entry> job_entry;float job_scale=0;std::atomic<bool> job_pending{false};bool worker=false;
};
// The renderer and optional sound thread live for the lifetime of the process.
State& state(){static State* instance=new State;return *instance;}
DWORD WINAPI sound_thread(void* parameter){
 auto& s=*static_cast<State*>(parameter);
 while(WaitForSingleObject(s.sound_event,INFINITE)==WAIT_OBJECT_0)
  if(s.show.load())PlaySoundW(L"TROPHY_SOUND",GetModuleHandleW(nullptr),SND_RESOURCE|SND_ASYNC|SND_NODEFAULT);
 return 0;
}
std::wstring upper(const wchar_t* text,int language){
 const wchar_t* locale=language==8?L"tr-TR":LOCALE_NAME_INVARIANT;
 const int n=LCMapStringEx(locale,LCMAP_UPPERCASE|LCMAP_LINGUISTIC_CASING,text,-1,nullptr,0,nullptr,nullptr,0);
 if(n<=1)return text;std::wstring out(size_t(n),L'\0');
 if(!LCMapStringEx(locale,LCMAP_UPPERCASE|LCMAP_LINGUISTIC_CASING,text,-1,out.data(),n,nullptr,nullptr,0))return text;
 out.resize(size_t(n-1));return out;
}
struct Tier {Color top,bottom,text;const wchar_t* name;};
Tier tier_of(int grade,int language){
 using cv_trophy_text::get;
 switch(grade){
  case 1:return {Color(255,0xCD,0xEB,0xF3),Color(255,0x6F,0xA9,0xBC),Color(255,0xCD,0xEB,0xF3),get(language,cv_trophy_text::Platinum)};
  case 2:return {Color(255,0xF3,0xD2,0x7A),Color(255,0xB0,0x7A,0x2A),Color(255,0xF3,0xD2,0x7A),get(language,cv_trophy_text::Gold)};
  case 3:return {Color(255,0xEE,0xF1,0xF5),Color(255,0x8C,0x93,0x9E),Color(255,0xEE,0xF1,0xF5),get(language,cv_trophy_text::Silver)};
  default:return {Color(255,0xE0,0xA0,0x70),Color(255,0x8A,0x55,0x30),Color(255,0xE0,0xA0,0x70),get(language,cv_trophy_text::Bronze)};
 }
}
// One line of text centred on `cy` in a box 1.8 font heights tall: a fallback face (Japanese, Arabic...) is taller than Barlow and
// would otherwise be clipped to nothing in the CSS line box.
RectF line_box(Graphics& g,const Font& f,float x,float cy,float w){const float h=std::max(1.8f*f.GetHeight(&g),10.f);return RectF(x,cy-h/2,w,h);}
std::unique_ptr<Bitmap> decode(const std::vector<unsigned char>& bytes){
 if(bytes.empty())return nullptr;
 HGLOBAL block=GlobalAlloc(GMEM_MOVEABLE,bytes.size());if(!block)return nullptr;
 void* p=GlobalLock(block);if(!p){GlobalFree(block);return nullptr;}
 memcpy(p,bytes.data(),bytes.size());GlobalUnlock(block);
 IStream* input=nullptr;if(FAILED(CreateStreamOnHGlobal(block,TRUE,&input))){GlobalFree(block);return nullptr;}
 // The decoded copy is independent of the stream, which must outlive the source bitmap.
 std::unique_ptr<Bitmap> out;
 {Bitmap source(input);
  if(source.GetLastStatus()==Ok&&source.GetWidth()&&source.GetHeight()){out=std::make_unique<Bitmap>(int(source.GetWidth()),int(source.GetHeight()),PixelFormat32bppARGB);
   Graphics c(out.get());c.SetCompositingMode(CompositingModeSourceCopy);c.DrawImage(&source,0,0,int(source.GetWidth()),int(source.GetHeight()));}}
 input->Release();return out;
}
// Soft shadow around (not under) the card, like "box-shadow: 0 10px 30px rgba(0,0,0,.55)" but within `pad` design units.
void card_shadow(Graphics& g,GraphicsPath& body,float reach){
 const auto state=g.Save();g.SetClip(&body,CombineModeExclude);
 for(int k=int(reach);k>=1;k--){const float e=float(k);SolidBrush b(Color(BYTE(std::lround(9.f*(1.f-e/(reach+1)))+3),0,0,0));
  auto p=cv_port_ui::round_path(RectF(-e,-e+4,design_w+2*e,design_h+2*e),6+e);g.FillPath(&b,p.get());}
 g.Restore(state);
}
std::shared_ptr<Card> prepare(const cv_trophies::Trophy& trophy,int language,float scale){
 auto& s=state();if(!s.gdip)return {};
 std::lock_guard<std::mutex> one_at_a_time(s.render_lock);
 scale=std::clamp(scale,min_scale,max_scale);
 const unsigned pad=unsigned(std::ceil(10*scale)),body_w=unsigned(std::lround(design_w*scale)),body_h=unsigned(std::lround(design_h*scale));
 const unsigned W=body_w+2*pad,H=body_h+2*pad+strip_rows;
 Bitmap bitmap(int(W),int(H),PixelFormat32bppARGB);if(bitmap.GetLastStatus()!=Ok)return {};
 Graphics g(&bitmap);const float sx=float(body_w)/design_w,sy=float(body_h)/design_h;
 g.SetSmoothingMode(SmoothingModeAntiAlias);g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);g.SetPixelOffsetMode(PixelOffsetModeHalf);g.Clear(Color(0,0,0,0));
 g.TranslateTransform(float(pad),float(pad));g.ScaleTransform(sx,sy);   // from here on: design units
 auto body=cv_port_ui::round_path(RectF(0,0,design_w,design_h),6);
 card_shadow(g,*body,float(pad)/sx-4);
 // Card: translucent dark fill, the red bar and the faint top line (clipped to the rounded shape), then the 1px border over all.
 {SolidBrush bg(Color(240,21,17,17));g.FillPath(&bg,body.get());}
 {const auto state=g.Save();g.SetClip(body.get(),CombineModeIntersect);
  SolidBrush bar(Color(255,0xC4,0x1C,0x22)),line(Color(255,0x46,0x1C,0x1C));g.FillRectangle(&bar,RectF(0,0,5,design_h));g.FillRectangle(&line,RectF(5,1,design_w-5,1));g.Restore(state);}
 {auto edge=cv_port_ui::round_path(RectF(.5f,.5f,design_w-1,design_h-1),5.5f);Pen border(Color(255,0x46,0x3C,0x3A),1.f);g.DrawPath(&border,edge.get());}
 // Icon: the trophy's own picture, complete, in a rounded square (radius 9.6) with a 2px tier gradient border and a shadow.
 const Tier tier=tier_of(trophy.grade,language);const RectF slot(20,16,80,80);
 for(int k=5;k>=1;k--){SolidBrush b(Color(BYTE(7+(5-k)*2),0,0,0));auto p=cv_port_ui::round_path(RectF(slot.X-k,slot.Y-k+4,slot.Width+2*k,slot.Height+2*k),9.6f+k);g.FillPath(&b,p.get());}
 {SolidBrush b(Color(70,0,0,0));auto p=cv_port_ui::round_path(RectF(slot.X,slot.Y+1,slot.Width,slot.Height),9.6f);g.FillPath(&b,p.get());}
 {LinearGradientBrush ring(slot,tier.top,tier.bottom,LinearGradientModeVertical);auto p=cv_port_ui::round_path(slot,9.6f);g.FillPath(&ring,p.get());}
 {const RectF inner(slot.X+2,slot.Y+2,slot.Width-4,slot.Height-4);auto p=cv_port_ui::round_path(inner,7.6f);
  auto icon=decode(trophy.icon);
  if(icon){
   // Scaled once to the pixels it will occupy (bicubic), then laid as a texture on the rounded shape: the fill is anti-aliased, a clip would not be.
   const int px=std::max(4,int(std::lround(inner.Width*sx)));Bitmap fit(px,px,PixelFormat32bppARGB);
   {Graphics f(&fit);f.SetInterpolationMode(InterpolationModeHighQualityBicubic);f.SetPixelOffsetMode(PixelOffsetModeHalf);f.Clear(Color(0,0,0,0));
    ImageAttributes edge;edge.SetWrapMode(WrapModeTileFlipXY);f.DrawImage(icon.get(),Rect(0,0,px,px),0,0,int(icon->GetWidth()),int(icon->GetHeight()),UnitPixel,&edge);}
   TextureBrush tex(&fit,WrapModeClamp);tex.ScaleTransform(inner.Width/float(px),inner.Height/float(px),MatrixOrderAppend);tex.TranslateTransform(inner.X,inner.Y,MatrixOrderAppend);g.FillPath(&tex,p.get());
  }else{SolidBrush b(Color(255,32,26,26));g.FillPath(&b,p.get());}
 }
 // Text: label (the toast words and the grade), name and description, left edge x=120. Right-to-left languages are right-aligned.
 const bool rtl=language==9;const float x=120,w=design_w-24-x,right=x+w;
 const Font& label_font=cv_port_ui::font_cached(language,cv_port_ui::Role::BODY600,15);
 const Font& name_font=cv_port_ui::font_cached(language,cv_port_ui::Role::TITLE,27);
 const Font& desc_font=cv_port_ui::font_cached(language,cv_port_ui::Role::BODY400,19);
 const Color label_color(255,0xE2,0xAC,0x52);
 {const std::wstring head=upper(cv_trophy_text::get(language,cv_trophy_text::Toast),language),grade=L" · "+upper(tier.name,language);
  const RectF box=line_box(g,label_font,x,27,w);float tracking=rtl?0.f:2.5f;   // no letter-spacing in Arabic: it breaks the joined letters
  float head_w=cv_port_ui::text_width(g,head,label_font,tracking),grade_w=cv_port_ui::text_width(g,grade,label_font,tracking);
  if(head_w+tracking+grade_w>w){tracking=0;head_w=cv_port_ui::text_width(g,head,label_font,0);grade_w=cv_port_ui::text_width(g,grade,label_font,0);}
  const float gap=tracking;   // the letter-spacing after the last letter
  using cv_port_ui::text;
  if(!rtl){
   text(g,head,box,label_font,label_color,StringAlignmentNear,StringAlignmentCenter,false,tracking);
   text(g,grade,RectF(x+head_w+gap,box.Y,std::max(10.f,w-head_w-gap),box.Height),label_font,tier.text,StringAlignmentNear,StringAlignmentCenter,false,tracking);
  }else{
   text(g,head,box,label_font,label_color,StringAlignmentFar,StringAlignmentCenter,false,0);
   const float gx=right-head_w-4;text(g,grade,RectF(x,box.Y,std::max(10.f,gx-x),box.Height),label_font,tier.text,StringAlignmentFar,StringAlignmentCenter,false,0);
  }
 }
 {using cv_port_ui::text;const auto align=rtl?StringAlignmentFar:StringAlignmentNear;
  // GDI+'s default string format leaves 1/6 em of padding at the aligned edge: take it back so the text starts exactly at x (as the label does).
  auto at=[&](const Font& f,float cy){RectF r=line_box(g,f,x,cy,w);const float pad=f.GetSize()/6;if(rtl)r.Width+=pad;else{r.X-=pad;r.Width+=pad;}return r;};
  text(g,trophy.name,at(name_font,58),name_font,Color(255,0xF2,0xEA,0xE0),align,StringAlignmentCenter,false,0);
  text(g,trophy.description,at(desc_font,89),desc_font,Color(255,0xB8,0xAD,0xA4),align,StringAlignmentCenter,false,0);}
 // The progress line's strip: solid #C41C22, the last rows of the atlas.
 g.ResetTransform();g.SetCompositingMode(CompositingModeSourceCopy);{SolidBrush red(Color(255,0xC4,0x1C,0x22));g.FillRectangle(&red,0,int(H-strip_rows),int(W),int(strip_rows));}
 g.Flush(FlushIntentionSync);
 BitmapData pixels{};Rect rect(0,0,int(W),int(H));if(bitmap.LockBits(&rect,ImageLockModeRead,PixelFormat32bppARGB,&pixels)!=Ok)return {};
 auto card=std::make_shared<Card>();card->rgba.resize(size_t(W)*H);card->w=W;card->h=H;card->pad=pad;card->scale=scale;
 for(unsigned y=0;y<H;y++){auto* src=reinterpret_cast<const uint32_t*>(static_cast<const unsigned char*>(pixels.Scan0)+ptrdiff_t(y)*pixels.Stride);
  for(unsigned px=0;px<W;px++){uint32_t c=src[px];card->rgba[size_t(y)*W+px]=(c&0xff00ff00)|((c&255)<<16)|((c>>16)&255);}}
 bitmap.UnlockBits(&pixels);return card;
}
// Re-draws the toast on screen at a new scale, off the render thread (which only ever draws what is ready).
void worker_main(State* s){
 for(;;){
  std::shared_ptr<Entry> entry;float scale=0;
  {std::unique_lock<std::mutex> l(s->job_lock);s->job_cv.wait(l,[&]{return s->job_entry!=nullptr;});entry=std::move(s->job_entry);s->job_entry=nullptr;scale=s->job_scale;}
  auto card=prepare(entry->trophy,entry->language,scale);
  {std::lock_guard<std::mutex> g(s->lock);
   if(card&&s->active==entry){card->generation=s->next_generation++;s->retired.push_back(std::move(entry->card));entry->card=std::move(card);}}
  s->job_pending=false;
 }
}
void request_redraw(State& s,const std::shared_ptr<Entry>& entry,float scale){
 if(s.job_pending.exchange(true))return;
 {std::lock_guard<std::mutex> l(s.job_lock);
  if(!s.worker){s.worker=true;std::thread(worker_main,&s).detach();}
  s.job_entry=entry;s.job_scale=scale;}
 s.job_cv.notify_one();
}
const rsx_ui_sprite* frame(uint32_t w,uint32_t h,const uint32_t** rgba,uint32_t* aw,uint32_t* ah,uint64_t* generation){return frame_at(w,h,GetTickCount64(),rgba,aw,ah,generation);}
float smooth(float x){x=std::clamp(x,0.f,1.f);return x*x*(3-2*x);}
}
void initialize(){
 static std::once_flag initialized;std::call_once(initialized,[]{auto& s=state();GdiplusStartupInput input;GdiplusStartup(&s.gdip,&input,nullptr);
  s.sound_event=CreateEventW(nullptr,FALSE,FALSE,nullptr);if(s.sound_event){HANDLE thread=CreateThread(nullptr,0,sound_thread,&s,0,nullptr);if(thread)CloseHandle(thread);else{CloseHandle(s.sound_event);s.sound_event=nullptr;}}
  g_rsx_trophy_hook=frame;
 });
}
void corner(int position){state().position=std::clamp(position,0,3);}
void enabled(bool show){auto& s=state();s.show=show;if(!show){std::lock_guard<std::mutex> guard(s.lock);s.queue.clear();s.canceled.fetch_add(1);s.work=false;}}
void enqueue(const cv_trophies::Trophy& trophy,int language){
 initialize();auto& s=state();if(!s.show.load())return;
 float scale=s.output_scale.load();if(scale<=0)scale=1;   // before the first frame the output size is unknown: frame_at redraws if it differs
 auto card=prepare(trophy,language,scale);if(!card)return;
 auto entry=std::make_shared<Entry>();entry->trophy=trophy;entry->language=language;entry->card=std::move(card);
 std::lock_guard<std::mutex> guard(s.lock);if(!s.show.load()||s.queue.size()>=128)return;entry->card->generation=s.next_generation++;s.queue.push_back(std::move(entry));s.work=true;
}
const rsx_ui_sprite* frame_at(uint32_t w,uint32_t h,uint64_t now,const uint32_t** rgba,uint32_t* aw,uint32_t* ah,uint64_t* generation){
 *rgba=nullptr;*aw=*ah=0;*generation=0;auto& s=state();if(!w||!h)return nullptr;
 // Target scale: the output's height against 1080 (a narrow window shrinks it so the card and its margins fit). Kept even while idle,
 // so that the next toast is drawn for the current output from the start.
 const float scale=std::max(min_scale,std::min(std::clamp(float(h)/1080,.65f,max_scale),float(w)/(design_w+2*design_margin)));
 s.output_scale.store(scale);
 if(!s.show.load()||!s.work.load())return nullptr;
 std::lock_guard<std::mutex> guard(s.lock);
 s.retired.clear();   // the pictures a redraw replaced: the previous call's upload is over
 if(s.observed_cancel!=s.canceled.load()){s.active.reset();s.observed_cancel=s.canceled.load();}
 if(s.active&&now-s.start>=total_ms)s.active.reset();
 if(!s.active){if(s.queue.empty()){s.work=false;return nullptr;}s.active=s.queue.front();s.queue.pop_front();s.start=now;
  if(s.sound_observer)s.sound_observer();else if(s.sound_event)SetEvent(s.sound_event);
 }
 const Card& card=*s.active->card;
 if(std::fabs(scale/card.scale-1)>.03f)request_redraw(s,s.active,scale);   // meanwhile this one is drawn stretched
 const float ratio=scale/card.scale;
 const uint64_t age=now-s.start;float alpha=age<enter_ms?smooth(float(age)/enter_ms):age>enter_ms+hold_ms?1-smooth(float(age-enter_ms-hold_ms)/exit_ms):1;
 const int position=s.position.load();const bool right=(position&1)!=0,bottom=(position&2)!=0;
 const float body_w=design_w*scale,body_h=design_h*scale,margin=design_margin*scale;
 const float bx=std::round((right?float(w)-body_w-margin:margin)+(right?1.f:-1.f)*(1-alpha)*78*scale),by=std::round(bottom?float(h)-body_h-margin:margin);
 const uint32_t argb=(uint32_t(std::lround(alpha*255))<<24)|0xffffff;
 const float aw_px=float(card.w),ah_px=float(card.h-strip_rows);
 s.sprites[0]={std::round(bx-card.pad*ratio),std::round(by-card.pad*ratio),aw_px*ratio,ah_px*ratio,0,0,1,ah_px/float(card.h),argb,1};
 // Progress line: from the end of the enter animation the card shows how much of its time is left.
 const float left=std::clamp(1-float(age)/float(enter_ms+hold_ms),0.f,1.f);
 const float line_w=std::floor(left*(design_w-8)*scale),line_h=std::max(1.f,std::round(2*scale));
 if(line_w>=1){
  s.sprites[1]={std::round(bx+5*scale),std::round(by+body_h-line_h),line_w,line_h,.25f,float(card.h-strip_rows+1)/float(card.h),.75f,float(card.h-1)/float(card.h),argb,2};
  s.sprites[0].after=2;
 }
 *rgba=card.rgba.data();*aw=card.w;*ah=card.h;*generation=card.generation;return s.sprites;
}
void sound_observer_for_test(void(*observer)()){auto& s=state();std::lock_guard<std::mutex> guard(s.lock);s.sound_observer=observer;}
}
