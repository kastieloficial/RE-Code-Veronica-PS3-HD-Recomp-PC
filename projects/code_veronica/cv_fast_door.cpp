/* cv_fast_door.cpp -- Fast Door Skip + Sync I/O + Native x64 zlib Inflate for RE:CVX (PS3 EU).
 *
 * Bottlenecks fixed:
 *  1. bhSysCallDoordemo (0x001AFAE8): jump straight from case 0 to case 10
 *     (no-animation load screen path), skipping bhInitDoor / bhReadDoorData /
 *     bhSetDoor / bhExitDoor, and execute case 11 in the same frame.
 *  2. bhSysCallMonitor (0x001B0CB0) step 10 (func_001E2314):
 *     Stop Sound Bank 9 (CV_FN_STOP_BANK) so step 10 doesn't stall for ~3000 ms
 *     waiting for door/room SE to finish playing.
 *  3. bhSysCallMonitor + bhSysCallSndMonitor (0x001B2A48):
 *     Collapse synchronous step-per-frame transitions into a single frame.
 *  4. MtFile::read (0x007846D0):
 *     Avoid spawning & joining ~32 PPU threads ("readAsyncProc") per room .arc
 *     by syncing the OS fd via MtFile::seek and reading synchronously.
 *  5. decodeProc SPU zlib inflate (0x00538AEC / 0x00539B0C / loc_00539EB0):
 *     Replace per-chunk SPU zlib inflate jobs (0x0099F880 / 0x00917E00) with
 *     native x64 zlib 1.3.2 inflate() directly on guest memory.
 *
 * Environment controls:
 *   CV_FAST_DOOR=0       -- disable fast door hooks
 *   CV_FAST_DOOR_FADE=1  -- snap door fade-out / fade-in to 1 frame
 *   CV_FAST_DOOR_SYNC=0  -- disable synchronous MtFile::read optimization
 *   CV_NATIVE_ZLIB=0     -- disable native x64 zlib inflate (fall back to SPU)
 *   CV_FAST_DOOR_LOG=1   -- log detailed per-stage timing to stderr and door-profile.txt
 */
#include "ppu_recomp.h"
#include "cv_version.h"
#include "zlib.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <thread>

#if defined(CV_REGION_US)
#define CV_FD_FN_DOOR_DEMO       func_001AE078
#define CV_FD_ORIG_DOOR_DEMO     cv_original_001AE078
#define CV_FD_FN_MONITOR         func_001AF240
#define CV_FD_ORIG_MONITOR       cv_original_001AF240
#define CV_FD_FN_SND_MON         func_001B0FD8
#define CV_FD_ORIG_SND_MON       cv_original_001B0FD8
#define CV_FD_FN_MT_READ         func_00782710
#define CV_FD_ORIG_MT_READ       cv_original_00782710
#define CV_FD_FN_MT_READ_ASYNC   func_0078379C
#define CV_FD_ORIG_MT_READ_ASYNC cv_original_0078379C
#define CV_FD_FN_MT_SEEK         func_00783934
#define CV_FD_FN_ZLIB_CTOR       func_00536B2C
#define CV_FD_ORIG_ZLIB_CTOR     cv_original_00536B2C
#define CV_FD_FN_ZLIB_DTOR       func_00537B4C
#define CV_FD_ORIG_ZLIB_DTOR     cv_original_00537B4C
#define CV_FD_FN_CALC_POINT4     func_001CD7B4
#define CV_FD_ORIG_CALC_POINT4   cv_original_001CD7B4
#define CV_FD_FN_KWAY_SORT       func_00285790
#define CV_FD_ORIG_KWAY_SORT     cv_original_00285790
#define CV_FD_FN_VTX_XFORM       func_001D14D0
#define CV_FD_ORIG_VTX_XFORM     cv_original_001D14D0
#define CV_FD_FN_MULTI_MATRIX    func_001CCB10
#define CV_FD_ORIG_MULTI_MATRIX  cv_original_001CCB10
#define CV_FD_FN_VTX_PACKER      func_004B8B00
#define CV_FD_ORIG_VTX_PACKER    cv_original_004B8B00
#define CV_FD_FN_VB_BUILDER      func_0001D5D0
#define CV_FD_ORIG_VB_BUILDER    cv_original_0001D5D0
#define CV_FD_FN_MERGE_SORT2     func_00284EB4
#define CV_FD_ORIG_MERGE_SORT2   cv_original_00284EB4
#define CV_FD_FN_DRAW_DESC_ALLOC func_0027CFD0
#define CV_FD_ORIG_DRAW_DESC_ALLOC cv_original_0027CFD0
#define CV_FD_FN_BUMP_ALLOC      func_000124F4
#define CV_FD_ORIG_BUMP_ALLOC    cv_original_000124F4
#define CV_FD_FN_TREE_TRAV       func_00547FBC
#define CV_FD_ORIG_TREE_TRAV     cv_original_00547FBC
#define CV_FD_FN_DRAW_MODEL      func_001D0208
#define CV_FD_ORIG_DRAW_MODEL    cv_original_001D0208
#define CV_FD_FN_TEST_SPHERE     func_00283910
#define CV_FD_ORIG_TEST_SPHERE   cv_original_00283910
#define CV_FD_FN_MAT4_VEC4       func_00202464
#define CV_FD_ORIG_MAT4_VEC4     cv_original_00202464
#define CV_FD_FN_CULL_PARTS      func_00203788
#define CV_FD_ORIG_CULL_PARTS    cv_original_00203788
#define CV_FD_FN_ENQUEUE_TRANS   func_00015DDC
#define CV_FD_ORIG_ENQUEUE_TRANS cv_original_00015DDC
#define CV_FD_FN_HEAP_ALLOC_INNER   func_006D8B4C
#define CV_FD_ORIG_HEAP_ALLOC_INNER cv_original_006D8B4C
#define CV_FD_FN_XFORM_VEC3      func_001CC7C0
#define CV_FD_ORIG_XFORM_VEC3    cv_original_001CC7C0
#define CV_FD_FN_DEPTH_MERGE     func_003DBAB8
#define CV_FD_ORIG_DEPTH_MERGE   cv_original_003DBAB8
#define CV_FD_FN_DESC_LOOKUP     func_007BE4D8
#define CV_FD_ORIG_DESC_LOOKUP   cv_original_007BE4D8
#define CV_FD_FN_EMIT_SPRITES    func_001D2E60
#define CV_FD_ORIG_EMIT_SPRITES  cv_original_001D2E60
#define CV_FD_FN_EMIT_STRIP      func_001C6A10
#define CV_FD_ORIG_EMIT_STRIP    cv_original_001C6A10
#define CV_FD_FN_DRAWCMD_DTOR    func_007C0D68
#define CV_FD_ORIG_DRAWCMD_DTOR  cv_original_007C0D68
#define CV_FD_FN_DRAWELEM_DTOR   func_007C0F68
#define CV_FD_ORIG_DRAWELEM_DTOR cv_original_007C0F68
#define CV_FD_FN_HEAP_FREE       func_006D94FC
#define CV_FD_ORIG_HEAP_FREE     cv_original_006D94FC
#define CV_FD_FN_HEAP_ALLOC_BKT  func_006D90DC
#define CV_FD_ORIG_HEAP_ALLOC_BKT cv_original_006D90DC
#define CV_FD_FN_DEPTH_KEY       func_00016658
#define CV_FD_ORIG_DEPTH_KEY     cv_original_00016658
#define CV_FD_FN_TEX_LOOKUP      func_0001638C
#define CV_FD_ORIG_TEX_LOOKUP    cv_original_0001638C
#define CV_FD_FN_RENDER_STATE    func_00016838
#define CV_FD_ORIG_RENDER_STATE  cv_original_00016838
#define CV_FD_FN_PACK_VTX64      func_007BE738
#define CV_FD_ORIG_PACK_VTX64    cv_original_007BE738
#define CV_FD_FN_DRAW_EFFECT     func_00016E10
#define CV_FD_ORIG_DRAW_EFFECT   cv_original_00016E10
#define CV_FD_FN_SYNC_PARAM      func_000102AC
#define CV_FD_ORIG_SYNC_PARAM    cv_original_000102AC
#define CV_FD_FN_DRAW_EFF_LIST   func_00017EB4
#define CV_FD_ORIG_DRAW_EFF_LIST cv_original_00017EB4
#define CV_FD_FN_HASH_FIND       func_002680BC
#define CV_FD_ORIG_HASH_FIND     cv_original_002680BC
#define CV_FD_FN_HASH_GET        func_00267DC4
#define CV_FD_ORIG_HASH_GET      cv_original_00267DC4
#define CV_FD_FN_UPLOAD_MESH     func_00019A94
#define CV_FD_ORIG_UPLOAD_MESH   cv_original_00019A94
#define CV_FD_FN_DRAW_EFF_ALT    func_0001CD40
#define CV_FD_FN_HEAP_COALESCE   func_0084059C
#define CV_FD_FN_GET_MAT         func_00283E9C
#define CV_FD_FN_DEPTH_FALLBACK  func_004B7548
#define CV_FD_FN_TEX_FIND_IDX    func_002461C8
#define CV_FD_FN_TEX_GET_BY_IDX  func_00246210
#define CV_FD_FN_TEX_GET_SUB     func_00246240
#define CV_FD_FN_VB_FLUSH        func_003E18B8
#define CV_FD_FN_CONST_FLUSH     func_0027B130
#define CV_FD_FN_LIGHT_QUERY     func_002148F0
#define CV_FD_FN_GET_SC          func_005191BC
#define CV_FD_FN_SET_TEX_DIRECT  func_007BE2C8
#define CV_FD_FN_PACK_VTX_NOTEX  func_007BEF10
#define CV_FD_FN_PACK_VTX_ALT    func_007BF590
#elif defined(CV_REGION_JP)
#define CV_FD_FN_DOOR_DEMO       func_001AE120
#define CV_FD_ORIG_DOOR_DEMO     cv_original_001AE120
#define CV_FD_FN_MONITOR         func_001AF2E8
#define CV_FD_ORIG_MONITOR       cv_original_001AF2E8
#define CV_FD_FN_SND_MON         func_001B1080
#define CV_FD_ORIG_SND_MON       cv_original_001B1080
#define CV_FD_FN_MT_READ         func_00783000
#define CV_FD_ORIG_MT_READ       cv_original_00783000
#define CV_FD_FN_MT_READ_ASYNC   func_0078408C
#define CV_FD_ORIG_MT_READ_ASYNC cv_original_0078408C
#define CV_FD_FN_MT_SEEK         func_00784224
#define CV_FD_FN_ZLIB_CTOR       func_0053741C
#define CV_FD_ORIG_ZLIB_CTOR     cv_original_0053741C
#define CV_FD_FN_ZLIB_DTOR       func_0053843C
#define CV_FD_ORIG_ZLIB_DTOR     cv_original_0053843C
#define CV_FD_FN_CALC_POINT4     func_001CD934
#define CV_FD_ORIG_CALC_POINT4   cv_original_001CD934
#define CV_FD_FN_KWAY_SORT       func_00286080
#define CV_FD_ORIG_KWAY_SORT     cv_original_00286080
#define CV_FD_FN_VTX_XFORM       func_001D1650
#define CV_FD_ORIG_VTX_XFORM     cv_original_001D1650
#define CV_FD_FN_MULTI_MATRIX    func_001CCC90
#define CV_FD_ORIG_MULTI_MATRIX  cv_original_001CCC90
#define CV_FD_FN_VTX_PACKER      func_004B93F0
#define CV_FD_ORIG_VTX_PACKER    cv_original_004B93F0
#define CV_FD_FN_VB_BUILDER      func_0001D5D0
#define CV_FD_ORIG_VB_BUILDER    cv_original_0001D5D0
#define CV_FD_FN_MERGE_SORT2     func_002857A4
#define CV_FD_ORIG_MERGE_SORT2   cv_original_002857A4
#define CV_FD_FN_DRAW_DESC_ALLOC func_0027D8C0
#define CV_FD_ORIG_DRAW_DESC_ALLOC cv_original_0027D8C0
#define CV_FD_FN_BUMP_ALLOC      func_000124F4
#define CV_FD_ORIG_BUMP_ALLOC    cv_original_000124F4
#define CV_FD_FN_TREE_TRAV       func_005488AC
#define CV_FD_ORIG_TREE_TRAV     cv_original_005488AC
#define CV_FD_FN_DRAW_MODEL      func_001D0388
#define CV_FD_ORIG_DRAW_MODEL    cv_original_001D0388
#define CV_FD_FN_TEST_SPHERE     func_00284200
#define CV_FD_ORIG_TEST_SPHERE   cv_original_00284200
#define CV_FD_FN_MAT4_VEC4       func_00202D54
#define CV_FD_ORIG_MAT4_VEC4     cv_original_00202D54
#define CV_FD_FN_CULL_PARTS      func_00204078
#define CV_FD_ORIG_CULL_PARTS    cv_original_00204078
#define CV_FD_FN_ENQUEUE_TRANS   func_00015DDC
#define CV_FD_ORIG_ENQUEUE_TRANS cv_original_00015DDC
#define CV_FD_FN_HEAP_ALLOC_INNER   func_006D943C
#define CV_FD_ORIG_HEAP_ALLOC_INNER cv_original_006D943C
#define CV_FD_FN_XFORM_VEC3      func_001CC940
#define CV_FD_ORIG_XFORM_VEC3    cv_original_001CC940
#define CV_FD_FN_DEPTH_MERGE     func_003DC3A8
#define CV_FD_ORIG_DEPTH_MERGE   cv_original_003DC3A8
#define CV_FD_FN_DESC_LOOKUP     func_007BEDC8
#define CV_FD_ORIG_DESC_LOOKUP   cv_original_007BEDC8
#define CV_FD_FN_EMIT_SPRITES    func_001D2FE0
#define CV_FD_ORIG_EMIT_SPRITES  cv_original_001D2FE0
#define CV_FD_FN_EMIT_STRIP      func_001C6B90
#define CV_FD_ORIG_EMIT_STRIP    cv_original_001C6B90
#define CV_FD_FN_DRAWCMD_DTOR    func_007C1658
#define CV_FD_ORIG_DRAWCMD_DTOR  cv_original_007C1658
#define CV_FD_FN_DRAWELEM_DTOR   func_007C1858
#define CV_FD_ORIG_DRAWELEM_DTOR cv_original_007C1858
#define CV_FD_FN_HEAP_FREE       func_006D9DEC
#define CV_FD_ORIG_HEAP_FREE     cv_original_006D9DEC
#define CV_FD_FN_HEAP_ALLOC_BKT  func_006D99CC
#define CV_FD_ORIG_HEAP_ALLOC_BKT cv_original_006D99CC
#define CV_FD_FN_DEPTH_KEY       func_00016658
#define CV_FD_ORIG_DEPTH_KEY     cv_original_00016658
#define CV_FD_FN_TEX_LOOKUP      func_0001638C
#define CV_FD_ORIG_TEX_LOOKUP    cv_original_0001638C
#define CV_FD_FN_RENDER_STATE    func_00016838
#define CV_FD_ORIG_RENDER_STATE  cv_original_00016838
#define CV_FD_FN_PACK_VTX64      func_007BF028
#define CV_FD_ORIG_PACK_VTX64    cv_original_007BF028
#define CV_FD_FN_DRAW_EFFECT     func_00016E10
#define CV_FD_ORIG_DRAW_EFFECT   cv_original_00016E10
#define CV_FD_FN_SYNC_PARAM      func_000102AC
#define CV_FD_ORIG_SYNC_PARAM    cv_original_000102AC
#define CV_FD_FN_DRAW_EFF_LIST   func_00017EB4
#define CV_FD_ORIG_DRAW_EFF_LIST cv_original_00017EB4
#define CV_FD_FN_HASH_FIND       func_002689AC
#define CV_FD_ORIG_HASH_FIND     cv_original_002689AC
#define CV_FD_FN_HASH_GET        func_002686B4
#define CV_FD_ORIG_HASH_GET      cv_original_002686B4
#define CV_FD_FN_UPLOAD_MESH     func_00019A94
#define CV_FD_ORIG_UPLOAD_MESH   cv_original_00019A94
#define CV_FD_FN_DRAW_EFF_ALT    func_0001CD40
#define CV_FD_FN_HEAP_COALESCE   func_00840E8C
#define CV_FD_FN_GET_MAT         func_0028478C
#define CV_FD_FN_DEPTH_FALLBACK  func_004B7E38
#define CV_FD_FN_TEX_FIND_IDX    func_00246AB8
#define CV_FD_FN_TEX_GET_BY_IDX  func_00246B00
#define CV_FD_FN_TEX_GET_SUB     func_00246B30
#define CV_FD_FN_VB_FLUSH        func_003E21A8
#define CV_FD_FN_CONST_FLUSH     func_0027BA20
#define CV_FD_FN_LIGHT_QUERY     func_002151E0
#define CV_FD_FN_GET_SC          func_00519AAC
#define CV_FD_FN_SET_TEX_DIRECT  func_007BEBB8
#define CV_FD_FN_PACK_VTX_NOTEX  func_007BF800
#define CV_FD_FN_PACK_VTX_ALT    func_007BFE80
#else /* EU */
#define CV_FD_FN_DOOR_DEMO       func_001AFAE8
#define CV_FD_ORIG_DOOR_DEMO     cv_original_001AFAE8
#define CV_FD_FN_MONITOR         func_001B0CB0
#define CV_FD_ORIG_MONITOR       cv_original_001B0CB0
#define CV_FD_FN_SND_MON         func_001B2A48
#define CV_FD_ORIG_SND_MON       cv_original_001B2A48
#define CV_FD_FN_MT_READ         func_007846D0
#define CV_FD_ORIG_MT_READ       cv_original_007846D0
#define CV_FD_FN_MT_READ_ASYNC   func_0078575C
#define CV_FD_ORIG_MT_READ_ASYNC cv_original_0078575C
#define CV_FD_FN_MT_SEEK         func_007858F4
#define CV_FD_FN_ZLIB_CTOR       func_00538AEC
#define CV_FD_ORIG_ZLIB_CTOR     cv_original_00538AEC
#define CV_FD_FN_ZLIB_DTOR       func_00539B0C
#define CV_FD_ORIG_ZLIB_DTOR     cv_original_00539B0C
#define CV_FD_FN_CALC_POINT4     func_001CF5D4
#define CV_FD_ORIG_CALC_POINT4   cv_original_001CF5D4
#define CV_FD_FN_KWAY_SORT       func_00287750
#define CV_FD_ORIG_KWAY_SORT     cv_original_00287750
#define CV_FD_FN_VTX_XFORM       func_001D32F0
#define CV_FD_ORIG_VTX_XFORM     cv_original_001D32F0
#define CV_FD_FN_MULTI_MATRIX    func_001CE930
#define CV_FD_ORIG_MULTI_MATRIX  cv_original_001CE930
#define CV_FD_FN_VTX_PACKER      func_004BAAC0
#define CV_FD_ORIG_VTX_PACKER    cv_original_004BAAC0
#define CV_FD_FN_VB_BUILDER      func_0001D5D0
#define CV_FD_ORIG_VB_BUILDER    cv_original_0001D5D0
#define CV_FD_FN_MERGE_SORT2     func_00286E74
#define CV_FD_ORIG_MERGE_SORT2   cv_original_00286E74
#define CV_FD_FN_DRAW_DESC_ALLOC func_0027EF90
#define CV_FD_ORIG_DRAW_DESC_ALLOC cv_original_0027EF90
#define CV_FD_FN_BUMP_ALLOC      func_000124F4
#define CV_FD_ORIG_BUMP_ALLOC    cv_original_000124F4
#define CV_FD_FN_TREE_TRAV       func_00549F7C
#define CV_FD_ORIG_TREE_TRAV     cv_original_00549F7C
#define CV_FD_FN_DRAW_MODEL      func_001D2028
#define CV_FD_ORIG_DRAW_MODEL    cv_original_001D2028
#define CV_FD_FN_TEST_SPHERE     func_002858D0
#define CV_FD_ORIG_TEST_SPHERE   cv_original_002858D0
#define CV_FD_FN_MAT4_VEC4       func_00204404
#define CV_FD_ORIG_MAT4_VEC4     cv_original_00204404
#define CV_FD_FN_CULL_PARTS      func_00205728
#define CV_FD_ORIG_CULL_PARTS    cv_original_00205728
#define CV_FD_FN_ENQUEUE_TRANS   func_00015DDC
#define CV_FD_ORIG_ENQUEUE_TRANS cv_original_00015DDC
#define CV_FD_FN_HEAP_ALLOC_INNER   func_006DAB0C
#define CV_FD_ORIG_HEAP_ALLOC_INNER cv_original_006DAB0C
#define CV_FD_FN_XFORM_VEC3      func_001CE5E0
#define CV_FD_ORIG_XFORM_VEC3    cv_original_001CE5E0
#define CV_FD_FN_DEPTH_MERGE     func_003DDA78
#define CV_FD_ORIG_DEPTH_MERGE   cv_original_003DDA78
#define CV_FD_FN_DESC_LOOKUP     func_007C0498
#define CV_FD_ORIG_DESC_LOOKUP   cv_original_007C0498
#define CV_FD_FN_EMIT_SPRITES    func_001D4C80
#define CV_FD_ORIG_EMIT_SPRITES  cv_original_001D4C80
#define CV_FD_FN_EMIT_STRIP      func_001C8830
#define CV_FD_ORIG_EMIT_STRIP    cv_original_001C8830
#define CV_FD_FN_DRAWCMD_DTOR    func_007C2D28
#define CV_FD_ORIG_DRAWCMD_DTOR  cv_original_007C2D28
#define CV_FD_FN_DRAWELEM_DTOR   func_007C2F28
#define CV_FD_ORIG_DRAWELEM_DTOR cv_original_007C2F28
#define CV_FD_FN_HEAP_FREE       func_006DB4BC
#define CV_FD_ORIG_HEAP_FREE     cv_original_006DB4BC
#define CV_FD_FN_HEAP_ALLOC_BKT  func_006DB09C
#define CV_FD_ORIG_HEAP_ALLOC_BKT cv_original_006DB09C
#define CV_FD_FN_DEPTH_KEY       func_00016658
#define CV_FD_ORIG_DEPTH_KEY     cv_original_00016658
#define CV_FD_FN_TEX_LOOKUP      func_0001638C
#define CV_FD_ORIG_TEX_LOOKUP    cv_original_0001638C
#define CV_FD_FN_RENDER_STATE    func_00016838
#define CV_FD_ORIG_RENDER_STATE  cv_original_00016838
#define CV_FD_FN_PACK_VTX64      func_007C06F8
#define CV_FD_ORIG_PACK_VTX64    cv_original_007C06F8
#define CV_FD_FN_DRAW_EFFECT     func_00016E10
#define CV_FD_ORIG_DRAW_EFFECT   cv_original_00016E10
#define CV_FD_FN_SYNC_PARAM      func_000102AC
#define CV_FD_ORIG_SYNC_PARAM    cv_original_000102AC
#define CV_FD_FN_DRAW_EFF_LIST   func_00017EB4
#define CV_FD_ORIG_DRAW_EFF_LIST cv_original_00017EB4
#define CV_FD_FN_HASH_FIND       func_0026A08C
#define CV_FD_ORIG_HASH_FIND     cv_original_0026A08C
#define CV_FD_FN_HASH_GET        func_00269D94
#define CV_FD_ORIG_HASH_GET      cv_original_00269D94
#define CV_FD_FN_UPLOAD_MESH     func_00019A94
#define CV_FD_ORIG_UPLOAD_MESH   cv_original_00019A94
#define CV_FD_FN_DRAW_EFF_ALT    func_0001CD40
#define CV_FD_FN_HEAP_COALESCE   func_0084255C
#define CV_FD_FN_GET_MAT         func_00285E5C
#define CV_FD_FN_DEPTH_FALLBACK  func_004B9508
#define CV_FD_FN_TEX_FIND_IDX    func_00248198
#define CV_FD_FN_TEX_GET_BY_IDX  func_002481E0
#define CV_FD_FN_TEX_GET_SUB     func_00248210
#define CV_FD_FN_VB_FLUSH        func_003E3878
#define CV_FD_FN_CONST_FLUSH     func_0027D0F0
#define CV_FD_FN_LIGHT_QUERY     func_002168C0
#define CV_FD_FN_GET_SC          func_0051B17C
#define CV_FD_FN_SET_TEX_DIRECT  func_007C0288
#define CV_FD_FN_PACK_VTX_NOTEX  func_007C0ED0
#define CV_FD_FN_PACK_VTX_ALT    func_007C1550
#endif

void CV_FD_ORIG_DOOR_DEMO(ppu_context* ctx);      /* bhSysCallDoordemo */
void CV_FD_ORIG_MONITOR(ppu_context* ctx);        /* bhSysCallMonitor */
void CV_FD_ORIG_SND_MON(ppu_context* ctx);        /* bhSysCallSndMonitor */
void CV_FD_ORIG_MT_READ(ppu_context* ctx);        /* MtFile::read (sync/async) */
void CV_FD_ORIG_MT_READ_ASYNC(ppu_context* ctx);  /* MtFile::readAsync */
void CV_FD_ORIG_ZLIB_CTOR(ppu_context* ctx);      /* decodeProc zlib stream ctor */
void CV_FD_ORIG_ZLIB_DTOR(ppu_context* ctx);      /* decodeProc zlib stream dtor */
void CV_FD_ORIG_CALC_POINT4(ppu_context* ctx);    /* njCalcPoint4 (mat4 * vec3) */
void CV_FD_ORIG_KWAY_SORT(ppu_context* ctx);      /* K-way draw-call merge sort */
void CV_FD_ORIG_VTX_XFORM(ppu_context* ctx);      /* vertex + normal + bbox transform */
void CV_FD_ORIG_MULTI_MATRIX(ppu_context* ctx);   /* njMultiMatrix (affine 3x4) */
void CV_FD_ORIG_VTX_PACKER(ppu_context* ctx);     /* vertex attribute packer (14 formats) */
void CV_FD_ORIG_VB_BUILDER(ppu_context* ctx);     /* vertex buffer builder (strips/lists) */
void CV_FD_ORIG_MERGE_SORT2(ppu_context* ctx);    /* 2-way bottom-up merge sort (8B pairs) */
void CV_FD_ORIG_DRAW_DESC_ALLOC(ppu_context* ctx);/* vertex buffer + draw desc allocator */
void CV_FD_ORIG_BUMP_ALLOC(ppu_context* ctx);     /* atomic scratch bump allocator */
void CV_FD_ORIG_TREE_TRAV(ppu_context* ctx);      /* recursive tree traversal */
void CV_FD_ORIG_DRAW_MODEL(ppu_context* ctx);     /* njCnkDrawModel */
void CV_FD_ORIG_TEST_SPHERE(ppu_context* ctx);    /* cCamera::testSphere (6-plane) */
void CV_FD_ORIG_MAT4_VEC4(ppu_context* ctx);      /* mat4x4 * vec4 */
void CV_FD_ORIG_CULL_PARTS(ppu_context* ctx);     /* uModel::cullParts */
void CV_FD_ORIG_ENQUEUE_TRANS(ppu_context* ctx);  /* translucent/effect command enqueue */
void CV_FD_ORIG_HEAP_ALLOC_INNER(ppu_context* ctx); /* MtHeapAllocator first-fit free-list alloc */
void CV_FD_ORIG_XFORM_VEC3(ppu_context* ctx);     /* Vec3[count] * affine 4x3 matrix */
void CV_FD_ORIG_DEPTH_MERGE(ppu_context* ctx);    /* 8-byte translucent depth-sort merge */
void CV_FD_ORIG_DESC_LOOKUP(ppu_context* ctx);    /* 5x u32 descriptor linear lookup/insert */
void CV_FD_ORIG_EMIT_SPRITES(ppu_context* ctx);   /* billboard/particle sprite array emitter */
void CV_FD_ORIG_EMIT_STRIP(ppu_context* ctx);     /* polygon/strip particle emitter */
void CV_FD_ORIG_DRAWCMD_DTOR(ppu_context* ctx);   /* draw command container destructor */
void CV_FD_ORIG_DRAWELEM_DTOR(ppu_context* ctx);  /* draw command element destructor */
void CV_FD_ORIG_HEAP_FREE(ppu_context* ctx);      /* MtHeapAllocator::free */
void CV_FD_ORIG_HEAP_ALLOC_BKT(ppu_context* ctx); /* MtHeapAllocator::alloc_bucket */
void CV_FD_ORIG_DEPTH_KEY(ppu_context* ctx);      /* translucent depth-sort key calc */
void CV_FD_ORIG_TEX_LOOKUP(ppu_context* ctx);     /* texture/material descriptor lookup */
void CV_FD_ORIG_RENDER_STATE(ppu_context* ctx);   /* render-state/blend bitfield builder */
void CV_FD_ORIG_PACK_VTX64(ppu_context* ctx);     /* 64B->32B vertex packer + draw enqueue */
void CV_FD_ORIG_DRAW_EFFECT(ppu_context* ctx);    /* effect/particle/translucent mesh drawer */
void CV_FD_ORIG_SYNC_PARAM(ppu_context* ctx);     /* 4B global param sync */
void CV_FD_ORIG_DRAW_EFF_LIST(ppu_context* ctx);  /* effect list dispatcher */
void CV_FD_ORIG_HASH_FIND(ppu_context* ctx);      /* open-addressing hash table find */
void CV_FD_ORIG_HASH_GET(ppu_context* ctx);       /* hash table get value by key */
void CV_FD_ORIG_UPLOAD_MESH(ppu_context* ctx);    /* mesh vertex buffer upload */
void CV_FD_FN_DRAW_EFF_ALT(ppu_context* ctx);
void CV_FD_FN_DRAW_DESC_ALLOC(ppu_context* ctx);
void CV_FD_FN_DRAWELEM_DTOR(ppu_context* ctx);
void CV_FD_FN_HEAP_FREE(ppu_context* ctx);
void CV_FD_FN_HEAP_ALLOC_INNER(ppu_context* ctx);
void CV_FD_FN_HEAP_COALESCE(ppu_context* ctx);
void CV_FD_FN_GET_MAT(ppu_context* ctx);
void CV_FD_FN_DEPTH_FALLBACK(ppu_context* ctx);
void CV_FD_FN_TEX_FIND_IDX(ppu_context* ctx);
void CV_FD_FN_TEX_GET_BY_IDX(ppu_context* ctx);
void CV_FD_FN_TEX_GET_SUB(ppu_context* ctx);
void CV_FD_FN_VB_FLUSH(ppu_context* ctx);
void CV_FD_FN_CONST_FLUSH(ppu_context* ctx);
void CV_FD_FN_LIGHT_QUERY(ppu_context* ctx);
void CV_FD_FN_GET_SC(ppu_context* ctx);
void CV_FD_FN_SET_TEX_DIRECT(ppu_context* ctx);
void CV_FD_FN_PACK_VTX_NOTEX(ppu_context* ctx);
void CV_FD_FN_PACK_VTX_ALT(ppu_context* ctx);
void CV_FD_FN_BUMP_ALLOC(ppu_context* ctx);
void CV_FD_FN_XFORM_VEC3(ppu_context* ctx);
void CV_FD_FN_DESC_LOOKUP(ppu_context* ctx);
void cv_ui_original_enqueue(ppu_context* ctx);    /* UI/draw command vector push_back */
void CV_FD_FN_MT_SEEK(ppu_context* ctx);          /* MtFile::seek(obj, off, origin) */
void CV_FN_STOP_BANK(ppu_context* ctx);           /* (sound_mgr, bank_id) */

