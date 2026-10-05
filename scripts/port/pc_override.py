#!/usr/bin/env python3
"""Take the definitions port/src overrides out of the ps2/src units the PC port compiles.

    pc_override.py list port/src -o overrides.txt
    pc_override.py strip --overrides overrides.txt [--stubs port/include/stubs/mglib.hpp] \\
        [--define PAL ...] ps2/src/mglib.cpp port/build/pc/ps2_src/mglib.cpp
    pc_override.py check --overrides overrides.txt --stubs port/include/stubs [--define PAL ...] \\
        ps2/src -o checked

port/src tags a definition that replaces one of ps2/src's with PC_OVERRIDE (port/include/port.h
defines the macro as nothing):

    PC_OVERRIDE void MGClearScreen(u_char r) { ... }

`list` writes the name each tagged definition links under: the qualified name and parameter types
of a function, the name alone of a variable or of a function with C linkage. `strip` writes a copy
of a ps2/src unit without the definitions of those names, which the port compiles in place of the
unit:

    void MGClearScreen(u_char r) { ... }    ->  void MGClearScreen(u_char r);
    int CWater::CheckClip() { ... }         ->  (removed; the class declares it)
    CMap OP_GroundMap;                      ->  extern CMap OP_GroundMap;

Line numbers are kept and a #line directive names the original, so diagnostics and debug information
still point into ps2/src.

`check` fails when no unit of ps2/src defines a listed name. Such a tag takes nothing out: where the
two definitions are one symbol the link fails on the duplicate, but where their signatures differ
both would link and the game would go on calling ps2/src's.
"""

import argparse
import bisect
import pathlib
import re
import sys

TAG = re.compile(r"\bPC_OVERRIDE\b")
DIRECTIVE = re.compile(r"[ \t]*#[ \t]*(\w+)")
LITERAL = re.compile(
    r'//(?:\\\r?\n|[^\n])*|/\*.*?\*/|(?<![A-Za-z0-9_])(?:u8|[uUL])?R"([^ ()\\\t\n]*)\(.*?\)\1"'
    r'|"(?:\\.|[^"\\\n])*"|\'(?:\\.|[^\'\\\n])*\'',
    re.DOTALL,
)
RENAME = re.compile(r"^[ \t]*#[ \t]*define[ \t]+([A-Za-z_]\w*)[ \t]+([A-Za-z_]\w*)[ \t]*\r?$", re.MULTILINE)
C_LINKAGE = re.compile(r'\bextern\s*"C"')
ATTRIBUTE = re.compile(r"\b__attribute__\s*\(")
NAME = re.compile(r"((?:[A-Za-z_]\w*(?:<[^<>()]*>)?\s*::\s*)*(?:operator\b.*|~?[A-Za-z_]\w*))\s*$", re.DOTALL)
TYPE_BODY = re.compile(r"\s*(?:class|struct|union|enum|namespace)\b[^;(]*$")
BUILTIN = {"int", "char", "short", "long", "float", "double", "void", "bool", "unsigned", "signed", "wchar_t"}
NOT_A_TYPE = {"const", "volatile", "register", "struct", "class", "union", "enum"}
INLINE = re.compile(r"\b(?:__)?inline(?:__)?\b")
CONDITIONAL = "a preprocessor directive before the definition's body"


class Error(Exception):
    pass


class ConditionalError(Error):
    pass


def mask(text):
    """Returns text with comments and literals blanked, at the same offsets."""
    return LITERAL.sub(lambda match: re.sub(r"[^\n]", " ", match[0]), text)


def directive_lines(code, start=0):
    """Yields each line of code from start with the preprocessor directive it belongs to: None for a
    line of code, an empty string for the continuation of a directive."""
    continued = False
    while True:
        end = code.find("\n", start)
        line = code[start:] if end < 0 else code[start:end]
        directive = DIRECTIVE.match(line)
        word = "" if continued else directive[1] if directive else None
        continued = word is not None and line.rstrip().endswith("\\")
        yield line, word
        if end < 0:
            return
        start = end + 1


