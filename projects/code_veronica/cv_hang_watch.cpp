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
    }
}
}

extern "C" void cv_hang_watch_init(void) {
    const char* e = getenv("CV_HANG_LOG");
    if (e && *e == '0') return;
    HANDLE h = CreateThread(nullptr, 0, watch, nullptr, 0, nullptr);
    if (h) CloseHandle(h);
}
#endif
