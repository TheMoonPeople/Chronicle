#!/usr/bin/env python3
import argparse
import hashlib
import os
import re
import struct
import sys
from pathlib import Path

sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__))), 'diff'))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import region  # noqa: E402

# ---------------------------------------------------------------- match kinds
#
# What counts as a fuzzy match, and which functions claim to be one.
#
# A fuzzy match is a function whose compiled code has retail's instructions in
# retail's order, and differs only in which registers the compiler picked. It
# occupies the same bytes, so nothing after it moves; the rest of the image, and
# all of the data, still comes out byte-identical.
#
# `FUZZY_MATCH("<image>", <symbol>)` in a source declares one. The declaration
# is checked, not trusted: same length, same opcodes, same immediates and branch
# offsets, register fields free to differ. Anything looser is not a fuzzy match.

SECTIONS = ('main', 'title', 'dun')

# A declaration names a function that is a fuzzy match. Only the symbol is read:
# retail's names are unique across the three images.
DECLARATION = re.compile(r'^\s*FUZZY_MATCH\s*\(\s*"[^"]*"\s*,\s*([^)\s]+)\s*\)', re.M)

PERFECT, FUZZY, UNMATCHED = 'perfect', 'fuzzy', 'unmatched'

# Where the register fields sit, by instruction class. Everything outside them
# -- opcode, function code, shift amount, immediate, branch offset -- has to
# match, which is what makes this "same instructions, same control flow".
SPECIAL, MMI, COP0, COP1, COP2 = 0x00, 0x1C, 0x10, 0x11, 0x12
JUMP = (0x02, 0x03)

_RS_RT_RD = ~((31 << 21) | (31 << 16) | (31 << 11)) & 0xFFFFFFFF
_RS_RT = ~((31 << 21) | (31 << 16)) & 0xFFFFFFFF
# Coprocessor formats keep the fmt field in rs; ft/fs/fd are the operands.
_FT_FS_FD = ~((31 << 16) | (31 << 11) | (31 << 6)) & 0xFFFFFFFF


def _mask(word):
    """The bits of an instruction that a register choice cannot change."""
    op = word >> 26
    if op in JUMP:
        return 0xFFFFFFFF          # no registers: any difference is real
    if op in (SPECIAL, MMI):
        return _RS_RT_RD
    if op in (COP1, COP2):
        return _FT_FS_FD
    if op == COP0:
        return _RS_RT_RD
    return _RS_RT


def same_but_for_registers(a, b):
    """True if two instruction words differ only in their register fields."""
    if a == b:
        return True
    if (a >> 26) != (b >> 26):
        return False
    m = _mask(a)
    return (a & m) == (b & m)


def classify(retail, built):
    """PERFECT, FUZZY or UNMATCHED for one function's bytes.

    Anything not a whole number of instructions, or of a different length, is
    UNMATCHED: a fuzzy match is a register choice, not a different function.
    """
    if retail == built:
        return PERFECT
    if len(retail) != len(built) or len(retail) % 4:
        return UNMATCHED
    n = len(retail) // 4
    for a, b in zip(struct.unpack(f'<{n}I', retail), struct.unpack(f'<{n}I', built)):
        if not same_but_for_registers(a, b):
            return UNMATCHED
    return FUZZY


def _scan(pattern, src_dir):
    out = {}
    for root, _dirs, files in os.walk(src_dir):
        for name in sorted(files):
            if not name.endswith(('.c', '.cpp')):
                continue
            # Compiler temporaries live beside sources and can disappear
            # during a scan. Match CMake, layout.py and disassemble.py.
            if name.startswith('tmp'):
                continue
            path = os.path.join(root, name)
            # A marker under `#ifdef PAL` is the PAL build's alone.
            text = region.active_text(open(path, encoding='utf-8', errors='replace').read())
            for symbol in pattern.findall(text):
                out[symbol] = path
    return out


def declarations(src_dir='ps2/src'):
    """{symbol: source} for every FUZZY_MATCH in the tree."""
    return _scan(DECLARATION, src_dir)


# ------------------------------------------------------------------ file hashes


# Constants
COLOR_GREEN = '\033[92m'
COLOR_RED = '\033[91m'
COLOR_GREY = '\033[90m'
COLOR_END = '\033[0m'

class FileHash:
    def __init__(self, hash, offset=0, size=-1):
        self.hash = hash
        self.offset = offset
        self.size = size

    def compare(self, hash: str):
        return hash.lower() == self.hash.lower()

