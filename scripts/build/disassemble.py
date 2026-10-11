#!/usr/bin/env python3
"""Split the disc with splat, and fix up what splat cannot express.

    disassemble.py [image...]                    split; default all three
    disassemble.py --list-units                  the translation units
    disassemble.py --list-dumps                  the whole-section dumps
    disassemble.py --symbol-aliases OUT OBJ...   templated names, for objdiff

This is also where retail's section layout, the unit classification and the
spelling of a symbol live; the build and the diff tools read them from here.
"""

import argparse
import os
import subprocess
import re
import sys
from concurrent.futures import ProcessPoolExecutor
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import region  # noqa: E402

CONFIG = Path(region.CONFIG)
SRC = Path("ps2/src")
# Where splat writes the release's reference assembly.
ASM = region.ASM


# Retail's section layout, per image, for the release being built.
SECTIONS = region.SECTIONS

IMAGES = tuple(SECTIONS)

# The kinds that get a whole-section dump. `.text` and `.rodata` belong to
# translation units instead, and `header` is raw bytes.
DUMPED = ("data", "rdata", "sdata", "sbss", "bss")

# The sections holding vector-unit microcode and the DMA tags that upload it.
VU_SECTIONS = (".vutext", ".vudata")

RODATA_SECTIONS = (".rodata", ".rdata")


def sections_named(names):
    """[(start, end)] for every section of every image with one of these names."""
    return [
        (start, end)
        for image in IMAGES
        for name, _kind, start, end in SECTIONS[image]
        if name in names
    ]


def image_range(image):
    """The address range an image's own code and data occupy.

    main's symbol list carries the overlays' symbols too, so that its code can
    name what it calls into; this is what tells the two apart again.
    """
    return SECTIONS[image][0][2], SECTIONS[image][-1][3]


def dump_spans():
    """[(image, dump, start, end)] for every whole-section dump, in address order."""
    return [
        (image, f"{image}.{name.lstrip('.')}", start, end)
        for image in IMAGES
        for name, kind, start, end in SECTIONS[image]
        if kind in DUMPED
    ]


def dump_names():
    """Every whole-section dump the link can take, in address order."""
    return [name for _image, name, _lo, _hi in dump_spans()]


def dump_of(image, address):
    """The whole-section dump an address falls in, e.g. 'main.data', or None.

    An address in `.text` or `.rodata` belongs to the translation unit whose
    source defines it.
    """
    for name, kind, start, end in SECTIONS[image]:
        if start <= address < end:
            return f"{image}.{name.lstrip('.')}" if kind in DUMPED else None
    return None


def dump_path(name):
    """Where a whole-section dump lives, from its 'main.data' name.

    Under the image's own directory, so that an overlay's dumps are recognised
    as belonging to it; splat names the file after the subsegment and the
    segment type, which is why `.vutext` lands in `vutext.data.s`.
    """
    image, _, section = name.partition(".")
    for section_name, kind, _start, _end in SECTIONS[image]:
        if section_name.lstrip(".") == section:
            return f"{ASM}/data/{image}/{section}.{kind}.s"
    return f"{ASM}/data/{image}/{section}.{section}.s"


# MWCC numbers a function-local static by appending a dot and a digit, which is
# not a path character.
LOCAL_STATIC_SUFFIX = re.compile(r"\.(?=[0-9])")


def normalize_sym(sym):
    """How this project spells a symbol everywhere.

    A symbol has to be a file name, so the characters a C++ mangling carries
    that a path cannot are folded to '_'.
    """
    sym = sym.replace(",", "_").replace("<", "_").replace(">", "_")
    return LOCAL_STATIC_SUFFIX.sub("_", sym)


# A subsegment as the yaml writes them, one flow mapping per line.
SUBSEGMENT = re.compile(
    r"^\s*-\s*\{start:[^,]+,\s*type:\s*([^,}\s]+),\s*name:\s*([^,}\s]+)"
)

# The subsegment types that stand for a translation unit, and how each is
# built: `asm` is linked from the single file splat writes, `mixed` is compiled
# from its source.
UNIT_TYPES = {"asmtu": "asm", "c": "mixed", "cpp": "mixed"}


def image_of_unit(unit):
    """Which image a unit belongs to, from the directory its source is filed in."""
    head = unit.split("/", 1)[0]
    return head if head in IMAGES else "main"


