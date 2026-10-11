#!/usr/bin/env python3
"""Bind an object's float constants to retail's literal pool.

    literals.py --bind <object> [<object> ...]
    literals.py --resolve-names <image> <object> [<object> ...]
    literals.py --report <source or object>

A source writes the number. Nothing in it names an address, and nothing has to
be kept in step by hand: this reads the address each constant has to land on
out of retail and rewrites the object to use it.

## How the toolchain places a literal

Established by experiment against `tools/compilers/mw/2.3.3`, because the
manual describes only the first line of it.

MWCC gives each *distinct* float constant of a translation unit a `.lit4` or
`.lit8` section of its own, holding just that value, with a local `@<digits>`
symbol on it, and loads it through an `R_MIPS_LITERAL` relocation on a
gp-relative `lwc1` or `ld`. The numbers rise in order of first use -- they come
off the same counter as the compiler's other invented names, so they are not
contiguous -- and the sections appear in the object in the reverse of that
order. The deduplication is per translation unit and has no limit: one unit
with 64 other constants between two uses of `0.1f` still gets one section for
it, and a constant that reaches the same bits by another route --
`3.1415927f * 2.0f`, `(float) 6.283185307179586` -- lands on that same section.

Version 2.3 of the toolchain, which is also in the tree, does all of this
identically; see re/ai/build_pipeline.md.

MWLD's `LITERAL` directive then emits, at the point the directive stands:
every object's `.lit8` sections, then every object's `.lit4` sections; objects
in link order; and within an object, its sections in `@` order.

**It does not merge.** The manual says the directive "merges them so that you
do not have duplicate entries"; version 2.4 (Engineering Build 0017), which is
what `mwldmips.exe` here is, does not, not even when the two sections carry the
same value under the same `@` name. Retail's own pool holds `6.2831855f` 29
times over, which says the same thing about the linker that built it.

## Why that is not enough, and what is done instead

Retail's pool is data of its own -- ps2/src/literals.cpp defines it -- and it
cannot be rebuilt from the objects' constants. So an object that brings its own
copy of a constant has that copy appended rather than merged, and the small
data after the pool moves. Writing `0.1f` in a source used to be enough to
shift the image.

Nor can retail's pool be predicted from the rules above. Retail's copy of the
pool holds one value at several addresses *within a single function* --
`DrawNPCDraw` loads `6.2831855f` from `0x2A1DBC` and again from `0x2A1DD0` --
and this MWCC cannot emit that, since it deduplicates per unit. Retail was
built by a toolchain that differs here.

What is left is retail's own instruction stream, which says exactly which entry
each load wants. `--bind` reads it out of the retail image (scripts/build/
retail.py), in four steps, stopping at the first that answers:

1. **The same instruction.** The function the relocation sits in is found in
   retail's symbol table and the instruction at that offset taken. If it is a
   gp-relative float load of the same value, that is the address. This is what
   a matching function resolves by.
2. **The same function, in order.** Otherwise the function's literal loads are
   lined up against ours in offset order and the *n*th load of a value is bound
   to retail's *n*th load of it. A function still being worked on gets retail's
   addresses before it matches, which is what stops the literal from being the
   thing that keeps it from matching.
3. **The same translation unit.** Otherwise any address the unit's other
   functions load that value from, taken from the unit's span in the linker
   script.
4. **The pool.** Otherwise the first entry in the pool holding that value. The
   constant is right and the game behaves the same; only that instruction's
   offset differs from retail's, and the unit not loading the value anywhere is
   worth saying so out loud.

A constant retail's pool does not hold at all is left where MWCC put it. MWLD
appends it, the small data after the pool moves, and `scripts/build/verify.py`
reports that as the failure it is.

Each bound relocation keeps the compiler's original `@<digits>` name as a
local absolute symbol at the selected retail address. `--resolve-names` does
the same for undefined `@` references introduced by generated assembly.
`--report` shows what a source's unit loads in retail, and what its object binds
to.
"""

import argparse
import bisect
import os
import re
import struct
import sys
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import disassemble  # noqa: E402
import region  # noqa: E402
import retail as retail_image  # noqa: E402

