#!/usr/bin/env python3
"""Generate the complete Code Veronica SPU folder from your own EBOOT.elf.

    python tools/cv_make_spu.py <EBOOT.elf> [--out projects/code_veronica/spu]

One command, no hand steps. It runs, in order:

  1. tools/extract_spu_images.py   -> <out>/images (the 25 embedded SPU ELFs)
  2. tools/build_spu_workloads.py  -> lifts them into <out>/spu_XXXX_at_*/ and
                                      writes <out>/spu_workloads.c
  3. extracts the three SPURS job binaries the scanner cannot see (raw code, no
     ELF header) from the EBOOT by virtual address, and checks their FNV-1a-64
     against the known hashes (a mismatch means a different game version)
  4. tools/wrap_spu_elf.py + tools/spu_lifter.py for each job
  5. tools/rebase_spu_elf.py + tools/spu_lifter.py for the sound driver's
     plug-in (spu_0023), which runs at LS 0x30000
  6. patches spu_workloads.c to register the jobs and the plug-in

Without steps 3-6 the game boots to a black screen (SPURS dispatch MISS, the
PPU waits forever on an event flag). This file only holds addresses, sizes and
hashes; every byte comes from the EBOOT you pass in.

Then build with build-release.cmd.
"""
import argparse
import os
import shutil
import struct
import subprocess
import sys
import tempfile

TOOLS = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(TOOLS)
DEFAULT_OUT = os.path.join(REPO, "projects", "code_veronica", "spu")

# Images whose interrupt-vector handler --auto-functions cannot reach.
EXTRA_FUNCS = ["spu_0019_at_00A25400=0x3110", "spu_0021_at_00A2C200=0x3110"]

# SPURS jobs embedded as raw code: (vaddr, size, fnv1a64, lift dir, prefix,
# image id, registry name). Lifted at LS base 0, entry 0.
JOBS = [
    (0x00A3CC80, 8384, 0x5AAD12B917A076FE, "code_veronica_job_5aad",
     "cv_job_", 201, "CodeVeronica_JOBCRT_8384"),
    (0x0094F280, 528, 0x8E52E3DE04D5C7A2, "code_veronica_job_8e52",
     "cv_job_small_", 202, "CodeVeronica_SPURSJob_528"),
    (0x00A23C80, 3728, 0x0DA78AE320040FBA, "edge_job_0DA78AE320040FBA",
     "cv_edge_job_", 26, "code_veronica.edge_job"),
]

# Sound driver plug-in: streamed by spu_0023 to LS 0x30080.
PLUGIN_SRC = "spu_0023_at_00A3F480.elf"
PLUGIN_BASE = 0x30000
PLUGIN_DIR = "spu_0023r_at_30000"
PLUGIN_PREFIX = "spu_0023r_"
PLUGIN_EXTRA = "0x30090,0x30F28"
PLUGIN_SIG_ADDR = 0x30080
PLUGIN_SIG = bytes([0x43, 0x49, 0x68, 0x02, 0x42, 0x26, 0xF2, 0x82,
                    0x42, 0x48, 0xD1, 0x02, 0x42, 0x76, 0x07, 0x82])
PLUGIN_SPAN = 0x1F80
PLUGIN_IMAGE_ID = 60

PATCH_MARKER = "cv_job_spu_recomp_register"


class Fail(Exception):
    pass


def fnv1a64(data):
    # Same as runtime/spu/spu_workload.c.
    h = 1469598103934665603
    for c in data:
        h ^= c
        h = (h * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    return h


def _short(a):
    if not os.path.isabs(a):
        return a
    try:
        return os.path.relpath(a, REPO)
    except ValueError:  # another drive on Windows
        return a


def run(args):
    cmd = [sys.executable] + args
    print("  $ " + " ".join(_short(a) for a in args))
    r = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                       text=True, errors="replace")
    if r.returncode != 0:
        sys.stdout.write(r.stdout)
        raise Fail(f"command failed ({r.returncode}): {' '.join(args[:1])}")
    return r.stdout


