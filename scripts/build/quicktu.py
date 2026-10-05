#!/usr/bin/env python3
"""Compile one unit and score one of its functions against retail.

    quicktu.py <source> --nodes                 list the unit's float constants
    quicktu.py <source> <function> [--ones 3,17]  score one function
    quicktu.py <source> --all [--ones 3,17]       score every function of it

Retail's argument-materialisation order is a byte of leaked compiler state, one
decision per float constant, and the only witness is retail's own instruction
stream (re/ai/compiler/leaked_state.md). Searching it needs a compare that costs
a compile rather than a build, so this compiles the unit alone and lines the
function up against retail without linking: immediates are masked, because an
unlinked object has none, and what is left -- the opcodes and the registers --
is exactly what the flag decides.

`--ones` is passed to statefix as an ephemeral node-index override. Persistent
decisions live in `ps2/config/<region>/expression_node_overrides.json` and are keyed by the
exact live MWCC identity rather than by a translation-unit-wide index.
"""

import argparse
import json
import os
import re
import subprocess
import sys
import tempfile

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import region  # noqa: E402
PREFIX = os.environ.get('MIPS_TOOL_PREFIX', 'mips-ps2-decompals-')
FLAGS = ['-O2', '-c', '-Cpp_exceptions', 'off', '-RTTI', 'off',
         '-strings', 'readonly', '-pragma', 'divbyzerocheck on', '-i', 'ps2/include']
if region.NAME == region.PAL:
    FLAGS.extend(['-DPAL', '-DPAL_TIMING'])
OVERLAY_BASE = region.OVERLAY_ORIGIN
IMAGES = {'title': (f'{region.EXTRACTED_ISO}/TITLE.BIN', f'{region.CONFIG}/title.symbols.txt', OVERLAY_BASE),
          'dun': (f'{region.EXTRACTED_ISO}/DUN.BIN', f'{region.CONFIG}/dun.symbols.txt', OVERLAY_BASE),
          'main': (f'{region.EXTRACTED_ISO}/SCUS_971.11', f'{region.CONFIG}/main.symbols.txt', None)}

# An operand that says nothing until the image is linked.
NUMBER = re.compile(r'(?<![$a-z])-?(?:0x)?[0-9a-f]+\b')
LABEL = re.compile(r'^([0-9a-f]+)\s+<(.+)>:$')
BRANCH = re.compile(r'^(b|beql?|bnel?|beqz|bnez|bgtz|blez|bltz|bgez|bltzal'
                    r'|bgezal|bc1t|bc1f|bc1tl|bc1fl|j|jal)\s')
INSN = re.compile(r'^\s*([0-9a-f]+):\s+[0-9a-f]{8}\s+(.*)$')


def shape(text, masked):
    """One instruction, with the operands only the linker knows taken out.

    An unlinked object carries a zero where retail carries the address, so a
    relocated operand cannot be compared; everything else can, and has to be --
    the constant a `lui` builds is exactly what the flag being searched for
    decides the order of, and a comparison that dropped it would call two
    different orders the same.
    """
    # Objdump labels may themselves contain template brackets.
    text = re.sub(r'\s*<.*>', '', text).strip()
    text = text.split('#')[0].strip()
    # A branch names an address too: section-relative here, absolute in the
    # image, and never a relocation, so nothing but the mnemonic survives it.
    if BRANCH.match(text):
        masked = True
    return NUMBER.sub('N', text) if masked else text


def retail_function(image, name):
    path, symbols, base = IMAGES[image]
    address = size = None
    want = re.compile(re.escape(name) +
                      r' = 0x([0-9a-fA-F]+); // type:func(?: \S+)* size:0x([0-9a-fA-F]+)')
    for line in open(os.path.join(REPO, symbols)):
        m = want.match(line)
        if m:
            address, size = int(m.group(1), 16), int(m.group(2), 16)
            break
    if address is None:
        raise SystemExit('%s: not in %s' % (name, symbols))
    # A name the overlay shares with main resolves to main's copy unless the
    # address is checked: the symbol tables list both, and reading an overlay
    # at a main address gives whatever byte happens to be there.
    if (base is not None) != (address >= OVERLAY_BASE):
        raise SystemExit('%s: 0x%08x is not in %s' % (name, address, image))
    args = [PREFIX + 'objdump', '-D', '-m', 'mips:5900', '-EL',
            '--start-address=0x%x' % address,
            '--stop-address=0x%x' % (address + size)]
    if base is not None:
        args += ['-b', 'binary', '--adjust-vma=0x%x' % base]
    text = subprocess.run(args + [os.path.join(REPO, path)],
                          capture_output=True, text=True).stdout
    return [m.group(2) for m in
            (INSN.match(line) for line in text.splitlines()) if m]


