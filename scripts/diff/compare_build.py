#!/usr/bin/env python3
"""Report every way the build differs from retail, and where.

    compare_build.py                          # everything, all images
    compare_build.py --section main           # one image
    compare_build.py --context 4              # more instructions per hit
    compare_build.py <retail_elf> <build_elf> # non-default paths

Two independent comparisons run, because neither implies the other:

CONTENT -- the bytes. Every differing byte is attributed to the symbol that
    owns it, via the splat configuration (ref_index.py). Since
    that index records which dump a symbol came from, a hit in text is
    disassembled and reported as an instruction at an offset into a function,
    and a hit in data as the differing bytes. This is verify.py's pass/fail
    made specific.

PLACEMENT -- the symbol table. Un-migrated functions are reassembled from .s
    with retail's own bytes, so their addresses should coincide with retail's
    unless something earlier in the same region has a different size than the
    code it replaces. A name-joined, address-sorted comparison pinpoints the
    first symbol whose size or content diverges; everything after it differs
    only because of that. It also reports symbols whose size or occurrence
    count changed, and names the index knows that the build never defines.

Placement needs a symbol table, so it covers the main executable only -- the
overlays ship as raw images. Content covers all three.

Needs mips-ps2-decompals-nm/readelf/objdump on PATH, so run it in the Kazarov
shell:

    tools/kazarov/scripts/kazarov shell python3 scripts/diff/compare_build.py
"""
import argparse
import bisect
import os
import re
import shutil
import subprocess
import sys
import tempfile
from collections import namedtuple

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ref_index  # noqa: E402  (needs the script's own directory on the path)
sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__))), 'build'))
import region  # noqa: E402

NM = "mips-ps2-decompals-nm"
READELF = "mips-ps2-decompals-readelf"
OBJDUMP = "mips-ps2-decompals-objdump"

REPO = ref_index.REPO
RETAIL_ELF = os.path.join(REPO, region.EXTRACTED_ISO, "SCUS_971.11")
BUILD_ELF = os.path.join(REPO, region.BUILD, "SCUS_971.11")
PROVENANCE = os.path.join(REPO, region.BUILD, "symbol_provenance.txt")

# Where each image's bytes come from. main is a section of the executable;
# the overlays are whole files, headers and all.
OVERLAY_IMAGE = {"title": "TITLE.BIN", "dun": "DUN.BIN"}

Sym = namedtuple("Sym", "addr size type bind name")
Image = namedtuple("Image", "section vaddr retail build")


class Palette:
    """ANSI codes, or empty strings when the output is not a terminal."""

    CODES = {
        "red": "\033[31m", "green": "\033[32m", "yellow": "\033[33m",
        "blue": "\033[34m", "magenta": "\033[35m", "cyan": "\033[36m",
        "grey": "\033[90m", "bold": "\033[1m", "dim": "\033[2m",
        "reset": "\033[0m",
    }

    def __init__(self, enabled):
        for name, code in self.CODES.items():
            setattr(self, name, code if enabled else "")


def want_colour(choice):
    if choice == "always":
        return True
    if choice == "never":
        return False
    # https://no-color.org, then the usual "is anyone watching" test.
    return not os.environ.get("NO_COLOR") and sys.stdout.isatty()


def run(cmd):
    return subprocess.run(cmd, capture_output=True, text=True, check=True).stdout


def require_tools():
    missing = [tool for tool in (NM, READELF, OBJDUMP) if not shutil.which(tool)]
    if missing:
        raise SystemExit(
            "compare_build: %s not on PATH. This has to run in the Kazarov\n"
            "shell, which is where the toolchain lives:\n\n"
            "    tools/kazarov/scripts/kazarov shell python3 scripts/diff/compare_build.py\n"
            % ", ".join(missing))


# ---------------------------------------------------------------- symbol table