def ppu_vaddr_to_offset(elf, vaddr, size):
    """File offset of [vaddr, vaddr+size) in a big-endian ELF64 PPU image."""
    if elf[:4] != b"\x7fELF" or elf[4] != 2 or elf[5] != 2:
        raise Fail("input is not a big-endian ELF64 (decrypt EBOOT.BIN to "
                   "EBOOT.elf first, e.g. with RPCS3)")
    phoff, = struct.unpack_from(">Q", elf, 0x20)
    phentsize, phnum = struct.unpack_from(">HH", elf, 0x36)
    for i in range(phnum):
        p = phoff + i * phentsize
        p_type, = struct.unpack_from(">I", elf, p)
        p_offset, p_vaddr, _, p_filesz = struct.unpack_from(">QQQQ", elf, p + 8)
        if p_type == 1 and p_vaddr <= vaddr and vaddr + size <= p_vaddr + p_filesz:
            return p_offset + (vaddr - p_vaddr)
    raise Fail(f"vaddr 0x{vaddr:08X} (+{size}) is not in any loadable segment")


def spu_ls_bytes(elf, addr, size):
    """Bytes at local-store address addr in a big-endian ELF32 SPU image."""
    phoff, = struct.unpack_from(">I", elf, 0x1C)
    phentsize, phnum = struct.unpack_from(">HH", elf, 0x2A)
    for i in range(phnum):
        p = phoff + i * phentsize
        p_type, p_offset, p_vaddr, _, p_filesz = struct.unpack_from(">IIIII", elf, p)
        if p_type == 1 and p_vaddr <= addr and addr + size <= p_vaddr + p_filesz:
            o = p_offset + (addr - p_vaddr)
            return elf[o:o + size]
    return b""


