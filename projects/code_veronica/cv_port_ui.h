#pragma once
/* The port's shared menu look (F6 items, F7 archives, the return-to-title box, the achievement card):
 * embedded fonts, the gold/blue style primitives of docs/PLANO-MELHORIAS-v1.7.md, the footer with
 * button glyphs and the input helpers (hint kind, Xbox 360 shoulder swap). Everything draws in design
 * units: the caller scales the Graphics. Primitives only draw; layout, hit rectangles and text live in
 * the menu that uses them. */
#include <memory>
#include <string>
#include <vector>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include "../../libs/input/pad_phys.h"

namespace cv_port_ui {
// ---- Fonts -----------------------------------------------------------------
// Registered families (embedded RCDATA, SIL OFL): "Cinzel" (700), "Cinzel SemiBold" (600),
// "Barlow Semi Condensed" (400 Regular, 700 Bold), "Barlow Semi Condensed Medium" (500),
// "Barlow Semi Condensed SemiBold" (600).
enum class Role { TITLE, TITLE_SEMI, BODY400, BODY500, BODY600, BODY700 };
bool fonts_available();                    // the embedded fonts registered (needs GdiplusStartup done)
const wchar_t* family_name(Role role);     // the family actually registered for it, or nullptr
void register_gdi_fonts();                 // AddFontMemResourceEx for GDI controls (the F7 search box), idempotent
bool native_fonts(int lang);               // false for JA, AR, ZH, KO, RU: they use cv_srt_i18n::font_face
std::unique_ptr<Gdiplus::Font> font(int lang,Role role,float px);   // px in design units (UnitPixel)

// ---- Colours (mock.css) -----------------------------------------------------
namespace col {
inline const Gdiplus::Color GOLD_HI(255,246,231,168),GOLD(255,201,161,62),GOLD_LO(255,110,80,20),GOLD_DARK(255,32,24,10),
    BG0(255,18,26,51),BG1(255,10,15,30),BG2(255,7,10,20),BAND0(255,23,32,61),BAND1(255,13,20,40),
    PANEL0(255,11,29,71),PANEL1(255,7,18,48),BLUE_LINE(255,58,95,168),CELL0(255,26,60,134),CELL1(255,16,40,94),
    SEL(255,255,216,106),TXT(255,230,236,248),MUTED(255,159,168,192),IVORY(255,237,230,214),INK(255,26,18,6),
    OK(255,143,209,138),WARN(255,240,180,76),BAD(255,255,107,107),FOOT(255,200,206,221);
}
// ---- Mouse feedback --------------------------------------------------------------
// Every clickable primitive takes a ButtonState: HOVER = lighter gradient + glow, PRESSED = darker, 1px inset,
// content scaled ~0.94, DISABLED = flat grey.
enum class ButtonState { NORMAL, HOVER, PRESSED, DISABLED };
// Pure helper for a window's hit rectangles (ids are the caller's: a rect index, a HitKind...). Feed it from
// WM_MOUSEMOVE (move + arm), WM_LBUTTONDOWN (down), WM_LBUTTONUP (up), WM_MOUSELEAVE (leave); each returns true
// when the look changed (repaint). up() sets `fired` to the id to act on only when released over the id pressed.
struct MouseTrack {
    int hover=-1,pressed=-1;bool tracking=false;
    bool move(int id) {const bool c=id!=hover;hover=id;return c;}
    bool down(int id) {const bool c=pressed!=id;pressed=id;hover=id;return c;}
    bool up(int id,int& fired) {fired=(pressed>=0&&pressed==id)?id:-1;const bool c=pressed!=-1;pressed=-1;hover=id;return c||fired>=0;}
    bool leave() {const bool c=hover!=-1||pressed!=-1;hover=pressed=-1;tracking=false;return c;}
    ButtonState state(int id,bool enabled=true) const {return !enabled?ButtonState::DISABLED:pressed==id&&hover==id?ButtonState::PRESSED:hover==id&&pressed<0?ButtonState::HOVER:ButtonState::NORMAL;}
    // Ask Windows for WM_MOUSELEAVE once (call it in WM_MOUSEMOVE).
    void arm(HWND w) {if(tracking)return;TRACKMOUSEEVENT t{sizeof t,TME_LEAVE,w,0};if(TrackMouseEvent(&t))tracking=true;}
};

// ---- Primitives (rects in design units) -------------------------------------
std::unique_ptr<Gdiplus::GraphicsPath> round_path(Gdiplus::RectF r,float radius);
// Window frame: 3px bevelled gold ring, inner line, dark blue radial fill. Content starts 3px inside `outer`.
void gold_frame(Gdiplus::Graphics& g,Gdiplus::RectF outer);
void header_band(Gdiplus::Graphics& g,Gdiplus::RectF r);                // #17203D->#0D1428, bottom line #6E5014
void panel_blue(Gdiplus::Graphics& g,Gdiplus::RectF r);                 // catalog panel
void cell_blue(Gdiplus::Graphics& g,Gdiplus::RectF r,bool selected,ButtonState st=ButtonState::NORMAL);    // item cell; selected: gold border, glow, corner triangle
// Plates (tabs/buttons). active: gold with dark text; inactive: dark steel with a gold border. Label optional.
void plate(Gdiplus::Graphics& g,Gdiplus::RectF r,bool active,ButtonState st=ButtonState::NORMAL);
void plate_label(Gdiplus::Graphics& g,Gdiplus::RectF r,bool active,const std::wstring& label,int lang,float px=18.5f,float tracking=0.3f,ButtonState st=ButtonState::NORMAL);
void badge_gold(Gdiplus::Graphics& g,Gdiplus::RectF r,const std::wstring& label,int lang,float px=12,float tracking=0.3f);
void stepper_arrow(Gdiplus::Graphics& g,Gdiplus::RectF r,bool left,ButtonState st=ButtonState::NORMAL);    // gold square with a chevron
void primary_button_red(Gdiplus::Graphics& g,Gdiplus::RectF r,const std::wstring& label,int lang,float px=19,float tracking=1.4f,ButtonState st=ButtonState::NORMAL);
void close_button(Gdiplus::Graphics& g,Gdiplus::RectF r,ButtonState st=ButtonState::NORMAL);   // gold-ring round button with an X
void button_disabled(Gdiplus::Graphics& g,Gdiplus::RectF r,const std::wstring& label,int lang,float px=19,float tracking=1.4f);
void warning_box(Gdiplus::Graphics& g,Gdiplus::RectF r,const std::wstring& message,int lang,float px=19);   // amber bar #F0B44C
void diamond(Gdiplus::Graphics& g,float cx,float cy,float size);        // the gold separator
// Gold gradient title (Cinzel 700): #FFF3C0 -> #E0B650 -> #9C7424 with a 2px dark shadow.
void title_gold(Gdiplus::Graphics& g,const std::wstring& s,Gdiplus::RectF r,int lang,float px,float tracking=2,
                Gdiplus::StringAlignment h=Gdiplus::StringAlignmentNear,Gdiplus::StringAlignment v=Gdiplus::StringAlignmentCenter);
// Text: the PlayStation square drawn as a box, right-to-left text, ellipsis, optional letter-spacing (small caps).
void text(Gdiplus::Graphics& g,const std::wstring& s,Gdiplus::RectF r,const Gdiplus::Font& f,Gdiplus::Color c,
          Gdiplus::StringAlignment h=Gdiplus::StringAlignmentNear,Gdiplus::StringAlignment v=Gdiplus::StringAlignmentNear,
          bool wrap=false,float tracking=0);
float text_width(Gdiplus::Graphics& g,const std::wstring& s,const Gdiplus::Font& f,float tracking=0);

// ---- Footer -----------------------------------------------------------------
enum class GlyphKind { KEY, PAD, DPAD, ARROWS, TEXT };
// KEY: value = a virtual key, label its cap (empty: cv_key_cap_label); PAD: value = PB_*; DPAD: the d-pad;
// ARROWS: value = bitmask 1 up, 2 down, 4 left, 8 right (keycaps); TEXT: a keycap with `label`.
struct Glyph { GlyphKind kind; int value=0; std::wstring label; };
// One footer group: the glyphs shown with a keyboard, the ones shown with a controller, and the action.
struct FooterGroup { std::vector<Glyph> keyboard,pad; std::wstring label; };
// Draws the groups centred in `r` (a thin gold line on top), separated by gold diamonds; scaled down when too wide.
// `kind` is hint_kind(): keyboard, PlayStation, Xbox or Switch glyphs.
void draw_footer(Gdiplus::Graphics& g,Gdiplus::RectF r,int lang,int kind,const std::vector<FooterGroup>& groups);
float footer_width(Gdiplus::Graphics& g,int lang,int kind,const std::vector<FooterGroup>& groups);
// A glyph alone (tab side keycaps, close button hints): its width is returned.
float draw_glyph(Gdiplus::Graphics& g,const Glyph& glyph,int kind,float x,float y,float size);

// ---- Input helpers (moved from cv_save_library.cpp) ---------------------------
int normalize_kind(int kind);                 // anything but a known controller counts as the keyboard
int hint_kind(int last_input);                // CV_BUTTON_ICONS override, else the last device used
int glyph_layout(int kind);                   // 0 PlayStation, 1 Xbox, 2 Switch (cv_port_glyphs::btn_glyph's layout)
void menu_pad(PadPhys& p);                    // undo the Xbox 360 shoulder swap, left stick -> d-pad
std::wstring switch_hint(std::wstring t);     // PlayStation names -> Switch names

// ---- Window placement ---------------------------------------------------------
float scale_for(HWND game,int w,int h);       // clamp(min(cw/1920,ch/1080),0.55,2.5), never larger than the client
RECT center_on_game(HWND game,int w,int h);   // screen rect of a w*h window centred on the game's client area
}
