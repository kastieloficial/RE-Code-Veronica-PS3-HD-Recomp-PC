/* Drive the real native menu with guest input, without a game or GPU. */
#define NOMINMAX
#include <windows.h>
static bool keyboard_keys[256]={},keyboard_focused=true;
static SHORT menu_key_state(int vk){return vk>=0&&vk<256&&keyboard_keys[vk]?static_cast<SHORT>(0x8000):0;}
static HWND menu_foreground(){return keyboard_focused?reinterpret_cast<HWND>(1):nullptr;}
#define GetAsyncKeyState menu_key_state
#define GetForegroundWindow menu_foreground
#include "../cv_appearance_menu.cpp"
#undef GetForegroundWindow
#undef GetAsyncKeyState
#include <cassert>
#include <utility>

static std::array<unsigned char, 16 * 1024 * 1024> memory;
static std::array<int,8> settings={4,2,1,160,0,0,70,4};
static std::array<int,7> enhanced={100,100,0,1,1,0,0};
static int language=1,device=100;
static PadPhys test_pad{};
static int test_360_layout=-1;
static std::vector<std::pair<int,int>> changes;
static unsigned largest_batch,last_batch,flip_counter,flip_step=1;
static std::vector<rsx_ui_quad> preview_quads;
extern "C" {
int cv_enhanced_get(int which){return enhanced[which-8];}
int cv_enhanced_language(){return language;}
int pad_last_input(){return device;}
int pad_phys_360_layout(int kind){return test_360_layout==1||(test_360_layout==-1&&kind==PAD_KIND_XINPUT);}
int pad_phys_poll(int index,PadPhys* out){
 *out=index==0?test_pad:PadPhys{};
 if(pad_phys_360_layout(out->kind)){
  const unsigned b=out->buttons,mask=(1u<<PB_L1)|(1u<<PB_R1)|(1u<<PB_L2)|(1u<<PB_R2);
  out->buttons=(b&~mask)|(((b>>PB_L1)&1u)<<PB_L2)|(((b>>PB_R1)&1u)<<PB_R2)|(((b>>PB_L2)&1u)<<PB_L1)|(((b>>PB_R2)&1u)<<PB_R1);
 }
 return out->connected;
}
void* rsx_d3d12_window(){return reinterpret_cast<void*>(1);}
uint8_t* vm_base=memory.data();
PPU_THREAD_LOCAL void (*g_trampoline_fn)(void*)=nullptr;
PPU_THREAD_LOCAL ppu_context* g_active_ctx=nullptr;
uint32_t vm_read32(uint64_t a) {
    assert(a+4<=memory.size());
    return uint32_t(memory[a])<<24|uint32_t(memory[a+1])<<16|uint32_t(memory[a+2])<<8|memory[a+3];
}
void vm_write32(uint64_t a,uint32_t v) {
    assert(a+4<=memory.size());
    for(int i=0;i<4;i++)memory[a+i]=uint8_t(v>>(24-8*i));
}
void vm_write64(uint64_t a,uint64_t v) { vm_write32(a,uint32_t(v>>32));vm_write32(a+4,uint32_t(v)); }
int cv_pc_settings_read() {
    return settings[2]|settings[0]<<4|settings[1]<<8|settings[3]<<10|settings[4]<<18;
}
int cv_pc_settings_saturation_read() { return settings[5]; }
int cv_pc_settings_effects_read() { return settings[6]|settings[7]<<8; }
void cv_pc_settings_change(int which,int value) { if(which<8)settings[which]=value;else enhanced[which-8]=value;changes.emplace_back(which,value); }
uint32_t cellGcm_flip_submit_count() { flip_counter+=flip_step;return flip_counter; }   // one frame per menu call
void rsx_d3d12_set_ui_quads(const rsx_ui_quad* q,uint32_t count) {
    largest_batch=std::max(largest_batch,count);last_batch=count;
    if(count)preview_quads.assign(q,q+count);else preview_quads.clear();
    assert(count<131072);
    for(uint32_t i=0;i<count;i++) {
        assert(q[i].w>0&&q[i].h>0);
        assert(q[i].x>=0&&q[i].x+q[i].w<640);
        if(!(q[i].y>=0&&q[i].y+q[i].h<448))std::fprintf(stderr,"BOUNDS lang=%d tab=%d row=%d y=%f h=%f\n",language,pc_tab,pc_row,q[i].y,q[i].h);
        assert(q[i].y>=0&&q[i].y+q[i].h<448);
    }
}
}
void CV_OG_MENU_RESET(ppu_context* ctx) {ctx->gpr[3]=0;}
void CV_OG_MENU_INPUT(ppu_context* ctx) {ctx->gpr[3]=0;}
void CV_OG_MENU_COLOR(ppu_context*) {}
void CV_FN_MENU_FRAME(ppu_context*) {}
void CV_FN_MENU_SOUND(ppu_context*) {}


