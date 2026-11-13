/* Hang watchdog: writes hang-log.txt when the game looks stuck.
 *
 * Two triggers, checked once a second by a sleeping thread:
 *   frozen  - no frame presented for 8 s;
 *   black   - frames keep coming but each draws at most 12 things for 90 s (menus draw few things too,
 *             so this needs a long, very empty stretch)
 *             (the "door never finishes loading" case: a black window at a
 *             steady frame rate).
 * On a trigger it records, twice 3 s apart so a parked thread can be told
 * from a busy one: the last HLE call, which guest threads are inside an HLE,
 * each guest PPU thread's lr, and for every host thread its name, where it is
 * (game code or which DLL) and the guest functions found on its stack.
 * One report per episode; it re-arms once the picture comes back.
 *
 * CV_HANG_LOG=0 disables it. The file is written in the working folder
 * (projects/code_veronica when started by the launcher), appended to. */
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <tlhelp32.h>
#include <io.h>
#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <ctime>
#include "ppu_recomp.h"
#include "cv_version.h"
#include "cv_scene_region.h"
#include <vector>
#include <algorithm>
#include <map>
extern "C" uint8_t vm_read8(uint64_t);
extern "C" uint32_t vm_read32(uint64_t);

extern "C" volatile LONG g_rsx_watch_frames;   /* rsx_d3d12_backend.c */
extern "C" volatile LONG g_rsx_watch_draws;
extern "C" uint32_t      g_last_hle_nid;       /* ppu_hle.cpp */
extern "C" const char*   g_last_hle_name;
extern "C" const char*   g_hle_inflight[];     /* 64 entries */
extern "C" void ppu_report_guest_lrs(void);    /* ppu_loader.cpp, prints to stderr */

