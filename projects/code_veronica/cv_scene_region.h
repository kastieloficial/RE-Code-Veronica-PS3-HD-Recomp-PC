#pragma once
#include <cstdint>
// Addresses independently checked against each original EBOOT.elf.
// See docs/region-smoothing-map.json for matching native references.
#if defined(CV_REGION_JP)
inline constexpr uint32_t CV_SCENE_SYSTEM=0x00A79C34;
inline constexpr uint32_t CV_SCENE_CONFIG=0x00A79C38;
inline constexpr uint32_t CV_SCENE_ACTOR=0x00A79C3C;
inline constexpr uint32_t CV_SCENE_RENDER=0x00A73CD0;
inline constexpr uint32_t CV_SCENE_CAMERA_MANAGER=0x00AEB870;
inline constexpr uint32_t CV_SCENE_ROOM_MANAGER=0x00AEBD9C;
inline constexpr uint32_t CV_SCENE_CAMERA=0x00BAE108;
inline constexpr uint32_t CV_SCENE_VIEW_MATRIX=0x00CA7090;
inline constexpr uint32_t CV_SCENE_INVENTORY=0x00CA8920;
inline constexpr uint32_t CV_SCENE_CONTROLLER_VTABLE=0x008FBF20;
inline constexpr uint32_t CV_SCENE_MESH_VTABLE=0x008EE620;
inline constexpr uint32_t CV_SCENE_SPRITE_VTABLE=0x008EE378;
inline constexpr uint32_t CV_SCENE_CONTROLLER=0x00AB78C8;
inline constexpr uint32_t CV_SCENE_PSN=0x00AE9420;
inline constexpr uint32_t CV_SCENE_STARTUP=0x00AE9444;
inline constexpr uint32_t CV_SCENE_PSN_MODE=0x00BAE713;
#define CV_SCENE_FN_000155D0 func_000155D0
#define CV_SCENE_FN_00034478 func_00034020
#define CV_SCENE_FN_00146BE0 func_00146110
#define CV_SCENE_FN_00158620 func_00157A58
inline constexpr uint32_t CV_SCENE_PREPARE_CUT=0x001F2FB0;
inline constexpr uint32_t CV_SCENE_HD_UPDATE=0x0022AA28;
#define CV_SCENE_FN_001F4C40 func_001F2FB0
#define CV_SCENE_FN_0022B9F0 func_0022A310
#define CV_SCENE_FN_0022C108 func_0022AA28
#define CV_SCENE_FN_TIMER func_001CE5DC
#define CV_SCENE_FN_NOTICE func_001E3BBC
#define CV_SCENE_OG_NOTICE cv_original_001E3BBC
#define CV_SCENE_FN_PSN_DIALOG func_0020F264
#define CV_SCENE_OG_PSN_DIALOG cv_original_0020F264
#define CV_SCENE_FN_PANE_READY func_001EE7EC
#define CV_SCENE_FN_PSN_CLEANUP func_00269A90
#elif defined(CV_REGION_US)
inline constexpr uint32_t CV_SCENE_SYSTEM=0x00A79BD0;
inline constexpr uint32_t CV_SCENE_CONFIG=0x00A79BD4;
inline constexpr uint32_t CV_SCENE_ACTOR=0x00A79BD8;
inline constexpr uint32_t CV_SCENE_RENDER=0x00A73CD0;
inline constexpr uint32_t CV_SCENE_CAMERA_MANAGER=0x00AEB7F0;
inline constexpr uint32_t CV_SCENE_ROOM_MANAGER=0x00AEBD1C;
inline constexpr uint32_t CV_SCENE_CAMERA=0x00BAE088;
inline constexpr uint32_t CV_SCENE_VIEW_MATRIX=0x00CA7010;
inline constexpr uint32_t CV_SCENE_INVENTORY=0x00CA88A0;
inline constexpr uint32_t CV_SCENE_CONTROLLER_VTABLE=0x008FB670;
inline constexpr uint32_t CV_SCENE_MESH_VTABLE=0x008EDD70;
inline constexpr uint32_t CV_SCENE_SPRITE_VTABLE=0x008EDAC8;
inline constexpr uint32_t CV_SCENE_CONTROLLER=0x00AB7808;
inline constexpr uint32_t CV_SCENE_PSN=0x00AE93A0;
inline constexpr uint32_t CV_SCENE_STARTUP=0x00AE93C4;
inline constexpr uint32_t CV_SCENE_PSN_MODE=0x00BAE693;
#define CV_SCENE_FN_000155D0 func_000155D0
#define CV_SCENE_FN_00034478 func_00034078
#define CV_SCENE_FN_00146BE0 func_00145EE8
#define CV_SCENE_FN_00158620 func_00157830
inline constexpr uint32_t CV_SCENE_PREPARE_CUT=0x001F2CA0;
inline constexpr uint32_t CV_SCENE_HD_UPDATE=0x0022A138;
#define CV_SCENE_FN_001F4C40 func_001F2CA0
#define CV_SCENE_FN_0022B9F0 func_00229A20
#define CV_SCENE_FN_0022C108 func_0022A138
#define CV_SCENE_FN_TIMER func_001CE45C
#define CV_SCENE_FN_NOTICE func_001E395C
#define CV_SCENE_OG_NOTICE cv_original_001E395C
#define CV_SCENE_FN_PSN_DIALOG func_0020E974
#define CV_SCENE_OG_PSN_DIALOG cv_original_0020E974
#define CV_SCENE_FN_PANE_READY func_001EE4DC
#define CV_SCENE_FN_PSN_CLEANUP func_002691A0
#else
inline constexpr uint32_t CV_SCENE_SYSTEM=0x00A89BD0;
inline constexpr uint32_t CV_SCENE_CONFIG=0x00A89BD4;
inline constexpr uint32_t CV_SCENE_ACTOR=0x00A89BD8;
inline constexpr uint32_t CV_SCENE_RENDER=0x00A83CD0;
inline constexpr uint32_t CV_SCENE_CAMERA_MANAGER=0x00AFBAF0;
inline constexpr uint32_t CV_SCENE_ROOM_MANAGER=0x00AFC01C;
inline constexpr uint32_t CV_SCENE_CAMERA=0x00BBE388;
inline constexpr uint32_t CV_SCENE_VIEW_MATRIX=0x00CB7310;
inline constexpr uint32_t CV_SCENE_INVENTORY=0x00CB8BA0;
inline constexpr uint32_t CV_SCENE_CONTROLLER_VTABLE=0x008FE020;
inline constexpr uint32_t CV_SCENE_MESH_VTABLE=0x008F0720;
inline constexpr uint32_t CV_SCENE_SPRITE_VTABLE=0x008F0478;
inline constexpr uint32_t CV_SCENE_CONTROLLER=0x00AC7B58;
inline constexpr uint32_t CV_SCENE_PSN=0x00AF96A0;
inline constexpr uint32_t CV_SCENE_STARTUP=0x00AF96C4;
inline constexpr uint32_t CV_SCENE_PSN_MODE=0x00BBE993;
#define CV_SCENE_FN_000155D0 func_000155D0
#define CV_SCENE_FN_00034478 func_00034478
#define CV_SCENE_FN_00146BE0 func_00146BE0
#define CV_SCENE_FN_00158620 func_00158620
inline constexpr uint32_t CV_SCENE_PREPARE_CUT=0x001F4C40;
inline constexpr uint32_t CV_SCENE_HD_UPDATE=0x0022C108;
#define CV_SCENE_FN_001F4C40 func_001F4C40
#define CV_SCENE_FN_0022B9F0 func_0022B9F0
#define CV_SCENE_FN_0022C108 func_0022C108
#define CV_SCENE_FN_TIMER func_001D027C
#define CV_SCENE_FN_NOTICE func_001E58FC
#define CV_SCENE_OG_NOTICE cv_original_001E58FC
#define CV_SCENE_FN_PSN_DIALOG func_00210914
#define CV_SCENE_OG_PSN_DIALOG cv_original_00210914
#define CV_SCENE_FN_PANE_READY func_001F047C
#define CV_SCENE_FN_PSN_CLEANUP func_0026B170
#endif
