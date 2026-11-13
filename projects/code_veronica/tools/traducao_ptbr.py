#!/usr/bin/env python3
"""PT-BR translation of Code Veronica: review and correction.

The translation is distributed only as patches (localization/<title>/**/*.cvp
over the original room archives and localization/<title>/sysmes.msb). This
tool reads the original game files from the user's installation (read-only),
applies the patches in memory and:

  extrair    writes the review file: every translated text next to the
             original English (localization/revisao-ptbr.md).
  aplicar    applies a corrections file (localization/correcoes-ptbr.tsv)
             to EU and US and regenerates the .cvp and sysmes.msb.
  referencias writes the translated archives (test fixtures, NOT for
             distribution) for code_veronica_translation_patch_tests.

Text notation (exact and reversible, used in the corrections file):
  space = FF01 word space, '|' = line break (FF00), '·' = glyph 0 (wide space),
  accented letters = PT-BR atlas glyph + invisible advance slot
  (cv_ptbr_spacing.h), {xxxx} = control or symbol, {ff02:nnnn} = pause,
  {ff03:nn} = item name, {ff04:nnnn} = color/variable, {ffff} = end of page.

Usage:
  python tools/traducao_ptbr.py extrair --jogo D:/.../dev_hdd0/game
  python tools/traducao_ptbr.py aplicar --jogo D:/.../dev_hdd0/game
  python tools/traducao_ptbr.py referencias --jogo D:/.../dev_hdd0/game --saida <temp>
"""
import argparse, hashlib, os, re, struct, sys, zlib

TITLES = ("NPEB00553", "NPUB30467")
HERE = os.path.dirname(os.path.abspath(__file__))
LOCALIZATION = os.path.normpath(os.path.join(HERE, "..", "localization"))
TEXT_TYPES = (0x46c78353, 0x6a76e771)   # room message table, adv menu messages

# ---------------------------------------------------------------- font
# PT-BR atlas (ptbr-textures/*/1F496ECD75E79D71_768x768_DXT5.png), 18 glyphs
# per row. Accented glyphs have no advance of their own: the next code is an
# invisible slot that takes the base letter's width (cv_ptbr_spacing.h).
ACCENTS = {0x74: 'à', 0x75: 'á', 0x76: 'â', 0x77: 'ä', 0x79: 'è', 0x7a: 'é', 0x7b: 'ê', 0x7c: 'ë', 0x7d: 'ì',
           0x7e: 'í', 0x7f: 'î', 0x80: 'ï', 0x81: 'ò', 0x82: 'ó', 0x83: 'ô', 0x84: 'ö', 0x85: 'ù', 0x86: 'ú',
           0x87: 'û', 0x88: 'ü', 0x8a: 'ç', 0x8f: 'Á', 0x90: 'À', 0x91: 'Â', 0x92: 'É', 0x93: 'Ê', 0x94: 'Í',
           0x95: 'Ó', 0x96: 'Ô', 0x97: 'Ú', 0x98: 'Ç', 0x99: 'Ã', 0x9a: 'Õ', 0xa0: 'ã', 0xa1: 'õ'}
ACCENT_CODE = {v: k for k, v in ACCENTS.items()}
_BASE = {'a': 'àáâäãòóôöõ', 'e': 'èéêë', 'i': 'ìíîï', 'u': 'ùúûü', 'c': 'ç'}
_SLOT = {'a': 0x68, 'e': 0x69, 'i': 0x6a, 'u': 0x6b, 'c': 0x40}


def advance_slot(ch):
    if ch == 'Í':           # the existing translation gives narrow I-acute the e slot
        return 0x69
    if ch.isupper():
        return 0x00
    for base, letters in _BASE.items():
        if ch in letters:
            return _SLOT[base]
    raise ValueError(ch)


PLAIN = {0x00: '·'}
for _c in range(1, 0x3b):
    if chr(_c + 0x20) not in "#$&*<=>@":
        PLAIN[_c] = chr(_c + 0x20)
for _c in range(0x41, 0x5b):
    PLAIN[_c] = chr(_c).lower()
PLAIN_CODE = {v: k for k, v in PLAIN.items()}
WITH_ARGUMENT = (0xff02, 0xff03, 0xff04)


