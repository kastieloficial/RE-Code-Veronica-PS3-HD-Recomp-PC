/* spu_cpu_select.c -- register the SPU code this processor can run (CV_CPU_UNIFIED).
 *
 * The lifted SPU images are linked twice: built for AVX2 (the originals, with
 * port_spu_register_all) and for SSE4.1 (tools/spu_cpu_variant.py, every
 * symbol prefixed sse4_). Both tables have lost their own constructor; this
 * one registers exactly one of them, after cpu_tier.cpp's check (.CRT$XCB)
 * has decided. Built without /arch, like cpu_tier.cpp.
 */
#include <stdio.h>

int cv_cpu_haswell(void);
void port_spu_register_all(void);
void sse4_port_spu_register_all(void);

__attribute__((constructor)) static void port_spu_register_selected(void)
{
    const int avx2 = cv_cpu_haswell();
    if (avx2) port_spu_register_all();
    else sse4_port_spu_register_all();
    fprintf(stderr, "[CPU] SPU code: %s\n", avx2 ? "AVX2" : "SSE4.1 (no AVX2 or CV_CPU_FORCE_COMPAT=1)");
}