def load_symbols(elf_path):
    """Return {name: [Sym, ...]} from `nm -S --defined-only`, keeping every
    occurrence of a name rather than the first.

    Retail genuinely has multiple distinct LOCAL symbols sharing a mangled name
    at different addresses -- `GetArg__FR9input_strPiPPv` twice,
    `CheckChar__Fc` three times, plus per-TU libm constant tables. Keeping only
    the first mis-pairs them against whichever came first in the other binary's
    nm output, which accounted for 71 of 114 delta events in one sweep. Callers
    reconcile multi-occurrence names explicitly; see `reconcile`.
    """
    out = run([NM, "-S", "--defined-only", elf_path])
    syms = {}
    for line in out.splitlines():
        parts = line.split()
        if len(parts) < 4:
            # nm -S omits the size field for some symbol kinds (e.g. absolute);
            # fall back to a 3-field parse with size=0.
            if len(parts) == 3:
                addr_s, typ, name = parts
                size_s = "0"
            else:
                continue
        else:
            addr_s, size_s, typ, name = parts[0], parts[1], parts[2], parts[3]
        try:
            addr = int(addr_s, 16)
            size = int(size_s, 16)
        except ValueError:
            continue
        # 'a'/'A' = absolute symbol (a compile-time constant, not a real
        # linked address -- e.g. a #define-like value or assembler
        # equate). Comparing these as if they were addresses produces
        # nonsensical multi-megabyte "deltas" for symbols that were never
        # placed by the linker at all.
        if typ.lower() == "a":
            continue
        bind = "LOCAL" if typ.islower() else "GLOBAL"
        syms.setdefault(name, []).append(Sym(addr, size, typ.upper(), bind, name))
    return syms


def reconcile(name, rlist, clist, current_syms, bounds=None):
    """Pair retail's and current's occurrences of one (possibly duplicated) name by
    ascending-address rank rather than nm's arbitrary order. Returns (pairs,
    count_mismatch, clist_count); `clist_count` is the current-side count after
    folding in `__N` aliases, and is what callers must report.

    Before concluding a mismatch, look for `name__2`, `name__3`, ...: the split
    convention renames the symbol itself for the 2nd+ occurrence of a name
    retail genuinely duplicates, since GNU as cannot preserve retail's LOCAL
    scoping and two same-named GLOBALs would be a link error. The suffixes are
    not consecutive from 2 -- main.data.s has `maxFloorTbl`, `__3`, `__4` with
    no `__2` -- so scan a fixed range rather than stopping at the first gap.

    `bounds` applies to the base name and every alias: the numbering is
    assigned project-wide with no regard for section, so `gCd` sits outside
    `main` while `gCd__2` is the occurrence that matches.
    """
    rlist = sorted(rlist, key=lambda s: s.addr)
    clist = list(clist)
    for n in range(2, 20):
        alias = current_syms.get(f"{name}__{n}")
        if alias:
            clist.extend(alias)
    if bounds:
        clist = [s for s in clist if bounds[0] <= s.addr < bounds[1]]
    clist.sort(key=lambda s: s.addr)
    pairs = list(zip(rlist, clist))
    return pairs, len(rlist) != len(clist), len(clist)


def is_compiler_local_label(name):
    """Compiler-generated per-function branch-target labels ($L123, .L456,
    numeric-only) are not stable, globally-unique names -- the same literal
    label text is reused inside countless unrelated functions across the
    whole binary, so a plain name join treats all of them as "the same
    symbol" and produces meaningless giant address deltas. Real function/
    global names (including retail's own `.L0014xxxx`-style *local static*
    labels emitted by the disassembler, which DO encode a unique address in
    their own name) are unaffected by this filter."""
    if name.startswith("$L") or name.startswith(".L"):
        return True
    if name.startswith("$") or name.startswith("_$"):
        return True
    # VU microcode local labels/branch targets (e.g. "$CPwtr", "$JJ0") show
    # up prefixed with an extra leading underscore in nm's demangled-ish
    # output for some object kinds; also reject any name containing "$"
    # anywhere, which covers those plus MWCC's function-local static
    # "$NNN"-suffixed statics (e.g. "sub_map$439") -- the latter are a
    # different, real symbol per function, but sub_map$NNN's *own* address
    # is documented (CLAUDE.md) as not expected to match until the whole
    # surrounding binary matches anyway, so excluding them from this
    # broad-sweep report avoids drowning the real signal in known noise.
    if "$" in name:
        return True
    # MWCC also emits anonymous per-compiland literal-pool/jump-table labels
    # named "@NNN" (confirmed: e.g. "@181"/"@182" rodata literals emitted for
    # ps2/src/main.cpp's own switch statements) -- same non-unique-across-the-
    # whole-binary problem as "$NNN", just a different prefix convention.
    # Confirmed by direct count: retail's `main` section alone has 1149
    # duplicate-by-name symbols, of which 953 are "@NNN"-prefixed.
    if name.startswith("@"):
        return True
    return False


