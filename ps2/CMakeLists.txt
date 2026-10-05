# The PS2 build: compiles ps2/src with MWCC, links the retail images and
# verifies them byte for byte.
#
# The default target builds and verifies the executable.

set(BASENAME SCUS_971.11)

set(SECTIONS main title dun)

# The release the build is for: NTSC 1.02 or the July 12, 2001 PAL prototype.
# Its disc, split configuration, linker script and split each live in a
# directory named for it; scripts/build/region.py is the one place that says
# where, and every script run here is told the region through DCDECOMP_REGION.
set(REGION NTSC CACHE STRING "Release to build: NTSC or PAL")
set_property(CACHE REGION PROPERTY STRINGS NTSC PAL)
if(NOT REGION MATCHES "^(NTSC|PAL)$")
    message(FATAL_ERROR "REGION must be NTSC or PAL, not ${REGION}")
endif()
string(TOLOWER ${REGION} REGION_DIR)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
include(ObjectLists)
include(Overlays)

set(SRC_DIR ps2/src)
set(INCLUDE_DIR ps2/include)
set(TOOLS_DIR tools)
set(SCRIPTS_DIR scripts)
set(ROM_DIR rom)
set(ROM_REGION_DIR ${ROM_DIR}/${REGION_DIR})
set(ASM_DIR ps2/asm/${REGION_DIR})
set(ASM_DATA_DIR ${ASM_DIR}/data)
set(CONFIG_DIR ps2/config/${REGION_DIR})
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/${ROM_DIR}/Dark Cloud (${REGION}).iso"
   AND NOT EXISTS "${CMAKE_SOURCE_DIR}/${ROM_REGION_DIR}/extracted/iso/${BASENAME}")
    message(FATAL_ERROR
        "Nothing to split ${ASM_DIR}/ from. Place the ${REGION} disc image at "
        "rom/Dark Cloud (${REGION}).iso, or copy the extracted binaries in from "
        "the private repository with scripts/host/overlay_private.sh.")
endif()
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/${CONFIG_DIR}/main.yaml")
    message(FATAL_ERROR
        "Nothing to split ${ASM_DIR}/ with: ${CONFIG_DIR}/ holds no split "
        "configuration for ${REGION}.")
endif()
set(REF_INDEX ${CONFIG_DIR}/main.symbols.txt)
if(EXISTS ${CMAKE_SOURCE_DIR}/${REF_INDEX})
    set(REF_STAMP ${CMAKE_SOURCE_DIR}/${REF_INDEX})
else()
    set(REF_STAMP "")
endif()

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/${SCRIPTS_DIR}/build/disassemble.py
    ${CMAKE_SOURCE_DIR}/${SCRIPTS_DIR}/build/region.py
    ${CMAKE_SOURCE_DIR}/${CONFIG_DIR}/main.yaml
    ${CMAKE_SOURCE_DIR}/${CONFIG_DIR}/title.yaml
    ${CMAKE_SOURCE_DIR}/${CONFIG_DIR}/dun.yaml)

file(RELATIVE_PATH BUILD_DIR ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR})

set(MIPS_TOOL_PREFIX mips-ps2-decompals-
    CACHE STRING "Prefix of the binutils-mips-ps2-decompals tools")

set(MW ${TOOLS_DIR}/compilers/mw/2.3.3/)
# MWCIncludes supplies the compiler's system-header search path.
set(STD_INCLUDE_DIR ${INCLUDE_DIR}/std)
set(SCE_INCLUDE_DIR ${INCLUDE_DIR}/sce)
set(LIB_INCLUDE_DIRS "${STD_INCLUDE_DIR}\;${SCE_INCLUDE_DIR}")
set(CC_MW ${CMAKE_COMMAND} -E env MWCIncludes=${LIB_INCLUDE_DIRS} wibo ${MW}mwccmips.exe)
set(LD_MW wibo ${MW}mwldmips.exe)
set(AS ${MIPS_TOOL_PREFIX}as)
set(OBJCOPY ${MIPS_TOOL_PREFIX}objcopy)
set(OBJDUMP ${MIPS_TOOL_PREFIX}objdump)
find_program(PYTHON NAMES python3 python REQUIRED HINTS $ENV{VIRTUAL_ENV}/bin)

