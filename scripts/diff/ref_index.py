#!/usr/bin/env python3
"""Look up a retail symbol in the reference index.

The splat configuration under ``ps2/config/<region>/`` carries retail's own symbol table,
and splat files each function's assembly under the translation unit it belongs
to. Together those say where every symbol lives and which file holds it, which
makes them the project's symbol table for the retail side. They are preferred
over the link map for two reasons: they cover every function, including the
ones already superseded by a compiled ``.cpp``, and they name them the way the
compiler does (mangled) rather than the way mwld prints them (demangled, with
spaces in the signature).

    python3 scripts/diff/ref_index.py <symbol> [section]

prints ``<section> <source> <vram> <size> <image>`` with the numbers in hex,
and exits non-zero with a message on stderr if the symbol is not indexed.
With no section it searches all of them, which is how a caller can find out
whether a symbol belongs to an overlay.
"""
import bisect
import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

sys.path.insert(0, os.path.join(REPO, 'scripts', 'build'))
import disassemble  # noqa: E402
import region  # noqa: E402

CONFIG_DIR = os.path.join(REPO, region.CONFIG)

# Where splat files a function's own assembly: `nonmatchings` for a function no
# source defines yet, `matchings` for one a source does.
ASM_DIRS = (region.ASM + '/nonmatchings', region.ASM + '/matchings')

# The main executable first: it is what most lookups are for, and the overlays
# reuse its address space, so a plain search order would otherwise be ambiguous.
SECTIONS = ('main', 'title', 'dun')


class Entry:
    """One indexed symbol: where retail keeps it and which dump holds it."""

    def __init__(self, section, source, symbol, vram, size):
        self.section = section
        self.source = source
        self.symbol = symbol
        self.vram = vram
        self.size = size

    def __repr__(self):
        return 'Entry(%s, %s, %#x, %#x)' % (self.section, self.symbol,
                                            self.vram, self.size)

    @property
    def is_overlay(self):
        return self.section != 'main'

    @property
    def is_function(self):
        """Whether this row is one function or a whole data section.

        splat files text a function at a time under ps2/asm/nonmatchings and
        ps2/asm/matchings, and dumps every other section whole under ps2/asm/data --
        so the path says which of the two a row is, and nothing else has to
        guess.
        """
        return any(d in self.source for d in ('/nonmatchings/', '/matchings/'))

    @property
    def end(self):
        return self.vram + self.size

    def contains(self, addr):
        return self.vram <= addr < self.end

    def offset_of(self, addr):
        return addr - self.vram

    @property
    def image_offset(self):
        """Where the symbol sits in the file that gets diffed.

        An overlay ships as a raw image whose byte 0 is the MWo3 header, and
        the header loads at the overlay origin -- so an offset into the file is
        just the address biased by that origin. main ships as an ELF and is
        diffed by symbol, so there its address is the answer as it stands.
        """
        return self.vram - overlay_origin() if self.is_overlay else self.vram


def overlay_origin():
    """Where the overlays load, which scripts/build/region.py decides."""
    return region.OVERLAY_ORIGIN


_CACHE = {}
_STARTS = {}   # section -> the cached rows' start addresses, for bisect


def _asm_paths():
    """{symbol: path} for every per-function file splat wrote."""
    paths = {}
    for directory in ASM_DIRS:
        root = os.path.join(REPO, directory)
        for base, _dirs, files in os.walk(root):
            for name in files:
                if name.endswith('.s'):
                    full = os.path.join(base, name)
                    paths.setdefault(name[:-2],
                                     os.path.relpath(full, REPO))

    # A translation unit that is still wholly assembly is one asmtu file; index
    # its glabels too.
    asm_root = os.path.join(REPO, region.ASM)
    if os.path.isdir(asm_root):
        for name in os.listdir(asm_root):
            if not name.endswith('.s'):
                continue
            full = os.path.join(asm_root, name)
            with open(full, encoding='utf-8') as source:
                for line in source:
                    match = re.match(r'glabel\s+(\S+)', line)
                    if match:
                        paths.setdefault(match.group(1),
                                         os.path.relpath(full, REPO))
    return paths


def entries(section):
    """Every symbol of one image that the build can take, in address order.

    Cached: the callers that walk a whole binary ask for this per address, and
    main alone is four thousand rows.
    """
    if section in _CACHE:
        return _CACHE[section]

    if not os.path.isdir(CONFIG_DIR):
        raise SystemExit(
            'ref_index: %s/ is missing; run `./build.sh ps2-pal` '
            'to split the retail binary first.' % region.CONFIG)

    paths = _asm_paths()
    table = disassemble.read_symbol_table(CONFIG_DIR)

    # main's list carries the overlays' symbols too, so that its code can name
    # what it calls into. Those belong to the overlay, not to main.
    lo, hi = disassemble.image_range(section)

    out = []
    for name, (vram, sym_type, size) in table.get(section, {}).items():
        path = paths.get(name)
        if path is None or sym_type != 'func' or not lo <= vram < hi:
            continue
        out.append(Entry(section, path, name, vram, size))

    for image, dump, start, end in disassemble.dump_spans():
        if image == section:
            out.append(Entry(section, disassemble.dump_path(dump), dump,
                             start, end - start))

    out.sort(key=lambda entry: entry.vram)
    _CACHE[section] = out
    return out


def lookup(symbol, section=None):
    """Find `symbol`, in `section` if given or in all of them otherwise."""
    for name in (section,) if section else SECTIONS:
        for entry in entries(name):
            if entry.symbol == symbol:
                return entry
    return None


def covering(section, addr):
    """The indexed row whose span contains `addr`, or None if there is a hole.

    This is what turns a raw byte offset in the image back into "which
    function", and it is exact rather than nearest-preceding: the rows carry
    their own sizes, so an address between two of them reports as unattributed
    instead of being blamed on whatever came before it.
    """
    rows = entries(section)
    if section not in _STARTS:
        _STARTS[section] = [row.vram for row in rows]
    at = bisect.bisect_right(_STARTS[section], addr) - 1
    if at < 0:
        return None
    row = rows[at]
    return row if row.contains(addr) else None


def main():
    args = sys.argv[1:]
    if not 1 <= len(args) <= 2:
        raise SystemExit(__doc__)
    symbol, section = args[0], args[1] if len(args) == 2 else None
    if section and section not in SECTIONS:
        raise SystemExit('ref_index: unknown section %r; expected one of %s'
                         % (section, ', '.join(SECTIONS)))

    entry = lookup(symbol, section)
    if not entry:
        where = 'the %s index' % section if section else 'any reference index'
        raise SystemExit('ref_index: %s is not in %s' % (symbol, where))

    print('%s %s %#x %#x %#x' % (entry.section, entry.source, entry.vram,
                                 entry.size, entry.image_offset))


if __name__ == '__main__':
    main()