# Where MWLD's LITERAL directive put the pool in retail, taken from the image:
# the 8-byte entries run from the end of .data to the first 4-byte one, and the
# 4-byte entries run from there to the start of .sdata. Splitting the two
# matters when the pool is searched by value -- half of an 8-byte entry can
# read as a plausible 4-byte one. The release decides where; region.py says.
POOL = region.LITERAL_POOL

# The linker script that fixes _gp.
LCF = region.LCF
GP_RE = re.compile(r'^\s*_gp\s*=\s*(0x[0-9A-Fa-f]+)\s*;', re.M)

# A translation unit's placement in the linker script, which is what says
# where the unit starts and so which of retail's functions belong to it. The
# spelling is scripts/build/layout.py's, and the two have to agree.
PLACEMENT = re.compile(
    r'^\s*(\S+)\.o\s*\(\s*(\.[td]?text)\s*\)\s*//\s*(0x[0-9a-fA-F]+)\s*$', re.M)

# Compiler-invented datum names. The disassembler adds `__N` when retail has
# more than one symbol with the same original name.
NUMBERED_DATUM = re.compile(r'^@\d+(?:__\d+)?$')

# A function-local static: `name$N`, with splat's `__N` on a duplicate.
LOCAL_STATIC = re.compile(r'^[A-Za-z_]\w*\$\d+(?:__\d+)?$')

# A `@<n>` marked as external names retail's constant, not the one MWCC
# invented for this unit. The suffix keeps the two apart through the assembler;
# the address to pin is still retail's.
EXTERNAL_INVENTED = '$ext'
EXTERNAL_DATUM = re.compile(r'^(@\d+(?:__\d+)?)%s$' % re.escape(EXTERNAL_INVENTED))

# The suffix splat puts on all but the first copy of a duplicated name.
SUFFIX = re.compile(r'__\d+$')


# ------------------------------------------------------------------ ELF32
#
# Enough of ELF to drop a section, retarget a relocation and add a symbol.
# pyelftools reads but does not write, and objcopy cannot pick one `.lit4` out
# of the several an object carries -- `--remove-section` takes a name, and they
# all have the same one.

SHT_SYMTAB, SHT_STRTAB, SHT_REL, SHT_NOBITS = 2, 3, 9, 8
SHN_UNDEF, SHN_ABS, SHN_LORESERVE = 0, 0xFFF1, 0xFF00
STB_GLOBAL, STT_OBJECT, STT_FUNC = 1, 1, 2
R_MIPS_GPREL16, R_MIPS_LITERAL = 7, 8

SHDR = struct.Struct('<10I')
SYM = struct.Struct('<3IBBH')
REL = struct.Struct('<2I')


def cstr(blob, offset):
    end = blob.index(b'\0', offset)
    return blob[offset:end].decode()


class Section:
    def __init__(self, fields):
        (self.name_off, self.type, self.flags, self.addr, self.offset,
         self.size, self.link, self.info, self.align, self.entsize) = fields
        self.name = ''
        self.data = b''

    def fields(self):
        return (self.name_off, self.type, self.flags, self.addr, self.offset,
                self.size, self.link, self.info, self.align, self.entsize)


class Symbol:
    def __init__(self, name_off, value, size, info, other, shndx):
        self.name_off = name_off
        self.value = value
        self.size = size
        self.info = info
        self.other = other
        self.shndx = shndx
        self.name = ''

    @property
    def bind(self):
        return self.info >> 4

    @property
    def type(self):
        return self.info & 0xF


class Relocation:
    def __init__(self, offset, info):
        self.offset = offset
        self.sym = info >> 8
        self.type = info & 0xFF


