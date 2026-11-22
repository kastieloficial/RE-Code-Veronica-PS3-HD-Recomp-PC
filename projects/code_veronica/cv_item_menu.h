#pragma once
/* The F6 item menu over the game (cv_items.h has the rules and the write).
 * A window of its own, as the F7 map window: keyboard, controller and mouse,
 * the game ignoring the pad while it has the focus. */
#include <string>
#include <vector>
#include "cv_items.h"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include "cv_port_ui.h"

namespace cv_item_menu {
constexpr int W=1400,H=820;   // design units (docs/previas-v1.7/inventario-*.png), scaled to the window
enum Action { UP, DOWN, LEFT, RIGHT, PREV, NEXT, CONFIRM, BACK, REMOVE };
struct Hit { Gdiplus::RectF rect; int kind, value; };
enum HitKind { HIT_TAB, HIT_ITEM, HIT_SLOT, HIT_QTY_MINUS, HIT_QTY_PLUS, HIT_TARGET_PREV, HIT_TARGET_NEXT, HIT_PLACE, HIT_CLOSE };
// The id the window's MouseTrack uses for a hit (hover / pressed feedback follows it).
constexpr int hit_id(int kind,int value) {return kind*256+value;}
enum Tone { NEUTRAL, GOOD, WARN, BAD };
struct View {
    cv_items::Inventory inventory;
    int language=1;            // launcher_i18n.h Lang
    int category=0, selected=0, scroll=0;
    int focus=0;               // 0 the item grid, 1 the placement panel, 2 choosing an item to remove
    int input=PAD_INPUT_KEYBOARD;   // the last device used (pad_phys.h): picks the footer's glyphs via cv_port_ui::hint_kind
    cv_port_ui::MouseTrack mouse;   // hover / pressed look of the clickable controls (ids from hit_id)
    int slot=1;                // removing: the inventory entry chosen
    int row=1;                 // in the panel: 0 quantity, 1 space, 2 the button
    int quantity=1, target=0;  // target: 0 first free entry, else the entry replaced
    std::wstring status;Tone tone=NEUTRAL;
    unsigned serial=0;         // cv_items::outcomes() already shown
    std::vector<Hit> hits;     // filled by draw(), for the mouse
    int item() const;          // the selected item's id, or 0
};
// The entries the selected item may go to: 0 (the first free one) when it fits there, then every occupied
// entry but the equipped weapon's whose replacement leaves room (a wide weapon needs two spaces).
std::vector<int> targets(const View& view);
// The live inventory, with the items queued so far in place.
void refresh(View& view,const cv_items::Inventory& live);
// True when the action closes the menu (Esc on the item grid).
bool apply(View& view,Action action);
cv_items::Result submit(View& view);
// Outcomes the game frame wrote since the view last looked, as the status line.
void poll_result(View& view);
// What the game shows once the queued items went in (or did not).
std::wstring summary(int language,const std::vector<cv_items::Outcome>& done,bool& ok);
// The control under a point (design units), or nullptr: the first hit drawn there.
const Hit* hit_at(const View& view,float x,float y);
// Draws in design units: the caller scales the Graphics to the window.
void draw(Gdiplus::Graphics& g,View& view,const std::wstring& game_root);
// Frees the cached images: before GdiplusShutdown, where there is one.
void release();
}

void cv_item_menu_enable(bool on);
void cv_item_menu_toggle(const std::wstring& game_root,int language);   // F6
bool cv_item_menu_visible();
void cv_item_menu_close();
#endif