def section_header(elf_path, section):
    """(addr, file offset, size) for a named output section of an ELF."""
    out = run([READELF, "-S", "-W", elf_path])
    for line in out.splitlines():
        line = line.strip()
        if not line.startswith("["):
            continue
        # Format: [ N] Name  Type  Addr  Off  Size ...
        fields = line.split("]", 1)[1].split()
        if len(fields) < 5 or fields[0] != section:
            continue
        return int(fields[2], 16), int(fields[3], 16), int(fields[4], 16)
    raise ValueError(f"section {section!r} not found in {elf_path}")


def section_bounds(elf_path, section):
    addr, _off, size = section_header(elf_path, section)
    return addr, addr + size


# -------------------------------------------------------------------- content


def load_image(section, retail_elf, build_elf):
    """The retail and built bytes of one image, plus the address they load at.

    main lives inside the executable, so it is carved out by section header.
    The overlays are whole files -- MWo3 header included, which is why their
    load address is the overlay origin rather than the start of their text.
    """
    if section == "main":
        addr, off, size = section_header(retail_elf, section)
        with open(retail_elf, "rb") as f:
            f.seek(off)
            retail = f.read(size)
        baddr, boff, bsize = section_header(build_elf, section)
        with open(build_elf, "rb") as f:
            f.seek(boff)
            build = f.read(bsize)
        if addr != baddr:
            raise SystemExit(f"compare_build: {section} loads at {addr:#x} in "
                             f"retail but {baddr:#x} in the build")
        return Image(section, addr, retail, build)

    name = OVERLAY_IMAGE[section]
    retail_path = os.path.join(REPO, region.EXTRACTED_ISO, name)
    build_path = os.path.join(REPO, region.BUILD, name)
    for path in (retail_path, build_path):
        if not os.path.exists(path):
            raise SystemExit("compare_build: %s is missing; build the elf "
                             "target first." % os.path.relpath(path, REPO))
    with open(retail_path, "rb") as f:
        retail = f.read()
    with open(build_path, "rb") as f:
        build = f.read()
    return Image(section, ref_index.overlay_origin(), retail, build)


def differing_words(image):
    """Differing byte offsets in `image`, rounded out to 4-byte words.

    Word granularity, not byte: an instruction is the unit a difference is
    read in, and a data word is how the rest of the image is laid out anyway.
    Only the overlapping prefix is compared -- a length difference is reported
    separately, since past that point everything trivially "differs".
    """
    shared = min(len(image.retail), len(image.build))
    words = []
    for start in range(0, shared & ~3, 4):
        stop = start + 4
        if image.retail[start:stop] != image.build[start:stop]:
            words.append(start)
    # A trailing partial word, if the image is not a multiple of four.
    tail = shared & ~3
    if tail < shared and image.retail[tail:shared] != image.build[tail:shared]:
        words.append(tail)
    return words


def attribute(image, words):
    """Group differing words by the indexed symbol that owns them.

    Returns [(entry or None, [addresses])] in the address order the words came
    in, so the first group is the first difference in the image. Addresses no
    indexed row covers -- there should be none -- collect under one None group.
    """
    groups, order = {}, []
    for offset in words:
        addr = image.vaddr + offset
        entry = ref_index.covering(image.section, addr)
        key = (entry.symbol, entry.vram) if entry else None
        if key not in groups:
            groups[key] = (entry, [])
            order.append(key)
        groups[key][1].append(addr)
    return [groups[key] for key in order]


def disassemble(path, vaddr, start, stop):
    """{address: (word, text)} over one span of a raw image on disk."""
    out = run([OBJDUMP, "-D", "-b", "binary", "-m", "mips:5900", "-EL",
               f"--adjust-vma={vaddr:#x}", f"--start-address={start:#x}",
               f"--stop-address={stop:#x}", path])
    decoded = {}
    for line in out.splitlines():
        match = re.match(r"\s*([0-9a-f]+):\t([0-9a-f ]+?)\t(.*)", line)
        if match:
            text = re.sub(r"\s+", " ", match.group(3).strip())
            decoded[int(match.group(1), 16)] = (match.group(2).strip(), text)
    return decoded


