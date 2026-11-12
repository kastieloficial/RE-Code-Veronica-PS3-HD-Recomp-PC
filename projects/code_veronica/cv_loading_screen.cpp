/* First-start shader screen: while the shipped shader list builds with the
 * game waiting (libs/video/rsx_d3d12_backend.c, "Shader preload"), the whole
 * window shows Code: Veronica art, a title in a Resident Evil style font, the
 * count, a bar and the percentage. Drawn with GDI+ into one 1920x1080 image,
 * redrawn only when the count changes, handed to the renderer as a texture
 * through g_rsx_loading_hook (the trophy card's path).
 *
 * The art and the font are embedded (cv_loading_screen.rc). Texts come from
 * the launcher in its language: SHADER_SCREEN_TEXT="<title>|<subtitle>" (UTF-8).
 * CV_SHADER_SCREEN=0 keeps the small corner note instead. */
#define NOMINMAX
#include <windows.h>
#include <gdiplus.h>
#include <objidl.h>
#include <shlwapi.h>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include "../../libs/video/rsx_ui_batch.h"
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "shlwapi.lib")

extern "C" const rsx_ui_sprite* (*g_rsx_loading_hook)(uint32_t, uint32_t, const uint32_t**, uint32_t*, uint32_t*, uint64_t*);
extern "C" int rsx_shader_preload_state(int* done, int* total, unsigned long long* finished_ms);
extern "C" int rsx_shader_preload_waiting(void);

namespace {
using namespace Gdiplus;
constexpr int W = 1920, H = 1080;

struct State {
    std::mutex lock;
    ULONG_PTR gdip = 0;
    std::unique_ptr<Bitmap> art;              /* the background, already 1920x1080 */
    std::unique_ptr<PrivateFontCollection> fonts;
    std::wstring title, subtitle;
    std::vector<uint32_t> rgba;               /* RGBA8, straight alpha */
    uint64_t generation = 0;
    int drawn_done = -1, drawn_total = -1;
    ULONGLONG shown_at = 0, finished_at = 0;
    rsx_ui_sprite sprite{};
    bool ready = false;
};
State& state() { static State* s = new State; return *s; }

std::wstring widen(const char* s)
{
    if (!s || !*s) return {};
    /* UTF-8 from the launcher; anything else (a console's code page) as ANSI. */
    UINT cp = CP_UTF8;
    int n = MultiByteToWideChar(cp, MB_ERR_INVALID_CHARS, s, -1, nullptr, 0);
    if (n <= 0) { cp = CP_ACP; n = MultiByteToWideChar(cp, 0, s, -1, nullptr, 0); }
    std::wstring w(n > 0 ? n - 1 : 0, L'\0');
    if (n > 1) MultiByteToWideChar(cp, 0, s, -1, &w[0], n);
    return w;
}

bool resource(const wchar_t* name, const void** data, DWORD* size)
{
    HMODULE m = GetModuleHandleW(nullptr);
    HRSRC r = FindResourceW(m, name, MAKEINTRESOURCEW(10) /* RT_RCDATA */);
    HGLOBAL g = r ? LoadResource(m, r) : nullptr;
    *data = g ? LockResource(g) : nullptr;
    *size = r ? SizeofResource(m, r) : 0;
    return *data && *size;
}

/* Only plain ASCII goes in the Resident Evil font: it has no accents. */
bool ascii(const std::wstring& s)
{
    for (wchar_t c : s) if (c > 0x7E) return false;
    return true;
}

void load(State& s)
{
    GdiplusStartupInput in;
    if (GdiplusStartup(&s.gdip, &in, nullptr) != Ok) return;
    const void* p; DWORD n;
    if (resource(L"CV_LOADING_ART", &p, &n)) {
        IStream* st = SHCreateMemStream(static_cast<const BYTE*>(p), n);
        if (st) { s.art.reset(new Bitmap(st)); st->Release(); if (s.art->GetLastStatus() != Ok) s.art.reset(); }
    }
    s.fonts.reset(new PrivateFontCollection);
    if (resource(L"CV_LOADING_FONT", &p, &n)) s.fonts->AddMemoryFont(p, (INT)n);
    std::wstring t = widen(getenv("SHADER_SCREEN_TEXT"));
    const size_t bar = t.find(L'|');
    s.title = bar == std::wstring::npos ? t : t.substr(0, bar);
    s.subtitle = bar == std::wstring::npos ? L"" : t.substr(bar + 1);
    if (s.title.empty()) s.title = L"COMPILING SHADERS";
    if (s.subtitle.empty()) s.subtitle = L"Only on the first start after an install or update. The game starts when this is done.";
    s.ready = true;
}

void centered(Graphics& g, const std::wstring& text, Font& font, float y, float h, const Color& color, bool shadow)
{
    StringFormat f; f.SetAlignment(StringAlignmentCenter); f.SetLineAlignment(StringAlignmentCenter);
    if (shadow) { SolidBrush sh(Color(170, 0, 0, 0)); g.DrawString(text.c_str(), -1, &font, RectF(3, y + 3, (REAL)W, h), &f, &sh); }
    SolidBrush b(color); g.DrawString(text.c_str(), -1, &font, RectF(0, y, (REAL)W, h), &f, &b);
}

void compose(State& s, int done, int total)
{
    Bitmap bmp(W, H, PixelFormat32bppARGB);
    Graphics g(&bmp);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintAntiAlias);
    g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
    g.Clear(Color(255, 8, 8, 10));
    if (s.art) g.DrawImage(s.art.get(), Rect(0, 0, W, H));
    /* Darker towards the bottom, where the text sits, and a soft vignette. */
    /* The gradients reach past the filled area: GDI+ tiles a gradient at its
     * end points, which left a one-pixel line across the picture. */
    { LinearGradientBrush fade(PointF(0, H * 0.45f - 2), PointF(0, (REAL)H + 2), Color(0, 0, 0, 0), Color(235, 4, 4, 6));
      fade.SetWrapMode(WrapModeTileFlipXY);
      g.FillRectangle(&fade, 0, (INT)(H * 0.45f), W, H - (INT)(H * 0.45f)); }
    { LinearGradientBrush top(PointF(0, -2), PointF(0, 162), Color(140, 0, 0, 0), Color(0, 0, 0, 0));
      top.SetWrapMode(WrapModeTileFlipXY); g.FillRectangle(&top, 0, 0, W, 160); }

