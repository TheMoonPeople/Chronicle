#!/usr/bin/env bash
# Build the PC port as a Flatpak (docs/FLATPAK.md) from the working tree, on
# the host: flatpak-builder brings its own sandbox, so the dev container is
# not involved. The result is chronicle.flatpak in the repository root.
#
#   port/flatpak.sh              build chronicle.flatpak
#   INSTALL=1 port/flatpak.sh    also install it for this user
#   CLEAN=1 port/flatpak.sh      throw flatpak-builder's cache away first, so
#                                every module is built again
#
# The runtime, the SDK and its llvm22 extension are installed from Flathub
# for this user when they are missing. build-dir/, repo/ and .flatpak-builder/
# in the repository root hold the build between runs.
set -euo pipefail

cd "$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

APP_ID=org.themoonpeople.Chronicle
MANIFEST=port/flatpak/$APP_ID.yml
BUNDLE=chronicle.flatpak

usage() {
    echo "Usage: port/flatpak.sh"
    echo "Set INSTALL=1 to install the bundle, CLEAN=1 to rebuild every module."
}

case "${1-}" in
    "") ;;
    -h|--help) usage; exit 0 ;;
    *) usage >&2; exit 2 ;;
esac

for tool in flatpak flatpak-builder; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "$tool is required and was not found on PATH." >&2
        exit 1
    fi
done

if [ "${CLEAN:-0}" = 1 ]; then
    echo "CLEAN=1: discarding build-dir, repo and .flatpak-builder."
    rm -rf build-dir repo .flatpak-builder
fi

flatpak remote-add --user --if-not-exists flathub https://dl.flathub.org/repo/flathub.flatpakrepo

flatpak-builder --user --force-clean --install-deps-from=flathub \
    --repo=repo build-dir "$MANIFEST"
flatpak build-bundle repo "$BUNDLE" "$APP_ID"
echo "Built $BUNDLE"

if [ "${INSTALL:-0}" = 1 ]; then
    flatpak install --user --noninteractive --reinstall "$BUNDLE"
    echo "Installed; start it with: flatpak run $APP_ID"
fi