def read_units(config_dir=CONFIG, src_dir=SRC):
    """[(kind, image, source, reference)] for every translation unit.

    YAML classifies split units, including assembly-only libraries that have
    no source anchor. Other sources are compiled as data-only units.
    """
    sources = {}
    for path in sorted(Path(src_dir).rglob("*")):
        # A compiler temporary beside a source is gone again a moment later, so a
        # scan that catches one would make a translation unit of a file that
        # cannot be found.
        if path.suffix in (".c", ".cpp") and not path.name.startswith("tmp"):
            unit = path.with_suffix("").relative_to(src_dir).as_posix()
            sources[unit] = path.as_posix()

    # A hand-written assembly source (crt0) stands in for the split assembly
    # of the unit it is named after. Only units the yaml splits qualify.
    handwritten = {
        path.with_suffix("").relative_to(src_dir).as_posix(): path.as_posix()
        for path in sorted(Path(src_dir).rglob("*.s"))
    }

    classified, order = {}, []
    data_only = {}
    for image in IMAGES:
        path = Path(config_dir) / f"{image}.yaml"
        if not path.exists():
            continue
        for line in path.read_text(encoding="utf-8").splitlines():
            match = SUBSEGMENT.match(line)
            if match and match.group(1) in UNIT_TYPES:
                unit = match.group(2)
                classified[unit] = (UNIT_TYPES[match.group(1)], image)
                order.append(unit)
            elif match and match.group(1) in DATA_ONLY_TYPES:
                data_only.setdefault(match.group(2), (match.group(1), image))
    # A library object with data and no code -- newlib's impure.c -- is a
    # `.data` subsegment with no code sibling. splat writes it on its own under
    # ps2/asm/data, and it links as a whole-unit object like any other asm unit.
    standalone = {}
    for unit, (kind, image) in data_only.items():
        if unit not in classified and unit.startswith("lib/"):
            classified[unit] = ("asm", image)
            order.append(unit)
            standalone[unit] = kind

    for unit, kind in classified.items():
        if kind[0] == "asm" and unit in handwritten:
            sources[unit] = handwritten[unit]

    # Library units are always linked from their split assembly until they
    # have real source. They therefore do not need placeholder source files
    # merely to make the unit discoverable.
    for unit in classified:
        if unit.startswith("lib/") and unit not in sources:
            sources[unit] = f"{src_dir}/{unit}.c"

    rest = sorted(unit for unit in sources if unit not in classified)
    rows = []
    for unit in order + rest:
        source = sources.get(unit)
        if source is None:
            continue
        kind, image = classified.get(unit, ("mixed", image_of_unit(unit)))
        reference = f"{ASM}/{unit}.s"
        if unit in standalone:
            reference = data_only_dump(unit, standalone[unit])
        rows.append((kind, image, source, reference))
    return rows


# The subsegment types a unit without code can consist of.
DATA_ONLY_TYPES = (".data", ".bss", ".sdata", ".sbss")


def data_only_dump(unit, kind):
    """Where splat writes a unit that is one data subsegment and nothing else."""
    return f"{ASM}/data/{unit}.{kind.lstrip('.')}.s"


def data_only_dumps():
    """The files of the data-only units, which drop_redundant_dumps must keep."""
    return {Path(row[3]).name for row in read_units() if row[0] == "asm" and row[3].startswith(f"{ASM}/data/")}


SYMBOL_ROW = re.compile(r"^(\S+)\s*=\s*(0x[0-9A-Fa-f]+)\s*;(?:\s*//\s*(.*))?$")


def read_symbol_table(config_dir=CONFIG):
    """{image: {name: (address, type, size)}} from the symbol lists splat reads."""
    table = {}
    for image in IMAGES:
        rows = {}
        path = Path(config_dir) / f"{image}.symbols.txt"
        if not path.exists():
            table[image] = rows
            continue
        for line in path.read_text(encoding="utf-8").splitlines():
            m = SYMBOL_ROW.match(line.strip())
            if not m:
                continue
            name, address, attrs = m.group(1), int(m.group(2), 16), m.group(3) or ""
            sym_type = "func" if "type:func" in attrs else "object"
            size = 0
            for attr in attrs.split():
                if attr.startswith("size:"):
                    size = int(attr.split(":", 1)[1], 0)
            rows[name] = (address, sym_type, size)
        table[image] = rows
    return table


def configure_spimdisasm():
    """Set what splat leaves at spimdisasm's default.

    splat's disassembler resets GlobalConfig when it configures, so this wraps
    that rather than running before it.
    """
    import spimdisasm
    from splat.disassembler import spimdisasm_disassembler

    original = spimdisasm_disassembler.SpimdisasmDisassembler.configure

    def configure(self):
        original(self)
        # main's code reaches into the overlays, which do not exist from its
        # side; left on, spimdisasm invents a symbol nothing defines and the
        # link fails on it. Off, the address stays the constant it is.
        spimdisasm.common.GlobalConfig.ALLOW_UNKSEGMENT = False
        # A value only becomes a symbol if it lands in a declared segment;
        # anything else stays the constant it is.
        spimdisasm.common.GlobalConfig.SYMBOL_FINDER_FILTERED_ADDRESSES_AS_CONSTANTS = (
            True
        )
        # Name an unreferenced symbol after the section it sits in -- `B_` in
        # .bss and `RO_` in .rodata. These names keep the generated section
        # dumps readable and distinguish padding from known objects.
        spimdisasm.common.GlobalConfig.AUTOGENERATED_NAMES_BASED_ON_SECTION_TYPE = True
        spimdisasm.common.GlobalConfig.AUTOGENERATED_NAMES_BASED_ON_DATA_TYPE = True

    spimdisasm_disassembler.SpimdisasmDisassembler.configure = configure


