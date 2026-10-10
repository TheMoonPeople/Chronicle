#!/usr/bin/env python3
"""Packs controller / keyboard button art into port/glyphs/ (one atlas PNG per style + glyphs.json).

    python tools/glyphs/build_glyphs.py --src "C:/path/To be used" [--out port/glyphs]

--src holds the art as downloaded: P4Gamepad/Retro, P5Gamepad/Retro, XGamepad/Alt 2, SGamepad/Retro and
Keyboard_Mouse/Dark, one 128x128 PNG per button. Each glyph is cropped to its pixels and scaled by one
factor per style (REFERENCE source pixels become CELL atlas pixels), so the symbols keep their relative
sizes; glyphs.json gives every glyph's rectangle in its style's atlas.
"""
import argparse
import json
import os
import re
import sys

from PIL import Image, ImageDraw

CELL = 64          # atlas pixels a REFERENCE-pixel-wide glyph is drawn at
REFERENCE = 96     # source pixels of a round face button
ATLAS_WIDTH = 1024
PAD = 2            # transparent border around each packed glyph

# game button name -> file stem per gamepad style. Face buttons follow the position on the pad
# (cross is the bottom button: A on Xbox, B on Switch).
PAD_STYLES = {
    "ps4": ("P4Gamepad/Retro", {
        "cross": "T_P4_Cross_Color_Retro", "circle": "T_P4_Circle_Color_Retro",
        "square": "T_P4_Square_Color_Retro", "triangle": "T_P4_Triangle_Color_Retro",
        "l1": "T_P4_L1_Retro", "r1": "T_P4_R1_Retro", "l2": "T_P4_L2_Retro", "r2": "T_P4_R2_Retro",
        "start": "T_P4_Options_Retro", "select": "T_P4_Share_Retro",
        "dpad": "T_P4_Dpad_Retro", "dpad_ud": "T_P4_Dpad_Y_Retro", "dpad_lr": "T_P4_Dpad_X_Retro",
        "up": "T_P4_Dpad_UP_Retro", "down": "T_P4_Dpad_Down_Retro", "left": "T_P4_Dpad_Left_Retro",
        "right": "T_P4_Dpad_Right_Retro", "l3": "T_P4_L3_Retro", "r3": "T_P4_R3_Retro",
        "lstick": "T_P4_L_Retro", "rstick": "T_P4_R_Retro"}),
    "ps5": ("P5Gamepad/Retro", {
        "cross": "T_P5_Cross_Color_Retro", "circle": "T_P5_Circle_Color_Retro",
        "square": "T_P5_Square_Color_Retro", "triangle": "T_P5_Triangle_Color_Retro",
        "l1": "T_P5_L1_Retro", "r1": "T_P5_R1_Retro", "l2": "T_P5_L2_Retro", "r2": "T_P5_R2_Retro",
        "start": "T_P5_Options_Retro", "select": "T_P5_Share_Retro",
        "dpad": "T_P5_Dpad_Retro", "dpad_ud": "T_P5_Dpad_Y_Retro", "dpad_lr": "T_P5_Dpad_X_Retro",
        "up": "T_P5_Dpad_UP_Retro", "down": "T_P5_Dpad_Down_Retro", "left": "T_P5_Dpad_Left_Retro",
        "right": "T_P5_Dpad_Right_Retro", "l3": "T_P5_L3_Retro", "r3": "T_P5_R3_Retro",
        "lstick": "T_P5_L_Retro", "rstick": "T_P5_R_Retro"}),
    "xbox": ("XGamepad/Alt 2", {
        "cross": "T_X_A_Color_Alt_2", "circle": "T_X_B_Color_Alt_2",
        "square": "T_X_X_Color_Alt_2", "triangle": "T_X_Y_Color_Alt_2",
        "l1": "T_X_LB_Alt_2", "r1": "T_X_RB_Alt_2", "l2": "T_X_LT_Alt_2", "r2": "T_X_RT_Alt_2",
        "start": "@xbox_menu", "select": "T_X_Share_Alt_2",
        "dpad": "T_X_Dpad_Alt_2", "dpad_ud": "T_X_Dpad_Y_Alt_2", "dpad_lr": "T_X_Dpad_X_Alt_2",
        "up": "T_X_Dpad_Up_Alt_2", "down": "T_X_Dpad_Down_Alt_2", "left": "T_X_Dpad_Left_Alt_2",
        "right": "T_X_Dpad_Right_Alt_2", "l3": "T_X_Left_Stick_Click_Alt_2", "r3": "T_X_Right_Stick_Click_Alt_2",
        "lstick": "T_X_L_Alt_2", "rstick": "T_X_R_Alt_2"}),
    "switch": ("SGamepad/Retro", {
        "cross": "T_S_B_Retro", "circle": "T_S_A_Retro", "square": "T_S_Y_Retro", "triangle": "T_S_X_Retro",
        "l1": "T_S_LB_Retro", "r1": "T_S_RB_Retro", "l2": "T_S_LT_Retro", "r2": "T_S_RT_Retro",
        "start": "T_S_Plus_Retro", "select": "T_S_Minus_Retro",
        "dpad": "T_S_Dpad_Retro", "dpad_ud": "T_S_Dpad_Y_Retro", "dpad_lr": "T_S_Dpad_X_Retro",
        "up": "T_S_Dpad_Up_Retro", "down": "T_S_Dpad_Down_Retro", "left": "T_S_Dpad_Left_Retro",
        "right": "T_S_Dpad_Right_Retro", "l3": "T_S_L_Retro", "r3": "T_S_R_Retro",
        "lstick": "T_S_L_Retro", "rstick": "T_S_R_Retro"}),
}