class Object:
    """One relocatable object, read whole and written back whole."""

    def __init__(self, path):
        self.path = path
        with open(path, 'rb') as f:
            data = f.read()
        self.ident = data[:16]
        (self.e_type, self.e_machine, self.e_version, self.e_entry,
         self.e_phoff, e_shoff, self.e_flags, self.e_ehsize, self.e_phentsize,
         self.e_phnum, e_shentsize, e_shnum,
         self.e_shstrndx) = struct.unpack_from('<HHIIIIIHHHHHH', data, 16)

        self.sections = []
        for i in range(e_shnum):
            sec = Section(SHDR.unpack_from(data, e_shoff + i * e_shentsize))
            if sec.type != SHT_NOBITS:
                sec.data = data[sec.offset:sec.offset + sec.size]
            self.sections.append(sec)

        names = self.sections[self.e_shstrndx].data
        for sec in self.sections:
            sec.name = cstr(names, sec.name_off)

        # The symbol table, and the relocations decoded per section. Both are
        # rewritten below, so they are kept as objects rather than bytes.
        self.symtab = next((s for s in self.sections if s.type == SHT_SYMTAB),
                           None)
        self.symbols = []
        if self.symtab is not None:
            strings = self.sections[self.symtab.link].data
            for off in range(0, self.symtab.size, SYM.size):
                sym = Symbol(*SYM.unpack_from(self.symtab.data, off))
                sym.name = cstr(strings, sym.name_off)
                self.symbols.append(sym)
        for sec in self.sections:
            if sec.type == SHT_REL:
                sec.relocations = [Relocation(*REL.unpack_from(sec.data, off))
                                   for off in range(0, sec.size, REL.size)]

    # -- queries

    def owning_function(self, section_index, offset):
        """The function symbol a byte offset in a section falls in.

        MWCC gives each function a `.text` section of its own, so this is
        almost always the section's one function; the search covers an object
        whose sections were merged.
        """
        best = None
        for sym in self.symbols:
            if sym.shndx != section_index or sym.type != STT_FUNC:
                continue
            if sym.value <= offset and (best is None or sym.value > best.value):
                best = sym
        return best

    # -- edits

    def add_absolute(self, name, address):
        """Add a local absolute object symbol and return its table index.

        This deliberately does not deduplicate names: two loads against one
        compiler `@N` can select different retail pool entries, and each
        relocation identifies its intended value by symbol-table index.
        """
        self.symbols.append(Symbol(0, address, 0, STT_OBJECT, 0, SHN_ABS))
        self.symbols[-1].name = name
        return len(self.symbols) - 1

    def renumber_symbols(self, order):
        """Reorder the symbol table, following every relocation into it."""
        moved = {old: new for new, old in enumerate(order)}
        self.symbols = [self.symbols[old] for old in order]
        for sec in self.sections:
            if sec.type == SHT_REL:
                for rel in sec.relocations:
                    rel.sym = moved[rel.sym]

    def drop_sections(self, doomed):
        """Remove sections by index, with every reference to them.

        A symbol defined in a dropped section goes with it, and so does a
        relocation table that applies to one.
        """
        if not doomed:
            return
        self.renumber_symbols([i for i, sym in enumerate(self.symbols)
                               if sym.shndx not in doomed])

        keep = [i for i, sec in enumerate(self.sections)
                if i not in doomed
                and not (sec.type == SHT_REL and sec.info in doomed)]
        section_map = {old: new for new, old in enumerate(keep)}
        self.sections = [self.sections[i] for i in keep]
        self.e_shstrndx = section_map[self.e_shstrndx]
        for sym in self.symbols:
            if sym.shndx < SHN_LORESERVE:
                sym.shndx = section_map[sym.shndx]
        for sec in self.sections:
            # Every sh_link an object of MWCC's carries is a section index:
            # a relocation table's symbol table, a symbol table's strings, and
            # the `.mwcats` entry's function.
            if sec.link:
                sec.link = section_map.get(sec.link, 0)
            if sec.type == SHT_REL:
                sec.info = section_map[sec.info]

    def write(self):
        """Serialise, rebuilding the tables the edits above touched."""
        # A symbol table has to list its local symbols first, and sh_info is
        # where the global ones start.
        order = sorted(range(len(self.symbols)),
                       key=lambda i: (self.symbols[i].bind != 0, i))
        self.renumber_symbols(order)

        strtab = self.sections[self.symtab.link]
        strings = bytearray(strtab.data)
        for sym in self.symbols:
            if sym.name and not sym.name_off:
                sym.name_off = len(strings)
                strings += sym.name.encode() + b'\0'
        strtab.data = bytes(strings)
        strtab.size = len(strings)

        self.symtab.data = b''.join(
            SYM.pack(s.name_off, s.value, s.size, s.info, s.other, s.shndx)
            for s in self.symbols)
        self.symtab.size = len(self.symtab.data)
        self.symtab.info = sum(1 for s in self.symbols if s.bind == 0)

        for sec in self.sections:
            if sec.type == SHT_REL:
                sec.data = b''.join(REL.pack(r.offset, (r.sym << 8) | r.type)
                                    for r in sec.relocations)
                sec.size = len(sec.data)

        out = bytearray(52)
        for sec in self.sections:
            if sec.type == SHT_NOBITS:
                sec.offset = len(out)
                continue
            if sec.align > 1:
                out += b'\0' * (-len(out) % sec.align)
            sec.offset = len(out)
            out += sec.data
        out += b'\0' * (-len(out) % 4)
        shoff = len(out)
        for sec in self.sections:
            out += SHDR.pack(*sec.fields())

        out[:16] = self.ident
        struct.pack_into('<HHIIIIIHHHHHH', out, 16,
                         self.e_type, self.e_machine, self.e_version,
                         self.e_entry, self.e_phoff, shoff, self.e_flags,
                         self.e_ehsize, self.e_phentsize, self.e_phnum,
                         SHDR.size, len(self.sections), self.e_shstrndx)
        with open(self.path, 'wb') as f:
            f.write(out)