def configure_r5900_registers():
    """Spell the R5900's vector registers `$ACC`, `$I`, `$Q`, as gas wants them.

    splat turns this on only around the whole-unit file it writes last, so the
    per-function files come out unassemblable; setting it before the split
    covers both.
    """
    import spimdisasm
    from rabbitizer import TrinaryValue
    from splat.segtypes.common.c import CommonSegC

    original = CommonSegC.split

    def split(self, rom_bytes):
        if self.spim_section is not None:
            for symbol in self.spim_section.get_section().symbolList:
                if isinstance(symbol, spimdisasm.mips.symbols.SymbolFunction):
                    for instruction in symbol.instructions:
                        instruction.flag_r5900UseDollar = TrinaryValue.TRUE
        return original(self, rom_bytes)

    CommonSegC.split = split


def configure_vu_sections():
    """Keep the words of the VU sections the constants they are.

    Split as data, spimdisasm reads a word landing in a declared segment as a
    pointer; microcode holds none, so both halves of that guess are turned off
    for these sections alone.
    """
    from spimdisasm.mips.sections import MipsSectionBase
    from spimdisasm.mips.symbols import MipsSymbolBase

    ranges = sections_named(VU_SECTIONS)

    def is_vu(vram):
        return any(lo <= vram < hi for lo, hi in ranges)

    section_original = MipsSectionBase.SectionBase.checkWordIsASymbolReference
    symbol_original = MipsSymbolBase.SymbolBase._allowWordSymbolReference

    def check_word_is_a_symbol_reference(self, word):
        if is_vu(self.getVramOffset(0)):
            return False
        return section_original(self, word)

    def allow_word_symbol_reference(self, symbol_ref, word):
        if is_vu(self.vram):
            return False
        return symbol_original(self, symbol_ref, word)

    MipsSectionBase.SectionBase.checkWordIsASymbolReference = (
        check_word_is_a_symbol_reference
    )
    MipsSymbolBase.SymbolBase._allowWordSymbolReference = allow_word_symbol_reference


HEAP_SYMBOL = "GlobalDataBuffer"
# The quadword at the buffer's end holding its `used` cursor.
HEAP_CURSOR_SIZE = 0x10


def configure_heap_words():
    """Keep a word that only looks like a pointer into the heap a constant.

    Nothing static can point into a buffer the game allocates at runtime, so a
    word landing inside it is data that happens to read like an address --
    `.word 0x00786565`, the tail of "c06a_tameex", became
    `GlobalDataBuffer + 0x4DB4E5` and moved with the heap.

    Only a reference that resolves to the buffer *itself* is refused: a word
    that reaches a named object inside main's .bss is a real pointer, and
    spimdisasm names it after that object rather than after the buffer.

    Code gets the same treatment for a `%hi`/`%lo` pair. The buffer is a
    `CDataAlloc<1, 1690000>`: its storage array, then the `used` cursor in the
    last quadword. Code can name the array (`GlobalDataBuffer`) and the cursor
    (`GlobalDataBuffer + 0x19C9900`), so those two stay; a pair landing
    anywhere between is a constant that reads like an address -- the cursor's
    own offset off `this` in `CDataAlloc<1, 1690000>::Align64`
    (`lui $1, 0x19D; addu $1, $17, $1; lw $3, -0x6700($1)`), or libdma's
    0xFFFFFF mask -- and it comes out as `GlobalDataBuffer + 0x171E880` with a
    relocation the compiler never emits.
    """
    from spimdisasm.mips.symbols import MipsSymbolBase
    from spimdisasm.mips.symbols.analysis import InstrAnalyzer

    entry = read_symbol_table().get("main", {}).get(HEAP_SYMBOL)
    if entry is None:
        return
    heap, _, size = entry
    cursor = heap + size - HEAP_CURSOR_SIZE

    original = MipsSymbolBase.SymbolBase._allowWordSymbolReference

    def allow_word_symbol_reference(self, symbol_ref, word):
        if getattr(symbol_ref, "vram", None) == heap:
            return False
        return original(self, symbol_ref, word)

    MipsSymbolBase.SymbolBase._allowWordSymbolReference = allow_word_symbol_reference

    process_original = InstrAnalyzer.processSymbol

    def process_symbol(self, address, lui_offset, lower_instr, lower_offset):
        # Unpaired, both halves print as the immediates they are.
        if heap < address < cursor:
            return None
        return process_original(self, address, lui_offset, lower_instr, lower_offset)

    InstrAnalyzer.processSymbol = process_symbol


