"""Second copy of the lifted SPU code for processors without AVX2 (CV_CPU_UNIFIED).

One executable runs on every processor: the lifted SPU images are compiled
twice, for AVX2 (the originals) and for SSE4.1 (this copy), and
spu_cpu_select.c registers the copy this processor can run. Here every symbol
the images define is renamed with a prefix (sse4_), so both copies link into
the same executable. Runtime symbols they only use keep their names.

  spu_cpu_variant.py --spu-dir <SPU_DIR> --out-dir <dir>

Writes <dir>/<image>/spu_recomp.{c,h} (renamed), spu_workloads_avx2.c and
spu_workloads_sse4.c (the registration table, without its constructor, as
port_spu_register_all and sse4_port_spu_register_all). Files whose content
did not change are left untouched, so the build does not recompile them.
"""
import argparse
import re
import sys
from pathlib import Path

PREFIX = 'sse4_'
IDENT = re.compile(r'\b[A-Za-z_]\w*\b')
# A file-scope definition: a line that starts at column 0 with a type, not
# static/typedef/extern, and names a function (name followed by '(').
DEFINITION = re.compile(r'^(?!static\b|typedef\b|extern\b|return\b|if\b|for\b|while\b)'
                        r'[A-Za-z_][\w \t\*]*?[\s\*]([A-Za-z_]\w*)\s*\(', re.M)
GLOBAL = re.compile(r'^(?!static\b|typedef\b|extern\b|return\b)'
                    r'[A-Za-z_][\w \t\*]*?[\s\*]([A-Za-z_]\w*)\s*(?:\[[^\]]*\])?\s*(?:=|;)', re.M)
CTOR = re.compile(r'(?:/\*[^*]*\*/\s*)?__attribute__\(\(constructor\)\)\s*static\s+void\s+'
                  r'port_spu_register_all_ctor\s*\(void\)\s*\{[^{}]*\}\s*')


def defined_names(source):
    names = set(DEFINITION.findall(source)) | set(GLOBAL.findall(source))
    return {n for n in names if n not in ('main',)}


def rename(text, names):
    return IDENT.sub(lambda m: PREFIX + m.group(0) if m.group(0) in names else m.group(0), text)


def write(path, text):
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.is_file() and path.read_text(encoding='utf-8') == text:
        return False
    path.write_text(text, encoding='utf-8', newline='\n')
    return True


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--spu-dir', required=True, type=Path)
    ap.add_argument('--out-dir', required=True, type=Path)
    args = ap.parse_args()

    images = sorted(p.parent for p in args.spu_dir.glob('*/spu_recomp.c'))
    if not images:
        sys.exit(f'No lifted SPU images in {args.spu_dir}')
    sources = {d: (d / 'spu_recomp.c').read_text(encoding='utf-8') for d in images}

    names, owner = set(), {}
    for d, src in sources.items():
        for n in defined_names(src):
            if n in owner:
                sys.exit(f'{n} is defined by both {owner[n]} and {d.name}')
            owner[n] = d.name
            names.add(n)

    workloads = (args.spu_dir / 'spu_workloads.c').read_text(encoding='utf-8')
    stripped, count = CTOR.subn('', workloads)
    if count != 1:
        sys.exit('spu_workloads.c: expected one port_spu_register_all_ctor constructor, found %d' % count)
    names.add('port_spu_register_all')

    changed = 0
    for d, src in sources.items():
        changed += write(args.out_dir / d.name / 'spu_recomp.c', rename(src, names))
        header = d / 'spu_recomp.h'
        if header.is_file():
            changed += write(args.out_dir / d.name / 'spu_recomp.h',
                             rename(header.read_text(encoding='utf-8'), names))
    changed += write(args.out_dir / 'spu_workloads_avx2.c', stripped)
    changed += write(args.out_dir / 'spu_workloads_sse4.c', rename(stripped, names))
    print(f'spu_cpu_variant: {len(images)} images, {len(names)} symbols renamed with {PREFIX}, '
          f'{changed} file(s) updated in {args.out_dir}')


if __name__ == '__main__':
    main()
