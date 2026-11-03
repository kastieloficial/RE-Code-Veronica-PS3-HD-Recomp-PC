#pragma once
#include <cstdlib>
#include <cstring>
namespace cv_options {
inline bool enabled(const char* name) {
#if defined(_MSC_VER)
    char value[8]={};size_t length=0;
    return getenv_s(&length,value,sizeof value,name)==0&&length==2&&value[0]=='1';
#else
    const char* value=std::getenv(name);
    return value&&std::strcmp(value,"1")==0;
#endif
}
}