class Disassembler:
    """Disassembles spans of both sides of an image, on demand.

    The bytes have to be on disk for objdump to read them, and for main they
    are a section of a larger ELF rather than a file of their own, so each
    side is written out once into a scratch directory and reused.
    """

    def __init__(self, image, tmpdir):
        self.image = image
        self.paths = {}
        for side, data in (("retail", image.retail), ("build", image.build)):
            path = os.path.join(tmpdir, f"{image.section}.{side}.bin")
            with open(path, "wb") as f:
                f.write(data)
            self.paths[side] = path

    def span(self, side, start, stop):
        """Disassemble [start, stop) -- addresses, not offsets: --adjust-vma
        biases the whole image, and --start-address is read after that."""
        return disassemble(self.paths[side], self.image.vaddr, start, stop)


class Locator:
    """Names an address from retail's own symbol table.

    The index splits text a function at a time but dumps data whole, so a
    difference in data attributes only as far as "somewhere in main.data".
    Retail's symbol table is finer-grained than that, and it is the side that
    is by definition correct, so it is what narrows a data hit down to a name.
    """

    def __init__(self, symbols):
        self.rows = sorted(
            (s.addr, s.size, name)
            for name, syms in symbols.items() if not is_compiler_local_label(name)
            for s in syms)
        self.starts = [row[0] for row in self.rows]

    def __call__(self, addr):
        at = bisect.bisect_right(self.starts, addr) - 1
        if at < 0:
            return None
        start, size, name = self.rows[at]
        # A sized symbol that does not reach the address is not the answer;
        # an unsized one is the best guess available.
        if size and addr >= start + size:
            return None
        return name, addr - start


def load_provenance():
    """{address: (provider, source)} -- which file the build sourced each
    symbol from, as layout.py worked it out. Absent until the build has
    run once, and only an annotation, so a missing file is not an error."""
    if not os.path.exists(PROVENANCE):
        return {}
    out = {}
    with open(PROVENANCE, encoding="utf-8") as f:
        for line in f:
            if line.startswith("#"):
                continue
            fields = line.rstrip("\n").split("\t")
            if len(fields) == 4:
                out[int(fields[0], 16)] = (fields[1], fields[2])
    return out


# --------------------------------------------------------------------- report


def hexdump(data):
    return " ".join(f"{b:02x}" for b in data)


def rel(path):
    """A repo-relative path, or the path itself when it is somewhere else."""
    inside = os.path.relpath(path, REPO)
    return path if inside.startswith(os.pardir) else inside


