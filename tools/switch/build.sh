#!/usr/bin/env bash
# Builds the Nintendo Switch homebrew port (docs/SWITCH.md) in the image tools/switch/Containerfile
# describes, building that image first when this checkout's Containerfile has no image yet.
# podman by default, docker with BUILDER=docker. The arguments go to the build (targets, -v);
# `tools/switch/build.sh shell` opens a shell in the image instead.
set -euo pipefail

root=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
builder=${BUILDER:-}
if [ -z "$builder" ]; then
    if command -v podman > /dev/null; then builder=podman; else builder=docker; fi
fi
[ -f "$root/tools/kazarov/include/kazarov/port.h" ] || git -C "$root" submodule update --init tools/kazarov

# One tag per Containerfile, so an edit to it builds a new image and an old checkout keeps its own.
tag=$(sha256sum "$root/tools/switch/Containerfile" | cut -c1-12)
image=chronicle-switch:$tag
if ! "$builder" image inspect "$image" > /dev/null 2>&1; then
    echo "switch: building $image (the first build takes a while: Mesa, Rust's std, SDL3)" >&2
    "$builder" build -t "$image" -t chronicle-switch:latest -f "$root/tools/switch/Containerfile" "$root/tools/switch"
fi

# The build runs as the caller, so the tree it writes stays theirs.
if [ "$builder" = podman ]; then
    user=(--userns=keep-id)
else
    user=(--user "$(id -u):$(id -g)")
fi
interactive=()
[ -t 0 ] && interactive=(-it)
run=("$builder" run --rm "${interactive[@]}" "${user[@]}" -e HOME=/tmp -v "$root:$root:z" -w "$root" "$image")

if [ "${1:-}" = shell ]; then
    shift
    exec "${run[@]}" bash "$@"
fi
exec "${run[@]}" bash -c 'cmake --preset switch > /dev/null && cmake --build --preset switch "$@"' build "$@"