void CV_FD_FN_SND_MON(ppu_context* ctx);

extern "C" uint8_t* vm_base;
extern "C" void ps3_indirect_call(ppu_context* ctx);
extern "C" void ps3_hle_call(uint32_t nid, ppu_context* ctx);
extern "C" PPU_THREAD_LOCAL ppu_context* g_active_ctx;
extern "C" PPU_THREAD_LOCAL void (*g_trampoline_fn)(void*);
extern "C" volatile unsigned char* ps3_patch_flag(const char* name);

static bool frustum_log_enabled();
static void cv_frustum_record_32f0(uint32_t count, bool all_offscreen);

namespace {

#if defined(CV_REGION_US)
constexpr uint32_t kSysPtrAddr   = 0x00A79BD0u;
constexpr uint32_t kScratchMgr   = 0x00A73C68u;
constexpr uint32_t kMatPtr       = 0x00AAEA44u;
constexpr uint32_t kAspectX      = 0x00AAEA74u;
constexpr uint32_t kAspectY      = 0x00AAEA78u;
constexpr uint32_t kLastModel    = 0x00AAEAACu;
constexpr uint32_t kCtxObj       = 0x00AAEAB0u;
constexpr uint32_t kFileStatAddr = 0x00AC3B6Cu;
constexpr uint32_t kProjD        = 0x00ACC810u;
constexpr uint32_t kCullOn       = 0x00ADCB70u;
constexpr uint32_t kNoCull       = 0x00ADCB78u;
constexpr uint32_t kTrackBbox    = 0x00ADCBA0u;
constexpr uint32_t kBboxCnt      = 0x00ADCBA4u;
constexpr uint32_t kBboxMin      = 0x00ADCBB0u;
constexpr uint32_t kBboxMax      = 0x00ADCBC0u;
constexpr uint32_t kGlobalObj    = 0x00AEBADCu;
constexpr uint32_t kAllocObj     = 0x00B30CCCu;
constexpr uint32_t kDefCol       = 0x00BA10ECu;
constexpr uint32_t kDefUv        = 0x00BA93ECu;
constexpr uint32_t kDefNrm       = 0x00BA9550u;
constexpr uint32_t kZNear        = 0x00CA7050u;
constexpr uint32_t kZFar         = 0x00CA7054u;
constexpr uint32_t kClipX        = 0x00CA7058u;
constexpr uint32_t kClipY        = 0x00CA705Cu;
constexpr uint32_t kVtxPackTab   = 0x004B8AB8u;
constexpr uint32_t kNrmScale     = 0x0001D5CCu;
constexpr uint32_t kEnqMgrPtr    = 0x00A73CD0u;
constexpr uint32_t kEnqG0        = 0x00AAE9C0u;
constexpr uint32_t kEnqG2        = 0x00ACC8B0u;
constexpr uint32_t kSpriteColor  = 0x00ADE824u;
constexpr uint32_t kObjVtable    = 0x008EDD70u;
constexpr uint32_t kObjMeta      = 0x00905528u;
constexpr uint32_t kSubVtable    = 0x008EDD90u;
constexpr uint32_t kSpriteConstA = 0x001D2E40u;
constexpr uint32_t kStripConstA  = 0x001C69E0u;
constexpr uint32_t kSpriteVecB   = 0x00BA9500u;
constexpr uint32_t kSpriteVecC   = 0x00BA9490u;
constexpr uint32_t kFlag3470     = 0x00ADCB90u;
constexpr uint32_t kMatStatePtr  = 0x00AAEA68u;
constexpr uint32_t kByte3654     = 0x00ADC9ACu;
constexpr uint32_t kSpriteUvTpl  = 0x00AAEC40u;
constexpr uint32_t kAllocDesc    = 0x00BAA018u;
constexpr uint32_t kTexIdAddr    = 0x00ADC8E8u;
constexpr uint32_t kDrawCmdBaseVtbl = 0x009032B8u;
constexpr uint32_t kDrawElemDtorAddr = 0x007C0F68u;
constexpr uint32_t kHeapFreeAddr = 0x006D94FCu;
constexpr uint32_t kDepthHi      = 0x00AEB0E8u;
constexpr uint32_t kDepthLo      = 0x008B2064u;
constexpr uint32_t kTexMgrPtr    = 0x00A73C24u;
constexpr uint32_t kStateFlagAddr = 0x00AC2E24u;
constexpr uint32_t kPackConsts   = 0x007BE6F0u;
constexpr uint32_t k16E10MgrPtr  = 0x00AEBD1Cu;
constexpr uint32_t k16E10TypeDesc = 0x00AE806Cu;
constexpr uint32_t k16E10CtxPtr  = 0x00AB782Cu;
constexpr uint32_t k16E10Mat2    = 0x00BAE540u;
constexpr uint32_t k16E10BboxMin = 0x00BA9570u;
constexpr uint32_t kSyncParamSrc = 0x00AC2940u;
constexpr uint32_t kSyncParamDst = 0x00A73BB0u;
constexpr uint32_t kHashFindAddr = 0x002680BCu;
constexpr uint32_t kHashGetAddr  = 0x00267DC4u;
#elif defined(CV_REGION_JP)
constexpr uint32_t kSysPtrAddr   = 0x00A79C34u;
constexpr uint32_t kScratchMgr   = 0x00A73C68u;
constexpr uint32_t kMatPtr       = 0x00AAEB00u;
constexpr uint32_t kAspectX      = 0x00AAEB30u;
constexpr uint32_t kAspectY      = 0x00AAEB34u;
constexpr uint32_t kLastModel    = 0x00AAEB68u;
constexpr uint32_t kCtxObj       = 0x00AAEB6Cu;
constexpr uint32_t kFileStatAddr = 0x00AC3BECu;
constexpr uint32_t kProjD        = 0x00ACC890u;
constexpr uint32_t kCullOn       = 0x00ADCBF0u;
constexpr uint32_t kNoCull       = 0x00ADCBF8u;
constexpr uint32_t kTrackBbox    = 0x00ADCC20u;
constexpr uint32_t kBboxCnt      = 0x00ADCC24u;
constexpr uint32_t kBboxMin      = 0x00ADCC30u;
constexpr uint32_t kBboxMax      = 0x00ADCC40u;
constexpr uint32_t kGlobalObj    = 0x00AEBB5Cu;
constexpr uint32_t kAllocObj     = 0x00B30D4Cu;
constexpr uint32_t kDefCol       = 0x00BA116Cu;
constexpr uint32_t kDefUv        = 0x00BA946Cu;
constexpr uint32_t kDefNrm       = 0x00BA95D0u;
constexpr uint32_t kZNear        = 0x00CA70D0u;
constexpr uint32_t kZFar         = 0x00CA70D4u;
constexpr uint32_t kClipX        = 0x00CA70D8u;
constexpr uint32_t kClipY        = 0x00CA70DCu;
constexpr uint32_t kVtxPackTab   = 0x004B93A8u;
constexpr uint32_t kNrmScale     = 0x0001D5CCu;
constexpr uint32_t kEnqMgrPtr    = 0x00A73CD0u;
constexpr uint32_t kEnqG0        = 0x00AAEA7Cu;
constexpr uint32_t kEnqG2        = 0x00ACC930u;
constexpr uint32_t kSpriteColor  = 0x00ADE8A4u;
constexpr uint32_t kObjVtable    = 0x008EE620u;
constexpr uint32_t kObjMeta      = 0x00905DD8u;
constexpr uint32_t kSubVtable    = 0x008EE640u;
constexpr uint32_t kSpriteConstA = 0x001D2FC0u;
constexpr uint32_t kStripConstA  = 0x001C6B60u;
constexpr uint32_t kSpriteVecB   = 0x00BA9580u;
constexpr uint32_t kSpriteVecC   = 0x00BA9510u;
constexpr uint32_t kFlag3470     = 0x00ADCC10u;
constexpr uint32_t kMatStatePtr  = 0x00AAEB24u;
constexpr uint32_t kByte3654     = 0x00ADCA2Cu;
constexpr uint32_t kSpriteUvTpl  = 0x00AAECFCu;
constexpr uint32_t kAllocDesc    = 0x00BAA098u;
constexpr uint32_t kTexIdAddr    = 0x00ADC968u;
constexpr uint32_t kDrawCmdBaseVtbl = 0x00903B68u;
constexpr uint32_t kDrawElemDtorAddr = 0x007C1858u;
constexpr uint32_t kHeapFreeAddr = 0x006D9DECu;
constexpr uint32_t kDepthHi      = 0x00AEB168u;
constexpr uint32_t kDepthLo      = 0x008B290Cu;
constexpr uint32_t kTexMgrPtr    = 0x00A73C24u;
constexpr uint32_t kStateFlagAddr = 0x00AC2EA4u;
constexpr uint32_t kPackConsts   = 0x007BEFE0u;
constexpr uint32_t k16E10MgrPtr  = 0x00AEBD9Cu;
constexpr uint32_t k16E10TypeDesc = 0x00AE80ECu;
constexpr uint32_t k16E10CtxPtr  = 0x00AB78ECu;
constexpr uint32_t k16E10Mat2    = 0x00BAE5C0u;
constexpr uint32_t k16E10BboxMin = 0x00BA95F0u;
constexpr uint32_t kSyncParamSrc = 0x00AC2A00u;
constexpr uint32_t kSyncParamDst = 0x00A73BB0u;
constexpr uint32_t kHashFindAddr = 0x002689ACu;
constexpr uint32_t kHashGetAddr  = 0x002686B4u;
#else /* EU */
constexpr uint32_t kSysPtrAddr   = 0x00A89BD0u;
constexpr uint32_t kScratchMgr   = 0x00A83C68u;
constexpr uint32_t kMatPtr       = 0x00ABED90u;
constexpr uint32_t kAspectX      = 0x00ABEDC0u;
constexpr uint32_t kAspectY      = 0x00ABEDC4u;
constexpr uint32_t kLastModel    = 0x00ABEDF8u;
constexpr uint32_t kCtxObj       = 0x00ABEDFCu;
constexpr uint32_t kFileStatAddr = 0x00AD3E6Cu;
constexpr uint32_t kProjD        = 0x00ADCB10u;
constexpr uint32_t kCullOn       = 0x00AECE70u;
constexpr uint32_t kNoCull       = 0x00AECE78u;
constexpr uint32_t kTrackBbox    = 0x00AECEA0u;
constexpr uint32_t kBboxCnt      = 0x00AECEA4u;
constexpr uint32_t kBboxMin      = 0x00AECEB0u;
constexpr uint32_t kBboxMax      = 0x00AECEC0u;
constexpr uint32_t kGlobalObj    = 0x00AFBDDCu;
constexpr uint32_t kAllocObj     = 0x00B40FCCu;
constexpr uint32_t kDefCol       = 0x00BB13ECu;
constexpr uint32_t kDefUv        = 0x00BB96ECu;
constexpr uint32_t kDefNrm       = 0x00BB9850u;
constexpr uint32_t kZNear        = 0x00CB7350u;
constexpr uint32_t kZFar         = 0x00CB7354u;
constexpr uint32_t kClipX        = 0x00CB7358u;
constexpr uint32_t kClipY        = 0x00CB735Cu;
constexpr uint32_t kVtxPackTab   = 0x004BAA78u;
constexpr uint32_t kNrmScale     = 0x0001D5CCu;
constexpr uint32_t kEnqMgrPtr    = 0x00A83CD0u;
constexpr uint32_t kEnqG0        = 0x00ABED0Cu;
constexpr uint32_t kEnqG2        = 0x00ADCBB0u;
constexpr uint32_t kSpriteColor  = 0x00AEEB24u;
constexpr uint32_t kObjVtable    = 0x008F0720u;
constexpr uint32_t kObjMeta      = 0x00907ED8u;
constexpr uint32_t kSubVtable    = 0x008F0740u;
constexpr uint32_t kSpriteConstA = 0x001D4C60u;
constexpr uint32_t kStripConstA  = 0x001C8800u;
constexpr uint32_t kSpriteVecB   = 0x00BB9800u;
constexpr uint32_t kSpriteVecC   = 0x00BB9790u;
constexpr uint32_t kFlag3470     = 0x00AECE90u;
constexpr uint32_t kMatStatePtr  = 0x00ABEDB4u;
constexpr uint32_t kByte3654     = 0x00AECCACu;
constexpr uint32_t kSpriteUvTpl  = 0x00ABEF8Cu;
constexpr uint32_t kAllocDesc    = 0x00BBA318u;
constexpr uint32_t kTexIdAddr    = 0x00AECBE8u;
constexpr uint32_t kDrawCmdBaseVtbl = 0x00905C68u;
constexpr uint32_t kDrawElemDtorAddr = 0x007C2F28u;
constexpr uint32_t kHeapFreeAddr = 0x006DB4BCu;
constexpr uint32_t kDepthHi      = 0x00AFB3E8u;
constexpr uint32_t kDepthLo      = 0x008B4A14u;
constexpr uint32_t kTexMgrPtr    = 0x00A83C24u;
constexpr uint32_t kStateFlagAddr = 0x00AD3124u;
constexpr uint32_t kPackConsts   = 0x007C06B0u;
constexpr uint32_t k16E10MgrPtr  = 0x00AFC01Cu;
constexpr uint32_t k16E10TypeDesc = 0x00AF836Cu;
constexpr uint32_t k16E10CtxPtr  = 0x00AC7B7Cu;
constexpr uint32_t k16E10Mat2    = 0x00BBE840u;
constexpr uint32_t k16E10BboxMin = 0x00BB9870u;
constexpr uint32_t kSyncParamSrc = 0x00AD2C90u;
constexpr uint32_t kSyncParamDst = 0x00A83BB0u;
constexpr uint32_t kHashFindAddr = 0x0026A08Cu;
constexpr uint32_t kHashGetAddr  = 0x00269D94u;
#endif
constexpr uint32_t kOffSsFlg     = 0x00050u;    /* bit 0x400000 = attract demo */
constexpr uint32_t kOffTsFlg     = 0x00068u;    /* bit 0x800 = door ready */
constexpr uint32_t kOffCbFlg     = 0x00074u;    /* bit 0x2 = screen fade active */
constexpr uint32_t kOffMnMd0     = 0x1B108u;    /* 4 = room load */
constexpr uint32_t kOffMnMd1     = 0x1B109u;    /* sub-step inside room load */
constexpr uint32_t kOffSdmFlg    = 0x1B130u;    /* bit 0x1 = sound monitor active */
constexpr uint32_t kOffSdmMode   = 0x1B134u;    /* sound monitor state machine */
constexpr uint32_t kOffFadeCt    = 0x2A5CCu;    /* float fade_ct */
constexpr uint32_t kOffFadeHkn   = 0x2A5D0u;    /* float fade_hkn (1.0f = finish in 1 frame) */
constexpr uint32_t kOffFadeAn    = 0x2A5D8u;    /* float fade_an */
constexpr uint32_t kOffFadeAp    = 0x2A5DCu;    /* float fade_ap */
constexpr uint32_t kOffFadeWait  = 0x2A608u;    /* uint32 fade delay frames */
constexpr uint32_t kOffDoorMode0 = 0x2A6ACu;    /* sys->door.mode0 (door + 0x8) */

/* Safe nested guest call (same pattern as cv_title_return.cpp). */
struct GuestCall {
    ppu_context c;
    ppu_context* previous;
    void (*tail)(void*);

    explicit GuestCall(ppu_context* p)
        : c(*p), previous(g_active_ctx), tail(g_trampoline_fn)
    {
        c.gpr[1] -= 0x2000;
        vm_write64(c.gpr[1], p->gpr[1]);
        c.gpr[2] = CV_TOC;
        g_active_ctx = &c;
        g_trampoline_fn = nullptr;
    }

    ~GuestCall()
    {
        g_active_ctx = previous;
        g_trampoline_fn = tail;
    }

    void run(void (*fn)(ppu_context*), uint32_t a, uint32_t b, uint32_t d = 0)
    {
        c.gpr[3] = a;
        c.gpr[4] = b;
        c.gpr[5] = d;
        fn(&c);
        while (g_trampoline_fn) {
            auto next = g_trampoline_fn;
            g_trampoline_fn = nullptr;
            next(&c);
        }
    }

    uint32_t run_u32(void (*fn)(ppu_context*), uint32_t a, uint32_t b)
    {
        run(fn, a, b, 0);
        return (uint32_t)c.gpr[3];
    }

    uint32_t run_indirect_u32(uint32_t fn_addr, uint32_t a, uint32_t b, uint32_t d = 0)
    {
        c.gpr[3] = a;
        c.gpr[4] = b;
        c.gpr[5] = d;
        c.ctr = fn_addr;
        ps3_indirect_call(&c);
        while (g_trampoline_fn) {
            auto next = g_trampoline_fn;
            g_trampoline_fn = nullptr;
            next(&c);
        }
        return (uint32_t)c.gpr[3];
    }
};

void stop_sound_bank(ppu_context* ctx, uint32_t bank)
{
    const uint32_t sound = vm_read32(CV_SOUND_MANAGER);
    if (!sound) return;
    GuestCall call(ctx);
    call.run(CV_FN_STOP_BANK, sound, bank);
}

bool env_flag(const char* name, bool default_val)
{
    const char* v = std::getenv(name);
    if (!v || !*v) return default_val;
    return v[0] != '0';
}

int env_tri(const char* name)
{
    const char* v = std::getenv(name);
    if (!v || !*v) return -1;
    return v[0] != '0' ? 1 : 0;
}

bool fast_door_enabled()
{
    static int cached_env = -2;
    if (cached_env == -2) cached_env = env_tri("CV_FAST_DOOR");
    if (cached_env == 0) return false;

    static volatile unsigned char* door_flag = nullptr;
    static volatile unsigned char* fast_flag = nullptr;
    if (!door_flag) door_flag = ps3_patch_flag("cv_door_skip");
    if (!fast_flag) fast_flag = ps3_patch_flag("cv_door_fast");
    if (!door_flag || *door_flag == 0) return false;
    if (cached_env == 1) return true;
    return fast_flag && (*fast_flag != 0);
}

bool fast_fade_enabled()
{
    static int cached_env = -2;
    if (cached_env == -2) cached_env = env_tri("CV_FAST_DOOR_FADE");
    if (cached_env >= 0) return cached_env != 0;

    static volatile unsigned char* sup_flag = nullptr;
    if (!sup_flag) sup_flag = ps3_patch_flag("cv_door_supreme");
    return sup_flag && (*sup_flag != 0);
}

bool fast_sync_read_enabled()
{
    static int cached = -1;
    if (cached < 0) cached = env_flag("CV_FAST_DOOR_SYNC", true) ? 1 : 0;
    return cached != 0;
}

bool native_zlib_enabled()
{
    static int cached = -1;
    if (cached < 0) cached = env_flag("CV_NATIVE_ZLIB", true) ? 1 : 0;
    return cached != 0;
}

bool native_hot_enabled()
{
    static int cached = -1;
    if (cached < 0) cached = env_flag("CV_NATIVE_HOT", true) ? 1 : 0;
    return cached != 0;
}

static inline uint32_t bswap32_fast(uint32_t v)
{
#ifdef _MSC_VER
    return _byteswap_ulong(v);
#else
    return __builtin_bswap32(v);
#endif
}

static inline uint16_t read_be16_fast(const uint8_t* p)
{
    uint16_t v;
    std::memcpy(&v, p, 2);
#ifdef _MSC_VER
    return _byteswap_ushort(v);
#else
    return __builtin_bswap16(v);
#endif
}

static inline uint32_t read_be32_fast(const uint8_t* p)
{
    uint32_t v;
    std::memcpy(&v, p, 4);
    return bswap32_fast(v);
}

static inline float read_bef32_fast(const uint8_t* p)
{
    uint32_t u = read_be32_fast(p);
    float f;
    std::memcpy(&f, &u, 4);
    return f;
}

static inline void write_be32_fast(uint8_t* p, uint32_t v)
{
#ifdef _MSC_VER
    v = _byteswap_ulong(v);
#else
    v = __builtin_bswap32(v);
#endif
    std::memcpy(p, &v, 4);
}

static inline void write_be16_fast(uint8_t* p, uint16_t v)
{
#ifdef _MSC_VER
    v = _byteswap_ushort(v);
#else
    v = __builtin_bswap16(v);
#endif
    std::memcpy(p, &v, 2);
}

static inline void write_bef32_fast(uint8_t* p, float f)
{
    uint32_t u;
    std::memcpy(&u, &f, 4);
#ifdef _MSC_VER
    u = _byteswap_ulong(u);
#else
    u = __builtin_bswap32(u);
#endif
    std::memcpy(p, &u, 4);
}

bool fast_log_enabled()
{
#ifdef CV_DISTRIBUTION
    return false;
#else
    static int cached = -1;
    if (cached < 0) cached = env_flag("CV_FAST_DOOR_LOG", false) ? 1 : 0;
    return cached != 0;
#endif
}

uint32_t get_sys()
{
    return vm_read32(kSysPtrAddr);
}

bool is_attract_demo(uint32_t sys)
{
    return (vm_read32(sys + kOffSsFlg) & 0x00400000u) != 0;
}

bool is_disc_swap(uint32_t sys)
{
    if (!sys) return false;
    const uint32_t tasks = vm_read32(sys + 0x64u);
    if (tasks == 0x00240000u) return true;
    const uint32_t ss_flg = vm_read32(sys + kOffSsFlg);
    if ((ss_flg & 0x00400000u) == 0u) {
        const int8_t stage = (int8_t)vm_read8(sys + 0x95u);
        const bool   disc2 = (ss_flg & 1u) != 0u;
        if ((!disc2 && stage > 5) || (disc2 && stage < 6)) return true;
    }
    return false;
}

void write_float(uint32_t addr, float val)
{
    uint32_t bits = 0;
    std::memcpy(&bits, &val, sizeof(bits));
    vm_write32(addr, bits);
}

void snap_active_fade(uint32_t sys)
{
    const uint32_t cb = vm_read32(sys + kOffCbFlg);
    if (!(cb & 0x2u)) return;
    vm_write32(sys + kOffFadeWait, 0u);
    write_float(sys + kOffFadeHkn, 1.0f);
    write_float(sys + kOffFadeCt, 1.0f);
    vm_write32(sys + kOffFadeAn, vm_read32(sys + kOffFadeAp));
    vm_write32(sys + kOffCbFlg, cb & ~0x2u);
}

void log_line(const char* line)
{
#ifdef CV_DISTRIBUTION
    (void)line;
#else
    if (!fast_log_enabled()) return;
    std::fprintf(stderr, "%s\n", line);
    std::fflush(stderr);
#endif
}

std::atomic<int> g_door_read_calls{0};
std::atomic<long long> g_door_read_us{0};
std::atomic<int> g_door_inflate_calls{0};
std::atomic<long long> g_door_inflate_us{0};

/* Native x64 zlib stream pool keyed by guest stream object address. */
struct NativeZlibSlot {
    uint32_t addr = 0;
    bool active = false;
    z_stream zs{};
};

constexpr int kMaxZlibSlots = 64;
std::mutex g_zlib_mu;
NativeZlibSlot g_zlib_slots[kMaxZlibSlots];

NativeZlibSlot* zlib_slot_create(uint32_t addr)
{
    std::lock_guard<std::mutex> lk(g_zlib_mu);
    NativeZlibSlot* free_slot = nullptr;
    for (auto& s : g_zlib_slots) {
        if (s.active && s.addr == addr) {
            inflateEnd(&s.zs);
            std::memset(&s.zs, 0, sizeof(s.zs));
            if (inflateInit(&s.zs) == Z_OK) return &s;
            s.active = false;
            return nullptr;
        }
        if (!s.active && !free_slot) free_slot = &s;
    }
    if (!free_slot) return nullptr;
    std::memset(&free_slot->zs, 0, sizeof(free_slot->zs));
    if (inflateInit(&free_slot->zs) != Z_OK) return nullptr;
    free_slot->addr = addr;
    free_slot->active = true;
    return free_slot;
}

NativeZlibSlot* zlib_slot_find(uint32_t addr)
{
    std::lock_guard<std::mutex> lk(g_zlib_mu);
    for (auto& s : g_zlib_slots) {
        if (s.active && s.addr == addr) return &s;
    }
    return nullptr;
}

void zlib_slot_destroy(uint32_t addr)
{
    std::lock_guard<std::mutex> lk(g_zlib_mu);
    for (auto& s : g_zlib_slots) {
        if (s.active && s.addr == addr) {
            inflateEnd(&s.zs);
            std::memset(&s.zs, 0, sizeof(s.zs));
            s.addr = 0;
            s.active = false;
            return;
        }
    }
}

struct DoorProfiler {
    using clock = std::chrono::steady_clock;
    bool active = false;
    bool fade_in_pending = false;
    bool stopped_bank9 = false;
    bool stopped_bank10 = false;
    int door_frames = 0;
    int monitor_frames = 0;
    int step_frames[24]{};
    int wait_file_frames = 0;
    int wait_snd_frames = 0;
    int wait_door_flag_frames = 0;
    int wait_fade_frames = 0;
    clock::time_point start_time{};
    clock::time_point last_change{};
    uint8_t cur_md1 = 255;
    long long step_ms[24]{};

    void begin()
    {
        active = true;
        stopped_bank9 = false;
        stopped_bank10 = false;
        door_frames = 0;
        monitor_frames = 0;
        wait_file_frames = 0;
        wait_snd_frames = 0;
        wait_door_flag_frames = 0;
        wait_fade_frames = 0;
        g_door_read_calls.store(0, std::memory_order_relaxed);
        g_door_read_us.store(0, std::memory_order_relaxed);
        g_door_inflate_calls.store(0, std::memory_order_relaxed);
        g_door_inflate_us.store(0, std::memory_order_relaxed);
        std::memset(step_frames, 0, sizeof(step_frames));
        std::memset(step_ms, 0, sizeof(step_ms));
        cur_md1 = 255;
        start_time = clock::now();
        last_change = start_time;
    }

    void record_md1(uint8_t md1, uint32_t sys)
    {
        if (!active) return;
        const auto now = clock::now();
        if (cur_md1 < 24) {
            step_ms[cur_md1] += std::chrono::duration_cast<std::chrono::milliseconds>(now - last_change).count();
        }
        last_change = now;
        cur_md1 = md1;
        if (md1 < 24) step_frames[md1]++;
        if (vm_read32(kFileStatAddr) != 0) wait_file_frames++;
        if (vm_read32(sys + kOffSdmFlg) & 1u) wait_snd_frames++;
        if (!(vm_read32(sys + kOffTsFlg) & 0x800u)) wait_door_flag_frames++;
        if (vm_read32(sys + kOffCbFlg) & 2u) wait_fade_frames++;
    }

    void finish(uint16_t room)
    {
        if (!active) return;
        const auto now = clock::now();
        if (cur_md1 < 24) {
            step_ms[cur_md1] += std::chrono::duration_cast<std::chrono::milliseconds>(now - last_change).count();
        }
        const auto total_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count();
        const int rcalls = g_door_read_calls.load(std::memory_order_relaxed);
        const long long rms = g_door_read_us.load(std::memory_order_relaxed) / 1000;
        const int icalls = g_door_inflate_calls.load(std::memory_order_relaxed);
        const long long ims = g_door_inflate_us.load(std::memory_order_relaxed) / 1000;
        char buf[1024];
        int n = std::snprintf(buf, sizeof(buf),
            "[cv_fast_door] room=0x%04X total=%lldms door_f=%d mon_f=%d (file=%df snd=%df reads=%d/%lldms inf=%d/%lldms) fast_fade=%d | steps:",
            (unsigned)room, (long long)total_ms, door_frames, monitor_frames,
            wait_file_frames, wait_snd_frames, rcalls, rms, icalls, ims,
            fast_fade_enabled() ? 1 : 0);
        for (int i = 0; i < 24 && n < (int)sizeof(buf) - 32; ++i) {
            if (step_frames[i] > 0 || step_ms[i] > 0) {
                n += std::snprintf(buf + n, sizeof(buf) - n, " s%d=%df/%lldms",
                    i, step_frames[i], step_ms[i]);
            }
        }
        log_line(buf);
        active = false;
    }
};

DoorProfiler g_prof;

} // namespace

/* Hook 1: bhSysCallDoordemo (EU 0x001AFAE8) */
void CV_FD_FN_DOOR_DEMO(ppu_context* ctx)
{
    const uint32_t sys = get_sys();
    if (sys && is_disc_swap(sys)) {
        g_prof.active = false;
        g_prof.fade_in_pending = false;
        CV_FD_ORIG_DOOR_DEMO(ctx);
        return;
    }
    const bool enabled = fast_door_enabled() && sys && !is_attract_demo(sys);

    if (!enabled) {
        CV_FD_ORIG_DOOR_DEMO(ctx);
        return;
    }

    uint8_t mode0 = vm_read8(sys + kOffDoorMode0);

    if (mode0 == 0) {
        g_prof.begin();
        vm_write8(sys + kOffDoorMode0, 10u);
        mode0 = 10u;
    }

    if (g_prof.active) {
        g_prof.door_frames++;
    }

    if (mode0 == 10u && fast_fade_enabled()) {
        snap_active_fade(sys);
    }

    CV_FD_ORIG_DOOR_DEMO(ctx);

    if (mode0 == 10u && vm_read8(sys + kOffDoorMode0) == 11u) {
        CV_FD_ORIG_DOOR_DEMO(ctx);
    }
}

