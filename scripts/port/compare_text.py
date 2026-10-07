#!/usr/bin/env python3
"""Compares the game's text across languages, message by message.

Reads the language files that `darkcloud --export-text` writes (by default <save>/lang-export, see
docs/LOCALIZATION.md): en_gb.json, fr_fr.json, de_de.json, it_it.json, es_es.json. Each is a flat
object of "<message file key>.<message id>": text, with the same keys in every language, so the
languages are matched key to key. Writes one CSV per language into <out> (default: compare/ beside the
export), with British English as the reference:

    key, english, text, flag

Flags:
    missing    English has the message, this language's file does not
    extra      this language has a message English does not
    same       the text is the English text (often an untranslated line; skip it for names and numbers)
    empty      this language's text is empty where English's is not
    long       the text is more than twice, or less than half, the English length (a window may not fit it)

The export is the disc's own text: keep what this writes out of git, as the export is.

    python scripts/port/compare_text.py <export dir> [<out dir>]
"""

import csv
import json
import re
import sys
from pathlib import Path

REFERENCE = "en_gb"
LANGUAGES = ["fr_fr", "de_de", "it_it", "es_es"]


def load(path):
    with open(path, encoding="utf-8") as stream:
        return json.load(stream)


def informative(text):
    """False for a text that is the same in every language: only codes, digits and punctuation."""
    stripped = re.sub(r"\{-?\d+\}", "", text)
    return re.search(r"[A-Za-z]{3,}", stripped) is not None


def sort_key(key):
    """Keys by message file, then message id as a number."""
    file, _, id_ = key.rpartition(".")
    return (file, int(id_) if id_.lstrip("-").isdigit() else 0)


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 2
    export = Path(argv[1])
    out = Path(argv[2]) if len(argv) > 2 else export.parent / "compare"
    out.mkdir(parents=True, exist_ok=True)
    english = load(export / (REFERENCE + ".json"))
    lines = ["Messages against English (%s.json), by language" % REFERENCE, ""]
    for name in LANGUAGES:
        other = load(export / (name + ".json"))
        counts = {"missing": 0, "extra": 0, "same": 0, "empty": 0, "long": 0}
        with open(out / (name + ".csv"), "w", encoding="utf-8", newline="") as stream:
            writer = csv.writer(stream)
            writer.writerow(["key", "english", "text", "flag"])
            for key in sorted(set(english) | set(other), key=sort_key):
                en = english.get(key)
                text = other.get(key)
                flag = ""
                if en is not None and text is None:
                    flag = "missing"
                elif en is None:
                    flag = "extra"
                elif en and not text:
                    flag = "empty"
                elif text == en and informative(en):
                    flag = "same"
                elif len(en) > 20 and not (0.5 <= len(text) / len(en) <= 2.0):
                    flag = "long"
                if flag:
                    counts[flag] += 1
                    writer.writerow([key, en or "", text or "", flag])
        lines.append("%s: %d messages, %s" % (name, len(other), ", ".join("%d %s" % (n, k) for k, n in counts.items())))
    (out / "summary.txt").write_text("\n".join(lines) + "\n", encoding="utf-8")
    print("\n".join(lines))
    print("written to", out)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
