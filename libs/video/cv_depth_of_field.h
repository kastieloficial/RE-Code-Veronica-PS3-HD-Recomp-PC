#ifndef CV_DEPTH_OF_FIELD_H
#define CV_DEPTH_OF_FIELD_H
/* Captured depth-weighted blur composite, identified by code without inline
 * constants. Preserve its destination's sharp scene and all other passes. */
static int cv_depth_of_field_skip(int enabled,int vp,int clear,int copy,
    unsigned rt,int exp32,unsigned bytes,unsigned code,int rts,
    unsigned vertices,unsigned mask,int depth_test,unsigned blend) {
 return !enabled&&vp&&!clear&&!copy&&rt&&!exp32&&bytes==416&&
        code==0x15F29A80u&&rts==1&&vertices==6&&mask==7&&!depth_test&&
        blend==0x0121118Bu;
}
#endif
