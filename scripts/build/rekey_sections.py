#!/usr/bin/env python3
"""Re-key a unit's `.data` section fixups after a source edit.

    rekey_sections.py <source> [<object>]

MWCC numbers an invented name from a counter the whole translation unit shares,
so an edit that allocates one more front-end node renames every constant after
it. `ps2/config/<region>/object_fixups.json` names the constants that start each run the
linker script places, and those names go stale; the build then stops with
"a numbered constant in `sections` is not exported".

The runs themselves do not move -- a unit emits its generated data in source
order -- so the names are read back off the object rather than kept by hand.
Empty every `sections` entry the config marks as re-keyable, build the object,
and run this: it matches the object's `.data` sections against the sizes the
config records and writes the new names in.

`rekey_sections_skip` can omit an initial run that remains in ordinary `.data`.
Its entries count symbols, including named symbols, rather than bytes.

    ./build.sh ps2-pal objdiff        # with the entries emptied
    scripts/build/rekey_sections.py ps2/src/editloop3.cpp
    ./build.sh ps2-pal objdiff        # with the names filled in

A re-keyable entry is one whose name matches `rekey_sections` in the unit's
fixups: a list of `[section, [size, ...]]` pairs in emission order.
"""

import json
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import region  # noqa: E402
PREFIX = os.environ.get('MIPS_TOOL_PREFIX', 'mips-ps2-decompals-')
CONFIG = os.path.join(ROOT, region.CONFIG, 'object_fixups.json')


def data_symbols(obj):
    """[(name, size)] for the object's generated `.data` sections, in order."""
    sections = {}
    text = subprocess.run([PREFIX + 'readelf', '-SW', obj],
                          capture_output=True, text=True).stdout
    for line in text.splitlines():
        m = re.match(r'\s*\[\s*(\d+)\]\s+(\S+)\s+\S+\s+[0-9a-f]+\s+[0-9a-f]+\s+([0-9a-f]+)',
                     line)
        if m:
            sections[int(m.group(1))] = m.group(2)
    rows = []
    text = subprocess.run([PREFIX + 'readelf', '-sW', obj],
                          capture_output=True, text=True).stdout
    for line in text.splitlines():
        parts = line.split()
        if len(parts) >= 8 and parts[3] == 'OBJECT' and parts[6].isdigit():
            index = int(parts[6])
            if sections.get(index) == '.data':
                rows.append((index, parts[7], int(parts[2])))
    rows.sort()
    return [(name, size) for _index, name, size in rows]


def main():
    source = sys.argv[1]
    obj = sys.argv[2] if len(sys.argv) > 2 else os.path.join(
        ROOT, region.BUILD, source + '.o')
    config = json.loads(open(CONFIG, encoding='utf-8').read())
    groups = config.get(source, {}).get('rekey_sections')
    if not groups:
        raise SystemExit('rekey_sections: %s records no re-keyable runs' % source)

    have = data_symbols(obj)
    # Some units keep an initial run in ordinary .data. These symbols are
    # already placed by the linker script and must be skipped when re-keying
    # the later named sections.
    skip = config[source].get('rekey_sections_skip', 0)
    have = have[skip:]
    wanted = [size for _name, sizes in groups for size in sizes]
    if [size for _n, size in have] != wanted:
        raise SystemExit(
            'rekey_sections: the object holds %s, the config expects %s;\n'
            '                empty the sections entries and rebuild first, or\n'
            '                the source has gained or lost a template.'
            % ([s for _n, s in have], wanted))

    names = iter(name for name, _size in have)
    text = open(CONFIG, encoding='utf-8').read()
    for section, sizes in groups:
        members = [next(names) for _ in sizes]
        body = ',\n'.join('        "%s"' % m for m in members)
        pattern = re.compile(r'("%s": \[)[^\]]*(\])' % re.escape(section))
        text, count = pattern.subn(
            lambda m: m.group(1) + '\n' + body + '\n      ' + m.group(2),
            text, count=1)
        if not count:
            raise SystemExit('rekey_sections: %s is not in %s' % (section, CONFIG))
    json.loads(text)
    open(CONFIG, 'w', encoding='utf-8').write(text)
    print('rekey_sections: re-keyed %d runs of %s' % (len(groups), source))


if __name__ == '__main__':
    main()
