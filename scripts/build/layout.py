#!/usr/bin/env python3
"""Work out what the link takes, in what order, and what objdiff compares.

    layout.py --link-order <dir> [--build-dir ps2/build/ntsc] [--main-tail obj...]
              [--tail <image>=<obj>...]
    layout.py --objdiff objdiff.json
    layout.py --provenance <file>
    layout.py --list-extra-objects

The linker script says where each translation unit starts: every object it names
carries the retail address of its first byte in the comment beside it. The link
order is those units in address order, a unit owns everything from its start to
the next one's, and nothing here reads a compiled object.

Placement comes from the linker script alone: where a unit starts is a fact
about the retail layout, not about how far its functions have been decompiled.
"""

import argparse
import bisect
import json
import os
import re
import sys
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
# verify.py owns what a source declares about a function; objdiff needs the
# fuzzy declarations to categorise a unit.
import verify  # noqa: E402
# disassemble.py owns the section layout, the unit classification and retail's
# symbol table.
import disassemble  # noqa: E402
import region  # noqa: E402

SECTIONS = ("main", "title", "dun")

SRC_DIR = "ps2/src"
# Where splat files a function's own assembly: `nonmatchings` for a function no
# source defines yet, `matchings` for one a source does.
ASM_DIRS = (f"{region.ASM}/nonmatchings", f"{region.ASM}/matchings")
LCF = region.LCF

# A unit's placement in the linker script: the object, the section it is placed
# by, and the retail address it starts at. Which image it belongs to is the
# section -- the overlays' objects carry privately-prefixed names.
PLACEMENT = re.compile(
    r"^\s*(\S+)\.o\s*\(\s*(\.[td]?text)\s*\)\s*//\s*(0x[0-9a-fA-F]+)\s*$", re.M
)
# A unit with data but no code has no `.text` line for mwld to place, so its
# position in the link order is given as a comment mwld skips:
#     // @ORDER editpartsdata.cpp.o 0x001579e0
ORDER_ONLY = re.compile(r"^\s*//\s*@ORDER\s+(\S+)\.o\s+(0x[0-9a-fA-F]+)", re.M)
IMAGE_OF = {".text": "main", ".ttext": "title", ".dtext": "dun"}

# objdiff runs `<custom_make> <custom_args...> <object path>` to rebuild. The
# path is meaningless here -- this project builds in one step -- so the trailing
# argument is swallowed by handing it to `sh -c` as $0.
OBJDIFF = {
    "min_version": "2.0.0-beta.5",
    "custom_make": "sh",
    "custom_args": ["-c", "exec ./build.sh ps2-pal objdiff"],
    "build_target": False,
    "build_base": True,
    "watch_patterns": [
        "ps2/src/**/*.{c,cpp,h,hpp,s,inc,lcf}",
        "ps2/include/**/*.{h,hpp,s,inc,lcf}",
        "ps2/asm/**/*.s",
        "ps2/config/*/*.{yaml,txt}",
    ],
    # What each function is expected to be, so objdiff can show the three
    # apart. A perfect function's bytes are retail's; a fuzzy one is the same
    # instructions in the same order with different registers, and says so with
    # FUZZY_MATCH; an undecompiled one has none.
    "progress_categories": [
        {"id": "game", "name": "Game"},
        {"id": "title", "name": "TITLE overlay"},
        {"id": "dun", "name": "DUN overlay"},
    ],
    # MWCC's own mangling -- `__ct__7CCameraFf` -- so objdiff can read those
    # names back if it is told which scheme to use.
    # Retail and MWCC name anonymous per-compiland constants `@N`. Some of
    # retail's constants also
    # live in a separate data unit, so their values are unavailable while its
    # code object is diffed. Ignore relocation targets in function scores;
    # instructions and relocation types still differ, and the linked-image
    # verification catches an incorrect target.
    "options": {
        "demangler": "codewarrior",
        "functionRelocDiffs": "none",
    },
}


# Which category a unit is counted under, from where its source sits. The
# overlays are called out because they are separate images; everything else
# objdiff is shown falls through to `game`.
CATEGORY_DIRS = (
    ("ps2/src/title/", "title"),
    ("ps2/src/dun/", "dun"),
)


def category_of(source):
    """The progress category a unit is counted under."""
    for prefix, name in CATEGORY_DIRS:
        if source.startswith(prefix):
            return name
    return "game"


def included_in_objdiff(source):
    """Return whether objdiff should expose this translation unit."""
    return not source.startswith("ps2/src/lib/")


