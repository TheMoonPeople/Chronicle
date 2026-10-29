"""Which disc files belong to which area, and what the areas are called."""
import fnmatch
import re

DUNGEONS = [
    # index, tag on the disc, English name, Japanese name
    (0, "d01", "Divine Beast Cave", "神獣の洞窟"),
    (1, "d02", "Wise Owl Forest", "白ひげふくろうの森"),
    (2, "d03", "Shipwreck", "沈没船"),
    (3, "d04", "Sun and Moon Temple", "太陽と月の神殿"),
    (4, "d05", "Moon Sea", "月の海"),
    (5, "d06", "Gallery of Time (Dark Heaven Castle)", "ダークヘブン城"),
    (6, "d07", "Demon Shaft", "デーモンシャフト"),
]

# Words in a town/event map's Japanese label that tie it to a dungeon.
DUNGEON_KEYWORDS = {
    "d01": ["神獣", "D01"],
    "d02": ["ふくろう", "D02"],
    "d03": ["沈没船", "D03"],
    "d04": ["太陽と月", "D04"],
    "d05": ["月の海", "D05"],
    "d06": ["ダークヘブン", "D06"],
    "d07": ["デーモンシャフト", "100F", "D07", "黒騎士"],
}

# The towns' own labels are not in the map files; these are the ones the localisation names.
TOWN_NAMES = {
    "e01": "Norune Village",
    "e02": "Matataki Village",
    "e03": "Queens",
    "e04": "Muska Racka",
    "s04": "Brownboo Village",
    "s13": "Yellow Drops",
    "s01": "Goro's House",
}

# Packs for files no area claims, first match wins. (name, description, path prefixes)
GLOBAL_PACKS = [
    ("_characters", "Playable characters, their weapons and the shop characters",
     ["chara/", "dun/mainchara/", "commenu/weapon/", "commenu/shopchara/"]),
    ("_items", "Item models and icons", ["dun/item/"]),
    ("_effects", "Effects, cloth, shared dungeon props", ["dun/effect/", "dun/etc/", "dun/cloth/", "commenu/effect/"]),
    ("_audio", "Sound effects and music", ["sound/"]),
    ("_ui", "Menus, fonts, title screen, message windows and localised pictures",
     ["commenu/", "check/", "dun/menu/", "meswin/", "titledat/", "img/", "img_1/", "img_2/", "img_3/", "img_4/",
      "img_5/", "img_6/", "dun/img/"]),
    ("_cutscenes", "Opening and cutscene data", ["opdat/", "rmdat/", "noda_w/"]),
    ("_text_scripts", "Dungeon messages and event scripts", ["dun/message/", "dun/script/"]),
]
MISC_PACK = ("_misc", "Everything else (system files, event battles, shared packs)")


def dungeon_matcher(tag):
    """Returns f(path) -> True for the files that belong to dungeon `tag` (like 'd01')."""
    rx = re.compile(r"(^|/)" + re.escape(tag) + r"(?![0-9])")

    def match(path):
        if not path.startswith("dun/"):
            return False
        parts = path.split("/")
        if tag in parts[:-1]:
            return True
        if parts[-1].startswith(tag) and not parts[-1][len(tag):len(tag) + 1].isdigit():
            return True
        return False

    return match


def town_matcher(dir_name):
    prefix = "gedit/" + dir_name + "/"
    return lambda path: path.startswith(prefix)


def jp_label(mapinfo_bytes):
    """The Japanese comment on the third line of a map's mapinfo.cfg, e.g. 'エディット(ノルン村）'."""
    lines = mapinfo_bytes.split(b"\n")
    if len(lines) < 3:
        return ""
    return lines[2].decode("cp932", "replace").strip().lstrip("/").strip()


def related_dungeons(label):
    return [tag for tag, words in DUNGEON_KEYWORDS.items() if any(w in label for w in words)]


def glob_any(path, patterns):
    return any(fnmatch.fnmatch(path, p) for p in patterns)
