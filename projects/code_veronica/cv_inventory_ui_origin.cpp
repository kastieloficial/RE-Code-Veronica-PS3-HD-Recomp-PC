#include "cv_scene_region.h"
#include "ppu_recomp.h"
#include "cv_inventory_ui_interpolation.h"
void cv_ui_original_enqueue(ppu_context*);
void func_00015AFC(ppu_context* ctx){
    using namespace cv_inventory_ui;
    const uint32_t command=uint32_t(ctx->gpr[4]);
    if(enabled()&&range(command,0x3C)&&vm_read32(command)==CV_SCENE_SPRITE_VTABLE){
        DrawRole role{};role[0]=uint32_t(ctx->lr);role[4]=uint32_t(ctx->gpr[31]);
        uint32_t sp=uint32_t(ctx->gpr[1]);
        // The generated PPC prologues save LR at parent SP + 0x10.
        // Bound each backchain step to its frame; never walk arbitrary pointers.
        for(unsigned i=1;i<=3;i++){
            if(sp<0x10000||sp>0xFFFF0000u)break;
            const uint32_t parent=uint32_t(vm_read64(sp));
            if(parent<=sp||parent-sp>0x10000||parent>0xFFFF0000u)break;
            role[i]=uint32_t(vm_read64(parent+0x10));sp=parent;
        }
        record(command,role);
    }
    cv_ui_original_enqueue(ctx);
}
