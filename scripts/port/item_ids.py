#!/usr/bin/env python3
"""Write port/src/item/item_ids.inc, the vanilla item IDs, from port/include/game/itemdata.hpp's enum Item.

    item_ids.py [--check]

A vanilla item's ID is vanilla: and its enumerator's name without ITEM_, in lower case
(ITEM_BREAD is vanilla:bread, ITEM_WEAPON_DAGGER is vanilla:weapon_dagger). The range markers
(*_START, *_END) and ITEM_WEAPON_SLOT_EMPTY, which share a number with a real item, are left out.
Each entry names the enumerator itself, so the number always comes from the header and a renamed
enumerator stops the build. --check fails, writing nothing, when the file is not what this would
write.
"""

import argparse
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
HEADER = os.path.join(ROOT, "port", "include", "game", "itemdata.hpp")
OUTPUT = os.path.join(ROOT, "port", "src", "item", "item_ids.inc")
ENTRY = re.compile(r"^\s*(ITEM_\w+)\s*=\s*(\d+)\s*,")
SKIPPED = {"ITEM_WEAPON_SLOT_EMPTY"}


def entries():
    with open(HEADER, encoding="utf-8") as stream:
        text = stream.read()
    body = re.search(r"enum Item \{(.*?)\};", text, re.S)
    if not body:
        sys.exit("item_ids.py: no enum Item in " + HEADER)
    found, numbers, ids = [], {}, set()
    for line in body.group(1).splitlines():
        match = ENTRY.match(line)
        if not match:
            continue
        name, number = match.group(1), int(match.group(2))
        if name.endswith("_START") or name.endswith("_END") or name in SKIPPED:
            continue
        item_id = name[len("ITEM_"):].lower()
        if number in numbers:
            sys.exit("item_ids.py: %s and %s are both %d" % (numbers[number], name, number))
        if item_id in ids:
            sys.exit("item_ids.py: two enumerators give the ID %s" % item_id)
        numbers[number] = name
        ids.add(item_id)
        found.append((name, item_id))
    return found


def render(found):
    lines = ["// Written by scripts/port/item_ids.py from port/include/game/itemdata.hpp's enum Item.",
             "// The vanilla items' IDs without their vanilla: prefix."]
    lines += ['{%s, "%s"},' % (name, item_id) for name, item_id in found]
    return "\n".join(lines) + "\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--check", action="store_true")
    options = parser.parse_args()
    text = render(entries())
    current = None
    if os.path.exists(OUTPUT):
        with open(OUTPUT, encoding="utf-8", newline="") as stream:
            current = stream.read()
    if options.check:
        if current != text:
            print("item_ids.py: port/src/item/item_ids.inc is stale; run scripts/port/item_ids.py and commit it")
            return 1
        return 0
    if current != text:
        with open(OUTPUT, "w", encoding="utf-8", newline="\n") as stream:
            stream.write(text)
    print("item_ids.py: %d vanilla item IDs in port/src/item/item_ids.inc" % (text.count("\n") - 2))
    return 0


if __name__ == "__main__":
    sys.exit(main())