extern "C" void rsx_d3d12_set_ui_retained(int){}
static int modern=0,requests=0,appends=0;
extern "C" int cv_modern_get(){return modern;}
extern "C" void cv_modern_change(int v){modern=v;}
namespace cv_appearance {
std::filesystem::path appearance_system_directory(const std::filesystem::path& p){return p;}
bool AppearancePreview::open(const std::filesystem::path&){return false;}
bool AppearancePreview::load(unsigned){return false;}
void AppearancePreview::select(unsigned i){selected_=i;}
void AppearancePreview::append(std::vector<rsx_ui_quad>&,float,float,float,float){++appends;}
bool request(ppu_context*,unsigned,const PlayerAsset*){++requests;return true;}
void frame(ppu_context*,bool){} bool busy(){return false;}const char* status(){return "";}
}
int main(int argc,char** argv){
 _putenv_s("CV_FPS_TEST_60","1");_putenv_s("CV_RENDER_INTERPOLATION","0");_putenv_s("CV_SMOOTH_ANIMATION","1");
 flip_step=2;ppu_context ctx{};ctx.gpr[1]=0x8000;
 vm_write32(CV_MENU_PULSE,255);CV_FN_MENU_RESET(&ctx);vm_write32(kCursor,4);
 auto press=[&](uint32_t in,uint32_t rep){ctx.gpr[3]=0;ctx.gpr[4]=1;vm_write32(kInput,in);vm_write32(kRepeat,rep);CV_FN_MENU_INPUT(&ctx);};
 for(int i=0;i<5;i++)press(0,0);assert(last_batch>0); // Actual appearance path with PR36 and old pose OFF.
 vm_write32(kScreen,3);press(0,0);press(0,0);assert(last_batch==0);
 vm_write32(kScreen,0);press(0,0);press(0,0);assert(last_batch>0);
 press(0,kDown);assert(pc_selected);press(0x4000,0);assert(pc_open&&pc_tab==0&&menu_rows()==11);
 assert(appends==0&&requests==0);auto before=appearance_choice;
 press(0,kRight);assert(changes.back().first==0&&appearance_choice==before); // Graphics first row adjusts resolution.
 pc_row=7;press(0,kRight);assert(changes.back().first==11);pc_row=8;press(0,kRight);assert(changes.back().first==12);
 pc_row=kResetColorsRow;press(0x4000,0);assert(changes.back().first==5);
 // Physical shoulder tab switch into Gameplay, skipping Audio.
 test_pad.connected=1;test_pad.kind=PAD_KIND_DS4;test_pad.buttons=1u<<PB_L1;press(0,0);test_pad.buttons=0;press(0,0);
 assert(pc_tab==2&&pc_row==0&&menu_rows()==6&&appends>0);
 press(0,kRight);assert(requests==1&&appearance_choice!=before);
 press(0,kDown);assert(pc_row==1&&modern==0);press(0,kRight);assert(modern==1);press(0,kLeft);assert(modern==0);
 press(0,kDown);press(0,kRight);assert(changes.back().first==13&&enhanced[5]==1);
 press(0,kDown);press(0,kRight);assert(changes.back().first==14&&enhanced[6]==1);
 press(0,kDown);press(0,kRight);assert(changes.back().first==10&&enhanced[2]==1);
 press(0,kDown);assert(menu_back());press(0x4000,0);assert(!pc_open&&pc_selected);
 press(0x4000,0);assert(pc_tab==0);keyboard_keys['E']=true;press(0,0);keyboard_keys['E']=false;press(0,0);assert(pc_tab==1&&menu_rows()==4);
 pc_row=0;press(0,kLeft);assert(changes.back().first==8);pc_row=2;press(0x4000,0);assert(enhanced[0]==100&&enhanced[1]==100);
 for(int lang=0;lang<13;lang++)for(int tab=0;tab<3;tab++){
  language=lang;pc_tab=tab;for(pc_row=0;pc_row<menu_rows();pc_row++)draw_settings(&ctx);
 }
 if(argc>1){const char* folder=argv[1];
      CLSID encoder={0x557cf400,0x1a04,0x11d3,{0x9a,0x73,0x00,0x00,0xf8,0x1e,0xf3,0x2e}};
      for(int lang=0;lang<13;lang++)for(int tab=0;tab<3;tab++){
        language=lang;pc_tab=tab;pc_row=0;draw_settings(&ctx);
        Gdiplus::Bitmap image(1280,896,PixelFormat32bppARGB);
        std::vector<uint32_t> pixels(1280*896,0xff161413);
        for(const auto& q:preview_quads){
          int left=std::max(0,int(q.x*2)),right=std::min(1280,int(std::ceil((q.x+q.w)*2)));
          int top=std::max(0,int(q.y*2)),bottom=std::min(896,int(std::ceil((q.y+q.h)*2)));
          for(int y=top;y<bottom;y++)for(int x=left;x<right;x++){
            float u=std::clamp(((x+.5f)/2-q.x)/q.w,0.f,1.f),v=std::clamp(((y+.5f)/2-q.y)/q.h,0.f,1.f);
            float alpha=((q.corner_a&255)*(1-u)*(1-v)+((q.corner_a>>8)&255)*(1-u)*v+((q.corner_a>>16)&255)*u*(1-v)+(q.corner_a>>24)*u*v)/255.f*(q.argb>>24)/255.f;
            auto& dst=pixels[y*1280+x];uint32_t rgb=0xff000000;
            for(int sh:{0,8,16})rgb|=uint32_t(((q.argb>>sh)&255)*alpha+((dst>>sh)&255)*(1-alpha))<<sh;
            dst=rgb;
          }
        }
        Gdiplus::BitmapData data{};Gdiplus::Rect bounds(0,0,1280,896);image.LockBits(&bounds,Gdiplus::ImageLockModeWrite,PixelFormat32bppARGB,&data);
        for(int y=0;y<896;y++)std::memcpy(static_cast<uint8_t*>(data.Scan0)+y*data.Stride,pixels.data()+y*1280,1280*4);image.UnlockBits(&data);
        std::string file=std::string(folder)+"/menu-"+std::to_string(lang)+"-"+std::to_string(tab)+".bmp";std::wstring wide(file.begin(),file.end());assert(image.Save(wide.c_str(),&encoder,nullptr)==Gdiplus::Ok);
      }

 }
 puts("PASS real appearance menu: visible Recomp entry with PR/legacy OFF, subscreen clear/reentry, reorganized Graphics/Gameplay, appearance preview/actions, classic initial state, modern toggle, HUD/reload/Battle/audio controls, physical shoulders and Q/E tabs, all rows fit in 13 languages");
}
