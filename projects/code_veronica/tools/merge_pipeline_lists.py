"""Merge recorded shader pipeline lists (shader_cache/*/pipelines.list) into one seed list.

The renderer (libs/video/rsx_d3d12_backend.c, "Shader preload") records every
pipeline it builds as guest programs plus a fixed-function key. Lists from
several play sessions, folders and compiler tags are interchangeable: they hold
no translated text, so any build rebuilds them with its own translator.

  python merge_pipeline_lists.py -o seed.list LIST [LIST ...]   merge
  python merge_pipeline_lists.py --stats LIST [LIST ...]        only count

Layout (little endian): header 'CVPL' + version 1, then records
type u32 (1 vertex program, 2 fragment pipeline), payload length u32,
FNV-1a 32 checksum of the payload u32, payload.
  VS payload: hash, vstart, vtex_mask, len, ucode
  FP payload: vs_hash, blend, nrt, rtfmt, exp32, ucode_hash, cmask, cull,
              cube_mask, fp_addr, len, ucode
"""
import argparse, struct, sys
from pathlib import Path

MAGIC, VERSION = 0x4C505643, 1
PL_MAX_VS, PL_MAX_FP = 2048, 8192   # the renderer's limits

def fnv(data: bytes) -> int:
    h = 2166136261
    for b in data:
        h = ((h ^ b) * 16777619) & 0xFFFFFFFF
    return h

def read_list(path: Path):
    """Yield (type, payload) for every intact record; stops at the first bad one."""
    d = path.read_bytes()
    if len(d) < 8 or struct.unpack_from('<II', d, 0) != (MAGIC, VERSION):
        return [], 0
    at, out, bad = 8, [], 0
    while at + 12 <= len(d):
        t, n, s = struct.unpack_from('<III', d, at)
        if n > len(d) - at - 12:
            bad = 1; break
        p = d[at + 12: at + 12 + n]
        if fnv(p) != s:
            bad = 1; break
        out.append((t, p)); at += 12 + n
    return out, bad or (at != len(d))

def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('lists', nargs='+', type=Path)
    ap.add_argument('-o', '--output', type=Path)
    ap.add_argument('--stats', action='store_true')
    a = ap.parse_args()
    if not a.stats and not a.output:
        ap.error('give -o OUTPUT or --stats')
    vs, fp = {}, {}
    for path in a.lists:
        recs, cut = read_list(path)
        nv = nf = 0
        for t, p in recs:
            if t == 1 and len(p) >= 16 and 16 + struct.unpack_from('<I', p, 12)[0] == len(p):
                if p[:4] not in vs: vs[p[:4]] = p; nv += 1
            elif t == 2 and len(p) >= 44 and 44 + struct.unpack_from('<I', p, 40)[0] == len(p):
                f = struct.unpack_from('<9I', p, 0)           # the renderer's pl_fp_id fields
                if f not in fp: fp[f] = p; nf += 1
        print(f'{path}: {len(recs)} records{" (cut off)" if cut else ""}, +{nv} vs, +{nf} fp', file=sys.stderr)
    # A pipeline is rebuilt only with its vertex program in the same list.
    have = {struct.unpack('<I', k)[0] for k in vs}
    orphan = [k for k, p in fp.items() if k[0] not in have]
    for k in orphan: del fp[k]
    print(f'unique: {len(vs)} vertex programs (limit {PL_MAX_VS}), {len(fp)} pipelines (limit {PL_MAX_FP}); dropped {len(orphan)} without a vertex program', file=sys.stderr)
    if a.stats: return
    body = bytearray(struct.pack('<II', MAGIC, VERSION))
    for t, items in ((1, vs.values()), (2, fp.values())):
        for p in items:
            body += struct.pack('<III', t, len(p), fnv(p)) + p
    a.output.write_bytes(body)
    print(f'wrote {a.output} ({len(body)} bytes)', file=sys.stderr)

if __name__ == '__main__':
    main()