namespace {
FILE* s_out = nullptr;
char  s_buf[16384];
size_t s_n = 0;

/* While a thread is suspended nothing may take a CRT lock it could own:
 * format into a buffer, write it out after ResumeThread. */
void cap(const char* fmt, ...) {
    va_list ap; va_start(ap, fmt);
    int n = vsnprintf(s_buf + s_n, sizeof s_buf - s_n, fmt, ap);
    va_end(ap);
    if (n > 0) s_n += (size_t)n;
    if (s_n >= sizeof s_buf) s_n = sizeof s_buf - 1;
}
void flush_cap() { if (s_n) { fwrite(s_buf, 1, s_n, s_out); s_n = 0; } }

uint32_t guest_func(uintptr_t v, uintptr_t* off) {
    uintptr_t best = 0, next = 0, maxh = 0; uint32_t g = 0;
    for (uint64_t q = 0; q < function_table_count; q++) {
        uintptr_t h = (uintptr_t)function_table[q].func;
        if (h > maxh) maxh = h;
        if (h <= v && h > best) { best = h; g = (uint32_t)function_table[q].addr; }
    }
    (void)next;
    if (!g || best == maxh || v - best > 0x4000u) return 0;
    *off = v - best; return g;
}

typedef HRESULT (WINAPI* GetDescFn)(HANDLE, PWSTR*);
/* Which guest functions the main thread keeps running while stuck: sample it
 * for 2 s. A wait leaves RIP in ntdll, so then the innermost guest function on
 * its stack is credited instead. */
struct HostFn { uintptr_t host; uint32_t guest; };
std::vector<HostFn> s_fns;
uint32_t guest_at(uintptr_t v) {
    if (s_fns.empty()) {
        for (uint64_t q = 0; q < function_table_count; q++) s_fns.push_back({(uintptr_t)function_table[q].func, (uint32_t)function_table[q].addr});
        std::sort(s_fns.begin(), s_fns.end(), [](const HostFn& a, const HostFn& b) { return a.host < b.host; });
    }
    auto it = std::upper_bound(s_fns.begin(), s_fns.end(), v, [](uintptr_t x, const HostFn& f) { return x < f.host; });
    if (it == s_fns.begin() || it == s_fns.end()) return 0;
    --it; if (v - it->host > 0x4000u) return 0;
    return it->guest;
}
void sample_main(HMODULE self) {
    static GetDescFn get_desc = (GetDescFn)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "GetThreadDescription");
    HANDLE main_thread = nullptr; DWORD pid = GetCurrentProcessId();
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0); THREADENTRY32 te; te.dwSize = sizeof te;
    if (snap != INVALID_HANDLE_VALUE && Thread32First(snap, &te)) do {
        if (te.th32OwnerProcessID != pid || !get_desc) continue;
        HANDLE th = OpenThread(THREAD_GET_CONTEXT | THREAD_SUSPEND_RESUME | THREAD_QUERY_LIMITED_INFORMATION, FALSE, te.th32ThreadID);
        if (!th) continue;
        PWSTR w = nullptr; bool hit = false;
        if (SUCCEEDED(get_desc(th, &w)) && w) { hit = wcscmp(w, L"ppu:main") == 0; LocalFree(w); }
        if (hit) { main_thread = th; break; }
        CloseHandle(th);
    } while (Thread32Next(snap, &te));
    if (snap != INVALID_HANDLE_VALUE) CloseHandle(snap);
    if (!main_thread) { fprintf(s_out, "main thread sample: ppu:main not found\n"); return; }
    guest_at(0);
    std::map<uint32_t, unsigned> inner, caller; unsigned total = 0, waiting = 0;
    for (int i = 0; i < 2000; i++) {
        if (SuspendThread(main_thread) == (DWORD)-1) break;
        CONTEXT c; c.ContextFlags = CONTEXT_CONTROL; uint32_t g0 = 0, g1 = 0; bool in_exe = false;
        if (GetThreadContext(main_thread, &c)) {
            HMODULE m = nullptr;
            GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)c.Rip, &m);
            in_exe = m == self; if (in_exe) g0 = guest_at((uintptr_t)c.Rip);
            MEMORY_BASIC_INFORMATION mbi; uint64_t* sp = (uint64_t*)c.Rsp; uint64_t end = (uint64_t)sp;
            if (VirtualQuery(sp, &mbi, sizeof mbi) && mbi.State == MEM_COMMIT && !(mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)))
                end = (uint64_t)mbi.BaseAddress + mbi.RegionSize;
            long maxk = (long)((end - (uint64_t)sp) / 8); if (maxk > 0x40000 / 8) maxk = 0x40000 / 8;
            for (long k = 0; k < maxk && !(g0 && g1); k++) {
                uint32_t g = guest_at((uintptr_t)sp[k]); if (!g) continue;
                if (!g0) g0 = g; else if (g != g0) g1 = g;
            }
        }
        ResumeThread(main_thread);
        total++; if (!in_exe) waiting++;
        if (g0) inner[g0]++;
        if (g1) caller[g1]++;
        Sleep(1);
    }
    CloseHandle(main_thread);
    auto top = [&](std::map<uint32_t, unsigned>& h, const char* title) {
        std::vector<std::pair<unsigned, uint32_t>> v; for (auto& e : h) v.push_back({e.second, e.first});
        std::sort(v.rbegin(), v.rend());
        fprintf(s_out, "%s\n", title);
        for (size_t i = 0; i < v.size() && i < 20; i++) fprintf(s_out, "  %5.1f%%  func_%08X\n", 100.0 * v[i].first / (total ? total : 1), v[i].second);
    };
    fprintf(s_out, "main thread sampled %u times over ~2 s; %u%% outside game code (waiting/sleeping)\n", total, total ? waiting * 100 / total : 0);
    top(inner, "innermost game function:");
    top(caller, "its caller:");
}