def flatten(code):
    """Blanks preprocessor lines and the later branches of each conditional that shares braces.

    Retail's #ifdef PAL blocks inside a function open the same brace in each branch; keeping the
    first keeps the function's braces balanced. A conditional whose first branch leaves the braces
    as it found them, as one around whole definitions does, keeps every branch. Returns the text,
    the brace depth at the start of each line, and the indices of the lines blanked at file scope.
    """
    out, depths, depth, blocks, hidden = [], [], 0, [], []
    for number, (line, directive) in enumerate(directive_lines(code), 1):
        depths.append(depth)
        blanking = next((block for block in blocks if block["blank"]), None)
        if directive is None:
            if blanking is None:
                depth += line.count("{") - line.count("}")
            elif blanking["before"] == 0:
                hidden.append(number - 1)
            out.append(line if blanking is None else " " * len(line))
            continue
        blanked = blanking is not None
        out.append(" " * len(line))
        if directive in ("if", "ifdef", "ifndef"):
            blocks.append({"before": depth, "whole": None, "blank": False})
        elif directive in ("else", "elif", "endif") and blocks:
            block = blocks[-1]
            if not blanked:
                if block["whole"] is None and directive != "endif":
                    block["whole"] = depth == block["before"]
                    block["blank"] = not block["whole"]
                elif block["whole"] and depth != block["before"]:
                    raise Error(f"{number}: the branches of a conditional block leave different braces open")
            if directive == "endif":
                blocks.pop()
    return "\n".join(out), depths, hidden


def check_conditionals(code):
    """Raises unless the definition in code ends at the same brace whichever branches are compiled.

    Every conditional has to open and close inside the definition, and each of its branches, the
    empty one of a block without #else among them, has to leave as many braces open as the first
    without closing the definition's own.
    """
    depth, blocks = 0, []  # per open conditional: depth before it, depth after its first branch, has #else
    for line, directive in directive_lines(code):
        if directive is None:
            for c in line:
                depth += (c == "{") - (c == "}")
                if blocks and depth < 1:
                    raise Error("a conditional branch closes the definition")
            continue
        if directive in ("if", "ifdef", "ifndef"):
            blocks.append([depth, None, False])
        elif directive in ("else", "elif", "endif"):
            if not blocks:
                raise Error("the definition holds part of a conditional block")
            before, first, has_else = blocks[-1]
            if first is None:
                first = blocks[-1][1] = depth
            if depth != first or (directive == "endif" and not has_else and first != before):
                raise Error("the branches of a conditional block leave different braces open")
            if directive == "endif":
                blocks.pop()
            else:
                blocks[-1][2] = directive == "else"
                depth = before
    if blocks:
        raise Error("the definition holds part of a conditional block")


def signature(code, start):
    """The statement at start up to its body or semicolon, as directive_lines gives it: up to the
    first { or ; outside parentheses or outside the conditional blocks the statement opens."""
    lines, paren, blocks = [], 0, 0
    for line, directive in directive_lines(code, start):
        if directive is not None:
            blocks += (directive in ("if", "ifdef", "ifndef")) - (directive == "endif")
            lines.append((line, directive))
            continue
        for i, c in enumerate(line):
            paren += (c == "(") - (c == ")")
            if c in "{;" and (paren <= 0 or blocks <= 0):
                return lines + [(line[:i], None)]
        lines.append((line, None))
    return lines


def closing(code, opening):
    """The offset of the bracket that closes the one at opening."""
    pair = {"(": ")", "{": "}", "[": "]"}[code[opening]]
    depth = 0
    for i in range(opening, len(code)):
        if code[i] == code[opening]:
            depth += 1
        elif code[i] == pair:
            depth -= 1
            if depth == 0:
                return i
    raise Error("unbalanced brackets")


def without_attributes(code):
    """Returns code with each __attribute__((...)) blanked, at the same offsets."""
    while True:
        attribute = ATTRIBUTE.search(code)
        if attribute is None:
            return code
        end = closing(code, attribute.end() - 1) + 1
        code = code[: attribute.start()] + " " * (end - attribute.start()) + code[end:]


def parameter_type(parameter):
    """A parameter's declaration without its name, default argument and spacing."""
    depth = 0
    for i, c in enumerate(parameter):
        depth += (c in "(<[") - (c in ")>]")
        if c == "=" and depth == 0:
            parameter = parameter[:i]
            break
    parameter = re.sub(r"\(\s*([*&]+)\s*[A-Za-z_]\w*\s*\)", r"(\1)", parameter)
    declarator = re.match(r"(.*?)\b([A-Za-z_]\w*)((?:\s*\[[^\]]*\])*)\s*$", parameter, re.DOTALL)
    if declarator and declarator[2] not in BUILTIN:
        named = [word for word in re.findall(r"[A-Za-z_]\w*", declarator[1]) if word not in NOT_A_TYPE]
        if named:
            parameter = declarator[1] + declarator[3]
    words = [word for word in re.findall(r"[A-Za-z_]\w*|\S", parameter) if word != "register"]
    return "".join(
        (" " if i and re.match(r"\w", word) and re.match(r"\w", words[i - 1][-1]) else "") + word
        for i, word in enumerate(words)
    )


