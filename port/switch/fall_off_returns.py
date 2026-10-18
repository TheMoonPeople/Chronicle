#!/usr/bin/env python3
"""Gives the functions fall_off_returns.txt lists for a unit a return at their end, in place.

Usage: fall_off_returns.py LIST UNIT FILE

FILE is the build's copy of ps2/src/UNIT.cpp. A listed function the copy does not define is an
error, so the list cannot go stale unnoticed.
"""

import re
import sys


def read_list(path, unit):
    entries = []
    with open(path, encoding="utf-8") as stream:
        for line in stream:
            line = line.split("#", 1)[0].split()
            if len(line) == 3 and line[0] == unit:
                entries.append((line[1], line[2]))
    return entries


def skip_literal(text, i):
    """The index past the comment, string or character literal at i, or i if there is none."""
    if text.startswith("//", i):
        end = text.find("\n", i)
        return len(text) if end < 0 else end
    if text.startswith("/*", i):
        return text.index("*/", i + 2) + 2
    if text[i] in "\"'":
        quote = text[i]
        i += 1
        while text[i] != quote:
            i += 2 if text[i] == "\\" else 1
        return i + 1
    return i


def body_end(text, open_brace):
    """The index of the brace that closes the one at open_brace."""
    depth = 0
    i = open_brace
    while i < len(text):
        skipped = skip_literal(text, i)
        if skipped != i:
            i = skipped
            continue
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
            if depth == 0:
                return i
        i += 1
    raise ValueError("unbalanced braces")


def patch(text, name, value):
    # A definition starts a line: its return type, then the name and its parameters, then the body.
    pattern = re.compile(r"^[A-Za-z_][^;{}()]*?(?<![\w:])" + re.escape(name) + r"\s*\([^;{]*\)\s*\{", re.M)
    found = pattern.search(text)
    if found is None:
        raise LookupError(name)
    close = body_end(text, found.end() - 1)
    return text[:close] + "    return " + value + "; // fall_off_returns.txt\n" + text[close:]


def main():
    list_path, unit, path = sys.argv[1:]
    entries = read_list(list_path, unit)
    if not entries:
        return 0
    with open(path, encoding="utf-8") as stream:
        text = stream.read()
    for name, value in entries:
        try:
            text = patch(text, name, value)
        except LookupError:
            print(f"fall_off_returns.py: {path} defines no {name} (listed for {unit})", file=sys.stderr)
            return 1
    with open(path, "w", encoding="utf-8") as stream:
        stream.write(text)
    return 0


if __name__ == "__main__":
    sys.exit(main())
