#pragma once
/* The F7 archives window's drawing (Load / Save / Maps), in design units (1400x820, the approved previews
 * docs/previas-v1.7/arquivos-*.png). cv_save_library.cpp owns the state and the guest side: it fills a View each
 * paint; this file only draws and knows where each control is. Paint and hit testing share the named rectangles of
 * `layout`, so they cannot drift apart. */
#include <string>
#include <vector>
#include <cstdint>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "cv_port_ui.h"

namespace cv_archives_ui {
constexpr int W = 1400, H = 820;
enum Page { PAGE_LOAD = 0, PAGE_SAVE = 1, PAGE_MAPS = 2, PAGE_COUNT = 3 };   // tab order: Load, Save, Maps
constexpr int ROWS_VISIBLE = 5;
constexpr unsigned THUMB_W = 320, THUMB_H = 180;   // the saved pictures (ARGB)

namespace layout {
using Gdiplus::RectF;
constexpr float TAB_W = (W - 48 - 92 - 20) / 3.0f;   // 413.3
inline RectF tab(int i) { return RectF(70 + i * (TAB_W + 10), 80, TAB_W, 44); }
inline const RectF HEADER_BAND(3, 3, W - 6, 65);
inline const RectF TAB_PREV(24, 88, 30, 28), TAB_NEXT(W - 24 - 30, 88, 30, 28);   // the Q / E (L1 / R1) glyphs
inline const RectF FILTER_BOX(24, 136, 430, 40), FILTER_EDIT(66, 145, 374, 22);   // the search field; the EDIT sits inside the frame
inline const RectF LIBRARY_LABEL(24, 136, 820, 40), NOW_ROOM(880, 136, 496, 40);
constexpr float LIST_Y = 186, ROW_H = 104, ROW_STEP = 112;
inline RectF row(int slot) { return RectF(24, LIST_Y + slot * ROW_STEP, 820, ROW_H); }
inline const RectF SCROLL_TRACK(852, LIST_Y, 5, ROWS_VISIBLE * ROW_STEP - 8);
inline const RectF PREVIEW(880, 186, 496, 280), PREVIEW_MAPS(880, 186, 496, 250);
inline const RectF HEADING(880, 480, 496, 29), HEADING_MAPS(880, 446, 496, 29), NOTE_MAPS(880, 482, 496, 23), NOTE_NEW(880, 520, 496, 46);
inline const RectF DETAIL_CHARACTER(880, 524, 496, 24), DETAIL_TIME(880, 554, 496, 24);
// Maps: the entry stepper (label, minus, value, plus, the arrows glyph) and the explanation under it.
inline const RectF STEPPER(880, 526, 496, 40), STEP_MINUS(1085, 531, 30, 30), STEP_VALUE(1121, 531, 120, 30), STEP_PLUS(1247, 531, 30, 30),
    STEP_HINT(1301, 531, 59, 30), ENTRY_HINT(880, 578, 496, 46);
inline const RectF PRIMARY_MAPS(880, 650, 496, 46), PRIMARY(880, 652, 496, 46), SECONDARY(880, 600, 496, 42);
inline const RectF MESSAGE_MAPS(880, 710, 496, 44), MESSAGE(880, 712, 496, 44);
inline const RectF FOOTER(24, 776, 1352, 40);
// The confirmation dialog (overwrite): over a dimmed window.
inline const RectF DIALOG(300, 260, 800, 300), DIALOG_TITLE(343, 297, 714, 42), DIALOG_TEXT(363, 363, 674, 70),
    DIALOG_CANCEL(363, 459, 310, 56), DIALOG_YES(713, 459, 310, 56);
}

enum class Tone { NEUTRAL, GOOD, WARN, BAD };
enum HitKind { HIT_TAB, HIT_TAB_PREV, HIT_TAB_NEXT, HIT_ROW, HIT_STEP_MINUS, HIT_STEP_PLUS, HIT_PRIMARY, HIT_SECONDARY, HIT_BACK, HIT_DIALOG_CANCEL, HIT_DIALOG_YES };
constexpr int hit_id(int kind, int value = 0) { return kind * 65536 + value; }
struct Hit { Gdiplus::RectF rect; int kind, value; };

// One row of the list, ready to draw.
struct Row {
    enum Kind { MAP, SAVE, NEW_SAVE } kind = MAP;
    std::wstring title;        // MAP: rm_XXXX; SAVE: the character
    std::wstring where;        // MAP: "area 0 · room 00 · rm_0000"; SAVE: the room
    std::wstring event;        // MAP: "event version 1" (empty: none)
    std::wstring when;         // SAVE: the date
    const uint32_t* thumb = nullptr;   // SAVE: THUMB_W x THUMB_H ARGB, valid during the draw
    bool original = false;     // SAVE: an original DATA slot (no picture)
};
struct View {
    int language = 1, page = PAGE_LOAD, input = PAD_INPUT_KEYBOARD;
    bool unavailable = false;                 // no game to act on (neither gameplay nor the title menu)
    int count = 0, scroll = 0, selected = 0;  // all rows, the first visible, the chosen
    std::vector<Row> rows;                    // the visible ones: scroll .. scroll+ROWS_VISIBLE
    int files = 0;                            // the library's file count (Load / Save)
    std::wstring now_room;                    // Maps: the room the character is in, "rm_0000", or empty
    int spawn = 0;                            // Maps: the entry
    // The right column.
    std::wstring heading, note;               // the room label; Maps: the story note, New save: its hint
    std::wstring tag;                         // Maps: rm_XXXX over the picture
    const uint32_t* image = nullptr;          // the picture (THUMB_W x THUMB_H ARGB)
    std::wstring placeholder;                 // shown instead of a picture
    std::wstring character, play_time;        // Load / Save details (empty: hidden)
    bool primary_enabled = true, can_overwrite = false, busy = false;
    bool confirming = false, confirm_yes = false, search_focus = false;   // search_focus: the search field has the keyboard
    std::wstring search_cue;                  // previews only: the placeholder text (the window's EDIT draws its own)
    std::wstring status;                      // under the main button; empty: the page's own note
    Tone tone = Tone::NEUTRAL;
    cv_port_ui::MouseTrack mouse;
    mutable Gdiplus::RectF back_rect = Gdiplus::RectF(0, 0, 0, 0);   // the footer's "Back" group, set by draw()
};
// The controls under the current state (a dialog hides everything behind it), in draw order.
std::vector<Hit> hits(const View& view);
const Hit* hit_at(const std::vector<Hit>& list, float x, float y);   // the last hit containing the point
// Draws the whole window; the caller scales the Graphics to the window's pixels.
void draw(Gdiplus::Graphics& g, const View& view);
}