# A PC-relative branch and the label it lands on. `j`/`jal` are deliberately
# not matched: those encode an absolute target that mwld resolves correctly.
# `break` is the one other b-word and takes no label.
BRANCH_TARGET = re.compile(
    r"^(\s*/\*.*?\*/\s+b(?!reak\b)[a-z0-9]*\s+(?:[^,\s]+,\s*)*)"
    r"([A-Za-z_$.][\w.$]*)[ \t]*$",
    re.M,
)
# A function's own label sits at column zero and every other kind is indented,
# so the leading whitespace has to be allowed for and kept.
GLOBAL_LABEL = re.compile(r"^[ \t]*(?:glabel|jlabel) (\S+)$", re.M)

# gas treats a label starting with `.L` as local, and a function's size skips
# exactly the lines that look like `.L...:`.
TWIN_PREFIX = ".L"
TWIN_SUFFIX = "$b"

# A label standing on its own line, which is how spimdisasm writes a branch
# target that retail's symbol table happens to name -- `$L10:`, `loop1:`. The
# leading class excludes `.`, so the `.L…` labels it generates are left alone.
BARE_LABEL = re.compile(r"^[ \t]*([A-Za-z_$][\w.$]*):[ \t]*$", re.M)

# A label whose *address* is taken rather than one only branched to: the two
# halves of a %hi/%lo pair, and the absolute `j`/`jal` target encoding.
ADDRESSED_LABEL = re.compile(
    r"%(?:hi|lo)\(([A-Za-z_$.][\w.$]*)\)"
    r"|^\s*/\*.*?\*/\s+j(?:al)?\s+([A-Za-z_$.][\w.$]*)[ \t]*$",
    re.M,
)


def local_twin(name):
    return f"{TWIN_PREFIX}{name}{TWIN_SUFFIX}"


def globalize_addressed_labels(text):
    """Make every label whose address is taken a real, linker-visible symbol.

    gas otherwise writes the address against the section symbol, which
    the symbol table rebuilt by the object pipeline does not keep the name.
    A `.L` name is renamed to `T_`, because gas keeps `.L` out of the table.
    """
    addressed = {m.group(1) or m.group(2) for m in ADDRESSED_LABEL.finditer(text)}
    addressed.discard(None)
    if not addressed:
        return text

    renamed = {
        name: ("T_" + name[2:] if name.startswith(".L") else name) for name in addressed
    }

    alternation = "|".join(
        re.escape(name) for name in sorted(renamed, key=len, reverse=True)
    )
    text = re.sub(
        rf"(?<![\w.$])({alternation})(?![\w.$])", lambda m: renamed[m.group(1)], text
    )

    finals = "|".join(
        re.escape(name) for name in sorted(set(renamed.values()), key=len, reverse=True)
    )
    # `glabel` is deliberately not matched: such a label is already a symbol,
    # and promoting one in .rodata is not a valid constant.
    return re.sub(rf"^[ \t]*(?:alabel )?({finals}):?$", r"jlabel \1", text, flags=re.M)


def localize_alt_labels(text):
    """Give every named branch target inside a function a `.L` name.

    A function's size counts lines and only skips labels spelled `.L...`. One whose address is taken becomes a `glabel` instead,
    which it also skips, and which stays visible to the linker.
    """
    names = list(dict.fromkeys(BARE_LABEL.findall(text)))
    if not names:
        return text

    addressed = {m.group(1) or m.group(2) for m in ADDRESSED_LABEL.finditer(text)}

    keep = [name for name in names if name in addressed]
    if keep:
        alternation = "|".join(re.escape(name) for name in keep)
        text = re.sub(
            rf"^[ \t]*({alternation}):[ \t]*$", r"glabel \1", text, flags=re.M
        )

    rename = [name for name in names if name not in addressed]
    if not rename:
        return text

    alternation = "|".join(re.escape(name) for name in rename)
    return re.sub(
        rf"(?<![\w.$])({alternation})(?![\w.$])",
        lambda m: TWIN_PREFIX + m.group(1),
        text,
    )


def twin_branched_labels(text):
    """Give a local twin to every global label this file branches to.

    gas and mwld disagree by one instruction about an R_MIPS_PC16 addend, so a
    branch to a global label assembles one short. Only a name defined here can
    get a twin, and only the branches are repointed.
    """
    defined = set(GLOBAL_LABEL.findall(text))
    branched = {m.group(2) for m in BRANCH_TARGET.finditer(text)} & defined
    if not branched:
        return text

    text = BRANCH_TARGET.sub(
        lambda m: m.group(1)
        + (local_twin(m.group(2)) if m.group(2) in branched else m.group(2)),
        text,
    )
    alternation = "|".join(re.escape(name) for name in branched)
    return re.sub(
        rf"^([ \t]*(?:glabel|jlabel) ({alternation}))$",
        lambda m: f"{m.group(1)}\n{local_twin(m.group(2))}:",
        text,
        flags=re.M,
    )