# ------------------------------------------------------------ retail's side


class Lazy:
    """Builds its value the first time it is called, and not before."""

    def __init__(self, build):
        self.build = build
        self.value = None

    def __call__(self):
        if self.value is None:
            self.value = self.build()
        return self.value


class Retail:
    """Retail's literal pool, and the instructions that load from it."""

    def __init__(self, root='.'):
        self.root = root
        self.gp = self._read_gp()
        self.pool = self._read_pool()
        # Every address a value sits at, in pool order. Retail's pool holds
        # some values many times over, so this is a list and not one address.
        self.by_value = {}
        for address, value in sorted(self.pool.items()):
            self.by_value.setdefault(value, []).append(address)
        self._functions = None
        self._units = None
        self._loads = {}
        self._unit_loads = {}
        self._unit_names = {}

    def _path(self, *parts):
        return os.path.join(self.root, *parts)

    def _read_gp(self):
        with open(self._path(LCF)) as f:
            m = GP_RE.search(f.read())
        if not m:
            raise SystemExit(f'literals: no _gp assignment in {LCF}')
        return int(m.group(1), 16)

    def _read_pool(self):
        """{address: entry bytes} for every 4- and 8-byte slot in the pool."""
        pool = {}
        for size, (lo, hi) in POOL.items():
            for address in range(lo, hi, size):
                entry = retail_image.read('main', address, size)
                if entry is not None:
                    pool[address] = entry
        return pool

    # -- what the disassembler recorded

    @property
    def functions(self):
        """{symbol: (image, vram, size)}, and the same sorted by address.

        The address and the size come from retail's own symbol table, as splat
        records it; the instructions are read from the image that holds it.
        """
        if self._functions is None:
            by_name, by_address = {}, []
            for image in disassemble.IMAGES:
                path = Path(self._path(region.CONFIG, f'{image}.symbols.txt'))
                if not path.exists():
                    continue
                for line in path.read_text(encoding='utf-8').splitlines():
                    m = disassemble.SYMBOL_ROW.match(line.strip())
                    if not m:
                        continue
                    name, attrs = m.group(1), m.group(3) or ''
                    # Each image's list also names the other images' symbols
                    # it refers to, as absolute addresses; those are not its own.
                    if 'type:func' not in attrs or 'absolute:True' in attrs:
                        continue
                    size = next((int(a.split(':', 1)[1], 0) for a in attrs.split()
                                 if a.startswith('size:')), 0)
                    if not size or name in by_name:
                        continue
                    vram = int(m.group(2), 16)
                    by_name[name] = (image, vram, size)
                    by_address.append((vram, name))
            by_address.sort()
            self._functions = (by_name, by_address)
        return self._functions

    @property
    def units(self):
        """{object: (start, end)} for every unit the linker script places.

        The script names each translation unit at the address it starts on, in
        address order per image, so a unit runs until the next one begins.
        """
        if self._units is None:
            with open(self._path(LCF)) as f:
                placed = PLACEMENT.findall(f.read())
            spans, by_section = {}, {}
            for name, section, address in placed:
                by_section.setdefault(section, []).append(
                    (int(address, 16), name + '.o'))
            for entries in by_section.values():
                entries.sort()
                for i, (address, name) in enumerate(entries):
                    end = entries[i + 1][0] if i + 1 < len(entries) else None
                    spans[name] = (address, end)
            self._units = spans
        return self._units

    # -- the loads themselves

    def loaded_address(self, word):
        """The pool address a gp-relative float load names, or None.

        `lwc1` and `ld` off $gp are the two forms MWCC emits for a 4- and an
        8-byte entry.
        """
        if word is None or (word >> 26) not in (0x31, 0x37):
            return None
        if (word >> 21) & 0x1F != 28:
            return None
        offset = word & 0xFFFF
        if offset >= 0x8000:
            offset -= 0x10000
        return self.gp + offset

    def function_loads(self, name):
        """[(offset, address, value)] for one function, in offset order."""
        if name in self._loads:
            return self._loads[name]
        loads = []
        entry = self.functions[0].get(name)
        if entry is not None:
            image, vram, size = entry
            code = retail_image.read(image, vram, size) or b''
            for offset in range(0, len(code) - 3, 4):
                address = self.loaded_address(
                    int.from_bytes(code[offset:offset + 4], 'little'))
                if address in self.pool:
                    loads.append((offset, address, self.pool[address]))
        self._loads[name] = loads
        return loads

    def local_name(self, obj_name, name, extent=None):
        """Retail's name for one of an object's functions, inside its own unit.

        A name retail uses more than once carries a `__N` suffix on all but
        the first copy, and the object still spells it without one, so a bare
        lookup finds whichever copy the symbol table lists first. For an
        overlay that is the wrong one: a different unit, with different
        literal loads. Resolving inside the unit's own span fixes it.
        """
        key = self._unit_names.get(obj_name)
        if key is None:
            key = {}
            span = self.units.get(obj_name)
            if span is not None:
                start, end = span
                if end is None and extent:
                    end = start + extent
                _by_name, by_address = self.functions
                first = bisect.bisect_left(by_address, (start, ''))
                window = []
                for address, candidate in by_address[first:]:
                    if end is not None and address >= end:
                        break
                    window.append(candidate)
                # An exact name wins over one reached by stripping a suffix.
                for candidate in window:
                    key[candidate] = candidate
                for candidate in window:
                    key.setdefault(SUFFIX.sub('', candidate), candidate)
            self._unit_names[obj_name] = key
        return key.get(name, name)

    def unit_loads(self, obj_name):
        """{value: [address]} for everything one translation unit loads."""
        if obj_name in self._unit_loads:
            return self._unit_loads[obj_name]
        found = {}
        span = self.units.get(obj_name)
        if span is not None:
            start, end = span
            _by_name, by_address = self.functions
            first = bisect.bisect_left(by_address, (start, ''))
            for address, name in by_address[first:]:
                if end is not None and address >= end:
                    break
                for _offset, entry, value in self.function_loads(name):
                    found.setdefault(value, set()).add(entry)
        found = {value: sorted(entries) for value, entries in found.items()}
        self._unit_loads[obj_name] = found
        return found


