#!/usr/bin/env bash
# Builds a Chronicle configuration through Kazarov. Run it from anywhere.
set -euo pipefail

root=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
if [ "${1:-}" = switch ]; then
    shift
    exec "$root/port/switch/build.sh" "$@"
fi
[ -f "$root/tools/kazarov/scripts/kazarov" ] || git -C "$root" submodule update --init tools/kazarov
exec "$root/tools/kazarov/scripts/kazarov" build "$@"