GP_RELATIVE = re.compile(r"(?<![%\w])(-?0x[0-9A-Fa-f]+)\(\$28\)")


def restore_gp_relative_relocations(path, text, symbols, image_of):
    """Restore GP-relative relocations whose target has a declared symbol."""
    image = image_of(path)
    rows = symbols.get(image, {})
    gp = rows.get("_gp")
    if gp is None:
        return text

    by_address = {}
    for name, (address, sym_type, _size) in rows.items():
        # Splat preserves relocations for ordinary globals. MWCC's local
        # statics are the symbols it loses because their retail names use '$'.
        if sym_type == "object" and "$" in name:
            by_address.setdefault(address, []).append(name)

    def replace(match):
        immediate = match.group(1)
        offset = (
            -int(immediate[3:], 16)
            if immediate.startswith("-0x")
            else int(immediate, 16)
        )
        names = by_address.get(gp[0] + offset)
        if not names:
            return match.group(0)
        return f"%gp_rel({names[0]})($28)"

    return GP_RELATIVE.sub(replace, text)


def image_of_file():
    """A way to ask which image a reference file belongs to.

    title and dun occupy the same addresses, so a label's address means one
    thing in one and something else in the other.
    """
    by_reference = {reference: image for _k, image, _s, reference in read_units()}

    def image_of(path):
        posix = path.as_posix()
        if posix in by_reference:
            return by_reference[posix]
        parts = path.parts
        for marker in ("nonmatchings", "matchings"):
            if marker in parts:
                index = parts.index(marker)
                unit = "/".join(parts[index + 1 : -1])
                return by_reference.get(f"{ASM}/{unit}.s")
        if "data" in parts:
            return path.name.split("_", 1)[0]
        return None

    return image_of


def linked_object(path):
    """Which linked object a reference file's contents end up inside, or None.

    All of a unit's files answer with the same unit, however it is built; the
    whole-section dumps are objects of their own, and hand-written assembly is
    not splat's to rewrite.
    """
    parts = path.parts
    for marker in ("nonmatchings", "matchings"):
        if marker in parts:
            index = parts.index(marker)
            return "/".join(parts[index + 1 : -1])
    if "handwritten" in parts:
        return None
    if "data" in parts:
        return path.name
    posix = path.as_posix()
    prefix = f"{ASM}/"
    return posix[len(prefix) : -len(".s")] if posix.startswith(prefix) else None


# A generated local label, and every mention of one. gas keeps a name beginning
# with `.L` out of the symbol table however it is declared, which is why one
# reached from elsewhere has to be renamed.
GENERATED_LABEL = re.compile(r"\.L([0-9A-F]{8})\b")
GENERATED_DEFINITION = re.compile(
    r"^[ \t]*(?:jlabel[ \t]+)?\.L([0-9A-F]{8})[ \t]*:?[ \t]*$", re.M
)


def globalize_shared_labels(texts):
    """Export the generated labels that are reached from another file.

    A jump table travels with the function that uses it, but some of its
    targets sit in a different function and so in a different object. Those are
    renamed to the `T_` spelling and defined as `jlabel`; the rest stay local.
    """
    image_of = image_of_file()

    defined = {}
    referenced = {}
    for path, text in texts.items():
        owner = linked_object(path)
        if owner is None:
            continue
        image = image_of(path)
        for match in GENERATED_DEFINITION.finditer(text):
            defined.setdefault((image, match.group(1)), owner)
        for match in GENERATED_LABEL.finditer(text):
            referenced.setdefault((image, match.group(1)), set()).add(owner)

    shared = {
        key for key, home in defined.items() if referenced.get(key, set()) - {home}
    }
    if not shared:
        # A copy either way: the caller rewrites what it is given, and handing
        # back its own dict would edit its record of the files as they were.
        return dict(texts), 0

    by_image = {}
    for image, address in shared:
        by_image.setdefault(image, set()).add(address)

    out = {}
    for path, text in texts.items():
        addresses = by_image.get(image_of(path))
        if not addresses:
            out[path] = text
            continue
        alternation = "|".join(sorted(addresses))
        text = re.sub(rf"\.L({alternation})\b", lambda m: "T_" + m.group(1), text)
        out[path] = re.sub(
            rf"^[ \t]*T_({alternation}):[ \t]*$", r"jlabel T_\1", text, flags=re.M
        )
    return out, len(shared)


# Retail and MWCC call compiler-invented constants `@<n>`, but spimdisasm
# rewrites the leading `@` to `LIT_`. Restore the original spelling and quote
# it so GNU as does not interpret `@` as syntax.
INVENTED_MENTION = re.compile(r"\bLIT_(\d+(?:__\d+)?)\b")