/* Hook 2: bhSysCallMonitor (EU 0x001B0CB0) */
void CV_FD_FN_MONITOR(ppu_context* ctx)
{
    const uint32_t sys = get_sys();
    if (g_prof.active) {
        const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            DoorProfiler::clock::now() - g_prof.start_time).count();
        if (elapsed_ms > 5000 || !sys || is_disc_swap(sys)) {
            g_prof.active = false;
            g_prof.fade_in_pending = false;
        }
    }
    /* Only accelerate room loads that were actually started by a door
     * transition (g_prof.active). Save loads, title/new-game transitions,
     * and Disc 1 <-> Disc 2 swaps must run unmodified 1 step per frame. */
    const bool enabled = fast_door_enabled() && g_prof.active && sys && !is_attract_demo(sys) && !is_disc_swap(sys);

    if (!enabled) {
        CV_FD_ORIG_MONITOR(ctx);
        return;
    }

    const uint8_t start_md0 = vm_read8(sys + kOffMnMd0);
    if (start_md0 != 4u) {
        g_prof.active = false;
        g_prof.fade_in_pending = false;
        CV_FD_ORIG_MONITOR(ctx);
        return;
    }
    g_prof.monitor_frames++;
    g_prof.record_md1(vm_read8(sys + kOffMnMd1), sys);

    static int micropoll_env = -1;
    if (micropoll_env < 0) micropoll_env = env_flag("CV_FAST_DOOR_MICROPOLL", true) ? 1 : 0;
    int wait_budget_ms = (micropoll_env && g_prof.active) ? 120 : 0;

    for (int pass = 0; pass < 64; ++pass) {
        const uint8_t before_md0 = vm_read8(sys + kOffMnMd0);
        const uint8_t before_md1 = vm_read8(sys + kOffMnMd1);

        if (before_md0 == 4u && before_md1 == 0u && fast_fade_enabled()) {
            snap_active_fade(sys);
        }

        if (before_md0 == 4u && before_md1 == 10u && g_prof.active) {
            if (!g_prof.stopped_bank9) {
                stop_sound_bank(ctx, 9u);
                g_prof.stopped_bank9 = true;
            } else if (!g_prof.stopped_bank10) {
                stop_sound_bank(ctx, 10u);
                g_prof.stopped_bank10 = true;
            }
        }

        CV_FD_ORIG_MONITOR(ctx);

        const uint8_t after_md0 = vm_read8(sys + kOffMnMd0);
        const uint8_t after_md1 = vm_read8(sys + kOffMnMd1);

        if (before_md0 == 4u && after_md0 == 0u) {
            if (fast_fade_enabled()) {
                g_prof.fade_in_pending = true;
            }
            g_prof.finish(vm_read16(CV_SRT_ROOM));
            break;
        }

        if (after_md0 != 4u) {
            break;
        }

        /* Steps 12..15 (0x0C..0x0F) perform the Disc 1 <-> Disc 2 swap and
         * require real per-frame ticks between steps; never collapse them. */
        if ((before_md1 >= 12u && before_md1 <= 15u) ||
            (after_md1  >= 12u && after_md1  <= 15u)) {
            break;
        }

        if (after_md1 == 11u && before_md1 != 11u && (vm_read32(sys + kOffSdmFlg) & 0x1u)) {
            CV_FD_FN_SND_MON(ctx);
        }

        if (after_md0 == before_md0 && after_md1 == before_md1) {
            /* Screen is already black waiting on background Resource Loader /
             * decodeProc threads (kFileStatAddr). Micro-poll in 1 ms steps so
             * file loading completes in ~5-30 ms in a single frame instead of
             * sleeping 33.3 ms per step. Never micro-poll s11 (snddrv), which
             * requires main-thread audio ticks to advance. */
            if (wait_budget_ms > 0 && vm_read32(kFileStatAddr) != 0) {
                while (wait_budget_ms > 0 && vm_read32(kFileStatAddr) != 0) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    --wait_budget_ms;
                }
                if (vm_read32(kFileStatAddr) == 0) continue;
            }
            break;
        }
    }
}

/* Hook 3: bhSysCallSndMonitor (EU 0x001B2A48) */
void CV_FD_FN_SND_MON(ppu_context* ctx)
{
    const uint32_t sys = get_sys();
    if (sys && is_disc_swap(sys)) {
        g_prof.active = false;
        g_prof.fade_in_pending = false;
        CV_FD_ORIG_SND_MON(ctx);
        return;
    }
    const bool door_on = fast_door_enabled() && sys && !is_attract_demo(sys);

    if (door_on && g_prof.fade_in_pending && fast_fade_enabled()) {
        if (vm_read32(sys + kOffCbFlg) & 0x2u) {
            snap_active_fade(sys);
            g_prof.fade_in_pending = false;
        }
    }

    /* Only collapse sound monitor passes during an active door transition,
     * never during save loads or Disc 1 <-> Disc 2 bank swaps. */
    if (!door_on || !g_prof.active) {
        CV_FD_ORIG_SND_MON(ctx);
        return;
    }

    const uint8_t cur_md0 = vm_read8(sys + kOffMnMd0);
    const uint8_t cur_md1 = vm_read8(sys + kOffMnMd1);
    if (cur_md0 == 4u && cur_md1 >= 12u && cur_md1 <= 15u) {
        CV_FD_ORIG_SND_MON(ctx);
        return;
    }

    for (int pass = 0; pass < 16; ++pass) {
        const uint32_t before_flg  = vm_read32(sys + kOffSdmFlg);
        const uint32_t before_mode = vm_read32(sys + kOffSdmMode);

        CV_FD_ORIG_SND_MON(ctx);

        const uint32_t after_flg  = vm_read32(sys + kOffSdmFlg);
        const uint32_t after_mode = vm_read32(sys + kOffSdmMode);

        if (!(after_flg & 0x1u)) {
            break;
        }
        if (after_flg == before_flg && after_mode == before_mode) {
            break;
        }
    }
}

/* Hook 4a: MtFile::read (EU 0x007846D0) */
void CV_FD_FN_MT_READ(ppu_context* ctx)
{
    const uint32_t obj = (uint32_t)ctx->gpr[3];
    const uint32_t sys = get_sys();
    if (!fast_sync_read_enabled() || !obj || (sys && is_disc_swap(sys))) {
        CV_FD_ORIG_MT_READ(ctx);
        return;
    }

    const bool do_log = fast_log_enabled();
    const auto t0 = do_log ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    const uint32_t flags = vm_read32(obj + 0x8);
    if ((flags & 4u) && vm_read8(obj + 0x42C) == 0) {
        const uint32_t pos = vm_read32(obj + 0x10);
        vm_write32(obj + 0x8, flags & ~4u);
        {
            GuestCall seek_call(ctx);
            seek_call.run(CV_FD_FN_MT_SEEK, obj, pos, 1u);
        }
        CV_FD_ORIG_MT_READ(ctx);
        vm_write32(obj + 0x8, flags);
        vm_write32(obj + 0x424, (uint32_t)ctx->gpr[3]);
        vm_write8(obj + 0x42C, 0u);
    } else {
        CV_FD_ORIG_MT_READ(ctx);
    }

    if (do_log) {
        const auto dt = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - t0).count();
        g_door_read_calls.fetch_add(1, std::memory_order_relaxed);
        g_door_read_us.fetch_add(dt, std::memory_order_relaxed);
    }
}

/* Hook 4b: MtFile::readAsync (EU 0x0078575C) */
void CV_FD_FN_MT_READ_ASYNC(ppu_context* ctx)
{
    CV_FD_ORIG_MT_READ_ASYNC(ctx);
}

/* Hook 5a: decodeProc zlib stream ctor (EU 0x00538AEC) */
void CV_FD_FN_ZLIB_CTOR(ppu_context* ctx)
{
    const uint32_t stream_addr = (uint32_t)ctx->gpr[3];
    CV_FD_ORIG_ZLIB_CTOR(ctx);
    if (native_zlib_enabled() && stream_addr) {
        zlib_slot_create(stream_addr);
    }
}

/* Hook 5b: decodeProc zlib stream dtor (EU 0x00539B0C) */
void CV_FD_FN_ZLIB_DTOR(ppu_context* ctx)
{
    const uint32_t stream_addr = (uint32_t)ctx->gpr[3];
    if (stream_addr) {
        zlib_slot_destroy(stream_addr);
    }
    CV_FD_ORIG_ZLIB_DTOR(ctx);
}

/* Hook 5c: native x64 zlib inflate replacing SPU job at loc_00539EB0 */
extern "C" int cv_native_spu_inflate(ppu_context* ctx)
{
    if (!native_zlib_enabled() || !vm_base) return 0;

    const uint32_t stream_addr = (uint32_t)ctx->gpr[31];
    const uint32_t z_addr      = (uint32_t)ctx->gpr[30];
    NativeZlibSlot* slot = zlib_slot_find(stream_addr);
    if (!slot) return 0;

    const bool do_log = fast_log_enabled();
    const auto t0 = do_log ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};

    const uint32_t next_in   = vm_read32(z_addr + 0x00);
    const uint32_t avail_in  = vm_read32(z_addr + 0x04);
    const uint32_t total_in  = vm_read32(z_addr + 0x08);
    const uint32_t next_out  = vm_read32(z_addr + 0x0C);
    const uint32_t avail_out = vm_read32(z_addr + 0x10);
    const uint32_t total_out = vm_read32(z_addr + 0x14);
    const uint32_t copy_mode = vm_read32(stream_addr + 0xC8);

    z_stream& zs = slot->zs;
    zs.next_in  = vm_base + next_in;
    zs.avail_in = avail_in;

    int rc = Z_OK;
    uint32_t out_used = 0;

    /* Note: this+0xC8 == 0 is NORMAL read (write to next_out); != 0 is SKIP mode. */
    if (copy_mode == 0) {
        zs.next_out  = vm_base + next_out;
        zs.avail_out = avail_out;
        rc = ::inflate(&zs, Z_NO_FLUSH);
        out_used = avail_out - zs.avail_out;
        vm_write32(z_addr + 0x0C, next_out + out_used);
        vm_write32(z_addr + 0x10, zs.avail_out);
    } else {
        /* Skip/discard mode: inflate into stack scratch without writing to next_out. */
        uint8_t scratch[16384];
        uint32_t rem_out = avail_out;
        while (rem_out > 0 && zs.avail_in > 0) {
            const uint32_t chunk = rem_out < sizeof(scratch) ? rem_out : (uint32_t)sizeof(scratch);
            zs.next_out  = scratch;
            zs.avail_out = chunk;
            rc = ::inflate(&zs, Z_NO_FLUSH);
            const uint32_t got = chunk - zs.avail_out;
            out_used += got;
            rem_out  -= got;
            if (rc != Z_OK || (got == 0 && zs.avail_in == 0)) break;
        }
        vm_write32(z_addr + 0x10, rem_out);
    }

    const uint32_t in_used = avail_in - zs.avail_in;
    vm_write32(z_addr + 0x00, next_in + in_used);
    vm_write32(z_addr + 0x04, zs.avail_in);
    vm_write32(z_addr + 0x08, total_in + in_used);
    vm_write32(z_addr + 0x14, total_out + out_used);
    vm_write32(stream_addr + 0xB8, (uint32_t)rc);

    if (do_log) {
        const auto dt = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - t0).count();
        g_door_inflate_calls.fetch_add(1, std::memory_order_relaxed);
        g_door_inflate_us.fetch_add(dt, std::memory_order_relaxed);
    }
    return 1;
}

/* Hook 6: njCalcPoint4 (EU 0x001CF5D4) -- hot leaf mat4x4 * vec3 -> vec4 */
void CV_FD_FN_CALC_POINT4(ppu_context* ctx)
{
    if (!native_hot_enabled() || !vm_base) {
        CV_FD_ORIG_CALC_POINT4(ctx);
        return;
    }

    uint32_t mat = (uint32_t)ctx->gpr[3];
    const uint32_t src = (uint32_t)ctx->gpr[4];
    const uint32_t dst = (uint32_t)ctx->gpr[5];
    if (!mat) {
        mat = read_be32_fast(vm_base + kMatPtr); /* (0xAC << 16) - 0x1270 */
    }

    const uint8_t* s = vm_base + src;
    const uint8_t* m = vm_base + mat;
    uint8_t* d = vm_base + dst;

    const float x = read_bef32_fast(s + 0x0);
    const float y = read_bef32_fast(s + 0x4);
    const float z = read_bef32_fast(s + 0x8);

    const float ox = read_bef32_fast(m + 0x00) * x + read_bef32_fast(m + 0x10) * y + read_bef32_fast(m + 0x20) * z + read_bef32_fast(m + 0x30);
    const float oy = read_bef32_fast(m + 0x04) * x + read_bef32_fast(m + 0x14) * y + read_bef32_fast(m + 0x24) * z + read_bef32_fast(m + 0x34);
    const float oz = read_bef32_fast(m + 0x08) * x + read_bef32_fast(m + 0x18) * y + read_bef32_fast(m + 0x28) * z + read_bef32_fast(m + 0x38);
    const float ow = read_bef32_fast(m + 0x0C) * x + read_bef32_fast(m + 0x1C) * y + read_bef32_fast(m + 0x2C) * z + read_bef32_fast(m + 0x3C);

    write_bef32_fast(d + 0x0, ox);
    write_bef32_fast(d + 0x4, oy);
    write_bef32_fast(d + 0x8, oz);
    write_bef32_fast(d + 0xC, ow);
}

/* Hook 7: K-way draw-call merge sort (EU 0x00287750) -- hot leaf on 1000+ draw rooms */
void CV_FD_FN_KWAY_SORT(ppu_context* ctx)
{
    if (!native_hot_enabled() || !vm_base) {
        CV_FD_ORIG_KWAY_SORT(ctx);
        return;
    }

    const uint32_t desc = (uint32_t)ctx->gpr[4];
    const uint32_t lists_addr = read_be32_fast(vm_base + desc + 0x0);
    const uint32_t count      = read_be32_fast(vm_base + desc + 0x4);
    const uint32_t out_addr   = read_be32_fast(vm_base + desc + 0x8);

    if (count == 0) return;
    if (count > 32) {
        CV_FD_ORIG_KWAY_SORT(ctx);
        return;
    }

    const uint8_t* ptrs[32];
    uint32_t lens[32];
    uint32_t idxs[32];
    uint32_t total = 0;

    for (uint32_t i = 0; i < count; ++i) {
        const uint8_t* entry = vm_base + lists_addr + i * 12u;
        lens[i] = read_be32_fast(entry + 0x4);
        ptrs[i] = vm_base + read_be32_fast(entry + 0x8);
        idxs[i] = 0;
        total += lens[i];
    }

    if (total == 0) return;

    uint8_t* out = vm_base + out_addr;

    if (count == 1) {
        std::memcpy(out, ptrs[0], (size_t)total * 8u);
        return;
    }

    for (uint32_t k = 0; k < total; ++k) {
        uint32_t best_key = 0xFFFFFFFFu;
        uint32_t best_i = 0;
        for (uint32_t i = 0; i < count; ++i) {
            if (idxs[i] < lens[i]) {
                const uint32_t key = read_be32_fast(ptrs[i] + (size_t)idxs[i] * 8u);
                if (best_key > key) {
                    best_key = key;
                    best_i = i;
                }
            }
        }
        std::memcpy(out + (size_t)k * 8u, ptrs[best_i] + (size_t)idxs[best_i] * 8u, 8u);
        idxs[best_i]++;
    }
}

/* Hook 8: njMultiMatrix affine 3x4 (EU 0x001CE930) -- 3.8% of guest CPU */
void CV_FD_FN_MULTI_MATRIX(ppu_context* ctx)
{
    if (!native_hot_enabled() || !vm_base) {
        CV_FD_ORIG_MULTI_MATRIX(ctx);
        return;
    }

    uint32_t dst_addr = (uint32_t)ctx->gpr[3];
    const uint32_t src_addr = (uint32_t)ctx->gpr[4];
    if (!dst_addr) {
        dst_addr = read_be32_fast(vm_base + kMatPtr);
    }

    uint8_t* a = vm_base + dst_addr;
    const uint8_t* b = vm_base + src_addr;

    const float a00 = read_bef32_fast(a + 0x00), a01 = read_bef32_fast(a + 0x04), a02 = read_bef32_fast(a + 0x08);
    const float a10 = read_bef32_fast(a + 0x10), a11 = read_bef32_fast(a + 0x14), a12 = read_bef32_fast(a + 0x18);
    const float a20 = read_bef32_fast(a + 0x20), a21 = read_bef32_fast(a + 0x24), a22 = read_bef32_fast(a + 0x28);
    const float a30 = read_bef32_fast(a + 0x30), a31 = read_bef32_fast(a + 0x34), a32 = read_bef32_fast(a + 0x38);

    const float b00 = read_bef32_fast(b + 0x00), b01 = read_bef32_fast(b + 0x04), b02 = read_bef32_fast(b + 0x08);
    const float b10 = read_bef32_fast(b + 0x10), b11 = read_bef32_fast(b + 0x14), b12 = read_bef32_fast(b + 0x18);
    const float b20 = read_bef32_fast(b + 0x20), b21 = read_bef32_fast(b + 0x24), b22 = read_bef32_fast(b + 0x28);
    const float b30 = read_bef32_fast(b + 0x30), b31 = read_bef32_fast(b + 0x34), b32 = read_bef32_fast(b + 0x38);

    write_bef32_fast(a + 0x30, a00 * b30 + a10 * b31 + a20 * b32 + a30);
    write_bef32_fast(a + 0x34, a01 * b30 + a11 * b31 + a21 * b32 + a31);
    write_bef32_fast(a + 0x38, a02 * b30 + a12 * b31 + a22 * b32 + a32);

    write_bef32_fast(a + 0x00, a00 * b00 + a10 * b01 + a20 * b02);
    write_bef32_fast(a + 0x04, a01 * b00 + a11 * b01 + a21 * b02);
    write_bef32_fast(a + 0x08, a02 * b00 + a12 * b01 + a22 * b02);

    write_bef32_fast(a + 0x10, a00 * b10 + a10 * b11 + a20 * b12);
    write_bef32_fast(a + 0x14, a01 * b10 + a11 * b11 + a21 * b12);
    write_bef32_fast(a + 0x18, a02 * b10 + a12 * b11 + a22 * b12);

    write_bef32_fast(a + 0x20, a00 * b20 + a10 * b21 + a20 * b22);
    write_bef32_fast(a + 0x24, a01 * b20 + a11 * b21 + a21 * b22);
    write_bef32_fast(a + 0x28, a02 * b20 + a12 * b21 + a22 * b22);
}

/* Hook 15 helper: atomic scratch bump allocator (EU 0x000124F4) -- 3.3% of guest CPU */
static inline uint32_t cv_alloc_scratch_000124F4(uint32_t pool_idx, uint32_t size)
{
    const uint32_t mgr = read_be32_fast(vm_base + kScratchMgr); /* (0xA8<<16) + 0x3C68 */
    if (!mgr) return 0u;

    const uint32_t pool    = mgr + pool_idx * 24u;
    const uint32_t aligned = (size + 0xFu) & ~0xFu;
    const uint32_t cap     = read_be32_fast(vm_base + pool + 0x2Cu);
    const uint32_t cur0    = read_be32_fast(vm_base + pool + 0x30u);
    if (cur0 + aligned >= cap) return 0u;

    std::atomic_ref<uint32_t> cur_atom(*reinterpret_cast<uint32_t*>(vm_base + pool + 0x30u));
    uint32_t cur_be = cur_atom.load(std::memory_order_relaxed);
    uint32_t old_cur;
    for (;;) {
        old_cur = bswap32_fast(cur_be);
        const uint32_t next_be = bswap32_fast(old_cur + aligned);
        if (cur_atom.compare_exchange_weak(cur_be, next_be,
                                           std::memory_order_acq_rel,
                                           std::memory_order_relaxed)) {
            break;
        }
    }

    const uint32_t cnt = read_be32_fast(vm_base + pool + 0x34u);
    write_be32_fast(vm_base + pool + 0x34u, cnt + 1u);
    const uint32_t base = read_be32_fast(vm_base + pool + 0x38u);
    return base + old_cur;
}

void CV_FD_FN_BUMP_ALLOC(ppu_context* ctx)
{
    if (!native_hot_enabled() || !vm_base) {
        CV_FD_ORIG_BUMP_ALLOC(ctx);
        return;
    }
    ctx->gpr[3] = cv_alloc_scratch_000124F4((uint32_t)ctx->gpr[3], (uint32_t)ctx->gpr[4]);
}

/* Hook 9: vertex + normal + bbox transform loop (EU 0x001D32F0) -- 6.6% of guest CPU */
void CV_FD_FN_VTX_XFORM(ppu_context* ctx)
{
    if (!native_hot_enabled() || !vm_base) {
        CV_FD_ORIG_VTX_XFORM(ctx);
        return;
    }

    const uint32_t mesh = (uint32_t)ctx->gpr[3];
    const uint32_t count = vm_read16(mesh + 0x6);
    const uint32_t ctx_obj = read_be32_fast(vm_base + kCtxObj); /* (0xAC << 16) - 0x1204 */
    if (!ctx_obj) {
        ctx->gpr[3] = mesh + 0x40u + (count << 5);
        return;
    }

    uint32_t pos_buf = cv_alloc_scratch_000124F4(0u, count << 4);
    write_be32_fast(vm_base + ctx_obj + 0x04u, pos_buf);
    uint32_t col_buf = cv_alloc_scratch_000124F4(0u, count << 2);
    write_be32_fast(vm_base + ctx_obj + 0x08u, col_buf);
    write_be32_fast(vm_base + ctx_obj + 0x10u, count);
    uint32_t norm_buf = 0u;
    if (count != 0) {
        norm_buf = cv_alloc_scratch_000124F4(0u, count << 4);
        write_be32_fast(vm_base + ctx_obj + 0x0Cu, norm_buf);
        write_be32_fast(vm_base + ctx_obj + 0x14u, count);
    }

    if (count != 0) {
        uint8_t def_col_be[4];
        std::memcpy(def_col_be, vm_base + kDefCol, 4);
        uint8_t* col_p = vm_base + col_buf;
        for (uint32_t i = 0; i < count; ++i) {
            std::memcpy(col_p + i * 4u, def_col_be, 4);
        }
    }

    vm_write8(ctx_obj + 0x18, 0u);

    if (count != 0) {
        const uint32_t mat_addr = read_be32_fast(vm_base + kMatPtr);
        const uint8_t* m = vm_base + mat_addr;
        const float m00 = read_bef32_fast(m + 0x00), m01 = read_bef32_fast(m + 0x04), m02 = read_bef32_fast(m + 0x08);
        const float m10 = read_bef32_fast(m + 0x10), m11 = read_bef32_fast(m + 0x14), m12 = read_bef32_fast(m + 0x18);
        const float m20 = read_bef32_fast(m + 0x20), m21 = read_bef32_fast(m + 0x24), m22 = read_bef32_fast(m + 0x28);
        const float m30 = read_bef32_fast(m + 0x30), m31 = read_bef32_fast(m + 0x34), m32 = read_bef32_fast(m + 0x38);

        const bool track_bbox = (read_be32_fast(vm_base + kTrackBbox) == 1u);
        float bmin[4] = {}, bmax[4] = {};
        uint32_t bcnt = 0;
        if (track_bbox) {
            for (int k = 0; k < 4; ++k) {
                bmin[k] = read_bef32_fast(vm_base + kBboxMin + k * 4u);
                bmax[k] = read_bef32_fast(vm_base + kBboxMax + k * 4u);
            }
            bcnt = read_be32_fast(vm_base + kBboxCnt);
        }

        const uint8_t* vsrc = vm_base + mesh + 0x40u;
        uint8_t* pdst = vm_base + pos_buf;
        uint8_t* ndst = vm_base + norm_buf;

        const bool flog = frustum_log_enabled() && (read_be32_fast(vm_base + kNoCull) == 0u);
        uint32_t out_and = flog ? 0x3Fu : 0u;
        const float f_zn = flog ? read_bef32_fast(vm_base + kZNear) : 0.0f;
        const float f_zf = flog ? read_bef32_fast(vm_base + kZFar) : 0.0f;
        const float f_cx = flog ? read_bef32_fast(vm_base + kClipX) * 1.15f : 0.0f;
        const float f_cy = flog ? read_bef32_fast(vm_base + kClipY) * 1.15f : 0.0f;
        const float f_pd = flog ? read_bef32_fast(vm_base + kProjD) : 0.0f;

        for (uint32_t i = 0; i < count; ++i) {
            const uint8_t* v = vsrc + i * 0x20u;
            const float x = read_bef32_fast(v + 0x00);
            const float y = read_bef32_fast(v + 0x04);
            const float z = read_bef32_fast(v + 0x08);

            const float px = m00 * x + m10 * y + m20 * z + m30;
            const float py = m01 * x + m11 * y + m21 * z + m31;
            const float pz = m02 * x + m12 * y + m22 * z + m32;

            if (out_and) {
                uint32_t oc = 0u;
                if (pz < f_zn) oc |= 1u;
                if (pz > f_zf) oc |= 2u;
                if (std::fabs(pz) >= 1e-4f) {
                    const float inv_z = std::fabs(f_pd / pz);
                    const float sx = px * inv_z;
                    const float sy = py * inv_z;
                    if (sx < -f_cx) oc |= 4u;
                    if (sx >  f_cx) oc |= 8u;
                    if (sy < -f_cy) oc |= 16u;
                    if (sy >  f_cy) oc |= 32u;
                }
                out_and &= oc;
            }

            uint8_t* po = pdst + i * 0x10u;
            write_bef32_fast(po + 0x0, px);
            write_bef32_fast(po + 0x4, py);
            write_bef32_fast(po + 0x8, pz);

            if (track_bbox) {
                if (px < bmin[0]) bmin[0] = px;
                if (py < bmin[1]) bmin[1] = py;
                if (pz < bmin[2]) bmin[2] = pz;
                if (0.0f < bmin[3]) bmin[3] = 0.0f;
                if (px > bmax[0]) bmax[0] = px;
                if (py > bmax[1]) bmax[1] = py;
                if (pz > bmax[2]) bmax[2] = pz;
                if (0.0f > bmax[3]) bmax[3] = 0.0f;
                bcnt++;
            }

            const float nx = read_bef32_fast(v + 0x10);
            const float ny = read_bef32_fast(v + 0x14);
            const float nz = read_bef32_fast(v + 0x18);

            const float tnx = m00 * nx + m10 * ny + m20 * nz;
            const float tny = m01 * nx + m11 * ny + m21 * nz;
            const float tnz = m02 * nx + m12 * ny + m22 * nz;

            uint8_t* no = ndst + i * 0x10u;
            write_bef32_fast(no + 0x0, tnx);
            write_bef32_fast(no + 0x4, tny);
            write_bef32_fast(no + 0x8, tnz);
        }

        if (flog) {
            cv_frustum_record_32f0(count, out_and != 0u);
        }

        if (track_bbox) {
            for (int k = 0; k < 4; ++k) {
                write_bef32_fast(vm_base + kBboxMin + k * 4u, bmin[k]);
                write_bef32_fast(vm_base + kBboxMax + k * 4u, bmax[k]);
            }
            vm_write32(kBboxCnt, bcnt);
        }
    }

    ctx->gpr[3] = mesh + 0x40u + (count << 5);
}

/* Hook 10: UI/draw command vector push_back fast path (EU 0x00015AFC) -- 4.0%-6.8% of guest CPU */
void cv_fast_enqueue_00015AFC(ppu_context* ctx)
{
    if (!native_hot_enabled() || !vm_base) {
        cv_ui_original_enqueue(ctx);
        return;
    }

    const uint32_t obj  = (uint32_t)ctx->gpr[3];
    const uint32_t item = (uint32_t)ctx->gpr[4];
    const int32_t mode  = (int32_t)read_be32_fast(vm_base + obj + 0x14Cu);

    uint32_t off_cnt, off_cap, off_arr;
    if (mode == 3 || mode == 4) {
        off_cnt = 0xECu;
        off_cap = 0xF0u;
        off_arr = 0xF8u;
    } else if ((read_be32_fast(vm_base + item + 0x10u) & 0x100u) != 0u) {
        off_cnt = 0xD8u;
        off_cap = 0xDCu;
        off_arr = 0xE4u;
    } else {
        off_cnt = 0xC4u;
        off_cap = 0xC8u;
        off_arr = 0xD0u;
    }

    const uint32_t cnt = read_be32_fast(vm_base + obj + off_cnt);
    const uint32_t cap = read_be32_fast(vm_base + obj + off_cap);
    if (cnt < cap) {
        const uint32_t arr = read_be32_fast(vm_base + obj + off_arr);
        write_be32_fast(vm_base + obj + off_cnt, cnt + 1u);
        write_be32_fast(vm_base + arr + (cnt << 2), item);
        return;
    }

    /* Slow path: vector growth / realloc (rare) */
    cv_ui_original_enqueue(ctx);
}

/* Hook 11: vertex buffer + draw descriptor fast allocator (EU 0x0027EF90) -- 4.0% of guest CPU */
void CV_FD_FN_DRAW_DESC_ALLOC(ppu_context* ctx)
{
    if (!native_hot_enabled() || !vm_base) {
        CV_FD_ORIG_DRAW_DESC_ALLOC(ctx);
        return;
    }

    const uint32_t obj       = (uint32_t)ctx->gpr[3];
    const uint32_t vtx_count = (uint32_t)ctx->gpr[4];
    const uint32_t stride    = (uint32_t)ctx->gpr[5];

    if ((read_be32_fast(vm_base + obj + 0x4Cu) & 0x50u) != 0u) {
        ctx->gpr[3] = 0;
        return;
    }

    const uint32_t bytes = (uint32_t)((int32_t)stride * (int32_t)vtx_count);
    const uint32_t vcur  = read_be32_fast(vm_base + obj + 0x1Cu);
    const uint32_t vend  = read_be32_fast(vm_base + obj + 0x24u);
    if (vcur + bytes > vend || vcur == 0u) {
        CV_FD_ORIG_DRAW_DESC_ALLOC(ctx);
        return;
    }

    if ((uint32_t)vm_base[obj + 0x74u] != stride) {
        CV_FD_ORIG_DRAW_DESC_ALLOC(ctx);
        return;
    }

    const uint32_t dcur = read_be32_fast(vm_base + obj + 0x14u);
    const uint32_t dend = read_be32_fast(vm_base + obj + 0x18u);
    if (dcur + 0x20u > dend) {
        CV_FD_ORIG_DRAW_DESC_ALLOC(ctx);
        return;
    }

    /* Fast path: bump vertex & descriptor cursors, write 32-byte desc + 8-byte sort entry */
    write_be32_fast(vm_base + obj + 0x1Cu, vcur + bytes);
    write_be32_fast(vm_base + obj + 0x14u, dcur + 0x20u);

    uint8_t* d = vm_base + dcur;
    const uint32_t w0 = (read_be32_fast(d + 0x0) & 0xFFFF0000u)
                      | (read_be32_fast(vm_base + obj + 0x5Cu) & 0x00000FFFu);
    write_be32_fast(d + 0x00u, w0);
    write_be32_fast(d + 0x04u, read_be32_fast(vm_base + obj + 0x1CCu));
    write_be32_fast(d + 0x08u, read_be32_fast(vm_base + obj + 0x1C0u));
    write_be32_fast(d + 0x0Cu, read_be32_fast(vm_base + obj + 0x1C4u));
    write_be32_fast(d + 0x10u, read_be32_fast(vm_base + obj + 0xB8u));
    write_be32_fast(d + 0x14u, vtx_count);
    const uint32_t vtx_off = read_be32_fast(vm_base + obj + 0xB4u);
    write_be32_fast(d + 0x18u, vtx_off);
    write_be32_fast(d + 0x1Cu, 0u);
    write_be32_fast(vm_base + obj + 0xB4u, vtx_off + vtx_count);

    const uint32_t sort_idx  = read_be32_fast(vm_base + obj + 0x38u);
    const uint32_t sort_base = read_be32_fast(vm_base + obj + 0x34u);
    write_be32_fast(vm_base + obj + 0x38u, sort_idx + 1u);

    const uint32_t key_base = read_be32_fast(vm_base + obj + 0x194u);
    const uint32_t key_add  = read_be32_fast(vm_base + obj + 0x1A8u);
    const uint32_t sort_key = (key_base & 0xF8000000u)
                            | (((key_base & 0x07FFFFFFu) + key_add) & 0x07FFFFFFu);
    uint8_t* s = vm_base + sort_base + (sort_idx << 3);
    write_be32_fast(s + 0x0u, sort_key);
    write_be32_fast(s + 0x4u, dcur);

    ctx->gpr[3] = vcur;
}

