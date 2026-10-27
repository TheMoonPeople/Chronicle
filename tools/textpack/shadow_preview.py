"""Previews the port's picture shadow (LocalizeCastShadow in port/src/localize/localize_texture.cpp) the way the game casts it.

    python shadow_preview.py <folder with the pictures> <name> <area|floor|boss> <out.png>

Writes the picture at 0, 25, 50 and 100 percent over a grey backdrop, one under the other.
"""
import os
import sys

import cv2
import numpy as np
from PIL import Image

OFFSET = {'area': 5.6, 'floor': 3.1, 'boss': 3.6}   # texture pixels at 50%, as in localize_texture.cpp
NATIVE_W = {'area': 384, 'floor': 384, 'boss': 256}


def cast(rgba, mask, percent, offset):
    if percent <= 0:
        return rgba
    percent = min(percent, 100)
    opacity = 1.0 - np.exp(-percent / 30.0)
    move = offset * (0.85 + 0.3 * percent / 100)
    sigma = max(0.6, 0.42 * move)
    h, w = mask.shape
    plane = cv2.warpAffine(mask.astype(np.float32), np.float32([[1, 0, move], [0, 1, move]]), (w, h), flags=cv2.INTER_LINEAR)
    plane = cv2.GaussianBlur(plane, (0, 0), sigma)
    s = np.minimum(1.0, plane * opacity) * (1 - mask)
    a = rgba[..., 3] / 255.
    out_a = a + s * (1 - a)
    k = np.where(out_a > 0, a * (1 - s * a) / np.maximum(out_a, 1e-6), 0)
    res = rgba.astype(np.float32)
    res[..., :3] *= k[..., None]
    res[..., 3] = out_a * 255
    return res.clip(0, 255).astype(np.uint8)


def main(root, name, kind, out):
    pic = np.asarray(Image.open(os.path.join(root, name + '.png')).convert('RGBA'))
    tp = os.path.join(root, name + '_text.png')
    mask = (np.asarray(Image.open(tp).convert('L')) / 255.).astype(np.float32) if os.path.exists(tp) else pic[..., 3] / 255.
    offset = OFFSET[kind] * pic.shape[1] / NATIVE_W[kind]
    rows = []
    for pct in (0, 25, 50, 100):
        r = cast(pic, mask, pct, offset)
        bg = Image.new('RGBA', (r.shape[1], r.shape[0]), (74, 72, 80, 255))
        bg.alpha_composite(Image.fromarray(r))
        rows.append(np.asarray(bg.convert('RGB')))
    Image.fromarray(np.concatenate(rows, 0)).save(out)


if __name__ == '__main__':
    main(*sys.argv[1:5])
