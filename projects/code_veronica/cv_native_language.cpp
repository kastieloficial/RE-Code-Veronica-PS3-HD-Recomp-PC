#include "cv_native_language_overlay.h"
#include "cv_appearance_prepare.h"
#include "cv_translation_patch.h"
#include <map>
#include <mutex>
#include <cstdio>
#include <stdexcept>
namespace cv_native_language {
namespace {
using Resource=cv_appearance::Resource;using Bytes=std::vector<uint8_t>;
std::string normalize(std::string value){
 for(const char* locale:{"eng","fra","ger","ita","spa"}){
  const std::string from=std::string("biocv_tmp\\")+locale+"\\";
  size_t at=0;while((at=value.find(from,at))!=std::string::npos){value.replace(at+10,3,"usa");at+=from.size();}
 }
 return value;
}
Bytes normalize_bytes(const Bytes& bytes){
 std::string s(reinterpret_cast<const char*>(bytes.data()),bytes.size());s=normalize(std::move(s));return Bytes(s.begin(),s.end());
}
bool localizable(uint32_t type){
 // Only message tables and texture pixels are locale content. Keep every
 // original US script, model, map layout and hashed resource reference.
 return type==0x46c78353||type==0x6a76e771||type==0x241f5deb;
}
std::vector<Resource> load(const std::filesystem::path& file){
 std::vector<Resource> result;std::string error;
 if(!cv_appearance::load_archive_resources(file,result,error))throw std::runtime_error(error);
 return result;
}
}
bool prepare_archive(const std::filesystem::path& original,const std::filesystem::path& translated,std::filesystem::path& result,std::string& error){
 static std::mutex mutex;std::lock_guard<std::mutex> guard(mutex);
 try{
  auto parent=translated.parent_path();
  // Locate the locale directory under biocv_disc without depending on depth.
  auto locale=parent;while(locale.has_parent_path()&&locale.parent_path().filename()!="biocv_disc"){
   if(locale==locale.parent_path())throw std::runtime_error("Invalid locale root");locale=locale.parent_path();
  }
  if(locale.parent_path().filename()!="biocv_disc")throw std::runtime_error("Invalid locale root");
  const auto relative=translated.lexically_relative(locale);
  const auto english=locale.parent_path()/"eng"/relative;
  Bytes us,base,target;
  if(!cv_translation_patch::read(original,us)||!cv_translation_patch::read(english,base)||!cv_translation_patch::read(translated,target))throw std::runtime_error("Cannot read native locale source");
  const auto key=cv_translation_patch::hash(us)+cv_translation_patch::hash(base)+cv_translation_patch::hash(target);
  if(key.size()!=192)throw std::runtime_error("Cannot identify locale sources");
  const Bytes identity(key.begin(),key.end());
  result=std::filesystem::temp_directory_path()/"ps3recomp-mods/cv-us-language/v3"/cv_translation_patch::hash(identity)/"biocv_disc/usa"/relative;
  if(std::filesystem::is_regular_file(result)){error.clear();return true;}
  auto records=load(original),reference=load(english),localized=load(translated);
  using Key=std::pair<std::string,uint32_t>;
  std::map<Key,const Resource*> en,foreign;
  for(const auto& r:reference)if(!en.emplace(Key{normalize(r.name),r.type},&r).second)throw std::runtime_error("Ambiguous English locale resource");
  for(const auto& r:localized)if(!foreign.emplace(Key{normalize(r.name),r.type},&r).second)throw std::runtime_error("Ambiguous translated locale resource");
  unsigned changes=0;
  for(auto& r:records){
   if(!localizable(r.type))continue;
   const Key k{normalize(r.name),r.type};const auto a=en.find(k),b=foreign.find(k);
   if(a==en.end()||b==foreign.end())continue;
   auto replacement=normalize_bytes(b->second->bytes);
   if(replacement!=normalize_bytes(a->second->bytes)){r.bytes=std::move(replacement);++changes;}
  }
  if(!changes){result=original;error.clear();return true;}
  auto generated=cv_appearance::encode_archive_resources(records);
  std::filesystem::create_directories(result.parent_path());auto pending=result;pending+=L".tmp";
  {std::ofstream stream(pending,std::ios::binary|std::ios::trunc);stream.write(reinterpret_cast<const char*>(generated.data()),generated.size());stream.close();if(!stream)throw std::runtime_error("Cannot write native locale cache");}
  if(!MoveFileExW(pending.c_str(),result.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Cannot finalize native locale cache");
  std::fprintf(stderr,"[LANGUAGE-US] adapted %s: %u localized resources; US names/gameplay preserved\n",relative.string().c_str(),changes);
  error.clear();return true;
 }catch(const std::exception& e){result.clear();error=e.what();return false;}
}
}