# ------------------------------------------------------------------- binding


def bind(path, retail, report):
    """Point one object's literal relocations at retail's pool entries.

    `retail` is called for the pool rather than passed: an object with no
    float constant of its own -- most of them -- must not pay for reading it.
    """
    obj = Object(path)
    if obj.symtab is None:
        return

    # The `@N` symbol MWCC defines in each literal section, and the constant
    # that section holds.
    literal_sections = {i for i, sec in enumerate(obj.sections)
                        if sec.name in ('.lit4', '.lit8') and sec.data}
    entries = {}       # symbol index -> (section index, value bytes)
    for i, sym in enumerate(obj.symbols):
        if sym.shndx in literal_sections and sym.type == STT_OBJECT:
            entries[i] = (sym.shndx, obj.sections[sym.shndx].data)
    if not entries:
        return
    retail = retail()
    obj_name = os.path.basename(path)

    # Every literal relocation, grouped by the function it sits in and kept in
    # offset order, which is what step 2 of the resolution lines up against
    # retail's own loads.
    sites = {}
    for sec in obj.sections:
        if sec.type != SHT_REL:
            continue
        for rel in sec.relocations:
            if rel.type != R_MIPS_LITERAL or rel.sym not in entries:
                continue
            function = obj.owning_function(sec.info, rel.offset)
            name = function.name if function is not None else None
            base = function.value if function is not None else 0
            sites.setdefault(name, []).append((rel.offset - base, rel))
    for group in sites.values():
        group.sort(key=lambda pair: pair[0])

    # MWCC gives every function its own `.text`, so the unit's extent is the
    # sum of them, not the largest.
    extent = sum(sec.size for sec in obj.sections
                 if sec.name == '.text' or sec.name.startswith('.text.'))
    for name, group in sorted(sites.items(), key=lambda kv: kv[0] or ''):
        local = retail.local_name(obj_name, name, extent) if name else name
        for _offset, rel, address, how in resolve(retail, obj_name, local,
                                                  group, entries):
            report.note(how, path, name, entries[rel.sym][1])
            if address is None:
                continue
            literal_name = obj.symbols[rel.sym].name
            rel.sym = obj.add_absolute(literal_name, address)
            # An absolute symbol is not a literal section any more, so the
            # relocation becomes the plain gp-relative one MWLD applies to any
            # small-data reference.
            rel.type = R_MIPS_GPREL16

    # Whatever is still referenced stays: MWLD appends it to the pool, which
    # moves the small data after it, and that is what says the constant is not
    # one of retail's.
    live = {rel.sym for sec in obj.sections if sec.type == SHT_REL
            for rel in sec.relocations}
    doomed = {section for sym, (section, _) in entries.items()
              if sym not in live}
    obj.drop_sections(doomed)
    obj.write()