HASHES = {
    'DATA.HED': [FileHash('22b6684c0abb32eeb709e8a8794457681f1e0b7d55c9c3d0eced71326431df03')],
    'DUN.BIN': [FileHash('bf2051f70258d54fac37e0289d146c4000a71a3c5566bd7f1b198ea37bb4e3a2')],
    'MODHSYN.IRX': [FileHash('f4eb3549f7f2f3b6533444c33ec45fbb57102ad239853c3185dba973467b3ff9')],
    'INET.IRX': [FileHash('b12b384e6220b243c68c3526f44c1f6060da9b0814efff5732e4fdbb21c69f36')],
    'AN986.IRX': [FileHash('01cc63a4e90985c6057c2a7076696c35a4b854e33f3b50e2172abf7f0c389897')],
    'NETCNF.IRX': [FileHash('a5df47e49c430573cde4d3dafd09edd19f650581b95a9af7549ce4dda903800a')],
    'IOPRP21.IMG': [FileHash('5c82d57a3e81a53e16c665d8d4a99152005060fa6ea327066fe2bd3788a02549')],
    'LIBNET.IRX': [FileHash('0bb3ae21e961f9be997e1dcddc504ddeadd954d21d7599d2d38e0a61e652768e')],
    'USBD.IRX': [FileHash('d695af5f72cb766e597abe3b67cd88b4b2579445084488bea1aca6b48a594a22')],
    'PADMAN.IRX': [FileHash('c2200e7f7071b7e747e122bd036f70133efa6b631415f500a806b75d55fcb88b')],
    'SDRDRV.IRX': [FileHash('b75f0020a1e28ece42c8e790c2c48f7daf0773fd4dc4cca7ec72e044049198a3')],
    'IOPRP211.IMG': [FileHash('58e4dab3fbfd1b564a1bff1d9d62a10063d8c26a342af2198edb0b5623645943')],
    'SIO2MAN.IRX': [FileHash('f41e341efe08ac407fce467639d10bb2a4532b70a8a5829dbe350edef58516d7')],
    'CDVDMAN.IRX': [FileHash('1d5bd4024149cf4685e5d4e97952cf9a1e5f6e7834045a1b3780689f46c651c7')],
    'MCMAN.IRX': [FileHash('4f01c1ee5afc457b3a030b7910431f92ba623e3ecc234d5bc5be64f48e38cd3a')],
    'LIBSD.IRX': [FileHash('71596ef4029f672205e1523dedbd8dacfe5dfa50132c8a747b81289cd24f1db6')],
    'MSIFRPC.IRX': [FileHash('e91a27ad702873ae11764e1ba44516eeb3138e97fe5a43c713eb197b30ee7387')],
    'EZMIDI.IRX': [FileHash('8b6f8a8199197b0c9ebb403aeea6987d4106fb807ecc98bcc99b60251e648315')],
    'MODMIDI.IRX': [FileHash('2eed3e9598025d162a2424522a0dbd0ded343468a6d9dc7e50f55b6885d01851')],
    'MCSERV.IRX': [FileHash('c55abb687fd08e0df292c3636cfd9a34d06a3261a62a1916d06c463fa30cbae0')],
    'DATA.HD2': [FileHash('2fc1bddcbd16fb632d237a3ae5b2a29c62ae81b48a169be9e3fe20bdb934c0aa')],
    'TITLE.BIN': [FileHash('c7c825a22e249dd962678f686665fde121426b1270e0565663912ee7c0581f05')],
    'SYSTEM.CNF': [FileHash('988ef4d844b56d6b6dc5e9c2e12d48656f3c87d96e57a0583db53fe119c5f7f9')],
    'SCUS_971.11': [FileHash('de3015eefbe86dddbd126a122ef60eda0e29cd12b7d2eaf87ed3ad236e11989c'), FileHash('b128a252266b2e872fcc796ff18e9819a329cd0e0fa90155725793a19bb52c7e', 0x100, 0x1A2380)],
    'IFC003.CNF': [FileHash('666928da6e5c86a413117eabee81ef0785e3257249c97fc588f647fc88f2d739')],
    'IFC000.CNF': [FileHash('1a77809cb407c34bd95e520fb861242430523b336d0ec99402040d74c568a4f4')],
    'DEV000.CNF': [FileHash('95f18d485675aa913ab434f08807705188ff9050d1dd5fe435cc668c8f1de435')],
    'IFC001.CNF': [FileHash('3a357283e942bd05cf19e8c5f60e875db3c78b63e0d049247113acdf9e7a8dc4')],
    'NET003.CNF': [FileHash('60f38a9a8c902d23a8808390a79ba07446c157697882cca46149daa33b3e016d')],
    'NET000.CNF': [FileHash('211132a94378b8574be11007b2f0422a228928b56892b010cb3c987ca3f24e38')],
    'NET001.CNF': [FileHash('b7b67c815e7a66e6977cf7ccb5b6ae28d8d5ff288b84635a96b3ab392cf2eb35')],
    'DEV001.CNF': [FileHash('bb5c69cfeed43f3ba30ff8f2b38ee985f9e91c777582f7237beb6dd0c6389f83')],
    'DEV003.CNF': [FileHash('2313085880aa4199708c1426fc16c8b6f39db8c0703d9f7969d8c33fb4c7b7ba')],
    'NET.DB': [FileHash('7a8ecdfb6e303bbd3907054a29a616c648d27755db5819fb55ec0bf41bc37191')],
    'DMMY': [FileHash('32000a674a8e37c5345b6bce47c19e4894f8d01f59332aab614e1130a8ba5867')],
    'DATA.DAT': [FileHash('4f581c66d83d9270da6cb9adce7ec10b6b9ade5cf74cf44718bcdd8e70401fb5')]
}

