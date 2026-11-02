/* Skip the opening logos and the intro movie (patch flag cv_skip_logos, a
 * launcher switch): the game opens on its title screen.
 *
 * The CAPCOM and MT FRAMEWORK logos are one layout animation played by the
 * uBio_cvLogo unit. Its last step (func_0020140C) waits until the animation's
 * frame (a float at +0x34 of the layout, the unit's +0x3C) passes 0x333, then
 * marks the logo done and the game goes on to the intro movie and the title.
 * With the flag on, the frame is put past that point first, so the logo ends
 * as soon as it has started.
 *
 * The title sequence (func_0002B35C, state at 0x00BBE8FC) sets up the title,
 * then in state 7 plays movie 16 (the intro, movie/mv_016.pam), waits on it in
 * states 8 and 9, and in state -1 closes and hands over to PRESS START. With
 * the flag on, state 7 goes straight to -1, so the movie never starts.
 *
 * Both wrappers are built by tools/cv_native_menu_hooks.py. The addresses
 * named here are the European ones; cv_version.h has each version's.
 */
#include "ppu_recomp.h"
#include "cv_version.h"
#include "cv_options.h"
#include <cstdint>
#include <cstring>

void CV_OG_LOGO(ppu_context*);
void CV_OG_TITLE(ppu_context*);
extern "C" volatile unsigned char* ps3_patch_flag(const char* name);

static bool skip_on()
{
    static volatile unsigned char* skip = nullptr;
    if (!skip) skip = ps3_patch_flag("cv_skip_logos");
    return *skip != 0;
}

void CV_FN_TITLE(ppu_context* ctx)
{
    const uint32_t kTitleState = CV_TITLE_STATE;
    if (skip_on() && vm_read32(kTitleState) == 7) vm_write32(kTitleState, 0xFFFFFFFFu);   /* past the intro movie */
    CV_OG_TITLE(ctx);
}

void CV_FN_LOGO(ppu_context* ctx)
{
    if (skip_on()) {
        const uint32_t layout = vm_read32((uint32_t)ctx->gpr[3] + 0x3C);
        if (layout) {
            const float last = 820.0f;   /* one past the 0x333 the check waits for */
            uint32_t bits;
            std::memcpy(&bits, &last, 4);
            vm_write32(layout + 0x34, bits);
        }
    }
    CV_OG_LOGO(ctx);
}

#if !defined(CV_REGION_JP) && !defined(CV_REGION_US)
extern "C" PPU_THREAD_LOCAL void (*g_trampoline_fn)(void*);
static void drain_boot(ppu_context* ctx)
{
    while (g_trampoline_fn) {
        auto fn = g_trampoline_fn;
        g_trampoline_fn = nullptr;
        fn(ctx);
    }
}
void cv_original_001E58FC(ppu_context*);
void cv_original_00210914(ppu_context*);

// EU: skip only the initial autosave notice (message 0x29C). Continue at
// the same load state as its OK button, retaining normal save/error handling.
void func_001E58FC(ppu_context* ctx)
{
    if(!cv_options::enabled("CV_SKIP_START_NOTICES")){cv_original_001E58FC(ctx);return;}
    const uint32_t unit = uint32_t(ctx->gpr[3]);
    const uint32_t pane = vm_read32(unit + 0x9C);
    if (vm_read32(unit + 0x30) != 0 || !(vm_read32(pane + 0x34) & 4)) {
        cv_original_001E58FC(ctx);
        return;
    }
    ppu_context query = *ctx;
    query.gpr[3] = pane;
    func_001F047C(&query); drain_boot(&query);
    if (!query.gpr[3]) return;
    vm_write8(vm_read32(0xAF96C4) + 0x51, 0);
    const uint32_t settings = vm_read32(unit + 0x98);
    vm_write32(settings + 0x1C, 0x29D); // Loading save data, not a confirmation.
    vm_write16(settings + 0x14, 0);
    vm_write32(unit + 0x54, 1);
    vm_write32(unit + 0x30, 1);
}

// EU: initial PSN question 0x308 -> No, then 0x309 -> Start game.
// State 6 returns the original offline result (2). Explicit leaderboard
// requests (mode 4), profile errors and all other dialogs remain unchanged.
void func_00210914(ppu_context* ctx)
{
    if(!cv_options::enabled("CV_SKIP_START_NOTICES")){cv_original_00210914(ctx);return;}
    const uint32_t unit = uint32_t(ctx->gpr[3]);
    if (vm_read32(unit + 0x30) == 2 && vm_read8(0xBBE993) != 4) {
        ppu_context cleanup = *ctx;
        cleanup.gpr[3] = vm_read32(0xAF96A0);
        func_0026B170(&cleanup); drain_boot(&cleanup);
        vm_write32(unit + 0x30, 6);
    }
    cv_original_00210914(ctx);
}
#endif
