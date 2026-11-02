// Included inside the native camera namespace: text inspection cuts must not
// rebuild the scene lists, which also contain the original room messages.
#if !defined(CV_REGION_JP) && !defined(CV_REGION_US)
struct InspectionCut {
    bool valid=false;
    unsigned pending=0, hd_pending=0;
    uint32_t scene[scene_words]{}, mode=0, camera_type=0, eye[3]{}, target[3]{};
};
static InspectionCut inspection;
#endif
void inspection_before(bool enabled) {
#if !defined(CV_REGION_JP) && !defined(CV_REGION_US)
    const uint32_t system=vm_read32(0xA89BD0);
    const bool same=inspection.valid&&scene_matches(inspection.scene);
    inspection.valid=enabled&&system&&scene_read(inspection.scene);
    if(!same||!inspection.valid)inspection.pending=inspection.hd_pending=0;
    if(inspection.hd_pending)--inspection.hd_pending;
    if(!inspection.hd_pending)rsx_d3d12_camera_hold(0);
    if(!inspection.valid)return;
    inspection.mode=vm_read32(system+0x74);
    inspection.camera_type=vm_read32(0xBBE388);
    for(unsigned i=0;i<3;i++)inspection.eye[i]=vm_read32(0xBBE394+4*i);
#else
    (void)enabled;
#endif
}
bool inspection_after() {
#if !defined(CV_REGION_JP) && !defined(CV_REGION_US)
    if(!inspection.valid||!scene_matches(inspection.scene)) {
        inspection.valid=false;inspection.pending=inspection.hd_pending=0;rsx_d3d12_camera_hold(0);return false;
    }
    const uint32_t system=inspection.scene[1],mode=vm_read32(system+0x74);
    const uint32_t tasks=vm_read32(system+0x64)&~vm_read32(system+0x68);
    // Bit 0x2000 changes ownership for detail views; 0x100/0x200 vary by
    // room. Keep low-bit special camera modes and load/render paths excluded.
    if(!(tasks&0x80)||(tasks&0x1803F)||(vm_read32(system+0x6C)&0x300)||
       (mode&7)) {inspection.pending=inspection.hd_pending=0;rsx_d3d12_camera_hold(0);return false;}
    if(!(inspection.mode&7)&&((inspection.mode^mode)&0x2000))inspection.pending=3;
    // After loading, ownership may already be clear. The first text detail
    // view still changes camera type, without toggling the ownership bit.
    if(!(inspection.mode&7)&&!(mode&0x2000)&&
       inspection.camera_type!=2&&vm_read32(0xBBE388)==2&&
       (vm_read32(system+0x70)&0x200))inspection.pending=3;
    if(!inspection.pending)return false;
    --inspection.pending;
    // Ownership can change while the running camera still advances (0x128).
    // Wait for the actual detail/normal camera, not merely a moved eye.
    if(vm_read32(0xBBE388)!=((mode&0x2000)?0u:2u))return false;
    bool moved=false;
    for(unsigned i=0;i<3;i++)moved|=inspection.eye[i]!=vm_read32(0xBBE394+4*i);
    // Closing the text releases ownership one tick before restoring the eye.
    if(!moved)return false;
    inspection.pending=0;
    inspection.hd_pending=3;
    for(unsigned i=0;i<3;i++)inspection.target[i]=vm_read32(0xBBE394+4*i);
    return true;
#else
    return false;
#endif
}
#if !defined(CV_REGION_JP) && !defined(CV_REGION_US)
void inspection_hd_update(ppu_context* ctx) {
    const uint32_t manager=vm_read32(0xAFBAF0),hd=manager?vm_read32(manager+0x44):0;
    // Text preparation may leave derived vectors one logic tick behind the eye.
    // Keep the cut until this exact endpoint reaches the primary HD camera.
    bool arrived=true;
    for(unsigned i=0;i<3;i++)arrived&=inspection.target[i]==vm_read32(0xBBE3AC+4*i);
    if(inspection.hd_pending&&hd&&hd==ctx->gpr[3]&&scene_matches(inspection.scene)) {
        // The scene already uses the latest derived camera, even while its raw
        // eye points at the next detail view. Do not freeze an interpolated HD
        // position: refresh this scene's view with the previous projection mode.
        if(!arrived) {
            // This batch combines the old scene with new inspection state.
            // Execute its work, but keep the last complete image on screen.
            rsx_d3d12_camera_hold(1);
            const uint32_t system=inspection.scene[1],control=vm_read32(system+0x70);
            const uint32_t detail=(vm_read32(system+0x74)&0x2000)?1u:0u;
            vm_write32(system+0x70,(control&~1u)|detail);
            vm_write8(hd+0x180,static_cast<uint8_t>(detail));
            for(unsigned i=0;i<3;i++) {
                vm_write32(hd+0x140+4*i,vm_read32(0xBBE3AC+4*i));
                vm_write32(hd+0x160+4*i,vm_read32(0xBBE3A0+4*i));
            }
            vm_write32(hd+0x14C,0);vm_write32(hd+0x16C,0);
            for(unsigned i=0;i<4;i++)vm_write32(hd+0x150+4*i,0);
            func_0022C108(ctx);drain_scene(ctx);
            vm_write32(system+0x70,(vm_read32(system+0x70)&~1u)|(control&1u));
            return;
        }
        // Original FOV selection reads this cached detail flag before updating it.
        vm_write8(hd+0x180,(vm_read32(inspection.scene[1]+0x70)&1)?1:0);
        inspection.hd_pending=0;
        rsx_d3d12_camera_hold(0);
        rsx_d3d12_camera_cut();
        for(unsigned i=0;i<3;i++) {
            vm_write32(hd+0x140+4*i,vm_read32(0xBBE3AC+4*i));
            vm_write32(hd+0x160+4*i,vm_read32(0xBBE3A0+4*i));
        }
        vm_write32(hd+0x14C,0);vm_write32(hd+0x16C,0);
        for(unsigned i=0;i<4;i++)vm_write32(hd+0x150+4*i,0);
    }
    func_0022C108(ctx);
}
#endif
