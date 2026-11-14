"""Turn render-profile.txt (CV_PROFILE_RENDER=1) into a list of the hottest functions.

  python perfil_renderizacao.py [render-profile.txt] [CodeVeronicaPort.exe]

The game samples its render thread about 1000 times a second and counts the
module offset it is in. This resolves those offsets to function names with
llvm-symbolizer and the PDB beside the exe, and adds up the samples per
function. Needs LLVM (llvm-symbolizer on PATH or in Program Files\LLVM\bin).
"""
import collections, shutil, subprocess, sys
from pathlib import Path

def main():
    prof = Path(sys.argv[1] if len(sys.argv) > 1 else 'render-profile.txt')
    exe = Path(sys.argv[2] if len(sys.argv) > 2 else 'build-release/bin/CodeVeronicaPort_US.exe')
    sym = shutil.which('llvm-symbolizer') or r'C:\Program Files\LLVM\bin\llvm-symbolizer.exe'
    lines = prof.read_text().splitlines()
    head = lines[0]
    rows = [(int(a), b) for a, b in (l.split() for l in lines[1:] if l.strip())]
    total = int(head.split('samples=')[1].split()[0])
    out = subprocess.run([sym, '--obj=' + str(exe), '--relative-address', '--inlining=false', '--demangle'],
                         input='\n'.join(b for _, b in rows) + '\n', capture_output=True, text=True).stdout
    blocks = [b for b in out.split('\n\n') if b.strip()]
    fn = collections.Counter()
    for (n, _), blk in zip(rows, blocks):
        name = blk.strip().splitlines()[0] if blk.strip() else '?'
        fn[name] += n
    print(head)
    print('(share of ALL samples; the rest is time waiting outside the game code)')
    for name, n in fn.most_common(30):
        print(f'{100.0 * n / total:5.1f}%  {name}')

main()