def report_content(image, args, p, provenance, locate, tmpdir):
    """Print every content difference in one image. Returns a count summary."""
    stats = {"bytes": 0, "words": 0, "functions": 0, "data": 0,
             "size_delta": len(image.build) - len(image.retail)}

    shared = min(len(image.retail), len(image.build))
    stats["bytes"] = sum(1 for i in range(shared)
                         if image.retail[i] != image.build[i])
    words = differing_words(image)
    stats["words"] = len(words)
    if not words and not stats["size_delta"]:
        return stats

    print(f"\n{p.bold}{p.cyan}CONTENT: {image.section}{p.reset}")
    if stats["size_delta"]:
        print(f"  {p.red}image size differs{p.reset}: retail {len(image.retail):#x}, "
              f"build {len(image.build):#x} ({stats['size_delta']:+#x})")
    if not words:
        return stats

    groups = attribute(image, words)
    disasm = Disassembler(image, tmpdir)

    shown = 0
    for entry, addrs in groups:
        is_code = entry.is_function if entry else False
        if is_code:
            stats["functions"] += 1
        else:
            stats["data"] += 1
        if shown >= args.max_symbols:
            continue
        shown += 1

        if entry:
            provider = provenance.get(entry.vram)
            origin = (f"{p.grey}from {provider[1]}{p.reset}" if provider
                      else f"{p.grey}from {entry.source}{p.reset}")
            print(f"\n  {p.bold}{entry.symbol}{p.reset}  "
                  f"{p.grey}{entry.vram:#010x}+{entry.size:#x}{p.reset}  {origin}")
        else:
            print(f"\n  {p.yellow}(no indexed symbol covers this address){p.reset}")

        shown_here = addrs[:args.context]
        if is_code:
            low, high = min(shown_here), max(shown_here) + 4
            retail_text = disasm.span("retail", low, high)
            build_text = disasm.span("build", low, high)
        for addr in shown_here:
            offset = addr - entry.vram if entry else addr
            at = f"{addr:#010x}{p.grey} +{offset:#06x}{p.reset}"
            start = addr - image.vaddr
            rbytes = image.retail[start:start + 4]
            bbytes = image.build[start:start + 4]
            if is_code:
                rword, rtext = retail_text.get(addr, (hexdump(rbytes), "?"))
                bword, btext = build_text.get(addr, (hexdump(bbytes), "?"))
                print(f"    {at}  {p.green}retail{p.reset} {p.grey}{rword}{p.reset}  {rtext}")
                print(f"    {' ' * 17}  {p.red}build {p.reset} {p.grey}{bword}{p.reset}  {btext}")
            else:
                # Data dumps cover a whole section, so name the address from
                # retail's symbol table as well as from the index.
                near = locate(addr) if locate else None
                where = f"  {p.grey}({near[0]}+{near[1]:#x}){p.reset}" if near else ""
                print(f"    {at}  {p.green}retail{p.reset} {hexdump(rbytes)}"
                      f"   {p.red}build{p.reset} {hexdump(bbytes)}{where}")
        if len(addrs) > len(shown_here):
            print(f"    {p.grey}... and {len(addrs) - len(shown_here)} more "
                  f"differing word(s) in this symbol{p.reset}")

    if len(groups) > shown:
        print(f"\n  {p.grey}... and {len(groups) - shown} more symbol(s) with "
              f"content differences (raise --max-symbols to see them){p.reset}")
    return stats