/* Hook 12: 2-way bottom-up stable merge sort of 8-byte (u32 key, u32 val) pairs (EU 0x00286E74) -- 4.1% of guest CPU */
static inline uint32_t be_key_from_pair64(uint64_t entry)
{
#ifdef _MSC_VER
    return _byteswap_ulong((uint32_t)entry);
#else
    return __builtin_bswap32((uint32_t)entry);
#endif
}

void CV_FD_FN_MERGE_SORT2(ppu_context* ctx)
{
    if (!native_hot_enabled() || !vm_base) {
        CV_FD_ORIG_MERGE_SORT2(ctx);
        return;
    }

    const uint32_t arr_in  = (uint32_t)ctx->gpr[4];
    const int32_t  count   = (int32_t)ctx->gpr[5];
    const uint32_t arr_tmp = (uint32_t)ctx->gpr[6];

    int32_t pow2 = 1;
    for (int i = 0; i < 32 && count > pow2; ++i) {
        pow2 <<= 1;
    }
    if (pow2 < 2) {
        ctx->gpr[3] = arr_in;
        return;
    }

    uint64_t* a_in  = reinterpret_cast<uint64_t*>(vm_base + arr_in);
    uint64_t* a_tmp = reinterpret_cast<uint64_t*>(vm_base + arr_tmp);

    /* Pass 1: sort pairs (run = 2) from arr_in -> arr_tmp */
    for (int32_t i = 0; i < count; i += 2) {
        if (i + 1 >= count) {
            a_tmp[i] = a_in[i];
        } else {
            const uint64_t e0 = a_in[i];
            const uint64_t e1 = a_in[i + 1];
            if (be_key_from_pair64(e0) > be_key_from_pair64(e1)) {
                a_tmp[i]     = e1;
                a_tmp[i + 1] = e0;
            } else {
                a_tmp[i]     = e0;
                a_tmp[i + 1] = e1;
            }
        }
    }

    int parity = 1;
    for (int32_t run = 4; run <= pow2; run <<= 1) {
        const uint64_t* src = parity ? a_tmp : a_in;
        uint64_t*       dst = parity ? a_in  : a_tmp;
        const int32_t half = run >> 1;

        for (int32_t start = 0; start < count; start += run) {
            int32_t mid = start + half;
            if (mid > count) mid = count;
            int32_t end = mid + half;
            if (end > count) end = count;

            int32_t l = start, r = mid, o = start;
            while (l < mid && r < end) {
                const uint64_t el = src[l];
                const uint64_t er = src[r];
                if (be_key_from_pair64(el) > be_key_from_pair64(er)) {
                    dst[o++] = er;
                    ++r;
                } else {
                    dst[o++] = el;
                    ++l;
                }
            }
            if (l < mid) {
                std::memcpy(dst + o, src + l, (size_t)(mid - l) * 8u);
            } else if (r < end) {
                std::memcpy(dst + o, src + r, (size_t)(end - r) * 8u);
            }
        }
        parity ^= 1;
    }

    ctx->gpr[3] = parity ? arr_tmp : arr_in;
}

/* Hook 13: MT Framework vertex attribute packer (EU 0x004BAAC0) -- 6.9% (#1) of guest CPU */
static inline float cv_clampf(float v, float lo, float hi)
{
    if (v < lo) v = lo;
    if (v > hi) v = hi;
    return v;
}

static inline uint16_t cv_float32_to_half(uint32_t bits)
{
    const int32_t  exp  = (int32_t)((bits >> 23) & 0xFFu) - 112;
    const uint32_t mant = bits & 0x007FFFFFu;
    uint16_t h = (uint16_t)((bits >> 16) & 0x8000u);

    if (exp <= 0) {
        if (exp < -10) return 0u;
        const uint32_t m = (mant | 0x00800000u) >> (1 - exp);
        return (uint16_t)(h | (m >> 13));
    }
    if (exp == 0x8F) {
        if (mant == 0u) return (uint16_t)(h | 0x7C00u);
        const uint16_t q = (uint16_t)((bits >> 13) & 0x3FFu);
        return (uint16_t)(h | 0x7C00u | q | (q == 0u ? 1u : 0u));
    }
    if (exp > 0x1E) return (uint16_t)(h | 0x7C00u);
    return (uint16_t)(h | ((uint32_t)exp << 10) | ((bits >> 13) & 0x3FFu));
}

template <typename ReadFloatFn>
static inline uint32_t cv_pack_vertex_attr_impl(uint8_t* dst_base, uint32_t desc, ReadFloatFn read_src_f)
{
    const uint32_t dst_off = (desc >> 1) & 0x1FFu;
    const uint32_t fmt     = (desc >> 21) & 0x1Fu;
    const uint32_t count   = (desc >> 14) & 0x7Fu;
    const uint32_t sbase   = (desc >> 10) & 0x0Fu;
    uint8_t* dst = dst_base + dst_off;
    const uint8_t* ctab = vm_base + kVtxPackTab; /* (0x4C << 16) - 21896 */

    switch (fmt) {
    case 1: /* Float32 */
        for (uint32_t i = 0; i < count; ++i) {
            write_bef32_fast(dst + i * 4u, read_src_f(sbase + i));
        }
        return count;

    case 2: /* Float16 */
        for (uint32_t i = 0; i < count; ++i) {
            const float f = read_src_f(sbase + i);
            uint32_t bits;
            std::memcpy(&bits, &f, 4);
            write_be16_fast(dst + i * 2u, cv_float32_to_half(bits));
        }
        return count;

    case 3: { /* Int16 clamped */
        if (count == 0u) return 0u;
        const float lo = read_bef32_fast(ctab + 0x00u);
        const float hi = read_bef32_fast(ctab + 0x04u);
        for (uint32_t i = 0; i < count; ++i) {
            const float f = cv_clampf(read_src_f(sbase + i), lo, hi);
            write_be16_fast(dst + i * 2u, (uint16_t)(int32_t)f);
        }
        return count;
    }

    case 4: { /* Int16 (f - c08) clamped */
        if (count == 0u) return 0u;
        const float lo   = read_bef32_fast(ctab + 0x00u);
        const float hi   = read_bef32_fast(ctab + 0x04u);
        const float bias = read_bef32_fast(ctab + 0x08u);
        for (uint32_t i = 0; i < count; ++i) {
            const float f = cv_clampf(read_src_f(sbase + i) - bias, lo, hi);
            write_be16_fast(dst + i * 2u, (uint16_t)(int32_t)f);
        }
        return count;
    }

    case 5: { /* Snorm16: clamp(f, c0C, c10) * c04 */
        if (count == 0u) return 0u;
        const float lo    = read_bef32_fast(ctab + 0x0Cu);
        const float hi    = read_bef32_fast(ctab + 0x10u);
        const float scale = read_bef32_fast(ctab + 0x04u);
        for (uint32_t i = 0; i < count; ++i) {
            const float f = cv_clampf(read_src_f(sbase + i), lo, hi) * scale;
            write_be16_fast(dst + i * 2u, (uint16_t)(int32_t)f);
        }
        return count;
    }

    case 6: { /* Snorm16 biased: clamp((f - c14) * c18, c0C, c10) * c04 */
        if (count == 0u) return 0u;
        const float c14   = read_bef32_fast(ctab + 0x14u);
        const float c18   = read_bef32_fast(ctab + 0x18u);
        const float lo    = read_bef32_fast(ctab + 0x0Cu);
        const float hi    = read_bef32_fast(ctab + 0x10u);
        const float scale = read_bef32_fast(ctab + 0x04u);
        for (uint32_t i = 0; i < count; ++i) {
            const float f = cv_clampf((read_src_f(sbase + i) - c14) * c18, lo, hi) * scale;
            write_be16_fast(dst + i * 2u, (uint16_t)(int32_t)f);
        }
        return count;
    }

    case 7: { /* Uint8: clamp(f + c1C, c20, c24) */
        if (count == 0u) return 0u;
        const float bias = read_bef32_fast(ctab + 0x1Cu);
        const float lo   = read_bef32_fast(ctab + 0x20u);
        const float hi   = read_bef32_fast(ctab + 0x24u);
        for (uint32_t i = 0; i < count; ++i) {
            const float f = cv_clampf(read_src_f(sbase + i) + bias, lo, hi);
            dst[i] = (uint8_t)(int64_t)f;
        }
        return count;
    }

    case 8: { /* Uint8: clamp(f, c20, c24) */
        if (count == 0u) return 0u;
        const float lo = read_bef32_fast(ctab + 0x20u);
        const float hi = read_bef32_fast(ctab + 0x24u);
        for (uint32_t i = 0; i < count; ++i) {
            const float f = cv_clampf(read_src_f(sbase + i), lo, hi);
            dst[i] = (uint8_t)(int64_t)f;
        }
        return count;
    }

    case 9: { /* Uint8: clamp(f * c40 + c40, c20, c24) */
        if (count == 0u) return 0u;
        const float c40 = read_bef32_fast(ctab + 0x40u);
        const float lo  = read_bef32_fast(ctab + 0x20u);
        const float hi  = read_bef32_fast(ctab + 0x24u);
        for (uint32_t i = 0; i < count; ++i) {
            const float f = cv_clampf(read_src_f(sbase + i) * c40 + c40, lo, hi);
            dst[i] = (uint8_t)(int64_t)f;
        }
        return count;
    }

    case 10: { /* Unorm8: clamp(f, c20, c10) * c24 */
        if (count == 0u) return 0u;
        const float lo    = read_bef32_fast(ctab + 0x20u);
        const float hi    = read_bef32_fast(ctab + 0x10u);
        const float scale = read_bef32_fast(ctab + 0x24u);
        for (uint32_t i = 0; i < count; ++i) {
            const float f = cv_clampf(read_src_f(sbase + i), lo, hi) * scale;
            dst[i] = (uint8_t)(int64_t)f;
        }
        return count;
    }

    case 11: { /* Packed 3-comp signed -> RGBA8 (r<<24 | g<<16 | b<<8 | 0xFF) */
        if (count == 0u) return 0u;
        const float lo  = read_bef32_fast(ctab + 0x0Cu);
        const float hi  = read_bef32_fast(ctab + 0x10u);
        const float c40 = read_bef32_fast(ctab + 0x40u);
        for (uint32_t i = 0; i < count; ++i) {
            const uint32_t idx = (sbase + i) * 3u;
            const uint32_t b0 = (uint32_t)(uint8_t)(int64_t)(cv_clampf(read_src_f(idx + 0u), lo, hi) * c40 + c40);
            const uint32_t b1 = (uint32_t)(uint8_t)(int64_t)(cv_clampf(read_src_f(idx + 1u), lo, hi) * c40 + c40);
            const uint32_t b2 = (uint32_t)(uint8_t)(int64_t)(cv_clampf(read_src_f(idx + 2u), lo, hi) * c40 + c40);
            write_be32_fast(dst + i * 4u, (b0 << 24) | (b1 << 16) | (b2 << 8) | 0xFFu);
        }
        return count * 3u;
    }

    case 12: { /* Packed 3-comp unorm -> 0xFF000000 | (b2<<16) | (b1<<8) | b0 */
        if (count == 0u) return 0u;
        const float lo    = read_bef32_fast(ctab + 0x20u);
        const float hi    = read_bef32_fast(ctab + 0x10u);
        const float scale = read_bef32_fast(ctab + 0x24u);
        for (uint32_t i = 0; i < count; ++i) {
            const uint32_t idx = (sbase + i) * 3u;
            const uint32_t b0 = (uint32_t)(uint8_t)(int64_t)(cv_clampf(read_src_f(idx + 0u), lo, hi) * scale);
            const uint32_t b1 = (uint32_t)(uint8_t)(int64_t)(cv_clampf(read_src_f(idx + 1u), lo, hi) * scale);
            const uint32_t b2 = (uint32_t)(uint8_t)(int64_t)(cv_clampf(read_src_f(idx + 2u), lo, hi) * scale);
            write_be32_fast(dst + i * 4u, 0xFF000000u | (b2 << 16) | (b1 << 8) | b0);
        }
        return count * 3u;
    }

    case 13: { /* Uint8 Reinhard: clamp(f / (f + c10), c20, c10) * c24 */
        if (count == 0u) return 0u;
        const float c10   = read_bef32_fast(ctab + 0x10u);
        const float lo    = read_bef32_fast(ctab + 0x20u);
        const float scale = read_bef32_fast(ctab + 0x24u);
        for (uint32_t i = 0; i < count; ++i) {
            const float x = read_src_f(sbase + i);
            const float f = cv_clampf(x / (x + c10), lo, c10) * scale;
            dst[i] = (uint8_t)(int64_t)f;
        }
        return count;
    }

    case 14: { /* Packed 4-comp unorm -> (b3<<24) | (b2<<16) | (b1<<8) | b0 */
        if (count == 0u) return 0u;
        const float lo    = read_bef32_fast(ctab + 0x20u);
        const float hi    = read_bef32_fast(ctab + 0x10u);
        const float scale = read_bef32_fast(ctab + 0x24u);
        for (uint32_t i = 0; i < count; ++i) {
            const uint32_t idx = (sbase + i) * 4u;
            const uint32_t b0 = (uint32_t)(uint8_t)(int64_t)(cv_clampf(read_src_f(idx + 0u), lo, hi) * scale);
            const uint32_t b1 = (uint32_t)(uint8_t)(int64_t)(cv_clampf(read_src_f(idx + 1u), lo, hi) * scale);
            const uint32_t b2 = (uint32_t)(uint8_t)(int64_t)(cv_clampf(read_src_f(idx + 2u), lo, hi) * scale);
            const uint32_t b3 = (uint32_t)(uint8_t)(int64_t)(cv_clampf(read_src_f(idx + 3u), lo, hi) * scale);
            write_be32_fast(dst + i * 4u, (b3 << 24) | (b2 << 16) | (b1 << 8) | b0);
        }
        return count * 4u;
    }

    default:
        return 0u;
    }
}

void CV_FD_FN_VTX_PACKER(ppu_context* ctx)
{
    if (!native_hot_enabled() || !vm_base) {
        CV_FD_ORIG_VTX_PACKER(ctx);
        return;
    }

    const uint32_t dst_addr  = (uint32_t)ctx->gpr[3];
    const uint32_t desc_addr = (uint32_t)ctx->gpr[4];
    const uint32_t src_addr  = (uint32_t)ctx->gpr[5];
    const uint32_t desc      = read_be32_fast(vm_base + desc_addr + 0x4u);
    const uint8_t* src_ptr   = vm_base + src_addr;

    ctx->gpr[3] = cv_pack_vertex_attr_impl(
        vm_base + dst_addr, desc,
        [src_ptr](uint32_t idx) -> float { return read_bef32_fast(src_ptr + idx * 4u); });
}

/* Hook 14: Vertex buffer builder (EU 0x0001D5D0) -- 4.3% of guest CPU, inlines CV_FD_FN_VTX_PACKER */
void CV_FD_FN_VB_BUILDER(ppu_context* ctx)
{
    if (!native_hot_enabled() || !vm_base) {
        CV_FD_ORIG_VB_BUILDER(ctx);
        return;
    }

    const uint32_t geom_hdr = (uint32_t)ctx->gpr[4] + 4u;
    const uint32_t mesh     = (uint32_t)ctx->gpr[5];

    /* 1. Free existing vertex buffer if present */
    const uint32_t old_buf = read_be32_fast(vm_base + mesh + 0x34u);
    if (old_buf != 0u) {
        const uint32_t alloc = read_be32_fast(vm_base + kAllocObj + 0x20u);
        const uint32_t vt    = read_be32_fast(vm_base + alloc);
        const uint32_t fn    = read_be32_fast(vm_base + read_be32_fast(vm_base + vt + 0x28u));
        GuestCall call(ctx);
        call.run_indirect_u32(fn, alloc, old_buf);
        write_be32_fast(vm_base + mesh + 0x34u, 0u);
    }

    /* 2. Compute total vertex buffer size */
    const uint32_t num_groups  = read_be32_fast(vm_base + mesh + 0x04u);
    const uint32_t global_obj  = read_be32_fast(vm_base + kGlobalObj); /* (0xB0<<16) - 0x4224 */
    const uint32_t vfmt        = read_be32_fast(vm_base + global_obj + 0x208u);
    const uint32_t vtx_stride  = read_be32_fast(vm_base + vfmt + 0x20u);
    const bool     has_normals = (read_be32_fast(vm_base + geom_hdr + 0x10u) != 0u);
    const uint32_t prim_type   = read_be32_fast(vm_base + mesh + 0x3Cu);
    const uint32_t groups_base = read_be32_fast(vm_base + mesh + 0x28u);

    uint32_t total_bytes = 0u;
    for (uint32_t g = 0; g < num_groups; ++g) {
        uint32_t n = read_be32_fast(vm_base + groups_base + g * 0x10u);
        if (prim_type == 3u) {
            n = n * 3u - 6u;
        }
        total_bytes += n * vtx_stride;
    }

    /* 3. Allocate new 16-byte-aligned vertex buffer */
    uint32_t dst_vbuf = 0u;
    {
        const uint32_t alloc = read_be32_fast(vm_base + kAllocObj + 0x20u);
        const uint32_t vt    = read_be32_fast(vm_base + alloc);
        const uint32_t fn    = read_be32_fast(vm_base + read_be32_fast(vm_base + vt + 0x1Cu));
        GuestCall call(ctx);
        dst_vbuf = call.run_indirect_u32(fn, alloc, total_bytes, 0x10u);
    }
    write_be32_fast(vm_base + mesh + 0x34u, dst_vbuf);

    if (num_groups == 0u || !dst_vbuf) return;

    const uint32_t attr_descs = read_be32_fast(vm_base + vfmt + 0x18u);
    const uint32_t desc_pos   = read_be32_fast(vm_base + attr_descs + 0x04u);
    const uint32_t desc_nrm   = read_be32_fast(vm_base + attr_descs + 0x0Cu);
    const uint32_t desc_uv    = read_be32_fast(vm_base + attr_descs + 0x14u);

    const uint32_t pos_base = read_be32_fast(vm_base + geom_hdr + 0x00u);
    const uint32_t nrm_base = read_be32_fast(vm_base + geom_hdr + 0x08u);
    const uint8_t* def_nrm_p = vm_base + kDefNrm; /* (0xBC<<16) - 26544 */
    const float def_nrm[4] = {
        read_bef32_fast(def_nrm_p + 0x0u), read_bef32_fast(def_nrm_p + 0x4u),
        read_bef32_fast(def_nrm_p + 0x8u), read_bef32_fast(def_nrm_p + 0xCu)
    };
    const uint8_t* def_uv_p = vm_base + kDefUv; /* (0xBC<<16) - 26900 */
    const float def_uv[2] = {
        read_bef32_fast(def_uv_p + 0x0u), read_bef32_fast(def_uv_p + 0x4u)
    };

    uint8_t* out_p = vm_base + dst_vbuf;

    if (prim_type == 3u) {
        /* Triangle strip -> triangle list */
        for (uint32_t g = 0; g < num_groups; ++g) {
            const uint32_t grp_addr     = groups_base + g * 0x10u;
            const uint32_t n_verts      = read_be32_fast(vm_base + grp_addr + 0x0u);
            const uint8_t  flip_winding = vm_base[grp_addr + 0x4u];
            const uint8_t  has_uv       = vm_base[grp_addr + 0x5u];
            const uint32_t idx_arr      = read_be32_fast(vm_base + grp_addr + 0x8u);
            const uint32_t uv_arr       = read_be32_fast(vm_base + grp_addr + 0xCu);
            const uint32_t n_tris       = n_verts - 2u;

            for (uint32_t t = 0; t < n_tris; ++t) {
                const bool rev = ((flip_winding != 0u) ^ ((t & 1u) != 0u));
                for (uint32_t k = 0; k < 3u; ++k) {
                    const uint32_t vi   = rev ? (t + 2u - k) : (t + k);
                    const uint32_t vidx = (vi < n_verts && idx_arr)
                                        ? read_be32_fast(vm_base + idx_arr + vi * 4u)
                                        : read_be32_fast(vm_base);

                    const uint8_t* pp = vm_base + ((pos_base + (vidx << 4)) & ~0xFu);
                    const float pos[4] = {
                        read_bef32_fast(pp + 0x0u), read_bef32_fast(pp + 0x4u),
                        read_bef32_fast(pp + 0x8u), read_bef32_fast(pp + 0xCu)
                    };

                    float nrm[4];
                    if (has_normals) {
                        const uint8_t* np = vm_base + ((nrm_base + (vidx << 4)) & ~0xFu);
                        nrm[0] = read_bef32_fast(np + 0x0u);
                        nrm[1] = read_bef32_fast(np + 0x4u);
                        nrm[2] = read_bef32_fast(np + 0x8u);
                        nrm[3] = read_bef32_fast(np + 0xCu);
                    } else {
                        nrm[0] = def_nrm[0]; nrm[1] = def_nrm[1];
                        nrm[2] = def_nrm[2]; nrm[3] = def_nrm[3];
                    }

                    float uv[2];
                    if (has_uv == 1u) {
                        const uint32_t uv_a = (vi < n_verts && uv_arr) ? (uv_arr + vi * 12u) : 0u;
                        uv[0] = read_bef32_fast(vm_base + uv_a + 0x0u);
                        uv[1] = read_bef32_fast(vm_base + uv_a + 0x4u);
                    } else {
                        uv[0] = def_uv[0];
                        uv[1] = def_uv[1];
                    }

                    cv_pack_vertex_attr_impl(out_p, desc_pos, [&pos](uint32_t i) { return pos[i]; });
                    cv_pack_vertex_attr_impl(out_p, desc_nrm, [&nrm](uint32_t i) { return nrm[i]; });
                    cv_pack_vertex_attr_impl(out_p, desc_uv,  [&uv ](uint32_t i) { return uv[i]; });
                    out_p += vtx_stride;
                }
            }
        }
    } else {
        /* Direct vertex list */
        const float nrm_scale = read_bef32_fast(vm_base + kNrmScale); /* (0x2<<16) - 10804 */
        for (uint32_t g = 0; g < num_groups; ++g) {
            const uint32_t grp_addr = groups_base + g * 0x10u;
            const uint32_t n_verts  = read_be32_fast(vm_base + grp_addr + 0x0u);
            const uint8_t  flip_nrm = vm_base[grp_addr + 0x4u];
            const uint8_t  has_uv   = vm_base[grp_addr + 0x5u];
            const uint32_t idx_arr  = read_be32_fast(vm_base + grp_addr + 0x8u);
            const uint32_t uv_arr   = read_be32_fast(vm_base + grp_addr + 0xCu);

            for (uint32_t vi = 0; vi < n_verts; ++vi) {
                const uint32_t vidx = idx_arr
                                    ? read_be32_fast(vm_base + idx_arr + vi * 4u)
                                    : read_be32_fast(vm_base);

                const uint8_t* pp = vm_base + ((pos_base + (vidx << 4)) & ~0xFu);
                const float pos[4] = {
                    read_bef32_fast(pp + 0x0u), read_bef32_fast(pp + 0x4u),
                    read_bef32_fast(pp + 0x8u), read_bef32_fast(pp + 0xCu)
                };

                float nrm[4];
                if (has_normals) {
                    const uint8_t* np = vm_base + ((nrm_base + (vidx << 4)) & ~0xFu);
                    nrm[0] = read_bef32_fast(np + 0x0u);
                    nrm[1] = read_bef32_fast(np + 0x4u);
                    nrm[2] = read_bef32_fast(np + 0x8u);
                    nrm[3] = read_bef32_fast(np + 0xCu);
                    if (flip_nrm != 0u) {
                        nrm[0] *= nrm_scale;
                        nrm[1] *= nrm_scale;
                        nrm[2] *= nrm_scale;
                        nrm[3] *= nrm_scale;
                    }
                } else {
                    nrm[0] = def_nrm[0]; nrm[1] = def_nrm[1];
                    nrm[2] = def_nrm[2]; nrm[3] = def_nrm[3];
                }

                float uv[2];
                if (has_uv == 1u) {
                    const uint32_t uv_a = uv_arr ? (uv_arr + vi * 12u) : 0u;
                    uv[0] = read_bef32_fast(vm_base + uv_a + 0x0u);
                    uv[1] = read_bef32_fast(vm_base + uv_a + 0x4u);
                } else {
                    uv[0] = def_uv[0];
                    uv[1] = def_uv[1];
                }

                cv_pack_vertex_attr_impl(out_p, desc_pos, [&pos](uint32_t i) { return pos[i]; });
                cv_pack_vertex_attr_impl(out_p, desc_nrm, [&nrm](uint32_t i) { return nrm[i]; });
                cv_pack_vertex_attr_impl(out_p, desc_uv,  [&uv ](uint32_t i) { return uv[i]; });
                out_p += vtx_stride;
            }
        }
    }
}

/* Hook 16: Recursive tree traversal (EU 0x00549F7C) -- 4.0% of guest CPU */
static uint32_t cv_tree_traverse_00549F7C(uint32_t tree_base, uint32_t node_val,
                                          uint32_t entries, uint32_t out_ptr)
{
    const uint32_t hi = node_val & 0xFFFFF000u;
    if (hi != 0u) {
        write_be32_fast(vm_base + out_ptr, hi);
        out_ptr += 4u;
    }

    const uint32_t node_addr = tree_base + ((node_val & 0x0FFFu) << 3);
    const uint32_t hdr       = read_be32_fast(vm_base + node_addr + 0x0u);
    uint32_t       child_ptr = read_be32_fast(vm_base + node_addr + 0x4u);
    const uint32_t n_branch  = (hdr >> 6) & 0x3Fu;
    const uint32_t n_leaf    = hdr & 0x3Fu;

    for (uint32_t i = 0; i < n_branch; ++i) {
        const uint32_t idx  = read_be16_fast(vm_base + child_ptr);
        child_ptr += 2u;
        const uint32_t cval = read_be32_fast(vm_base + entries + (idx << 3) + 4u);
        out_ptr = cv_tree_traverse_00549F7C(tree_base, cval, entries, out_ptr);
    }

    for (uint32_t i = 0; i < n_leaf; ++i) {
        const uint32_t idx  = read_be16_fast(vm_base + child_ptr);
        child_ptr += 2u;
        const uint32_t lval = read_be32_fast(vm_base + entries + (idx << 3) + 4u);
        if (lval != 0u) {
            write_be32_fast(vm_base + out_ptr, lval);
            out_ptr += 4u;
        }
    }

    return out_ptr;
}

void CV_FD_FN_TREE_TRAV(ppu_context* ctx)
{
    if (!native_hot_enabled() || !vm_base) {
        CV_FD_ORIG_TREE_TRAV(ctx);
        return;
    }

    const uint32_t obj       = (uint32_t)ctx->gpr[3];
    const uint32_t node_val  = (uint32_t)ctx->gpr[4];
    const uint32_t entries   = (uint32_t)ctx->gpr[5];
    const uint32_t out_ptr   = (uint32_t)ctx->gpr[6];
    const uint32_t tree_base = read_be32_fast(vm_base + obj + 0x48u);

    ctx->gpr[3] = cv_tree_traverse_00549F7C(tree_base, node_val, entries, out_ptr);
}

/* =========================================================================
 * Hook 15: Frustum Culling Telemetry + Safe Culling (EU 0x001D2028 njCnkDrawModel)
 * ========================================================================= */
static bool frustum_log_enabled()
{
#ifdef CV_DISTRIBUTION
    return false;
#else
    static const bool en = []() {
        const char* v = std::getenv("CV_FRUSTUM_LOG");
        return v && v[0] && std::strcmp(v, "0") != 0;
    }();
    return en;
#endif
}

static bool frustum_cull_enabled()
{
#ifdef CV_DISTRIBUTION
    return false;
#else
    static const bool en = []() {
        const char* v = std::getenv("CV_FRUSTUM_CULL");
        return v && v[0] && std::strcmp(v, "0") != 0;
    }();
    return en;
#endif
}

struct FrustumStats {
    uint64_t calls = 0;
    uint64_t game_culled = 0;
    uint64_t our_culled = 0;
    uint64_t flag_off = 0;
    uint64_t no_cull = 0;
    uint64_t bbox_on = 0;
    uint64_t r_zero = 0;
    uint64_t would_cull_15 = 0;
    uint64_t v32f0_calls = 0;
    uint64_t v32f0_offscreen = 0;
    uint64_t v32f0_verts = 0;
    uint64_t v32f0_offscreen_verts = 0;
    std::chrono::steady_clock::time_point last_log = std::chrono::steady_clock::now();
};
static FrustumStats g_fstats;

static void cv_frustum_record_32f0(uint32_t count, bool all_offscreen)
{
    g_fstats.v32f0_calls++;
    g_fstats.v32f0_verts += count;
    if (all_offscreen) {
        g_fstats.v32f0_offscreen++;
        g_fstats.v32f0_offscreen_verts += count;
    }
}

static bool sphere_outside_frustum_15(float cx, float cy, float cz, float r,
                                      float z_near, float z_far,
                                      float clip_x, float clip_y,
                                      float proj_d, float ax, float ay)
{
    if (!(r > 0.0f) || std::isnan(cx) || std::isnan(cy) || std::isnan(cz) || std::isnan(r))
        return false;
    const float rp = r * 1.15f;
    if (cz + rp < z_near) return true;
    if (z_far < cz - rp)  return true;
    if (std::fabs(cz) < 1e-4f) return false;
    const float inv_z = std::fabs(proj_d / cz);
    const float sx = cx * inv_z;
    const float sr = rp * inv_z;
    const float cx_lim = clip_x * 1.15f;
    if (sx + sr * ax < -cx_lim) return true;
    if (cx_lim < sx - sr * ax)  return true;
    const float sy = cy * inv_z;
    const float cy_lim = clip_y * 1.15f;
    if (sy + sr * ay < -cy_lim) return true;
    if (cy_lim < sy - sr * ay)  return true;
    return false;
}

