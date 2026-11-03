#include "../cv_scene_region.h"
#include <cassert>
#include <cstdint>
#include <vector>
#include <limits>
#include <cstdio>
static std::vector<unsigned char> mem(0x1100000);
uint32_t vm_read32(uint64_t a){assert(a+4<=mem.size());return uint32_t(mem[a])<<24|uint32_t(mem[a+1])<<16|uint32_t(mem[a+2])<<8|mem[a+3];}
void vm_write32(uint64_t a,uint32_t w){assert(a+4<=mem.size());for(unsigned i=0;i<4;i++)mem[a+i]=uint8_t(w>>(24-8*i));}
uint8_t vm_read8(uint64_t a){assert(a<mem.size());return mem[a];}
#include "../cv_inventory_ui_interpolation.h"
using namespace cv_inventory_ui;
void put(uint32_t a,float f){vm_write32(a,word(f));}
void setup(){reset();begin();std::fill(mem.begin(),mem.end(),0);_putenv_s("CV_INVENTORY_UI_SMOOTH","1");vm_write32(CV_SCENE_SYSTEM,0x200000);vm_write32(CV_SCENE_RENDER,0x210000);vm_write32(0x200064,0x200);vm_write32(0x200070,8);}
void start(){begin();vm_write32(0x2100C4,0);vm_write32(0x2100D0,0x500000);}
uint32_t draw(unsigned slot,unsigned role,float x,uint32_t rgba=0xFFFFFFFF,unsigned uv=0){
 uint32_t c=0x510000+slot*0x100,d=0x600000+slot*0x1000;
 vm_write32(0x500000+4*slot,c);vm_write32(0x2100C4,slot+1);
 vm_write32(c,CV_SCENE_SPRITE_VTABLE);vm_write32(c+4,d);vm_write32(c+8,d+0x100);vm_write32(c+12,4);vm_write32(c+0x38,d+0x200);
 vm_write32(c+0x18,0x08060000);vm_write32(c+0x24,0x100000);
 for(unsigned i=0;i<4;i++) {put(d+16*i,x+float(i));put(d+16*i+4,float(i));put(d+16*i+8,123.f);vm_write32(d+16*i+12,0xDEADBEEF);vm_write32(d+0x100+4*i,rgba);vm_write32(d+0x200+8*i,uv+i*8);vm_write32(d+0x204+8*i,i*4);}
 record(c,{0x1CABB0,0x19AAAA,0x1BCCCC,0x210094,role,0});return d;
}
float at(uint32_t d){return real(vm_read32(d));}
int main(){
 setup();start();draw(0,100,0,0);capture();start();auto d=draw(0,100,10);capture();auto original=mem;
 for(double t:{0.,.25,.5,.75,1.}){render(t);assert(at(d)==float(10*t));assert(vm_read32(d+0x100)==color(0,0xFFFFFFFF,float(t)));assert(vm_read32(d+8)==word(123.f)&&vm_read32(d+12)==0xDEADBEEF);restore();assert(mem==original);}
 render(.25);render(.75);assert(at(d)==7.5f);restore();assert(mem==original);
 render(std::numeric_limits<double>::quiet_NaN());assert(mem==original);
 // Reordered identical texture tiles retain source identity, not index/nearest.
 setup();start();draw(0,100,0);draw(1,200,80);capture();
 start();auto panel=draw(0,200,90);auto bg=draw(1,100,0);capture();render(.5);assert(at(panel)==85&&at(bg)==0);restore();
 // New/changed text and a different packet count must NOT disable panel motion.
 start();panel=draw(0,200,100);bg=draw(1,100,0);auto glyph=draw(2,300,700,0x10101010,123);capture();auto all=mem;
 render(.5);assert(at(panel)==95&&at(bg)==0&&at(glyph)==700);restore();assert(mem==all);
 start();panel=draw(0,200,110);bg=draw(1,100,0);glyph=draw(2,300,700,0x10101010,456);capture();render(.5);assert(at(panel)==105&&at(glyph)==700);restore();
 // Per-source panel scaling/deformation is a real animation, not a layout mix.
 setup();start();draw(0,100,0);capture();start();d=draw(0,100,10);put(d+16,50);capture();render(.5);assert(at(d)==5&&real(vm_read32(d+16))==25.5f);restore();
 // Same UV/position with a DIFFERENT source must never be paired.
 setup();start();draw(0,100,0);capture();start();d=draw(0,999,10);capture();render(.5);assert(at(d)==10&&!borrowed);
 // Item/tab changes cut once; the next compatible animation resumes.
 for(unsigned field:{0x44u,0x54u}){
  setup();start();draw(0,100,0);capture();vm_write32(CV_SCENE_INVENTORY+field,1);start();d=draw(0,100,10);capture();render(.5);assert(at(d)==10);
  start();d=draw(0,100,20);draw(1,300,700,0xFFFFFFFF,88);capture();render(.5);assert(at(d)==15);restore();
 }
 // Equip/unequip leaves the selected item and tab unchanged. Cut just that
 // ownership boundary, then resume smooth motion on the next compatible frame.
 for(unsigned character=0;character<4;character++)for(const auto equipped:{std::pair{0u,2u},std::pair{2u,0u},std::pair{2u,3u}}){
  setup();mem[CV_SRT_CHARACTER]=uint8_t(character);const auto equip=CV_SRT_INVENTORY+character*0x40;
  vm_write32(equip,equipped.first);start();draw(0,100,0);capture();
  vm_write32(equip,equipped.second);start();d=draw(0,100,10);capture();all=mem;
  render(.5);assert(at(d)==10&&!borrowed&&mem==all);
  start();d=draw(0,100,20);capture();render(.5);assert(at(d)==15);restore();
  vm_write32(equip,equipped.first);all=mem;render(.5);assert(mem==all);
 }
 // Restore borrowed data even when equipment changes before the next tick.
 setup();start();draw(0,100,0);capture();start();d=draw(0,100,10);capture();all=mem;
 render(.5);vm_write32(CV_SRT_INVENTORY,2);restore();vm_write32(CV_SRT_INVENTORY,0);assert(mem==all);
 // A late buffer/selection replacement rejects writes before any partial draw.
 setup();start();draw(0,100,0);capture();start();d=draw(0,100,10);capture();vm_write32(0x510004,0x650000);all=mem;render(.5);assert(mem==all);
 setup();start();draw(0,100,0);capture();start();d=draw(0,100,10);capture();vm_write32(CV_SCENE_INVENTORY+0x54,99);all=mem;render(.5);assert(mem==all);
 // No registered origin means no guessed identity; default OFF also stays off.
 setup();start();draw(0,100,0);draw_ids.clear();capture();assert(current.sprites.empty());
 setup();start();draw(0,100,0);_putenv_s("CV_INVENTORY_UI_SMOOTH","0");all=mem;capture();render(.5);assert(mem==all);
 // Loading and malformed packet safeguards remain in force.
 setup();start();draw(0,100,0);vm_write32(0x200064,0x201);capture();assert(current.sprites.empty());
 setup();start();draw(0,100,0);vm_write32(0x2100C4,1025);capture();assert(current.sprites.empty());
 setup();start();draw(0,100,0);vm_write32(0x51000C,513);capture();assert(current.sprites.empty());
 setup();start();d=draw(0,100,0);put(d,std::numeric_limits<float>::quiet_NaN());capture();assert(current.sprites.empty());
 // Per-role occurrence numbers distinguish a loop emitting repeated elements.
 setup();start();draw(0,100,0);draw(1,100,80);capture();assert(current.sprites[0].id!=current.sprites[1].id);
 start();draw(0,100,10);draw(1,100,90);capture();render(.5);assert(at(0x600000)==5&&at(0x601000)==85);restore();
 puts("PASS source identity: smooth moving/scaling panels despite changed counts/text, stable reordered backgrounds, new-source snap, tab/item/equip/unequip cuts then resume, exact restore, repeated phases, glyph/texture ownership, live buffer/selection validation, default OFF and bounds");
}