# The PAL prototype's files, where they are not the same as NTSC's.
PAL_HASHES = {
    'DATA.DAT': [FileHash('55e1b687b180a49cf6f40a8582fa53ccb099a60af9b25defdf883360b8ae08a5')],
    'DATA.HD2': [FileHash('8e0d8e1f84f7a2ce2b3fed0b44d1ec4f94f7033f21c3e3e215ddfca224e70bbc')],
    'DUN.BIN': [FileHash('d6d07bbb82c2a78a72da54d5b9fa005a703009f6a7ec073856f9b316067aeeea')],
    'AN986.IRX': [FileHash('01cc63a4e90985c6057c2a7076696c35a4b854e33f3b50e2172abf7f0c389897')],
    'CDVDMAN.IRX': [FileHash('1d5bd4024149cf4685e5d4e97952cf9a1e5f6e7834045a1b3780689f46c651c7')],
    'EZMIDI.IRX': [FileHash('8b6f8a8199197b0c9ebb403aeea6987d4106fb807ecc98bcc99b60251e648315')],
    'INET.IRX': [FileHash('b12b384e6220b243c68c3526f44c1f6060da9b0814efff5732e4fdbb21c69f36')],
    'IOPRP211.IMG': [FileHash('58e4dab3fbfd1b564a1bff1d9d62a10063d8c26a342af2198edb0b5623645943')],
    'IOPRP21.IMG': [FileHash('5c82d57a3e81a53e16c665d8d4a99152005060fa6ea327066fe2bd3788a02549')],
    'LIBNET.IRX': [FileHash('0bb3ae21e961f9be997e1dcddc504ddeadd954d21d7599d2d38e0a61e652768e')],
    'LIBSD.IRX': [FileHash('71596ef4029f672205e1523dedbd8dacfe5dfa50132c8a747b81289cd24f1db6')],
    'MCMAN.IRX': [FileHash('4f01c1ee5afc457b3a030b7910431f92ba623e3ecc234d5bc5be64f48e38cd3a')],
    'MCSERV.IRX': [FileHash('c55abb687fd08e0df292c3636cfd9a34d06a3261a62a1916d06c463fa30cbae0')],
    'MODHSYN.IRX': [FileHash('f4eb3549f7f2f3b6533444c33ec45fbb57102ad239853c3185dba973467b3ff9')],
    'MODMIDI.IRX': [FileHash('2eed3e9598025d162a2424522a0dbd0ded343468a6d9dc7e50f55b6885d01851')],
    'MSIFRPC.IRX': [FileHash('e91a27ad702873ae11764e1ba44516eeb3138e97fe5a43c713eb197b30ee7387')],
    'NETCNF.IRX': [FileHash('a5df47e49c430573cde4d3dafd09edd19f650581b95a9af7549ce4dda903800a')],
    'PADMAN.IRX': [FileHash('c2200e7f7071b7e747e122bd036f70133efa6b631415f500a806b75d55fcb88b')],
    'SDRDRV.IRX': [FileHash('b75f0020a1e28ece42c8e790c2c48f7daf0773fd4dc4cca7ec72e044049198a3')],
    'SIO2MAN.IRX': [FileHash('f41e341efe08ac407fce467639d10bb2a4532b70a8a5829dbe350edef58516d7')],
    'USBD.IRX': [FileHash('d695af5f72cb766e597abe3b67cd88b4b2579445084488bea1aca6b48a594a22')],
    'SCUS_971.11': [FileHash('7ac6fe3e77e2592c6669dafd5ad66d48cfb10103a7ffff0ec488ee25b4be9129'), FileHash('929694ca9c0cde15a2813f9d3babdc2ff828214e6350044d8fcddefa91fab484', 0x100, 0x1A9A80)],
    'SYSTEM.CNF': [FileHash('dadb5dca5f8d3edffb4c1a1e739fbf68a83342df06cce104121d3749eed80d67')],
    'TITLE.BIN': [FileHash('fbb0219183a763783cfaa1edf6f7781e8df38ed79aea0c06456b22f487ee57da')],
    'DMMY': [FileHash('32000a674a8e37c5345b6bce47c19e4894f8d01f59332aab614e1130a8ba5867')],
}
if region.NAME == region.PAL:
    HASHES = PAL_HASHES