    FontFamily* re = nullptr; int nfam = 0;
    std::unique_ptr<FontFamily[]> fams;
    if (s.fonts && (nfam = s.fonts->GetFamilyCount()) > 0) {
        fams.reset(new FontFamily[nfam]); int got = 0;
        s.fonts->GetFamilies(nfam, fams.get(), &got);
        if (got > 0) re = &fams[0];
    }
    FontFamily plain(L"Segoe UI");
    const int pct = total > 0 ? (int)((long long)done * 100 / total) : 0;
    const Color gold(255, 226, 196, 140), dim(255, 196, 182, 160), red(255, 168, 24, 20);

    Font title_font(re && ascii(s.title) ? re : &plain, 64, FontStyleRegular, UnitPixel);
    centered(g, s.title, title_font, 676, 96, gold, true);

    wchar_t count[64];
    swprintf(count, 64, L"%d / %d   \u2022   %d%%", done, total, pct);
    Font count_font(&plain, 30, FontStyleBold, UnitPixel);
    centered(g, count, count_font, 790, 44, Color(255, 236, 226, 206), true);

    /* The bar, Resident Evil style: a thin double gold frame with diamond
     * ends, a near-black track and a blood-red fill with a darker core. */
    const float bx = 520, by = 862, bw = 880, bh = 18;
    SolidBrush track(Color(235, 14, 10, 10)); g.FillRectangle(&track, bx, by, bw, bh);
    const float fw = bw * (total > 0 ? (float)done / total : 0.f);
    if (fw > 0.5f) {
        LinearGradientBrush fill(PointF(0, by - 1), PointF(0, by + bh + 1), Color(255, 196, 30, 24), Color(255, 92, 6, 6));
        g.FillRectangle(&fill, bx, by, fw, bh);
        SolidBrush shine(Color(70, 255, 190, 150)); g.FillRectangle(&shine, bx, by + 1, fw, 2.f);
        for (float x = bx + 12; x < bx + fw - 2; x += 24) {   /* faint vertical ticks, like the HUD gauges */
            SolidBrush tick(Color(45, 0, 0, 0)); g.FillRectangle(&tick, x, by, 2.f, bh);
        }
    }
    Pen gold_out(Color(230, 176, 142, 82), 1.5f), gold_in(Color(150, 120, 92, 50), 1.f);
    g.DrawRectangle(&gold_out, bx - 5, by - 5, bw + 10, bh + 10);
    g.DrawRectangle(&gold_in, bx - 2, by - 2, bw + 4, bh + 4);
    for (int side = 0; side < 2; side++) {   /* diamond caps on both ends */
        const float cx = side ? bx + bw + 18 : bx - 18, cy = by + bh / 2;
        PointF d[4] = { PointF(cx, cy - 9), PointF(cx + 9, cy), PointF(cx, cy + 9), PointF(cx - 9, cy) };
        SolidBrush fillc(Color(255, 120, 14, 12)); g.FillPolygon(&fillc, d, 4); g.DrawPolygon(&gold_out, d, 4);
        g.DrawLine(&gold_in, side ? cx - 9 : cx + 9, cy, side ? bx + bw + 5 : bx - 5, cy);
    }