def decode(ws):
    out, k = [], 0
    while k < len(ws):
        w = ws[k]
        if w in ACCENTS and k + 1 < len(ws) and ws[k + 1] == advance_slot(ACCENTS[w]):
            out.append(ACCENTS[w]); k += 2; continue
        if w == 0xff01: out.append(' ')
        elif w == 0xff00: out.append('|')
        elif w in WITH_ARGUMENT and k + 1 < len(ws):
            out.append('{%04x:%02x}' % (w, ws[k + 1]) if w == 0xff03 else '{%04x:%04x}' % (w, ws[k + 1])); k += 1
        elif w in PLAIN: out.append(PLAIN[w])
        else: out.append('{%04x}' % w if w > 0xff else '{%02x}' % w)
        k += 1
    return ''.join(out)


def encode(text):
    ws, k = [], 0
    while k < len(text):
        ch = text[k]
        if ch == '{':
            end = text.index('}', k)
            ws += [int(part, 16) for part in text[k + 1:end].split(':')]
            k = end + 1; continue
        if ch == ' ': ws.append(0xff01)
        elif ch == '|': ws.append(0xff00)
        elif ch in ACCENT_CODE: ws += [ACCENT_CODE[ch], advance_slot(ch)]
        elif ch in PLAIN_CODE: ws.append(PLAIN_CODE[ch])
        else: raise ValueError("character %r is not in the PT-BR font: %r" % (ch, text))
        k += 1
    return ws


def readable(text):
    text = re.sub(r'\{ff02:[0-9a-f]+\}', ' ', text)
    text = text.replace('{ffff}', ' ¶ ').replace('·', ' ').replace('|', ' ')
    return re.sub(r' +', ' ', text).replace(' ¶', ' ¶').strip(' ¶')


