#!/usr/bin/env python3
"""Writes the localization's text pictures: the area name cards, the dungeon floor labels and the boss names.

    python tools/textpack/build.py --originals <extracted textures> --out <save folder>/lang/textures [--scale 1|4]

The game draws these words as textures, one set per language. The port takes them from
`textures/<language>/<name>.png` in its language folders (docs/LOCALIZATION.md, "Pictures of text"), so they
follow game.language. This tool makes those pictures from the words in `strings/*.json`: it erases the old
lettering from the disc's own textures and draws the language's words in the Dark Cloud Compendium font (the
message font), which is the face the cards are lettered in. The boss names are a serif; Cinzel stands in for it.

--originals is a folder of the disc's textures as PNG files, laid out `<game file with / as __>/<name>.png`
(`img_3__mt01.tm2/mt01.png`, `dun__img__us__dname00.img/floor00.png`, `gedit__s34__chara__boss_3_f.pak/boss_3.png`),
directly in it or in an `indexed/` or `rgba/` folder of it. Nothing from the disc is kept: the pictures are made
on the player's own machine, like the language export.

Needs Python 3 with numpy, Pillow and opencv-python.
"""
import argparse
import json
import os
import re
import sys

import cv2
import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import plates  # noqa: E402
import slots  # noqa: E402
import textpack  # noqa: E402

S = 4  # everything is drawn at 4x and brought back to the texture's size at the end
COMPENDIUM = os.path.join(HERE, '..', 'font', 'DarkCloudCompendium.ttf')
CINZEL = os.path.join(HERE, '..', 'font', 'Cinzel-Regular.ttf')
# language key in the strings files -> the language files' stems (UK and US English share the floor and boss pictures)
CODES = {'uk': ['en_gb'], 'us': ['en_us'], 'fr': ['fr_fr'], 'de': ['de_de'], 'it': ['it_it'], 'es': ['es_es']}
EN_BOTH = {'uk': ['en_gb', 'en_us'], 'us': ['en_us', 'en_gb']}


def strings(name):
    with open(os.path.join(HERE, 'strings', name), encoding='utf-8') as f:
        return json.load(f)


class Originals:
    def __init__(self, root):
        self.roots = [os.path.join(root, sub) for sub in ('indexed', 'rgba', '')]

    def load(self, game_file, name):
        """RGBA array scaled up by S, or None."""
        folder = game_file.replace('/', '__')
        for root in self.roots:
            path = os.path.join(root, folder, name + '.png')
            if os.path.isfile(path):
                a = np.asarray(Image.open(path).convert('RGBA'))
                return cv2.resize(a, (a.shape[1] * S, a.shape[0] * S), interpolation=cv2.INTER_CUBIC)
        return None


