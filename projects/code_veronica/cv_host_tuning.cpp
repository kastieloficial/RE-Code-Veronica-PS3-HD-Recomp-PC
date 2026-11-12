/* Host thread placement and process priority (God of War Ascension port idea).
 *
 * CV_PIN=auto   PPU main, RSX render and the SPURS kernel each get a physical
 *               core of their own, SPU workers share the remaining ones. Only
 *               on CPUs with exactly 6 physical cores: the GoW port measured a
 *               gain there (55.5 -> 59 FPS on a Ryzen 5600X) and 200 ms freezes
 *               on 8 cores.
 * CV_PIN=1      the same on any CPU with at least 5 physical cores.
 * CV_PIN=0/unset off (the default).
 * CV_PRIORITY=1 runs the game process above normal priority.
 *
 * Core 0 is left to Windows, the audio and everything not named here. A
 * thread is pinned to both SMT siblings of its core, never to one logical
 * processor, so Windows can still move it inside that core. */
#ifdef _WIN32
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <dxgi.h>
#include "../../runtime/platform/host_thread_name.h"

namespace {
std::vector<KAFFINITY> g_core;   /* one mask per physical core */
volatile LONG g_spu_rr = 0;

void read_cores()
{
    DWORD len = 0;
    GetLogicalProcessorInformation(nullptr, &len);
    std::vector<SYSTEM_LOGICAL_PROCESSOR_INFORMATION> v(len / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION) + 1);
    if (!GetLogicalProcessorInformation(v.data(), &len)) return;
    for (DWORD i = 0; i < len / sizeof v[0]; i++)
        if (v[i].Relationship == RelationProcessorCore) g_core.push_back((KAFFINITY)v[i].ProcessorMask);
}

void pin(HANDLE h, const char* name)
{
    const size_t n = g_core.size();
    int c = -1;
    if (!strcmp(name, "ppu:main")) c = 1;
    else if (!strcmp(name, "rsx_render")) c = 2;
    else if (!strcmp(name, "spurs:kernel")) c = 3;
    else if (!strncmp(name, "spu:", 4) || !strncmp(name, "spurs:pm", 8) || !strncmp(name, "spurs:jobchain", 14)) {
        const size_t first = 4, pool = n > first ? n - first : 0;
        if (!pool) return;
        c = (int)(first + (size_t)(InterlockedIncrement(&g_spu_rr) - 1) % pool);
    }
    if (c < 0 || (size_t)c >= n) return;
    SetThreadAffinityMask(h, g_core[(size_t)c]);
}
}

/* CV_FG_GPU_CHECK=1: FSR 3.1 frame generation off on GPUs older than
 * RX 5000 (RDNA) / RTX 20 (Turing) / Arc, where it costs more than it gives.
 * By PCI device id of the first hardware adapter; unknown ids stay as chosen. */
static bool fg_gpu_too_old(unsigned vendor, unsigned dev)
{
    if (vendor == 0x1002) {   /* AMD: RDNA dGPUs are 0x7310+, RDNA APUs listed */
        if (dev >= 0x7310 && dev < 0x7600) return false;
        if (dev == 0x163F || dev == 0x164D || dev == 0x1681 || dev == 0x164E || dev == 0x15BF ||
            dev == 0x15C8 || dev == 0x1900 || dev == 0x1586 || dev == 0x150E) return false;
        return dev >= 0x6600 && dev < 0x7310 || dev == 0x15DD || dev == 0x15D8 || dev == 0x1636 || dev == 0x1638 || dev == 0x164C;
    }
    if (vendor == 0x10DE) return dev < 0x1E00;   /* Turing (TU10x, 0x1E..) and newer */
    if (vendor == 0x8086) return !(dev >= 0x5600 && dev < 0x5700) && !(dev >= 0xE200 && dev < 0xE300);   /* Arc */
    return false;
}

static void fg_gpu_check(void)
{
    const char* e = getenv("CV_FG_GPU_CHECK");
    const char* fg = getenv("CV_FSR_FRAME_GENERATION");
    if (!e || e[0] != '1' || !fg || strcmp(fg, "1")) return;
    typedef HRESULT (WINAPI *mk_fn)(REFIID, void**);
    HMODULE dx = LoadLibraryA("dxgi.dll");
    mk_fn mk = dx ? (mk_fn)(void*)GetProcAddress(dx, "CreateDXGIFactory1") : nullptr;
    IDXGIFactory1* f = nullptr;
    if (!mk || FAILED(mk(__uuidof(IDXGIFactory1), (void**)&f)) || !f) return;
    IDXGIAdapter1* a = nullptr;
    for (UINT i = 0; f->EnumAdapters1(i, &a) == S_OK; i++) {
        DXGI_ADAPTER_DESC1 d; a->GetDesc1(&d); a->Release();
        if (d.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;
        if (fg_gpu_too_old(d.VendorId, d.DeviceId)) {
            _putenv_s("CV_FSR_FRAME_GENERATION", "0");
            fprintf(stderr, "[TUNING] frame generation off: GPU %04X:%04X is older than RX 5000 / RTX 20%c", d.VendorId, d.DeviceId, 10);
        }
        break;
    }
    f->Release();
}

extern "C" void cv_host_tuning_init(void)
{
    /* CV_NO_GHOSTING=1: Windows does not swap a slow window (under 60 FPS,
     * a long shader build) for a "Not Responding" ghost. */
    { const char* g = getenv("CV_NO_GHOSTING"); if (g && g[0] == '1') DisableProcessWindowsGhosting(); }
    fg_gpu_check();
    const char* pr = getenv("CV_PRIORITY");
    if (pr && pr[0] == '1') {
        const BOOL ok = SetPriorityClass(GetCurrentProcess(), ABOVE_NORMAL_PRIORITY_CLASS);
        fprintf(stderr, "[TUNING] process priority above normal%s%c", ok ? "" : " (refused)", 10);
    }
    const char* e = getenv("CV_PIN");
    if (!e || !e[0] || e[0] == '0') return;
    read_cores();
    const size_t n = g_core.size();
    const bool force = e[0] == '1';
    if ((force && n < 5) || (!force && n != 6)) {
        fprintf(stderr, "[TUNING] thread placement off: %zu physical cores%c", n, 10);
        return;
    }
    g_ps3_host_pin = pin;
    fprintf(stderr, "[TUNING] thread placement on: %zu physical cores (ppu 1, render 2, spurs 3, spu 4..%zu)%c",
            n, n - 1, 10);
}
#else
extern "C" void cv_host_tuning_init(void) {}
#endif