# ---------------------------------------------------------------- containers
def be32(b, a): return struct.unpack_from(">I", b, a)[0]
def sha(b): return hashlib.sha256(b).hexdigest()
def words(m): return list(struct.unpack(">%dH" % (len(m) // 2), m[:len(m) // 2 * 2]))
def pack_words(ws): return struct.pack(">%dH" % len(ws), *ws)

_ROT = [7, 2, 5, 4, 6, 3, 1, 4, 2, 6, 7, 3, 5, 7, 1, 5]
_SEED = [135, 85, 7, 181, 75, 4, 165, 174, 199, 103, 190, 203, 1, 80, 88, 68]


def sfh_unwrap(b):
    if be32(b, 0) != 0x00534648:
        return b, False
    assert be32(b, 4) == 0x00010001
    size, out, at = be32(b, 8), bytearray(), 0
    while at < len(b) and len(out) < size:
        n = min(0x1fff0, len(b) - at - 16, size - len(out)); out += b[at + 16:at + 16 + n]; at += 0x20000
    assert len(out) == size
    return bytes(out), True


def sfh_wrap(logical):
    out = bytearray(struct.pack(">III", 0x00534648, 0x10001, len(logical)) + bytes(4))
    for p in range(0, len(logical), 0x1fff0):
        chunk = logical[p:p + 0x1fff0]; chunk += bytes(-len(chunk) % 16)
        check = bytearray(16)
        for j in range(16):
            v = (_SEED[j] + sum(chunk[j::16])) & 255; check[j] = ((v << _ROT[j]) | (v >> (8 - _ROT[j]))) & 255
        out += chunk + check
    return bytes(out)


def arc_entries(logical):
    assert be32(logical, 0) == 0x00435241 and struct.unpack_from(">H", logical, 4)[0] == 8
    entries = []
    for i in range(struct.unpack_from(">H", logical, 6)[0]):
        a = 8 + i * 80
        typ, comp, cap, off = struct.unpack_from(">IIII", logical, a + 64)
        entries.append(dict(index=i, name=logical[a:a + 64].split(b"\0")[0].decode("latin1"),
                            type=typ, comp=comp, cap=cap, off=off))
    return entries


def arc_data(logical, e):
    raw = logical[e['off']:e['off'] + e['comp']]
    try: return zlib.decompress(raw)
    except zlib.error: return raw


def arc_replace(logical, e, data, original_size):
    """Replace one resource (zlib level 9), shifting the later ones. Same layout
    as the existing patches: the archive keeps the original logical size while
    the content fits and grows tight past it."""
    entries = arc_entries(logical)
    raw = zlib.compress(data, 9)
    delta = len(raw) - e['comp']
    end = max(x['off'] + x['comp'] for x in entries)
    out = bytearray(logical[:e['off']] + raw + logical[e['off'] + e['comp']:end])
    for x in entries:
        a = 8 + x['index'] * 80
        if x['index'] == e['index']:
            struct.pack_into(">II", out, a + 68, len(raw), (len(data) << 3) | (x['cap'] & 7))
        elif x['off'] > e['off']:
            struct.pack_into(">I", out, a + 76, x['off'] + delta)
    assert len(out) == end + delta
    if len(out) < original_size:
        out += bytes(original_size - len(out))
    return bytes(out)


def message_table(data, base=0, end=None):
    """Messages as bytes. An offset past the end of the table (sysmes.msb section 1 ends with a
    junk 0xBEBExxxx entry) is not a message: it is kept as its raw int so the table rebuilds exactly."""
    count = be32(data, base)
    offs = [be32(data, base + 4 + 4 * i) for i in range(count)]
    stop = len(data) - base if end is None else end
    valid = [o if o <= stop else None for o in offs]
    out = []
    for i, o in enumerate(offs):
        if valid[i] is None:
            out.append(o); continue
        nxt = next((v for v in valid[i + 1:] if v is not None), stop)
        out.append(data[base + o:base + nxt])
    return out


def build_table(messages):
    head = 4 + 4 * len(messages); out = bytearray(struct.pack(">I", len(messages)))
    at = head
    for m in messages:
        if isinstance(m, int):
            out += struct.pack(">I", m); continue
        out += struct.pack(">I", at); at += len(m)
    return bytes(out) + b"".join(m for m in messages if not isinstance(m, int))


def sysmes_sections(logical):
    sections, p = [], 0
    while be32(logical, p) != 0xffffffff:
        size = be32(logical, p); sections.append(message_table(logical, p + 4, size)); p += 4 + size
    assert p + 4 == len(logical)
    return sections


def build_sysmes(sections):
    out = bytearray()
    for messages in sections:
        t = build_table(messages); out += struct.pack(">I", len(t)) + t
    return bytes(out + b"\xff\xff\xff\xff")


# ---------------------------------------------------------------- patches
def patch_apply(base, patch):
    if patch[:8] != b"CVPTBR01": raise ValueError("not a CVPTBR01 patch")
    old, new, count = struct.unpack_from("<III", patch, 8)
    if old != len(base) or sha(base).encode() != patch[20:84]:
        raise ValueError("original file differs from the patch base")
    out, at = bytearray(), 148
    for _ in range(count):
        kind = patch[at]; off, n = struct.unpack_from("<II", patch, at + 1); at += 9
        if kind == 0: out += base[off:off + n]
        else: out += patch[at:at + n]; at += n
    if at != len(patch) or len(out) != new or sha(out).encode() != patch[84:148]:
        raise ValueError("patch result does not match")
    return bytes(out)


def patch_make(base, target, window=32):
    """Bounded COPY/literal records (format of cv_translation_patch.h)."""
    index = {}
    for j in range(0, len(base) - window + 1, window):
        index.setdefault(base[j:j + window], j)
    records, literal, i = [], bytearray(), 0

    def flush():
        if literal: records.append((1, 0, bytes(literal))); literal.clear()
    while i < len(target):
        j = index.get(target[i:i + window]) if i + window <= len(target) else None
        if j is None:
            literal.append(target[i]); i += 1; continue
        back = 0                     # grow the match backwards into the pending literal
        while back < len(literal) and j - back > 0 and base[j - back - 1] == literal[-1 - back]:
            back += 1
        if back: del literal[len(literal) - back:]
        flush()
        n = window + back; s, d = j - back, i - back
        while d + n < len(target) and s + n < len(base) and target[d + n] == base[s + n]:
            n += 1
        records.append((0, s, n)); i = d + n
    flush()
    out = bytearray(b"CVPTBR01" + struct.pack("<III", len(base), len(target), len(records)))
    out += sha(base).encode() + sha(target).encode()
    for kind, off, payload in records:
        if kind == 0: out += struct.pack("<BII", 0, off, payload)
        else: out += struct.pack("<BII", 1, 0, len(payload)) + payload
    assert patch_apply(base, bytes(out)) == target
    return bytes(out)


# ---------------------------------------------------------------- game files
def original_path(game, title, rel):
    for prefix in ("", "PS3_GAME"):
        p = os.path.join(game, title, prefix, "USRDIR/BHCV/nativePS3/biocv_disc",
                         "eng" if title == "NPEB00553" else "usa", rel)
        if os.path.isfile(p): return p
    raise FileNotFoundError("%s: %s not found under %s" % (title, rel, game))


def read(p):
    with open(p, "rb") as f: return f.read()


def patch_files(title):
    root = os.path.join(LOCALIZATION, title)
    for dp, _, fs in sorted(os.walk(root)):
        for f in sorted(fs):
            if f.endswith(".cvp"):
                yield os.path.relpath(os.path.join(dp, f), root)[:-4].replace("\\", "/")


def units(game, title):
    """Yield (key, original_messages, translated_messages) per text table."""
    for rel in patch_files(title):
        base = read(original_path(game, title, rel))
        new = patch_apply(base, read(os.path.join(LOCALIZATION, title, rel + ".cvp")))
        a, _ = sfh_unwrap(base); b, _ = sfh_unwrap(new)
        for x, y in zip(arc_entries(a), arc_entries(b)):
            if x['type'] in TEXT_TYPES:
                yield "%s#%d" % (rel, x['index']), message_table(arc_data(a, x)), message_table(arc_data(b, y))
    o, _ = sfh_unwrap(read(original_path(game, title, "system/sysmes.msb")))
    n, _ = sfh_unwrap(read(os.path.join(LOCALIZATION, title, "sysmes.msb")))
    for s, (x, y) in enumerate(zip(sysmes_sections(o), sysmes_sections(n))):
        yield "sysmes.msb#%d" % s, [m if isinstance(m, bytes) else b"" for m in x], [m if isinstance(m, bytes) else b"" for m in y]


# ---------------------------------------------------------------- commands
def cmd_extract(args):
    data = {t: list(units(args.jogo, t)) for t in TITLES}
    lines = ["# Revisão da tradução PT-BR — Code Veronica X", "",
             "Gerado por `tools/traducao_ptbr.py extrair` a partir dos patches de `localization/` e dos arquivos "
             "originais do jogo (somente leitura). Não contém arquivos do jogo, só o texto para revisão.", "",
             "Notação: `|` quebra de linha, `·` espaço largo, `{ffff}` fim de página, `{ff02:nnnn}` pausa, "
             "`{ff03:nn}` nome do item, `{ff04:nnnn}` cor. Correções vão em `localization/correcoes-ptbr.tsv` "
             "(arquivo, mensagem, trecho atual, trecho novo) e são aplicadas com `aplicar`.", ""]
    total = 0
    for (key, en, pt), (_, en_us, pt_us) in zip(data["NPEB00553"], data["NPUB30467"]):
        rows = []
        for i in range(max(len(en), len(pt))):
            e = decode(words(en[i])) if i < len(en) else ""
            p = decode(words(pt[i])) if i < len(pt) else ""
            if e == p and not args.todas: continue
            pu = decode(words(pt_us[i])) if i < len(pt_us) else ""
            rows.append((i, e, p, pu if pu != p else None,
                         decode(words(en_us[i])) if i < len(en_us) and en_us[i] != en[i] else None))
        if not rows: continue
        lines += ["## %s" % key, ""]
        for i, e, p, pu, eu in rows:
            total += 1
            lines.append("**%d** EN: %s  " % (i, readable(e)))
            if eu: lines.append("EN (US): %s  " % readable(eu))
            lines.append("PT: %s  " % readable(p))
            if pu: lines.append("PT (US): %s  " % readable(pu))
            if args.bruto:
                lines.append("<sub>`%s`</sub>" % p.replace("`", "'"))
            lines.append("")
    with open(args.saida, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines))
    print("%d translated messages -> %s" % (total, args.saida))


def load_corrections(path):
    fixes = []
    with open(path, encoding="utf-8") as f:
        for n, line in enumerate(f, 1):
            line = line.rstrip("\n")
            if not line or line.startswith("#"): continue
            cols = line.split("\t")
            if len(cols) != 4: raise ValueError("%s:%d: expected 4 tab-separated columns" % (path, n))
            fixes.append(dict(line=n, key=cols[0], index=int(cols[1]), old=cols[2], new=cols[3], used=set()))
    return fixes


def fix_messages(key, messages, fixes, title):
    changed = False
    for fx in fixes:
        if fx['key'] != key: continue
        m = messages[fx['index']]
        text = decode(words(m))
        if fx['new'] in text and (text.count(fx['old']) == text.count(fx['new']) if fx['old'] in fx['new']
                                  else fx['old'] not in text):
            fx['used'].add(title); continue          # already applied: the tool is idempotent
        if text.count(fx['old']) != 1:
            raise ValueError("line %d: %r occurs %d times in %s #%d (%s): %s"
                             % (fx['line'], fx['old'], text.count(fx['old']), key, fx['index'], title, text))
        new = encode(text.replace(fx['old'], fx['new']))
        tail = m[len(m) // 2 * 2:]
        messages[fx['index']] = pack_words(new) + tail
        fx['used'].add(title); changed = True
    return changed


def cmd_apply(args):
    fixes = load_corrections(args.correcoes)
    for title in TITLES:
        root = os.path.join(LOCALIZATION, title); written = 0
        for rel in patch_files(title):
            base = read(original_path(args.jogo, title, rel))
            cvp = os.path.join(root, rel + ".cvp")
            new = patch_apply(base, read(cvp))
            logical, wrapped = sfh_unwrap(new)
            original_size = len(sfh_unwrap(base)[0])
            dirty = False
            for e in arc_entries(logical):
                if e['type'] not in TEXT_TYPES: continue
                messages = message_table(arc_data(logical, e))
                if fix_messages("%s#%d" % (rel, e['index']), messages, fixes, title):
                    logical = arc_replace(logical, arc_entries(logical)[e['index']], build_table(messages), original_size)
                    dirty = True
            if dirty:
                result = sfh_wrap(logical) if wrapped else logical
                with open(cvp, "wb") as f: f.write(patch_make(base, result))
                written += 1
        path = os.path.join(root, "sysmes.msb")
        logical, _ = sfh_unwrap(read(path)); sections = sysmes_sections(logical)
        assert build_sysmes(sections) == logical
        dirty = False
        for s, messages in enumerate(sections):
            dirty |= fix_messages("sysmes.msb#%d" % s, messages, fixes, title)
        if dirty:
            with open(path, "wb") as f: f.write(sfh_wrap(build_sysmes(sections)))
            written += 1
        print("%s: %d files regenerated" % (title, written))
    unused = [fx for fx in fixes if set(TITLES) - fx['used']]   # EU and US must stay in sync
    for fx in unused:
        print("WARNING line %d not applied in %s" % (fx['line'], ", ".join(sorted(set(TITLES) - fx['used']))))
    if unused: sys.exit(1)


def cmd_references(args):
    for title in TITLES:
        for rel in patch_files(title):
            base = read(original_path(args.jogo, title, rel))
            out = os.path.join(args.saida, title, rel); os.makedirs(os.path.dirname(out), exist_ok=True)
            with open(out, "wb") as f: f.write(patch_apply(base, read(os.path.join(LOCALIZATION, title, rel + ".cvp"))))
    print("translated archives (test fixtures, do not distribute) -> %s" % args.saida)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    for name in ("extrair", "aplicar", "referencias"):
        p = sub.add_parser(name)
        p.add_argument("--jogo", required=True, help="folder holding NPEB00553/ and NPUB30467/ (e.g. dev_hdd0/game)")
        if name == "extrair":
            p.add_argument("--saida", default=os.path.join(LOCALIZATION, "revisao-ptbr.md"))
            p.add_argument("--todas", action="store_true", help="include untranslated messages")
            p.add_argument("--bruto", action="store_true", help="add the exact notation under each line")
        if name == "aplicar":
            p.add_argument("--correcoes", default=os.path.join(LOCALIZATION, "correcoes-ptbr.tsv"))
        if name == "referencias":
            p.add_argument("--saida", required=True)
    args = ap.parse_args()
    {"extrair": cmd_extract, "aplicar": cmd_apply, "referencias": cmd_references}[args.cmd](args)


if __name__ == "__main__":
    main()
