// The return-to-title box's drawing: geometry of the design, the three looks of every clickable control (normal, hover, pressed),
// the footer following the input device, the chosen button, and no clipping in the launcher's 13 languages. With an output folder
// it also writes the previews: titulo-pt-ps.png and titulo-en-kb.png (1920x1080, to compare with docs/previas-v1.7/) and one box per language.
#include "../cv_title_return_ui.h"
#include "../cv_title_return_i18n.h"
#include "../cv_port_ui.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <string>
#include <vector>
extern "C" int pad_phys_360_layout(int) {return 0;}   // cv_port_ui::menu_pad
std::wstring cv_key_cap_label(int vk) {   // button_icons.cpp: the keycaps the footer draws
    if(vk==0x0D)return L"Enter";if(vk==0x1B)return L"Esc";if(vk>=0x70&&vk<=0x87)return L"F"+std::to_wstring(vk-0x6F);
    return vk>0?std::wstring(1,wchar_t(vk)):std::wstring();
}
using namespace cv_title_return_ui;
using Gdiplus::Bitmap;using Gdiplus::Graphics;using Gdiplus::RectF;

static uint64_t region_hash(Bitmap& b,int x,int y,int w,int h) {
    Gdiplus::BitmapData d{};Gdiplus::Rect r(x,y,w,h);const auto ok=b.LockBits(&r,Gdiplus::ImageLockModeRead,PixelFormat32bppARGB,&d);assert(ok==Gdiplus::Ok);(void)ok;
    uint64_t hash=1469598103934665603ull;
    for(int j=0;j<h;j++){const uint8_t* row=static_cast<const uint8_t*>(d.Scan0)+j*d.Stride;for(int i=0;i<w*4;i++){hash^=row[i];hash*=1099511628211ull;}}
    b.UnlockBits(&d);return hash;
}
static const Hit* find_hit(const std::vector<Hit>& list,int kind) {for(const auto& h:list)if(h.kind==kind)return &h;return nullptr;}
static void render(Bitmap& bmp,const View& v) {Graphics g(&bmp);g.Clear(Gdiplus::Color(255,0,0,0));draw(g,v);}

static void save_png(Bitmap& image,const std::string& file) {
    using namespace Gdiplus;
    UINT n=0,size=0;GetImageEncodersSize(&n,&size);std::vector<uint8_t> buffer(size);
    auto* codecs=reinterpret_cast<ImageCodecInfo*>(buffer.data());GetImageEncoders(n,size,codecs);
    CLSID png{};for(UINT i=0;i<n;i++)if(!wcscmp(codecs[i].MimeType,L"image/png"))png=codecs[i].Clsid;
    const std::wstring name(file.begin(),file.end());
    const auto saved=image.Save(name.c_str(),&png);assert(saved==Ok);(void)saved;
}
// The box centred on a dark 1920x1080 stand-in for the game picture, as in the approved previews.
static void export_scene(const std::string& file,const View& v) {
    using namespace Gdiplus;
    Bitmap image(1920,1080,PixelFormat32bppARGB);
    {Graphics g(&image);
     LinearGradientBrush back(RectF(0,0,1920,1080),Color(255,10,8,12),Color(255,34,14,10),LinearGradientModeForwardDiagonal);g.FillRectangle(&back,0,0,1920,1080);
     g.TranslateTransform(float((1920-W)/2),float((1080-H)/2)-0.f);draw(g,v);}
    save_png(image,file);
}
static void export_box(const std::string& file,const View& v) {Bitmap image(W,H,PixelFormat32bppARGB);render(image,v);save_png(image,file);}

