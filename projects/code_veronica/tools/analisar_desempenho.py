"""Summarise the [PERF] lines the game appends to perf-log.txt (PERF=1).

  python analisar_desempenho.py [perf-log.txt ...]

Each [PERF] line covers 20 frames: where the frame time went (guest = the
game's own logic on the CPU, render_frame = our renderer, gpuwait = waiting for
the GPU, prepass = texture/pipeline preparation, tex/vtx = uploads). The
report gives the average share of each and the slowest windows, so it is
clear what to optimise first. Never needs the game files.
"""
import re, sys, statistics
from pathlib import Path

STEPS = re.compile(r'steps\(% of frame\) pre ([\d.]+) prepass ([\d.]+) pso ([\d.]+) setpso ([\d.]+) vp ([\d.]+) tex ([\d.]+) const ([\d.]+) draw ([\d.]+)')
EXTRA = re.compile(r'wait (\d+)% record (\d+)% \| (?:steps.*?\| )?t=(\d+) draws=(\d+) res=(\d+)x(\d+)')
LINE = re.compile(r'\[PERF\] ([\d.]+) fps \| tex ([\d.]+)s \((\d+)%.*?\| vtx ([\d.]+)s \((\d+)%.*?\| render_frame ([\d.]+)s \((\d+)%\) \[gpuwait ([\d.]+)s (\d+)%\] \| prepass ([\d.]+)s \((\d+)%\).*?pso ([\d.]+)s (\d+)% (\d+) calls (\d+) MISS.*?\| guest ([\d.]+)s \((\d+)%\)')
FIELDS = [('guest (game logic, CPU)', 16), ('render_frame (our renderer)', 6), ('  of which gpuwait', 8),
          ('prepass (textures/pipelines)', 10), ('  of which pso (pipelines)', 12), ('texture uploads', 2), ('vertex work', 4)]

def main():
    files = [Path(a) for a in sys.argv[1:]] or [Path('perf-log.txt')]
    rows = []
    for f in files:
        if f.is_file():
            for line in f.read_text(errors='replace').splitlines():
                m = LINE.search(line)
                if m:
                    e = EXTRA.search(line)
                    rows.append([float(x) for x in m.groups()] + ([float(x) for x in e.groups()] if e else []))
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
    ext = [r for r in rows if len(r) >= 23]
    steps = []
    for f in files:
        if f.is_file():
            for line in f.read_text(errors='replace').splitlines():
                if LINE.search(line):
                    st = STEPS.search(line)
                    if st: steps.append([float(x) for x in st.groups()])
    if steps:
        names = ['checks + render-target switches', 'texture/pipeline prepass', 'pipeline lookup', 'SetPipelineState',
                 'viewport', 'texture bindings', 'constant bindings', 'draw call (incl. batching)']
        print('\nInside the draw loop, share of the whole frame time (what to optimise first):')
        for i in sorted(range(8), key=lambda k: -statistics.mean(r[k] for r in steps)):
            print(f'  {statistics.mean(r[i] for r in steps):5.1f}%  {names[i]}')
    if ext:
        print("\nThe renderer's own time, split (only in newer logs):")
        print(f'  {statistics.mean(r[17] for r in ext):5.1f}%  waiting for the GPU to finish the previous frame')
        print(f'  {statistics.mean(r[18] for r in ext):5.1f}%  recording the draw commands (CPU work per draw)')
        by = {}
        for r in ext: by.setdefault((int(r[21]), int(r[22])), []).append(r)
        if len(by) > 1:
            print("\nBy render resolution (does the picture size matter?):")
            for res, g in sorted(by.items()):
                print(f'  {res[0]}x{res[1]}: {len(g)} windows, {statistics.mean(r[0] for r in g):.1f} fps, '
                      f'{statistics.mean(r[20] for r in g):.0f} draws/frame')
    print('\nSlowest windows (fps, guest%, render%, prepass%):')
    for r in sorted(rows)[:5]:
        print(f'  {r[0]:5.1f} fps   guest {r[16]:.0f}%   render {r[6]:.0f}%   prepass {r[10]:.0f}%   pso misses {int(r[14])}')

main()
