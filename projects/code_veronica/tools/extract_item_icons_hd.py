#!/usr/bin/env python3
"""Crops the F6 item menu's HD icons from the community HD texture pack's two item pages.

  python tools/extract_item_icons_hd.py --pages docs/mockup-inventario/hd --out assets/items-hd

Page 0 = 80825AC3566791AE.png, page 1 = 0970338B235A8658.png (2048x2048). The cells are the ones
cv_srt_icons.cpp sprite() uses (102x72 in the 768x768 original atlas), scaled by 8/3: 272x192, x = col*272,
y = row*192, width = span*272. The blue key colour is removed as extract() does there (dark blue only; Alfred's
Ring and Jewel keep their enclosed key-coloured pixels), the result is trimmed to its bounding box and written
as item_XX.png (XX = the item id in hex), long side capped at 340. Only the ids the menu keeps are written
(KEEP below mirrors cv_items.cpp: every id with a sprite, minus the Side Pack and the unlisted OTHER items).
The pack is credited in THIRD-PARTY-NOTICES.md. The result is committed: the build only embeds the PNGs."""
import argparse
import os
import re
import sys
from collections import deque

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
SOURCE = os.path.join(HERE, "..", "cv_srt_icons.cpp")
PAGES = ["80825AC3566791AE.png", "0970338B235A8658.png"]
CELL_W, CELL_H = 272, 192
CAP = 340
DIVIDER = {0x46, 0x4d, 0x54, 0x5b, 0x62, 0x69, 0x70, 0x77, 0x7e, 0x91}

WEAPONS = [9, 0x83, 0x0A, 5, 0x21, 4, 0x20, 6, 7, 3, 2, 0x22, 0x8E, 1, 0x0B, 8]
AMMO = [0x0C, 0x0D, 0x1E, 0x0E, 0x0F, 0x10, 0x11, 0x25, 0x12, 0x24, 0x23, 0x82, 0x28, 0x2A]
HEALING = list(range(0x14, 0x1E))
OTHER = [0x91, 0x1F, 0x6A, 0x3F, 0x80, 0x81]
NOT_KEY = {0x39, 0x7D, 0x8D, 0x92, 0x93, 0x94, 0x97}


def entries():
    text = open(SOURCE, encoding="utf-8").read()
    block = text[text.index("static const Entry entries[]"):text.index("for(const auto& e:entries)")]
    out = {}
    for m in re.finditer(r"\{(0x[0-9a-fA-F]+|\d+),(\d),(\d+),(\d+),(\d+)\}", block):
        out[int(m.group(1), 0)] = tuple(int(m.group(i)) for i in range(2, 6))
    return out


def keep_ids(sprites):
    fixed = set(WEAPONS + AMMO + HEALING + OTHER)
    keys = {i for i in range(0x13, 0x9A) if not (0x13 < i < 0x27) and i not in NOT_KEY and i not in fixed}
    return sorted(i for i in (fixed | keys) if i in sprites)


def is_key(r, g, b):
    return r <= 24 and g <= 16 and 32 <= b <= 112 and b >= r + 24 and b >= g + 24


def crop(page, cell, item_id):
    _, row, col, span = cell
    box = (col * CELL_W, row * CELL_H, (col + span) * CELL_W, (row + 1) * CELL_H)
    img = page.crop(box).convert("RGBA")
    w, h = img.size
    px = img.load()
    enclosed = item_id in (0x2D, 0x2E)
    exterior = None
    if enclosed:   # only the key colour connected to the border goes
        exterior = [[False] * w for _ in range(h)]
        queue = deque()
        for x in range(w):
            queue.append((x, 0)); queue.append((x, h - 1))
        for y in range(h):
            queue.append((0, y)); queue.append((w - 1, y))
        while queue:
            x, y = queue.popleft()
            if not (0 <= x < w and 0 <= y < h) or exterior[y][x]:
                continue
            r, g, b, _ = px[x, y]
            if not is_key(r, g, b):
                continue
            exterior[y][x] = True
            queue.extend(((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)))
    for y in range(h):
        for x in range(w):
            r, g, b, a = px[x, y]
            if a == 0 or (is_key(r, g, b) and (not enclosed or exterior[y][x])):
                px[x, y] = (0, 0, 0, 0)
            elif item_id in DIVIDER and x >= w - 3:
                px[x, y] = (0, 0, 0, 0)
    bbox = img.getchannel("A").getbbox()
    if not bbox:
        return None
    img = img.crop(bbox)
    if max(img.size) > CAP:
        s = CAP / max(img.size)
        img = img.resize((max(1, round(img.width * s)), max(1, round(img.height * s))), Image.LANCZOS)
    return img


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--pages", required=True, help="folder with the two 2048x2048 item pages")
    ap.add_argument("--out", required=True)
    args = ap.parse_args()
    sprites = entries()
    pages = [Image.open(os.path.join(args.pages, n)).convert("RGBA") for n in PAGES]
    os.makedirs(args.out, exist_ok=True)
    ids = keep_ids(sprites)
    for item_id in ids:
        img = crop(pages[sprites[item_id][0]], sprites[item_id], item_id)
        if img is None:
            print("empty cell for id 0x%02X" % item_id, file=sys.stderr)
            continue
        img.save(os.path.join(args.out, "item_%02x.png" % item_id), optimize=True)
    print("wrote %d icons to %s" % (len(ids), args.out))


if __name__ == "__main__":
    main()