def read_reference(placed=()):
    """(functions, dumps) for every image.

    `functions` is one row per per-function reference file, `dumps` one per
    whole-section dump the link can take. Both come from the splat
    configuration: retail's symbol table says what exists and where, and splat
    files each function under the translation unit it belongs to.

    A function in a unit that is taken whole has no file of its own; its
    reference is the unit's single assembly file, which is what objdiff
    compares it against and all the link ever needs.
    """
    functions = {s: [] for s in SECTIONS}
    dumps = {s: [] for s in SECTIONS}

    paths = {}
    for directory in ASM_DIRS:
        root = Path(directory)
        if root.is_dir():
            for path in root.rglob("*.s"):
                paths.setdefault(path.stem, path.as_posix())

    # Where a whole-file unit's functions are to be found, by address. Only
    # those fall back: a unit that is compiled has a file per function, and
    # pointing one at the whole unit would compare it against its neighbours
    # as well.
    reference = unit_references()
    kinds = {source: kind for kind, _image, source, _ref
             in disassemble.read_units()}
    spans = {
        image: owners(placed, image) for image in SECTIONS
    } if placed else {}

    def file_for(image, name, vram):
        if name in paths:
            return paths[name]
        source = owner_of(spans.get(image, []), vram)
        if source is None:
            return None
        if kinds.get(source) != "asm":
            return None
        return reference.get(source)

    table = disassemble.read_symbol_table()
    for image in SECTIONS:
        # main's list carries the overlays' symbols too, so that its code can
        # name what it calls into. Those belong to the overlay, not to main.
        lo, hi = disassemble.image_range(image)
        for name, (vram, sym_type, size) in table.get(image, {}).items():
            if not lo <= vram < hi:
                continue
            if sym_type == "func":
                path = file_for(image, name, vram)
                if path is not None:
                    functions[image].append((vram, name, size, path))

    for image in SECTIONS:
        functions[image].sort()

    for image, name, start, _end in disassemble.dump_spans():
        dumps[image].append((start, name, disassemble.dump_path(name)))
        dumps[image].sort()

    return functions, dumps


def unit_references():
    """{source: the assembly splat writes for that unit}.

    Every unit has one, whether or not the link uses it: a unit with nothing
    decompiled is assembled from it, and one that is compiled is compared
    against it.
    """
    return {
        source: reference
        for _kind, _image, source, reference in disassemble.read_units()
    }


def read_sources(src_dir=SRC_DIR):
    """The source files of the translation units, and the identities of the
    units that have none."""
    out = set()
    for root, _dirs, files in os.walk(src_dir):
        for name in files:
            # Compiler temporaries are not units.
            if name.endswith((".c", ".cpp")) and not name.startswith("tmp"):
                out.add(os.path.join(root, name))
    # Whole-assembly library units have no placeholder source. Keep their
    # synthetic source identities so linker-script object names still map to
    # the units derived from the split configuration.
    for _kind, _image, source, _reference in disassemble.read_units():
        out.add(source)
    return sorted(out)


def read_units(sources, path=LCF):
    """[(address, image, source)] for every translation unit, in link order.

    The linker script names objects; `sources` maps each back to the file it is
    built from, which is what the rest of this works in.
    """
    by_object = {}
    for source in sources:
        by_object.setdefault(os.path.basename(source), []).append(source)

    out, missing, ambiguous = [], [], []
    text = open(path).read()
    # A unit with no code shares its address with the unit whose code follows
    # it, and comes first: it covers nothing, and the next unit covers the code.
    entries = [
        (name, section, address, 1)
        for name, section, address in PLACEMENT.findall(text)
    ] + [(name, ".text", address, 0) for name, address in ORDER_ONLY.findall(text)]
    for name, section, address, rank in entries:
        candidates = by_object.get(name, [])
        if not candidates:
            missing.append(name)
        elif len(candidates) > 1:
            ambiguous.append(f'{name} ({", ".join(candidates)})')
        else:
            out.append((int(address, 16), rank, IMAGE_OF[section], candidates[0]))

    for name in missing:
        print(
            f"  ERROR: {path} places {name}.o, which no source under "
            f"{SRC_DIR}/ builds",
            file=sys.stderr,
        )
    # mwld identifies an object by base name alone and segfaults without a
    # diagnostic when two collide, so this cannot be allowed through either.
    for name in ambiguous:
        print(f"  ERROR: {name} is built from more than one source", file=sys.stderr)
    if missing or ambiguous:
        raise SystemExit(1)

    out.sort()
    return [(address, image, source) for address, _rank, image, source in out]


def owners(placed, section):
    """[(start, end, source)] for one image, covering it end to end."""
    spans = [(a, s) for a, img, s in placed if img == section]
    return [
        (a, spans[i + 1][0] if i + 1 < len(spans) else None, s)
        for i, (a, s) in enumerate(spans)
    ]