void CV_FD_FN_DRAW_MODEL(ppu_context* ctx)
{
    const bool do_log  = frustum_log_enabled();
    const bool do_cull = frustum_cull_enabled();
    if ((!do_log && !do_cull) || !vm_base) {
        CV_FD_ORIG_DRAW_MODEL(ctx);
        return;
    }

    const uint32_t model = (uint32_t)ctx->gpr[3];
    if (!model) {
        CV_FD_ORIG_DRAW_MODEL(ctx);
        return;
    }

    const uint32_t cull_on = read_be32_fast(vm_base + kCullOn);
    const uint32_t no_cull = read_be32_fast(vm_base + kNoCull);
    const uint32_t bbox_on = read_be32_fast(vm_base + kTrackBbox);
    const float r          = read_bef32_fast(vm_base + model + 0x14u);

    const float z_near = read_bef32_fast(vm_base + kZNear);
    const float z_far  = read_bef32_fast(vm_base + kZFar);
    const float clip_x = read_bef32_fast(vm_base + kClipX);
    const float clip_y = read_bef32_fast(vm_base + kClipY);
    const float proj_d = read_bef32_fast(vm_base + kProjD);
    const float ax     = read_bef32_fast(vm_base + kAspectX);
    const float ay     = read_bef32_fast(vm_base + kAspectY);

    bool outside = false;
    if (no_cull == 0u && bbox_on == 0u && r > 0.0f) {
        const uint32_t mat_addr = read_be32_fast(vm_base + kMatPtr);
        if (mat_addr) {
            const uint8_t* m = vm_base + mat_addr;
            const float x = read_bef32_fast(vm_base + model + 0x08u);
            const float y = read_bef32_fast(vm_base + model + 0x0Cu);
            const float z = read_bef32_fast(vm_base + model + 0x10u);
            const float cx = read_bef32_fast(m + 0x00) * x + read_bef32_fast(m + 0x10) * y + read_bef32_fast(m + 0x20) * z + read_bef32_fast(m + 0x30);
            const float cy = read_bef32_fast(m + 0x04) * x + read_bef32_fast(m + 0x14) * y + read_bef32_fast(m + 0x24) * z + read_bef32_fast(m + 0x34);
            const float cz = read_bef32_fast(m + 0x08) * x + read_bef32_fast(m + 0x18) * y + read_bef32_fast(m + 0x28) * z + read_bef32_fast(m + 0x38);
            outside = sphere_outside_frustum_15(cx, cy, cz, r, z_near, z_far, clip_x, clip_y, proj_d, ax, ay);
        }
    }

    if (do_cull && outside) {
        write_be32_fast(vm_base + kLastModel, model);
        ctx->gpr[3] = 0;
        if (do_log) {
            g_fstats.calls++;
            g_fstats.our_culled++;
        }
        return;
    }

    CV_FD_ORIG_DRAW_MODEL(ctx);

    if (do_log) {
        g_fstats.calls++;
        if (ctx->gpr[3] == 0) g_fstats.game_culled++;
        if (cull_on == 0u)    g_fstats.flag_off++;
        if (no_cull != 0u)    g_fstats.no_cull++;
        if (bbox_on != 0u)    g_fstats.bbox_on++;
        if (!(r > 0.0f))      g_fstats.r_zero++;
        if (outside && ctx->gpr[3] != 0) g_fstats.would_cull_15++;

        if ((g_fstats.calls & 0xFFu) == 0u) {
            auto now = std::chrono::steady_clock::now();
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - g_fstats.last_log).count();
            if (ms >= 2000) {
                std::fprintf(stderr,
                    "[FRUSTUM] 1D2028 calls=%llu game_cull=%llu our_cull=%llu would_cull15=%llu "
                    "flag_off=%llu no_cull=%llu bbox=%llu r0=%llu | "
                    "32F0 calls=%llu off=%llu verts=%llu off_v=%llu | "
                    "zn=%.2f zf=%.1f cx=%.1f cy=%.1f pd=%.1f ax=%.2f ay=%.2f\n",
                    (unsigned long long)g_fstats.calls,
                    (unsigned long long)g_fstats.game_culled,
                    (unsigned long long)g_fstats.our_culled,
                    (unsigned long long)g_fstats.would_cull_15,
                    (unsigned long long)g_fstats.flag_off,
                    (unsigned long long)g_fstats.no_cull,
                    (unsigned long long)g_fstats.bbox_on,
                    (unsigned long long)g_fstats.r_zero,
                    (unsigned long long)g_fstats.v32f0_calls,
                    (unsigned long long)g_fstats.v32f0_offscreen,
                    (unsigned long long)g_fstats.v32f0_verts,
                    (unsigned long long)g_fstats.v32f0_offscreen_verts,
                    z_near, z_far, clip_x, clip_y, proj_d, ax, ay);
                std::fflush(stderr);
                g_fstats = FrustumStats{};
                g_fstats.last_log = now;
            }
        }
    }
}

/* =========================================================================
 * Hook 17: cCamera::testSphere (6-plane frustum sphere test)
 *          EU 0x002858D0 | US 0x00283910 | JP 0x00284200
 * Hook 18: mat4x4 * vec4
 *          EU 0x00204404 | US 0x00202464 | JP 0x00202D54
 * Hook 19: uModel::cullParts (bounding-sphere transform + frustum cull loop)
 *          EU 0x00205728 | US 0x00203788 | JP 0x00204078
 * ========================================================================= */
static inline int32_t cv_test_sphere_6planes(uint32_t cam, float sx, float sy, float sz, float sr, int32_t mask)
{
    const uint32_t planes = read_be32_fast(vm_base + cam + 0xCE0u) & ~0xFu;
    int32_t out = mask;
    for (uint32_t i = 0; i < 6u; ++i) {
        const int32_t bit = 1 << i;
        if (mask & bit) continue;
        const uint8_t* p = vm_base + planes + i * 16u;
        const float dot = read_bef32_fast(p + 0x0u) * sx
                        + read_bef32_fast(p + 0x4u) * sy
                        + read_bef32_fast(p + 0x8u) * sz
                        + read_bef32_fast(p + 0xCu);
        if (dot < -sr) return -1;
        if (dot >= sr) out |= bit;
    }
    return out;
}

void CV_FD_FN_TEST_SPHERE(ppu_context* ctx)
{
    if (!native_hot_enabled() || !vm_base) {
        CV_FD_ORIG_TEST_SPHERE(ctx);
        return;
    }
    const uint32_t cam = (uint32_t)ctx->gpr[3];
    const uint8_t* sp  = vm_base + (uint32_t)ctx->gpr[4];
    const int32_t  msk = (int32_t)ctx->gpr[5];
    const float sx = read_bef32_fast(sp + 0x0u);
    const float sy = read_bef32_fast(sp + 0x4u);
    const float sz = read_bef32_fast(sp + 0x8u);
    const float sr = read_bef32_fast(sp + 0xCu);
    ctx->gpr[3] = (int64_t)cv_test_sphere_6planes(cam, sx, sy, sz, sr, msk);
}

static inline void cv_mat4_mul_vec4_inline(const uint8_t* m, float vx, float vy, float vz, float vw,
                                           float& ox, float& oy, float& oz, float& ow)
{
    ox = read_bef32_fast(m + 0x00u) * vx + read_bef32_fast(m + 0x04u) * vy + read_bef32_fast(m + 0x08u) * vz + read_bef32_fast(m + 0x0Cu) * vw;
    oy = read_bef32_fast(m + 0x10u) * vx + read_bef32_fast(m + 0x14u) * vy + read_bef32_fast(m + 0x18u) * vz + read_bef32_fast(m + 0x1Cu) * vw;
    oz = read_bef32_fast(m + 0x20u) * vx + read_bef32_fast(m + 0x24u) * vy + read_bef32_fast(m + 0x28u) * vz + read_bef32_fast(m + 0x2Cu) * vw;
    ow = read_bef32_fast(m + 0x30u) * vx + read_bef32_fast(m + 0x34u) * vy + read_bef32_fast(m + 0x38u) * vz + read_bef32_fast(m + 0x3Cu) * vw;
}

void CV_FD_FN_MAT4_VEC4(ppu_context* ctx)
{
    if (!native_hot_enabled() || !vm_base) {
        CV_FD_ORIG_MAT4_VEC4(ctx);
        return;
    }
    uint8_t*       dst = vm_base + ((uint32_t)ctx->gpr[3] & ~0xFu);
    const uint8_t* v   = vm_base + (uint32_t)ctx->gpr[5];
    const uint8_t* m   = vm_base + (uint32_t)ctx->gpr[6];
    const float vx = read_bef32_fast(v + 0x0u);
    const float vy = read_bef32_fast(v + 0x4u);
    const float vz = read_bef32_fast(v + 0x8u);
    const float vw = read_bef32_fast(v + 0xCu);
    float ox, oy, oz, ow;
    cv_mat4_mul_vec4_inline(m, vx, vy, vz, vw, ox, oy, oz, ow);
    write_bef32_fast(dst + 0x0u, ox);
    write_bef32_fast(dst + 0x4u, oy);
    write_bef32_fast(dst + 0x8u, oz);
    write_bef32_fast(dst + 0xCu, ow);
}

void CV_FD_FN_CULL_PARTS(ppu_context* ctx)
{
    if (!native_hot_enabled() || !vm_base) {
        CV_FD_ORIG_CULL_PARTS(ctx);
        return;
    }
    const uint32_t model = (uint32_t)ctx->gpr[3];
    const uint32_t cam   = (uint32_t)ctx->gpr[4];
    const uint32_t grp   = (uint32_t)ctx->gpr[5];
    const int32_t  mask  = (int32_t)ctx->gpr[6];
    if (mask == 0x3F) {
        ctx->gpr[3] = 0x3F;
        return;
    }
    const uint32_t count = read_be16_fast(vm_base + grp + 0x28u);
    if (count == 0u) {
        ctx->gpr[3] = (int64_t)-1;
        return;
    }
    const uint32_t parts_arr = read_be32_fast(vm_base + grp + 0x30u);
    const uint32_t bone_base = read_be32_fast(vm_base + model + 0x274u);
    const bool     has_xform = (vm_base[model + 0x2A5u] != 0u);
    const uint8_t* xform_m   = vm_base + model + 0x230u;
    int32_t acc = -1;

    for (uint32_t i = 0; i < count; ++i) {
        const uint32_t part = parts_arr + i * 0x90u;
        const uint32_t bone = read_be32_fast(vm_base + part);
        const uint32_t mat_addr = (bone == 0xFFu)
            ? (model + 0xB0u)
            : (bone_base + bone * 0xC0u + 0x10u);
        const uint8_t* m = vm_base + (mat_addr & ~0xFu);

        const float m00 = read_bef32_fast(m + 0x00u), m01 = read_bef32_fast(m + 0x04u), m02 = read_bef32_fast(m + 0x08u), m03 = read_bef32_fast(m + 0x0Cu);
        const float m10 = read_bef32_fast(m + 0x10u), m11 = read_bef32_fast(m + 0x14u), m12 = read_bef32_fast(m + 0x18u), m13 = read_bef32_fast(m + 0x1Cu);
        const float m20 = read_bef32_fast(m + 0x20u), m21 = read_bef32_fast(m + 0x24u), m22 = read_bef32_fast(m + 0x28u), m23 = read_bef32_fast(m + 0x2Cu);
        const float m30 = read_bef32_fast(m + 0x30u), m31 = read_bef32_fast(m + 0x34u), m32 = read_bef32_fast(m + 0x38u);

        const uint8_t* sp = vm_base + ((part + 0x10u) & ~0xFu);
        const float sx = read_bef32_fast(sp + 0x0u);
        const float sy = read_bef32_fast(sp + 0x4u);
        const float sz = read_bef32_fast(sp + 0x8u);
        const float sr = read_bef32_fast(sp + 0xCu);

        float wx = m00 * sx + m10 * sy + m20 * sz + m30;
        float wy = m01 * sx + m11 * sy + m21 * sz + m31;
        float wz = m02 * sx + m12 * sy + m22 * sz + m32;

        const float l0 = m00 * m00 + m01 * m01 + m02 * m02 + m03 * m03;
        const float l1 = m10 * m10 + m11 * m11 + m12 * m12 + m13 * m13;
        const float l2 = m20 * m20 + m21 * m21 + m22 * m22 + m23 * m23;
        float max_l = l0 > l1 ? l0 : l1;
        if (l2 > max_l) max_l = l2;
        const float wr = (max_l > 0.0f) ? (sr * std::sqrt(max_l)) : 0.0f;

        if (has_xform) {
            float tx, ty, tz, tw;
            cv_mat4_mul_vec4_inline(xform_m, wx, -wy, -wz, 1.0f, tx, ty, tz, tw);
            wx = tx; wy = ty; wz = tz;
        }

        acc &= cv_test_sphere_6planes(cam, wx, wy, wz, wr, mask);
        if (acc == 0) break;
    }
    ctx->gpr[3] = (int64_t)acc;
}

/* Hook 20: Translucent/effect draw command enqueue fast path (EU/US/JP 0x00015DDC).
 * Twin of cv_fast_enqueue_00015AFC: inlines all 7-11 global getter/setter helper calls
 * (func_001C5Fxx) and pushes directly into the target vector when cnt < cap. */
void CV_FD_FN_ENQUEUE_TRANS(ppu_context* ctx)
{
    if (!native_hot_enabled() || !vm_base) {
        CV_FD_ORIG_ENQUEUE_TRANS(ctx);
        return;
    }

    const uint32_t obj   = (uint32_t)ctx->gpr[3];
    const uint32_t item  = (uint32_t)ctx->gpr[4];
    const uint32_t mgr   = read_be32_fast(vm_base + kEnqMgrPtr);
    const uint32_t val44 = read_be32_fast(vm_base + mgr + 0x154u);
    const int32_t  mode  = (int32_t)read_be32_fast(vm_base + obj + 0x14Cu);

    uint32_t off_cnt, off_cap, off_arr;
    bool copy_vec = false;
    if (mode == 3) {
        off_cnt = 0x13Cu; off_cap = 0x140u; off_arr = 0x148u;
    } else if (mode == 4) {
        off_cnt = 0x128u; off_cap = 0x12Cu; off_arr = 0x134u;
    } else {
        const uint32_t hi = val44 & 0xFFFF0000u;
        if (hi == 0x08000000u || hi == 0x04000000u || hi == 0x00400000u) {
            off_cnt = 0x114u; off_cap = 0x118u; off_arr = 0x120u;
        } else if (hi == 0x80000000u) {
            off_cnt = 0x114u; off_cap = 0x118u; off_arr = 0x120u;
            copy_vec = (vm_base[kEnqG2 + 8u] != 0u);
        } else {
            off_cnt = 0x100u; off_cap = 0x104u; off_arr = 0x10Cu;
        }
    }

    const uint32_t cnt = read_be32_fast(vm_base + obj + off_cnt);
    const uint32_t cap = read_be32_fast(vm_base + obj + off_cap);
    if (cnt >= cap) {
        CV_FD_ORIG_ENQUEUE_TRANS(ctx);
        return;
    }

    write_be32_fast(vm_base + item + 0x44u, val44);
    write_be32_fast(vm_base + item + 0x48u, read_be32_fast(vm_base + kEnqG0 + 4u));
    write_be32_fast(vm_base + item + 0x4Cu, read_be32_fast(vm_base + kEnqG2));
    write_be32_fast(vm_base + item + 0x50u, read_be32_fast(vm_base + kEnqG2 + 4u));
    if (copy_vec) {
        std::memcpy(vm_base + item + 0x34u, vm_base + kEnqG2 + 12u, 12u);
        write_be32_fast(vm_base + item + 0x40u, 0x3F800000u);
    }

    const uint32_t arr = read_be32_fast(vm_base + obj + off_arr);
    write_be32_fast(vm_base + obj + off_cnt, cnt + 1u);
    write_be32_fast(vm_base + arr + (cnt << 2), item);

    write_be32_fast(vm_base + mgr + 0x154u, val44 & 0xFFFF0000u);
    write_be32_fast(vm_base + kEnqG0,      1u);
    write_be32_fast(vm_base + kEnqG0 + 4u, 0xFFFFFFFFu);
    write_be32_fast(vm_base + kEnqG2,      0u);
    write_be32_fast(vm_base + kEnqG2 + 4u, 0u);
}

/* Hook 21: MtHeapAllocator first-fit free-list alloc fast path
 * (EU 0x006DAB0C / US 0x006D8B4C / JP 0x006D943C).
 * Replaces the lifted PPC free-list scan + split + lwarx/stwcx stats update
 * with native x64 pointer walking and lock-free atomic_ref stats updates.
 * Falls back to original on slow path (no free block fits). */
void CV_FD_FN_HEAP_ALLOC_INNER(ppu_context* ctx)
{
    if (!native_hot_enabled() || !vm_base) {
        CV_FD_ORIG_HEAP_ALLOC_INNER(ctx);
        return;
    }

    const uint32_t alloc_obj = (uint32_t)ctx->gpr[3];
    const uint32_t pool      = (uint32_t)ctx->gpr[4];
    const uint32_t req_bytes = (uint32_t)ctx->gpr[5];
    const uint32_t need      = (req_bytes >> 4) + 3u;

    const uint32_t head = read_be32_fast(vm_base + pool + 0x50u);
    uint32_t node = head;
    while (node != 0u) {
        const uint32_t w20 = read_be32_fast(vm_base + node + 0x20u);
        if (((w20 >> 4) & 0x07FFFFFFu) >= need) break;
        node = read_be32_fast(vm_base + node + 0x14u);
    }
    if (node == 0u) {
        CV_FD_ORIG_HEAP_ALLOC_INNER(ctx);
        return;
    }

    uint32_t w20 = read_be32_fast(vm_base + node + 0x20u);
    const uint32_t blk_units = (w20 >> 4) & 0x07FFFFFFu;
    const uint32_t blk_type  = (w20 >> 2) & 0x3u;
    const uint32_t rem       = blk_units - need;

    /* Unlink node from free list if free count != 0 */
    const uint32_t free_cnt = read_be32_fast(vm_base + pool + 0x58u);
    if (free_cnt != 0u) {
        const uint32_t prev = read_be32_fast(vm_base + node + 0x10u);
        const uint32_t next = read_be32_fast(vm_base + node + 0x14u);
        if (node == head) {
            write_be32_fast(vm_base + pool + 0x50u, next);
        }
        if (node == read_be32_fast(vm_base + pool + 0x54u)) {
            write_be32_fast(vm_base + pool + 0x54u, prev);
        }
        if (prev != 0u) {
            write_be32_fast(vm_base + prev + 0x14u, next);
        }
        if (next != 0u) {
            write_be32_fast(vm_base + next + 0x10u, prev);
        }
        const uint32_t free_units = read_be32_fast(vm_base + pool + 0x5Cu);
        write_be32_fast(vm_base + pool + 0x5Cu, free_units - blk_units);
        write_be32_fast(vm_base + pool + 0x58u, free_cnt - 1u);
    }

    /* Split remainder back into free list if rem > 3 units */
    if (rem > 3u) {
        const uint32_t split      = node + ((need << 4) & 0xFFFFFFF0u);
        const uint32_t phys_next  = read_be32_fast(vm_base + node + 0x18u);
        const uint32_t owner_hdr  = read_be32_fast(vm_base + node + 0x04u);

        std::memcpy(vm_base + split, vm_base + node, 48u);
        const uint32_t split_w20 = (w20 & ~0x7FFFFFF0u) | ((rem << 4) & 0x7FFFFFF0u);
        write_be32_fast(vm_base + split + 0x20u, split_w20);
        write_be32_fast(vm_base + node  + 0x18u, split);
        write_be32_fast(vm_base + split + 0x1Cu, node);
        write_be32_fast(vm_base + split + 0x18u, phys_next);
        write_be32_fast(vm_base + split + 0x04u, owner_hdr);
        if (phys_next != 0u) {
            write_be32_fast(vm_base + phys_next + 0x1Cu, split);
        }

        if (blk_type == 0u) {
            const uint32_t ftail = read_be32_fast(vm_base + pool + 0x54u);
            if (ftail != 0u) {
                write_be32_fast(vm_base + pool  + 0x54u, split);
                write_be32_fast(vm_base + split + 0x10u, ftail);
                write_be32_fast(vm_base + split + 0x14u, 0u);
                write_be32_fast(vm_base + ftail + 0x14u, split);
            } else {
                write_be32_fast(vm_base + pool  + 0x50u, split);
                write_be32_fast(vm_base + pool  + 0x54u, split);
                write_be32_fast(vm_base + split + 0x14u, 0u);
                write_be32_fast(vm_base + split + 0x10u, 0u);
            }
        } else {
            const uint32_t fhead = read_be32_fast(vm_base + pool + 0x50u);
            if (fhead != 0u) {
                write_be32_fast(vm_base + pool  + 0x50u, split);
                write_be32_fast(vm_base + split + 0x10u, 0u);
                write_be32_fast(vm_base + split + 0x14u, fhead);
                write_be32_fast(vm_base + fhead + 0x10u, split);
            } else {
                write_be32_fast(vm_base + pool  + 0x50u, split);
                write_be32_fast(vm_base + pool  + 0x54u, split);
                write_be32_fast(vm_base + split + 0x14u, 0u);
                write_be32_fast(vm_base + split + 0x10u, 0u);
            }
        }

        const uint32_t fc2 = read_be32_fast(vm_base + pool + 0x58u);
        const uint32_t fu2 = read_be32_fast(vm_base + pool + 0x5Cu);
        write_be32_fast(vm_base + pool + 0x58u, fc2 + 1u);
        write_be32_fast(vm_base + pool + 0x5Cu, fu2 + rem);

        w20 = (w20 & ~0x7FFFFFF0u) | ((need << 4) & 0x7FFFFFF0u);
        write_be32_fast(vm_base + node + 0x20u, w20);
    }

    /* Mark node allocated and append to pool's used list */
    const uint32_t alloc_bytes = w20 & 0x7FFFFFF0u;
    const uint32_t new_w20     = ((w20 | 0x80000000u) & ~0x0Cu) | 0x04u;
    const uint32_t old_w24     = read_be32_fast(vm_base + node + 0x24u);
    const uint32_t new_w24     = (old_w24 & 0x03u)
                               | ((read_be32_fast(vm_base + alloc_obj + 0x58u) << 2) & 0xFCu)
                               | ((read_be32_fast(vm_base + pool + 0x60u) << 27) & 0xF8000000u);
    write_be32_fast(vm_base + node + 0x20u, new_w20);
    write_be32_fast(vm_base + node + 0x24u, new_w24);

    const uint32_t atail = read_be32_fast(vm_base + pool + 0x44u);
    if (atail != 0u) {
        write_be32_fast(vm_base + pool  + 0x44u, node);
        write_be32_fast(vm_base + node  + 0x10u, atail);
        write_be32_fast(vm_base + node  + 0x14u, 0u);
        write_be32_fast(vm_base + atail + 0x14u, node);
    } else {
        write_be32_fast(vm_base + pool + 0x40u, node);
        write_be32_fast(vm_base + pool + 0x44u, node);
        write_be32_fast(vm_base + node + 0x14u, 0u);
        write_be32_fast(vm_base + node + 0x10u, 0u);
    }

    const uint32_t used_units = (new_w20 >> 4) & 0x07FFFFFFu;
    write_be32_fast(vm_base + pool + 0x4Cu, read_be32_fast(vm_base + pool + 0x4Cu) + used_units);
    write_be32_fast(vm_base + pool + 0x48u, read_be32_fast(vm_base + pool + 0x48u) + 1u);

    const uint32_t chunk_hdr = read_be32_fast(vm_base + node + 0x04u);
    write_be32_fast(vm_base + chunk_hdr + 0x08u, read_be32_fast(vm_base + chunk_hdr + 0x08u) + 1u);
    write_be32_fast(vm_base + pool + 0x68u, read_be32_fast(vm_base + pool + 0x68u) + alloc_bytes);

    /* Lock-free atomic stats update (replaces ppu_stwcx32 coherency-locked loops) */
    std::atomic_ref<uint32_t> tot_atom(*reinterpret_cast<uint32_t*>(vm_base + alloc_obj + 0x04u));
    uint32_t tot_be = tot_atom.load(std::memory_order_relaxed);
    uint32_t old_tot;
    for (;;) {
        old_tot = bswap32_fast(tot_be);
        const uint32_t next_be = bswap32_fast(old_tot + alloc_bytes);
        if (tot_atom.compare_exchange_weak(tot_be, next_be,
                                           std::memory_order_acq_rel,
                                           std::memory_order_relaxed)) {
            break;
        }
    }

    std::atomic_ref<uint32_t> peak_atom(*reinterpret_cast<uint32_t*>(vm_base + alloc_obj + 0x08u));
    uint32_t peak_be = peak_atom.load(std::memory_order_relaxed);
    for (;;) {
        const uint32_t cur_peak = bswap32_fast(peak_be);
        if (cur_peak >= old_tot) break;
        if (peak_atom.compare_exchange_weak(peak_be, bswap32_fast(old_tot),
                                            std::memory_order_acq_rel,
                                            std::memory_order_relaxed)) {
            break;
        }
    }

    ctx->gpr[3] = node;
}

/* Shared inline helper for Hooks 20, 25, 26: translucent draw command enqueue */
static inline void cv_enqueue_trans_inline(ppu_context* ctx, uint32_t obj, uint32_t item)
{
    const uint32_t mgr   = read_be32_fast(vm_base + kEnqMgrPtr);
    const uint32_t val44 = read_be32_fast(vm_base + mgr + 0x154u);
    const int32_t  mode  = (int32_t)read_be32_fast(vm_base + obj + 0x14Cu);

    uint32_t off_cnt, off_cap, off_arr;
    bool copy_vec = false;
    if (mode == 3) {
        off_cnt = 0x13Cu; off_cap = 0x140u; off_arr = 0x148u;
    } else if (mode == 4) {
        off_cnt = 0x128u; off_cap = 0x12Cu; off_arr = 0x134u;
    } else {
        const uint32_t hi = val44 & 0xFFFF0000u;
        if (hi == 0x08000000u || hi == 0x04000000u || hi == 0x00400000u) {
            off_cnt = 0x114u; off_cap = 0x118u; off_arr = 0x120u;
        } else if (hi == 0x80000000u) {
            off_cnt = 0x114u; off_cap = 0x118u; off_arr = 0x120u;
            copy_vec = (vm_base[kEnqG2 + 8u] != 0u);
        } else {
            off_cnt = 0x100u; off_cap = 0x104u; off_arr = 0x10Cu;
        }
    }

    const uint32_t cnt = read_be32_fast(vm_base + obj + off_cnt);
    const uint32_t cap = read_be32_fast(vm_base + obj + off_cap);
    if (cnt >= cap) {
        const uint64_t s3 = ctx->gpr[3], s4 = ctx->gpr[4];
        ctx->gpr[3] = obj;
        ctx->gpr[4] = item;
        CV_FD_ORIG_ENQUEUE_TRANS(ctx);
        ctx->gpr[3] = s3;
        ctx->gpr[4] = s4;
        return;
    }

    write_be32_fast(vm_base + item + 0x44u, val44);
    write_be32_fast(vm_base + item + 0x48u, read_be32_fast(vm_base + kEnqG0 + 4u));
    write_be32_fast(vm_base + item + 0x4Cu, read_be32_fast(vm_base + kEnqG2));
    write_be32_fast(vm_base + item + 0x50u, read_be32_fast(vm_base + kEnqG2 + 4u));
    if (copy_vec) {
        std::memcpy(vm_base + item + 0x34u, vm_base + kEnqG2 + 12u, 12u);
        write_be32_fast(vm_base + item + 0x40u, 0x3F800000u);
    }

    const uint32_t arr = read_be32_fast(vm_base + obj + off_arr);
    write_be32_fast(vm_base + obj + off_cnt, cnt + 1u);
    write_be32_fast(vm_base + arr + (cnt << 2), item);

    write_be32_fast(vm_base + mgr + 0x154u, val44 & 0xFFFF0000u);
    write_be32_fast(vm_base + kEnqG0,      1u);
    write_be32_fast(vm_base + kEnqG0 + 4u, 0xFFFFFFFFu);
    write_be32_fast(vm_base + kEnqG2,      0u);
    write_be32_fast(vm_base + kEnqG2 + 4u, 0u);
}

/* Hook 22: Affine 4x3 matrix * Vec3[count] transform (EU 0x001CE5E0 / US 0x001CC7C0 / JP 0x001CC940) */
void CV_FD_FN_XFORM_VEC3(ppu_context* ctx)
{
    if (!native_hot_enabled() || !vm_base) {
        CV_FD_ORIG_XFORM_VEC3(ctx);
        return;
    }

    uint32_t       mat   = (uint32_t)ctx->gpr[3];
    const uint32_t src   = (uint32_t)ctx->gpr[4];
    const uint32_t dst   = (uint32_t)ctx->gpr[5];
    const uint32_t count = (uint32_t)ctx->gpr[6];
    if (count == 0u) return;
    if (mat == 0u) mat = read_be32_fast(vm_base + kMatPtr);

    const uint8_t* mp = vm_base + mat;
    const float m00 = read_bef32_fast(mp + 0x00u), m01 = read_bef32_fast(mp + 0x04u), m02 = read_bef32_fast(mp + 0x08u);
    const float m10 = read_bef32_fast(mp + 0x10u), m11 = read_bef32_fast(mp + 0x14u), m12 = read_bef32_fast(mp + 0x18u);
    const float m20 = read_bef32_fast(mp + 0x20u), m21 = read_bef32_fast(mp + 0x24u), m22 = read_bef32_fast(mp + 0x28u);
    const float m30 = read_bef32_fast(mp + 0x30u), m31 = read_bef32_fast(mp + 0x34u), m32 = read_bef32_fast(mp + 0x38u);

    const uint8_t* sp = vm_base + src;
    uint8_t*       dp = vm_base + dst;
    for (uint32_t i = 0; i < count; ++i, sp += 12u, dp += 12u) {
        const float x = read_bef32_fast(sp + 0x0u);
        const float y = read_bef32_fast(sp + 0x4u);
        const float z = read_bef32_fast(sp + 0x8u);
        const float ox = m00 * x + m10 * y + m20 * z + m30;
        const float oy = m01 * x + m11 * y + m21 * z + m31;
        const float oz = m02 * x + m12 * y + m22 * z + m32;
        write_bef32_fast(dp + 0x0u, ox);
        write_bef32_fast(dp + 0x4u, oy);
        write_bef32_fast(dp + 0x8u, oz);
    }
}

/* Hook 23: Translucent depth-sort 8-byte merge step (EU 0x003DDA78 / US 0x003DBAB8 / JP 0x003DC3A8) */
void CV_FD_FN_DEPTH_MERGE(ppu_context* ctx)
{
    if (!native_hot_enabled() || !vm_base) {
        CV_FD_ORIG_DEPTH_MERGE(ctx);
        return;
    }

    const uint32_t arr_addr = (uint32_t)ctx->gpr[4];
    const uint32_t tmp_addr = (uint32_t)ctx->gpr[5];
    const int32_t  lo       = (int32_t)ctx->gpr[6];
    const int32_t  mid      = (int32_t)ctx->gpr[7];
    const int32_t  hi       = (int32_t)ctx->gpr[8];
    if (lo < 0 || lo >= mid || mid >= hi) {
        CV_FD_ORIG_DEPTH_MERGE(ctx);
        return;
    }

    uint64_t* const arr = reinterpret_cast<uint64_t*>(vm_base + arr_addr);
    uint64_t* const tmp = reinterpret_cast<uint64_t*>(vm_base + tmp_addr);
    std::memcpy(tmp + lo, arr + lo, (size_t)(hi - lo) * 8u);

    int32_t i = lo, j = mid, k = lo;
    const uint8_t* const tmp_b = vm_base + tmp_addr;
    for (;;) {
        const uint32_t key_j = read_be32_fast(tmp_b + (size_t)j * 8u + 4u);
        const uint32_t key_i = read_be32_fast(tmp_b + (size_t)i * 8u + 4u);
        if (key_j > key_i) {
            arr[k++] = tmp[j++];
            if (j == hi) break;
        } else {
            arr[k++] = tmp[i++];
            if (i == mid) { i = j; break; }
        }
    }
    if (k < hi) {
        std::memcpy(arr + k, tmp + i, (size_t)(hi - k) * 8u);
    }
}