set(PYTHON_CMD ${CMAKE_COMMAND} -E env DCDECOMP_REGION=${REGION} ${PYTHON} -u)

execute_process(
    COMMAND ${PYTHON_CMD} ${SCRIPTS_DIR}/build/region.py overlay_origin
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE OVERLAY_ORIGIN
    RESULT_VARIABLE region_rc
    OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT region_rc EQUAL 0)
    message(FATAL_ERROR "scripts/build/region.py overlay_origin failed (${region_rc})")
endif()

set(LD_TEMPLATE ${CONFIG_DIR}/${BASENAME}.lcf)
set(OUTPUT ${BUILD_DIR}/${BASENAME})
set(RETAIL_ISO "${ROM_DIR}/Dark Cloud (${REGION}).iso")
set(BUILT_ISO "${BUILD_DIR}/Dark Cloud (${REGION} Build).iso")
set(CTX ${BUILD_DIR}/ctx.c)
set(CTX_CPP ${BUILD_DIR}/ctx.cpp)

set(AS_FLAGS -EL -march=r5900 -mabi=eabi -g -mno-pdr -non_shared -G0 -I ps2/include)
set(CC_MW_FLAGS -O2 -c -Cpp_exceptions off -RTTI off -strings readonly
    -pragma "divbyzerocheck on" -i ps2/include)
if(REGION STREQUAL "PAL")
    list(APPEND AS_FLAGS --defsym PAL=1)
    list(APPEND CC_MW_FLAGS -DPAL -DPAL_TIMING)
endif()
set(LD_MW_FLAGS -map -nostdlib -m _start -nodead -g)
set(LD_MW_ISO_FLAGS -nostdlib -m _start -nodead -sym on,noelf)
set(BIN_FLAGS -B mips:5900 -I binary -O elf32-littlemips)

# Decompiled globals replace these retail section dumps.
set(REPLACED_SECTIONS main.bss main.data main.rdata main.sbss main.sdata
                      main.vutext main.vudata
                      title.data title.bss dun.data dun.sinit dun.bss)

set(ASM_SOURCES "")
foreach(section IN LISTS SECTIONS)
    read_index_sources(${section} section_srcs "${REPLACED_SECTIONS}")
    list(APPEND ASM_SOURCES ${section_srcs})
endforeach()

