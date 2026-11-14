r"""Turn render-profile.txt (CV_PROFILE_RENDER=1) into a list of the hottest functions.

  python perfil_renderizacao.py [render-profile.txt] [CodeVeronicaPort_US.exe]

The game samples its render thread about 1000 times a second. A sample inside
the game's own code is counted by function. A sample inside Windows, the video
driver or the C runtime is counted against the game function that called it,
so time spent in getenv, memcpy or Direct3D calls is not hidden. Needs LLVM
(llvm-symbolizer on PATH or in Program Files, LLVM, bin) and the PDB beside the exe.
"""
import collections, shutil, subprocess, sys
from pathlib import Path

NL = chr(10)

def main():
    prof = Path(sys.argv[1] if len(sys.argv) > 1 else 'render-profile.txt')
    exe = Path(sys.argv[2] if len(sys.argv) > 2 else 'build-release/bin/CodeVeronicaPort_US.exe')
    sym = shutil.which('llvm-symbolizer') or r'C:\Program Files\LLVM\bin\llvm-symbolizer.exe'
    lines = prof.read_text().splitlines()
    head = lines[0]
    rows = []
    for l in lines[1:]:
        parts = l.split()
        if len(parts) == 3:
            rows.append((int(parts[0]), parts[1], parts[2]))
    total = int(head.split('samples=')[1].split()[0])
    addrs = sorted({a for _, _, a in rows if a != '0x0'})
    out = subprocess.run([sym, '--obj=' + str(exe), '--relative-address', '--inlining=false', '--demangle'],
                         input=NL.join(addrs) + NL, capture_output=True, text=True).stdout
    blocks = [b for b in out.split(NL + NL) if b.strip()]
    name_of = {a: (blk.strip().splitlines()[0] if blk.strip() else '?') for a, blk in zip(addrs, blocks)}
    fn = collections.Counter()
    for n, mod, a in rows:
        fname = name_of.get(a, '?') if a != '0x0' else 'unknown caller'
        fn[fname if mod == '-' else '[' + mod + '] called from ' + fname] += n
    print(head)
    print('(share of ALL samples; [module] lines are time inside Windows/driver/runtime, by calling function)')
    for name, n in fn.most_common(35):
        print(f'{100.0 * n / total:5.1f}%  {name}')

main()
