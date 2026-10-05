#!/bin/sh
# Run clang-tidy over the game's sources with the project's own checks loaded.
#
#   scripts/lint/tidy.sh [--fix] [file.cpp ...]
#
# With no files, every unit under ps2/src is checked. The plugin's own source is
# checked as well, against tools/tidy-module/.clang-tidy. Each unit is checked twice,
# as NTSC and as PAL (-DPAL), since both releases compile from the same
# sources. The dcdecomp-* checks are a clang-tidy plugin built from
# tools/tidy-module against the installed clang (llvm-config and the
# clang-tidy headers are needed); it is rebuilt when its source or the
# installed LLVM changes. With --fix, the fixes of every run are merged and
# applied once all the runs have finished.
set -eu
cd "$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"

plugin=build/clang-tidy/libdcdecomp-tidy.so
source=tools/tidy-module/dcdecomp_tidy.cpp
stamp=build/clang-tidy/llvm-version
# A plugin built against another LLVM does not load, so a clang upgrade rebuilds it too.
if [ ! -f "$plugin" ] || [ "$source" -nt "$plugin" ] || [ "$(cat "$stamp" 2>/dev/null)" != "$(llvm-config --version)" ]; then
    mkdir -p build/clang-tidy
    # shellcheck disable=SC2046
    clang++ -shared -fPIC $(llvm-config --cxxflags) -fno-rtti -O2 "$source" -o "$plugin"
    llvm-config --version > "$stamp"
fi
# clang-tidy only warns on stderr when a plugin fails to load, and then runs
# without its checks, which would read as a clean report.
if [ "$(clang-tidy --load="$plugin" --list-checks -checks='-*,dcdecomp-*' 2>/dev/null | grep -c dcdecomp-)" -ne 3 ]; then
    echo "tidy.sh: $plugin does not load into clang-tidy" >&2
    exit 1
fi

# The flags .clangd gives the editor: C++98 for a 32-bit MIPS target, with the
# SDK and standard headers from ps2/include/ rather than the host's.
flags="-xc++ -std=c++98 -Wno-deprecated-writable-strings -nostdinc -nostdinc++ --target=mipsel-unknown-elf -Ips2/include -Ips2/include/std -Ips2/include/sce"

out=$(mktemp -d)
trap 'rm -rf "$out"' EXIT

# Units that share a header, and the two releases of one unit, would write the
# same files at once and lose fixes. Each run exports its fixes instead and
# clang-apply-replacements merges them once every run has finished; without
# it, fixing runs one unit at a time.
fix=""
merge=""
jobs=$(nproc)
if [ "${1:-}" = "--fix" ]; then
    if command -v clang-apply-replacements > /dev/null; then
        merge="$out/fixes"
        mkdir "$merge"
    else
        fix="--fix"
        jobs=1
    fi
    shift
fi
if [ $# -eq 0 ]; then
    set -- $(find ps2/src -name '*.cpp' ! -name 'tmp*' | sort)
fi

for f in "$@"; do
    printf '%s\n%s\n' "NTSC $f" "PAL $f"
done | xargs -P "$jobs" -n 2 sh -c '
    name="$0_$(echo "$1" | tr / _)"
    extra=""
    [ "$0" = PAL ] && extra="-DPAL -DPAL_TIMING"
    fix='"$fix"'
    [ -n "'"$merge"'" ] && fix="-export-fixes='"$merge"'/$name.yaml"
    clang-tidy --quiet --load='"$plugin"' $fix "$1" -- '"$flags"' $extra > "'"$out"'/$name.log" 2>/dev/null || true
'
if [ -n "$merge" ]; then
    # clang-tidy also exports the compiler's own fix-its for the MWCC syntax clang
    # cannot parse (inline asm functions, register variables); only the checks'
    # fixes are applied.
    for yaml in "$merge"/*.yaml; do
        [ -f "$yaml" ] || continue
        awk '/^  - DiagnosticName:/ { skip = ($3 ~ /^clang-diagnostic-/) } /^[^ ]/ { skip = 0 } !skip' "$yaml" > "$yaml.tmp"
        mv "$yaml.tmp" "$yaml"
    done
    clang-apply-replacements -format -style=file "$merge"
fi
clang-tidy --quiet "$source" -- $(llvm-config --cxxflags) > "$out/plugin.log" 2>/dev/null || true

# Each finding is its warning plus the notes that follow it. Compiler errors and
# their notes are dropped, and a finding both releases report is shown once.
cat "$out"/*.log | grep -E "^[^ ].*: (warning|error|note): " | sed "s|^$PWD/||" | awk '
    / warning: .*\[[a-z0-9-]+\]$/ { if (block != "") print block; block = $0; keep = 1; next }
    / (warning|error): / { if (block != "") print block; block = ""; keep = 0; next }
    keep { block = block "\001" $0 }
    END { if (block != "") print block }
' | sort -u | tr '\001' '\n'
