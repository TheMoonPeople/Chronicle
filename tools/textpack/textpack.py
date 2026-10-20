"""Text pack renderer: draws Chronicle Sharp (the port's text font) into texture-sized images.

The font's accented letters and the marks acute/diaeresis/cedilla are blank in the file, so they are built here:
base letter + a mark drawn from the font's own grave/circumflex/tilde (acute = mirrored grave) or simple strokes.
Everything is in pixels of the 4x upscaled textures; `scale` converts to other sizes.
"""
import os, unicodedata
import numpy as np
import cv2
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
VARIANTS = {'BOLD': 'ChronicleSharp-Bold.ttf', 'THIN': 'ChronicleSharp-Regular.ttf'}  # Chronicle Sharp's two weights
FONT_PATH = os.path.join(HERE, '..', 'font', 'ChronicleSharp-Bold.ttf')
_fonts = {}


def use_font_file(path):
    """Draw with any TTF instead of Chronicle Sharp (used for the boss captions' serif, Cinzel, SIL OFL)."""
    global FONT_PATH
    FONT_PATH = path
    _fonts.clear()


def use_variant(name):
    """Select BOLD / THIN for everything drawn after this call."""
    global FONT_PATH
    assert name in VARIANTS, name
    FONT_PATH = os.path.join(HERE, '..', 'font', VARIANTS[name])
    _fonts.clear()


def font(px):
    if px not in _fonts:
        _fonts[px] = ImageFont.truetype(FONT_PATH, px)
    return _fonts[px]


def _glyph(ch, px):
    """Return (L image, advance) of one character drawn on a canvas with the baseline at y = 1.2*px."""
    f = font(px)
    W = int(px * 2.2)
    H = int(px * 2.0)
    im = Image.new('L', (W, H), 0)
    ImageDraw.Draw(im).text((px * 0.4, px * 0.2), ch, font=f, fill=255)
    return im, f.getlength(ch)


MARKS = {'̀': '`', '̂': '^', '̃': '~'}


def _compose(ch, px, cap_top_y, cap_h):
    """Draw a letter with accent. Returns (L image canvas, advance)."""
    base_ch = unicodedata.normalize('NFD', ch)
    base, marks = base_ch[0], base_ch[1:]
    im, adv = _glyph(base, px)
    bbox = im.getbbox()
    if bbox is None:
        return im, adv
    d = ImageDraw.Draw(im)
    cx = (bbox[0] + bbox[2]) / 2
    stroke = max(2, int(cap_h * 0.085))
    top = bbox[1]
    gap = int(cap_h * 0.05)
    for m in marks:
        if m in MARKS:
            mark_im, _ = _glyph(MARKS[m], px)
            mb = mark_im.getbbox()
            if mb is None:
                continue
            mark = mark_im.crop(mb)
            mark = mark.resize((int(mark.width * 0.8), int(mark.height * 0.8)))
            im.paste(255, (int(cx - mark.width / 2), top - gap - mark.height), mark)
        elif m == '́':  # acute: mirrored grave
            mark_im, _ = _glyph('`', px)
            mb = mark_im.getbbox()
            mark = mark_im.crop(mb).transpose(Image.FLIP_LEFT_RIGHT)
            mark = mark.resize((int(mark.width * 0.8), int(mark.height * 0.8)))
            im.paste(255, (int(cx - mark.width / 2), top - gap - mark.height), mark)
        elif m == '̈':  # diaeresis: two short strokes
            h = int(cap_h * 0.16)
            for dx in (-cap_h * 0.12, cap_h * 0.12):
                x = cx + dx
                d.line([(x, top - gap - h), (x - 1, top - gap)], fill=255, width=stroke)
        elif m == '̧':  # cedilla
            bot = bbox[3]
            d.line([(cx, bot - 2), (cx - 2, bot + int(cap_h * 0.1))], fill=255, width=stroke)
            d.line([(cx - 2, bot + int(cap_h * 0.1)), (cx - cap_h * 0.1, bot + int(cap_h * 0.16))], fill=255, width=stroke)
    return im, adv


def text_mask(text, cap_h, xsq, tracking=0.0, max_w=None, advance=False):
    """Render `text` (already the final string; caps recommended) -> uint8 mask cropped to ink, plus ink height info.

    cap_h: height of capital letters in px. xsq: horizontal squeeze (1 = font's own width).
    When max_w is set and the text is wider, the squeeze is reduced to fit.
    """
    # find font px so that cap letter height == cap_h
    px = 200
    probe, _ = _glyph('H', px)
    bb = probe.getbbox()
    px = int(px * cap_h / (bb[3] - bb[1]))
    probe, _ = _glyph('H', px)
    hb = probe.getbbox()
    cap_top = hb[1]
    glyphs = []
    x = 0.0
    gap = cap_h * (0.075 + tracking)
    SMALL = set(",.'’-:;!")
    for ch in text:
        if ch == ' ':
            x += cap_h * (0.26 if advance else 0.32)
            continue
        gim, adv = _glyph(ch, px)
        if gim.getbbox() is None and unicodedata.normalize('NFD', ch) != ch:
            gim, adv = _compose(ch, px, cap_top, cap_h)   # the font has no glyph for this accented letter
        gb = gim.getbbox()
        if gb is None:
            x += cap_h * 0.2
            continue
        if advance:   # serif faces: use the font's own advance (kerned pairs would need more; Q tails break ink spacing)
            glyphs.append((x - px * 0.4, gim))
            x += adv + cap_h * tracking
        else:
            glyphs.append((x - gb[0], gim))
            x += (gb[2] - gb[0]) + (gap * 0.5 if ch in SMALL else gap)
    total = int(x + px * 3)
    canvas = Image.new('L', (total, int(px * 2.0)), 0)
    for gx, gim in glyphs:
        canvas.paste(255, (int(gx), 0), gim)
    arr = np.asarray(canvas)
    ys = np.where(arr.max(1) > 0)[0]
    xs = np.where(arr.max(0) > 0)[0]
    if len(xs) == 0:
        return np.zeros((1, 1), np.uint8), 0
    # vertical: keep cap top aligned: crop from cap_top-0.45*cap_h to baseline+0.3*cap_h
    y0 = max(0, cap_top - int(cap_h * 0.45))
    y1 = min(arr.shape[0], cap_top + int(cap_h * 1.35))
    crop = arr[y0:y1, xs.min():xs.max() + 1]
    w = crop.shape[1]
    new_w = max(1, int(w * xsq))
    if max_w and new_w > max_w:
        new_w = max_w
    crop = cv2.resize(crop, (new_w, crop.shape[0]), interpolation=cv2.INTER_AREA)
    return crop, cap_top - y0  # cap top row inside the crop


