#!/usr/bin/env python3
"""Writes port/flatpak/cargo-sources.json from tools/kazarov/Cargo.lock.

flatpak-builder downloads every crate the lock names before its network-free
build, unpacks each into cargo/vendor and points cargo at that directory.
Run it again whenever Kazarov's Cargo.lock changes.
"""

import json
import sys
import tomllib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
LOCK = ROOT / 'tools' / 'kazarov' / 'Cargo.lock'
OUT = ROOT / 'port' / 'flatpak' / 'cargo-sources.json'
REGISTRY = 'registry+https://github.com/rust-lang/crates.io-index'
CONFIG = '''[source.crates-io]
replace-with = "vendored-sources"

[source.vendored-sources]
directory = "cargo/vendor"
'''


def main():
    sources = []
    for package in tomllib.loads(LOCK.read_text())['package']:
        source = package.get('source')
        if source is None:
            continue
        if source != REGISTRY:
            sys.exit(f"cargo_sources.py: {package['name']} comes from {source}, not crates.io")
        name, version, checksum = package['name'], package['version'], package['checksum']
        dest = f'cargo/vendor/{name}-{version}'
        sources.append({
            'type': 'archive',
            'archive-type': 'tar-gzip',
            'url': f'https://static.crates.io/crates/{name}/{name}-{version}.crate',
            'sha256': checksum,
            'dest': dest,
        })
        sources.append({
            'type': 'inline',
            'contents': json.dumps({'package': checksum, 'files': {}}),
            'dest': dest,
            'dest-filename': '.cargo-checksum.json',
        })
    sources.append({'type': 'inline', 'contents': CONFIG, 'dest': 'cargo', 'dest-filename': 'config.toml'})
    OUT.write_text(json.dumps(sources, indent=2) + '\n')
    print(f'cargo_sources.py: {len(sources) // 2} crates into {OUT.relative_to(ROOT)}')


if __name__ == '__main__':
    main()