# The PS5 style from a flat folder of one 64 px PNG per button ("ps5_a_butt.png"): the pack's A, B, X and Y
# are the pad's bottom, right, left and top buttons (Cross, Circle, Square, Triangle). It has no L3/R3 click
# art but the stick icons, so L3 and R3 are the plain stick icons and the stick symbols the all-directions ones.
PS5_PACK_REFERENCE = 60  # source pixels of its round face button
PS5_PACK = {
    "cross": "a_butt", "circle": "b_butt", "square": "x_butt", "triangle": "y_butt",
    "l1": "lb_butt", "r1": "rb_butt", "l2": "lt_butt", "r2": "rt_butt",
    "start": "start_butt", "select": "back_butt",
    "dpad": "dpad_all", "dpad_ud": "dpad_updown", "dpad_lr": "dpad_leftright",
    "up": "dpad_up", "down": "dpad_down", "left": "dpad_left", "right": "dpad_right",
    "l3": "lstick_none", "r3": "rstick_none", "lstick": "lstick_all", "rstick": "rstick_all"}

# Keyboard file stem (between T_ and _Key_Dark) -> the names the game looks keys up by: SDL's scancode
# name, lower-case, spaces dropped (input.hpp, InputPrimaryBindingName), or mouse1..mouse5.
KEY_ALIASES = {
    "Esc": ["escape"], "BackSpace": ["backspace"], "Backspace_Alt": [], "Enter": ["return", "keypadenter"],
    "Enter_Alt": [], "Enter_Tall": [], "Space": ["space"], "Tab": ["tab"], "Shift": ["leftshift", "rightshift"],
    "Crtl": ["leftctrl", "rightctrl"], "Alt": ["leftalt", "rightalt"], "CapsLock": ["capslock"],
    "Tilde": ["grave"], "Del": ["delete"], "Ins": ["insert"], "Home": ["home"], "End": ["end"],
    "PageUp": ["pageup"], "PageDown": ["pagedown"], "PrtScrn": ["printscreen"], "NumLock": ["numlock"],
    "Up": ["up"], "Down": ["down"], "Left": ["left"], "Right": ["right"], "Minus": ["-"], "Plus": ["="],
    "Asterisk": ["keypad*"], "Brackets_L": ["["], "Brackets_R": ["]"], "Semicolon": [";"], "Slash": ["/"],
    "Quotation": ["'"], "Question_Mark": [], "Cursor": [],
    "Mouse_Left": ["mouse1"], "Mouse_Right": ["mouse2"], "Mouse_Middle": ["mouse3"],
    "Mouse_Simple": ["mouse4", "mouse5"], "Mouse_XY": ["mouse_xy"],
    "Mouse_Scroll_Down": ["wheeldown"], "Mouse_Scroll_Up": ["wheelup"], "Mouse_Scroll": ["wheel"],
}


def load(path):
    return Image.open(path).convert("RGBA")