def calculate_sha256(path, offset=0, size=-1):
    with open(path, 'rb') as f:
        f.seek(offset)
        data = f.read(size)
        return hashlib.sha256(data).hexdigest()

# The whole file, as opposed to one of the partial spans in HASHES.
COMPLETE = (0, -1)

# How much of each built file has to match the retail original. Only the
# executable is allowed to differ anywhere: it is checked over its code and
# data, because the retail symbol and string tables are not reproduced yet.
# Anything not listed here -- both overlays -- has to come out byte-identical.
BUILT_SPANS = {
    'SCUS_971.11': (0x100, 0x1A2380),
}
if region.NAME == region.PAL:
    BUILT_SPANS = {'SCUS_971.11': (0x100, 0x1A9A80)}

def validate(path, offset=0, size=-1, log=True):
    partial = (offset, size) != COMPLETE
    log_prefix = f'{path.name}' if not partial else f'{path.name} {COLOR_GREY}(partial){COLOR_END}'

    if not path.exists():
        if log:
            print(f"{log_prefix}: {COLOR_RED}FAILED{COLOR_END} - File doesn't exist!")

        return False

    if not path.name in HASHES.keys():
        raise ValueError(f'No known hash exists for {path.name}')

    known_hashes = list(filter(lambda x: x.offset == offset and x.size == size, HASHES[path.name]))
    if not known_hashes:
        raise ValueError(f'No known exists for {path.name} with offset {offset:x} and size {size:x}')

    hash = calculate_sha256(path, offset, size)
    if known_hashes[0].compare(hash):
        if log:
            print(f'{log_prefix}: {COLOR_GREEN}OK{COLOR_END}')
        return True
    else:
        if log:
            print(f'{log_prefix}: {COLOR_RED}FAILED{COLOR_END}')
        return False

COLOR_YELLOW = '\033[93m'

# The file each image is verified in. main is a span of the executable; the
# overlays are whole files.
IMAGE_FILE = {'main': 'SCUS_971.11', 'title': 'TITLE.BIN', 'dun': 'DUN.BIN'}

# Worst last, so the eye lands on the failures.
ORDER = (PERFECT, FUZZY, UNMATCHED)
LABEL = {PERFECT: 'Perfect', FUZZY: 'Fuzzy', UNMATCHED: 'Unmatched'}