# How a relocation found its address, worst last. Only the last two are worth
# saying anything about.
EXACT, ORDER, UNIT, ANYWHERE, NONE = 'exact', 'order', 'unit', 'anywhere', 'none'


def resolve(retail, obj_name, function, group, entries):
    """Bind one function's literal relocations. See the four steps above.

    Yields (offset, relocation, address_or_None, how) for each of `group`,
    which is the function's relocations in offset order.
    """
    loads = retail.function_loads(function) if function else []
    at_offset = {offset: (address, value) for offset, address, value in loads}
    spare = {}
    for _offset, address, value in loads:
        spare.setdefault(value, []).append(address)

    # 1. The same instruction. Taken first and taken out of `spare`, so a
    #    function that matches leaves nothing for the ordering to get wrong.
    decided = {}
    for offset, rel in group:
        value = entries[rel.sym][1]
        found = at_offset.get(offset)
        if found is not None and found[1] == value and found[0] in spare[value]:
            spare[value].remove(found[0])
            decided[offset] = (found[0], EXACT)

    for offset, rel in group:
        if offset in decided:
            yield (offset, rel) + decided[offset]
            continue
        value = entries[rel.sym][1]
        # 2. The same function, in order.
        if spare.get(value):
            yield offset, rel, spare[value].pop(0), ORDER
            continue
        # 3. The same translation unit.
        unit = retail.unit_loads(obj_name).get(value)
        if unit:
            yield offset, rel, unit[0], UNIT
            continue
        # 4. The pool, or nothing.
        pool = retail.by_value.get(value)
        yield offset, rel, (pool[0] if pool else None), ANYWHERE if pool else NONE


def number(value):
    return struct.unpack('<f' if len(value) == 4 else '<d', value)[0]


class Report:
    """What could not be bound from retail, collected and said once."""

    def __init__(self):
        self.notes = {ANYWHERE: set(), NONE: set()}

    def note(self, how, path, function, value):
        if how in self.notes:
            self.notes[how].add((path, function or '?', value))

    def emit(self):
        for path, function, value in sorted(self.notes[ANYWHERE]):
            print(f'literals: {path}: {function} loads {number(value)!r}, '
                  f'which no function of retail\'s unit loads; bound to the '
                  f'first pool entry holding it', file=sys.stderr)
        for path, function, value in sorted(self.notes[NONE]):
            packed = int.from_bytes(value, 'little')
            print(f"literals: {path}: {function} loads {number(value)!r} "
                  f"({packed:#0{len(value) * 2 + 2}x}), which retail's pool "
                  f"does not hold; MWLD appends it and moves the small data "
                  f"after it", file=sys.stderr)


