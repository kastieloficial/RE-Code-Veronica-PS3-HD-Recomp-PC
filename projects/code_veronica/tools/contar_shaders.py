"""Count captured pipelines that the shipped list does not have yet.

  python contar_shaders.py SEED CACHE_DIR [CACHE_DIR ...]

Prints one number: distinct fragment pipelines in CACHE_DIR/*/pipelines.list
(the game's own recordings) whose identity is not in SEED. The project's own
shader_cache (beside shader-seed/) is always scanned too: the launcher starts
the game with projects/code_veronica as its working folder, so that is where
a game opened through the launcher records. Used by
capturar-shaders-live.cmd to show new shaders as they are captured. Reads
only intact records, so a list being written at the same moment is fine.
"""
import struct, sys
from pathlib import Path

MAGIC, VERSION = 0x4C505643, 1

def fp_ids(path: Path):
    try:
        d = path.read_bytes()
    except OSError:
        return set()
    if len(d) < 8 or struct.unpack_from('<II', d, 0) != (MAGIC, VERSION):
        return set()
    ids, at = set(), 8
    while at + 12 <= len(d):
        t, n, _ = struct.unpack_from('<III', d, at)
        if n > len(d) - at - 12:
            break
        if t == 2 and n >= 44:
            ids.add(d[at + 12: at + 12 + 36])   # the renderer's pl_fp_id fields
        at += 12 + n
    return ids

def main():
    seed_path = Path(sys.argv[1])
    seed = fp_ids(seed_path)
    dirs = [Path(a) for a in sys.argv[2:]] + [seed_path.resolve().parent.parent / 'shader_cache']
    found, seen = set(), set()
    for d in dirs:
        for p in d.glob('*/pipelines.list'):
            r = p.resolve()
            if r not in seen:
                seen.add(r); found |= fp_ids(p)
    print(len(found - seed))

if __name__ == '__main__':
    main()