void dump(const char* why, HMODULE self) {
    time_t t = time(nullptr); char ts[32]; strftime(ts, sizeof ts, "%Y-%m-%d %H:%M:%S", localtime(&t));
    fprintf(s_out, "\n==== %s  %s ====\n", ts, why);
    fprintf(s_out, "last HLE call: 0x%08X %s\n", g_last_hle_nid, g_last_hle_name ? g_last_hle_name : "");
    int any = 0;
    for (unsigned i = 0; i < 64; i++) if (g_hle_inflight[i]) {
        if (!any) { fprintf(s_out, "guest threads inside an HLE:\n"); any = 1; }
        fprintf(s_out, "  guest-tid %-3u in %s\n", i, g_hle_inflight[i]);
    }
    if (!any) fprintf(s_out, "no guest thread is inside an HLE\n");
    fprintf(s_out, "guest PPU threads (lr = where it returns to):\n");
    fflush(s_out);
    /* ppu_report_guest_lrs prints to stderr: point stderr at this file for the call. */
    fflush(stderr);
    int saved = _dup(2);
    if (saved >= 0 && _dup2(_fileno(s_out), 2) == 0) {
        ppu_report_guest_lrs(); fflush(stderr); _dup2(saved, 2);
    }
    if (saved >= 0) _close(saved);
    fseek(s_out, 0, SEEK_END);

    static GetDescFn get_desc = (GetDescFn)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "GetThreadDescription");
    fprintf(s_out, "host threads:\n");
    DWORD me = GetCurrentThreadId(), pid = GetCurrentProcessId();
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    THREADENTRY32 te; te.dwSize = sizeof te;
    if (snap != INVALID_HANDLE_VALUE && Thread32First(snap, &te)) {
        do {
            if (te.th32OwnerProcessID != pid || te.th32ThreadID == me) continue;
            HANDLE th = OpenThread(THREAD_GET_CONTEXT | THREAD_SUSPEND_RESUME | THREAD_QUERY_LIMITED_INFORMATION, FALSE, te.th32ThreadID);
            if (!th) continue;
            char name[96] = "";
            if (get_desc) { PWSTR w = nullptr;
                if (SUCCEEDED(get_desc(th, &w)) && w) { WideCharToMultiByte(CP_UTF8, 0, w, -1, name, sizeof name, nullptr, nullptr); LocalFree(w); } }
            s_n = 0;
            if (SuspendThread(th) != (DWORD)-1) {
                CONTEXT c; c.ContextFlags = CONTEXT_CONTROL;
                if (GetThreadContext(th, &c)) {
                    HMODULE m = nullptr;
                    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)c.Rip, &m);
                    if (m == self) {
                        uintptr_t off = 0; uint32_t g = guest_func((uintptr_t)c.Rip, &off);
                        if (g) cap("  tid %5lu %-22s game func_%08X+0x%llX\n", te.th32ThreadID, name, g, (unsigned long long)off);
                        else   cap("  tid %5lu %-22s game rva=0x%llX\n", te.th32ThreadID, name, (unsigned long long)(c.Rip - (DWORD64)self));
                    } else {
                        char path[MAX_PATH] = "?"; if (m) GetModuleFileNameA(m, path, sizeof path);
                        const char* b = strrchr(path, '\\');
                        cap("  tid %5lu %-22s in %s\n", te.th32ThreadID, name, b ? b + 1 : path);
                    }
                    MEMORY_BASIC_INFORMATION mbi; uint64_t* sp = (uint64_t*)c.Rsp; uint64_t end = (uint64_t)sp;
                    if (VirtualQuery(sp, &mbi, sizeof mbi) && mbi.State == MEM_COMMIT && !(mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)))
                        end = (uint64_t)mbi.BaseAddress + mbi.RegionSize;
                    long maxk = (long)((end - (uint64_t)sp) / 8); if (maxk > 0x200000 / 8) maxk = 0x200000 / 8;
                    int found = 0; uint32_t last = 0;
                    for (long k = 0; k < maxk && found < 12; k++) {
                        uint64_t v = sp[k]; if (v < (uint64_t)self) continue;
                        uintptr_t off = 0; uint32_t g = guest_func((uintptr_t)v, &off);
                        if (!g || g == last) continue;
                        HMODULE mm = nullptr;
                        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)v, &mm);
                        if (mm != self) continue;
                        cap("        <- func_%08X+0x%llX\n", g, (unsigned long long)off);
                        last = g; found++;
                    }
                }
                ResumeThread(th);
            }
            flush_cap();
            CloseHandle(th);
        } while (Thread32Next(snap, &te));
    }
    if (snap != INVALID_HANDLE_VALUE) CloseHandle(snap);
    fflush(s_out);
}

