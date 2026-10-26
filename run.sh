#!/usr/bin/env bash
# Builds and runs a Chronicle configuration through Kazarov. Run it from anywhere.
set -euo pipefail

root=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
# The launch's lock and extraction helpers run on the host.
command -v python3 > /dev/null || { echo "run.sh: Python 3 is required; install python3" >&2; exit 1; }
[ -f "$root/tools/kazarov/scripts/kazarov" ] || git -C "$root" submodule update --init tools/kazarov
exec "$root/tools/kazarov/scripts/kazarov" run "$@"