def parameter_types(parameters):
    types, depth, start = [], 0, 0
    for i, c in enumerate(parameters + ","):
        depth += (c in "(<[") - (c in ")>]")
        if c == "," and depth == 0:
            types.append(parameter_type(parameters[start:i]))
            start = i + 1
    return [] if types in ([""], ["void"]) else types


class Definition:
    """A definition at file scope: where it ends, the name it links under, what the port keeps of it."""

    def __init__(self, text, flat, renames=None):
        """text starts at the definition and flat is its flattened form; raises if neither a
        function with its body nor a variable starts there."""
        paren, init, i = 0, None, 0
        while i < len(flat):
            c = flat[i]
            if c == "(":
                paren += 1
            elif c == ")":
                paren -= 1
            elif paren == 0 and c == "{" and (init is not None or "(" not in without_attributes(flat[:i])):
                # A variable's braced initialiser or a type's body, not a function's.
                init = i if init is None else init
                i = closing(flat, i)
            elif paren == 0 and c == "=" and init is None and "operator" not in flat[:i]:
                init = i
            elif paren == 0 and c in "{;":
                break
            i += 1
        else:
            raise Error("no definition follows")
        end = i if init is None else init
        head = without_attributes(flat[:end])
        if re.match(r"\s*typedef\b", head) or (flat[end] == "{" and TYPE_BODY.match(head)):
            raise Error("a type, not a function or a variable")
        if re.search(r"^[ \t]*#", text[:end], re.MULTILINE):
            raise ConditionalError(CONDITIONAL)
        self.static = re.search(r"\bstatic\b", head) is not None
        c_linkage = any(C_LINKAGE.match(text, word.start()) for word in re.finditer(r"\bextern\b", head))
        declaration = text[: len(flat[:end].rstrip())].lstrip(" \t")
        opening = head.find("(")
        if flat[i] == ";":
            if opening >= 0:
                raise Error("a declaration, or a variable with constructor arguments")
            if init is None and re.search(r"\bextern\b", head):
                raise Error("a declaration")
            name = re.search(r"([A-Za-z_]\w*)\s*(?:\[[^\]]*\]\s*)*$", head)
            if name is None:
                raise Error("no name")
            self.name = self.spelled = (renames or {}).get(name[1], name[1])
            self.length = i + 1
            self.replacement = declaration + ";" if c_linkage else "extern " + declaration + ";"
            return
        self.length = closing(flat, i) + 1
        name = NAME.search(head[:opening].rstrip())
        if name is None:
            raise Error("no name")
        spelled = re.sub(r"\s+", "", name[1])
        if spelled.endswith("operator"):
            # operator(): the parameters are the second pair of parentheses.
            spelled += "()"
            opening = head.find("(", closing(head, opening))
        self.spelled = spelled = (renames or {}).get(spelled, spelled)
        if c_linkage or spelled == "main":
            self.name = spelled
        else:
            close = closing(head, opening)
            const = " const" if re.match(r"\s*const\b", head[close + 1 :]) else ""
            self.name = spelled + "(" + ",".join(parameter_types(head[opening + 1 : close])) + ")" + const
        # A member function is declared by its class; an explicit specialization of one has to stay
        # declared, or callers would instantiate the primary template.
        specialization = re.match(r"\s*template\s*<\s*>", head) is not None
        initialisers = re.search(r"(?<!:):(?!:)", head[closing(head, opening) + 1 :])
        if specialization and initialisers:
            declaration = text[: closing(head, opening) + 1 + initialisers.start()].rstrip().lstrip(" \t")
        self.replacement = "" if "::" in spelled and not specialization else declaration + ";"


def tagged_names(text):
    """The names the PC_OVERRIDE definitions of a port/src unit link under."""
    flat, _, _ = flatten(mask(text))
    names = []
    for tag in TAG.finditer(flat):
        line = text.count("\n", 0, tag.start()) + 1
        try:
            definition = Definition(text[tag.end() :], flat[tag.end() :])
            if definition.static:
                raise Error("a static definition replaces nothing in another unit")
        except Error as error:
            raise Error(f"{line}: {error}") from None
        names.append(definition.name)
    return names