def make_xbox_menu(src_dir):
    """The art has no Xbox Menu button: the View button with its icon swapped for three bars."""
    view = load(os.path.join(src_dir, "T_X_Share_Alt_2.png"))
    px = view.load()
    body = px[28, 64] if px[28, 64][3] > 200 else (20, 18, 28, 255)
    out = view.copy()
    d = ImageDraw.Draw(out)
    d.ellipse((22, 22, 106, 106), fill=body)  # wipe the icon, keep the rim
    for y in (50, 64, 78):
        d.rounded_rectangle((42, y - 3, 86, y + 3), radius=3, fill=(255, 255, 255, 255))
    return out


def pack(images, reference=REFERENCE):
    """Shelf-packs {name: RGBA image}, each cropped to its pixels and scaled by CELL/reference.
    Returns (atlas, {name: [x, y, w, h]})."""
    scale = CELL / reference
    crops = {}
    for name, im in images.items():
        box = im.getchannel("A").getbbox()
        if box is None:
            continue
        c = im.crop(box)
        w = max(1, round(c.width * scale))
        h = max(1, round(c.height * scale))
        if w > ATLAS_WIDTH // 2:
            h = max(1, round(h * (ATLAS_WIDTH // 2) / w))
            w = ATLAS_WIDTH // 2
        # resize in premultiplied space so transparent pixels do not bleed their colour in
        c = c.convert("RGBa").resize((w, h), Image.LANCZOS).convert("RGBA")
        crops[name] = c
    order = sorted(crops, key=lambda n: (-crops[n].height, n))
    x = y = row_h = 0
    placed = {}
    for name in order:
        c = crops[name]
        if x + c.width + 2 * PAD > ATLAS_WIDTH:
            x, y, row_h = 0, y + row_h, 0
        placed[name] = (x + PAD, y + PAD)
        x += c.width + 2 * PAD
        row_h = max(row_h, c.height + 2 * PAD)
    height = 1
    while height < y + row_h:
        height *= 2
    atlas = Image.new("RGBA", (ATLAS_WIDTH, height), (0, 0, 0, 0))
    rects = {}
    for name, (px, py) in placed.items():
        atlas.paste(crops[name], (px, py))
        rects[name] = [px, py, crops[name].width, crops[name].height]
    return atlas, rects


# The other flat packs (same layout and button names as the PS5 one): style name -> file prefix.
EXTRA_PACKS = {"ps3": "ps3_", "steamdeck": "sd_", "steamcontroller": "sc_"}


def build_ps5_pack(folder, out, style="ps5", prefix="ps5_"):
    images = {}
    missing = []
    for name, stem in PS5_PACK.items():
        path = os.path.join(folder, prefix + stem + ".png")
        if os.path.exists(path):
            images[name] = load(path)
        else:
            missing.append(path)
    if style == "ps5color":
        # The supplied Alt pack's four colored face symbols are 128x128; normalize
        # them to the 64px pack cells while keeping the rest of the controller art.
        color_faces = {
            "cross": "T_P5_Cross_Color_Alt.png",
            "circle": "T_P5_Circle_Color_Alt.png",
            "square": "T_P5_Square_Color_Alt.png",
            "triangle": "T_P5_Triangle_Color_Alt.png",
        }
        for name, filename in color_faces.items():
            path = os.path.join(folder, filename)
            if os.path.exists(path):
                images[name] = load(path).resize((64, 64), Image.LANCZOS)
            else:
                missing.append(path)
    atlas, rects = pack(images, PS5_PACK_REFERENCE)
    atlas.save(os.path.join(out, style + ".png"), optimize=True)
    index_path = os.path.join(out, "glyphs.json")
    with open(index_path, encoding="utf-8") as f:
        index = json.load(f)
    index["styles"][style] = {"atlas": style + ".png", "glyphs": rects}
    with open(index_path, "w", encoding="utf-8") as f:
        json.dump(index, f, indent=1, sort_keys=True)
        f.write("\n")
    for m in missing:
        print("missing:", m, file=sys.stderr)
    print(style, len(rects), "glyphs")
    return 1 if missing else 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--src")
    ap.add_argument("--ps5-pack", help="folder of ps5_*.png buttons: rebuilds only the ps5 atlas from it, "
                    "keeping the other styles in the existing glyphs.json")
    ap.add_argument("--ps5-color-pack", help="folder of ps5_*.png buttons: builds the selectable colored PS5 style")
    for style in EXTRA_PACKS:
        ap.add_argument("--%s-pack" % style, help="folder of %s*.png buttons: rebuilds only the %s atlas, as --ps5-pack "
                        "does for ps5" % (EXTRA_PACKS[style], style))
    ap.add_argument("--out", default=os.path.join(os.path.dirname(__file__), "..", "..", "port", "glyphs"))
    args = ap.parse_args()
    os.makedirs(args.out, exist_ok=True)
    if args.ps5_pack:
        return build_ps5_pack(args.ps5_pack, args.out)
    if args.ps5_color_pack:
        return build_ps5_pack(args.ps5_color_pack, args.out, style="ps5color")
    for style, prefix in EXTRA_PACKS.items():
        folder = getattr(args, style + "_pack")
        if folder:
            return build_ps5_pack(folder, args.out, style, prefix)
    if not args.src:
        ap.error("--src or one of the --*-pack folders is required")
    index = {"reference": CELL, "styles": {}}
    missing = []

    for style, (folder, table) in PAD_STYLES.items():
        src_dir = os.path.join(args.src, folder)
        images = {}
        for name, stem in table.items():
            if stem == "@xbox_menu":
                images[name] = make_xbox_menu(src_dir)
                continue
            path = os.path.join(src_dir, stem + ".png")
            if os.path.exists(path):
                images[name] = load(path)
            else:
                missing.append(path)
        atlas, rects = pack(images)
        atlas.save(os.path.join(args.out, style + ".png"), optimize=True)
        index["styles"][style] = {"atlas": style + ".png", "glyphs": rects}

    key_dir = os.path.join(args.src, "Keyboard_Mouse", "Dark")
    images = {}
    for file in sorted(os.listdir(key_dir)):
        m = re.fullmatch(r"T_(.+?)_Key(?:_Dark)?(?:-\d+)?\.png", file)
        if not m or re.search(r"-\d+\.png$", file):
            continue
        stem = m.group(1)
        if stem in KEY_ALIASES:
            names = KEY_ALIASES[stem]
        elif re.fullmatch(r"[A-Z0-9]|F\d{1,2}", stem):
            names = [stem.lower()]
        else:
            continue
        for n in names:
            images[n] = load(os.path.join(key_dir, file))
    # the pad's directional symbols as clusters of the keys that are bound to them
    def cluster(layout, keys):
        """layout: list of rows, each a list of key names or None (a gap); keys are drawn 48 px wide."""
        size = 56
        cols = max(len(r) for r in layout)
        canvas = Image.new("RGBA", (cols * size, len(layout) * size), (0, 0, 0, 0))
        for ry, row in enumerate(layout):
            offset = (cols - len(row)) * size // 2
            for cx, k in enumerate(row):
                if k is None or k not in keys:
                    continue
                im = keys[k]
                box = im.getchannel("A").getbbox()
                im = im.crop(box)
                f = (size - 4) / max(im.width, im.height)
                im = im.resize((max(1, round(im.width * f)), max(1, round(im.height * f))), Image.LANCZOS)
                canvas.alpha_composite(im, (offset + cx * size + (size - im.width) // 2,
                                            ry * size + (size - im.height) // 2))
        # the cluster is drawn about as big as one round button: scale it up to REFERENCE
        scale = REFERENCE * 1.35 / max(canvas.width, canvas.height)
        return canvas.resize((round(canvas.width * scale), round(canvas.height * scale)), Image.LANCZOS)
    images["dpad"] = cluster([[None, "up", None], ["left", "down", "right"]], images)
    images["dpad_ud"] = cluster([["up"], ["down"]], images)
    images["dpad_lr"] = cluster([["left", "right"]], images)
    images["lstick"] = cluster([[None, "w", None], ["a", "s", "d"]], images)
    images["rstick"] = cluster([[None, "i", None], ["j", "k", "l"]], images)
    images.pop("cursor", None)
    atlas, rects = pack(images)
    atlas.save(os.path.join(args.out, "keyboard.png"), optimize=True)
    index["styles"]["keyboard"] = {"atlas": "keyboard.png", "glyphs": rects}

    with open(os.path.join(args.out, "glyphs.json"), "w", encoding="utf-8") as f:
        json.dump(index, f, indent=1, sort_keys=True)
        f.write("\n")
    for m in missing:
        print("missing:", m, file=sys.stderr)
    for style, data in index["styles"].items():
        print(style, len(data["glyphs"]), "glyphs")
    return 1 if missing else 0


if __name__ == "__main__":
    sys.exit(main())
