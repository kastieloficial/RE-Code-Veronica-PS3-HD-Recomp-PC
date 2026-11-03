#include "../cv_native_language_overlay.h"
#include "../cv_appearance_assets.h"
#include "../cv_translation_patch.h"
#include <cassert>
#include <iostream>
using namespace cv_appearance;
static std::vector<Resource> load(const std::filesystem::path& file){std::vector<Resource> r;std::string e;assert(load_archive_resources(file,r,e));return r;}
int main(int argc,char** argv){
 assert(argc==3);const std::filesystem::path us=argv[1],eu=argv[2];unsigned runs=0,changed=0,protected_resources=0;
 for(const char* locale:{"fra","ger","ita","spa"})for(const char* rel:{"rdx_lnk/rm_0000.arc","adv/boot.arc","adv/opt.arc"}){
  const auto original=us/"usa"/rel,foreign=eu/locale/rel;std::vector<uint8_t> bytes;
  assert(cv_translation_patch::read(original,bytes));const auto before=cv_translation_patch::hash(bytes);
  std::filesystem::path cache,again;std::string error;
  if(!cv_native_language::prepare_archive(original,foreign,cache,error)){std::cerr<<error<<"\n";return 2;}
  assert(cv_native_language::prepare_archive(original,foreign,again,error)&&cache==again);
  const auto a=load(original),b=load(cache);assert(a.size()==b.size());
  for(size_t i=0;i<a.size();i++){
   assert(a[i].name==b[i].name&&a[i].type==b[i].type);
   const auto type=a[i].type;
   if(type!=0x46c78353&&type!=0x6a76e771&&type!=0x241f5deb){assert(a[i].bytes==b[i].bytes);++protected_resources;}
   else changed+=a[i].bytes!=b[i].bytes;
  }
  assert(cv_translation_patch::read(original,bytes)&&cv_translation_patch::hash(bytes)==before);++runs;
 }
 assert(runs==12&&changed>0&&protected_resources>0);
 std::cout<<"PASS: "<<runs<<" original US/EU archive combinations; "<<changed<<" translated resources; "<<protected_resources<<" US resources preserved byte-for-byte; US names and original files intact; cache reuse\n";
}
