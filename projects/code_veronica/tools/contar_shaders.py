"""Count captured pipelines that the shipped list does not have yet.

  python contar_shaders.py SEED CACHE_DIR

Prints one number: distinct fragment pipelines in CACHE_DIR/*/pipelines.list
(the game's own recordings) whose identity is not in SEED. Used by
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
    seed = fp_ids(Path(sys.argv[1]))
    found = set()
    for p in Path(sys.argv[2]).glob('*/pipelines.list'):
        found |= fp_ids(p)
    print(len(found - seed))

if __name__ == '__main__':
    main()
