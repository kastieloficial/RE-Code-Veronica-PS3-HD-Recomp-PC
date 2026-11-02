#pragma once
#include <vector>
#include <cmath>
#include <cstring>
#include <cstdio>

// EU inventory model commands own transformed vertices; sprites use other lists.
// Borrow only those arrays until the next logic tick, leaving all menu logic alone.
namespace cv_item_interpolation {
#if !defined(CV_REGION_JP) && !defined(CV_REGION_US)
constexpr uint32_t menu=0xCB8BA0;
struct Mesh {
    uint32_t command,model,vertices,normals,count;
    std::vector<uint32_t> words;
};
struct Pose {
    uint32_t system=0,object=0,item=0;
    std::vector<Mesh> meshes;
};
inline Pose current,previous;
inline bool paired=false,borrowed=false;
inline float as_float(uint32_t bits) {float f;std::memcpy(&f,&bits,4);return f;}
inline bool active() {
    const uint32_t system=vm_read32(0xA89BD0);
    if(!system)return false;
    const uint32_t tasks=vm_read32(system+0x64)&~vm_read32(system+0x68);
    return (tasks&0x200)&&!(tasks&0x180BF)&&(vm_read32(system+0x70)&8)&&
        (vm_read32(menu+0x44)>>16)==2&&(vm_read32(menu+0x48)&0x10);
}
inline bool same_target(const Pose& p) {
    if(!active()||p.system!=vm_read32(0xA89BD0)||p.object!=vm_read32(0xA83CD0)||
       p.item!=vm_read32(menu+0x54)||vm_read32(p.object+0x128)!=p.meshes.size())return false;
    const uint32_t list=vm_read32(p.object+0x134);
    if(!list)return false;
    for(unsigned i=0;i<p.meshes.size();i++) {
        const auto& m=p.meshes[i];
        if(vm_read32(list+4*i)!=m.command||vm_read32(m.command)!=0x8F0720||
           vm_read32(m.command+0x30)!=m.model||vm_read32(m.command+4)!=m.vertices||
           vm_read32(m.command+12)!=m.normals||vm_read32(m.command+16)!=m.count||
           vm_read32(m.command+20)!=m.count)return false;
    }
    return true;
}
inline void restore() {
    if(borrowed&&same_target(current))for(const auto& m:current.meshes)
        for(unsigned a=0;a<2;a++)for(unsigned i=0;i<m.count*4;i++)
            vm_write32((a?m.normals:m.vertices)+4*i,m.words[a*m.count*4+i]);
    borrowed=false;
}
inline void reset() {current={};previous={};paired=false;}
inline void capture() {
    if(!active()){reset();return;}
    Pose next;next.system=vm_read32(0xA89BD0);next.object=vm_read32(0xA83CD0);
    next.item=vm_read32(menu+0x54);
    if(!next.object){reset();return;}
    const uint32_t count=vm_read32(next.object+0x128),list=vm_read32(next.object+0x134);
    if(!count||count>32||!list){reset();return;}
    unsigned total=0;
    for(unsigned n=0;n<count;n++) {
        const uint32_t command=vm_read32(list+4*n);
        if(!command||vm_read32(command)!=0x8F0720){reset();return;}
        Mesh m{command,vm_read32(command+0x30),vm_read32(command+4),vm_read32(command+12),vm_read32(command+16),{}};
        total+=m.count;
        if(!m.model||!m.count||m.count>8192||total>32768||vm_read32(command+20)!=m.count||
           !m.vertices||!m.normals||m.vertices>0xFFFFFFFFu-m.count*16||
           m.normals>0xFFFFFFFFu-m.count*16){reset();return;}
        m.words.reserve(m.count*8);
        for(unsigned a=0;a<2;a++)for(unsigned i=0;i<m.count*4;i++) {
            const uint32_t bits=vm_read32((a?m.normals:m.vertices)+4*i);
            if(!std::isfinite(as_float(bits))){reset();return;}
            m.words.push_back(bits);
        }
        next.meshes.push_back(std::move(m));
    }
    paired=current.system==next.system&&current.object==next.object&&current.item==next.item&&
        current.meshes.size()==next.meshes.size();
    if(paired)for(unsigned i=0;i<next.meshes.size();i++)
        if(current.meshes[i].model!=next.meshes[i].model||current.meshes[i].count!=next.meshes[i].count)
            paired=false;
    previous=std::move(current);current=std::move(next);

}
inline void render(double phase) {
    if(!paired||!same_target(current))return;
    const float alpha=static_cast<float>(phase);
    for(unsigned m=0;m<current.meshes.size();m++) {
        const auto& now=current.meshes[m];const auto& before=previous.meshes[m];
        for(unsigned a=0;a<2;a++)for(unsigned i=0;i<now.count*4;i++) {
            // Preserve homogeneous/padding words exactly.
            if(i%4==3)continue;
            const unsigned index=a*now.count*4+i;
            const float p=as_float(before.words[index]),q=as_float(now.words[index]);
            const float value=p+(q-p)*alpha;uint32_t bits;std::memcpy(&bits,&value,4);
            vm_write32((a?now.normals:now.vertices)+4*i,bits);
        }
    }
    borrowed=true;
}
#else
inline void restore() {}
inline void reset() {}
inline void capture() {}
inline void render(double) {}
#endif
}