/* Hook 24: 5x u32 vertex-format/state descriptor lookup (EU 0x007C0498 / US 0x007BE4D8 / JP 0x007BEDC8) */
void CV_FD_FN_DESC_LOOKUP(ppu_context* ctx)
{
    if (!native_hot_enabled() || !vm_base) {
        CV_FD_ORIG_DESC_LOOKUP(ctx);
        return;
    }

    const uint32_t obj = (uint32_t)ctx->gpr[3];
    const uint32_t a   = (uint32_t)ctx->gpr[4];
    const uint32_t b   = (uint32_t)ctx->gpr[5];
    const uint32_t c   = (uint32_t)ctx->gpr[6];
    const uint32_t d   = (uint32_t)ctx->gpr[7];

    uint32_t flags = (b != 0u) ? 5u : 1u;
    if (c != 0u) flags |= 8u;
    if (d != 0u) flags |= 16u;

    const uint32_t key_be[5] = {
        bswap32_fast(flags), bswap32_fast(a), bswap32_fast(b),
        bswap32_fast(c),     bswap32_fast(d)
    };
    std::memcpy(vm_base + obj + 0x74u, key_be, 20u);

    const uint32_t arr     = read_be32_fast(vm_base + obj + 0x24Cu);
    const uint32_t count   = read_be32_fast(vm_base + obj + 0x250u);
    const uint32_t base_id = read_be32_fast(vm_base + obj + 0x258u);

    uint32_t result = 0u;
    if (count > 0u) {
        const uint8_t* p = vm_base + arr;
        for (uint32_t i = 0; i < count; ++i, p += 20u) {
            if (std::memcmp(p, key_be, 20u) == 0) {
                result = base_id + i;
                break;
            }
        }
    }

    if (result == 0u) {
        const uint32_t cap = read_be32_fast(vm_base + obj + 0x254u);
        if (count >= cap) {
            CV_FD_ORIG_DESC_LOOKUP(ctx);
            return;
        }
        std::memcpy(vm_base + arr + count * 20u, key_be, 20u);
        write_be32_fast(vm_base + obj + 0x250u, count + 1u);
        result = base_id + count;
    }

    write_be32_fast(vm_base + obj + 0x94u, result);
    ctx->gpr[3] = result;
}

/* Shared helper for Hooks 25 & 26: build the 16-byte Altivec blend constant */
static inline void cv_build_draw_vec80(uint32_t const_a_addr, uint8_t out_vec[16])
{
    uint8_t splat[16];
    const uint8_t* ca = vm_base + const_a_addr;
    for (int k = 0; k < 4; ++k) std::memcpy(splat + k * 4, ca, 4);
    const uint8_t* vb   = vm_base + kSpriteVecB;
    const uint8_t* mask = vm_base + const_a_addr + 0x10u;
    for (int k = 0; k < 16; ++k) {
        out_vec[k] = (uint8_t)((vb[k] & ~mask[k]) | (splat[k] & mask[k]));
    }
}

/* Hook 25: Billboard/sprite particle array emitter (EU 0x001D4C80 / US 0x001D2E60 / JP 0x001D2FE0)
 * #1 hottest function in Nosferatu snow+gas+gunfire (~14% of ppu:main). */
void CV_FD_FN_EMIT_SPRITES(ppu_context* ctx)
{
    if (!native_hot_enabled() || !vm_base) {
        CV_FD_ORIG_EMIT_SPRITES(ctx);
        return;
    }

    const uint32_t pos_arr = (uint32_t)ctx->gpr[3];
    const int32_t  count   = (int32_t)ctx->gpr[4];
    const float    half_w  = (float)ctx->fpr[1];
    const float    half_h  = (float)ctx->fpr[2];

    const uint32_t order_flag = read_be32_fast(vm_base + kEnqG0);
    const uint32_t blend_mode = read_be32_fast(vm_base + kEnqG0 + 4u);
    if (count <= 0) return;
    if (order_flag > 1u) {
        CV_FD_ORIG_EMIT_SPRITES(ctx);
        return;
    }

    const uint32_t col_w = read_be32_fast(vm_base + kSpriteColor);
    const uint32_t c_r   = ((col_w >> 16) & 0xFFu) + 1u;
    const uint32_t c_g   = ((col_w >> 8)  & 0xFFu) + 1u;
    const uint32_t c_b   = ( col_w        & 0xFFu) + 1u;
    const uint32_t c_a   = ((col_w >> 24) & 0xFFu) + 1u;
    const uint32_t packed_color =
          (((c_a >> 1) & 0xFFu) << 24)
        | (((c_b >> 1) & 0xFFu) << 16)
        | (((c_g >> 1) & 0xFFu) << 8)
        | ( (c_r >> 1) & 0xFFu);

    uint8_t vec_80[16];
    cv_build_draw_vec80(kSpriteConstA, vec_80);
    uint8_t vec_f0[16];
    std::memcpy(vec_f0, vm_base + kSpriteVecC, 16u);

    const uint32_t mat_addr = read_be32_fast(vm_base + kMatPtr);
    const uint8_t* mp       = vm_base + mat_addr;
    const float m00 = read_bef32_fast(mp + 0x00u), m01 = read_bef32_fast(mp + 0x04u), m02 = read_bef32_fast(mp + 0x08u);
    const float m10 = read_bef32_fast(mp + 0x10u), m11 = read_bef32_fast(mp + 0x14u), m12 = read_bef32_fast(mp + 0x18u);
    const float m20 = read_bef32_fast(mp + 0x20u), m21 = read_bef32_fast(mp + 0x24u), m22 = read_bef32_fast(mp + 0x28u);
    const float m30 = read_bef32_fast(mp + 0x30u), m31 = read_bef32_fast(mp + 0x34u), m32 = read_bef32_fast(mp + 0x38u);

    const uint32_t uv_u[4] = {
        read_be32_fast(vm_base + kSpriteUvTpl + 0x08u),
        read_be32_fast(vm_base + kSpriteUvTpl + 0x18u),
        read_be32_fast(vm_base + kSpriteUvTpl + 0x28u),
        read_be32_fast(vm_base + kSpriteUvTpl + 0x38u)
    };
    const uint32_t uv_v[4] = {
        read_be32_fast(vm_base + kSpriteUvTpl + 0x0Cu),
        read_be32_fast(vm_base + kSpriteUvTpl + 0x1Cu),
        read_be32_fast(vm_base + kSpriteUvTpl + 0x2Cu),
        read_be32_fast(vm_base + kSpriteUvTpl + 0x3Cu)
    };
    const float dx[4] = { -half_w, +half_w, -half_w, +half_w };
    const float dy[4] = { -half_h, -half_h, +half_h, +half_h };
    static constexpr uint32_t kIdxLut[2][4] = { { 0u, 2u, 1u, 3u }, { 0u, 1u, 2u, 3u } };
    const uint32_t* const idx_row = kIdxLut[order_flag];

    const uint32_t alloc_off  = (read_be32_fast(vm_base + kAllocDesc + 0x18u) >> 1) & 0xFCu;
    const uint32_t alloc_inst = read_be32_fast(vm_base + kAllocObj + alloc_off);
    if (!alloc_inst) {
        CV_FD_ORIG_EMIT_SPRITES(ctx);
        return;
    }
    const uint32_t alloc_vt   = read_be32_fast(vm_base + alloc_inst);
    const uint32_t alloc_fn   = read_be32_fast(vm_base + read_be32_fast(vm_base + alloc_vt + 0x1Cu));

    const uint32_t enq_mgr   = read_be32_fast(vm_base + kEnqMgrPtr);
    const uint32_t mat_state = read_be32_fast(vm_base + kMatStatePtr);
    const uint32_t tex_id    = read_be32_fast(vm_base + kTexIdAddr);
    const uint8_t  b1f       = vm_base[kByte3654];
    const uint8_t  b20       = vm_base[kByte3654 + 1u];

    for (int32_t i = 0; i < count; ++i) {
        write_be32_fast(vm_base + kEnqG0 + 4u, blend_mode);

        const uint32_t obj = cv_alloc_scratch_000124F4(0u, 0x100u);
        if (!obj) break;

        uint8_t* const op = vm_base + obj;
        write_be32_fast(op + 0x00u, kObjVtable);
        std::memset(op + 0x04u, 0, 0x18u);
        write_be32_fast(op + 0x1Cu, kObjMeta);
        write_be32_fast(op + 0x20u, 0u);
        write_be32_fast(op + 0x24u, 0u);
        op[0x28u] = 0u;
        write_be32_fast(op + 0x2Cu, 0u);
        write_be32_fast(op + 0x30u, mat_state);
        write_be32_fast(op + 0x44u, 0u);
        write_be32_fast(op + 0x54u, 0u);
        write_be32_fast(op + 0x58u, 0u);
        write_be32_fast(op + 0x5Cu, 1u);
        write_be32_fast(op + 0x60u, 1u);
        write_be32_fast(op + 0x64u, 0u);
        const uint32_t w68 = (read_be32_fast(op + 0x68u) & 0x0000FFFFu) | 0x0FE10000u;
        write_be32_fast(op + 0x68u, w68);
        write_be32_fast(op + 0x6Cu, 0u);
        write_be32_fast(op + 0x70u, 0x3F800000u);
        std::memcpy(op + 0x80u, vec_80, 16u);
        std::memcpy(op + 0xE0u, vec_80, 16u);
        std::memcpy(op + 0xF0u, vec_f0, 16u);
        vm_base[kFlag3470] = 0u;

        const uint32_t verts = cv_alloc_scratch_000124F4(0u, 0x40u);
        const uint32_t cols  = cv_alloc_scratch_000124F4(0u, 0x10u);
        write_be32_fast(op + 0x04u, verts);
        write_be32_fast(op + 0x08u, cols);
        write_be32_fast(op + 0x10u, 4u);
        op[0x18u] = 1u;

        const uint32_t sub = cv_alloc_scratch_000124F4(0u, 0x4Cu);
        if (!sub) break;
        uint8_t* const sp = vm_base + sub;
        write_be32_fast(sp + 0x00u, kSubVtable);
        write_be32_fast(sp + 0x04u, 1u);
        const uint8_t obj_lo = (uint8_t)(obj & 0xFFu);
        sp[0x08u] = 0xFFu; sp[0x09u] = 0xFFu; sp[0x0Au] = 0xFFu; sp[0x0Bu] = obj_lo;
        sp[0x0Cu] = 0xFFu; sp[0x0Du] = 0xFFu; sp[0x0Eu] = 0xFFu; sp[0x0Fu] = obj_lo;
        sp[0x10u] = 0xFFu; sp[0x11u] = 0xFFu; sp[0x12u] = 0xFFu; sp[0x13u] = obj_lo;
        sp[0x1Cu] = 0u;    sp[0x1Du] = 0xFFu; sp[0x1Eu] = 0x10u;
        sp[0x1Fu] = b1f;   sp[0x20u] = b20;   sp[0x21u] = 0u;
        write_be32_fast(sp + 0x24u, 0u);
        write_be32_fast(sp + 0x2Cu, 0xFFFFFFFFu);
        write_be32_fast(sp + 0x30u, 0u);
        write_be32_fast(sp + 0x34u, 0u);
        write_be32_fast(sp + 0x38u, 0u);
        write_be32_fast(sp + 0x3Cu, 4u);
        write_be32_fast(sp + 0x40u, 0xFFFFFFFFu);
        write_be32_fast(sp + 0x44u, 0u);
        const uint32_t s48 = (read_be32_fast(sp + 0x48u) & ~0x20000000u) | 0xC0000000u;
        write_be32_fast(sp + 0x48u, s48);

        const uint32_t entry   = cv_alloc_scratch_000124F4(0u, 0x10u);
        const uint32_t idx_buf = cv_alloc_scratch_000124F4(0u, 0x10u);
        const uint32_t uv_buf  = cv_alloc_scratch_000124F4(0u, 0x30u);
        write_be32_fast(sp + 0x28u, entry);
        uint8_t* const ep = vm_base + entry;
        write_be32_fast(ep + 0x00u, 4u);
        ep[0x04u] = 1u;
        ep[0x05u] = 1u;
        write_be32_fast(ep + 0x08u, idx_buf);
        write_be32_fast(ep + 0x0Cu, uv_buf);

        const uint8_t* const pp = vm_base + pos_arr + (uint32_t)i * 12u;
        const float px = read_bef32_fast(pp + 0x0u);
        const float py = read_bef32_fast(pp + 0x4u);
        const float pz = read_bef32_fast(pp + 0x8u);

        /* Set light pos (func_001C6054) + white light color (func_001C5FB0) */
        vm_base[kEnqG2 + 8u] = 1u;
        std::memcpy(vm_base + kEnqG2 + 12u, pp, 12u);
        write_be32_fast(vm_base + kEnqG0 + 0x08u, 0x437F0000u);
        write_be32_fast(vm_base + kEnqG0 + 0x0Cu, 0x437F0000u);
        write_be32_fast(vm_base + kEnqG0 + 0x10u, 0x437F0000u);
        write_be32_fast(vm_base + kEnqG0 + 0x14u, 0x00000000u);

        const float tx = m00 * px + m10 * py + m20 * pz + m30;
        const float ty = m01 * px + m11 * py + m21 * pz + m31;
        const float tz = m02 * px + m12 * py + m22 * pz + m32;

        uint8_t* const vp = vm_base + verts;
        uint8_t* const cp = vm_base + cols;
        uint8_t* const ip = vm_base + idx_buf;
        uint8_t* const up = vm_base + uv_buf;
        for (int j = 0; j < 4; ++j) {
            write_bef32_fast(vp + j * 16 + 0, tx + dx[j]);
            write_bef32_fast(vp + j * 16 + 4, ty + dy[j]);
            write_bef32_fast(vp + j * 16 + 8, tz);
            write_be32_fast(cp + j * 4, packed_color);
            write_be32_fast(ip + j * 4, idx_row[j]);
            write_be32_fast(up + j * 12 + 0, uv_u[j]);
            write_be32_fast(up + j * 12 + 4, uv_v[j]);
            write_be32_fast(up + j * 12 + 8, tex_id);
        }

        uint32_t sub_arr = 0u;
        {
            GuestCall call(ctx);
            sub_arr = call.run_indirect_u32(alloc_fn, alloc_inst, 64u, 16u);
        }
        if (sub_arr) {
            std::memset(vm_base + sub_arr, 0, 64u);
            write_be32_fast(vm_base + sub_arr, sub);
            write_be32_fast(op + 0x2Cu, sub_arr);
            write_be32_fast(op + 0x24u, 16u);
            write_be32_fast(op + 0x20u, 1u);
        }

        cv_enqueue_trans_inline(ctx, enq_mgr, obj);
    }
}

/* Hook 26: Polygon/strip particle emitter (EU 0x001C8830 / US 0x001C6A10 / JP 0x001C6B90)
 * #2 hottest function in Nosferatu snow+gas+gunfire (~7.5% of ppu:main). */
void CV_FD_FN_EMIT_STRIP(ppu_context* ctx)
{
    if (!native_hot_enabled() || !vm_base) {
        CV_FD_ORIG_EMIT_STRIP(ctx);
        return;
    }

    const uint32_t vtx_in = (uint32_t)ctx->gpr[3];
    const int32_t  count  = (int32_t)ctx->gpr[4];
    const uint32_t mode   = (uint32_t)ctx->gpr[5];
    if (count < 0) {
        CV_FD_ORIG_EMIT_STRIP(ctx);
        return;
    }

    const uint32_t order_flag = read_be32_fast(vm_base + kEnqG0);
    if (count > 0 && order_flag > 1u) {
        CV_FD_ORIG_EMIT_STRIP(ctx);
        return;
    }

    const uint8_t mode_byte = (uint8_t)((mode == 1u) ? (mode | 0x18u) : ((mode | 0x10u) & ~8u));

    const uint32_t obj = cv_alloc_scratch_000124F4(0u, 0x100u);
    if (!obj) {
        CV_FD_ORIG_EMIT_STRIP(ctx);
        return;
    }

    uint8_t vec_80[16];
    cv_build_draw_vec80(kStripConstA, vec_80);

    uint8_t* const op = vm_base + obj;
    write_be32_fast(op + 0x00u, kObjVtable);
    std::memset(op + 0x04u, 0, 0x18u);
    write_be32_fast(op + 0x1Cu, kObjMeta);
    write_be32_fast(op + 0x20u, 0u);
    write_be32_fast(op + 0x24u, 0u);
    op[0x28u] = 0u;
    write_be32_fast(op + 0x2Cu, 0u);
    write_be32_fast(op + 0x30u, read_be32_fast(vm_base + kMatStatePtr));
    write_be32_fast(op + 0x44u, 0u);
    write_be32_fast(op + 0x54u, 0u);
    write_be32_fast(op + 0x58u, 0u);
    write_be32_fast(op + 0x5Cu, 1u);
    write_be32_fast(op + 0x60u, 1u);
    write_be32_fast(op + 0x64u, 0u);
    const uint32_t w68 = (read_be32_fast(op + 0x68u) & 0x0000FFFFu) | 0x0FE10000u;
    write_be32_fast(op + 0x68u, w68);
    write_be32_fast(op + 0x6Cu, 0u);
    write_be32_fast(op + 0x70u, 0x3F800000u);
    std::memcpy(op + 0x80u, vec_80, 16u);
    std::memcpy(op + 0xE0u, vec_80, 16u);
    std::memcpy(op + 0xF0u, vm_base + kSpriteVecC, 16u);
    vm_base[kFlag3470] = 0u;

    const uint32_t ucnt  = (uint32_t)count;
    const uint32_t verts = cv_alloc_scratch_000124F4(0u, ucnt << 4);
    const uint32_t cols  = cv_alloc_scratch_000124F4(0u, ucnt << 2);
    write_be32_fast(op + 0x04u, verts);
    write_be32_fast(op + 0x08u, cols);
    write_be32_fast(op + 0x10u, ucnt);
    op[0x18u] = 0u;
    if (ucnt > 0u) {
        const uint32_t def_col = read_be32_fast(vm_base + kDefCol);
        uint8_t* cp = vm_base + cols;
        for (uint32_t i = 0; i < ucnt; ++i) write_be32_fast(cp + i * 4u, def_col);
    }

    const uint32_t sub = cv_alloc_scratch_000124F4(0u, 0x4Cu);
    if (!sub) return;
    uint8_t* const sp = vm_base + sub;
    write_be32_fast(sp + 0x00u, kSubVtable);
    write_be32_fast(sp + 0x04u, 1u);
    const uint8_t pad_a = (uint8_t)(ctx->gpr[28] & 0xFFu);
    sp[0x08u] = 0xFFu; sp[0x09u] = 0xFFu; sp[0x0Au] = 0xFFu; sp[0x0Bu] = pad_a;
    sp[0x0Cu] = 0xFFu; sp[0x0Du] = 0xFFu; sp[0x0Eu] = 0xFFu; sp[0x0Fu] = pad_a;
    sp[0x10u] = 0xFFu; sp[0x11u] = 0xFFu; sp[0x12u] = 0xFFu; sp[0x13u] = pad_a;
    sp[0x1Cu] = mode_byte;
    sp[0x1Du] = 0xFFu; sp[0x1Eu] = 0x10u;
    sp[0x1Fu] = vm_base[kByte3654];
    sp[0x20u] = vm_base[kByte3654 + 1u];
    sp[0x21u] = 0u;
    write_be32_fast(sp + 0x24u, 0u);
    write_be32_fast(sp + 0x2Cu, 0xFFFFFFFFu);
    write_be32_fast(sp + 0x30u, 0u);
    write_be32_fast(sp + 0x34u, 0u);
    write_be32_fast(sp + 0x38u, 0u);
    write_be32_fast(sp + 0x3Cu, 4u);
    write_be32_fast(sp + 0x40u, 0xFFFFFFFFu);
    write_be32_fast(sp + 0x44u, 0u);
    const uint32_t s48 = (read_be32_fast(sp + 0x48u) & ~0x20000000u) | 0xC0000000u;
    write_be32_fast(sp + 0x48u, s48);

    const uint32_t entry   = cv_alloc_scratch_000124F4(0u, 0x10u);
    const uint32_t idx_buf = cv_alloc_scratch_000124F4(0u, ucnt << 2);
    const uint32_t uv_buf  = cv_alloc_scratch_000124F4(0u, ucnt * 12u);
    write_be32_fast(sp + 0x28u, entry);
    uint8_t* const ep = vm_base + entry;
    write_be32_fast(ep + 0x00u, ucnt);
    ep[0x04u] = 1u;
    ep[0x05u] = 0u;
    write_be32_fast(ep + 0x08u, idx_buf);
    write_be32_fast(ep + 0x0Cu, uv_buf);

    if (ucnt > 0u) {
        const uint32_t mat_addr = read_be32_fast(vm_base + kMatPtr);
        const uint8_t* mp       = vm_base + mat_addr;
        const float m00 = read_bef32_fast(mp + 0x00u), m01 = read_bef32_fast(mp + 0x04u), m02 = read_bef32_fast(mp + 0x08u);
        const float m10 = read_bef32_fast(mp + 0x10u), m11 = read_bef32_fast(mp + 0x14u), m12 = read_bef32_fast(mp + 0x18u);
        const float m20 = read_bef32_fast(mp + 0x20u), m21 = read_bef32_fast(mp + 0x24u), m22 = read_bef32_fast(mp + 0x28u);
        const float m30 = read_bef32_fast(mp + 0x30u), m31 = read_bef32_fast(mp + 0x34u), m32 = read_bef32_fast(mp + 0x38u);

        const uint32_t tex_id = read_be32_fast(vm_base + kTexIdAddr);
        static constexpr uint32_t kIdxLut[2][4] = { { 0u, 2u, 1u, 3u }, { 0u, 1u, 2u, 3u } };
        const uint32_t* const idx_row = kIdxLut[order_flag];

        const uint8_t* in_p = vm_base + vtx_in;
        uint8_t* const vp   = vm_base + verts;
        uint8_t* const cp   = vm_base + cols;
        uint8_t* const ip   = vm_base + idx_buf;
        uint8_t* const up   = vm_base + uv_buf;

        for (uint32_t i = 0; i < ucnt; ++i, in_p += 0x18u) {
            const uint32_t dst_idx = (i & ~3u) + idx_row[i & 3u];
            if (dst_idx < ucnt) {
                const float x = read_bef32_fast(in_p + 0x00u);
                const float y = read_bef32_fast(in_p + 0x04u);
                const float z = read_bef32_fast(in_p + 0x08u);
                const float ox = m00 * x + m10 * y + m20 * z + m30;
                const float oy = m01 * x + m11 * y + m21 * z + m31;
                const float oz = m02 * x + m12 * y + m22 * z + m32;
                write_bef32_fast(vp + dst_idx * 16u + 0u, ox);
                write_bef32_fast(vp + dst_idx * 16u + 4u, oy);
                write_bef32_fast(vp + dst_idx * 16u + 8u, oz);

                const uint32_t c = read_be32_fast(in_p + 0x14u);
                const uint32_t swiz = (c & 0xFF00FF00u) | ((c & 0x000000FFu) << 16) | ((c & 0x00FF0000u) >> 16);
                write_be32_fast(cp + dst_idx * 4u, swiz);
                op[0x18u] = 1u;

                write_be32_fast(ip + dst_idx * 4u, i);
                ep[0x05u] = 1u;
                write_be32_fast(up + dst_idx * 12u + 0u, read_be32_fast(in_p + 0x0Cu));
                write_be32_fast(up + dst_idx * 12u + 4u, read_be32_fast(in_p + 0x10u));
                write_be32_fast(up + dst_idx * 12u + 8u, tex_id);
            }
        }
    }

    const uint32_t alloc_off  = (read_be32_fast(vm_base + kAllocDesc + 0x18u) >> 1) & 0xFCu;
    const uint32_t alloc_inst = read_be32_fast(vm_base + kAllocObj + alloc_off);
    const uint32_t alloc_vt   = alloc_inst ? read_be32_fast(vm_base + alloc_inst) : 0u;
    const uint32_t alloc_fn   = alloc_vt ? read_be32_fast(vm_base + read_be32_fast(vm_base + alloc_vt + 0x1Cu)) : 0u;
    uint32_t sub_arr = 0u;
    if (alloc_fn) {
        GuestCall call(ctx);
        sub_arr = call.run_indirect_u32(alloc_fn, alloc_inst, 64u, 16u);
    }
    if (sub_arr) {
        std::memset(vm_base + sub_arr, 0, 64u);
        write_be32_fast(vm_base + sub_arr, sub);
        write_be32_fast(op + 0x2Cu, sub_arr);
        write_be32_fast(op + 0x24u, 16u);
        write_be32_fast(op + 0x20u, 1u);
    }

    cv_enqueue_trans_inline(ctx, read_be32_fast(vm_base + kEnqMgrPtr), obj);
}

namespace {

static inline void cv_call_hle_1(ppu_context* ctx, uint32_t nid, uint32_t a)
{
    ctx->gpr[3] = a;
    ps3_hle_call(nid, ctx);
}

static inline void cv_call_hle_2(ppu_context* ctx, uint32_t nid, uint32_t a, uint32_t b)
{
    ctx->gpr[3] = a;
    ctx->gpr[4] = b;
    ps3_hle_call(nid, ctx);
}

static inline uint32_t cv_call_direct_u32(ppu_context* ctx, void (*fn)(ppu_context*),
                                          uint32_t a, uint32_t b = 0u, uint32_t c = 0u, uint32_t d = 0u)
{
    const uint64_t saved_lr = ctx->lr;
    ctx->gpr[3] = a;
    ctx->gpr[4] = b;
    ctx->gpr[5] = c;
    ctx->gpr[6] = d;
    fn(ctx);
    while (g_trampoline_fn) {
        auto next = g_trampoline_fn;
        g_trampoline_fn = nullptr;
        next(ctx);
    }
    ctx->lr = saved_lr;
    return (uint32_t)ctx->gpr[3];
}

static inline uint32_t cv_call_indirect_u32(ppu_context* ctx, uint32_t fn_addr,
                                            uint32_t a, uint32_t b = 0u)
{
    const uint64_t saved_lr = ctx->lr;
    ctx->gpr[3] = a;
    ctx->gpr[4] = b;
    ctx->ctr = fn_addr;
    ps3_indirect_call(ctx);
    while (g_trampoline_fn) {
        auto next = g_trampoline_fn;
        g_trampoline_fn = nullptr;
        next(ctx);
    }
    ctx->lr = saved_lr;
    return (uint32_t)ctx->gpr[3];
}

static inline uint32_t cv_atomic_sub_be32(uint32_t addr, uint32_t sub_val)
{
    auto* p = reinterpret_cast<std::atomic<uint32_t>*>(vm_base + addr);
    uint32_t cur_be = p->load(std::memory_order_relaxed);
    for (;;) {
        const uint32_t cur_host = bswap32_fast(cur_be);
        const uint32_t next_be  = bswap32_fast(cur_host - sub_val);
        if (p->compare_exchange_weak(cur_be, next_be, std::memory_order_acq_rel, std::memory_order_relaxed)) {
            return cur_host;
        }
    }
}

static inline void cv_atomic_max_be32(uint32_t addr, uint32_t val)
{
    auto* p = reinterpret_cast<std::atomic<uint32_t>*>(vm_base + addr);
    uint32_t cur_be = p->load(std::memory_order_relaxed);
    for (;;) {
        const uint32_t cur_host = bswap32_fast(cur_be);
        if (cur_host >= val) return;
        const uint32_t next_be = bswap32_fast(val);
        if (p->compare_exchange_weak(cur_be, next_be, std::memory_order_acq_rel, std::memory_order_relaxed)) {
            return;
        }
    }
}

} /* namespace */

/* Hook 27: Draw command container destructor (US 0x007C0D68 / EU 0x007C2D28 / JP 0x007C1658) */
void CV_FD_FN_DRAWCMD_DTOR(ppu_context* ctx)
{
    if (!native_hot_enabled() || !vm_base) {
        CV_FD_ORIG_DRAWCMD_DTOR(ctx);
        return;
    }

    const uint32_t obj   = (uint32_t)ctx->gpr[3];
    const uint32_t count = read_be32_fast(vm_base + obj + 0x20u);
    write_be32_fast(vm_base + obj + 0x00u, kObjVtable);

    for (uint32_t i = 0; i < count; ++i) {
        const uint32_t arr  = read_be32_fast(vm_base + obj + 0x2Cu);
        const uint32_t elem = read_be32_fast(vm_base + arr + i * 4u);
        if (!elem) continue;
        const uint32_t vt = read_be32_fast(vm_base + elem);
        const uint32_t fn = read_be32_fast(vm_base + read_be32_fast(vm_base + vt));
        if (fn == kDrawElemDtorAddr) {
            ctx->gpr[3] = elem;
            CV_FD_FN_DRAWELEM_DTOR(ctx);
        } else if (fn) {
            cv_call_indirect_u32(ctx, fn, elem);
        }
    }

    const uint32_t arr = read_be32_fast(vm_base + obj + 0x2Cu);
    if (arr) {
        const uint32_t alloc_off  = (read_be32_fast(vm_base + kAllocDesc + 0x18u) >> 1) & 0xFCu;
        const uint32_t alloc_inst = read_be32_fast(vm_base + kAllocObj + alloc_off);
        const uint32_t alloc_vt   = alloc_inst ? read_be32_fast(vm_base + alloc_inst) : 0u;
        const uint32_t free_fn    = alloc_vt ? read_be32_fast(vm_base + read_be32_fast(vm_base + alloc_vt + 0x28u)) : 0u;
        if (free_fn == kHeapFreeAddr) {
            ctx->gpr[3] = alloc_inst;
            ctx->gpr[4] = arr;
            CV_FD_FN_HEAP_FREE(ctx);
        } else if (free_fn) {
            cv_call_indirect_u32(ctx, free_fn, alloc_inst, arr);
        }
    }

    uint8_t* const op = vm_base + obj;
    write_be32_fast(op + 0x2Cu, 0u);
    write_be32_fast(op + 0x24u, 0u);
    write_be32_fast(op + 0x20u, 0u);
    write_be32_fast(op + 0x1Cu, kDrawCmdBaseVtbl);
    write_be32_fast(op + 0x00u, kDrawCmdBaseVtbl);
}