def owner_of(spans, address):
    """Which unit covers an address, from the spans one image was cut into.

    `owners` returns the spans in address order, so the only candidate is the
    last one starting at or below the address.
    """
    # A one-element key sorts ahead of any span that starts at the same
    # address, so this counts the spans starting at or below `address` without
    # comparing the `end` of any of them -- which is None for the last one.
    i = bisect.bisect_right(spans, (address + 1,)) - 1
    if i < 0:
        return None
    _start, end, source = spans[i]
    if end is not None and address >= end:
        return None
    return source


def owned_symbols(placed, functions, section):
    """{source: [symbol]} for one image, each unit's functions in address order.

    The same answer as calling `owner_of` for every function, resolved once for
    the whole image: a caller that wants every unit's symbols would otherwise
    re-cut the image and re-scan it per unit.
    """
    spans = owners(placed, section)
    grouped = {}
    for address, symbol, _size, _path in functions[section]:
        source = owner_of(spans, address)
        if source is not None:
            grouped.setdefault(source, []).append(symbol)
    return grouped


def link_order(args, placed, dumps, carved):
    """One mwld response file per image, in retail address order.

    A unit with nothing decompiled in it is taken whole: splat writes its code
    and its constants to a single assembly file and the link takes that object.
    A unit with any decompiled function is compiled instead.

    A whole-section dump is excluded when the migration carves it: the carved
    parts replace it and --main-tail or --tail supplies them.
    """
    for section in SECTIONS:
        # Every unit's object is named after its source, whether it was
        # compiled or assembled from the unit's reference; see
        # add_unit_asm_object in cmake/ObjectLists.cmake.
        items = [
            (a, f"{args.build_dir}/{s}.o") for a, img, s in placed if img == section
        ]
        items += [
            (a, f"{args.build_dir}/{src}.o")
            for a, name, src in dumps[section]
            if name not in carved
        ]
        items.sort()

        out, seen = [], set()
        for _a, obj in items:
            if obj not in seen:
                seen.add(obj)
                out.append(obj)
        if section == "main":
            out += args.main_tail
        out += [obj for entry in args.tail
                for image, _, obj in [entry.partition("=")] if image == section]

        with open(os.path.join(args.link_order, f"{section}_o_files"), "w") as f:
            f.write(" ".join(out) + "\n")
    return sum(1 for _ in placed)


def provenance(path, placed, functions, build_dir):
    """Where each function comes from, for reading rather than for the build."""
    rows = []
    kinds = {source: kind for kind, _image, source, _ref
             in disassemble.read_units()}
    for section in SECTIONS:
        spans = owners(placed, section)
        for address, symbol, _size, _src in functions[section]:
            source = owner_of(spans, address)
            kind = "asm" if kinds.get(source) == "asm" else "cpp"
            rows.append((address, kind, f"{build_dir}/{source}.o", symbol))
    rows.sort()
    with open(path, "w") as f:
        f.write("# Where each function in the build comes from.\n")
        f.write("# Generated by scripts/build/layout.py; do not edit.\n")
        f.write("#\n# address\tprovider\tsource\tsymbol\n")
        for address, kind, source, symbol in rows:
            f.write(f"{address:#010x}\t{kind}\t{source}\t{symbol}\n")
    return sum(1 for r in rows if r[1] == "cpp"), sum(1 for r in rows if r[1] == "asm")


def objdiff_target(build_dir, source):
    """Where scripts/build/objdiff_data.py writes a unit's target."""
    return f"{build_dir}/objdiff/{source}.o"


def objdiff_data_units(lcf=LCF, build_dir=region.BUILD):
    """[(source, code reference object or None, base object)] for every unit
    whose target scripts/build/objdiff_data.py writes: each compiled unit
    objdiff is shown, including those that define data and no code."""
    kinds = {source: kind for kind, _image, source, _ref
             in disassemble.read_units()}
    reference = unit_references()
    out = []
    for _address, _image, source in read_units(read_sources(), lcf):
        if not included_in_objdiff(source) or kinds.get(source) == "asm":
            continue
        code = f"{build_dir}/{reference[source]}.o"
        out.append((source, code if os.path.exists(reference[source]) else None,
                    f"{build_dir}/diff/{source}.o"))
    return out