def place(canvas_shape, mask, cap_top_in_mask, cx, cap_top_y):
    """Paste mask onto a blank canvas so its cap top is at cap_top_y and it is centered on cx."""
    H, W = canvas_shape
    out = np.zeros((H, W), np.uint8)
    x0 = int(cx - mask.shape[1] / 2)
    y0 = int(cap_top_y - cap_top_in_mask)
    sx0, sy0 = max(0, -x0), max(0, -y0)
    dx0, dy0 = max(0, x0), max(0, y0)
    w = min(mask.shape[1] - sx0, W - dx0)
    h = min(mask.shape[0] - sy0, H - dy0)
    if w > 0 and h > 0:
        out[dy0:dy0 + h, dx0:dx0 + w] = mask[sy0:sy0 + h, sx0:sx0 + w]
    return out


def underline_mask(shape, x0, x1, yc, thick):
    """Brush underline: slim tapered stroke with a slightly heavier left end."""
    H, W = shape
    m = np.zeros((H, W), np.uint8)
    n = max(2, int(x1 - x0))
    xs = np.linspace(x0, x1, n)
    t = np.linspace(0, 1, n)
    th = thick * (0.55 + 0.45 * np.sin(np.pi * np.clip(t * 0.85 + 0.05, 0, 1)))
    yy = yc + 4 * np.sin(t * np.pi)
    top = np.stack([xs, yy - th / 2], 1)
    bot = np.stack([xs[::-1], (yy + th / 2)[::-1]], 1)
    cv2.fillPoly(m, [np.round(np.concatenate([top, bot])).astype(np.int32)], 255)
    return m


def tint(rgb, mask01, base):
    """Composite color rgb over base using mask01 (H,W float)."""
    return base * (1 - mask01[..., None]) + np.array(rgb, float)[None, None, :] * mask01[..., None]


def drips(shape, x0, x1, y, n, length):
    """Hanging drops under the underline (Dark Heaven Castle)."""
    H, W = shape
    m = np.zeros((H, W), np.uint8)
    for k in range(n):
        x = x0 + (k + 0.5) * (x1 - x0) / n
        cv2.fillPoly(m, [np.array([[x - 9, y], [x + 9, y], [x, y + length * (0.7 + 0.3 * ((k * 7) % 3) / 2)]], np.int32)], 255)
    return m


def render_card(plate, text, style, cap_h=215, xsq=0.56, cap_top_y=140, ul_y=392, ul_thick=28, max_w=1440, tracking=0.0,
                underline=True, return_mask=False):
    """plate: HxWx3 uint8 clean backdrop. style: dict(fill, glow, edge, glow_amount, outline, bold, drips)."""
    H, W = plate.shape[:2]
    mask, ct = text_mask(text, cap_h, xsq, tracking, max_w=max_w)
    k = np.ones((3, 3), np.uint8)
    bold = style.get('bold', 2)
    if bold:
        mask = cv2.dilate(mask, k, iterations=bold)
    M = place((H, W), mask, ct, W / 2, cap_top_y)
    ys, xs = np.where(M > 0)
    if underline and len(xs):
        U = underline_mask((H, W), xs.min() - 18, xs.max() + 26, ul_y, ul_thick)
        M = np.maximum(M, U)
        if style.get('drips'):
            M = np.maximum(M, drips((H, W), xs.min(), xs.max(), ul_y + 10, 7, 46))
    m = M.astype(np.float32) / 255
    out = plate.astype(np.float32)
    ga = style.get('glow_amount', 0.85)
    if ga > 0:
        wide = cv2.GaussianBlur(cv2.dilate(M, k, iterations=9).astype(np.float32) / 255, (0, 0), 11)
        out = out + (np.array(style['glow'], np.float32)[None, None] - out) * (np.clip(wide * 1.3, 0, 1) * ga)[..., None]
    edge = cv2.GaussianBlur(cv2.dilate(M, k, iterations=style.get('outline', 4)).astype(np.float32) / 255, (0, 0), 1.6)
    out = out + (np.array(style['edge'], np.float32)[None, None] - out) * np.clip(edge * 1.4, 0, 1)[..., None]
    mm = cv2.GaussianBlur(m, (0, 0), 0.9)
    out = tint(style['fill'], mm, out)
    out = np.clip(out, 0, 255).astype(np.uint8)
    return (out, M) if return_mask else out