def check_compiled(code, found, defined):
    """Raises unless each definition in found is one the port compiles. Of a conditional block on a
    macro in defined, that is the branch the port takes; of any other, every branch, a missing #else
    among them. Otherwise the definition taken out may be a branch the port never compiles, while
    the one it does compile stays."""
    names = {}
    for start, definition in found:
        names.setdefault(code.count("\n", 0, start), set()).add(definition.name)
    blocks = []
    for index, (line, directive) in enumerate(directive_lines(code)):
        for block in blocks:
            block["branches"][-1].update(names.get(index, ()))
        if directive in ("if", "ifdef", "ifndef"):
            macro = re.fullmatch(r"\s*#\s*\w+\s+(\w+)\s*", line)
            known = directive != "if" and macro is not None and macro[1] in defined
            taken = known and directive == "ifdef"
            blocks.append({"branches": [set()], "known": known, "compiled": 0 if taken else 1, "else": False})
        elif directive in ("elif", "else") and blocks:
            blocks[-1]["branches"].append(set())
            blocks[-1]["known"] = blocks[-1]["known"] and directive == "else"
            blocks[-1]["else"] = directive == "else"
        elif directive == "endif" and blocks:
            block = blocks.pop()
            branches = block["branches"] + ([] if block["else"] else [set()])
            compiled = branches[block["compiled"]] if block["known"] else set.intersection(*branches)
            for name in sorted(set().union(*branches) - compiled):
                raise Error(f"{index + 1}: {name} is taken out of a branch of this conditional block that "
                            "the port may not compile")


def overridden(text, overrides, renames=None, defined=()):
    """The (start, definition) of each definition at file scope of a name in overrides, given the
    macros the port defines."""
    found = list(definitions(text, overrides, renames))
    if found:
        check_compiled(mask(text), found, set(defined))
    return found


def definitions(text, overrides, renames=None):
    """Yields (start, definition) for each definition at file scope of a name in overrides."""
    # A spliced line could join a name the scan below would not see.
    for number, (line, directive) in enumerate(directive_lines(text), 1):
        if directive is None and line.rstrip("\r").endswith("\\"):
            raise Error(f"{number}: a line continued with a backslash outside a preprocessor directive")
    renames = renames or {}
    # The last word of each name finds the places to look at; the definition there decides.
    words = {re.match(r"operator|\w+", name.split("(")[0].split("::")[-1].lstrip("~"))[0] for name in overrides}
    words |= {old for old, new in renames.items() if new in words}
    if not words:
        return
    candidate = re.compile(r"(?<!\w)(?:" + "|".join(sorted(words, key=len, reverse=True)) + r")(?!\w)")
    if candidate.search(text) is None:
        return
    code = mask(text)
    flat, depths, hidden = flatten(code)
    # The other branches of a conditional block that opens a definition are not read.
    lines = code.split("\n")
    for index in hidden:
        word = candidate.search(lines[index])
        if word:
            raise Error(f"{index + 1}: {word[0]} in a later branch of a conditional block that opens a "
                        "definition")
    line_starts = [0] + [match.end() for match in re.finditer("\n", flat)]
    last, seen = 0, set()
    for match in candidate.finditer(flat):
        line = bisect.bisect_right(line_starts, match.start()) - 1
        before = flat[line_starts[line] : match.start()]
        if depths[line] + before.count("{") - before.count("}") != 0 or match.start() < last:
            continue
        start = max(flat.rfind(";", 0, match.start()), flat.rfind("}", 0, match.start())) + 1
        start += len(flat[start:]) - len(flat[start:].lstrip())
        if start in seen:
            continue
        seen.add(start)
        number = text.count("\n", 0, start) + 1
        head = signature(code, start)
        if any(directive is not None for _, directive in head):
            raise Error(f"{number}: {CONDITIONAL}")
        try:
            definition = Definition(text[start:], flat[start:], renames)
        except ConditionalError as error:
            raise Error(f"{number}: {error}") from None
        except Error:
            definition = None
        # A function the port gives C linkage is listed by its name alone, whatever ps2/src's own
        # definition spells; its header declares the linkage.
        if definition is None or definition.static or not {definition.name, definition.spelled} & overrides:
            # The link fails on a definition left in by mistake, unless it is inline.
            head = "".join(line for line, _ in head)
            if INLINE.search(head) and not re.search(r"\bstatic\b", head):
                raise Error(f"{number}: an inline definition that mentions {match[0]}, but not as one it "
                            "can read")
            continue
        last = start + definition.length
        try:
            check_conditionals(code[start:last])
        except Error as error:
            raise Error(f"{number}: {error}") from None
        yield start, definition


