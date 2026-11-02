#include "../boot_skip.cpp"
#include <cassert>
#ifdef _WIN32
#define setenv(name,value,overwrite) _putenv_s(name,value)
#endif
#include <vector>
#include <cstdio>
static std::vector<unsigned char> memory(0x1100000);
uint32_t vm_read32(uint64_t a) {assert(a+4<=memory.size());return uint32_t(memory[a])<<24|uint32_t(memory[a+1])<<16|uint32_t(memory[a+2])<<8|memory[a+3];}
uint8_t vm_read8(uint64_t a) {assert(a<memory.size());return memory[a];}
void vm_write8(uint64_t a,uint8_t b) {assert(a<memory.size());memory[a]=b;}
void vm_write32(uint64_t a,uint32_t b) {assert(a+4<=memory.size());for(int i=0;i<4;i++)memory[a+i]=uint8_t(b>>(24-i*8));}
void vm_write16(uint64_t a,uint16_t b) {vm_write8(a,b>>8);vm_write8(a+1,b);}
static unsigned char flag=0;
extern "C" volatile unsigned char* ps3_patch_flag(const char*) {return &flag;}
void CV_OG_TITLE(ppu_context*) {}
void CV_OG_LOGO(ppu_context*) {}
#if !defined(CV_REGION_JP) && !defined(CV_REGION_US)
extern "C" {PPU_THREAD_LOCAL void (*g_trampoline_fn)(void*)=nullptr;}
static unsigned original,cleaned;static bool ready;
void cv_original_001E58FC(ppu_context*) {original++;}
void cv_original_00210914(ppu_context*) {original++;}
void func_001F047C(ppu_context* c) {c->gpr[3]=ready;}
void func_0026B170(ppu_context*) {cleaned++;}
#endif
int main() {
#if !defined(CV_REGION_JP) && !defined(CV_REGION_US)
 ppu_context c{};c.gpr[3]=0x1000;
 setenv("CV_SKIP_START_NOTICES","0",1);func_001E58FC(&c);func_00210914(&c);assert(original==2);original=0;
 setenv("CV_SKIP_START_NOTICES","1",1);
 vm_write32(0x109c,0x2000);vm_write32(0x1098,0x3000);vm_write32(0xAF96C4,0x4000);
 func_001E58FC(&c);assert(original==1);
 vm_write32(0x2034,4);func_001E58FC(&c);assert(vm_read32(0x1030)==0);
 ready=true;vm_write8(0x4051,1);func_001E58FC(&c);
 assert(vm_read32(0x1030)==1&&vm_read32(0x1054)==1&&vm_read32(0x301c)==0x29d&&vm_read8(0x4051)==0);
 assert(c.gpr[3]==0x1000&&original==1);
 for(unsigned state=1;state<=4;state++){vm_write32(0x1030,state);func_001E58FC(&c);assert(vm_read32(0x1030)==state);}
 assert(original==5);
 for(unsigned mode:{0u,4u})for(unsigned state=0;state<=8;state++) {
  vm_write8(0xBBE993,mode);vm_write32(0x1030,state);const auto old=cleaned;
  func_00210914(&c);const bool skip=state==2&&mode!=4;
  assert(vm_read32(0x1030)==(skip?6:state));assert(cleaned==old+unsigned(skip));assert(c.gpr[3]==0x1000);
 }
#endif
 puts("PASS: startup notices and untouched error/leaderboard paths");
}
