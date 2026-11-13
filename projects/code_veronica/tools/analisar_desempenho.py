"""Summarise the [PERF] lines the game writes to run-log.txt (PERF=1).

  python analisar_desempenho.py [run-log.txt ...]

Each [PERF] line covers 20 frames: where the frame time went (guest = the
game's own logic on the CPU, render_frame = our renderer, gpuwait = waiting for
the GPU, prepass = texture/pipeline preparation, tex/vtx = uploads). The
report gives the average share of each and the slowest windows, so it is
clear what to optimise first. Never needs the game files.
"""
import re, sys, statistics
from pathlib import Path

LINE = re.compile(r'\[PERF\] ([\d.]+) fps \| tex ([\d.]+)s \((\d+)%.*?\| vtx ([\d.]+)s \((\d+)%.*?\| render_frame ([\d.]+)s \((\d+)%\) \[gpuwait ([\d.]+)s (\d+)%\] \| prepass ([\d.]+)s \((\d+)%\).*?pso ([\d.]+)s (\d+)% (\d+) calls (\d+) MISS.*?\| guest ([\d.]+)s \((\d+)%\)')
FIELDS = [('guest (game logic, CPU)', 17), ('render_frame (our renderer)', 7), ('  of which gpuwait', 9),
          ('prepass (textures/pipelines)', 11), ('  of which pso (pipelines)', 13), ('texture uploads', 3), ('vertex work', 5)]

def main():
    files = [Path(a) for a in sys.argv[1:]] or [Path('run-log.txt'), Path('build-release/bin/run-log.txt')]
    rows = []
    for f in files:
        if f.is_file():
            for line in f.read_text(errors='replace').splitlines():
                m = LINE.search(line)
                if m: rows.append([float(x) for x in m.groups()])
    if not rows:
        print('No [PERF] lines found. Play with capturar-shaders-live.cmd (it sets PERF=1), then run this again.')
        return
    fps = [r[0] for r in rows]
    print(f'{len(rows)} windows of 20 frames. FPS: average {statistics.mean(fps):.1f}, '
          f'median {statistics.median(fps):.1f}, slowest window {min(fps):.1f}')
    print('\nAverage share of the frame time (the biggest is what to optimise):')
    for name, i in sorted(FIELDS, key=lambda x: -statistics.mean(r[x[1]] for r in rows)):
        print(f'  {statistics.mean(r[i] for r in rows):5.1f}%  {name}')
    miss = sum(r[14] for r in rows)
    print(f'\nPipeline cache misses: {int(miss)} in total' + ('  <- compiles during play (more shaders to capture)' if miss else ''))
    print('\nSlowest windows (fps, guest%, render%, prepass%):')
    for r in sorted(rows)[:5]:
        print(f'  {r[0]:5.1f} fps   guest {r[17]:.0f}%   render {r[7]:.0f}%   prepass {r[11]:.0f}%   pso misses {int(r[14])}')

main()
