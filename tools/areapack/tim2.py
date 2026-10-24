"""Finds and decodes the TIM2 pictures stored inside the disc's files.

Most of the game's textures are TIM2 pictures packed into .img/.pak/.chr/.mds files. A picture is found by its
`TIM2` tag; the files around it are not parsed. 8-bit pictures in an `IM2` container are stored in the PS2's
swizzled order and are put back here.
"""
import hashlib
import struct

import numpy as np

_LUT = [0, 36, 8, 44, 1, 37, 9, 45, 2, 38, 10, 46, 3, 39, 11, 47, 4, 32, 12, 40, 5, 33, 13, 41, 6, 34, 14, 42, 7,
        35, 15, 43, 16, 52, 24, 60, 17, 53, 25, 61, 18, 54, 26, 62, 19, 55, 27, 63, 20, 48, 28, 56, 21, 49, 29,
        57, 22, 50, 30, 58, 23, 51, 31, 59, 4, 32, 12, 40, 5, 33, 13, 41, 6, 34, 14, 42, 7, 35, 15, 43, 0, 36, 8,
        44, 1, 37, 9, 45, 2, 38, 10, 46, 3, 39, 11, 47, 20, 48, 28, 56, 21, 49, 29, 57, 22, 50, 30, 58, 23, 51,
        31, 59, 16, 52, 24, 60, 17, 53, 25, 61, 18, 54, 26, 62, 19, 55, 27, 63]
_INV = np.zeros((2, 64), np.int64)
for _par in range(2):
    for _m in range(64):
        _INV[_par][_LUT[_par * 64 + _m]] = _m


def _unswizzle_clut8(pal):
    out = pal.copy()
    for base in range(0, 256, 32):
        out[base + 8:base + 16] = pal[base + 16:base + 24]
        out[base + 16:base + 24] = pal[base + 8:base + 16]
    return out


def _colours(buf, n, kind):
    if kind == 3:
        a = np.frombuffer(buf, np.uint8, n * 4).reshape(n, 4).copy()
    elif kind == 2:
        a = np.frombuffer(buf, np.uint8, n * 3).reshape(n, 3)
        a = np.concatenate([a, np.full((n, 1), 128, np.uint8)], 1)
    else:  # 16-bit ABGR1555
        v = np.frombuffer(buf, "<u2", n).astype(np.int32)
        r, g, b = (v & 31) * 255 // 31, ((v >> 5) & 31) * 255 // 31, ((v >> 10) & 31) * 255 // 31
        al = np.where(v >> 15, 128, 0)
        a = np.stack([r, g, b, al], 1).astype(np.uint8)
    a = a.copy()
    a[:, 3] = np.minimum(a[:, 3].astype(np.int32) * 2, 255).astype(np.uint8)  # PS2 alpha 0..128 -> 0..255
    return a


def _unswizzle8(w, h, data):
    px, py = ((w - 1) >> 7) + 1, ((h - 1) >> 6) + 1
    pw = w if px == 1 else 128
    rb = w * 2 if px == 1 else 256
    rc = h >> 1 if py == 1 else 32
    y, x = np.mgrid[0:h, 0:w]
    page_x, page_y = x // pw, y >> 6
    lx, ly = x - page_x * pw, y & 63
    bx, by = lx >> 4, ly >> 4
    src = (ly & 15) * 16 + (lx & 15)
    col = src >> 6
    out = col * 64 + _INV[col & 1, src & 63]
    prow, pcol = by * 8 + (out >> 5), bx * 32 + (out & 31)
    bad = (prow >= rc) | (pcol >= rb)
    off = page_y * px * rb * rc + rb * page_x + prow * rb * px + pcol
    sw = np.frombuffer(data, np.uint8)
    off = np.where(bad, 0, off)
    return np.where(bad, 0, sw[np.minimum(off, len(sw) - 1)]).astype(np.uint8)


def decode(d, off, swizzled=False):
    """Decodes the TIM2 file at d[off:]. Returns [(rgba HxWx4, indices|None, palette|None, info), ...]."""
    if d[off:off + 4] != b"TIM2":
        return []
    count = struct.unpack_from("<H", d, off + 6)[0]
    p = off + (0x80 if d[off + 5] == 1 else 0x10)
    res = []
    for _ in range(count):
        total, clut_sz, img_sz, hdr_sz, clut_n = struct.unpack_from("<IIIHH", d, p)
        fmt, mips, clut_t, img_t, w, h = struct.unpack_from("<BBBBHH", d, p + 0x10)
        if w == 0 or h == 0 or w > 4096 or h > 4096 or p + hdr_sz + img_sz > len(d):
            break
        img = d[p + hdr_sz:p + hdr_sz + img_sz]
        clut = d[p + hdr_sz + img_sz:p + hdr_sz + img_sz + clut_sz]
        info = {"width": w, "height": h, "image_type": img_t, "clut_type": clut_t, "clut_colours": clut_n,
                "swizzled": bool(swizzled and img_t == 5)}
        idx = pal = None
        if img_t in (4, 5):
            n = 16 if img_t == 4 else 256
            kind = clut_t & 63
            per = 4 if kind == 3 else 3 if kind == 2 else 2
            pal = _colours(clut, min(n, len(clut) // per), kind)
            if len(pal) < n:
                pal = np.concatenate([pal, np.zeros((n - len(pal), 4), np.uint8)])
            if img_t == 5 and len(pal) == 256:
                pal = _unswizzle_clut8(pal)
            raw = np.frombuffer(img, np.uint8, w * h if img_t == 5 else w * h // 2)
            if img_t == 4:
                idx = np.empty(w * h, np.uint8)
                idx[0::2] = raw & 15
                idx[1::2] = raw >> 4
            else:
                idx = _unswizzle8(w, h, img) if swizzled else raw
            idx = idx.reshape(h, w)
            rgba = pal[idx]
        elif img_t in (1, 2, 3):
            rgba = _colours(img, w * h, img_t).reshape(h, w, 4)
        else:
            break
        res.append((rgba, idx, pal, info))
        p += hdr_sz + img_sz + clut_sz
    return res


def scan(d):
    """Yields (offset, name_or_None, pictures) for each TIM2 found in the bytes `d`."""
    pos = 0
    while True:
        pos = d.find(b"TIM2", pos)
        if pos < 0:
            return
        container = pos >= 0x40 and d[pos - 0x40:pos - 0x3d] in (b"IM2", b"IMG")
        swizzled = pos >= 0x40 and d[pos - 0x40:pos - 0x3d] == b"IM2"
        try:
            pics = decode(d, pos, swizzled)
        except Exception:
            pics = []
        if not pics:
            pos += 4
            continue
        name = None
        if container:
            name = d[pos - 0x30:pos - 0x20].split(b"\0")[0].decode("ascii", "replace")
            if not name.isprintable():
                name = None
        yield pos, name, pics
        pos += 4


def digest(rgba):
    return hashlib.md5(rgba.tobytes()).hexdigest()