def report_placement(args, p, retail, current):
    """The symbol-table comparison. Returns a count summary."""
    bounds = current_bounds = None
    if args.section != "all":
        bounds = section_bounds(args.retail_elf, args.section)
        # The CURRENT binary's own section is generally a slightly
        # different total size than retail's (that size delta is the whole
        # subject of this tool) -- using retail's bounds to filter the
        # CURRENT side too incorrectly excludes legitimate tail-end symbols
        # that have drifted just past retail's own boundary but are still
        # well within the current section's actual (larger) end. Confirmed:
        # ELifeB2/ELifeB1/oldUnitNow/startCnt2/ELife1/sub_fogColor all sit
        # ~0x5c-0x94 bytes past retail's main-section end (0x2a2380) but
        # comfortably inside current's own end (0x2a249c) -- using a single
        # shared `bounds` for both sides falsely reported them as "current
        # has 0 occurrences".
        current_bounds = section_bounds(args.build_elf, args.section)

    # Only compare symbols that exist (by name) in BOTH binaries -- that's
    # the set asm-differ's own -o auto-symbol mode requires too, and it's
    # the only set for which "same name, different address" is meaningful.
    # Names present on one side alone are reported separately, below.
    common_names = set(retail) & set(current)
    rows = []
    skipped_local_labels = 0
    count_mismatches = []  # (name, retail_count, current_count) -- a real bug, not a join artifact
    shadowed = []  # retail names sharing an address with another symbol -- not a bug
    current_addrs = {s.addr for syms in current.values() for s in syms}
    for name in common_names:
        if is_compiler_local_label(name):
            skipped_local_labels += 1
            continue
        # Filter retail occurrences to the requested section bounds BEFORE
        # counting/reconciling -- otherwise a name legitimately duplicated
        # across DIFFERENT overlay regions (e.g. once in `main`, again in
        # `title`/`dun`) falsely reports as an in-section count mismatch.
        # Confirmed: `LoadData__Fv` has 3 retail occurrences total but only
        # 1 is actually within `main` bounds (the other 2 sit at 0x1dba930/
        # 0x1db5420, inside the title/dun overlay address range); `Command`
        # has 7 total, 6 within `main`. The CURRENT side needs the same
        # filter applied (see `reconcile`'s own bounds handling) -- e.g.
        # `gCd` (unsuffixed) exists in the current binary at 0x1d00767,
        # nowhere near `main`, while its `gCd__2` alias is the one actually
        # inside `main` and correctly matching retail.
        rlist_all = retail[name]
        if bounds:
            rlist_all = [s for s in rlist_all if bounds[0] <= s.addr < bounds[1]]
        if not rlist_all:
            continue
        clist_base = current[name]
        if current_bounds:
            clist_base = [s for s in clist_base if current_bounds[0] <= s.addr < current_bounds[1]]
        pairs, count_mismatch, clist_count = reconcile(name, rlist_all, clist_base, current, current_bounds)
        if count_mismatch:
            # Several of retail's symbols can sit at one address, and only one
            # of them can be the label the dumps emit there -- Command and
            # _title_text_end are both at 0x01DD5380, and _title_text_end has
            # to win because real code branches to it. The occurrence that lost
            # is not missing from the build, it is just not separately nameable:
            # mwld keeps an absolute symbol definition only if something
            # references it, and records its value as 0 even then. Report those
            # apart from the genuine article.
            matched = {r.addr for r, _c in pairs}
            shared = [r for r in rlist_all
                      if r.addr not in matched and r.addr in current_addrs]
            if len(shared) == len(rlist_all) - clist_count:
                shadowed.append((name, len(rlist_all), clist_count, shared))
            else:
                count_mismatches.append((name, len(rlist_all), clist_count))
        for r, c in pairs:
            if r.size < args.min_size:
                continue
            rows.append((r.addr, name, r, c))

    rows.sort(key=lambda t: t[0])
    mismatches = [row for row in rows if row[2].addr != row[3].addr]

    # A size of 0 means "not recorded", not "zero bytes long". Retail's ELF
    # comes from MWCC and sizes almost everything; the build's reassembled .s
    # files come from GNU as via `glabel`, which emits no `.size` at all, so
    # every symbol still supplied as asm reports 0 on the build side. Comparing
    # those as size changes buries the real ones -- of 5642 size differences in
    # one sweep of `main`, 5641 were this artifact and exactly one (`DebugFont`)
    # was a genuine change. Count them, and compare only the sizes both sides
    # actually state.
    size_mismatches = [row for row in rows
                       if row[2].size and row[3].size and row[2].size != row[3].size]
    unsized = sum(1 for row in rows if row[2].size and not row[3].size)

    # Names the join could never see, which is its one blind spot: a symbol
    # the build never defined at all.
    #
    # Only the ones the reference index names are worth reporting as missing.
    # The two symbol tables are not expected to agree name-for-name in either
    # direction: retail's is MWCC's own, which spells templates and statics
    # differently from the way disassemble.py sanitises them (`CDataAlloc2_1_`,
    # `CRect_i_`), while the build's carries the labels the disassembler
    # invented for unnamed code and data (`B_002A239D`, `D_00245308`) and the
    # `__N` aliases it uses for duplicated names. In one sweep that came to 385
    # retail-only and 4609 build-only names, none of them defects. The index is
    # the list of what the build is actually meant to provide, so it is the
    # only side of this that can be checked.
    indexed = {entry.symbol for section in ref_index.SECTIONS
               for entry in ref_index.entries(section)}
    unmatched_retail = [n for n in set(retail) - set(current)
                        if not is_compiler_local_label(n)]
    missing = sorted(n for n in unmatched_retail if n in indexed)
    unnamed_retail = len(unmatched_retail) - len(missing)
    unnamed_build = sum(1 for n in set(current) - set(retail)
                        if not is_compiler_local_label(n))

    # "Delta-change events": walking retail-address order, every point where
    # the (current_addr - retail_addr) delta changes value marks a distinct
    # size/ordering discrepancy introduced somewhere between the previous
    # symbol and this one -- i.e. the number of *separate* bugs still to
    # find, as opposed to the number of symbols merely inheriting an
    # upstream one's drift (which is most of `mismatches`).
    events = []
    prev_delta = 0
    for addr, name, r, c in rows:
        delta = c.addr - r.addr
        if delta != prev_delta:
            events.append((addr, name, prev_delta, delta, delta - prev_delta))
            prev_delta = delta

    where = "" if args.section == "all" else f" in {args.section} ({bounds[0]:#x}-{bounds[1]:#x})"
    print(f"\n{p.bold}{p.cyan}PLACEMENT{p.reset}{p.grey} -- {len(rows)} name-matched "
          f"symbols{where}, {skipped_local_labels} compiler-local labels skipped "
          f"as unreliable joins{p.reset}")

    if mismatches:
        addr, name, r, c = mismatches[0]
        print(f"\n  {p.red}{len(mismatches)} symbol(s) at the wrong address.{p.reset} "
              f"The first is the root-cause candidate; everything after it in "
              f"address order is likely just inheriting its drift:")
        print(f"    {p.bold}{name}{p.reset}")
        print(f"      retail {r.addr:#010x} size {r.size:#x} ({r.type}/{r.bind})")
        print(f"      build  {c.addr:#010x} size {c.size:#x} ({c.type}/{c.bind})"
              f"  {p.red}delta {c.addr - r.addr:+#x}{p.reset}")
        print(f"\n  First {min(args.first_n, len(mismatches))} in retail address order:")
        for addr, name, r, c in mismatches[:args.first_n]:
            note = "" if r.size == c.size else f"  {p.yellow}size {r.size:#x} -> {c.size:#x}{p.reset}"
            print(f"    {r.addr:#010x}  {name:<44s} {p.red}{c.addr - r.addr:+#9x}{p.reset}{note}")

    if events:
        print(f"\n  {len(events)} distinct delta-change event(s) -- each is a separate "
              f"root cause to find, not a symbol inheriting drift:")
        for addr, name, prev_d, new_d, change in events[:args.first_n]:
            print(f"    {addr:#010x}  {name:<44s} {prev_d:+#x} -> {new_d:+#x} "
                  f"{p.yellow}({change:+#x} bytes since the previous symbol){p.reset}")

    if size_mismatches:
        print(f"\n  {p.yellow}{len(size_mismatches)} symbol(s) whose size changed{p.reset} "
              f"-- a size change is what pushes everything after it out of place:")
        for addr, name, r, c in size_mismatches[:args.first_n]:
            print(f"    {r.addr:#010x}  {name:<44s} {r.size:#x} -> {c.size:#x} "
                  f"({c.size - r.size:+#x})")
    if unsized:
        print(f"\n  {p.grey}{unsized} symbol(s) sized by retail but not by the build, "
              f"which is what an unmigrated .s looks like -- `glabel` emits no .size, "
              f"so there is nothing to compare (not a bug).{p.reset}")

    if count_mismatches:
        print(f"\n  {p.red}{len(count_mismatches)} name(s) occurring a different number "
              f"of times{p.reset} -- likely a missing, extra or duplicated .s in "
              f"the {args.section if args.section != 'all' else 'main'} link order "
              f"for a name retail legitimately repeats:")
        for name, rc, cc in count_mismatches[:args.first_n]:
            print(f"    {name:<44s} retail {rc}, build {cc}")

    if missing:
        print(f"\n  {p.red}{len(missing)} symbol(s) the reference index names but the "
              f"build never defines{p.reset} -- a dump that is not being linked:")
        for name in missing[:args.first_n]:
            entry = ref_index.lookup(name)
            print(f"    {entry.vram:#010x}  {name:<44s} {p.grey}{entry.source}{p.reset}")

    if unnamed_retail or unnamed_build:
        print(f"\n  {p.grey}{unnamed_retail} name(s) exist only in retail's symbol "
              f"table and {unnamed_build} only in the build's. The two are not expected "
              f"to agree name-for-name -- retail spells templates and statics its own "
              f"way, the build adds the disassembler's B_/D_ labels and __N aliases -- "
              f"so these are not compared.{p.reset}")

    if shadowed:
        print(f"\n  {p.grey}{len(shadowed)} name(s) whose extra retail occurrence shares "
              f"an address with another symbol, so only one of them can label it "
              f"(not a bug):{p.reset}")
        for name, rc, cc, sh in shadowed[:args.first_n]:
            at = ", ".join(f"{r.addr:#010x}" for r in sh)
            print(f"    {p.grey}{name}: retail {rc}, build {cc}; shared at {at}{p.reset}")

    if args.csv:
        import csv
        with open(args.csv, "w", newline="") as f:
            w = csv.writer(f)
            w.writerow(["retail_addr", "name", "retail_size", "retail_type",
                        "build_addr", "build_size", "build_type", "addr_delta"])
            for addr, name, r, c in rows:
                w.writerow([f"{r.addr:#x}", name, f"{r.size:#x}", r.type,
                            f"{c.addr:#x}", f"{c.size:#x}", c.type, c.addr - r.addr])
        print(f"\n  Full comparison table written to {args.csv}")

    return {
        "compared": len(rows), "addr": len(mismatches), "size": len(size_mismatches),
        "count": len(count_mismatches), "events": len(events), "missing": len(missing),
    }