def restore_invented_names(text):
    """Restore quoted retail names for compiler-invented constants."""
    return INVENTED_MENTION.sub(r'"@\1"', text)


def restore_invented_names_in_parts(root=ASM):
    """Restore invented names in residual dumps excluded from label passes."""
    changed = 0
    for path in sorted(Path(root).rglob("*.s")):
        if "parts" not in path.parts:
            continue
        text = path.read_text(encoding="utf-8")
        restored = restore_invented_names(text)
        if restored != text:
            path.write_text(restored, encoding="utf-8")
            changed += 1
    return changed


# What a worker process needs to run the per-file passes, built once in each
# of them. The symbol table is a few thousand rows and image_of is a closure,
# so neither travels well as an argument to every call.
_PER_FILE_STATE = {}


def _per_file_init():
    _PER_FILE_STATE["symbols"] = read_symbol_table()
    _PER_FILE_STATE["image_of"] = image_of_file()


def _per_file_passes(item):
    """The passes that read one file and nothing else."""
    path, text = item
    text = restore_gp_relative_relocations(
        path, text, _PER_FILE_STATE["symbols"], _PER_FILE_STATE["image_of"]
    )
    return path, drop_linker_symbol_labels(restore_invented_names(
        twin_branched_labels(localize_alt_labels(globalize_addressed_labels(text)))
    ))


# Symbols the linker script defines. splat labels the address they fall on
# inside a unit's data -- `_gp` sits in libmc's currentDir -- and the object
# would define them a second time.
LINKER_SYMBOLS = ("_gp", "__bss_start", "__data_start", "__data_end")


def drop_linker_symbol_labels(text):
    """Remove a data label that only restates a symbol the linker script defines."""
    return re.sub(r"^glabel (?:%s)\n" % "|".join(map(re.escape, LINKER_SYMBOLS)), "", text, flags=re.M)


def job_count():
    """How many processes to split per-file work over."""
    jobs = os.environ.get("JOBS")
    if jobs and jobs.isdigit() and int(jobs) > 0:
        return int(jobs)
    try:
        return len(os.sched_getaffinity(0))
    except AttributeError:
        return os.cpu_count() or 1


def fix_branches(root):
    """Apply every label pass to the assembly splat wrote.

    The files are read as a set, because whether a label has to be exported is
    a question about the whole tree rather than about one file. That question
    is the only one that is: once it has been answered, each file is rewritten
    from its own text alone, which is minutes of regular expressions over 50MB
    of assembly and the one part of the split worth spreading over the CPUs.
    """
    paths = sorted(
        path for path in Path(root).rglob("*.s") if "parts" not in path.parts
    )
    original = {path: path.read_text(encoding="utf-8") for path in paths}

    texts, shared = globalize_shared_labels(original)

    workers = min(job_count(), len(texts)) if texts else 1
    if workers > 1:
        # A chunk at a time: 5000 files one call each would spend longer
        # posting text between the processes than transforming it.
        with ProcessPoolExecutor(
            max_workers=workers, initializer=_per_file_init
        ) as pool:
            texts = dict(pool.map(_per_file_passes, texts.items(), chunksize=32))
    else:
        _per_file_init()
        texts = dict(_per_file_passes(item) for item in texts.items())

    changed = 0
    for path, text in texts.items():
        if text != original[path]:
            path.write_text(text, encoding="utf-8")
            changed += 1
    return changed, shared


def clear_generated():
    """Remove what a previous split wrote, so nothing stale survives it.

    splat writes files and never takes them away, so a moved boundary leaves
    the old file behind defining the same symbol twice. The checked-in
    residual section parts are not splat outputs and are kept.
    """
    removed = 0
    root = Path(ASM)
    if not root.is_dir():
        return removed
    for path in sorted(root.rglob("*.s"), reverse=True):
        if "handwritten" in path.parts or "parts" in path.parts:
            continue
        path.unlink()
        removed += 1
    for path in sorted((p for p in root.rglob("*") if p.is_dir()), reverse=True):
        if "handwritten" in path.parts or "parts" in path.parts:
            continue
        if not any(path.iterdir()):
            path.rmdir()
    return removed


ALIGN_DIRECTIVE = re.compile(r"^[ \t]*\.(?:align|balign)\b")
SECTION_DIRECTIVE = re.compile(r"^[ \t]*\.section[ \t]+(\S+)")