def strip(text, overrides, renames=None, defined=()):
    """Returns text without its definitions of the names in overrides."""
    out, last = [], 0
    for start, definition in overridden(text, overrides, renames, defined):
        end = start + definition.length
        newlines = text.count("\n", start, end) - definition.replacement.count("\n")
        out.append(text[last:start] + definition.replacement + "\n" * newlines)
        last = end
    out.append(text[last:])
    return "".join(out)


def read(path):
    # Bytes that are not UTF-8 pass through unchanged.
    with open(path, encoding="utf-8", errors="surrogateescape", newline="") as f:
        return f.read()


def write(path, text, if_changed=False):
    path.parent.mkdir(parents=True, exist_ok=True)
    if if_changed and path.exists() and read(path) == text:
        return
    with open(path, "w", encoding="utf-8", errors="surrogateescape", newline="") as f:
        f.write(text)


def renames(stubs):
    """The names a unit's stub header gives others to: its #define of one identifier as another."""
    return dict(RENAME.findall(read(stubs))) if stubs and stubs.exists() else {}


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    commands = parser.add_subparsers(dest="command", required=True)
    lister = commands.add_parser("list", help="write the names port/src's tagged definitions link under")
    lister.add_argument("port", type=pathlib.Path, help="port/src")
    lister.add_argument("-o", "--output", type=pathlib.Path, required=True)
    stripper = commands.add_parser("strip", help="write a ps2/src unit without the overridden definitions")
    stripper.add_argument("--overrides", type=pathlib.Path, required=True)
    stripper.add_argument("--stubs", type=pathlib.Path, help="the unit's port/include/stubs header, for its renames")
    stripper.add_argument("--define", action="append", default=[], help="a macro the port defines")
    stripper.add_argument("source", type=pathlib.Path)
    stripper.add_argument("output", type=pathlib.Path)
    checker = commands.add_parser("check", help="fail if a listed name is defined by no ps2/src unit")
    checker.add_argument("--overrides", type=pathlib.Path, required=True)
    checker.add_argument("--stubs", type=pathlib.Path, required=True, help="port/include/stubs")
    checker.add_argument("--define", action="append", default=[], help="a macro the port defines")
    checker.add_argument("ps2", type=pathlib.Path, help="ps2/src")
    checker.add_argument("-o", "--output", type=pathlib.Path, required=True, help="written when the check passes")
    options = parser.parse_args()
    try:
        if options.command == "list":
            names = set()
            for source in sorted(options.port.rglob("*.cpp")):
                if "tests" in source.relative_to(options.port).parts:
                    continue
                try:
                    names.update(tagged_names(read(source)))
                except Error as error:
                    raise Error(f"{source}:{error}") from None
            # Left alone when nothing changed, so that no ps2/src unit is rebuilt for it.
            write(options.output, "".join(name + "\n" for name in sorted(names)), if_changed=True)
            return 0
        overrides = set(read(options.overrides).split("\n")) - {""}
        if options.command == "strip":
            try:
                stripped = strip(read(options.source), overrides, renames(options.stubs), options.define)
            except Error as error:
                raise Error(f"{options.source}:{error}") from None
            # Left alone when unchanged, so that only the units a change reaches are recompiled.
            write(options.output, f'#line 1 "{options.source.resolve().as_posix()}"\n{stripped}', if_changed=True)
            return 0
        unmatched = set(overrides)
        for source in sorted(options.ps2.rglob("*.cpp")):
            # tools/mwccgap compiles a temporary beside the source it came from.
            if source.name.startswith("tmp"):
                continue
            stubs = options.stubs / source.relative_to(options.ps2).with_suffix(".hpp")
            try:
                found = overridden(read(source), overrides, renames(stubs), options.define)
            except Error as error:
                raise Error(f"{source}:{error}") from None
            for _, definition in found:
                unmatched -= {definition.name, definition.spelled}
        if unmatched:
            raise Error("".join(
                f"{name}: tagged PC_OVERRIDE in port/src, but no ps2/src unit defines it under that signature\n"
                for name in sorted(unmatched)).rstrip())
        write(options.output, "")
    except Error as error:
        sys.exit(str(error))
    return 0


if __name__ == "__main__":
    sys.exit(main())
