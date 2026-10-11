#!/bin/sh
# Stands in for wibo, so the compiler runs with its pragmas honoured.
#
# tools/kazarov's kz-mwcc-exec runs this in place of wibo, with the compiler and
# its arguments exactly as wibo would be given them. The repository is found
# from this file's physical path, so the runner works from any directory.
#
# On by default. A unit states the compiler state it is compiled under with the
# pragmas scripts/build/statefix.py adds; a source carrying them still compiles
# without this shim, because mwcc ignores a pragma it does not know. It just
# does not get the state.
#
#     STATEFIX=0 ./build.sh ps2-pal    plain wibo, no pragmas
set -e
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)
if [ "${STATEFIX:-1}" != "1" ]; then
    exec "${WIBO:-wibo}" "$@"
fi
exec python3 "$here/statefix.py" --wibo -- "$@"