def drop_rodata_alignment(root):
    """Take the alignment directives out of the constants.

    A constant is written with retail's own trailing padding included, so an
    `.align` before it makes mwld pad a second time. Only inside the constants:
    an alignment in `.text` is spimdisasm placing a jump table, and is real.
    """
    changed = 0
    for path in sorted(Path(root).rglob("*.s")):
        if "parts" in path.parts:
            continue
        lines = path.read_text(encoding="utf-8").splitlines(keepends=True)
        out, in_rodata, dropped = [], False, False
        for line in lines:
            section = SECTION_DIRECTIVE.match(line)
            if section:
                in_rodata = section.group(1).rstrip(",") in RODATA_SECTIONS
            elif in_rodata and ALIGN_DIRECTIVE.match(line):
                dropped = True
                continue
            out.append(line)
        if dropped:
            path.write_text("".join(out), encoding="utf-8")
            changed += 1
    return changed


FIRST_ADDRESS = re.compile(r"^\s*/\* [0-9A-F]+ ([0-9A-F]{8}) ", re.M)

RODATA_SECTION = re.compile(r"^\s*\.section\s+\.(rodata|rdata)\b")


def align_rodata(root=ASM):
    """Say what each run of constants is aligned to.

    A run inside a function's file stops at that function's last constant, so
    the padding up to whatever follows is not part of it and everything after
    ends up low. The alignment written is the run's own address's.
    """
    rodata = sections_named(RODATA_SECTIONS)

    marked = 0
    for path in sorted(Path(root).rglob("*.s")):
        if "parts" in path.parts:
            continue
        lines = path.read_text(encoding="utf-8").splitlines(keepends=True)

        # Where each run starts and what address it starts at. The directive
        # goes straight after the `.section`, above the label, so the runs are
        # found first and the file rewritten afterwards.
        runs = {}
        section = None
        for index, line in enumerate(lines):
            if RODATA_SECTION.match(line):
                section = index
                continue
            if section is None:
                continue
            match = FIRST_ADDRESS.match(line)
            if match:
                runs[section] = int(match.group(1), 16)
                section = None

        written = []
        changed = False
        for index, line in enumerate(lines):
            written.append(line)
            address = runs.get(index)
            if address is None:
                continue
            if not any(lo <= address < hi for lo, hi in rodata):
                continue
            alignment = min(address & -address, 16)
            if alignment >= 4:
                written.append(f".align {alignment.bit_length() - 1}\n")
                changed = True

        if changed:
            path.write_text("".join(written), encoding="utf-8")
            marked += 1
    return marked


def drop_redundant_dumps():
    """Remove the per-unit section dumps splat writes alongside the real ones.

    They are the same bytes a third time, and nothing links them -- but they do
    look like another object to globalize_shared_labels, which would then count
    a unit's own jump table targets as reached from outside.
    """
    keep = {Path(dump_path(name)).name for name in dump_names()} | data_only_dumps()
    removed = 0
    data = Path(ASM) / "data"
    if data.is_dir():
        for path in sorted(data.rglob("*.s")):
            if "parts" not in path.parts and path.name not in keep:
                path.unlink()
                removed += 1
    return removed


# How many objects one nm invocation is given.
NM_BATCH = 256


def write_symbol_aliases(out, objects):
    """Map the sanitised spelling of a symbol back to the one MWCC emits.

    scripts/build/fixup_sections.sh feeds this to objcopy for the per-function
    reference objects alone, so that objdiff can pair a templated symbol with
    its base; nothing that links is renamed.
    """
    prefix = os.environ.get("MIPS_TOOL_PREFIX", "mips-ps2-decompals-")
    present = [obj for obj in objects if os.path.exists(obj)]
    pairs = {}
    # One nm per few hundred objects rather than one per object: the whole
    # build's objects are passed in, and the process launches cost more than
    # the reading does. nm prefixes each object's names with a `<path>:` line,
    # which carries none of the characters looked for below.
    for start in range(0, len(present), NM_BATCH):
        text = subprocess.run(
            [prefix + "nm", "--defined-only", *present[start:start + NM_BATCH]],
            capture_output=True,
            text=True,
        ).stdout
        for line in text.splitlines():
            # The `<path>:` line that starts each object's names, and the blank
            # line before it. A symbol line always has the type letter and the
            # name separated by a space.
            if " " not in line:
                continue
            name = line.split(" ", 2)[-1].strip()
            # Only the templated names. The dot in a function-local static is
            # the other thing normalize_sym folds, and MWCC spells that with
            # the dot too, so those already agree.
            if not any(c in name for c in "<>,"):
                continue
            sanitised = normalize_sym(name)
            if sanitised != name:
                pairs[sanitised] = name

    body = "".join(f"{sanitised} {real}\n" for sanitised, real in sorted(pairs.items()))
    os.makedirs(os.path.dirname(out) or ".", exist_ok=True)
    # Only when it has actually changed. Every object objdiff compares against
    # is rebuilt when this file is newer than it, and the names in it move only
    # when a template is added or retired -- so rewriting it unconditionally
    # reassembles a hundred objects on every edit to any source.
    if not os.path.exists(out) or open(out, encoding="utf-8").read() != body:
        with open(out, "w", encoding="utf-8") as f:
            f.write(body)
    print(f"disassemble: {len(pairs)} templated symbols -> {out}")