def objdiff(path, placed, functions, build_dir, declared):
    """objdiff.json: one unit per translation unit.

    The target of a compiled unit is written by scripts/build/objdiff_data.py:
    the assembly splat writes for the unit's code, and retail's bytes for the
    data the unit defines, cut and named as its source does. The base is the
    unit compiled on its own. Pairing them that way is what lets a function be
    opened and diffed on its own, and what lets objdiff count the data.

    A unit with nothing decompiled in it is given no base at all, so it scores
    zero.
    """
    kinds = {source: kind for kind, _image, source, _ref
             in disassemble.read_units()}
    reference = unit_references()
    # Every unit's functions, resolved once per image rather than once per unit.
    owned = {image: owned_symbols(placed, functions, image) for image in SECTIONS}

    unit_list, decompiled, fuzzy_units = [], 0, 0
    for address, image, source in placed:
        if not included_in_objdiff(source):
            continue
        # Named after the source, so the sidebar groups a unit under the
        # library it came from.
        name = os.path.splitext(os.path.relpath(source, SRC_DIR))[0]
        unit = {"name": name.replace(os.sep, "/")}

        if kinds.get(source) != "asm":
            # Code and data both, or only data -- mainselect.cpp and literals.cpp
            # define nothing else -- written after the link.
            unit["target_path"] = objdiff_target(build_dir, source)
        elif os.path.exists(reference[source]):
            unit["target_path"] = f"{build_dir}/{reference[source]}.o"
        else:
            continue
        # The source compiled on its own, not the object the link takes, so a
        # function scores only for what the source writes.
        #
        # Only where there is a source to compile: a unit taken whole has no
        # base, and a hand-written assembly source is its own base.
        if kinds.get(source) != "asm":
            unit["base_path"] = f"{build_dir}/diff/{source}.o"
        elif source.endswith(".s"):
            # Hand-written assembly links as assembled, with nothing spliced.
            unit["base_path"] = f"{build_dir}/{source}.o"

        here = owned[image].get(source, [])
        # objdiff's `complete` flag overrides measured percentages; omit it
        # so regressions and unresolved symbol aliases remain visible.
        unit["metadata"] = {
            "progress_categories": [category_of(source)],
            "source_path": source,
        }
        if kinds.get(source) != "asm" or source.endswith(".s"):
            decompiled += 1
            if any(symbol in declared for symbol in here):
                fuzzy_units += 1

        unit_list.append(unit)

    config = dict(OBJDIFF, name="dcdecomp", units=unit_list)
    with open(path, "w", encoding="utf-8") as f:
        json.dump(config, f, indent=2)
        f.write("\n")
    return len(unit_list), decompiled, fuzzy_units


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--link-order", help="write <image>_o_files here")
    ap.add_argument("--objdiff", help="write the objdiff unit list here")
    ap.add_argument("--provenance", help="write the symbol provenance here")
    ap.add_argument("--build-dir", default=region.BUILD)
    ap.add_argument(
        "--lcf",
        default=LCF,
        help="the linker script to read the " "unit placement from",
    )
    ap.add_argument(
        "--main-tail",
        nargs="*",
        default=[],
        help="objects appended to main after the address-ordered ones",
    )
    ap.add_argument(
        "--tail",
        nargs="*",
        default=[],
        metavar="IMAGE=OBJECT",
        help="objects appended to one image after the address-ordered ones",
    )
    ap.add_argument(
        "--list-extra-objects",
        action="store_true",
        help="print the dumps objdiff needs as targets",
    )
    ap.add_argument(
        "--carved", nargs="*", default=[], help="section dumps the migration replaces"
    )
    args = ap.parse_args()

    sources = read_sources()
    placed = read_units(sources, args.lcf)
    functions, dumps = read_reference(placed)

    if args.list_extra_objects:
        # The whole-unit assembly of every unit objdiff shows, which is what its
        # target is compared against. None is linked: a unit taken whole is
        # assembled separately into the object the link takes.
        print("\n".join(sorted(
            reference
            for source, reference in unit_references().items()
            if included_in_objdiff(source) and os.path.exists(reference)
        )))
        return 0

    if args.link_order:
        n = link_order(args, placed, dumps, set(args.carved))
        print(
            f"layout: {n} translation units in address order -> "
            f"{args.link_order}/<image>_o_files"
        )
    if args.provenance:
        cpp, asm = provenance(
            args.provenance, placed, functions, args.build_dir
        )
        print(f"layout: {cpp} functions from cpp, {asm} from asm -> {args.provenance}")
    if args.objdiff:
        total, done, fz = objdiff(
            args.objdiff,
            placed,
            functions,
            args.build_dir,
            verify.declarations(),
        )
        print(
            f"layout: {total} objdiff units, {done} with a decompiled base "
            f"({fz} declared fuzzy) -> {args.objdiff}"
        )
    return 0


if __name__ == "__main__":
    sys.exit(main())