def ours(obj, name):
    """[(instruction, relocated)] for one function of the object."""
    text = subprocess.run([PREFIX + 'objdump', '-dr', '-m', 'mips:5900', '-EL', obj],
                          capture_output=True, text=True).stdout
    rows, taking, base = [], False, 0
    for line in text.splitlines():
        head = re.match(r"^Disassembly of section (\S+):", line)
        if head:
            taking = False
            continue
        label = LABEL.match(line.strip())
        if label:
            # Retail spells a template argument the way splat does.
            taking = (label.group(2) == name or
                      label.group(2).replace('<1>', '_1_') == name)
            base = int(label.group(1), 16)
            continue
        if taking:
            m = INSN.match(line)
            if m:
                rows.append([m.group(2), False])
            elif re.match(r'^\s+[0-9a-f]+:\s+R_MIPS', line) and rows:
                rows[-1][1] = True
            elif not line.strip():
                taking = False
    return rows


def index_ones_lines(ones, name='argument_flag_ones'):
    """Format index-list pragmas for the argument-read search tool.

    The compiler hands its pragma handler ninety-six characters of the line at
    a time, so a long list has to be said a few at a time; the pragma is
    cumulative, which is what makes that work.
    """
    lines = ['#pragma %s %s\n' % (name,
                                  ','.join(str(i) for i in ones[at:at + 10]))
             for at in range(0, len(ones), 10)]
    return lines or ['#pragma %s\n' % name]


def compile_unit(source, ones, verify=False, flag=None,
                 argones=(), argflag=None, arghook=False, argfree=()):
    directory, base = os.path.split(source)
    pragma = ''
    if argfree:
        free = sorted(argfree)
        pragma += ''.join(
            '#pragma argument_flag_free %s\n'
            % ','.join(str(i) for i in free[at:at + 10])
            for at in range(0, len(free), 10))
    if argflag is not None or argones or arghook:
        argones = sorted(argones)
        pragma += ''.join(
            '#pragma argument_flag_ones %s\n'
            % ','.join(str(i) for i in argones[at:at + 10])
            for at in range(0, len(argones), 10)) or (
                '#pragma argument_flag_ones\n')
        if argflag is not None:
            pragma = '#pragma argument_flag %d\n' % argflag + pragma
    body = pragma + open(os.path.join(REPO, source)).read()
    handle, temporary = tempfile.mkstemp(suffix='.cpp', prefix='tmpquick_',
                                         dir=os.path.join(REPO, directory))
    os.close(handle)
    obj = temporary[:-4] + '.o'
    try:
        with open(temporary, 'w') as f:
            f.write(body)
        command = [sys.executable, 'scripts/build/statefix.py']
        if verify:
            command.append('--verify')
        command += ['--', '-o', os.path.relpath(obj, REPO)] + FLAGS
        command.append(os.path.relpath(temporary, REPO))
        environment = dict(os.environ, STATEFIX_SOURCE=source)
        if verify:
            environment['STATEFIX_NODE_AUDIT'] = '1'
        if flag is not None:
            environment['STATEFIX_NODE_DEFAULT'] = str(flag)
        if ones:
            environment['STATEFIX_NODE_ONES'] = ','.join(str(i) for i in ones)
        done = subprocess.run(command, cwd=REPO, env=environment,
                              capture_output=True, text=True)
        # MWCC names the unit's static initialiser after the file it compiled,
        # so the copy's name has to be put back for the object to be read under
        # the names the image uses.
        return obj, done.stdout + done.stderr, (
            os.path.basename(temporary)[:-4], base[:-4])
    finally:
        os.unlink(temporary)


def compare(theirs, mine):
    """How many instructions differ, retail's beside ours."""
    bad = 0
    for i in range(max(len(theirs), len(mine))):
        masked = mine[i][1] if i < len(mine) else False
        a = shape(theirs[i], masked) if i < len(theirs) else None
        b = shape(mine[i][0], masked) if i < len(mine) else None
        bad += a != b
    return bad


def unhide(name, stem):
    """An object symbol with the temporary file's name put back."""
    return name.replace(stem[0], stem[1]) if stem else name