    Font sub_font(&plain, 22, FontStyleRegular, UnitPixel);
    centered(g, s.subtitle, sub_font, 900, 40, dim, true);

    g.Flush(FlushIntentionSync);
    BitmapData px{}; Rect r(0, 0, W, H);
    if (bmp.LockBits(&r, ImageLockModeRead, PixelFormat32bppARGB, &px) != Ok) return;
    s.rgba.resize((size_t)W * H);
    for (int y = 0; y < H; y++) {
        const uint32_t* src = reinterpret_cast<const uint32_t*>(static_cast<const unsigned char*>(px.Scan0) + (ptrdiff_t)y * px.Stride);
        uint32_t* dst = &s.rgba[(size_t)y * W];
        for (int x = 0; x < W; x++) { const uint32_t c = src[x]; dst[x] = 0xFF000000u | ((c & 0xFF) << 16) | (c & 0xFF00) | ((c >> 16) & 0xFF); }
    }
    bmp.UnlockBits(&px);
    s.generation++;
}

/* Renderer thread, every frame. */
const rsx_ui_sprite* frame(uint32_t w, uint32_t h, const uint32_t** rgba, uint32_t* aw, uint32_t* ah, uint64_t* gen)
{
    *rgba = nullptr; *aw = *ah = 0; *gen = 0;
    static int off = -1;
    if (off < 0) { const char* e = getenv("CV_SHADER_SCREEN"); off = e && e[0] == '0'; }
    if (off || !w || !h) return nullptr;
    State& s = state();
    std::lock_guard<std::mutex> guard(s.lock);
    int done = 0, total = 0; unsigned long long fin = 0;
    const int st = rsx_shader_preload_state(&done, &total, &fin);
    const ULONGLONG now = GetTickCount64();
    if (rsx_shader_preload_waiting()) { if (!s.shown_at) s.shown_at = now; s.finished_at = 0; }
    else if (s.shown_at && !s.finished_at) s.finished_at = now;   /* show 100% a moment, then fade */
    if (!s.shown_at) return nullptr;
    const ULONGLONG HOLD = 600, FADE = 400;
    if (s.finished_at && now - s.finished_at >= HOLD + FADE) { s.shown_at = 0; s.finished_at = 0; s.rgba.clear(); s.rgba.shrink_to_fit(); return nullptr; }
    if (!s.ready) { static bool tried = false; if (tried) return nullptr; tried = true; load(s); if (!s.ready) return nullptr; }
    if (st == 2 || s.finished_at) done = total;
    /* Redraw at most four times a second: one 1920x1080 upload each. */
    static ULONGLONG last;
    if (s.rgba.empty() || ((done != s.drawn_done || total != s.drawn_total) && now - last >= 250)) {
        compose(s, done, total); s.drawn_done = done; s.drawn_total = total; last = now;
    }
    if (s.rgba.empty()) return nullptr;
    float a = 1.f;
    if (s.finished_at && now - s.finished_at > HOLD) a = 1.f - (float)(now - s.finished_at - HOLD) / FADE;
    /* Cover the window, keeping 16:9 (crop the long side). */
    const float sc = std::max((float)w / W, (float)h / H);
    s.sprite.w = W * sc; s.sprite.h = H * sc;
    s.sprite.x = ((float)w - s.sprite.w) / 2; s.sprite.y = ((float)h - s.sprite.h) / 2;
    s.sprite.u0 = s.sprite.v0 = 0; s.sprite.u1 = s.sprite.v1 = 1;
    s.sprite.argb = ((uint32_t)std::clamp((int)(a * 255.f), 0, 255) << 24) | 0xFFFFFF;
    s.sprite.after = 0;
    *rgba = s.rgba.data(); *aw = W; *ah = H; *gen = s.generation;
    return &s.sprite;
}
}

extern "C" void cv_loading_screen_init(void) { g_rsx_loading_hook = frame; }
