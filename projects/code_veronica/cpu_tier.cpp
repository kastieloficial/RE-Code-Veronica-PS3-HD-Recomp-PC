/* cpu_tier.cpp -- start the executable this processor can run.
 *
 * The normal executables build their SPU code for Haswell (/arch:AVX2: AVX2,
 * FMA, BMI1/2, MOVBE, LZCNT, F16C). A processor without one of those --
 * Sandy/Ivy Bridge, AMD FX, Nehalem -- died with an illegal instruction
 * right at boot (issue #12: MOVBE in the NID hash, i3-2120). Those processors
 * run the compatibility build instead (CV_CPU_COMPAT, SSE4.1), *_SSE4.exe
 * beside this one.
 *
 * The check runs from .CRT$XCB, before every C++ constructor and every
 * __attribute__((constructor)) (.CRT$XCU), so no code built for AVX2 runs
 * first. This file itself is built without /arch.
 *
 * CV_CPU_FORCE_COMPAT=1 takes the compatibility path on any processor (tests).
 */
#include <windows.h>
#include <intrin.h>
#include <cstdlib>
#include <string>

namespace {

struct Cpu { bool haswell; bool sse41; };

Cpu probe()
{
    int r[4] = {};
    __cpuid(r, 0);
    const unsigned max = (unsigned)r[0];
    unsigned ecx1 = 0, ebx7 = 0, ecx81 = 0;
    if (max >= 1) { __cpuid(r, 1); ecx1 = (unsigned)r[2]; }
    if (max >= 7) { __cpuidex(r, 7, 0); ebx7 = (unsigned)r[1]; }
    __cpuid(r, 0x80000000);
    if ((unsigned)r[0] >= 0x80000001) { __cpuid(r, 0x80000001); ecx81 = (unsigned)r[2]; }

    const bool osxsave = ecx1 & (1u << 27);
    const bool avx_os = osxsave && (ecx1 & (1u << 28)) && ((_xgetbv(0) & 6) == 6);
    Cpu c;
    c.sse41 = (ecx1 & (1u << 9)) && (ecx1 & (1u << 19));        /* SSSE3, SSE4.1 */
    c.haswell = avx_os
        && (ebx7 & (1u << 5))                                    /* AVX2 */
        && (ebx7 & (1u << 3)) && (ebx7 & (1u << 8))              /* BMI1, BMI2 */
        && (ecx1 & (1u << 12)) && (ecx1 & (1u << 22))            /* FMA, MOVBE */
        && (ecx1 & (1u << 29)) && (ecx1 & (1u << 23))            /* F16C, POPCNT */
        && (ecx81 & (1u << 5));                                  /* LZCNT */
    return c;
}

[[noreturn]] void refuse(const wchar_t* text)
{
    MessageBoxW(nullptr, text, L"Code Veronica", MB_OK | MB_ICONERROR);
    ExitProcess(1);
}

#if !defined(CV_CPU_COMPAT)
/* "...\CodeVeronicaPort_US.exe" -> "...\CodeVeronicaPort_US_SSE4.exe" */
std::wstring compat_sibling()
{
    wchar_t self[32768] = {};
    const DWORD n = GetModuleFileNameW(nullptr, self, 32768);
    if (!n || n >= 32768) return L"";
    std::wstring p(self, n);
    const size_t dot = p.rfind(L'.');
    if (dot == std::wstring::npos) return L"";
    p.insert(dot, L"_SSE4");
    return GetFileAttributesW(p.c_str()) == INVALID_FILE_ATTRIBUTES ? L"" : p;
}

/* The arguments of this process's command line, without argv[0]. */
std::wstring arguments()
{
    const wchar_t* c = GetCommandLineW();
    if (*c == L'"') { ++c; while (*c && *c != L'"') ++c; if (*c) ++c; }
    else while (*c && *c != L' ' && *c != L'\t') ++c;
    return c;
}

[[noreturn]] void run_compat(const std::wstring& exe)
{
    std::wstring cmd = L"\"" + exe + L"\"" + arguments();
    STARTUPINFOW si = { sizeof si };
    PROCESS_INFORMATION pi = {};
    wchar_t cwd[32768] = {};
    GetCurrentDirectoryW(32768, cwd);
    if (!CreateProcessW(exe.c_str(), &cmd[0], nullptr, nullptr, TRUE, 0, nullptr, cwd, &si, &pi))
        refuse(L"Nao foi possivel abrir a versao de compatibilidade (_SSE4.exe).\n\n"
               L"Could not start the compatibility version (_SSE4.exe).");
    CloseHandle(pi.hThread);
    /* Wait, so a script that started us sees the game's exit code. */
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 0;
    GetExitCodeProcess(pi.hProcess, &code);
    ExitProcess(code);
}
#endif

void check()
{
    const Cpu cpu = probe();
#if defined(CV_CPU_COMPAT)
    if (!cpu.sse41)
        refuse(L"Este processador nao tem SSE4.1, o minimo que o jogo precisa.\n\n"
               L"This processor lacks SSE4.1, the minimum the game needs.");
#else
    const char* force = std::getenv("CV_CPU_FORCE_COMPAT");
    if (cpu.haswell && !(force && *force == '1')) return;
    const std::wstring exe = compat_sibling();
    if (!exe.empty()) run_compat(exe);
    refuse(L"Este processador nao tem AVX2. Esta pasta nao tem a versao de compatibilidade "
           L"(arquivos _SSE4.exe ao lado deste).\n\n"
           L"This processor lacks AVX2, and the compatibility version (_SSE4.exe files beside "
           L"this one) is not in this folder.");
#endif
}

void __cdecl check_entry() { check(); }

}  // namespace

/* .CRT$XCB runs before .CRT$XCU, where the compiler puts every dynamic
 * initializer and constructor function. Entries there are void(*)(void). */
#pragma section(".CRT$XCB", read)
extern "C" __declspec(allocate(".CRT$XCB")) void (__cdecl* const cv_cpu_tier_entry)(void) = check_entry;
#pragma comment(linker, "/include:cv_cpu_tier_entry")