/* Hook 28: Draw command element destructor (US 0x007C0F68 / EU 0x007C2F28 / JP 0x007C1858) */
void CV_FD_FN_DRAWELEM_DTOR(ppu_context* ctx)
{
    if (!native_hot_enabled() || !vm_base) {
        CV_FD_ORIG_DRAWELEM_DTOR(ctx);
        return;
    }

    const uint32_t obj = (uint32_t)ctx->gpr[3];
    uint8_t* const op  = vm_base + obj;
    write_be32_fast(op + 0x00u, kSubVtable);

    const uint32_t cnt = read_be32_fast(op + 0x04u);
    if (cnt > 0u) {
        const uint32_t base = read_be32_fast(op + 0x28u);
        uint8_t* ep = vm_base + base;
        for (uint32_t i = 0; i < cnt; ++i, ep += 0x10u) {
            write_be32_fast(ep + 0x00u, 0u);
            ep[0x05u] = 0u;
            write_be32_fast(ep + 0x08u, 0u);
            write_be32_fast(ep + 0x0Cu, 0u);
        }
    }

    const uint32_t p34 = read_be32_fast(op + 0x34u);
    write_be32_fast(op + 0x04u, 0u);
    if (p34) {
        const uint32_t alloc_inst = read_be32_fast(vm_base + kAllocObj + 0x20u);
        const uint32_t alloc_vt   = alloc_inst ? read_be32_fast(vm_base + alloc_inst) : 0u;
        const uint32_t free_fn    = alloc_vt ? read_be32_fast(vm_base + read_be32_fast(vm_base + alloc_vt + 0x28u)) : 0u;
        if (free_fn == kHeapFreeAddr) {
            ctx->gpr[3] = alloc_inst;
            ctx->gpr[4] = p34;
            CV_FD_FN_HEAP_FREE(ctx);
        } else if (free_fn) {
            cv_call_indirect_u32(ctx, free_fn, alloc_inst, p34);
        }
        write_be32_fast(op + 0x34u, 0u);
    }

    write_be32_fast(op + 0x00u, kDrawCmdBaseVtbl);
}

/* Hook 29: MtHeapAllocator::free (US 0x006D94FC / EU 0x006DB4BC / JP 0x006D9DEC) */
void CV_FD_FN_HEAP_FREE(ppu_context* ctx)
{
    if (!native_hot_enabled() || !vm_base) {
        CV_FD_ORIG_HEAP_FREE(ctx);
        return;
    }

    const uint32_t alloc = (uint32_t)ctx->gpr[3];
    const uint32_t ptr   = (uint32_t)ctx->gpr[4];
    if (!ptr || read_be32_fast(vm_base + alloc + 0x5Cu) == 0u) return;

    const uint32_t hdr     = ptr - read_be32_fast(vm_base + ptr - 4u);
    const uint32_t bkt_idx = (read_be32_fast(vm_base + hdr + 0x24u) >> 27) & 0x1Fu;
    const uint32_t bucket  = alloc + bkt_idx * 0x88u;

    cv_call_hle_2(ctx, 0x1573DC3Fu, bucket + 0x70u, 0u); /* sys_lwmutex_lock */

    uint8_t* const bp = vm_base + bucket;
    uint8_t* const hp = vm_base + hdr;

    const uint32_t active_cnt = read_be32_fast(bp + 0x48u);
    if (active_cnt != 0u) {
        const uint32_t prev = read_be32_fast(hp + 0x10u);
        const uint32_t next = read_be32_fast(hp + 0x14u);
        if (hdr == read_be32_fast(bp + 0x40u)) write_be32_fast(bp + 0x40u, next);
        if (hdr == read_be32_fast(bp + 0x44u)) write_be32_fast(bp + 0x44u, prev);
        if (prev) write_be32_fast(vm_base + prev + 0x14u, next);
        if (next) write_be32_fast(vm_base + next + 0x10u, prev);
        const uint32_t sz_units = (read_be32_fast(hp + 0x20u) >> 4) & 0x07FFFFFFu;
        write_be32_fast(bp + 0x4Cu, read_be32_fast(bp + 0x4Cu) - sz_units);
        write_be32_fast(bp + 0x48u, active_cnt - 1u);
    }

    const uint32_t w20 = read_be32_fast(hp + 0x20u) & 0x7FFFFFFFu;
    write_be32_fast(hp + 0x20u, w20);

    const uint32_t arena = read_be32_fast(hp + 0x04u);
    write_be32_fast(vm_base + arena + 0x08u, read_be32_fast(vm_base + arena + 0x08u) - 1u);

    const uint32_t blk_bytes = w20 & 0x7FFFFFF0u;
    write_be32_fast(bp + 0x68u, read_be32_fast(bp + 0x68u) - blk_bytes);

    const uint32_t old_total = cv_atomic_sub_be32(alloc + 0x04u, blk_bytes);
    if (read_be32_fast(vm_base + alloc + 0x08u) < old_total) {
        cv_atomic_max_be32(alloc + 0x08u, old_total);
    }

    ctx->gpr[1] -= 0x20u;
    const uint32_t out_addr = (uint32_t)ctx->gpr[1] + 0x10u;
    write_be32_fast(vm_base + out_addr, 0u);
    cv_call_direct_u32(ctx, CV_FD_FN_HEAP_COALESCE, alloc, bucket, hdr, out_addr);
    ctx->gpr[1] += 0x20u;

    cv_call_hle_1(ctx, 0x1BC200F4u, bucket + 0x70u); /* sys_lwmutex_unlock */
}

/* Hook 30: MtHeapAllocator::alloc_bucket (US 0x006D90DC / EU 0x006DB09C / JP 0x006D99CC) */
void CV_FD_FN_HEAP_ALLOC_BKT(ppu_context* ctx)
{
    if (!native_hot_enabled() || !vm_base) {
        CV_FD_ORIG_HEAP_ALLOC_BKT(ctx);
        return;
    }

    const uint32_t alloc    = (uint32_t)ctx->gpr[3];
    const uint32_t bucket   = (uint32_t)ctx->gpr[4];
    const uint32_t size     = (uint32_t)ctx->gpr[5];
    uint32_t       align    = (uint32_t)ctx->gpr[6];
    const uint32_t out_flag = (uint32_t)ctx->gpr[9];

    if (align < 16u) align = 16u;
    const uint32_t pad      = read_be32_fast(vm_base + alloc + 0x4F4u) & 0xFFu;
    const uint32_t mask     = ~(align - 1u);
    const uint32_t alloc_sz = (size + align + pad + (align - 1u)) & mask;

    cv_call_hle_2(ctx, 0x1573DC3Fu, bucket + 0x70u, 0u); /* sys_lwmutex_lock */

    const uint32_t blk = cv_call_direct_u32(ctx, CV_FD_FN_HEAP_ALLOC_INNER, alloc, bucket, alloc_sz, out_flag);
    uint32_t user_ptr = 0u;
    if (blk) {
        write_be32_fast(vm_base + blk + 0x28u, size);
        user_ptr = (blk + 0x30u + align - 1u) & mask;
        write_be32_fast(vm_base + user_ptr - 4u, user_ptr - blk);
    }

    cv_call_hle_1(ctx, 0x1BC200F4u, bucket + 0x70u); /* sys_lwmutex_unlock */
    ctx->gpr[3] = user_ptr;
}

/* Hook 31: Translucent depth-sort key calculation (EU/US/JP 0x00016658 -- replaces 61 Altivec ops) */
void CV_FD_FN_DEPTH_KEY(ppu_context* ctx)
{
    if (!native_hot_enabled() || !vm_base) {
        CV_FD_ORIG_DEPTH_KEY(ctx);
        return;
    }

    const uint32_t sc       = (uint32_t)ctx->gpr[5];
    const uint32_t pos_addr = (uint32_t)ctx->gpr[6];
    const uint32_t n        = read_be32_fast(vm_base + sc + 0x54u);
    const uint32_t mode     = read_be32_fast(vm_base + sc + 0x88u);
    const uint32_t idx      = (n != 0u) ? read_be32_fast(vm_base + sc + (n - 1u) * 12u + 0x14u) : 0u;

    int32_t res = 0;
    if (mode == 0x10u || mode == 0x14u) {
        const uint32_t mat = cv_call_direct_u32(ctx, CV_FD_FN_GET_MAT, idx);
        const uint8_t* const mp = vm_base + mat;
        const uint8_t* const pp = vm_base + (pos_addr & ~0xFu);
        const float px = read_bef32_fast(pp + 0u);
        const float py = read_bef32_fast(pp + 4u);
        const float pz = read_bef32_fast(pp + 8u);
        const float z  = px * read_bef32_fast(mp + 0x08u)
                       + py * read_bef32_fast(mp + 0x18u)
                       + pz * read_bef32_fast(mp + 0x28u)
                       + read_bef32_fast(mp + 0x38u);
        const float neg = -z;
        int32_t iv = 0;
        if (neg != neg || neg <= -2147483648.0f) iv = (int32_t)0x80000000;
        else if (neg >= 2147483647.0f)           iv = 0x7FFFFFFF;
        else                                     iv = (int32_t)neg;
        if (iv > 0x7FFF) iv = 0x7FFF;
        if (iv < 0)      iv = 0;
        res = 0x7FFF - (int32_t)((uint32_t)(0x7FFF - iv) & 0xFFFFFu);
    } else {
        const double saved_f1 = ctx->fpr[1];
        ctx->fpr[1] = 1.0;
        res = (int32_t)cv_call_direct_u32(ctx, CV_FD_FN_DEPTH_FALLBACK, pos_addr, sc + 0x60u);
        ctx->fpr[1] = saved_f1;
    }

    const int32_t hi = (int32_t)read_be32_fast(vm_base + kDepthHi);
    if (hi <= res) {
        const int32_t lo = (int32_t)read_be32_fast(vm_base + kDepthLo);
        if (lo >= res) res = lo;
    }
    ctx->gpr[3] = (int64_t)res;
}

static inline int32_t cv_hash_find_inline(uint32_t map, uint32_t key)
{
    const uint32_t cap = read_be32_fast(vm_base + map + 4u);
    if (cap == 0u) return -1;
    uint32_t h = ((key >> 4) | key) ^ key;
    const uint32_t table = read_be32_fast(vm_base + map + 8u);
    for (uint32_t i = 0; i < cap; ++i, ++h) {
        const uint32_t slot = h % cap;
        const uint8_t* const ent = vm_base + table + slot * 20u;
        if (ent[0] != 0u && read_be32_fast(ent + 4u) == key) {
            return (int32_t)slot;
        }
    }
    return -1;
}

static inline uint32_t cv_hash_get_inline(ppu_context* ctx, uint32_t map, uint32_t key)
{
    if (key == 0u) return 0u;
    const uint32_t vt = read_be32_fast(vm_base + map);
    const uint32_t fn = read_be32_fast(vm_base + read_be32_fast(vm_base + vt + 0x14u));
    const int32_t slot = (fn == kHashFindAddr)
        ? cv_hash_find_inline(map, key)
        : (fn ? (int32_t)cv_call_indirect_u32(ctx, fn, map, key) : -1);
    if (slot <= -1) return 0u;
    const uint32_t table = read_be32_fast(vm_base + map + 8u);
    return read_be32_fast(vm_base + table + (uint32_t)slot * 20u + 8u);
}

/* Hook 32: Texture/material descriptor lookup (EU/US/JP 0x0001638C) */
void CV_FD_FN_TEX_LOOKUP(ppu_context* ctx)
{
    if (!native_hot_enabled() || !vm_base) {
        CV_FD_ORIG_TEX_LOOKUP(ctx);
        return;
    }

    const uint32_t flag = (uint32_t)ctx->gpr[4];
    uint32_t       list = (uint32_t)ctx->gpr[5];
    uint32_t       idx  = (uint32_t)ctx->gpr[6];

    const uint32_t sys = get_sys();
    if (flag == 0x00800000u) {
        idx = (read_be32_fast(vm_base + sys + 0x28A84u) != 0u) ? 4u : 1u;
    }

    const uint32_t default_list = sys + 0x1ED20u;
    if (!list) list = default_list;

    auto query_tex_mgr = [ctx](uint32_t key) -> uint32_t {
        const uint32_t mgr = read_be32_fast(vm_base + kTexMgrPtr) + 0x24u;
        const uint32_t vt  = read_be32_fast(vm_base + mgr);
        const uint32_t fn  = read_be32_fast(vm_base + read_be32_fast(vm_base + vt + 8u));
        if (fn == kHashGetAddr) return cv_hash_get_inline(ctx, mgr, key);
        return fn ? cv_call_indirect_u32(ctx, fn, mgr, key) : 0u;
    };

    uint32_t res = 0u;
    if (list && list != default_list) {
        const uint32_t obj = query_tex_mgr(list);
        if (obj) {
            uint32_t found_idx = idx;
            if (read_be32_fast(vm_base + obj + 0x64u) <= idx) {
                found_idx = cv_call_direct_u32(ctx, CV_FD_FN_TEX_FIND_IDX, obj, idx);
            }
            if ((int32_t)found_idx != -1) {
                res = cv_call_direct_u32(ctx, CV_FD_FN_TEX_GET_BY_IDX, obj, found_idx);
                if (res) {
                    ctx->gpr[3] = res;
                    return;
                }
            }
        }
    }

    if (list != default_list) {
        ctx->gpr[3] = res;
        return;
    }

    uint32_t cnt = read_be32_fast(vm_base + list + 4u);
    if (cnt > idx) {
        const uint32_t entry = read_be32_fast(vm_base + list) + idx * 12u;
        const uint32_t obj   = query_tex_mgr(entry);
        if (obj) res = read_be32_fast(vm_base + obj + 0x1Cu);
        ctx->gpr[3] = res;
        return;
    }

    const uint32_t base_obj = query_tex_mgr(list);
    uint32_t i = base_obj ? read_be32_fast(vm_base + base_obj + 0x64u) : 0u;
    cnt = read_be32_fast(vm_base + list + 4u);
    for (; i < cnt; ++i) {
        const uint32_t entry = read_be32_fast(vm_base + list) + i * 12u;
        const uint32_t item  = query_tex_mgr(entry);
        if (item) {
            const uint32_t sub = read_be32_fast(vm_base + item + 0x20u);
            res = sub ? cv_call_direct_u32(ctx, CV_FD_FN_TEX_GET_SUB, sub, idx) : 0u;
            if (res) break;
        }
        cnt = read_be32_fast(vm_base + list + 4u);
    }
    ctx->gpr[3] = res;
}

static inline int32_t cv_f2i32_trunc(float f)
{
    if (f != f || f <= -2147483648.0f) return (int32_t)0x80000000;
    if (f >= 2147483647.0f) return 0x7FFFFFFF;
    return (int32_t)f;
}

static inline uint32_t cv_f2u32_sat(float v)
{
    if (v != v || v <= 0.0f) return 0u;
    if (v >= 4294967295.0f) return 4294967295u;
    return (uint32_t)v;
}

/* Hook 33: Render-state / blend bitfield builder (EU/US/JP 0x00016838, pure leaf) */
void CV_FD_FN_RENDER_STATE(ppu_context* ctx)
{
    if (!native_hot_enabled() || !vm_base) {
        CV_FD_ORIG_RENDER_STATE(ctx);
        return;
    }

    const uint32_t p    = (uint32_t)ctx->gpr[3] + 8u;
    uint32_t       r9   = (uint32_t)ctx->gpr[4];
    const int32_t  s5   = (int32_t)ctx->gpr[5];
    const int32_t  s6   = (int32_t)ctx->gpr[6];
    const uint32_t out  = (uint32_t)ctx->gpr[7];

    const uint32_t r11  = r9 & 0xFFFF0000u;
    const uint8_t  b14  = vm_base[p + 0x14u];
    const uint8_t  b17  = vm_base[p + 0x17u];

    uint32_t r10 = 0x10u;
    uint32_t r30 = 0x10u;
    uint32_t r3  = 0x10u;
    if (r11 == 0x80000000u) {
        r10 = 0x12u;
        r3  = 0u;
    }

    if ((b14 & 0x10u) == 0u) {
        r3 |= 0x00020000u;
    }

    if (b17 != 0u) {
        const uint8_t b18 = vm_base[p + 0x18u];
        if (b18 != 0u) {
            const uint8_t b19 = vm_base[p + 0x19u];
            uint32_t v30 = b17;
            uint32_t v31 = b18;
            if (b19 != 0u) {
                static constexpr uint32_t kBlendLut[8] = { 0xBu, 0xAu, 3u, 5u, 8u, 6u, 2u, 4u };
                if (b17 >= 8u || b18 >= 8u) {
                    CV_FD_ORIG_RENDER_STATE(ctx);
                    return;
                }
                v30 = kBlendLut[b17];
                v31 = kBlendLut[b18];
            }
            if (v30 == 8u && v31 == 6u) {
                r30 = 0x12u;
            } else if (v30 == 8u && v31 == 0xAu) {
                r30 = 0x11u;
            } else {
                r30 = (v30 == 4u && v31 == 5u) ? 0x19u : 0x12u;
            }
        }
    }

    if (r11 == 0x80000000u) {
        if (s6 <= 0x50) {
            if (s6 <= 0x22) {
                r10 = (s6 == 0x1E || s6 == 0x11 || s6 == 0x0B) ? 0x12u : 0x11u;
            } else if (s6 <= 0x3A) {
                if (s6 <= 0x2E) {
                    r10 = (s6 <= 0x24 || s6 > 0x29) ? 0x12u : 0x11u;
                } else if (s6 <= 0x32) {
                    r10 = (s6 <= 0x31) ? 0x11u : 0x12u;
                } else {
                    r10 = (s6 == 0x37) ? 0x12u : 0x11u;
                }
            } else if (s6 > 0x45) {
                r10 = 0x11u;
            } else if (s6 <= 0x40) {
                r10 = (s6 == 0x3F) ? 0x11u : 0x12u;
            } else if (s6 <= 0x42) {
                r10 = (s6 <= 0x41) ? 0x11u : 0x12u;
            } else {
                r10 = (s6 <= 0x43) ? 0x11u : 0x12u;
            }
        } else if (s6 <= 0x1A0) {
            if (s6 <= 0x61) {
                if (s6 > 0x5A) {
                    r10 = 0x12u;
                } else if (s6 <= 0x54) {
                    r10 = (s6 <= 0x53) ? 0x12u : 0x11u;
                } else {
                    r10 = (s6 <= 0x55) ? 0x12u : 0x11u;
                }
            } else if (s6 <= 0x199) {
                if (s6 <= 0x197) {
                    r10 = (s6 <= 0x18F) ? 0x11u : 0x12u;
                } else {
                    r10 = (s6 <= 0x198) ? 0x11u : 0x12u;
                }
            } else if (s6 <= 0x19B) {
                r10 = (s6 <= 0x19A) ? 0x11u : 0x12u;
            } else {
                r10 = (s6 == 0x19D) ? 0x12u : 0x11u;
            }
        } else if (s6 <= 0x1AC) {
            if (s6 <= 0x1A6) {
                r10 = 0x12u;
            } else if (s6 <= 0x1AA) {
                r10 = (s6 <= 0x1A9) ? 0x11u : 0x12u;
            } else {
                r10 = (s6 <= 0x1AB) ? 0x11u : 0x12u;
            }
        } else if (s6 <= 0x1B7) {
            r10 = (s6 == 0x1AE) ? 0x12u : 0x11u;
        } else {
            r10 = (s6 > 0x1B9) ? 0x11u : 0x12u;
        }

        if (s5 <= 0xDD) {
            if (s5 <= 0x6F) {
                if (s5 == 0x6F) {
                    r30 = 0x10u; r9 |= 0x10u; r10 = 0x10u;
                } else if (s5 == 0x0D || s5 == 0x0A || s5 == 0x07 || s5 == 0x00) {
                    r30 = 0x12u; r10 = 0x12u;
                } else if (s5 == 0x05) {
                    r30 = 1u; r10 = 1u;
                } else {
                    r30 = r10;
                }
            } else if (s5 == 0xDD) {
                const uint32_t sys = get_sys();
                const uint8_t b95 = vm_base[sys + 0x95u];
                const uint8_t b96 = vm_base[sys + 0x96u];
                r10 = (b95 == 9u && (b96 == 0x1Eu || b96 == 6u || b96 == 5u)) ? 0x15u : 0x12u;
                r30 = r10;
            } else if (s5 == 0xCB || s5 == 0x77 || s5 == 0x72) {
                r30 = 0x12u; r10 = 0x12u;
            } else if (s5 == 0xC1) {
                const uint32_t sys = get_sys();
                r10 = (vm_base[sys + 0x95u] == 9u && vm_base[sys + 0x96u] == 5u) ? 0x15u : 0x11u;
                r30 = r10;
            } else if (s5 == 0x93) {
                r30 = 1u; r10 = 1u;
            } else {
                r30 = r10;
            }
        } else if (s5 <= 0x12C) {
            if (s5 == 0x128 || s5 == 0xFA) {
                r30 = 0x12u; r10 = 0x12u;
            } else if (s5 == 0x109 || s5 == 0x100) {
                r30 = 1u; r10 = 1u;
            } else {
                r30 = r10;
            }
        } else if (s5 <= 0x169) {
            if (s5 <= 0x134) {
                if (s5 <= 0x12E)      { r30 = 1u; r10 = 1u; }
                else if (s5 == 0x12F) { r30 = 0x12u; r10 = 0x12u; }
                else                  { r30 = r10; }
            } else if (s5 <= 0x136) {
                if (s5 <= 0x135)      { r30 = 1u; r10 = 1u; }
                else                  { r30 = r10; }
            } else if (s5 <= 0x137) {
                r30 = 0x12u; r10 = 0x12u;
            } else {
                r30 = r10;
            }
        } else if (s5 <= 0x172) {
            if (s5 <= 0x16D) {
                if (s5 <= 0x16C)      { r30 = 0x12u; r10 = 0x12u; }
                else                  { r30 = r10; }
            } else if (s5 <= 0x16E) {
                r30 = 0x12u; r9 |= 0x2u; r10 = 0x12u;
            } else {
                r30 = r10;
            }
        } else if (s5 <= 0x18C) {
            if (s5 <= 0x175) { r30 = 0x12u; r10 = 0x12u; }
            else             { r30 = r10; }
        } else if (s5 <= 0x18E) {
            r30 = 1u; r10 = 1u;
        } else {
            r30 = r10;
        }
    }

    if (b14 & 0x08u) {
        r10 = r30;
    }

    if (r9 != 0u) {
        if (r9 & 0x02u) r3 &= ~0x00020000u;
        if (r9 & 0x04u) r3 &= ~0x00000010u;
        if (r9 & 0x08u) r3 |= 0x00000008u;
        if (r9 & 0x20u) r3 |= 0x00001000u;
        if (r9 & 0x10u) r10 = 0x10u;
    }

    const uint8_t flag4 = vm_base[kStateFlagAddr];
    if (flag4 != 0u) {
        r10 = 0x10u;
    }

    if (out != 0u) {
        const uint32_t hi = (uint32_t)(flag4 & 0xC0u) | ((r10 & 0x1Fu) << 16);
        write_be32_fast(vm_base + out + 0u, hi);
        write_be32_fast(vm_base + out + 4u, r3);
    }
}

/* Hook 34: 64B->32B vertex packer + draw command enqueue (US 0x007BE738 / EU 0x007C06F8 / JP 0x007BF028) */
void CV_FD_FN_PACK_VTX64(ppu_context* ctx)
{
    if (!native_hot_enabled() || !vm_base) {
        CV_FD_ORIG_PACK_VTX64(ctx);
        return;
    }

    const uint32_t sc        = (uint32_t)ctx->gpr[3];
    const uint32_t vtx_in    = (uint32_t)ctx->gpr[4];
    const uint32_t vtx_cnt   = (uint32_t)ctx->gpr[5];
    const uint32_t state_ptr = (uint32_t)ctx->gpr[6];
    const uint32_t arg7      = (uint32_t)ctx->gpr[7];
    const int32_t  depth_in  = (int32_t)ctx->gpr[8];
    const uint32_t arg9      = (uint32_t)ctx->gpr[9];
    const uint32_t arg10     = (uint32_t)ctx->gpr[10];

    uint32_t       state_hi  = read_be32_fast(vm_base + state_ptr + 0u);
    const uint32_t state_lo  = read_be32_fast(vm_base + state_ptr + 4u);
    const uint32_t sc94      = read_be32_fast(vm_base + sc + 0x94u);
    state_hi = (state_hi & ~0x0000FFC0u) | ((sc94 << 6) & 0x0000FFC0u);
    state_hi = (state_hi & ~0xF8000000u) | (3u << 27);

    const uint32_t mode      = read_be32_fast(vm_base + sc + 0x88u);
    const int32_t  sentinel  = (int32_t)read_be32_fast(vm_base + kDepthLo + 8u);
    int32_t        depth     = depth_in;

    if (depth == sentinel) {
        const uint32_t n   = read_be32_fast(vm_base + sc + 0x54u);
        const uint32_t idx = (n != 0u) ? read_be32_fast(vm_base + sc + (n - 1u) * 12u + 0x14u) : 0u;
        if (mode == 0x10u || mode == 0x14u) {
            const uint32_t mat = cv_call_direct_u32(ctx, CV_FD_FN_GET_MAT, idx);
            const uint8_t* const mp = vm_base + mat;
            const uint8_t* const pp = vm_base + vtx_in;
            const float px = read_bef32_fast(pp + 0u);
            const float py = read_bef32_fast(pp + 4u);
            const float pz = read_bef32_fast(pp + 8u);
            const float z  = px * read_bef32_fast(mp + 0x08u)
                           + py * read_bef32_fast(mp + 0x18u)
                           + pz * read_bef32_fast(mp + 0x28u)
                           + read_bef32_fast(mp + 0x38u);
            int32_t iv = cv_f2i32_trunc(-z);
            if (iv > 0x7FFF) iv = 0x7FFF;
            if (iv < 0)      iv = 0;
            depth = 0x7FFF - (int32_t)((uint32_t)(0x7FFF - iv) & 0xFFFFFu);
        } else {
            ctx->gpr[1] -= 0x20u;
            const uint32_t pos_tmp = (uint32_t)ctx->gpr[1] + 0x10u;
            std::memcpy(vm_base + pos_tmp, vm_base + vtx_in, 12u);
            const double saved_f1 = ctx->fpr[1];
            ctx->fpr[1] = read_bef32_fast(vm_base + kPackConsts + 0x30u);
            depth = (int32_t)cv_call_direct_u32(ctx, CV_FD_FN_DEPTH_FALLBACK, pos_tmp, sc + 0x60u);
            ctx->fpr[1] = saved_f1;
            ctx->gpr[1] += 0x20u;
        }
    }

    if (depth < 0) {
        ctx->gpr[3] = 0xEu;
        return;
    }

    const int32_t max_depth = (int32_t)read_be32_fast(vm_base + kDepthLo);
    if (max_depth < depth) depth = max_depth;

    uint32_t       vb        = read_be32_fast(vm_base + sc + 0x08u);
    const uint32_t req_bytes = vtx_cnt << 5;
    const uint32_t cur_off   = read_be32_fast(vm_base + vb + 0x0Cu);
    uint32_t       cap       = read_be32_fast(vm_base + vb + 0x08u);
    uint32_t       dst_ptr   = 0u;
    if ((uint32_t)(cur_off + req_bytes) <= cap) {
        dst_ptr = read_be32_fast(vm_base + vb + 0x14u);
    } else {
        write_be32_fast(vm_base + vb + 0x04u, 0u);
        cv_call_direct_u32(ctx, CV_FD_FN_VB_FLUSH, vb, 0x00020000u);
        cap = read_be32_fast(vm_base + vb + 0x08u);
        if (req_bytes > cap) {
            ctx->gpr[3] = 4u;
            return;
        }
        dst_ptr = read_be32_fast(vm_base + vb + 0x14u);
    }
    write_be32_fast(vm_base + vb + 0x04u, req_bytes);
    if (dst_ptr == 0u) {
        ctx->gpr[3] = 4u;
        return;
    }

    const uint8_t* const kc  = vm_base + kPackConsts;
    const float c34          = read_bef32_fast(kc + 0x34u);
    const uint16_t v28_u16   = (uint16_t)cv_f2i32_trunc((float)(int32_t)(arg7 + 0x8000u) - c34);

    if (vtx_cnt > 0u) {
        const float   c38       = read_bef32_fast(kc + 0x38u);
        const float   c3C       = read_bef32_fast(kc + 0x3Cu);
        const float   c40       = read_bef32_fast(kc + 0x40u);
        const uint8_t b_arg9    = (uint8_t)cv_f2i32_trunc((float)(int32_t)(arg9 - 128u) + c40);
        const bool    has_extra = ((arg10 & 0xFFu) != 0u);

        uint32_t src_addr = vtx_in;
        uint8_t* dst      = vm_base + dst_ptr;
        for (uint32_t i = 0; i < vtx_cnt; ++i, src_addr += 0x40u, dst += 0x20u) {
            const uint8_t* const src = vm_base + src_addr;
            std::memcpy(dst, src, 16u);

            const float u = read_bef32_fast(src + 0x18u);
            const float v = read_bef32_fast(src + 0x1Cu);
            write_be16_fast(dst + 0x10u, (uint16_t)cv_f2i32_trunc(u * c38));
            write_be16_fast(dst + 0x12u, (uint16_t)cv_f2i32_trunc(v * c38));
            write_be16_fast(dst + 0x14u, v28_u16);

            const uint16_t s14 = read_be16_fast(src + 0x14u);
            write_be16_fast(dst + 0x16u, (uint16_t)cv_f2i32_trunc((float)s14 - c34));

            if (has_extra) {
                const uint8_t* const p20 = vm_base + ((src_addr + 0x20u) & ~0xFu);
                dst[0x18u] = (uint8_t)cv_f2u32_sat(read_bef32_fast(p20 + 0u) * c3C + c40);
                dst[0x19u] = (uint8_t)cv_f2u32_sat(read_bef32_fast(p20 + 4u) * c3C + c40);
                dst[0x1Au] = (uint8_t)cv_f2u32_sat(read_bef32_fast(p20 + 8u) * c3C + c40);

                const uint8_t* const p30 = vm_base + ((src_addr + 0x30u) & ~0xFu);
                dst[0x1Cu] = (uint8_t)cv_f2u32_sat(read_bef32_fast(p30 + 0u) * c3C + c40);
                dst[0x1Du] = (uint8_t)cv_f2u32_sat(read_bef32_fast(p30 + 4u) * c3C + c40);
                dst[0x1Eu] = (uint8_t)cv_f2u32_sat(read_bef32_fast(p30 + 8u) * c3C + c40);

                dst[0x1Bu] = (uint8_t)cv_f2i32_trunc((float)src[0x16u] + c40);
            }
            dst[0x1Fu] = b_arg9;
        }
    }

    vb = read_be32_fast(vm_base + sc + 0x08u);
    const uint32_t pending = read_be32_fast(vm_base + vb + 0x04u);
    if (pending != 0u) {
        write_be32_fast(vm_base + vb + 0x04u, 0u);
        write_be32_fast(vm_base + vb + 0x14u, read_be32_fast(vm_base + vb + 0x14u) + pending);
        write_be32_fast(vm_base + vb + 0x0Cu, read_be32_fast(vm_base + vb + 0x0Cu) + pending);
        vb = read_be32_fast(vm_base + sc + 0x08u);
    }

    const uint32_t m88    = read_be32_fast(vm_base + sc + 0x88u);
    const uint32_t m8C    = read_be32_fast(vm_base + sc + 0x8Cu) & 0x0FFFu;
    const uint32_t key25  = ((m88 & 0x1Fu) << 27) | (((uint32_t)depth & 0x7FFFu) << 12) | m8C;
    const uint32_t q      = read_be32_fast(vm_base + sc + 0x04u);
    const uint32_t n2     = read_be32_fast(vm_base + sc + 0x54u);
    const uint32_t idx2   = (n2 != 0u) ? read_be32_fast(vm_base + sc + (n2 - 1u) * 12u + 0x14u) : 0u;

    uint32_t const_buf = 0u;
    if ((state_lo & 0xF08900A0u) != 0u) {
        if (vm_base[sc + 0xC4u] != 0u) {
            uint32_t c_cur = read_be32_fast(vm_base + idx2 + 0x14u);
            const uint32_t c_end = read_be32_fast(vm_base + idx2 + 0x18u);
            if (c_cur + 0x20u > c_end) {
                cv_call_direct_u32(ctx, CV_FD_FN_CONST_FLUSH, idx2);
                c_cur = read_be32_fast(vm_base + idx2 + 0x14u);
            }
            write_be32_fast(vm_base + idx2 + 0x14u, c_cur + 0x20u);
            std::memcpy(vm_base + c_cur, vm_base + sc + 0xA0u, 32u);
            write_be32_fast(vm_base + sc + 0xC0u, c_cur);
            vm_base[sc + 0xC4u] = 0u;
        }
        const_buf = read_be32_fast(vm_base + sc + 0xC0u);
    }

    const uint32_t q_cnt = read_be32_fast(vm_base + q + 0x80u);
    const uint32_t q_max = read_be32_fast(vm_base + q + 0x7Cu);
    if (q_cnt + 1u > q_max) {
        ctx->gpr[3] = 0xCu;
        return;
    }

    const uint32_t q64    = read_be32_fast(vm_base + q + 0x64u);
    const uint32_t cmd    = read_be32_fast(vm_base + q + 0x74u);
    const uint32_t sort_p = read_be32_fast(vm_base + q + 0x78u);

    uint8_t* const cp = vm_base + cmd;
    write_be32_fast(cp + 0x00u, state_hi & 0xFFFFFFFCu);
    write_be32_fast(cp + 0x04u, state_lo);
    write_be32_fast(cp + 0x08u, key25);
    write_be32_fast(cp + 0x0Cu, vb);
    write_be32_fast(cp + 0x10u, const_buf);
    write_be32_fast(cp + 0x14u, dst_ptr);
    write_be32_fast(cp + 0x18u, 0u);
    write_be32_fast(cp + 0x1Cu, vtx_cnt);
    write_be32_fast(cp + 0x20u, ((vtx_cnt + 1u) & ~1u) + 2u);

    write_be32_fast(vm_base + sort_p + 0x00u, cmd);
    write_be32_fast(vm_base + sort_p + 0x04u, key25);

    write_be32_fast(vm_base + q + 0x74u, cmd + 0x28u);
    write_be32_fast(vm_base + q + 0x78u, sort_p + 8u);
    write_be32_fast(vm_base + q + 0x80u, q_cnt + 1u);

    const uint32_t bkt_cnt_addr = q + ((q64 - 1u) << 4) + 0x0Cu;
    write_be32_fast(vm_base + bkt_cnt_addr, read_be32_fast(vm_base + bkt_cnt_addr) + 1u);

    ctx->gpr[3] = 0u;
}

