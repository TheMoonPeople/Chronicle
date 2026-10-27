#!/usr/bin/env bash
# Builds the Nintendo Switch homebrew port (docs/SWITCH.md) in port/switch/Containerfile
# describes, building that image first when this checkout's Containerfile has no image yet.
# Docker by default; BUILDER can select another OCI-compatible runtime. The arguments go to the
# build (targets, -v); `port/switch/build.sh shell` opens a shell in the image instead, and
# `port/switch/build.sh image` prints the image's name.
set -euo pipefail

root=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
builder=${BUILDER:-docker}
[ -f "$root/tools/kazarov/include/kazarov/port.h" ] || git -C "$root" submodule update --init tools/kazarov

preset=${SWITCH_PRESET:-switch}
containerfile=$root/port/switch/Containerfile
name=chronicle-switch
if [ "$preset" != switch ]; then
    echo "unsupported Switch preset: $preset" >&2
    exit 2
fi

# Include every patch in the tag so editing one rebuilds the image.
tag=$({
    cat "$containerfile" "$root"/port/switch/build-mesa.sh \
        "$root"/port/switch/bindgen-switch-wrapper.sh \
        "$root"/port/switch/rustc-switch-wrapper.sh \
        "$root"/port/switch/bindgen-atomic-shim.h \
        "$root"/port/switch/switch_cross_file.txt \
        "$root"/port/switch/switch_native_file.txt \
        "$root"/port/switch/patches/*.patch
} | sha256sum | cut -c1-12)
image=$name:$tag
# `port/switch/build.sh image` names the image this checkout builds in, for a cache to key on.
if [ "${1:-}" = image ]; then
    echo "$image"
    exit 0
fi
if ! "$builder" image inspect "$image" > /dev/null 2>&1; then
    echo "switch: building $image (the first build takes a while)" >&2
    "$builder" build -t "$image" -t "$name:latest" -f "$containerfile" "$root/port/switch"
fi

# The build runs as the caller, so the tree it writes stays theirs.
if [ "$builder" = podman ]; then
    user=(--userns=keep-id)
else
    user=(--user "$(id -u):$(id -g)")
fi
interactive=()
[ -t 0 ] && interactive=(-it)
run=("$builder" run --rm "${interactive[@]}" "${user[@]}" -e HOME=/tmp -e "PRESET=$preset" -e "CLEAN=${CLEAN:-}" -e "JOBS=${JOBS:-}" -v "$root:/work:z" -w /work "$image")

if [ "${1:-}" = shell ]; then
    shift
    exec "${run[@]}" bash "$@"
fi
"${run[@]}" bash -c '
set -euo pipefail
if [ -n "$CLEAN" ] && [ "$CLEAN" != 0 ]; then
    rm -rf "port/build/$PRESET"
fi
cmake --preset "$PRESET" > /dev/null
build_args=()
if [ -n "$JOBS" ]; then
    build_args+=(--parallel "$JOBS")
fi
exec cmake --build --preset "$PRESET" "${build_args[@]}" "$@"
' build "$@"

echo "switch: NVK compiles shaders at startup and saves a cache for later launches" >&2