def categorise(section, declared):
    """Sort one image's functions into perfect, fuzzy and unmatched.

    Returns (rows, data_differs). `rows` is one (entry, verdict) per function;
    `data_differs` is the number of bytes outside any function that do not
    match, which is always a failure -- a fuzzy match is a register choice in
    code, and there is no such thing for data.
    """
    import compare_build
    import ref_index

    image = compare_build.load_image(section, compare_build.RETAIL_ELF,
                                     compare_build.BUILD_ELF)
    covered = bytearray(len(image.retail))
    rows = []
    for entry in ref_index.entries(section):
        if not entry.is_function:
            continue
        start = entry.vram - image.vaddr
        end = start + entry.size
        if start < 0 or end > len(image.retail) or end > len(image.build):
            continue
        covered[start:end] = b'\x01' * entry.size
        verdict = classify(image.retail[start:end], image.build[start:end])
        if verdict == FUZZY and entry.symbol not in declared:
            # Undeclared drift looks like a fuzzy match but nothing promised it
            # would stay one, so it is not allowed to pass as one.
            verdict = UNMATCHED
        rows.append((entry, verdict))

    # A build that is not the same length as retail is a failure like any
    # other, and has to be reported rather than crash the count: every byte
    # past the end of the shorter of the two differs by definition.
    shared = min(len(image.retail), len(image.build))
    data_differs = sum(1 for i, c in enumerate(covered[:shared])
                       if not c and image.retail[i] != image.build[i])
    data_differs += abs(len(image.retail) - len(image.build))
    return rows, data_differs


def verify_functions(sections=SECTIONS):
    """Report the share of code that is a perfect, fuzzy or unmatched match."""
    declared = declarations()
    totals = {k: [0, 0] for k in ORDER}
    bad_data = 0
    failures = []

    print('Verifying built files')
    per_image = {}
    for section in sections:
        rows, data_differs = categorise(section, declared)
        bad_data += data_differs
        counts = {k: [0, 0] for k in ORDER}
        for entry, verdict in rows:
            counts[verdict][0] += 1
            counts[verdict][1] += entry.size
            totals[verdict][0] += 1
            totals[verdict][1] += entry.size
            if verdict == UNMATCHED:
                failures.append((section, entry.symbol))
        per_image[section] = (counts, data_differs)

        name = IMAGE_FILE[section]
        if data_differs or counts[UNMATCHED][0]:
            verdict = f'{COLOR_RED}FAILED{COLOR_END}'
        elif counts[FUZZY][0]:
            verdict = f'{COLOR_YELLOW}FUZZY{COLOR_END}'
        else:
            verdict = f'{COLOR_GREEN}OK{COLOR_END}'
        detail = (f'{counts[PERFECT][0]} perfect, {counts[FUZZY][0]} fuzzy, '
                  f'{counts[UNMATCHED][0]} unmatched')
        if data_differs:
            detail += f', {data_differs} data bytes differ'
        print(f'{name}: {verdict} {COLOR_GREY}({detail}){COLOR_END}')

    code = sum(v[1] for v in totals.values()) or 1

    def pct(kind):
        return 100.0 * totals[kind][1] / code

    print('\nCode, by byte')
    for kind, colour in ((PERFECT, COLOR_GREEN), (FUZZY, COLOR_YELLOW),
                         (UNMATCHED, COLOR_RED)):
        print(f'  {colour}{LABEL[kind]}{COLOR_END}{" " * (10 - len(LABEL[kind]))}'
              f' {pct(kind):6.2f}%   {totals[kind][0]:>5} functions')
    print('  Data       ' + (f'{COLOR_RED}{bad_data} bytes differ{COLOR_END}'
                             if bad_data else f'{COLOR_GREEN}perfect{COLOR_END}'))

    for section, symbol in failures[:10]:
        print(f'    {COLOR_RED}unmatched{COLOR_END} {section}/{symbol}')
    if len(failures) > 10:
        print(f'    ... and {len(failures) - 10} more')

    return not failures and not bad_data


def ensure_ok(path):
    if not validate(path):
        sys.exit(1)

def verify_extracted():
    print('Verifying extracted files')
    for path in Path(region.EXTRACTED).rglob('*.*'):
        ensure_ok(path)

def verify_built(files):
    print('Verifying built files')
    paths = [Path(f) for f in files]
    # Check every file before failing, so one mismatch doesn't hide the others.
    results = [validate(p, *BUILT_SPANS.get(p.name, COMPLETE)) for p in paths]
    if not all(results):
        sys.exit(1)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Verify decompiled files')
    parser.add_argument('-f', '--file', dest='files', nargs='+', help='Verify built files against the retail originals')
    parser.add_argument('-e', '--verify_extracted', dest='verify_extracted', action='store_true', help='Verify that the game has been extracted correctly')
    parser.add_argument('-c', '--categorise', action='store_true',
                        help='Report perfect/fuzzy/unmatched shares per function')
    args = parser.parse_args()

    if args.categorise:
        sys.exit(0 if verify_functions() else 1)
    elif args.files:
        verify_built(args.files)
    elif args.verify_extracted:
        verify_extracted()
