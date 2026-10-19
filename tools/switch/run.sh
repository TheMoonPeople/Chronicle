#!/usr/bin/env bash
# Sends a build to the Switch and prints what the game logs until it exits (docs/SWITCH.md, "Testing
# from a computer"): to the test runner (tools/switch/runner), which starts it and comes back when
# it exits, or to the Homebrew Menu's netloader (Y). SWITCH_ADDRESS is the Switch's IPv4 address
# (nxlink broadcasts for it without one); the image is tools/switch/build.sh's.
#   tools/switch/run.sh [--nro FILE] [--path PATH] [-- game arguments...]
set -euo pipefail

root=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
builder=${BUILDER:-}
if [ -z "$builder" ]; then
    if command -v podman > /dev/null; then builder=podman; else builder=docker; fi
fi
nro=$root/port/build/switch/darkcloud.nro
path=chronicle/darkcloud.nro
while [ $# -gt 0 ]; do
    case $1 in
        --nro) nro=$(realpath "$2"); shift 2 ;;
        --path) path=$2; shift 2 ;;
        --) shift; break ;;
        *) echo "usage: run.sh [--nro FILE] [--path PATH] [-- game arguments...]" >&2; exit 2 ;;
    esac
done
address=()
[ -n "${SWITCH_ADDRESS:-}" ] && address=(-a "$SWITCH_ADDRESS")
user=(--user "$(id -u):$(id -g)")
[ "$builder" = podman ] && user=(--userns=keep-id)
dir=$(dirname "$nro")
# The host's network, for nxlink's broadcast and for the game's connection back to its log server.
exec "$builder" run --rm --network host "${user[@]}" -v "$dir:$dir:z" chronicle-switch:latest \
    nxlink "${address[@]}" -r 5 -p "$path" -s "$nro" --args "$*"
