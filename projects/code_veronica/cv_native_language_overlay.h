#pragma once
#include <filesystem>
#include <string>
#include <cstring>
#include <cstdlib>
namespace cv_native_language {
// Produces a private cache with US names and US gameplay data preserved.
bool prepare_archive(const std::filesystem::path& original,const std::filesystem::path& translated,std::filesystem::path& result,std::string& error);
// US language resources come from the user's registered European installation.
// Resolve reads only. Never copy or modify the original game directories.
inline std::filesystem::path resolve(const char* title,const char* host,const char* root){
    if(!title||std::strcmp(title,"NPUB30467")||!host||!root||!*root)return {};
    std::string path(host);
    for(char& c:path){if(c=='\\')c='/';if(c>='A'&&c<='Z')c+=32;}
    const std::string marker="/biocv_disc/";const auto pos=path.rfind(marker);
    if(pos==std::string::npos)return {};
    const auto first=pos+marker.size(),slash=path.find('/',first);
    if(slash==std::string::npos)return {};
    const auto locale=path.substr(first,slash-first);
    if(locale!="usa"&&locale!="eng"&&locale!="fra"&&locale!="ger"&&locale!="ita"&&locale!="spa")return {};
    const auto relative=path.substr(slash+1);
    if(relative.empty()||relative.find(':')!=std::string::npos)return {};
    const std::filesystem::path suffix(relative);
    for(const auto& component:suffix)if(component==".."||component==".")return {};
    std::error_code error;const auto target=std::filesystem::path(root)/suffix;
    return std::filesystem::is_regular_file(target,error)?target:std::filesystem::path{};
}
inline const char* folder(int choice){
    static const char* names[]={"eng","ger","eng","fra","ita","spa"};
    return choice>=0&&choice<6?names[choice]:"eng";
}
}