def save(out, code, name, rgba, scale):
    if scale == 1:
        rgba = cv2.resize(rgba, (rgba.shape[1] // S, rgba.shape[0] // S), interpolation=cv2.INTER_AREA)
    folder = os.path.join(out, code)
    os.makedirs(folder, exist_ok=True)
    Image.fromarray(rgba).save(os.path.join(folder, name + '.png'))


# ---- area name cards -------------------------------------------------------------------------------------------------

def sample_style(card):
    a = card.astype(np.float32)
    lum = a.mean(2)
    # The lettering's rows only, and its brightest 1%: a wider band or share picks up the scenery behind the name
    # (olive on the town cards, whose lettering is white).
    h = a.shape[0]
    band, bl = a[int(0.27 * h):int(0.69 * h)], lum[int(0.27 * h):int(0.69 * h)]
    fill = np.median(band[bl >= np.percentile(bl, 99)], 0)
    fl = fill.mean()
    sel = (bl > 0.18 * fl) & (bl < 0.5 * fl)
    sat = band.max(2) - band.min(2)
    if sel.any():
        sel &= sat > np.percentile(sat[sel], 40)
    glow = np.median(band[sel], 0) if sel.any() else fill * 0.5
    return tuple(fill), tuple(glow), tuple(np.clip(glow * 0.25, 0, 255))


def titles(orig, out, scale):
    words, styles = strings('titles.json'), strings('title_styles.json')
    dirs = {k: v for k, v in words['_lang_dirs'].items()}
    n = 0
    textpack.use_font_file(COMPENDIUM)
    for area in [k for k in words if re.match(r'mt\d+$', k)]:
        cards = {}
        for d in ['img'] + list(dirs.values()):
            c = orig.load('%s/%s.tm2' % (d, area), area)
            if c is not None:
                cards[d] = c[..., :3]
        if not cards:
            print('titles: no originals for', area)
            continue
        base = next(cards[d] for d in ('img_1', 'img_2', 'img_3', 'img_4', 'img_5', 'img_6', 'img') if d in cards)
        plate, _ = plates.build_plate(list(cards.values()))
        fill, glow, edge = sample_style(base)
        st = {'fill': fill, 'glow': glow, 'edge': edge}
        st.update(styles['default'])
        st.update(styles.get(area, {}))
        if st.get('plate') == 'black':
            plate = np.zeros_like(plate)
        for lang in dirs:
            text = words[area].get(lang) or words[area].get('us') or words[area]['uk']
            img, lettering = textpack.render_card(plate, text, st, return_mask=True)
            rgba = np.dstack([img, np.full(img.shape[:2], 255, np.uint8)])
            for code in CODES[lang]:
                save(out, code, area, rgba, scale)
                save(out, code, area + '_text', lettering, scale)  # what casts the port's shadow
            n += 1
    return n


def plate_cards(orig, out, scale):
    """The area name cards' backdrops without their lettering, one picture for every language, and the json that says where
    and how the port draws each name over its plate (`<out>/plates/<card>.png|json`, the port's areacard)."""
    words, styles = strings('titles.json'), strings('title_styles.json')
    dirs = {k: v for k, v in words['_lang_dirs'].items()}
    folder = os.path.join(out, 'plates')
    os.makedirs(folder, exist_ok=True)
    n = 0
    for area in [k for k in words if re.match(r'mt\d+$', k)]:
        cards = {}
        for d in ['img'] + list(dirs.values()):
            c = orig.load('%s/%s.tm2' % (d, area), area)
            if c is not None:
                cards[d] = c[..., :3]
        if not cards:
            print('plates: no originals for', area)
            continue
        base = next(cards[d] for d in ('img_1', 'img_2', 'img_3', 'img_4', 'img_5', 'img_6', 'img') if d in cards)
        plate, _ = plates.build_plate(list(cards.values()))
        fill, glow, edge = sample_style(base)
        st = {'fill': fill, 'glow': glow, 'edge': edge}
        st.update(styles['default'])
        st.update(styles.get(area, {}))
        if st.get('plate') == 'black':
            plate = np.zeros_like(plate)
        H, W = plate.shape[:2]
        rgba = np.dstack([plate, np.full(plate.shape[:2], 255, np.uint8)])
        save(out, 'plates', area, rgba, scale)
        # Where render_card puts the lettering (its defaults, for a 1536x512 card), as fractions of the card.
        cap_h, cap_top_y, ul_y, ul_thick, max_w = 215, 140, 392, 28, 1440
        sheet = {
            '_doc': 'Where and how the port draws the name over plates/<card>.png: fractions of the card (x of its width, y of its height).',
            'layout': {
                'cap_top': round(cap_top_y / 512, 4), 'cap_height': round(cap_h / 512, 4), 'xsq': 0.56,
                'max_width': round(max_w / 1536, 4), 'underline': 1, 'underline_y': round(ul_y / 512, 4),
                'underline_thick': round(ul_thick / 512, 4), 'underline_pad_left': round(18 / 1536, 4),
                'underline_pad_right': round(26 / 1536, 4), 'underline_bow': round(4 / 512, 4),
            },
            'style': {
                'fill': [int(round(v)) for v in st['fill']], 'edge': [int(round(v)) for v in st['edge']],
                'glow': [int(round(v)) for v in st['glow']], 'glow_amount': st.get('glow_amount', 0.85),
                'edge_width': round(st.get('outline', 4) / 1536, 5), 'glow_width': round(18 / 1536, 5),
                'drips': 1 if st.get('drips') else 0,
            },
        }
        with open(os.path.join(folder, area + '.json'), 'w', encoding='utf-8') as f:
            json.dump(sheet, f, indent=2)
        n += 1
    return n


# ---- dungeon floor labels --------------------------------------------------------------------------------------------

ROW1, ROW2, ROW3 = (0, 205), (205, 430), (470, 720)


def band_colors(a):
    vis = a[..., 3] > 200
    px = a[vis][:, :3].astype(np.float32)
    lum = px.mean(1)
    p = np.percentile(lum, [3, 25, 78, 97])
    return (tuple(np.median(px[(lum >= p[2]) & (lum <= p[3])], 0)),
            tuple(np.median(px[(lum >= p[0]) & (lum <= p[1])], 0)))


def floor_sheet(a, rows):
    """Re-letters a sheet. Returns (RGBA, filled lettering mask)."""
    out = a.copy()
    hi, lo = band_colors(a[205:430])  # the tint of this sheet's digit row
    H, W = a.shape[:2]
    lettering = np.zeros((H, W), np.uint8)
    for (y0, y1), words in rows:
        bx = slots.boxes(a[..., 3], y0, y1, len(words))
        if len(bx) != len(words):
            print('floors: %d boxes for %d words %s' % (len(bx), len(words), words))
            continue
        out[y0:y1] = 0
        cap_h = int(np.median([b[3] - b[1] for b in bx]) * 0.92)
        layer = np.zeros((H, W, 4), np.uint8)
        for w, b in zip(words, bx):
            m, ct = slots.fit_word(w, b, cap_h)
            M = textpack.place((H, W), m, ct, (b[0] + b[2]) / 2, b[3] - cap_h)
            lettering = np.maximum(lettering, M)
            e = slots.emboss_rgba(M, hi, lo)
            keep = e[..., 3] > layer[..., 3]
            layer[keep] = e[keep]
        out = np.where((layer[..., 3] > out[..., 3])[..., None], layer, out)
    return out, lettering


def floors(orig, out, scale):
    words = strings('floors.json')
    n = 0
    textpack.use_font_file(COMPENDIUM)
    for sheet in range(7):
        for lang, folder in words['_lang_dirs'].items():
            row1 = words['row1'][lang]
            if sheet >= len(row1):
                continue
            a = orig.load('dun/img/%s/dname0%d.img' % (folder, sheet), 'floor0%d' % sheet)
            if a is None:
                continue
            w1 = row1[sheet] if isinstance(row1[sheet], list) else [row1[sheet]]
            res, lettering = floor_sheet(a, [(ROW1, w1), (ROW2, list('0123456789')), (ROW3, words['row3'][lang])])
            for code in (EN_BOTH.get(lang) or CODES[lang]):
                save(out, code, 'floor0%d' % sheet, res, scale)
                save(out, code, 'floor0%d_text' % sheet, lettering, scale)
            n += 1
    return n


# ---- boss names ------------------------------------------------------------------------------------------------------

SUFFIX = {'uk': '', 'fr': '_f', 'de': '_g', 'it': '_i', 'es': '_s'}


def line_boxes(ink, k):
    rows = np.where(ink.any(1))[0]
    runs = []
    s = p = rows[0]
    for r in rows[1:]:
        if r - p > 6:
            runs.append((s, p))
            s = r
        p = r
    runs.append((s, p))
    if len(runs) == 1 and k == 2:
        a, b = runs[0]
        prof = np.convolve(ink[a:b + 1].sum(1).astype(float), np.ones(9) / 9, 'same')
        lo, hi = int((b - a) * 0.3), int((b - a) * 0.7)
        cut = a + lo + int(np.argmin(prof[lo:hi]))
        runs = [(a, cut - 1), (cut + 1, b)]
    if len(runs) > k:
        gaps = sorted(range(len(runs) - 1), key=lambda i: runs[i + 1][0] - runs[i][1], reverse=True)[:k - 1]
        merged, st = [], 0
        for c in sorted(gaps) + [len(runs) - 1]:
            merged.append((runs[st][0], runs[c][1]))
            st = c + 1
        runs = merged
    out = []
    for a, b in runs:
        xs = np.where(ink[a:b + 1].any(0))[0]
        out.append((a, b, xs.min(), xs.max()))
    return out


def shade(M, fill=(244, 244, 248)):
    """White lettering, no shadow: the port casts that (localize_texture.hpp), at the strength of the Options slider."""
    f = cv2.GaussianBlur(M.astype(np.float32) / 255, (0, 0), 0.8)
    rgb = np.empty(M.shape + (3,), np.float32)
    rgb[...] = np.array(fill, np.float32)
    return np.dstack([rgb, f * 255]).clip(0, 255).astype(np.uint8)


def bosses(orig, out, scale):
    words = strings('bosses.json')
    n = 0
    textpack.use_font_file(CINZEL)
    for key in [k for k in words if re.match(r's\d+_boss_\d+$', k)]:
        sid, num = re.match(r'(s\d+)_boss_(\d+)', key).groups()
        for lang, lines in words[key].items():
            f = 'boss.pak' if lang == 'uk' else 'boss_%s%s.pak' % (num, SUFFIX[lang])
            a = orig.load('gedit/%s/chara/%s' % (sid, f), 'boss_%s' % num)
            if a is None:
                continue
            H, W = a.shape[:2]
            white = (a[..., 3] > 120) & (a[..., :3].mean(2) > 150)
            bx = line_boxes(white, len(lines))
            if len(bx) != len(lines):
                print('bosses: %d lines for %s %s' % (len(bx), key, lang))
                continue
            layer = np.zeros((H, W, 4), np.uint8)
            for txt, (y0, y1, x0, x1) in zip(lines, bx):
                cap_h = int((y1 - y0 + 1) * (0.84 if any(ord(c) > 127 for c in txt) else 1.0))
                nat, _ = textpack.text_mask(txt, cap_h, 1.0, advance=True)
                bw = x1 - x0 + 1
                xsq = float(np.clip(bw / nat.shape[1], 0.5, 1.15))
                m, ct = textpack.text_mask(txt, cap_h, xsq, max_w=int(bw * 1.05), advance=True)
                m = cv2.dilate(m, np.ones((3, 3), np.uint8), iterations=2)
                lay = shade(textpack.place((H, W), m, ct, (x0 + x1) / 2, y1 - cap_h + 1))
                keep = lay[..., 3] > layer[..., 3]
                layer[keep] = lay[keep]
            for code in (EN_BOTH.get(lang) or CODES[lang]):
                save(out, code, 'boss_%s' % num, layer, scale)
            n += 1
    return n


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--originals', required=True, help="folder of the disc's textures as PNG (see above)")
    ap.add_argument('--out', required=True, help='<save folder>/lang/textures')
    ap.add_argument('--scale', type=int, choices=(1, 4), default=1,
                    help="1 (default): the texture's own size, as the game draws it; 4: four times that")
    ap.add_argument('--only', choices=('titles', 'plates', 'floors', 'bosses'), action='append')
    args = ap.parse_args()
    orig = Originals(args.originals)
    jobs = {'titles': titles, 'plates': plate_cards, 'floors': floors, 'bosses': bosses}
    for name, job in jobs.items():
        if args.only and name not in args.only:
            continue
        print('%s: %d pictures drawn' % (name, job(orig, args.out, args.scale)))


if __name__ == '__main__':
    main()
