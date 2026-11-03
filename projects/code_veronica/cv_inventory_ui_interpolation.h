#pragma once
#include "cv_scene_region.h"
#include "cv_version.h"
#include <vector>
#include <array>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <algorithm>
#include <map>
#include <unordered_map>

// Region-mapped inventory interpolation. Native sprite constructors produce
// packets identified by CV_SCENE_SPRITE_VTABLE: +4 float4 positions, +8 RGBA words, +C vertex count,
// +38 integer texture coordinates. 00015AFC enqueues them in these lists.
// Borrow only drawing arrays, exactly as cv_item_interpolation does for meshes.
// No task, animation timer, input, inventory item or game state is advanced.
namespace cv_inventory_ui {
using DrawRole=std::array<uint32_t,6>;
using DrawId=std::array<uint32_t,7>;
inline std::unordered_map<uint32_t,DrawId> draw_ids;
inline std::map<DrawRole,uint32_t> draw_counts;
inline void begin(){draw_ids.clear();draw_counts.clear();}
inline void record(uint32_t command,const DrawRole& role){
    DrawId id{};std::copy(role.begin(),role.end(),id.begin());id[6]=draw_counts[role]++;draw_ids[command]=id;
}
struct Sprite {
    DrawId id{};
    uint32_t key=0,command=0,positions=0,colors=0,uv=0,count=0;
    std::array<uint32_t,6> state{};
    std::vector<uint32_t> xyzw,rgba,tex;
    int previous=-1;
};
// Equipment can change without a new tab or selected item. Cut that boundary
// before borrowing geometry from the previous owner of the blue panels.
using Selection=std::array<uint32_t,4>;
inline Selection selection_state(){
    const uint32_t character=vm_read8(CV_SRT_CHARACTER);
    return {vm_read32(CV_SCENE_INVENTORY+0x44),vm_read32(CV_SCENE_INVENTORY+0x54),
        character,character<4?vm_read32(CV_SRT_INVENTORY+character*0x40):0xFFFFFFFFu};
}
struct Frame {uint32_t system=0,object=0;Selection selection{};std::vector<Sprite> sprites;};
inline Frame current,previous;
inline bool borrowed=false;
inline unsigned captures=0;
inline bool enabled() {const char* e=std::getenv("CV_INVENTORY_UI_SMOOTH");return e&&!std::strcmp(e,"1");}
inline bool range(uint32_t p,uint32_t n) {return p>=0x10000&&p<0xBFFFF000u&&n<=0xBFFFF000u-p&&(p&3)==0;}
inline float real(uint32_t w) {float f;std::memcpy(&f,&w,4);return f;}
inline uint32_t word(float f) {uint32_t w;std::memcpy(&w,&f,4);return w;}
inline bool active() {
    if(!enabled())return false;
    const uint32_t sys=vm_read32(CV_SCENE_SYSTEM);
    if(!range(sys,0x78))return false;
    const uint32_t tasks=vm_read32(sys+0x64)&~vm_read32(sys+0x68);
    return (tasks&0x200)&&!(tasks&0x180BF)&&(vm_read32(sys+0x70)&8);
}
inline void reset() {current={};previous={};borrowed=false;}
inline bool live(const Sprite& s) {
    if(!range(s.command,0x3C)||vm_read32(s.command)!=CV_SCENE_SPRITE_VTABLE||
       vm_read32(s.command+4)!=s.positions||vm_read32(s.command+8)!=s.colors||
       vm_read32(s.command+12)!=s.count||vm_read32(s.command+0x38)!=s.uv)return false;
    for(unsigned i=0;i<6;i++)if(vm_read32(s.command+0x10+4*i)!=s.state[i])return false;
    return true;
}
inline bool owner() {return active()&&current.system==vm_read32(CV_SCENE_SYSTEM)&&current.object==vm_read32(CV_SCENE_RENDER);}
inline void restore() {
    if(borrowed&&owner())for(const auto& s:current.sprites)if(live(s)) {
        for(unsigned i=0;i<s.count;i++) {
            // Z, W and texture coordinates are never modified.
            vm_write32(s.positions+16*i,s.xyzw[4*i]);
            vm_write32(s.positions+16*i+4,s.xyzw[4*i+1]);
            vm_write32(s.colors+4*i,s.rgba[i]);
        }
    }
    borrowed=false;
}
inline void capture() {
    if(!active()){reset();return;}
    Frame next;next.system=vm_read32(CV_SCENE_SYSTEM);next.object=vm_read32(CV_SCENE_RENDER);
    if(!range(next.object,0x100)){reset();return;}
    next.selection=selection_state();
    constexpr uint32_t counts[]={0xC4,0xD8,0xEC};
    unsigned vertices=0;
    for(unsigned group=0;group<3;group++) {
        const uint32_t count=vm_read32(next.object+counts[group]);
        const uint32_t list=vm_read32(next.object+counts[group]+12);
        if(count>1024||(count&&!range(list,4*count))){reset();return;}
        for(unsigned i=0;i<count;i++) {
            Sprite s;s.key=(group<<16)|i;s.command=vm_read32(list+4*i);
            if(!range(s.command,0x3C)||vm_read32(s.command)!=CV_SCENE_SPRITE_VTABLE)continue;
            const auto origin=draw_ids.find(s.command);
            if(origin==draw_ids.end())continue;
            s.id=origin->second;
            s.positions=vm_read32(s.command+4);s.colors=vm_read32(s.command+8);
            s.count=vm_read32(s.command+12);s.uv=vm_read32(s.command+0x38);
            if(!s.count||s.count>512||!range(s.positions,16*s.count)||
               !range(s.colors,4*s.count)||!range(s.uv,8*s.count))continue;
            vertices+=s.count;if(vertices>16384){reset();return;}
            for(unsigned j=0;j<6;j++)s.state[j]=vm_read32(s.command+0x10+4*j);
            bool valid=true;
            for(unsigned j=0;j<s.count*4;j++) {
                auto v=vm_read32(s.positions+4*j);s.xyzw.push_back(v);
                if(j%4<2&&(!std::isfinite(real(v))||std::abs(real(v))>16384))valid=false;
            }
            if(!valid)continue;
            for(unsigned j=0;j<s.count;j++)s.rgba.push_back(vm_read32(s.colors+4*j));
            for(unsigned j=0;j<s.count*2;j++)s.tex.push_back(vm_read32(s.uv+4*j));
            next.sprites.push_back(std::move(s));
        }
    }
    previous=std::move(current);current=std::move(next);
    unsigned matched=0,moving=0;
    // Match the ORIGINAL draw role, not a screen-space neighbor or transient
    // list index. A changing glyph/icon can no longer disable a whole panel.
    std::map<DrawId,size_t> identities;
    if(previous.system==current.system&&previous.object==current.object&&
       previous.selection==current.selection)
        for(size_t i=0;i<previous.sprites.size();i++)identities.emplace(previous.sprites[i].id,i);
    for(auto& s:current.sprites) {
        const auto found=identities.find(s.id);if(found==identities.end())continue;
        const auto& p=previous.sprites[found->second];
        if((p.key>>16)!=(s.key>>16)||p.count!=s.count||p.state!=s.state||p.tex!=s.tex)continue;
        bool valid=true,changed=p.rgba!=s.rgba;
        for(unsigned v=0;v<s.count;v++) {
            for(unsigned k=0;k<2;k++) {
                const double delta=real(s.xyzw[4*v+k])-real(p.xyzw[4*v+k]);
                if(std::abs(delta)>128)valid=false;
                if(std::abs(delta)>0.001)changed=true;
            }
            // Original draw identity permits scaling and panel deformation;
            // depth and homogeneous terms retain their original ownership.
            if(p.xyzw[4*v+2]!=s.xyzw[4*v+2]||p.xyzw[4*v+3]!=s.xyzw[4*v+3])valid=false;
        }
        if(valid){s.previous=int(found->second);++matched;if(changed)++moving;}
    }
    static unsigned moving_reports=0;
    if(captures++%150==0||(moving&&moving_reports++<8))
        std::fprintf(stderr,"[INVENTORY-UI] origin-v5 sprites=%zu paired=%u animated=%u registered=%zu; mode=%08X item=%08X character=%u equipped=%u; logic unchanged at 30 Hz\n",current.sprites.size(),matched,moving,draw_ids.size(),current.selection[0],current.selection[1],current.selection[2],current.selection[3]);
}

inline uint32_t color(uint32_t a,uint32_t b,float t) {
    uint32_t out=0;for(unsigned shift=0;shift<32;shift+=8) {
        const float x=float((a>>shift)&255),y=float((b>>shift)&255);
        out|=uint32_t(std::lround(x+(y-x)*t))<<shift;
    }return out;
}
inline void render(double phase) {
    if(!owner()||!std::isfinite(phase))return;
    if(current.selection!=selection_state())return;
    // Each element owns a verified source identity; unrelated new content snaps.
    for(const auto& s:current.sprites)if(s.previous>=0&&!live(s))return;
    const float t=float(std::clamp(phase,0.0,1.0));
    for(const auto& s:current.sprites) {
        if(s.previous<0)continue;
        const auto& p=previous.sprites[size_t(s.previous)];
        for(unsigned i=0;i<s.count;i++) {
            for(unsigned k=0;k<2;k++) {
                const auto a=real(p.xyzw[4*i+k]),b=real(s.xyzw[4*i+k]);
                vm_write32(s.positions+16*i+4*k,word(a+(b-a)*t));
            }
            vm_write32(s.colors+4*i,color(p.rgba[i],s.rgba[i],t));
        }
        borrowed=true;
    }
}
}