static inline int64_t cv_f2i64_trunc(float f)
{
    if (f != f || f <= -9223372036854775808.0f) return (int64_t)0x8000000000000000ULL;
    if (f >= 9223372036854775807.0f) return 0x7FFFFFFFFFFFFFFFLL;
    return (int64_t)f;
}

static inline uint32_t cv_call_direct_u32_5(ppu_context* ctx, void (*fn)(ppu_context*),
                                            uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e)
{
    const uint64_t saved_lr = ctx->lr;
    ctx->gpr[3] = a;
    ctx->gpr[4] = b;
    ctx->gpr[5] = c;
    ctx->gpr[6] = d;
    ctx->gpr[7] = e;
    fn(ctx);
    while (g_trampoline_fn) {
        auto next = g_trampoline_fn;
        g_trampoline_fn = nullptr;
        next(ctx);
    }
    ctx->lr = saved_lr;
    return (uint32_t)ctx->gpr[3];
}

static inline uint32_t cv_call_direct_u32_8(ppu_context* ctx, void (*fn)(ppu_context*),
                                            uint32_t a, uint32_t b, uint32_t c, uint32_t d,
                                            uint32_t e, uint32_t f, uint32_t g, uint32_t h)
{
    const uint64_t saved_lr = ctx->lr;
    ctx->gpr[3] = a;
    ctx->gpr[4] = b;
    ctx->gpr[5] = c;
    ctx->gpr[6] = d;
    ctx->gpr[7] = e;
    ctx->gpr[8] = f;
    ctx->gpr[9] = g;
    ctx->gpr[10] = h;
    fn(ctx);
    while (g_trampoline_fn) {
        auto next = g_trampoline_fn;
        g_trampoline_fn = nullptr;
        next(ctx);
    }
    ctx->lr = saved_lr;
    return (uint32_t)ctx->gpr[3];
}

/* Hook 35: Effect / particle / translucent mesh drawer (EU/US/JP 0x00016E10 -- #1 hottest PPU function) */
void CV_FD_FN_DRAW_EFFECT(ppu_context* ctx)
{
    if (!native_hot_enabled() || !vm_base) {
        CV_FD_ORIG_DRAW_EFFECT(ctx);
        return;
    }

    const uint32_t self    = (uint32_t)ctx->gpr[3];
    const uint32_t arg4    = (uint32_t)ctx->gpr[4];
    const uint32_t arg5    = (uint32_t)ctx->gpr[5];
    const uint32_t mat_ptr = (uint32_t)ctx->gpr[6];
    const uint32_t obj     = (uint32_t)ctx->gpr[7];
    const uint32_t flags8  = (uint32_t)ctx->gpr[8];

    struct StackGuard {
        ppu_context* c;
        uint64_t     orig_sp;
        StackGuard(ppu_context* ctx_, uint32_t sz) : c(ctx_), orig_sp(ctx_->gpr[1]) {
            c->gpr[1] = (orig_sp - sz) & ~0xFULL;
        }
        ~StackGuard() { c->gpr[1] = orig_sp; }
    } sg(ctx, 0xC0u);

    const uint32_t sp_base   = (uint32_t)ctx->gpr[1];
    const uint32_t sp_tmp    = sp_base + 0x10u; /* 0x30 bytes for light query */
    const uint32_t sp_mat    = sp_base + 0x40u; /* 0x40 bytes for transposed 4x4 matrix */
    const uint32_t sp_state  = sp_base + 0x80u; /* 0x10 bytes for 8B render state */
    const uint32_t sp_center = sp_base + 0x90u; /* 0x10 bytes for 16B bbox center */

    constexpr uint32_t kConsts = 0x00016DD8u;
    const uint8_t* const kc    = vm_base + kConsts;

    uint32_t r30_mode = 0x10u;
    int32_t  id24     = -1;
    uint8_t  has_tint = 0u;
    float    tint_b   = read_bef32_fast(kc + 0x00u);
    float    tint_g   = tint_b;
    float    tint_r   = tint_b;

    if (flags8 == 0x80000000u) {
        id24 = (int32_t)read_be32_fast(vm_base + obj + 0x48u);
        if (id24 == 0x6F) {
            const uint32_t sys = get_sys();
            if (vm_base[sys + 0x95u] == 7u) r30_mode = 0x15u;
            write_be32_fast(vm_base + obj + 0x6Cu, (uint32_t)-500);
        } else if (id24 == 0x7B) {
            const uint32_t sys = get_sys();
            if ((int8_t)vm_base[sys + 0x95u] == 3 && vm_base[sys + 0x96u] == 6u) r30_mode = 0xCu;
        }

        if (id24 != 0x6F) {
            const uint32_t mgr = read_be32_fast(vm_base + k16E10MgrPtr);
            uint32_t o29 = read_be32_fast(vm_base + mgr + 0xC0u);
            if (o29 != 0u) {
                const uint32_t vt = read_be32_fast(vm_base + o29);
                const uint32_t fn = read_be32_fast(vm_base + read_be32_fast(vm_base + vt + 0x14u));
                uint32_t node = fn ? cv_call_indirect_u32(ctx, fn, o29) : 0u;
                const uint32_t target_type = read_be32_fast(vm_base + k16E10TypeDesc + 4u);
                while (node != 0u && read_be32_fast(vm_base + node + 4u) != target_type) {
                    node = read_be32_fast(vm_base + node + 0x10u);
                }
                if (node == 0u) o29 = 0u;
                tint_b = tint_g = tint_r = read_bef32_fast(kc + 0x04u);
                if (o29 != 0u) {
                    if (cv_call_direct_u32(ctx, CV_FD_FN_LIGHT_QUERY, o29, sp_tmp) != 0u) {
                        const uint8_t* const lp = vm_base + ((o29 + 0x14E0u) & ~0xFu);
                        const float sc_f = read_bef32_fast(kc + 0x08u);
                        tint_r   = read_bef32_fast(lp + 0u) * sc_f;
                        tint_g   = read_bef32_fast(lp + 4u) * sc_f;
                        tint_b   = read_bef32_fast(lp + 8u) * sc_f;
                        has_tint = 1u;
                    }
                }
            }
        }
    }

    const uint32_t sc      = cv_call_direct_u32(ctx, CV_FD_FN_GET_SC,
                                                read_be32_fast(vm_base + k16E10CtxPtr), arg4, r30_mode, arg5);
    const uint32_t geom    = obj + 4u;
    const uint32_t pos_cnt = read_be32_fast(vm_base + geom + 0x0Cu);
    const uint32_t buf_p1  = cv_call_direct_u32(ctx, CV_FD_FN_BUMP_ALLOC, 1u, pos_cnt * 12u);
    const uint32_t buf_p2  = cv_call_direct_u32(ctx, CV_FD_FN_BUMP_ALLOC, 1u, pos_cnt * 12u);

    const uint32_t nrm_cnt = read_be32_fast(vm_base + geom + 0x10u);
    uint32_t buf_nrm = 0u;
    if (nrm_cnt != 0u) {
        const uint32_t n1 = cv_call_direct_u32(ctx, CV_FD_FN_BUMP_ALLOC, 1u, nrm_cnt * 12u);
        buf_nrm           = cv_call_direct_u32(ctx, CV_FD_FN_BUMP_ALLOC, 1u, nrm_cnt * 12u);
        if (n1 == 0u || buf_nrm == 0u) return;
    }
    if (buf_p1 == 0u || buf_p2 == 0u) return;

    /* Transpose 4x4 matrix from mat_ptr (& ~0xF) into sp_mat */
    {
        const uint8_t* const ms = vm_base + (mat_ptr & ~0xFu);
        uint8_t* const md       = vm_base + sp_mat;
        for (uint32_t c = 0; c < 4u; ++c) {
            for (uint32_t r = 0; r < 4u; ++r) {
                std::memcpy(md + (c * 4u + r) * 4u, ms + (r * 4u + c) * 4u, 4u);
            }
        }
    }

    const bool no_negate_yz = (id24 == 0x92 || id24 == 0x163);
    const uint32_t src_pos  = read_be32_fast(vm_base + geom + 0x00u);
    const uint32_t src_nrm  = (nrm_cnt != 0u) ? read_be32_fast(vm_base + geom + 0x08u) : 0u;

    if (no_negate_yz) {
        for (uint32_t i = 0; i < pos_cnt; ++i) {
            const uint8_t* const sp = vm_base + src_pos + i * 16u;
            std::memcpy(vm_base + buf_p1 + i * 12u, sp, 12u);
            std::memcpy(vm_base + buf_p2 + i * 12u, sp, 12u);
            if (nrm_cnt != 0u) {
                std::memcpy(vm_base + buf_nrm + i * 12u, vm_base + src_nrm + i * 16u, 12u);
            }
        }
        cv_call_direct_u32(ctx, CV_FD_FN_XFORM_VEC3, k16E10Mat2, buf_p2, buf_p1, pos_cnt);
        cv_call_direct_u32(ctx, CV_FD_FN_XFORM_VEC3, sp_mat, buf_p1, buf_p2, pos_cnt);
    } else {
        for (uint32_t i = 0; i < pos_cnt; ++i) {
            const uint8_t* const sp = vm_base + src_pos + i * 16u;
            uint8_t* const d1       = vm_base + buf_p1 + i * 12u;
            uint8_t* const d2       = vm_base + buf_p2 + i * 12u;
            const float x  = read_bef32_fast(sp + 0u);
            const float ny = -read_bef32_fast(sp + 4u);
            const float nz = -read_bef32_fast(sp + 8u);
            write_bef32_fast(d1 + 0u, x);  write_bef32_fast(d2 + 0u, x);
            write_bef32_fast(d1 + 4u, ny); write_bef32_fast(d2 + 4u, ny);
            write_bef32_fast(d1 + 8u, nz); write_bef32_fast(d2 + 8u, nz);
            if (nrm_cnt != 0u) {
                const uint8_t* const sn = vm_base + src_nrm + i * 16u;
                uint8_t* const dn       = vm_base + buf_nrm + i * 12u;
                write_bef32_fast(dn + 0u,  read_bef32_fast(sn + 0u));
                write_bef32_fast(dn + 4u, -read_bef32_fast(sn + 4u));
                write_bef32_fast(dn + 8u, -read_bef32_fast(sn + 8u));
            }
        }
        cv_call_direct_u32(ctx, CV_FD_FN_XFORM_VEC3, sp_mat, buf_p1, buf_p2, pos_cnt);
    }

    if (nrm_cnt != 0u) {
        uint8_t* const md = vm_base + sp_mat;
        write_be32_fast(md + 0x30u, 0u);
        write_be32_fast(md + 0x34u, 0u);
        write_be32_fast(md + 0x38u, 0u);
        write_be32_fast(md + 0x3Cu, 0x3F800000u);
        cv_call_direct_u32(ctx, CV_FD_FN_XFORM_VEC3, sp_mat, buf_nrm, buf_nrm, nrm_cnt);
    }

    const uint32_t mesh_cnt = read_be32_fast(vm_base + obj + 0x20u);
    if (mesh_cnt == 0u || obj == 0u) return;

    const float f31         = read_bef32_fast(kc + 0x08u);
    const float f30         = read_bef32_fast(kc + 0x0Cu);
    const float f_def_alpha = read_bef32_fast(kc + 0x28u);
    const float f_16c_alpha = read_bef32_fast(kc + 0x2Cu);
    const float f_half      = read_bef32_fast(kc + 0x30u);
    const int32_t sentinel  = (int32_t)read_be32_fast(vm_base + kDepthLo + 8u);
    const uint8_t* const bb_min_init = vm_base + k16E10BboxMin;
    const uint8_t* const bb_max_init = vm_base + (k16E10BboxMin - 0x10u);

    const uint32_t mesh_arr = read_be32_fast(vm_base + obj + 0x2Cu);
    for (uint32_t m = 0; m < mesh_cnt; ++m) {
        const uint32_t mesh    = read_be32_fast(vm_base + mesh_arr + m * 4u);
        const uint32_t sub_cnt = read_be32_fast(vm_base + mesh + 0x04u);
        const uint32_t mesh8   = mesh + 8u;
        const int32_t  mesh24  = (int32_t)read_be32_fast(vm_base + mesh + 0x24u);

        for (uint32_t s = 0; s < sub_cnt; ++s) {
            const uint32_t sub_arr = read_be32_fast(vm_base + mesh + 0x28u);
            const uint32_t sub_ent = sub_arr + s * 0x10u;
            const uint8_t  flag_a0 = vm_base[sub_ent + 4u];
            uint8_t        flag18  = vm_base[sub_ent + 5u];
            const uint32_t vtx_cnt = read_be32_fast(vm_base + sub_ent + 0u);

            const uint32_t vtx64_buf = cv_call_direct_u32(ctx, CV_FD_FN_BUMP_ALLOC, 1u, vtx_cnt * 64u);
            if (vtx64_buf == 0u) continue;

            float bb_min[3] = {
                read_bef32_fast(bb_min_init + 0u),
                read_bef32_fast(bb_min_init + 4u),
                read_bef32_fast(bb_min_init + 8u)
            };
            float bb_max[3] = {
                read_bef32_fast(bb_max_init + 0u),
                read_bef32_fast(bb_max_init + 4u),
                read_bef32_fast(bb_max_init + 8u)
            };

            std::memset(vm_base + sp_state, 0, 8u);
            cv_call_direct_u32_5(ctx, CV_FD_FN_RENDER_STATE, mesh,
                                 read_be32_fast(vm_base + obj + 0x44u),
                                 read_be32_fast(vm_base + obj + 0x48u),
                                 read_be32_fast(vm_base + obj + 0x4Cu),
                                 sp_state);

            if (vtx_cnt == 0u) continue;

            uint8_t all_transparent = 1u;
            if (flag_a0 != 0u) {
                write_be32_fast(vm_base + obj + 0x44u, read_be32_fast(vm_base + obj + 0x44u) | 2u);
            }

            const uint32_t idx_arr  = read_be32_fast(vm_base + sub_ent + 8u);
            const uint32_t uv_arr   = read_be32_fast(vm_base + sub_ent + 0x0Cu);
            const uint8_t  has_vcol = vm_base[geom + 0x14u];
            const uint32_t col_arr  = has_vcol ? read_be32_fast(vm_base + geom + 4u) : 0u;
            const int32_t  obj48    = (int32_t)read_be32_fast(vm_base + obj + 0x48u);
            const uint32_t blend_md = (read_be32_fast(vm_base + sp_state) >> 16) & 0x7FFu;
            const bool     do_tint  = (flags8 == 0x80000000u) && (blend_md == 0x12u || blend_md == 1u)
                                      && (has_tint != 0u)
                                      && !(obj48 == 0x44 || obj48 == 0x5E || obj48 == 0x5F
                                           || obj48 == 0xB2 || obj48 == 0x100 || obj48 == 0x18D);

            for (uint32_t k = 0; k < vtx_cnt; ++k) {
                uint8_t* const dp = vm_base + vtx64_buf + k * 0x40u;
                const uint32_t v_idx = idx_arr ? read_be32_fast(vm_base + idx_arr + k * 4u)
                                               : read_be32_fast(vm_base);
                const uint8_t* const pp = vm_base + buf_p2 + v_idx * 12u;
                std::memcpy(dp, pp, 12u);

                if (flags8 == 0x80000000u) {
                    const float px = read_bef32_fast(pp + 0u);
                    const float py = read_bef32_fast(pp + 4u);
                    const float pz = read_bef32_fast(pp + 8u);
                    if (px < bb_min[0]) bb_min[0] = px;
                    if (py < bb_min[1]) bb_min[1] = py;
                    if (pz < bb_min[2]) bb_min[2] = pz;
                    if (px > bb_max[0]) bb_max[0] = px;
                    if (py > bb_max[1]) bb_max[1] = py;
                    if (pz > bb_max[2]) bb_max[2] = pz;
                }

                if (has_vcol != 0u) {
                    const uint32_t col = read_be32_fast(vm_base + col_arr + v_idx * 4u);
                    write_be32_fast(dp + 0x0Cu, col);
                    if (flags8 == 0x80000000u) {
                        if (obj48 == 0x18E) {
                            uint32_t a = (col >> 24) & 0xFFu;
                            if (a > 0x80u) {
                                a = (a + 0x40u > 0xFFu) ? 0xFFu : (a + 0x40u);
                                dp[0x0Cu] = (uint8_t)a;
                            }
                            float af = (float)cv_f2i64_trunc(((float)(uint8_t)a * f31) / f30);
                            if (af > f31) af = f31;
                            dp[0x0Cu] = (uint8_t)cv_f2i64_trunc(af);
                        } else if (obj48 == 0xDD) {
                            const uint32_t sys = get_sys();
                            if (vm_base[sys + 0x95u] == 9u && vm_base[sys + 0x96u] == 0x1Eu) {
                                dp[0x0Fu] = (uint8_t)(dp[0x0Fu] / 3u);
                                dp[0x0Eu] = (uint8_t)(dp[0x0Eu] / 3u);
                                dp[0x0Du] = (uint8_t)(dp[0x0Du] / 3u);
                            }
                        }
                    }
                } else {
                    float fa = f_def_alpha;
                    bool override_col = false;
                    if (flags8 == 0x80000000u) {
                        if (obj48 == 0x180 || obj48 == 0x0D || obj48 == 7 || obj48 == 0) {
                            override_col = true;
                        } else if (obj48 == 0x16C) {
                            override_col = true;
                            fa = f_16c_alpha;
                        }
                    } else if (flags8 == 0x40000000u) {
                        const int32_t o54 = (int32_t)read_be32_fast(vm_base + obj + 0x54u);
                        if (o54 == -16) {
                            override_col = true;
                            fa = f31;
                        } else if ((o54 > -1 && o54 <= 1) || o54 == 0x8B) {
                            override_col = true;
                        }
                    }
                    if (override_col) {
                        vm_base[mesh8 + 0x13u] = 0xCDu;
                        vm_base[mesh8 + 0x12u] = 0xCDu;
                        vm_base[mesh8 + 0x11u] = 0xCDu;
                        vm_base[mesh8 + 0x10u] = (uint8_t)cv_f2i64_trunc(fa);
                    }
                    std::memcpy(dp + 0x0Cu, vm_base + mesh8 + 0x10u, 4u);
                }

                write_be16_fast(dp + 0x14u, 0x0100u);

                if (do_tint) {
                    float r = ((float)dp[0x0Fu] * tint_r) / f31;
                    if (r > f31) r = f31;
                    dp[0x0Fu] = (uint8_t)cv_f2i32_trunc(r);

                    float g = ((float)dp[0x0Eu] * tint_g) / f31;
                    if (g > f31) g = f31;
                    dp[0x0Eu] = (uint8_t)cv_f2i32_trunc(g);

                    float b = ((float)dp[0x0Du] * tint_b) / f31;
                    if (b > f31) b = f31;
                    dp[0x0Du] = (uint8_t)cv_f2i32_trunc(b);
                }

                if (mesh24 == 0 && flag18 == 1u) {
                    const uint32_t uv_ptr = uv_arr ? (uv_arr + k * 0x0Cu) : 0u;
                    const int32_t  tex_id = (int32_t)read_be32_fast(vm_base + uv_ptr + 8u);
                    if (tex_id == -1) {
                        const uint32_t direct_tex = read_be32_fast(vm_base + self + 0x174u);
                        if (direct_tex != 0u && id24 == 0x6F) {
                            cv_call_direct_u32_5(ctx, CV_FD_FN_SET_TEX_DIRECT, sc, direct_tex, 0u, 0u, 0u);
                            std::memcpy(dp + 0x18u, vm_base + uv_ptr, 8u);
                        } else {
                            flag18 = 0u;
                        }
                    } else {
                        const uint32_t tflag   = read_be32_fast(vm_base + obj + 0x44u) & 0xFFFF0000u;
                        const uint32_t tlist   = read_be32_fast(vm_base + obj + 0x30u);
                        const uint32_t tex_res = cv_call_direct_u32(ctx, CV_FD_FN_TEX_LOOKUP,
                                                                    self, tflag, tlist, (uint32_t)tex_id);
                        if (tex_res != 0u) {
                            cv_call_direct_u32_5(ctx, CV_FD_FN_DESC_LOOKUP, sc, tex_res, 0u, 0u, 0u);
                            std::memcpy(dp + 0x18u, vm_base + uv_ptr, 8u);
                        } else {
                            flag18 = 0u;
                        }
                    }
                }

                if (dp[0x0Cu] > 8u) all_transparent = 0u;
            }

            if (all_transparent != 0u) continue;

            cv_call_direct_u32_5(ctx, CV_FD_FN_RENDER_STATE, mesh,
                                 read_be32_fast(vm_base + obj + 0x44u),
                                 read_be32_fast(vm_base + obj + 0x48u),
                                 read_be32_fast(vm_base + obj + 0x4Cu),
                                 sp_state);

            if (mesh24 == 0 || mesh24 == 1 || mesh24 == 2) {
                int32_t depth = sentinel;
                if (flags8 == 0x80000000u) {
                    write_bef32_fast(vm_base + sp_center + 0u, (bb_min[0] + bb_max[0]) * f_half);
                    write_bef32_fast(vm_base + sp_center + 4u, (bb_min[1] + bb_max[1]) * f_half);
                    write_bef32_fast(vm_base + sp_center + 8u, (bb_min[2] + bb_max[2]) * f_half);
                    write_bef32_fast(vm_base + sp_center + 12u, 0.0f);
                    depth = (int32_t)cv_call_direct_u32(ctx, CV_FD_FN_DEPTH_KEY, self, arg4, sc, sp_center);
                }

                if (mesh24 == 0) {
                    if (flag18 != 0u) {
                        cv_call_direct_u32_8(ctx, CV_FD_FN_PACK_VTX64, sc, vtx64_buf, vtx_cnt, sp_state,
                                             0u, (uint32_t)depth, 0u, 0u);
                    } else {
                        cv_call_direct_u32_8(ctx, CV_FD_FN_PACK_VTX_NOTEX, sc, vtx64_buf, vtx_cnt, sp_state,
                                             0u, (uint32_t)depth, 0u, 0u);
                    }
                } else {
                    cv_call_direct_u32_8(ctx, CV_FD_FN_PACK_VTX_ALT, sc, vtx64_buf, vtx_cnt, sp_state,
                                         0u, (uint32_t)depth, 0u, 0u);
                }
            }
        }
    }
}

/* Hook 36: 4-byte global parameter sync (EU/US/JP 0x000102AC) */
void CV_FD_FN_SYNC_PARAM(ppu_context* ctx)
{
    if (!native_hot_enabled() || !vm_base) {
        CV_FD_ORIG_SYNC_PARAM(ctx);
        return;
    }
    std::memcpy(vm_base + kSyncParamDst, vm_base + kSyncParamSrc, 4u);
}

/* Hook 37: Effect list dispatcher (EU/US/JP 0x00017EB4) */
void CV_FD_FN_DRAW_EFF_LIST(ppu_context* ctx)
{
    if (!native_hot_enabled() || !vm_base) {
        CV_FD_ORIG_DRAW_EFF_LIST(ctx);
        return;
    }

    const uint32_t self    = (uint32_t)ctx->gpr[3];
    const uint32_t arg4    = (uint32_t)ctx->gpr[4];
    const uint32_t arg5    = (uint32_t)ctx->gpr[5];
    const uint32_t mat_ptr = (uint32_t)ctx->gpr[6];
    const uint32_t list    = (uint32_t)ctx->gpr[7];

    const uint32_t cnt = read_be32_fast(vm_base + list + 4u);
    if (cnt == 0u) return;

    for (uint32_t i = 0; i < cnt; ++i) {
        const uint32_t arr  = read_be32_fast(vm_base + list + 0x10u);
        const uint32_t item = read_be32_fast(vm_base + arr + i * 4u);
        if (item == 0u) continue;
        const uint32_t flags8 = read_be32_fast(vm_base + item + 0x44u) & 0xFFFF0000u;
        if (flags8 == 0x80000000u || flags8 == 0x40000000u) {
            if ((read_be16_fast(vm_base + arg4 + 0x19Au) & 0x0Cu) == 0u) {
                ctx->gpr[3] = self;
                ctx->gpr[4] = arg4;
                ctx->gpr[5] = arg5;
                ctx->gpr[6] = mat_ptr;
                ctx->gpr[7] = item;
                ctx->gpr[8] = flags8;
                CV_FD_FN_DRAW_EFFECT(ctx);
            }
        } else {
            cv_call_direct_u32_8(ctx, CV_FD_FN_DRAW_EFF_ALT, self, arg4, arg5, mat_ptr, item, flags8, 0u, 0u);
        }
    }
}

/* Hook 38: Open-addressing hash table find (US 0x002680BC / EU 0x0026A08C / JP 0x002689AC) */
void CV_FD_FN_HASH_FIND(ppu_context* ctx)
{
    if (!native_hot_enabled() || !vm_base) {
        CV_FD_ORIG_HASH_FIND(ctx);
        return;
    }
    ctx->gpr[3] = (int64_t)cv_hash_find_inline((uint32_t)ctx->gpr[3], (uint32_t)ctx->gpr[4]);
}

/* Hook 39: Hash table get value by key (US 0x00267DC4 / EU 0x00269D94 / JP 0x002686B4) */
void CV_FD_FN_HASH_GET(ppu_context* ctx)
{
    if (!native_hot_enabled() || !vm_base) {
        CV_FD_ORIG_HASH_GET(ctx);
        return;
    }
    ctx->gpr[3] = cv_hash_get_inline(ctx, (uint32_t)ctx->gpr[3], (uint32_t)ctx->gpr[4]);
}

/* Hook 40: Mesh vertex buffer upload (EU/US/JP 0x00019A94) */
void CV_FD_FN_UPLOAD_MESH(ppu_context* ctx)
{
    if (!native_hot_enabled() || !vm_base) {
        CV_FD_ORIG_UPLOAD_MESH(ctx);
        return;
    }

    const uint32_t arg4 = (uint32_t)ctx->gpr[4];
    const uint32_t mesh = (uint32_t)ctx->gpr[5];

    uint32_t src = read_be32_fast(vm_base + mesh + 0x34u);
    if (src == 0u) return;

    const uint32_t cnt = read_be32_fast(vm_base + mesh + 4u);
    for (uint32_t i = 0; i < cnt; ++i) {
        const uint32_t cur_cnt = read_be32_fast(vm_base + mesh + 4u);
        uint32_t vcnt = (cur_cnt > i)
            ? read_be32_fast(vm_base + read_be32_fast(vm_base + mesh + 0x28u) + i * 0x10u)
            : 0u;
        if (read_be32_fast(vm_base + mesh + 0x3Cu) == 3u) {
            vcnt = vcnt * 3u - 6u;
        }
        const uint32_t g      = read_be32_fast(vm_base + kGlobalObj);
        const uint32_t stride = read_be32_fast(vm_base + read_be32_fast(vm_base + g + 0x208u) + 0x20u);
        const uint32_t dst    = cv_call_direct_u32(ctx, CV_FD_FN_DRAW_DESC_ALLOC, arg4, vcnt, stride);
        if (dst != 0u) {
            const uint32_t bytes = vcnt * stride;
            std::memcpy(vm_base + dst, vm_base + src, bytes);
            src += bytes;
        }
    }
}
