#!/bin/sh
# Prepare an object for MWLD, in a single objcopy pass.
#
# 1. Strips zero-sized .text/.data/.bss. GNU-lineage assemblers always emit
#    those section headers even when a source file never puts anything in
#    them, and MWLD errors on any zero-sized input section.
# 2. Localizes the `.L…` branch labels. A label whose address is taken has to
#    stay global so a reference from another function can still resolve, but
#    that leaves each one a global text symbol -- and objdiff counts every
#    global text symbol in an object as a function, so `main`'s reference
#    object came out as 26 functions rather than one and scored accordingly.
#    Localizing keeps the symbol, and its address, without it being mistaken
#    for a function. The linked section dumps carry no such label, so this only
#    ever touches the per-function objects objdiff compares against.
# 3. Renames the templated symbols back to the spelling MWCC emits, but only
#    for the per-function objects under ps2/asm/<region>/{non,}matchings --
#    the ones objdiff compares against, which nothing links. objdiff pairs a target with a base
#    by symbol name, and `__ct__14CDataAlloc2_1_Fv` in the dump never matched
#    the `__ct__14CDataAlloc2<1>Fv` the compiler produces. The linked objects
#    keep the sanitised name, which is what the rest of the build uses.
# 4. Applies any extra objcopy arguments given after the object. Two callers
#    use that: an overlay's objects get their sections renamed onto the
#    overlay's private prefix (see tools/kazarov/cmake/KazarovOverlays.cmake), and the library
#    translation units get .text dropped to 4-byte alignment.
#
#    The alignment matters because MWCC gives every function's .text 16-byte
#    alignment, and a unit compiled as one object has one such section per
#    function. The game's own code is 16-aligned in retail too, so that is
#    exactly right for it. The libraries are not -- 305 of their 739 functions
#    start 8 bytes off a 16-byte boundary -- and padding each one up grew the
#    image by 0xBA0 and moved everything after it.
#
# This runs once per object -- roughly four thousand times for a full build --
# so it stays at two processes: one readelf, one objcopy.

# The binutils-mips-ps2-decompals tools. Override MIPS_TOOL_PREFIX in the
# environment for a differently-named build of them.
: "${MIPS_TOOL_PREFIX:=mips-ps2-decompals-}"

obj="$1"
shift

# Both of the objdiff fixups below apply to the per-function reference objects
# only. Nothing links those. The compiled objects must keep their `.L…` labels
# global and their names sanitised: a transplanted function can branch to a
# label another object defines -- 37 of them do -- and localizing the
# definition breaks the link.
refonly=""
case "$obj" in
  */ps2/asm/*/nonmatchings/*|*/ps2/asm/*/matchings/*)
    refonly="-w --localize-symbol=.L*"
    # The build directory the object sits in: <build>/ps2/asm/<region>/...
    aliases="${obj%%/ps2/asm/*}/symbol_aliases.txt"
    [ -f "$aliases" ] &&
        refonly="$refonly --redefine-syms=$aliases"
    ;;
esac

remove=$("${MIPS_TOOL_PREFIX}readelf" -SW "$obj" | awk '
  { sub(/^ *\[[ 0-9]+\] +/, "") }
  ($1 == ".text" || $1 == ".data" || $1 == ".bss") && $5 ~ /^0+$/ {
      printf "--remove-section=%s ", $1
  }')

# 5. Restores the functions MWCC marked for the linker to keep one copy of.
#    Their symbols are global in what the compiler wrote, but it places them
#    among the locals, and every objcopy pass rebuilds the table from sh_info
#    and demotes them. A local one is a second definition MWLD lays out in
#    full, which costs the image a copy of every constructor the compiler
#    wrote itself. postprocess_object.py leaves the names beside the object.
coal=""
[ -s "$obj.coal" ] && coal="--globalize-symbols=$obj.coal"

if [ -n "$remove" ] || [ -n "$refonly" ] || [ -n "$coal" ] || [ "$#" -gt 0 ]; then
  "${MIPS_TOOL_PREFIX}objcopy" -I elf32-littlemips -O elf32-littlemips \
    $refonly $remove $coal "$@" "$obj"
fi