def report_summary(content, placement, p):
    print(f"\n{p.bold}{p.cyan}SUMMARY{p.reset}")

    clean = True
    header = f"  {'image':<8} {'bytes':>10} {'functions':>10} {'data spans':>11}"
    print(f"{p.grey}{header}{p.reset}")
    for section, stats in content.items():
        differs = stats["bytes"] or stats["size_delta"]
        clean = clean and not differs
        mark = f"{p.red}✗{p.reset}" if differs else f"{p.green}✓{p.reset}"
        colour = p.red if differs else p.green
        print(f"  {mark} {section:<6} {colour}{stats['bytes']:>10}{p.reset} "
              f"{stats['functions']:>10} {stats['data']:>11}")

    if placement:
        problems = [
            ("at the wrong address", placement["addr"]),
            ("with a changed size", placement["size"]),
            ("with a changed occurrence count", placement["count"]),
            ("never defined by the build", placement["missing"]),
        ]
        broken = [(what, n) for what, n in problems if n]
        if broken:
            clean = False
            roots = (f"; {placement['events']} distinct root cause(s)"
                     if placement["addr"] else "")
            print(f"\n  {p.red}✗{p.reset} placement: "
                  + ", ".join(f"{n} symbol(s) {what}" for what, n in broken) + roots)
        else:
            print(f"\n  {p.green}✓{p.reset} placement: all "
                  f"{placement['compared']} name-matched symbols are where retail "
                  f"puts them, at retail's size")

    if clean:
        print(f"\n  {p.green}{p.bold}The build matches retail.{p.reset}")
    return 0 if clean else 1


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("retail_elf", nargs="?", default=RETAIL_ELF)
    ap.add_argument("build_elf", nargs="?", default=BUILD_ELF)
    ap.add_argument("--section", default="all", choices=("all",) + ref_index.SECTIONS,
                    help="restrict to one image (default: all of them)")
    ap.add_argument("--first-n", type=int, default=20,
                    help="how many rows to print per placement category (default 20)")
    ap.add_argument("--max-symbols", type=int, default=40,
                    help="how many differing symbols to detail per image (default 40)")
    ap.add_argument("--context", type=int, default=8,
                    help="how many differing words to show per symbol (default 8)")
    ap.add_argument("--no-placement", action="store_true",
                    help="skip the symbol-table comparison")
    ap.add_argument("--csv", default=None,
                    help="write the full placement table to this CSV path")
    ap.add_argument("--min-size", type=int, default=0,
                    help="ignore symbols smaller than this many bytes (0 = no filter)")
    ap.add_argument("--color", "--colour", dest="colour", default="auto",
                    choices=("auto", "always", "never"))
    args = ap.parse_args()

    require_tools()
    p = Palette(want_colour(args.colour))
    provenance = load_provenance()

    for path in (args.retail_elf, args.build_elf):
        if not os.path.exists(path):
            raise SystemExit(f"compare_build: {path} does not exist")

    print(f"{p.bold}retail{p.reset} {rel(args.retail_elf)}")
    print(f"{p.bold}build {p.reset} {rel(args.build_elf)}")

    # Read once and share: nm over these two is the slowest thing here, and
    # both halves of the report want it.
    retail_syms = load_symbols(args.retail_elf)
    build_syms = load_symbols(args.build_elf)
    locate = Locator(retail_syms)

    sections = ref_index.SECTIONS if args.section == "all" else (args.section,)
    content = {}
    with tempfile.TemporaryDirectory(prefix="compare_build.") as tmpdir:
        for section in sections:
            image = load_image(section, args.retail_elf, args.build_elf)
            content[section] = report_content(image, args, p, provenance, locate, tmpdir)

    # Placement needs a symbol table on both sides, and only the executable
    # has one -- the overlays ship as raw images.
    placement = None
    if not args.no_placement and "main" in sections:
        placement = report_placement(args, p, retail_syms, build_syms)

    return report_summary(content, placement, p)


if __name__ == "__main__":
    sys.exit(main())
