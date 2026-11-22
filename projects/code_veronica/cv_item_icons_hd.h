#pragma once
/* The F6 item menu's HD icons: assets/items-hd/item_XX.png (cropped from the community HD texture pack by
 * tools/extract_item_icons_hd.py, committed), embedded as RCDATA "ITEM_HD_XX" (XX = the item id in hex).
 * The menu falls back to the icon read from the selected game copy (cv_srt_icons) for an id without one. */
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
namespace cv_item_icons_hd {
// A cached GDI+ bitmap with alpha, or nullptr. Needs GdiplusStartup done. Never freed except by release().
Gdiplus::Bitmap* get(int id);
void release();   // before GdiplusShutdown
}
#endif