# ------------------------------------------------------------------ reporting


def resolve_names(path, addresses):
    """Resolve generated assembly's external `@N` names inside one object.

    MWLD otherwise binds these references to whichever global definition its
    packing selected. An absolute local retains the literal name while fixing
    the relocation to the address recorded for that retail symbol. Definitions
    are localized as well; exported constants retain explicit retail padding
    so MWLD's local-literal packing keeps their placement.
    """
    obj = Object(path)
    changed = False
    for sym in obj.symbols:
        external = EXTERNAL_DATUM.match(sym.name)
        if sym.shndx == SHN_UNDEF and LOCAL_STATIC.match(sym.name) \
                and sym.name in addresses:
            # A function reaches its own function-local statics under retail's
            # names; the source defines them where retail put them, so the address
            # is retail's.
            sym.value = addresses[sym.name]
            sym.size = 0
            sym.info = STT_OBJECT
            sym.shndx = SHN_ABS
            changed = True
            continue
        if not external and not NUMBERED_DATUM.match(sym.name):
            continue
        if sym.shndx == SHN_UNDEF:
            # A marked reference carries retail's name under the suffix.
            wanted = external.group(1) if external else sym.name
            try:
                sym.value = addresses[wanted]
            except KeyError:
                raise SystemExit(
                    f'literals: {path}: no retail address for {wanted}')
            sym.size = 0
            sym.info = STT_OBJECT
            sym.shndx = SHN_ABS
            changed = True
        elif sym.bind == STB_GLOBAL:
            # Retail's numbered data is unit-local. Keeping it local also
            # prevents MWLD from accidentally binding another unit's same
            # compiler-invented spelling to this definition.
            sym.info = sym.type
            changed = True
    if changed:
        obj.write()


def retail_addresses(image, config=region.CONFIG):
    """Return the numbered datum and local static addresses recorded for one retail image."""
    try:
        rows = disassemble.read_symbol_table(config)[image]
    except KeyError:
        raise SystemExit(f'literals: unknown image {image!r}')
    return {name: address for name, (address, _type, _size) in rows.items()
            if NUMBERED_DATUM.match(name) or LOCAL_STATIC.match(name)}


def unit_report(target, root='.'):
    """What retail's copy of one translation unit loads from the pool."""
    retail = Retail(root)
    name = os.path.basename(target)
    name = name if name.endswith('.o') else name + '.o'
    span = retail.units.get(name)
    if span is None:
        print(f'literals: {LCF} places no unit called {name}', file=sys.stderr)
        return 1
    start, end = span
    print(f'{name}  {start:#010x}..{"" if end is None else f"{end:#010x}"}')
    loads = retail.unit_loads(name)
    if not loads:
        print('  loads nothing from the literal pool')
        return 0
    for value in sorted(loads, key=lambda v: (len(v), loads[v][0])):
        where = ' '.join(f'{a:08X}' for a in loads[value])
        print(f'  {number(value)!r:>24}  {len(value)}-byte  {where}')
    return 0


def main():
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--bind', action='store_true')
    ap.add_argument('--resolve-names', action='store_true')
    ap.add_argument('--report', action='store_true')
    ap.add_argument('args', nargs='+')
    a = ap.parse_args()

    if a.resolve_names:
        if len(a.args) < 2:
            ap.error('--resolve-names requires an image and at least one object')
        addresses = retail_addresses(a.args[0])
        for path in a.args[1:]:
            resolve_names(path, addresses)
        return 0

    if a.report:
        return max(unit_report(target) for target in a.args)

    if a.bind:
        report = Report()
        pool = Lazy(Retail)
        for path in a.args:
            bind(path, pool, report)
        report.emit()
        return 0

    ap.error('one of --bind/--resolve-names/--report is required')


if __name__ == '__main__':
    sys.exit(main())