def object_functions(obj):
    """[(object symbol, section, size)] for the functions the object defines."""
    text = subprocess.run([PREFIX + 'readelf', '-sW', obj],
                          capture_output=True, text=True).stdout
    found = []
    for line in text.splitlines():
        parts = line.split()
        if len(parts) >= 8 and parts[3] == 'FUNC' and parts[6].isdigit():
            found.append(parts[7])
    return found


def retail_names(source, names):
    """What retail calls each of an object's functions.

    The object's own spelling is what the source wrote; the fixups rename the
    copies retail suffixed, and the unit's span in the linker script settles
    the rest -- the same two steps `literals.py` takes.
    """
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import literals
    fixups = json.load(open(os.path.join(REPO, region.CONFIG, 'object_fixups.json')))
    renames = fixups.get(source, {}).get('symbols', {})
    retail = literals.Retail(REPO)
    unit = os.path.basename(source) + '.o'
    out = {}
    for name in names:
        want = renames.get(name, name)
        out[name] = retail.local_name(unit, want)
    return out


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('source')
    parser.add_argument('function', nargs='?')
    parser.add_argument('--image', default='title')
    parser.add_argument('--ones', default='')
    parser.add_argument('--nodes', action='store_true')
    parser.add_argument('--expression-nodes', action='store_true',
                        help='list exact live identities used by the override config')
    parser.add_argument('--all', action='store_true')
    parser.add_argument('--argones', default='')
    parser.add_argument('--argflag', type=int, default=None)
    parser.add_argument('--argfree', default='',
                        help='reads to leave to the expression node override')
    parser.add_argument('--argnodes', action='store_true')
    parser.add_argument('--diff', action='store_true',
                        help="retail's instructions beside ours, before linking")
    parser.add_argument('--flag', type=int, default=None,
                        help='the standing value every other constant reads')
    args = parser.parse_args()

    ones = [int(i, 0) for i in args.ones.replace(',', ' ').split()]
    obj, log, stem = compile_unit(
        args.source, ones,
        verify=args.nodes or args.expression_nodes or args.argnodes,
        flag=args.flag,
        argones=[int(i, 0) for i in args.argones.replace(',', ' ').split()],
        argfree=[int(i, 0) for i in args.argfree.replace(',', ' ').split()],
        argflag=args.argflag if args.argflag is not None
        else (0 if args.argnodes else None))
    try:
        if args.nodes or args.expression_nodes or args.argnodes:
            want = ('statefix: argument' if args.argnodes else
                    'statefix: expression-node' if args.expression_nodes else
                    'statefix: node')
            for line in log.splitlines():
                if want in line:
                    print(line.split('statefix: ', 1)[1])
            return
        if args.all:
            names = retail_names(args.source, [unhide(n, stem)
                                               for n in object_functions(obj)])
            total = 0
            for symbol, name in sorted(names.items()):
                symbol = symbol.replace(stem[1], stem[0])
                # Retail spells a template argument the way splat does.
                name = name.replace('<1>', '_1_')
                try:
                    theirs = retail_function(args.image, name)
                except SystemExit:
                    continue
                mine = ours(obj, symbol)
                if not mine:
                    continue
                bad = compare(theirs, mine)
                total += bad
                if bad:
                    print('%6d  %s' % (bad, name))
            print('total %d' % total)
            return
        theirs = retail_function(args.image, args.function)
        # A unit whose retail name our compiler cannot spell is renamed on the
        # way out; score it under the name the object actually carries.
        fixups = json.load(open(os.path.join(REPO, region.CONFIG, 'object_fixups.json')))
        renames = fixups.get(args.source, {}).get('symbols', {})
        compiled = next((ours_name for ours_name, retail in renames.items()
                         if retail == args.function), args.function)
        mine = ours(obj, compiled.replace(stem[1], stem[0]))
        if not mine:
            raise SystemExit('%s: not in the object -- %s' % (args.function, log[-400:]))
        if args.diff:
            for i in range(max(len(theirs), len(mine))):
                masked = mine[i][1] if i < len(mine) else False
                a = shape(theirs[i], masked) if i < len(theirs) else ''
                b = shape(mine[i][0], masked) if i < len(mine) else ''
                print('%-44s %s%s' % (a, b, '' if a == b else '   <--'))
        bad = compare(theirs, mine)
        print('%s %d/%d%s' % (args.function, bad, len(theirs),
                              '' if len(theirs) == len(mine) else
                              ' LENGTH %d' % len(mine)))
    finally:
        if os.path.exists(obj):
            os.unlink(obj)


if __name__ == '__main__':
    main()