class ActiveSource:
    """A source file as the release being split compiles it."""

    def __init__(self, path):
        self.path = Path(path)

    def read_text(self, encoding="utf-8"):
        return region.active_text(self.path.read_text(encoding=encoding))


def configure_region_markers():
    """Read a source's function definitions as this release compiles it.

    splat decides which functions still need reference assembly from the
    function definitions in the source text; a definition a `#ifdef PAL` guard
    leaves out is not this release's.
    """
    from splat.segtypes.common.c import CommonSegC

    for name in ("get_funcs_defined_in_c", "get_global_asm_funcs",
                 "get_global_asm_rodata_syms"):
        original = getattr(CommonSegC, name)
        setattr(CommonSegC, name,
                staticmethod(lambda c_file, original=original: original(ActiveSource(c_file))))


def split_one(config):
    """Split a single image. Runs in a process of its own; see main()."""
    configure_region_markers()
    configure_spimdisasm()
    configure_r5900_registers()
    configure_vu_sections()
    configure_heap_words()
    from splat.scripts import split

    # The yaml carries every option; nothing is overridden here.
    split.main([config], None, verbose=False, use_cache=False)


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument(
        "names", nargs="*", help="images to split, or objects for --symbol-aliases"
    )
    ap.add_argument("--config", default=CONFIG, type=Path)
    ap.add_argument(
        "--list-units",
        action="store_true",
        help="print the translation units, one tab-separated row each",
    )
    ap.add_argument(
        "--list-dumps",
        action="store_true",
        help="print the whole-section dumps, one row each",
    )
    ap.add_argument(
        "--symbol-aliases",
        metavar="OUT",
        help="write the templated symbol names of the given objects",
    )
    ap.add_argument("--single", help=argparse.SUPPRESS)
    args = ap.parse_args()

    if args.list_units:
        for row in read_units(args.config):
            print("\t".join(row))
        return 0

    if args.list_dumps:
        for image, name, _lo, _hi in dump_spans():
            print(f"{image}\t{name}\t{dump_path(name)}")
        return 0

    if args.symbol_aliases:
        write_symbol_aliases(args.symbol_aliases, args.names)
        return 0

    if args.single:
        split_one(Path(args.single))
        return 0

    # Before anything is removed: clear_generated() takes out the whole of the
    # previous split, and a run that cannot reach splat would leave no ps2/asm/ at
    # all to build against until the next successful one.
    try:
        import splat  # noqa: F401
        import spimdisasm  # noqa: F401
    except ImportError as missing:
        print(f"disassemble: {missing.name} is not installed, so nothing can be "
              f"split; leaving the existing {ASM}/ alone.", file=sys.stderr)
        return 1

    # splat keeps the symbol table and the disassembler's context in module
    # globals, so a second image would split against the first one's symbols.
    # One process each is what keeps them apart.
    stale = clear_generated()
    if stale:
        print(f"disassemble: cleared {stale} file(s) from the previous split")

    configs = []
    for image in args.names or IMAGES:
        config = args.config / f"{image}.yaml"
        if not config.exists():
            print(
                f"disassemble: {config} is missing -- the configuration is "
                f"checked in under {args.config}/",
                file=sys.stderr,
            )
            return 1
        configs.append((image, config))

    # The images run at the same time. Each one already needs a process of its
    # own for the globals above, and they write disjoint files -- an image owns
    # its own subtree of ps2/asm/ and its own build/splat names -- so the split
    # costs the largest image rather than the sum of the three. Their output is
    # held back and printed per image, or three progress lines would interleave.
    print(f"disassemble: splitting {', '.join(image for image, _c in configs)} "
          f"at once; each image reports when it is done")
    running = []
    for image, config in configs:
        running.append((image, subprocess.Popen(
            [sys.executable, os.path.abspath(__file__), "--single", str(config)],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
        )))

    status = 0
    for image, process in running:
        output = process.communicate()[0]
        if output:
            sys.stdout.write(output)
            sys.stdout.flush()
        if process.returncode != 0:
            print(f"disassemble: splitting {image} failed", file=sys.stderr)
            status = status or process.returncode
    if status:
        return status

    unaligned = drop_rodata_alignment(ASM)
    if unaligned:
        print(
            f"disassemble: dropped the alignment from the constants in "
            f"{unaligned} file(s)"
        )
    aligned = align_rodata()
    if aligned:
        print(f"disassemble: aligned the constants in {aligned} file(s)")
    removed = drop_redundant_dumps()
    changed, shared = fix_branches(ASM)
    changed += restore_invented_names_in_parts()
    if removed:
        print(f"disassemble: dropped {removed} duplicate per-unit section dump(s)")
    print(
        f"disassemble: fixed up the labels in {changed} file(s); "
        f"{shared} reached from another object were exported"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
