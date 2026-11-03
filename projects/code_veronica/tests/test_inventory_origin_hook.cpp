#include "ppu_recomp.h"
#include <cassert>
#include <vector>
#include <cstring>
#include <cstdio>
static std::vector<uint8_t> mem(0x1100000);
uint32_t vm_read32(uint64_t a){assert(a+4<=mem.size());return uint32_t(mem[a])<<24|uint32_t(mem[a+1])<<16|uint32_t(mem[a+2])<<8|mem[a+3];}
uint8_t vm_read8(uint64_t a){assert(a<mem.size());return mem[a];}
uint64_t vm_read64(uint64_t a){return uint64_t(vm_read32(a))<<32|vm_read32(a+4);}
void vm_write32(uint64_t a,uint32_t w){assert(a+4<=mem.size());for(unsigned i=0;i<4;i++)mem[a+i]=uint8_t(w>>(24-8*i));}
#include "../cv_inventory_ui_origin.cpp"
static unsigned calls=0;static ppu_context incoming;
void cv_ui_original_enqueue(ppu_context* ctx){assert(std::memcmp(ctx,&incoming,sizeof incoming)==0);++calls;}
int main(){
 using namespace cv_inventory_ui;_putenv_s("CV_INVENTORY_UI_SMOOTH","1");
 ppu_context ctx{};ctx.gpr[4]=0x510000;ctx.gpr[31]=0x880000;ctx.gpr[1]=0x700000;ctx.lr=0x1CABB0;
 vm_write32(0x510000,CV_SCENE_SPRITE_VTABLE);
 auto put64=[](uint32_t a,uint64_t w){vm_write32(a,uint32_t(w>>32));vm_write32(a+4,uint32_t(w));};
 put64(0x700000,0x700100);put64(0x700110,0x19ABCD);put64(0x700100,0x700200);put64(0x700210,0x1B1234);put64(0x700200,0x700300);put64(0x700310,0x210094);
 incoming=ctx;begin();func_00015AFC(&ctx);assert(calls==1&&draw_ids.size()==1);
 const auto id=draw_ids.at(0x510000);assert(id[0]==0x1CABB0&&id[1]==0x19ABCD&&id[2]==0x1B1234&&id[3]==0x210094&&id[4]==0x880000&&id[6]==0);
 func_00015AFC(&ctx);assert(draw_ids.at(0x510000)[6]==1);
 begin();func_00015AFC(&ctx);assert(draw_ids.at(0x510000)==id);
 // A different caller is a different semantic draw role, despite same UV.
 put64(0x700110,0x19ABCE);begin();func_00015AFC(&ctx);assert(draw_ids.at(0x510000)!=id);
 // Stop malformed stack chains before following their target.
 put64(0x700000,0xFFFFFFF0);begin();func_00015AFC(&ctx);assert(draw_ids.at(0x510000)[1]==0);
 _putenv_s("CV_INVENTORY_UI_SMOOTH","0");begin();unsigned old=calls;func_00015AFC(&ctx);assert(calls==old+1&&draw_ids.empty());
 puts("PASS enqueue hook: original context preserved, delegate called once, caller/template role, per-role occurrences, reset, malformed backchain bounds and disabled behavior");
}
