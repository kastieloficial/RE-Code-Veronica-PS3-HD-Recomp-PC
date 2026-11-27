#pragma once
/* The return-to-title box's drawing (900x440 design units, the approved previews docs/previas-v1.7/titulo-*.png).
 * cv_title_return.cpp owns the window, the input and the guest side: it fills a View each paint; this file only draws and
 * knows where each control is. Paint and hit testing share the named rectangles of `layout`. */
#include <string>
#include <vector>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "cv_port_ui.h"

namespace cv_title_return_ui {
constexpr int W = 900, H = 440;
enum Choice { CHOICE_YES = 0, CHOICE_NO = 1 };   // No on the left, Yes on the right; No is the one chosen when the box opens

namespace layout {
using Gdiplus::RectF;
inline const RectF BAND(3, 3, W - 6, 56), HEADER(0, 0, W, 62), QUESTION(40, 96, W - 80, 48), WARNING(130, 176, W - 260, 62),
    NO(130, 268, 310, 62), YES(460, 268, 310, 62), FOOTER(24, H - 62, W - 48, 42);
}

enum HitKind { HIT_YES = 0, HIT_NO = 1, HIT_BACK = 2 };   // also the MouseTrack ids
struct Hit { Gdiplus::RectF rect; int kind; };

struct View {
    int language = 1, input = PAD_INPUT_KEYBOARD;   // input: the last device used (picks the footer glyphs)
    int selected = CHOICE_NO;
    cv_port_ui::MouseTrack mouse;                   // hover / pressed look of Yes, No and the footer's Back
    mutable Gdiplus::RectF back_rect = Gdiplus::RectF(0, 0, 0, 0);   // the footer's "Back" group, set by draw()
};

// What draw() lays out for a language: the font sizes after fitting and the widths they give (the 13-language no-clipping test reads this).
struct Metrics {
    float header_px = 0, header_w = 0, header_max = 0;
    float question_px = 0, question_w = 0, question_max = 0;
    float warning_px = 0, warning_w = 0, warning_max = 0;
    float label_px = 0, yes_w = 0, no_w = 0, label_max = 0;
    static constexpr float MIN_PX = 13;
};
Metrics measure(Gdiplus::Graphics& g, const View& view);

std::vector<Hit> hits(const View& view);
const Hit* hit_at(const std::vector<Hit>& list, float x, float y);   // the last hit containing the point
void draw(Gdiplus::Graphics& g, const View& view);                   // the caller scales the Graphics to the window's pixels
}
