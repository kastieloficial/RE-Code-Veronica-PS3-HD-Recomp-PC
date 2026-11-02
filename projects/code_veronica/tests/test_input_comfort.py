from pathlib import Path
import subprocess,tempfile
source=(Path(__file__).resolve().parents[1]/'cv_modern.cpp').read_text()
fn=source[source.index('extern "C" uint32_t cv_controls_previous'):source.index('extern "C" uint32_t cv_controls_input')]
fn+=source[source.index('extern "C" unsigned cv_title_input'):]
harness='''#include <cstdint>
#include <cassert>
namespace cv_options {static bool on=true;bool enabled(const char*){return on;}}
static uint32_t flags=0x200, edges=0, input=0x20000;
static bool readable(uint32_t p,unsigned){return p==0x10000||p==0x20000;}
static uint32_t vm_read32(uint64_t p){
 if(p==0xA89BD0)return 0x10000;
 if(p==0x10070)return flags;
 if(p==0x2b078)return input;
 if(p==0x20010)return edges;
 assert(false);return 0;
}
'''+fn+'''
int main(){
 cv_options::on=false;assert(cv_title_input(0xc000)==0);assert(cv_title_input(8)==8);edges=0x4000;assert(cv_controls_previous(0xc00)==0xc00);
 cv_options::on=true;assert(cv_title_input(0x4000)==0x4000);assert(cv_title_input(0x8000)==0x8000);assert(cv_title_input(8)==8);edges=0;
 assert(cv_controls_previous(0xc00)==0xc00);
 edges=0x4000;assert(cv_controls_previous(0xc00)==0x400);
 edges=0x8000;assert(cv_controls_previous(0xc00)==0x400);
 edges=0x2000;assert(cv_controls_previous(0xc00)==0xc00);
 edges=0x4000;flags=0;assert(cv_controls_previous(0xc00)==0xc00);
 flags=0x200;input=0;assert(cv_controls_previous(0xc00)==0xc00);
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.cpp';p.write_text(harness)
 subprocess.run(['c++','-std=c++20','-Wall','-Wextra','-Werror',str(p),'-o',d+'/test'],check=True)
 subprocess.run([d+'/test'],check=True)
print('Textkanten: gehaltene Tasten, neue Bestätigung, fremde Tasten und inaktiver Text geprüft.')
