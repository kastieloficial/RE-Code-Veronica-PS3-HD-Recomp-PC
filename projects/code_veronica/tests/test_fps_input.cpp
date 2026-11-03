#include <cassert>
#include <cstdint>
#include <vector>
#include "../cv_version.h"
#include "../cv_scene_region.h"
static std::vector<uint8_t> memory(0x1000000);
uint32_t vm_read32(uint64_t a){assert(a+4<=memory.size());return uint32_t(memory[a])<<24|uint32_t(memory[a+1])<<16|uint32_t(memory[a+2])<<8|memory[a+3];}
uint8_t vm_read8(uint64_t a){assert(a<memory.size());return memory[a];}
void vm_write32(uint64_t a,uint32_t b){assert(a+4<=memory.size());for(int i=0;i<4;i++)memory[a+i]=uint8_t(b>>(24-8*i));}
#include "../cv_fps_input.h"
int main(){
 cv_fps_input::Latch latch;constexpr uint32_t controller=0x100000,other=0x110000;
 const auto setup=[](uint32_t a){vm_write32(a,CV_SCENE_CONTROLLER_VTABLE);memory[a+0x41]=1;};
 setup(controller);setup(other);vm_write32(CV_SCENE_CONTROLLER,controller);
 vm_write32(controller+0x1A0,1);latch.capture();vm_write32(controller+0x1A0,0);latch.capture();
 vm_write32(controller+0x1A0,2);latch.capture();latch.begin_tick(true);assert(vm_read32(controller+0x1A0)==3);
 latch.end_tick();assert(vm_read32(controller+0x1A0)==2);
 for(unsigned port=0;port<4;port++){vm_write32(CV_MENU_INPUT+60*port,0xFFFFFFFF);vm_write32(CV_MENU_REPEAT+60*port,0xFFFFFFFF);}
 latch.begin_tick(false);assert(vm_read32(controller+0x1A0)==0);
 for(unsigned port=0;port<4;port++)assert(vm_read32(CV_MENU_INPUT+60*port)==0&&vm_read32(CV_MENU_REPEAT+60*port)==0);
 latch.end_tick();assert(vm_read32(controller+0x1A0)==2);
 latch.capture();vm_write32(controller+0x1A0,0);latch.last-=std::chrono::milliseconds(101);latch.capture();latch.begin_tick(true);assert(vm_read32(controller+0x1A0)==0);latch.end_tick();
 vm_write32(controller+0x1A0,4);latch.capture();vm_write32(CV_SCENE_CONTROLLER,other);vm_write32(other+0x1A0,8);latch.capture();latch.begin_tick(true);assert(vm_read32(other+0x1A0)==8);latch.end_tick();
 memory[other+0x41]=0;latch.capture();assert(latch.owner==0);latch.begin_tick(true);assert(vm_read32(other+0x1A0)==8);
 vm_write32(other,0);memory[other+0x41]=1;latch.capture();assert(latch.owner==0);
}
