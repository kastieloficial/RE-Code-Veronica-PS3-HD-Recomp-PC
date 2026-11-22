#pragma once
/* Shared vector artwork of the port's UI: the controller button glyphs and keycaps the launcher draws,
 * moved out of launcher.cpp unchanged so the in-game menus (F6/F7/title dialog) draw the very same icons. */
#include <algorithm>
#include <cmath>
#include <string>
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include "../../libs/input/pad_phys.h"
#include "../../libs/input/analog_bindings.h"
#include "cv_keycap_style.h"

/* button_icons.cpp: the keycaps' short labels, shared with the game. */
std::wstring cv_key_cap_label(int vk);

namespace cv_port_glyphs {
using namespace Gdiplus;

inline GraphicsPath* round_rect(RectF r, float rad)
{
    auto* p = new GraphicsPath();
    float d = rad * 2;
    p->AddArc(r.X, r.Y, d, d, 180, 90);
    p->AddArc(r.X + r.Width - d, r.Y, d, d, 270, 90);
    p->AddArc(r.X + r.Width - d, r.Y + r.Height - d, d, d, 0, 90);
    p->AddArc(r.X, r.Y + r.Height - d, d, d, 90, 90);
    p->CloseFigure();
    return p;
}

/* Controller button icons, drawn as vectors on a transparent background in
 * one style for both layouts: face buttons are a ring with the symbol or
 * letter in the controller's colour, shoulders/triggers are light shapes with
 * dark labels, sticks are rings, the d-pad is a cross with the pressed arm
 * lit. layout 0 = PlayStation, 1 = Xbox, 2 = Switch; pb is a PB_* position. */
inline const Color G_LINE(255, 205, 198, 190), G_FILL(255, 218, 212, 204), G_INK(255, 28, 22, 22),
            G_DIM(255, 92, 84, 80);

/* Centred on the glyphs' own outline: DrawString pads text with side bearings,
 * which pushed short labels like "R3" or "A" visibly off centre. */
inline void g_label(Graphics& gr, const wchar_t* t, RectF r, float px, const Color& c)
{
    FontFamily ff(L"Segoe UI");
    StringFormat sf(StringFormat::GenericTypographic());
    GraphicsPath p;
    p.AddString(t, -1, &ff, FontStyleBold, px, PointF(0, 0), &sf);
    RectF bb; p.GetBounds(&bb);
    Matrix m; m.Translate(r.X + (r.Width - bb.Width) / 2 - bb.X, r.Y + (r.Height - bb.Height) / 2 - bb.Y);
    p.Transform(&m);
    SolidBrush b(c);
    gr.FillPath(&b, &p);
}
/* A keycap like the ones the game shows in keyboard mode (button_icons.cpp):
 * light grey-blue, dark border, the key's short label. */
inline void key_glyph(Graphics& gr, int vk, float x, float y, float s)
{
    cv_keycap::draw(gr, vk > 0 ? cv_key_cap_label(vk) : L"", x, y, s, s);
}

/* pb = BTN_DPAD_NEUTRAL: the whole d-pad, no direction lit (footers say "use the d-pad", not "press up"). */
constexpr int BTN_DPAD_NEUTRAL = -2;

inline void btn_glyph(Graphics& gr, int layout, int pb, float x, float y, float s = 30.0f)
{
    const float cx = x + s / 2, cy = y + s / 2, k = s / 26.0f;
    /* every icon sits on the same dark disc with a light ring */
    {
        const float r = 12.0f * k;
        SolidBrush disc(Color(255, 40, 34, 34)); gr.FillEllipse(&disc, cx - r, cy - r, 2 * r, 2 * r);
        Pen ring(G_LINE, 1.5f * k); gr.DrawEllipse(&ring, cx - r, cy - r, 2 * r, 2 * r);
    }
    if(pb>=CV_AXIS_FIRST&&pb<CV_BINDING_COUNT){
        int n=pb-CV_AXIS_FIRST,d=n%4;
        const float dx=d==2?-1.f:d==3?1.f:0.f,dy=d==0?-1.f:d==1?1.f:0.f;
        Pen inner(G_LINE,1.1f*k);gr.DrawEllipse(&inner,cx-6*k,cy-6*k,12*k,12*k);
        g_label(gr,n<4?L"L":L"R",RectF(cx-5*k,cy-5*k,10*k,10*k),7*k,G_FILL);
        PointF tip(cx+dx*10*k,cy+dy*10*k),base(cx+dx*7*k,cy+dy*7*k);
        PointF tri[]={tip,PointF(base.X-dy*2*k,base.Y+dx*2*k),PointF(base.X+dy*2*k,base.Y-dx*2*k)};
        SolidBrush arrow(G_FILL);gr.FillPolygon(&arrow,tri,3);return;
    }
    SolidBrush fill(G_FILL);
    switch (pb) {
    case PB_SOUTH: case PB_EAST: case PB_WEST: case PB_NORTH: {
        static const Color ps[4] = { Color(255, 124, 178, 240), Color(255, 255, 102, 110),
                                     Color(255, 222, 140, 210), Color(255, 70, 220, 170) };
        static const Color xb[4] = { Color(255, 110, 200, 80), Color(255, 240, 80, 70),
                                     Color(255, 70, 150, 245), Color(255, 245, 200, 50) };
        const int i = pb - PB_SOUTH;
        if (layout == 2) {           /* Switch: one cream colour, B at the bottom, A on the right */
            static const wchar_t* L[4] = { L"B", L"A", L"Y", L"X" };
            g_label(gr, L[i], RectF(x, y, s, s), 12.0f * k, Color(255, 236, 228, 212));
            break;
        }
        if (layout == 1) {
            static const wchar_t* L[4] = { L"A", L"B", L"X", L"Y" };
            /* B's bowls sit right of its stem: centred by outline it still
             * reads left, so it gets a small optical nudge */
            const float nudge = (pb == PB_EAST) ? 0.7f * k : 0.0f;
            g_label(gr, L[i], RectF(x + nudge, y, s, s), 12.0f * k, xb[i]);
            break;
        }
        Pen p(ps[i], 1.9f * k); p.SetLineJoin(LineJoinRound); p.SetStartCap(LineCapRound); p.SetEndCap(LineCapRound);
        const float a = 5.2f * k;
        if (pb == PB_SOUTH) { gr.DrawLine(&p, cx - a, cy - a, cx + a, cy + a); gr.DrawLine(&p, cx - a, cy + a, cx + a, cy - a); }
        else if (pb == PB_EAST) gr.DrawEllipse(&p, cx - a, cy - a, 2 * a, 2 * a);
        else if (pb == PB_WEST) gr.DrawRectangle(&p, cx - a + 0.5f * k, cy - a + 0.5f * k, 2 * a - k, 2 * a - k);
        else { PointF tri[3] = { PointF(cx, cy - a - 0.5f * k), PointF(cx + a + 0.5f * k, cy + a * 0.8f),
                                 PointF(cx - a - 0.5f * k, cy + a * 0.8f) };
               gr.DrawPolygon(&p, tri, 3); }
        break; }
    case PB_L1: case PB_R1: {        /* bumper: a low rounded bar */
        RectF r(cx - 8.5f * k, cy - 4.5f * k, 17 * k, 9 * k);
        GraphicsPath* p = round_rect(r, 4 * k); gr.FillPath(&fill, p); delete p;
        g_label(gr, layout == 1 ? (pb == PB_L1 ? L"LB" : L"RB") : layout == 2 ? (pb == PB_L1 ? L"L" : L"R")
                                : (pb == PB_L1 ? L"L1" : L"R1"), r, 7.5f * k, G_INK);
        break; }
    case PB_L2: case PB_R2: {        /* trigger: tall, rounded on top */
        RectF r(cx - 6 * k, cy - 7.5f * k, 12 * k, 15 * k);
        GraphicsPath p;
        const float d = 10 * k;
        p.AddArc(r.X, r.Y, d, d, 180, 90);
        p.AddArc(r.X + r.Width - d, r.Y, d, d, 270, 90);
        p.AddLine(r.X + r.Width, r.Y + r.Height - 2 * k, r.X + r.Width - 2 * k, r.Y + r.Height);
        p.AddLine(r.X + 2 * k, r.Y + r.Height, r.X, r.Y + r.Height - 2 * k);
        p.CloseFigure();
        gr.FillPath(&fill, &p);
        g_label(gr, layout == 1 ? (pb == PB_L2 ? L"LT" : L"RT") : layout == 2 ? (pb == PB_L2 ? L"ZL" : L"ZR")
                                : (pb == PB_L2 ? L"L2" : L"R2"),
                RectF(r.X, r.Y + 2 * k, r.Width, r.Height - 2 * k), (layout == 2 ? 6.0f : 7.0f) * k, G_INK);
        break; }
    case PB_L3: case PB_R3: {        /* stick: an inner ring */
        const float r2 = 8.5f * k;
        Pen in(G_DIM, 1.2f * k); gr.DrawEllipse(&in, cx - r2, cy - r2, 2 * r2, 2 * r2);
        g_label(gr, layout ? (pb == PB_L3 ? L"LS" : L"RS") : (pb == PB_L3 ? L"L3" : L"R3"),
                RectF(x, y, s, s), 8.5f * k, G_LINE);
        break; }
    case BTN_DPAD_NEUTRAL: case PB_UP: case PB_DOWN: case PB_LEFT: case PB_RIGHT: {
        const float w = 5.6f * k, h = 8.5f * k;   /* arm width, arm length from centre */
        GraphicsPath p;
        PointF pts[12] = {
            PointF(cx - w / 2, cy - h), PointF(cx + w / 2, cy - h), PointF(cx + w / 2, cy - w / 2),
            PointF(cx + h, cy - w / 2), PointF(cx + h, cy + w / 2), PointF(cx + w / 2, cy + w / 2),
            PointF(cx + w / 2, cy + h), PointF(cx - w / 2, cy + h), PointF(cx - w / 2, cy + w / 2),
            PointF(cx - h, cy + w / 2), PointF(cx - h, cy - w / 2), PointF(cx - w / 2, cy - w / 2) };
        p.AddPolygon(pts, 12);
        SolidBrush body(Color(255, 74, 66, 66)); gr.FillPath(&body, &p);
        if (pb == BTN_DPAD_NEUTRAL) {   /* four identical arms: a lighter rim and a centre dot, nothing lit */
            Pen rim(Color(255, 150, 142, 136), 1.0f * k); rim.SetLineJoin(LineJoinRound); gr.DrawPath(&rim, &p);
            SolidBrush dot(Color(255, 120, 112, 108)); gr.FillEllipse(&dot, cx - 1.3f * k, cy - 1.3f * k, 2.6f * k, 2.6f * k);
            break;
        }
        RectF arm = pb == PB_UP    ? RectF(cx - w / 2, cy - h, w, h - w / 2)
                  : pb == PB_DOWN  ? RectF(cx - w / 2, cy + w / 2, w, h - w / 2)
                  : pb == PB_LEFT  ? RectF(cx - h, cy - w / 2, h - w / 2, w)
                  :                  RectF(cx + w / 2, cy - w / 2, h - w / 2, w);
        GraphicsPath* ap = round_rect(arm, 1.2f * k); gr.FillPath(&fill, ap); delete ap;
        break; }
    case PB_SELECT: {
        if (layout == 2) {           /* Switch Minus */
            Pen p(G_LINE, 2.0f * k); p.SetStartCap(LineCapRound); p.SetEndCap(LineCapRound);
            gr.DrawLine(&p, cx - 5 * k, cy, cx + 5 * k, cy);
        } else if (layout == 1) {    /* Xbox View: two overlapping windows */
            Pen p(G_LINE, 1.4f * k);
            gr.DrawRectangle(&p, cx - 5 * k, cy - 4.5f * k, 7 * k, 6 * k);
            SolidBrush bk(Color(255, 40, 34, 34)); gr.FillRectangle(&bk, cx - 2 * k, cy - 1.5f * k, 7 * k, 6 * k);
            gr.DrawRectangle(&p, cx - 2 * k, cy - 1.5f * k, 7 * k, 6 * k);
        } else {                     /* PS Create: a pill with three rays above it */
            Pen p(G_LINE, 1.5f * k); p.SetStartCap(LineCapRound); p.SetEndCap(LineCapRound);
            GraphicsPath* pp = round_rect(RectF(cx - 3 * k, cy - 1 * k, 6 * k, 8.5f * k), 3 * k);
            gr.DrawPath(&p, pp); delete pp;
            gr.DrawLine(&p, cx, cy - 3.5f * k, cx, cy - 7 * k);
            gr.DrawLine(&p, cx - 3.8f * k, cy - 2.8f * k, cx - 6 * k, cy - 5.6f * k);
            gr.DrawLine(&p, cx + 3.8f * k, cy - 2.8f * k, cx + 6 * k, cy - 5.6f * k);
        }
        break; }
    case PB_START: {                 /* Options / Menu: three lines; Switch Plus */
        Pen p(G_LINE, layout == 2 ? 2.0f * k : 1.8f * k); p.SetStartCap(LineCapRound); p.SetEndCap(LineCapRound);
        if (layout == 2) { gr.DrawLine(&p, cx - 5 * k, cy, cx + 5 * k, cy); gr.DrawLine(&p, cx, cy - 5 * k, cx, cy + 5 * k); break; }
        for (int i = -1; i <= 1; i++) gr.DrawLine(&p, cx - 5 * k, cy + i * 3.6f * k, cx + 5 * k, cy + i * 3.6f * k);
        break; }
    case PB_HOME: case PB_TOUCH:
        if (layout == 2) {           /* Switch Home (a house) and Capture (a ringed square) */
            if (pb == PB_HOME) {
                PointF house[7] = { PointF(cx, cy - 6 * k), PointF(cx + 6 * k, cy - 0.5f * k), PointF(cx + 4 * k, cy - 0.5f * k),
                                    PointF(cx + 4 * k, cy + 5 * k), PointF(cx - 4 * k, cy + 5 * k),
                                    PointF(cx - 4 * k, cy - 0.5f * k), PointF(cx - 6 * k, cy - 0.5f * k) };
                gr.FillPolygon(&fill, house, 7);
                SolidBrush door(Color(255, 40, 34, 34)); gr.FillRectangle(&door, cx - 1.3f * k, cy + 1.5f * k, 2.6f * k, 3.5f * k);
            } else {
                Pen p(G_LINE, 1.4f * k);
                GraphicsPath* sq = round_rect(RectF(cx - 5.5f * k, cy - 5.5f * k, 11 * k, 11 * k), 2 * k); gr.DrawPath(&p, sq); delete sq;
                gr.DrawEllipse(&p, cx - 2.8f * k, cy - 2.8f * k, 5.6f * k, 5.6f * k);
            }
            break;
        }
        [[fallthrough]];
    default:                         /* home / touchpad */
        g_label(gr, pb == PB_HOME ? (layout ? L"\u2302" : L"PS") : L"T", RectF(x, y, s, s), 9.0f * k, G_LINE);
        break;
    }
}
}