# scripts/build/globs.sh rewrites globbed.txt when a globbed file is added or
# removed; CONFIGURE_DEPENDS would re-check the globs on every build instead.
if(EXISTS ${CMAKE_BINARY_DIR}/globbed.txt)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        ${CMAKE_BINARY_DIR}/globbed.txt)
endif()
file(GLOB_RECURSE ALL_CPP_SOURCES
     RELATIVE ${CMAKE_SOURCE_DIR}
     ${CMAKE_SOURCE_DIR}/${SRC_DIR}/*.cpp ${CMAKE_SOURCE_DIR}/${SRC_DIR}/*.c)
# tools/mwccgap compiles a temporary beside the source it came from, so a glob
# that runs while another unit is compiling sees a file that is gone by the
# time anything tries to build it.
list(FILTER ALL_CPP_SOURCES EXCLUDE REGEX "(^|/)tmp[^/]*$")

# Classify units as compiled or assembly-only.
derive_config(--list-units unit_rows)

set(CPP_SOURCES "")
set(UNIT_ASM_SOURCES "")
set(WHOLE_UNIT_OBJS "")
set(WHOLE_UNIT_REFS "")
foreach(row IN LISTS unit_rows)
    string(REPLACE "\t" ";" parts "${row}")
    list(GET parts 0 kind)
    list(GET parts 2 source)
    list(GET parts 3 reference)
    list(APPEND UNIT_ASM_SOURCES ${reference})
    if(kind STREQUAL "asm")
        list(APPEND WHOLE_UNIT_OBJS ${BUILD_DIR}/${source}.o)
        # A hand-written source is assembled in place of the split assembly.
        if(source MATCHES "\\.s$")
            list(APPEND WHOLE_UNIT_REFS ${source})
        else()
            list(APPEND WHOLE_UNIT_REFS ${reference})
        endif()
    else()
        list(APPEND CPP_SOURCES ${source})
    endif()
endforeach()

set(CPP_OBJS "")
foreach(src IN LISTS CPP_SOURCES)
    list(APPEND CPP_OBJS ${CMAKE_SOURCE_DIR}/${BUILD_DIR}/${src}.o)
endforeach()

# Custom commands resolve a relative dependency against this directory, so the
# sources they depend on are named absolutely.
set(ABS_CPP_SOURCES "")
foreach(src IN LISTS CPP_SOURCES)
    list(APPEND ABS_CPP_SOURCES ${CMAKE_SOURCE_DIR}/${src})
endforeach()

# Checked-in splat configuration remains an input, not a generated output.
set(REF_CONFIGS "")
foreach(section IN LISTS SECTIONS)
    list(APPEND REF_CONFIGS ${CMAKE_SOURCE_DIR}/${CONFIG_DIR}/${section}.symbols.txt)
    list(APPEND REF_CONFIGS ${CMAKE_SOURCE_DIR}/${CONFIG_DIR}/${section}.yaml)
endforeach()

set(ALL_OBJS "")
foreach(src IN LISTS ASM_SOURCES CPP_SOURCES)
    list(APPEND ALL_OBJS ${BUILD_DIR}/${src}.o)
endforeach()
list(APPEND ALL_OBJS ${WHOLE_UNIT_OBJS})

set(LINK_ORDER "")
foreach(section IN LISTS SECTIONS)
    list(APPEND LINK_ORDER ${CMAKE_SOURCE_DIR}/${BUILD_DIR}/${section}_o_files)
endforeach()

make_object_dirs("${ALL_OBJS}")

foreach(obj IN LISTS ALL_OBJS)
    if(NOT obj IN_LIST WHOLE_UNIT_OBJS)
        add_object(${obj})
    endif()
endforeach()

# Assemble undecompiled units from their whole-unit references.
list(LENGTH WHOLE_UNIT_OBJS whole_count)
if(whole_count GREATER 0)
    math(EXPR whole_last "${whole_count} - 1")
    foreach(i RANGE ${whole_last})
        list(GET WHOLE_UNIT_OBJS ${i} obj)
        list(GET WHOLE_UNIT_REFS ${i} ref)
        add_unit_asm_object(${obj} ${ref})
    endforeach()
endif()

# Expand overlay placement in the linker command template.
set(OVERLAY_LCF ${BUILD_DIR}/${BASENAME}.overlays.lcf)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/${LD_TEMPLATE})
expand_overlay_markers(${CMAKE_SOURCE_DIR}/${LD_TEMPLATE}
                       ${CMAKE_SOURCE_DIR}/${OVERLAY_LCF})

set(GEN_LCF ${OVERLAY_LCF})

set(ABS_OBJS "")
foreach(obj IN LISTS ALL_OBJS)
    list(APPEND ABS_OBJS ${CMAKE_SOURCE_DIR}/${obj})
endforeach()

overlay_link_args(OVERLAY_ARGS)
check_overlay_prefixes()

set(OVERLAY_BINS "")
set(PAD_COMMANDS "")
set(VERIFY_FILES ${OUTPUT})
foreach(name IN LISTS OVERLAYS)
    string(TOUPPER ${name} upper)
    list(APPEND OVERLAY_BINS ${CMAKE_BINARY_DIR}/${upper}.BIN)
    list(APPEND VERIFY_FILES ${BUILD_DIR}/${upper}.BIN)
    list(APPEND PAD_COMMANDS
        COMMAND truncate -s %${OVERLAY_FILE_ALIGN} ${BUILD_DIR}/${upper}.BIN)
endforeach()

add_custom_command(
    OUTPUT ${CMAKE_SOURCE_DIR}/${OUTPUT} ${OVERLAY_BINS}
    COMMAND ${LD_MW} ${LD_MW_FLAGS} -o ${OUTPUT} ${GEN_LCF}
            @${BUILD_DIR}/main_o_files ${OVERLAY_ARGS}
    ${PAD_COMMANDS}
    DEPENDS ${ABS_OBJS} ${CMAKE_SOURCE_DIR}/${GEN_LCF} ${LINK_ORDER}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Linking ${OUTPUT} + overlays"
    VERBATIM)

add_custom_target(elf DEPENDS ${CMAKE_SOURCE_DIR}/${OUTPUT} ${OVERLAY_BINS})

# Record whether each symbol comes from source or reference assembly.
set(PROVENANCE ${BUILD_DIR}/symbol_provenance.txt)

add_custom_command(
    OUTPUT ${CMAKE_SOURCE_DIR}/${PROVENANCE} ${LINK_ORDER}
    COMMAND ${PYTHON_CMD} ${SCRIPTS_DIR}/build/layout.py
            --link-order ${BUILD_DIR} --provenance ${PROVENANCE}
            --build-dir ${BUILD_DIR}
            --carved ${REPLACED_SECTIONS} --lcf ${LD_TEMPLATE}
    DEPENDS ${CMAKE_SOURCE_DIR}/${LD_TEMPLATE} ${ABS_CPP_SOURCES}
            ${CMAKE_SOURCE_DIR}/${SCRIPTS_DIR}/build/layout.py
            ${CMAKE_SOURCE_DIR}/${SCRIPTS_DIR}/build/disassemble.py
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Resolving the layout"
    VERBATIM)

add_custom_target(provenance DEPENDS ${CMAKE_SOURCE_DIR}/${PROVENANCE})

# Export compiler flags for diff tooling.
string(JOIN " " CC_MW_FLAGS_TEXT ${CC_MW_FLAGS})
file(GENERATE OUTPUT ${CMAKE_BINARY_DIR}/compiler_flags.txt
     CONTENT "${CC_MW_FLAGS_TEXT}\n")

# Build a shared m2c context from project headers.
file(GLOB CTX_HEADERS
     ${CMAKE_SOURCE_DIR}/${INCLUDE_DIR}/*.hpp
     ${CMAKE_SOURCE_DIR}/${INCLUDE_DIR}/*.h
     ${CMAKE_SOURCE_DIR}/${STD_INCLUDE_DIR}/*
     ${CMAKE_SOURCE_DIR}/${SCE_INCLUDE_DIR}/*)

add_custom_command(
    OUTPUT ${CMAKE_SOURCE_DIR}/${CTX} ${CMAKE_SOURCE_DIR}/${CTX_CPP}
    COMMAND ${PYTHON_CMD} ${SCRIPTS_DIR}/diff/m2ctx.py -o ${CTX}
    DEPENDS ${CTX_HEADERS} ${CMAKE_SOURCE_DIR}/${SCRIPTS_DIR}/diff/m2ctx.py
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Generating m2c context"
    VERBATIM)

add_custom_target(ctx DEPENDS ${CMAKE_SOURCE_DIR}/${CTX} ${CMAKE_SOURCE_DIR}/${CTX_CPP})

# Build reference and source objects for objdiff.
execute_process(
    COMMAND ${PYTHON_CMD} ${SCRIPTS_DIR}/build/layout.py --list-extra-objects
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE objdiff_extra_raw
    RESULT_VARIABLE objdiff_rc
    ERROR_QUIET
    OUTPUT_STRIP_TRAILING_WHITESPACE)

set(OBJDIFF_TARGET_OBJS "")
if(objdiff_rc EQUAL 0 AND objdiff_extra_raw)
    string(REGEX REPLACE "[ \t\r\n]+" ";" objdiff_extra "${objdiff_extra_raw}")
    foreach(src IN LISTS objdiff_extra)
        list(APPEND OBJDIFF_TARGET_OBJS ${BUILD_DIR}/${src}.o)
    endforeach()

    set(OBJDIFF_BASE_OBJS "")
    foreach(src IN LISTS CPP_SOURCES)
        list(APPEND OBJDIFF_BASE_OBJS ${BUILD_DIR}/diff/${src}.o)
    endforeach()

    set(SYMBOL_ALIASES ${BUILD_DIR}/symbol_aliases.txt)
    make_object_dirs("${OBJDIFF_TARGET_OBJS};${OBJDIFF_BASE_OBJS}")
    set(ASM_OBJECT_EXTRA_DEPENDS ${CMAKE_SOURCE_DIR}/${SYMBOL_ALIASES})
    set(OBJDIFF_TARGET_ONLY 1)
    foreach(obj IN LISTS OBJDIFF_TARGET_OBJS)
        add_object(${obj})
    endforeach()
    unset(OBJDIFF_TARGET_ONLY)

    list(LENGTH CPP_SOURCES base_count)
    if(base_count GREATER 0)
        math(EXPR base_last "${base_count} - 1")
        foreach(i RANGE ${base_last})
            list(GET CPP_SOURCES ${i} src)
            list(GET OBJDIFF_BASE_OBJS ${i} obj)
            add_diff_base_object(${obj} ${src})
        endforeach()
    endif()
    unset(ASM_OBJECT_EXTRA_DEPENDS)
endif()

# Align templated reference symbols with MWCC names.
add_custom_command(
    OUTPUT ${CMAKE_SOURCE_DIR}/${SYMBOL_ALIASES}
    COMMAND ${PYTHON_CMD} ${SCRIPTS_DIR}/build/disassemble.py
            --symbol-aliases ${SYMBOL_ALIASES} ${ALL_OBJS}
    DEPENDS ${ABS_OBJS} ${CMAKE_SOURCE_DIR}/${SCRIPTS_DIR}/build/disassemble.py
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Collecting templated symbol names for objdiff"
    VERBATIM)

# objdiff's GUI reads the configuration at the root of the tree, which is the
# NTSC build's; a PAL one stays in its own build directory.
if(REGION STREQUAL "NTSC")
    set(OBJDIFF_CONFIG objdiff.json)
else()
    set(OBJDIFF_CONFIG ${BUILD_DIR}/objdiff.json)
endif()

add_custom_command(
    OUTPUT ${CMAKE_SOURCE_DIR}/${OBJDIFF_CONFIG}
    COMMAND ${PYTHON_CMD} ${SCRIPTS_DIR}/build/layout.py
            --objdiff ${OBJDIFF_CONFIG} --build-dir ${BUILD_DIR}
            --lcf ${LD_TEMPLATE}
    DEPENDS ${CMAKE_SOURCE_DIR}/${LD_TEMPLATE} ${ABS_CPP_SOURCES}
            ${CMAKE_SOURCE_DIR}/${SCRIPTS_DIR}/build/layout.py
            ${CMAKE_SOURCE_DIR}/${SCRIPTS_DIR}/build/disassemble.py
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Generating ${OBJDIFF_CONFIG}"
    VERBATIM)

set(OBJDIFF_ABS_OBJS "")
foreach(obj IN LISTS OBJDIFF_TARGET_OBJS OBJDIFF_BASE_OBJS)
    list(APPEND OBJDIFF_ABS_OBJS ${CMAKE_SOURCE_DIR}/${obj})
endforeach()

set(OBJDIFF_DATA_STAMP ${BUILD_DIR}/objdiff/stamp)
add_custom_command(
    OUTPUT ${CMAKE_SOURCE_DIR}/${OBJDIFF_DATA_STAMP}
    COMMAND ${PYTHON_CMD} ${SCRIPTS_DIR}/build/objdiff_data.py
            --build-dir ${BUILD_DIR} --lcf ${LD_TEMPLATE}
            --stamp ${OBJDIFF_DATA_STAMP}
    DEPENDS ${CMAKE_SOURCE_DIR}/${OUTPUT} ${OBJDIFF_ABS_OBJS}
            ${CMAKE_SOURCE_DIR}/${LD_TEMPLATE}
            ${CMAKE_SOURCE_DIR}/${CONFIG_DIR}/main.symbols.txt
            ${CMAKE_SOURCE_DIR}/${CONFIG_DIR}/title.symbols.txt
            ${CMAKE_SOURCE_DIR}/${CONFIG_DIR}/dun.symbols.txt
            ${CMAKE_SOURCE_DIR}/${SCRIPTS_DIR}/build/objdiff_data.py
            ${CMAKE_SOURCE_DIR}/${SCRIPTS_DIR}/build/layout.py
            ${CMAKE_SOURCE_DIR}/${SCRIPTS_DIR}/build/disassemble.py
            ${CMAKE_SOURCE_DIR}/${SCRIPTS_DIR}/build/retail.py
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Writing objdiff's targets with each unit's data"
    VERBATIM)

add_custom_target(objdiff
    DEPENDS ${CMAKE_SOURCE_DIR}/${OBJDIFF_CONFIG} ${OBJDIFF_ABS_OBJS} ${ABS_OBJS}
            ${CMAKE_SOURCE_DIR}/${SYMBOL_ALIASES}
            ${CMAKE_SOURCE_DIR}/${OBJDIFF_DATA_STAMP})
add_dependencies(objdiff provenance)

add_custom_target(verify_extracted
    COMMAND ${PYTHON_CMD} ${SCRIPTS_DIR}/build/verify.py -e
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    USES_TERMINAL
    VERBATIM)

add_custom_target(build ALL
    COMMAND ${PYTHON_CMD} ${SCRIPTS_DIR}/build/verify.py -c
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    USES_TERMINAL
    VERBATIM)
add_dependencies(build verify_extracted elf provenance ctx)

# Link a stripped executable for the rebuilt disc.
set(ISO_ELF ${BUILD_DIR}/nodebug/${BASENAME})

add_custom_command(
    OUTPUT ${CMAKE_SOURCE_DIR}/${ISO_ELF}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${BUILD_DIR}/nodebug
    COMMAND ${LD_MW} ${LD_MW_ISO_FLAGS} -o ${ISO_ELF} ${GEN_LCF}
            @${BUILD_DIR}/main_o_files ${OVERLAY_ARGS}
    DEPENDS ${ABS_OBJS} ${CMAKE_SOURCE_DIR}/${GEN_LCF} ${LINK_ORDER}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Linking ${ISO_ELF} (no debug info)"
    VERBATIM)

add_custom_command(
    OUTPUT "${CMAKE_SOURCE_DIR}/${BUILT_ISO}"
    COMMAND ${PYTHON_CMD} ${SCRIPTS_DIR}/build/make_iso.py
            --source "${RETAIL_ISO}" --tree ${ROM_REGION_DIR}/extracted/iso
            --out "${BUILT_ISO}"
            ${ISO_ELF} ${BUILD_DIR}/TITLE.BIN ${BUILD_DIR}/DUN.BIN
    DEPENDS ${CMAKE_SOURCE_DIR}/${ISO_ELF} ${OVERLAY_BINS}
            ${CMAKE_SOURCE_DIR}/${SCRIPTS_DIR}/build/make_iso.py
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Mastering ${BUILT_ISO}"
    USES_TERMINAL
    VERBATIM)

add_custom_target(iso DEPENDS "${CMAKE_SOURCE_DIR}/${BUILT_ISO}")

add_custom_target(run
    COMMAND ${SCRIPTS_DIR}/host/pcsx2.sh "${BUILT_ISO}"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    USES_TERMINAL
    VERBATIM)
add_dependencies(run iso)

set(STAMP_DIR ${BUILD_DIR}/stamps)
set(ROM_VERIFIED ${STAMP_DIR}/rom.verified)
set(DISASSEMBLE_STAMP ${STAMP_DIR}/disassembled)
set(EXTRACTED_ELF ${ROM_REGION_DIR}/extracted/iso/${BASENAME})

set(SPLIT_COMMANDS
    COMMAND ${PYTHON_CMD} ${SCRIPTS_DIR}/build/disassemble.py --config ${CONFIG_DIR})

# ps2/asm/ is never committed; it is split from the three binaries under
# rom/<region>/extracted/iso. They come from extracting the disc image, which
# `iso` and `run` need in full, or, without one, from the private repository.
# Either way they are checked against rom/<region>/extracted.sha256 before the
# split.
if(EXISTS "${CMAKE_SOURCE_DIR}/${RETAIL_ISO}")

add_custom_command(
    OUTPUT ${CMAKE_SOURCE_DIR}/${ROM_VERIFIED}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${STAMP_DIR}
    COMMAND ${CMAKE_COMMAND} -E chdir ${ROM_DIR} sha256sum -c ${REGION_DIR}/checksum.sha256
    COMMAND ${CMAKE_COMMAND} -E touch ${ROM_VERIFIED}
    DEPENDS "${CMAKE_SOURCE_DIR}/${RETAIL_ISO}"
            ${CMAKE_SOURCE_DIR}/${ROM_REGION_DIR}/checksum.sha256
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Checking the retail disc image"
    USES_TERMINAL
    VERBATIM)

add_custom_command(
    OUTPUT ${CMAKE_SOURCE_DIR}/${EXTRACTED_ELF}
    COMMAND ${PYTHON_CMD} ${SCRIPTS_DIR}/build/extract.py
    DEPENDS ${CMAKE_SOURCE_DIR}/${ROM_VERIFIED}
            ${CMAKE_SOURCE_DIR}/${SCRIPTS_DIR}/build/extract.py
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Extracting the disc"
    USES_TERMINAL
    VERBATIM)

endif()

add_custom_command(
    OUTPUT ${CMAKE_SOURCE_DIR}/${DISASSEMBLE_STAMP}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${STAMP_DIR}
    COMMAND ${CMAKE_COMMAND} -E chdir ${ROM_DIR} sha256sum -c ${REGION_DIR}/extracted.sha256
    ${SPLIT_COMMANDS}
    COMMAND ${CMAKE_COMMAND} -E touch ${DISASSEMBLE_STAMP}
    DEPENDS ${CMAKE_SOURCE_DIR}/${EXTRACTED_ELF}
            ${CMAKE_SOURCE_DIR}/${SCRIPTS_DIR}/build/disassemble.py
            ${CMAKE_SOURCE_DIR}/${LD_TEMPLATE}
            ${CMAKE_SOURCE_DIR}/${ROM_REGION_DIR}/extracted.sha256
            ${REF_CONFIGS}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Splitting the disc with splat"
    USES_TERMINAL
    VERBATIM)

add_custom_target(setup DEPENDS ${CMAKE_SOURCE_DIR}/${DISASSEMBLE_STAMP})

add_custom_target(disassemble
    ${SPLIT_COMMANDS}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${STAMP_DIR}
    COMMAND ${CMAKE_COMMAND} -E touch ${DISASSEMBLE_STAMP}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    USES_TERMINAL
    VERBATIM)
