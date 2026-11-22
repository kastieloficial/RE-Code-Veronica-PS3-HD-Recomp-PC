// The achievement card: timing, queue and sound (unchanged), the card drawn at the output's own scale, the progress line as a second
// sprite, the redraw when the output size changes, and every trophy in the launcher's 13 languages.
// Usage: test <game folder> <storage folder> [preview folder [background image [folder of TROP%03d.PNG]]]. Without the game's trophy package
// (TROPDIR/.../TROPHY.TRP) the 12 trophies are built from the port's own translation table, with the PNGs of the last argument or a drawn
// stand-in. With a preview folder it writes PNGs to compare with
// docs/previas-v1.7/conquista-square-*.png: toasts-<lang>.png (three tiers on a 1920x1080 scene), tiers-bright.png, card-<lang>.png.
// GdiplusShutdown is never called (the keycap cache crash) and the process exits right after the last check.
#include "../cv_trophy_popup.cpp"
#include "../cv_trophies.cpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include "../cv_trophy_translations.h"
extern "C" {
const rsx_ui_sprite* (*g_rsx_trophy_hook)(uint32_t,uint32_t,const uint32_t**,uint32_t*,uint32_t*,uint64_t*)=nullptr;
int pad_phys_360_layout(int) {return 0;}   // cv_port_ui::menu_pad
int pad_last_input(void) {return 0;}
}
std::wstring cv_key_cap_label(int vk) {return vk>0?std::wstring(1,wchar_t(vk)):std::wstring();}   // button_icons.cpp: the footer's keycaps (not drawn here)
static unsigned sounds=0;
static void heard(){sounds++;}
struct Canvas {unsigned w,h;std::vector<uint32_t> px;Canvas(unsigned W,unsigned H,uint32_t c):w(W),h(H),px(size_t(W)*H,c){}};   // 0xAARRGGBB
static void save_canvas(const Canvas& c,const std::wstring& name){
 using namespace Gdiplus;Bitmap b(c.w,c.h,PixelFormat32bppARGB);BitmapData data{};Rect r(0,0,int(c.w),int(c.h));assert(b.LockBits(&r,ImageLockModeWrite,PixelFormat32bppARGB,&data)==Ok);
 for(unsigned y=0;y<c.h;y++)memcpy(static_cast<unsigned char*>(data.Scan0)+size_t(y)*data.Stride,&c.px[size_t(y)*c.w],size_t(c.w)*4);
 b.UnlockBits(&data);CLSID png;assert(CLSIDFromString(L"{557CF406-1A04-11D3-9A73-0000F81EF32E}",&png)==S_OK);assert(b.Save(name.c_str(),&png,nullptr)==Ok);
}
// What the renderer does with the sprites: each quad samples its u0..v1 rectangle of the atlas (nearest here), straight alpha times the sprite's alpha.
static void draw_sprites(Canvas& c,const rsx_ui_sprite* sp,const uint32_t* rgba,unsigned aw,unsigned ah,float dx,float dy){
 const unsigned n=sp[0].after<1?1:sp[0].after>2?2:sp[0].after;
 for(unsigned i=0;i<n;i++){const auto& q=sp[i];const float a_mul=float(q.argb>>24)/255.f;
  for(int y=int(std::lround(q.y+dy));y<int(std::lround(q.y+dy+q.h));y++)for(int x=int(std::lround(q.x+dx));x<int(std::lround(q.x+dx+q.w));x++){
   if(x<0||y<0||x>=int(c.w)||y>=int(c.h))continue;
   const float fu=(float(x)+.5f-(q.x+dx))/q.w,fv=(float(y)+.5f-(q.y+dy))/q.h;
   const unsigned sx=std::min(aw-1,unsigned((q.u0+(q.u1-q.u0)*fu)*float(aw))),sy=std::min(ah-1,unsigned((q.v0+(q.v1-q.v0)*fv)*float(ah)));
   const uint32_t s=rgba[size_t(sy)*aw+sx];const float a=float(s>>24)/255.f*a_mul;uint32_t& d=c.px[size_t(y)*c.w+x];
   auto mix=[&](unsigned from,unsigned to){return uint32_t(std::lround(float((s>>from)&255)*a+float((d>>to)&255)*(1-a)))<<to;};
   d=0xff000000u|mix(0,16)|mix(8,8)|mix(16,0);   // atlas bytes are R,G,B,A (low byte red); the canvas is 0xAARRGGBB
  }}
}
// The 12 trophies of a language: the game's own package when it is there, else the translation table (+ the same grades and icon files).
static bool g_real_package=false;
static std::vector<cv_trophies::Trophy> items_for(const std::filesystem::path& game,int lang,const std::filesystem::path& icons){
 auto snapshot=cv_trophies::view(game,lang,true);if(snapshot.catalog&&snapshot.catalog->items.size()==12){g_real_package=true;return snapshot.catalog->items;}
 std::vector<cv_trophies::Trophy> out;
 for(int id=0;id<12;id++){
  cv_trophies::Trophy t;t.id=id;t.grade=id==9?2:(id==7||id==2)?3:4;
  const cv_trophy_translation::Entry* found=nullptr;
  for(const auto& e:cv_trophy_translation::entries)if(e.id==id&&(e.language==lang||(!found&&e.language==0)))found=&e;   // a language the table lacks (the game's own package has it) falls back to Portuguese
  assert(found);t.name=found->name;t.description=found->detail;
  char file[32];snprintf(file,sizeof file,"TROP%03d.PNG",id);std::ifstream f(icons/file,std::ios::binary);
  if(f)t.icon.assign(std::istreambuf_iterator<char>(f),{});
  else{   // a drawn stand-in: a 240x240 diagonal gradient
   using namespace Gdiplus;Bitmap b(240,240,PixelFormat32bppARGB);{Graphics g(&b);LinearGradientBrush br(Rect(0,0,240,240),Color(255,40+id*15,60,160),Color(255,230,200-id*10,60),45.f,FALSE);g.FillRectangle(&br,0,0,240,240);}
   IStream* st=nullptr;assert(SUCCEEDED(CreateStreamOnHGlobal(nullptr,TRUE,&st)));CLSID png;CLSIDFromString(L"{557CF406-1A04-11D3-9A73-0000F81EF32E}",&png);assert(b.Save(st,&png,nullptr)==Ok);
   HGLOBAL mem=nullptr;GetHGlobalFromStream(st,&mem);const size_t n=size_t(GlobalSize(mem));auto* p=static_cast<unsigned char*>(GlobalLock(mem));t.icon.assign(p,p+n);GlobalUnlock(mem);st->Release();
  }
  out.push_back(std::move(t));
 }
 return out;
}
static Canvas scene(unsigned w,unsigned h,bool bright,const wchar_t* image){
 Canvas c(w,h,0xff000000);
 if(image){using namespace Gdiplus;Bitmap src(image);if(src.GetLastStatus()==Ok){Bitmap fit(int(w),int(h),PixelFormat32bppARGB);{Graphics g(&fit);g.DrawImage(&src,0,0,int(w),int(h));}
   BitmapData d{};Rect r(0,0,int(w),int(h));if(fit.LockBits(&r,ImageLockModeRead,PixelFormat32bppARGB,&d)==Ok){for(unsigned y=0;y<h;y++)memcpy(&c.px[size_t(y)*w],static_cast<unsigned char*>(d.Scan0)+size_t(y)*d.Stride,size_t(w)*4);fit.UnlockBits(&d);}
   if(!bright)return c;}}
 for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++){const unsigned v=bright?200+((x/64+y/64)&1)*40:18+(x*40/w);c.px[size_t(y)*w+x]=0xff000000u|(v<<16)|(v<<8)|(bright?v-20:v/2);}
 return c;
}
// One toast at its held pose on a 1920x1080 output, drawn on `c` shifted down by `dy`.
static void put_toast(Canvas& c,const cv_trophies::Trophy& t,int lang,uint64_t& clock,float dy){
 using namespace cv_trophy_popup;const uint32_t* px=nullptr;uint32_t aw=0,ah=0;uint64_t gen=0;
 enabled(false);enabled(true);enqueue(t,lang);clock+=20000;frame_at(1920,1080,clock,&px,&aw,&ah,&gen);
 auto* sp=frame_at(1920,1080,clock+400,&px,&aw,&ah,&gen);assert(sp&&px);draw_sprites(c,sp,px,aw,ah,0,dy);
}
int main(int argc,char** argv){
 assert(argc>=3);
 const std::filesystem::path game=std::filesystem::u8path(argv[1]);
 const std::wstring preview=argc>3?std::filesystem::u8path(argv[3]).wstring()+L"\\":L"";
 const std::wstring background=argc>4?std::filesystem::u8path(argv[4]).wstring():L"";
 const std::filesystem::path icons=argc>5?std::filesystem::u8path(argv[5]):std::filesystem::path();
 cv_trophies::storage_for_test(std::filesystem::u8path(argv[2]));
 using namespace cv_trophy_popup;initialize();sound_observer_for_test(heard);assert(g_rsx_trophy_hook);
 auto trophy=items_for(game,0,icons)[0];
 const uint32_t* pixels=nullptr;uint32_t w=0,h=0;uint64_t generation=0;
 assert(!frame_at(1920,1080,1000,&pixels,&w,&h,&generation));enqueue(trophy,0);
 // 1080p: scale 1. Atlas = card 640x112 + 10px of shadow all around + the 4-row strip.
 auto* card=frame_at(1920,1080,1000,&pixels,&w,&h,&generation);assert(card&&w==660&&h==136&&sounds==1&&(card->argb>>24)==0);float start_x=card->x;
 assert(card[0].w==660&&card[0].h==132);   // the strip is not part of the card quad
 card=frame_at(1920,1080,1140,&pixels,&w,&h,&generation);assert(card&&(card->argb>>24)>100&&(card->argb>>24)<160);assert(card->x<start_x);
 card=frame_at(1920,1080,1280,&pixels,&w,&h,&generation);assert(card&&(card->argb>>24)==255&&sounds==1);float held_x=card->x;
 // The card sits 32px from the top-right corner (the atlas has 10px of shadow around the 640x112 body).
 assert(card[0].x==1920-32-640-10&&card[0].y==32-10);
 // Progress line: a second sprite along the bottom edge, shorter as time passes, gone at the end.
 assert(card[0].after==2&&card[1].h==2&&card[1].y==32+112-2&&card[1].x==card[0].x+10+5);
 const float full=card[1].w;assert(full>=590&&full<=632);
 card=frame_at(1920,1080,3280,&pixels,&w,&h,&generation);assert(card&&card[0].after==2&&card[1].w<full-100&&card[1].w>200);float mid=card[1].w;
 card=frame_at(1920,1080,5200,&pixels,&w,&h,&generation);assert(card&&card[0].after==2&&card[1].w<mid&&card[1].w>=1);
 // Atlas: shadow only outside the body, the strip is solid red in its last rows, the card has the red left bar.
 assert((pixels[0]>>24)<40);assert(pixels[size_t(h-2)*w+w/2]==0xff221cc4);   // A,B,G,R bytes: R=C4 G=1C B=22
 assert((pixels[size_t(10+56)*w+10+2]&0xffffff)==0x221cc4&&(pixels[size_t(10+56)*w+10+2]>>24)>200);
 card=frame_at(1920,1080,6280,&pixels,&w,&h,&generation);assert(card&&(card->argb>>24)==255&&card->x==held_x&&sounds==1&&card[0].after==1);
 card=frame_at(1920,1080,6430,&pixels,&w,&h,&generation);assert(card&&(card->argb>>24)>100&&(card->argb>>24)<160&&card->x>held_x);
 assert(!frame_at(1920,1080,6580,&pixels,&w,&h,&generation));assert(!frame_at(1920,1080,9000,&pixels,&w,&h,&generation));
 // 1280x720: scale 0.667; the card is drawn for it from the first frame (the scale of the last frame is known).
 enqueue(trophy,0);enqueue(trophy,0);card=frame_at(1280,720,10000,&pixels,&w,&h,&generation);assert(sounds==2&&card);
 {const unsigned expect=unsigned(std::lround(640*(720.f/1080)))+2*unsigned(std::ceil(10*(720.f/1080)));
  // First draw after a size change is the stretched old picture, then the redraw arrives from the worker thread.
  for(int tries=0;tries<200&&w!=expect;tries++){Sleep(25);card=frame_at(1280,720,10000,&pixels,&w,&h,&generation);}
  assert(w==expect&&card[0].w==float(w));}
 frame_at(1280,720,15580,&pixels,&w,&h,&generation);assert(sounds==3);
 // Output size change: 1440p wants scale 1.33; the old atlas is drawn stretched at once and a new one replaces it, with a new generation.
 enabled(false);enabled(true);frame_at(1920,1080,29990,&pixels,&w,&h,&generation);enqueue(trophy,0);card=frame_at(1920,1080,30000,&pixels,&w,&h,&generation);assert(card&&w==660);const uint64_t gen1=generation;
 card=frame_at(2560,1440,30400,&pixels,&w,&h,&generation);assert(card&&w==660&&card[0].w>660*1.3f);   // stretched meanwhile
 {const float s=1440.f/1080;const unsigned expect=unsigned(std::lround(640*s))+2*unsigned(std::ceil(10*s));
  for(int tries=0;tries<200&&w==660;tries++){Sleep(25);card=frame_at(2560,1440,30400,&pixels,&w,&h,&generation);}
  assert(w==expect&&generation!=gen1&&card[0].w==float(w)&&std::fabs(card[0].x-(2560-32*s-640*s-std::ceil(10*s)))<1.01f);}
 // The maximum atlas (scale 1.5) fits the renderer's 1024x256 texture; 4K and a tiny window stay in range.
 card=frame_at(3840,2160,30400,&pixels,&w,&h,&generation);for(int tries=0;tries<200&&w!=990;tries++){Sleep(25);card=frame_at(3840,2160,30400,&pixels,&w,&h,&generation);}assert(w<=1024&&h<=256);
 // A small drift in the output (under 3%) does not redraw.
 {const uint64_t g0=generation;card=frame_at(3840,2170,30400,&pixels,&w,&h,&generation);assert(generation==g0);}
 enabled(false);assert(!frame_at(1280,720,36000,&pixels,&w,&h,&generation));enqueue(trophy,0);enabled(true);assert(!frame_at(1280,720,36001,&pixels,&w,&h,&generation));
 uint64_t clock=100000;frame_at(1920,1080,clock,&pixels,&w,&h,&generation);   // the idle output size is what the next card is drawn for
 for(int lang=0;lang<13;lang++){
  const auto items=items_for(game,lang,icons);
  for(const auto& localized:items){
   clock+=20000;enabled(false);enabled(true);enqueue(localized,lang);frame_at(1920,1080,clock,&pixels,&w,&h,&generation);
   card=frame_at(1920,1080,clock+280,&pixels,&w,&h,&generation);assert(card&&pixels&&generation&&w==660);
   if(g_real_package&&lang!=0&&localized.id==0)assert(localized.name!=trophy.name&&localized.description!=trophy.description);
   if(!preview.empty()&&localized.id==0){Canvas c(w,h,0xff1a1414);draw_sprites(c,card,pixels,w,h,-card[0].x,-card[0].y);save_canvas(c,preview+L"card-"+std::to_wstring(lang)+L".png");}
  }
  // Three tiers on the game scene, as in square-toasts.png (the same three trophies).
  if(!preview.empty()&&(lang==0||lang==1||lang==3||lang==7||lang==8||lang==9||lang==12)){
   Canvas c=scene(1920,1080,false,background.empty()?nullptr:background.c_str());
   put_toast(c,items[0],lang,clock,0);put_toast(c,items[7],lang,clock,144);put_toast(c,items[9],lang,clock,288);
   save_canvas(c,preview+L"toasts-"+std::to_wstring(lang)+L".png");
  }
 }
 if(!preview.empty()){   // every tier on a bright background: the card must stay readable
  auto items=items_for(game,1,icons);Canvas c=scene(1920,1080,true,nullptr);float dy=0;
  items.push_back(items[9]);items.back().grade=1;   // the game has no platinum: shown here for its colours
  for(int grade=1;grade<=4;grade++)for(const auto& t:items)if(t.grade==grade){put_toast(c,t,1,clock,dy);dy+=130;break;}
  save_canvas(c,preview+L"tiers-bright.png");
 }
 HRSRC sound=FindResourceW(GetModuleHandleW(nullptr),L"TROPHY_SOUND",L"WAVE");assert(sound);
 assert(SizeofResource(GetModuleHandleW(nullptr),sound)>170000);
 auto* data=static_cast<const char*>(LockResource(LoadResource(GetModuleHandleW(nullptr),sound)));assert(data&&!memcmp(data,"RIFF",4)&&!memcmp(data+8,"WAVE",4));
 assert(PlaySoundW(L"TROPHY_SOUND",GetModuleHandleW(nullptr),SND_RESOURCE|SND_ASYNC|SND_NODEFAULT));Sleep(1100);
 puts("PASS popup: card 640x112 at the output scale with shadow and strip; corner margin 32; progress line shrinks; redraw on size change (>3%) off the render thread; 280ms entry; 5s hold; 300ms exit; sound once on display; sequential queue; disabled cancels; idle empty; all 12 trophies in all 13 launcher languages; embedded WAV playback accepted.");
 fflush(stdout);_exit(0);   // no GdiplusShutdown: the keycap cache crashes at exit
}