int main(int argc,char** argv) {
    setvbuf(stdout,nullptr,_IONBF,0);
    Gdiplus::GdiplusStartupInput input;ULONG_PTR token=0;Gdiplus::GdiplusStartup(&token,&input,nullptr);
    cv_port_ui::fonts_available();
    Bitmap bmp(W,H,PixelFormat32bppARGB);
    View v;v.language=1;
    {   // the design: No is chosen when the box opens, Yes on the left, No on the right
        assert(v.selected==CHOICE_NO&&CHOICE_YES<CHOICE_NO);
        render(bmp,v);const auto list=hits(v);
        const Hit* yes=find_hit(list,HIT_YES),*no=find_hit(list,HIT_NO),*back=find_hit(list,HIT_BACK);
        assert(yes&&no&&back);
        assert(yes->rect.X==130&&yes->rect.Y==268&&yes->rect.Width==310&&yes->rect.Height==62);
        assert(no->rect.X==460&&no->rect.Y==268&&no->rect.Width==310&&no->rect.Height==62&&yes->rect.X<no->rect.X);
        assert(back->rect.Y>=H-62&&back->rect.Y<H);                 // the footer's Back group
        assert(hit_at(list,yes->rect.X+5,yes->rect.Y+5)==yes&&hit_at(list,no->rect.X+5,no->rect.Y+5)==no&&!hit_at(list,2,2));
        assert(W==900&&H==440);
    }
    {   // the chosen button is drawn differently from the other, and follows the choice
        View a=v;a.selected=CHOICE_NO;render(bmp,a);const auto yes_off=region_hash(bmp,130,268,310,62),no_on=region_hash(bmp,460,268,310,62);
        a.selected=CHOICE_YES;render(bmp,a);const auto yes_on=region_hash(bmp,130,268,310,62),no_off=region_hash(bmp,460,268,310,62);
        assert(yes_off!=yes_on&&no_on!=no_off);
    }
    {   // every clickable control: normal, hover and pressed look different
        for(int kind:{int(HIT_YES),int(HIT_NO),int(HIT_BACK)}) {
            View a=v;render(bmp,a);const auto* hit=find_hit(hits(a),kind);assert(hit);
            const int x=int(hit->rect.X),y=int(hit->rect.Y),w=int(hit->rect.Width),h=int(hit->rect.Height);
            const auto normal=region_hash(bmp,x,y,w,h);
            a.mouse.move(kind);render(bmp,a);const auto hover=region_hash(bmp,x,y,w,h);
            a.mouse.down(kind);render(bmp,a);const auto pressed=region_hash(bmp,x,y,w,h);
            assert(normal!=hover&&hover!=pressed&&normal!=pressed);
        }
        // the mouse track: the action is the release over the control pressed
        View a=v;int fired=-1;a.mouse.down(HIT_YES);assert(a.mouse.up(HIT_YES,fired)&&fired==HIT_YES);
        a.mouse.down(HIT_YES);a.mouse.up(HIT_NO,fired);assert(fired==-1);
    }
    {   // the footer follows the input device
        View a=v;a.input=PAD_INPUT_KEYBOARD;render(bmp,a);const auto kb=region_hash(bmp,24,H-62,W-48,42);
        a.input=PAD_KIND_DS4;render(bmp,a);const auto ps=region_hash(bmp,24,H-62,W-48,42);
        a.input=PAD_KIND_XINPUT;render(bmp,a);const auto xb=region_hash(bmp,24,H-62,W-48,42);
        a.input=PAD_KIND_SWITCH;render(bmp,a);const auto sw=region_hash(bmp,24,H-62,W-48,42);
        assert(kb!=ps&&ps!=xb&&xb!=sw&&kb!=xb&&ps!=sw);
    }
    {   // no clipping in any of the 13 languages: every text fits its box at the size the box chose, and no size is below the floor
        Bitmap probe(W,H,PixelFormat32bppARGB);Gdiplus::Graphics g(&probe);g.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);
        for(int l=0;l<13;l++) {
            View a=v;a.language=l;
            const Metrics m=measure(g,a);
            std::printf("lang %2d  header %.0fpx %.0f/%.0f  question %.0fpx %.0f/%.0f  warning %.0fpx %.0f/%.0f  labels %.0fpx %.0f,%.0f/%.0f\n",l,m.header_px,m.header_w,m.header_max,
                m.question_px,m.question_w,m.question_max,m.warning_px,m.warning_w,m.warning_max,m.label_px,m.yes_w,m.no_w,m.label_max);
            assert(m.header_w<=m.header_max&&m.question_w<=m.question_max&&m.warning_w<=m.warning_max);
            assert(m.yes_w<=m.label_max&&m.no_w<=m.label_max);
            assert(m.header_px>=Metrics::MIN_PX&&m.question_px>=Metrics::MIN_PX&&m.warning_px>=Metrics::MIN_PX&&m.label_px>=Metrics::MIN_PX);
            assert(m.question_px>=24&&m.warning_px>=14&&m.label_px>=18);   // shrunk a little at most: nothing is squeezed to unreadable
            // every language draws, and draws something different from the others (no language falls back to another one's strings)
            render(bmp,a);
            for(int o=0;o<l;o++){View b=v;b.language=o;Bitmap other(W,H,PixelFormat32bppARGB);render(other,b);assert(region_hash(bmp,0,0,W,H)!=region_hash(other,0,0,W,H));}
        }
        // the strings themselves: nothing empty (the header assert is compile time), the header has its separator, a notice is a sentence
        for(int l=0;l<13;l++){
            const std::wstring h=cv_title_return_i18n::tr(l,cv_title_return_i18n::HEADER);assert(h.find(L" · ")!=std::wstring::npos&&h.rfind(L"CODE: VERONICA",0)==0);
            assert(wcslen(cv_title_return_i18n::tr(l,cv_title_return_i18n::BLOCKED))>8&&wcslen(cv_title_return_i18n::tr(l,cv_title_return_i18n::RETURNING))>4);
        }
    }
    if(argc>1) {
        const std::string dir=argv[1];
        View pt;pt.language=0;pt.input=PAD_KIND_DS4;pt.selected=CHOICE_YES;      // titulo-pt-ps: Portuguese, PlayStation, Yes chosen
        export_scene(dir+"/titulo-pt-ps.png",pt);
        View en;en.language=1;en.input=PAD_INPUT_KEYBOARD;en.selected=CHOICE_NO;  // titulo-en-kb: English, keyboard, No chosen (the default)
        export_scene(dir+"/titulo-en-kb.png",en);
        for(int l=0;l<13;l++){View a;a.language=l;a.input=PAD_KIND_XINPUT;export_box(dir+"/box-lang"+(l<10?"0":"")+std::to_string(l)+".png",a);}
        View hover=en;hover.mouse.move(HIT_YES);export_box(dir+"/box-en-hover-yes.png",hover);
        View pressed=en;pressed.mouse.down(HIT_YES);export_box(dir+"/box-en-pressed-yes.png",pressed);
        std::printf("previews written to %s\n",dir.c_str());
    }
    cv_port_ui::release_fonts();
    // No GdiplusShutdown: the keycap cache (cv_keycap::draw) holds GDI+ images in a function-static map that is destroyed at exit.
    std::puts("title return box drawing: ok");
    return 0;
}
