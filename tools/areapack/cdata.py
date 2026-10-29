"""Reads the game-data tables that the decomp keeps as C source.

The tables (monster stats, floor layouts, treasure-box item lists, ...) live in ps2/src and ps2/include.
This reads them as data: it parses `enum` blocks for names and the brace-initialised arrays for values, so
nothing here is copied from the source files; they are read at run time from the checkout.
"""
import re
from pathlib import Path

_COMMENT = re.compile(r"//[^\n]*|/\*.*?\*/", re.S)


def strip_comments(text):
    return _COMMENT.sub(lambda m: "\n" * m.group(0).count("\n"), text)


def read_enums(include_dir, groups=None):
    """Every `NAME = value` / bare `NAME` entry of every enum in the headers, as {name: int}.

    If `groups` is a dict it also receives {enum name: {name: int}} for each named enum."""
    values = {}
    for path in sorted(Path(include_dir).glob("*.h*")):
        text = strip_comments(path.read_text(encoding="utf-8", errors="replace"))
        for ename, body in re.findall(r"\benum\b\s*(?:class\s+)?(\w*)[^{;]*\{([^}]*)\}", text):
            nxt = 0
            for raw in body.split(","):
                raw = raw.strip()
                if not raw:
                    continue
                if "=" in raw:
                    name, expr = (s.strip() for s in raw.split("=", 1))
                    val = _eval_enum(expr, values)
                    if val is None:
                        continue
                    nxt = val
                else:
                    name = raw
                if not re.fullmatch(r"[A-Za-z_]\w*", name):
                    continue
                values[name] = nxt
                if groups is not None and ename:
                    groups.setdefault(ename, {})[name] = nxt
                nxt += 1
    return values


def _eval_enum(expr, known):
    expr = expr.strip()
    try:
        return int(expr, 0)
    except ValueError:
        pass
    if expr in known:
        return known[expr]
    m = re.fullmatch(r"(\w+)\s*([+-])\s*(\w+)", expr)
    if m:
        a, op, b = m.groups()
        a = known.get(a) if not a.isdigit() else int(a)
        b = known.get(b) if not b.isdigit() else int(b)
        if a is not None and b is not None:
            return a + b if op == "+" else a - b
    return None


_TOKEN = re.compile(r'"(?:[^"\\]|\\.)*"|[{}]|,|[^\s{},]+')


def _leaf(tok, enums):
    if tok.startswith('"'):
        return tok[1:-1]
    t = tok.rstrip("fF") if re.fullmatch(r"-?[\d.]+(e[-+]?\d+)?[fF]", tok) else tok
    try:
        return int(t, 0)
    except ValueError:
        pass
    try:
        return float(t)
    except ValueError:
        pass
    neg = tok.startswith("-")
    name = tok[1:] if neg else tok
    if name in enums:
        return -enums[name] if neg else enums[name]
    return tok


def parse_initializer(body, enums):
    """`{1, {2, 3}, "x", NAME}` -> [1, [2, 3], 'x', value-of-NAME]. `body` starts at the first `{`."""
    toks = _TOKEN.findall(body)
    pos = 0

    def node():
        nonlocal pos
        if toks[pos] != "{":
            val = _leaf(toks[pos], enums)
            pos += 1
            return val
        pos += 1
        out = []
        while toks[pos] != "}":
            if toks[pos] == ",":
                pos += 1
                continue
            out.append(node())
        pos += 1
        return out

    return node()


def find_array(text, name):
    """Returns the initializer text of `name[...] = { ... };` (from its first `{`), or None."""
    m = re.search(r"\b" + re.escape(name) + r"\s*\[[^\]]*\]\s*=\s*\{", text)
    if not m:
        return None
    start = m.end() - 1
    depth = 0
    i = start
    in_str = False
    while i < len(text):
        c = text[i]
        if in_str:
            if c == "\\":
                i += 1
            elif c == '"':
                in_str = False
        elif c == '"':
            in_str = True
        elif c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0:
                return text[start:i + 1]
        i += 1
    return None


def read_array(text_nocomments, name, enums):
    body = find_array(text_nocomments, name)
    return None if body is None else parse_initializer(body, enums)


def read_pointer_array(text, name):
    """`Type *name[N] = { A, B, C };` -> ['A', 'B', 'C']."""
    m = re.search(r"\*\s*" + re.escape(name) + r"\s*\[[^\]]*\]\s*=\s*\{([^}]*)\}", text)
    if not m:
        return None
    return [s.strip() for s in m.group(1).split(",") if s.strip()]


def reverse_names(enums, prefix):
    """{value: NAME} for the names starting with `prefix`; the first name wins on a tie."""
    out = {}
    for name, val in enums.items():
        if name.startswith(prefix) and val not in out:
            out[val] = name
    return out


_FIELD = re.compile(r"^\s*(?:const\s+)?(?:struct\s+)?([A-Za-z_]\w*(?:\s*\*)?)\s+([A-Za-z_]\w*)((?:\s*\[[^\]]*\])*)\s*$")


def read_structs(include_dir):
    """{struct name: [(type, field, [dims])]} for every plain `struct NAME { ... };` in the headers."""
    structs = {}
    for path in sorted(Path(include_dir).glob("*.h*")):
        text = strip_comments(path.read_text(encoding="utf-8", errors="replace"))
        for m in re.finditer(r"\bstruct\s+([A-Za-z_]\w*)\s*\{", text):
            depth, i = 1, m.end()
            while i < len(text) and depth:
                depth += (text[i] == "{") - (text[i] == "}")
                i += 1
            body = text[m.end():i - 1]
            if "(" in body or "{" in body or "union" in body:
                continue  # methods, nested types and unions are not plain data
            fields = []
            for fm in filter(None, (_FIELD.match(piece) for piece in body.split(";"))):
                dims = [int(d) if d.strip().isdigit() else d.strip() for d in re.findall(r"\[([^\]]*)\]", fm.group(3))]
                fields.append((fm.group(1).replace(" ", ""), fm.group(2), dims))
            if fields and name_ok(m.group(1)):
                structs.setdefault(m.group(1), fields)
    return structs


def name_ok(name):
    return not name.startswith("_")


def to_record(structs, struct_name, value, drop_unknown=True):
    """Names a parsed initializer after the struct's fields (recursing into nested structs).

    Fields called unk_XX are kept under their own name unless drop_unknown. A value with more or fewer
    entries than the struct has fields is named as far as it goes."""
    fields = structs.get(struct_name)
    if fields is None or not isinstance(value, list):
        return value
    out = {}
    for (ftype, fname, dims), item in zip(fields, value):
        if drop_unknown and fname.startswith("unk_"):
            continue
        out[fname] = _typed(structs, ftype, dims, item, drop_unknown)
    return out


def _typed(structs, ftype, dims, item, drop_unknown):
    if ftype in structs:
        if dims and isinstance(item, list):
            return [to_record(structs, ftype, x, drop_unknown) for x in item]
        return to_record(structs, ftype, item, drop_unknown)
    return item