DWORD WINAPI watch(LPVOID) {
    HMODULE self = GetModuleHandleW(nullptr);
    LONG last_frames = g_rsx_watch_frames;
    int still = 0, black = 0, reported = 0, ever = 0;
    for (;;) {
        Sleep(1000);
        LONG f = g_rsx_watch_frames, d = g_rsx_watch_draws;
        if (f != last_frames) { ever = 1; still = 0; if (d <= 12) black++; else { black = 0; reported = 0; } }
        else if (ever) still++;
        last_frames = f;
        const char* why = nullptr;
        if (still >= 8) why = "FROZEN: no frame for 8 s";
        else if (black >= 90) why = "BLACK: 90 s with at most 12 draws per frame";
        if (!why || reported) continue;
        reported = 1;
        if (!s_out) s_out = fopen("hang-log.txt", "a");
        if (!s_out) continue;
        fprintf(stderr, "[HANG] %s; details in hang-log.txt\n", why);
        dump(why, self);
        Sleep(3000);
        dump("same, 3 s later (what did not move is what is stuck)", self);
        fprintf(s_out, "room=%02X%02X%02X status=%02X game_time=%u\n", vm_read8(CV_SRT_ROOM), vm_read8(CV_SRT_ROOM + 1), vm_read8(CV_SRT_ROOM + 2), vm_read8(CV_SRT_STATUS), vm_read32(CV_SRT_TIME));
        sample_main(self);
        fflush(s_out);
    }
}
}


/* CV_TIMER_PROBE=1: find where the game keeps a rising timer (Battle Game).
 * Once a second it reads two windows of guest memory (the system object and
 * the data block around the play time) and logs the words that rose steadily
 * at a timer-like rate over the last 4 seconds, to timer-probe.txt. Diagnosis
 * only; off unless asked. */
namespace {
bool safe_read(uint32_t a, uint32_t* out) {
    __try { *out = vm_read32(a); return true; } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
struct Win { uint32_t base, words; std::vector<uint32_t> h[4]; };
DWORD WINAPI probe(LPVOID) {
    FILE* f = fopen("timer-probe.txt", "w"); if (!f) return 0;
    std::vector<Win> w(2);
    int tick = 0;
    for (;;) {
        Sleep(1000);
        const uint32_t system = vm_read32(CV_SCENE_SYSTEM);
        w[0].base = system ? (system + 0x1A000) & ~3u : 0; w[0].words = system ? 0x2000 / 4 : 0;
        w[1].base = (CV_SRT_TIME - 0x80000) & ~3u; w[1].words = 0xC0000 / 4;
        for (auto& win : w) {
            std::vector<uint32_t> cur(win.words);
            for (uint32_t i = 0; i < win.words; i++) { uint32_t v = 0; if (!safe_read(win.base + 4 * i, &v)) v = 0; cur[i] = v; }
            for (int k = 0; k < 3; k++) win.h[k] = std::move(win.h[k + 1]);
            win.h[3] = std::move(cur);
        }
        if (++tick < 5) continue;
        fprintf(f, "t=%d room=%02X%02X%02X status=%02X game_time=%u\n", tick, vm_read8(CV_SRT_ROOM), vm_read8(CV_SRT_ROOM + 1), vm_read8(CV_SRT_ROOM + 2), vm_read8(CV_SRT_STATUS), vm_read32(CV_SRT_TIME));
        int shown = 0;
        for (auto& win : w) {
            if (win.h[0].size() != win.words) continue;
            for (uint32_t i = 0; i < win.words && shown < 40; i++) {
                const uint32_t a = win.h[0][i], b = win.h[1][i], c = win.h[2][i], d = win.h[3][i];
                if (!(a < b && b < c && c < d)) continue;
                const uint32_t d1 = b - a, d2 = c - b, d3 = d - c;
                const uint32_t lo = std::min(d1, std::min(d2, d3)), hi = std::max(d1, std::max(d2, d3));
                if (lo < 20 || hi > 140 || hi * 10 > lo * 14) continue;
                fprintf(f, "  %08X = %u (+%u/s)\n", win.base + 4 * i, d, d3); shown++;
            }
        }
        fflush(f);
    }
}
}
extern "C" void cv_hang_watch_init(void) {
    const char* e = getenv("CV_HANG_LOG");
    if (e && *e == '0') return;
    HANDLE h = CreateThread(nullptr, 0, watch, nullptr, 0, nullptr);
    if (h) CloseHandle(h);
    const char* tp = getenv("CV_TIMER_PROBE");
    if (tp && *tp == '1') { HANDLE ph = CreateThread(nullptr, 0, probe, nullptr, 0, nullptr); if (ph) CloseHandle(ph); }
}
#endif
