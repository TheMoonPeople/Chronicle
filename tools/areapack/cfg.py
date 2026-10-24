"""Reads the game's .cfg map definition files into JSON-ready data.

A .cfg is text (Shift-JIS comments): one command per line (`FOG 380,650,0,0,0,0,255`), plus blocks
`DEF_PATS ... DEF_ENDS` that define one map part each (`PT_BASE "d01g01_0.mds",0,0,0,0,0`). Commands keep their
order and their arguments as written; nothing is interpreted here.
"""
import re

_ARG = re.compile(r'"([^"]*)"|([^\s,]+)')


def _num(tok):
    try:
        return int(tok, 0)
    except ValueError:
        pass
    try:
        return float(tok)
    except ValueError:
        return tok


def _split(rest):
    return [m.group(1) if m.group(1) is not None else _num(m.group(2)) for m in _ARG.finditer(rest)]


def parse(data):
    text = data.decode("cp932", errors="replace").replace("\r\n", "\n").replace("\r", "\n")
    out = {"title": "", "commands": [], "parts": []}
    part = None
    note = ""
    header = []
    for raw in text.split("\n"):
        line, _, comment = raw.partition("//")
        comment = comment.strip(" /\t-")
        line = line.strip()
        if not line:
            if comment and not out["commands"] and not out["parts"]:
                header.append(comment)
            elif comment:
                note = comment
            continue
        cmd, _, rest = line.partition(" ")
        if "\t" in cmd:
            cmd, _, rest2 = line.partition("\t")
            rest = rest2
        args = _split(rest)
        entry = {"cmd": cmd, "args": args}
        if comment:
            entry["comment"] = comment
        if cmd == "DEF_PATS":
            part = {"no": len(out["parts"]), "note": note, "entries": []}
            note = ""
        elif cmd == "DEF_ENDS":
            if part is not None:
                out["parts"].append(part)
            part = None
        elif part is not None:
            part["entries"].append(entry)
        else:
            out["commands"].append(entry)
    out["title"] = " ".join(h for h in header if h)
    return out
