#include "cv_item_icons_hd.h"
#ifdef _WIN32
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#pragma comment(lib,"gdiplus.lib")

namespace cv_item_icons_hd {
namespace {
std::map<int,std::unique_ptr<Gdiplus::Bitmap>> cache;
std::unique_ptr<Gdiplus::Bitmap> load(int id) {
    wchar_t name[32];std::swprintf(name,32,L"ITEM_HD_%02X",id&255);
    const HMODULE module=GetModuleHandleW(nullptr);const HRSRC res=FindResourceW(module,name,MAKEINTRESOURCEW(10));
    if(!res)return nullptr;const DWORD size=SizeofResource(module,res);
    const void* data=LockResource(LoadResource(module,res));if(!data||!size||size>4*1024*1024)return nullptr;
    HGLOBAL block=GlobalAlloc(GMEM_MOVEABLE,size);if(!block)return nullptr;
    void* bytes=GlobalLock(block);if(!bytes){GlobalFree(block);return nullptr;}
    std::memcpy(bytes,data,size);GlobalUnlock(block);
    IStream* stream=nullptr;if(CreateStreamOnHGlobal(block,TRUE,&stream)!=S_OK){GlobalFree(block);return nullptr;}
    std::unique_ptr<Gdiplus::Bitmap> image;
    {Gdiplus::Bitmap source(stream);   // the stream must outlive the decode: Clone copies the pixels
        if(source.GetLastStatus()==Gdiplus::Ok&&source.GetWidth()&&source.GetWidth()<=2048)
            image.reset(source.Clone(0,0,int(source.GetWidth()),int(source.GetHeight()),PixelFormat32bppARGB));}
    stream->Release();return image;
}
}
Gdiplus::Bitmap* get(int id) {
    auto found=cache.find(id);if(found!=cache.end())return found->second.get();
    auto& slot=cache[id];slot=load(id);return slot.get();
}
void release() {cache.clear();}
}
#endif