def patch_workloads(path):
    txt = open(path, encoding="utf-8").read()
    if PATCH_MARKER in txt:
        print("  spu_workloads.c already patched")
        return

    externs = []
    for _, _, _, _, prefix, _, _ in JOBS:
        if not externs:
            externs.append(f"extern void {PLUGIN_PREFIX}spu_recomp_register(void);")
        externs.append(f"extern void {prefix}spu_recomp_register(void);")
        externs.append(f"extern void {prefix}spu_func_00000000(spu_context*);")
    externs.append("extern void spu_overlay_register_sig_region(const unsigned char sig[16], "
                   "unsigned span, int image_id);")

    body = [
        "    /* Hand-added: SPURS jobs embedded in the EBOOT, lifted at LS base 0 from",
        "     * the exact bytes their descriptors point at (FNV-1a-64 below):",
        "     * JOBCRT (8384 bytes at 0x00A3CC80), a 528-byte job (0x0094F280) and a",
        "     * raw Edge job (3728 bytes at 0x00A23C80). Without them the game hangs",
        "     * at boot waiting on SPURS event flags. */",
    ]
    for _, _, fp, _, prefix, img_id, name in JOBS:
        body.append(f"    spu_begin_image({img_id}); {prefix}spu_recomp_register();")
        body.append(f"    spu_workload_register_img(0x{fp:016X}ULL,")
        body.append(f"        {prefix}spu_func_00000000, {img_id}, \"{name}\");")
    sig = ",".join(f"0x{b:02X}" for b in PLUGIN_SIG)
    body += [
        "    /* Hand-added: the sound driver (spu_0023) copies its plugin to the heap and",
        "     * streams it to LS 0x30080; lifted rebased to 0x30000 (tools/rebase_spu_elf.py,",
        "     * then spu_lifter --extra-funcs 0x30090,0x30F28 --symbol-prefix spu_0023r_)",
        "     * so it runs compiled instead of through the interpreter. */",
        f"    spu_begin_image({PLUGIN_IMAGE_ID}); {PLUGIN_PREFIX}spu_recomp_register();",
        f"    {{ static const unsigned char sig[16] = {{{sig}}};",
        f"      spu_overlay_register_sig_region(sig, 0x{PLUGIN_SPAN:X}, {PLUGIN_IMAGE_ID}); }}",
    ]

    lines = txt.split("\n")
    # Externs go after the generated per-image extern block.
    last_extern = max(i for i, l in enumerate(lines)
                      if l.startswith("extern void spu_") and "_spu_recomp_register(void);" in l)
    lines[last_extern + 1:last_extern + 1] = externs
    # Registrations go before the final spu_begin_image(0).
    end = max(i for i, l in enumerate(lines) if l.strip() == "spu_begin_image(0);")
    lines[end:end] = body
    open(path, "w", encoding="utf-8", newline="\n").write("\n".join(lines))
    print("  spu_workloads.c patched: images 201, 202, 26 and 60 registered")


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("eboot", help="decrypted EBOOT.elf of Resident Evil Code: Veronica X HD")
    ap.add_argument("--out", default=DEFAULT_OUT,
                    help="output folder (default: projects/code_veronica/spu)")
    args = ap.parse_args()

    out = os.path.abspath(args.out)
    images = os.path.join(out, "images")
    eboot = open(args.eboot, "rb").read()

    # Check the job hashes first: a wrong EBOOT fails in a second, not after
    # a few minutes of lifting.
    print("[1/7] checking the SPURS job binaries in the EBOOT")
    jobs = []
    for vaddr, size, want, *rest in JOBS:
        off = ppu_vaddr_to_offset(eboot, vaddr, size)
        data = eboot[off:off + size]
        got = fnv1a64(data)
        if got != want:
            raise Fail(f"job at 0x{vaddr:08X} ({size} bytes) has FNV-1a-64 0x{got:016X}, "
                       f"expected 0x{want:016X}.\n  This EBOOT is a different game "
                       f"version than the one this port supports.")
        print(f"  0x{vaddr:08X}  {size:5d} bytes  0x{got:016X}  OK")
        jobs.append((data, rest))

    print("[2/7] extracting the embedded SPU images")
    if os.path.isdir(images):
        shutil.rmtree(images)
    run([os.path.join(TOOLS, "extract_spu_images.py"), args.eboot, "--output", images])
    n = len([f for f in os.listdir(images) if f.endswith(".elf")])
    if n != 25:
        raise Fail(f"expected 25 SPU images, found {n} (wrong game version?)")
    print(f"  {n} images")

    print("[3/7] lifting the SPU images (about a minute)")
    cmd = [os.path.join(TOOLS, "build_spu_workloads.py"),
           "--images", images, "--lifted", out,
           "--out", os.path.join(out, "spu_workloads.c"),
           "--register-fn", "port_spu_register_all", "--title", "code_veronica",
           "--constructor", "--relift"]
    for e in EXTRA_FUNCS:
        cmd += ["--extra-funcs", e]
    run(cmd)

    work = tempfile.mkdtemp(prefix="cv_make_spu_")
    try:
        print("[4/7] lifting the SPURS jobs")
        for data, (lift_dir, prefix, _, _) in jobs:
            raw = os.path.join(work, lift_dir + ".bin")
            elf = os.path.join(work, lift_dir + ".elf")
            open(raw, "wb").write(data)
            run([os.path.join(TOOLS, "wrap_spu_elf.py"), raw, "--out", elf,
                 "--base", "0", "--entry", "0"])
            dst = os.path.join(out, lift_dir)
            os.makedirs(dst, exist_ok=True)
            run([os.path.join(TOOLS, "spu_lifter.py"), "--auto-functions", elf,
                 "--symbol-prefix", prefix, "-o", dst])

        print("[5/7] lifting the sound plug-in at LS 0x30000")
        rebased = os.path.join(work, "spu_0023r.elf")
        run([os.path.join(TOOLS, "rebase_spu_elf.py"),
             os.path.join(images, PLUGIN_SRC), rebased, hex(PLUGIN_BASE)])
        sig = spu_ls_bytes(open(rebased, "rb").read(), PLUGIN_SIG_ADDR, len(PLUGIN_SIG))
        if sig != PLUGIN_SIG:
            raise Fail(f"sound plug-in bytes at LS 0x{PLUGIN_SIG_ADDR:X} do not match "
                       f"(got {sig.hex(' ')}); wrong game version?")
        dst = os.path.join(out, PLUGIN_DIR)
        os.makedirs(dst, exist_ok=True)
        run([os.path.join(TOOLS, "spu_lifter.py"), "--auto-functions", rebased,
             "--symbol-prefix", PLUGIN_PREFIX, "--extra-funcs", PLUGIN_EXTRA, "-o", dst])
    finally:
        shutil.rmtree(work, ignore_errors=True)

    print("[6/7] registering the jobs and the plug-in in spu_workloads.c")
    patch_workloads(os.path.join(out, "spu_workloads.c"))

    lifted = sorted(d for d in os.listdir(out)
                    if os.path.isfile(os.path.join(out, d, "spu_recomp.c")))
    print("[7/7] done")
    print(f"  {out}")
    print(f"  {len(lifted)} lifted folders (25 images + 3 SPURS jobs + 1 plug-in)")
    print("  Next step: build-release.cmd")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Fail as e:
        sys.exit(f"cv_make_spu: ERROR: {e}")
