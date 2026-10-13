# Object rules for the translation units of the split configuration.

# The rows the split configuration gives for one kind of listing, read once per run.
function(derive_config flag out_var)
    string(MAKE_C_IDENTIFIER "derive_config_${flag}" key)
    get_property(known GLOBAL PROPERTY ${key} SET)
    if(known)
        get_property(rows GLOBAL PROPERTY ${key})
        set(${out_var} "${rows}" PARENT_SCOPE)
        return()
    endif()

    execute_process(
        COMMAND ${PYTHON_CMD} ${SCRIPTS_DIR}/build/disassemble.py ${flag}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE rows
        RESULT_VARIABLE status
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "disassemble.py ${flag} failed (${status})")
    endif()
    string(REPLACE "\n" ";" rows "${rows}")
    set_property(GLOBAL PROPERTY ${key} "${rows}")
    set(${out_var} "${rows}" PARENT_SCOPE)
endfunction()

# The whole-section dumps of one image that the link takes.
function(read_index_sources section out_var exclude)
    derive_config(--list-dumps rows)

    set(srcs "")
    foreach(row IN LISTS rows)
        string(REPLACE "\t" ";" parts "${row}")
        list(GET parts 0 image)
        list(GET parts 1 name)
        list(GET parts 2 dump)
        if(image STREQUAL section AND NOT name IN_LIST exclude)
            list(APPEND srcs ${dump})
        endif()
    endforeach()
    list(REMOVE_DUPLICATES srcs)

    set(${out_var} "${srcs}" PARENT_SCOPE)
endfunction()

# The image an object links into, and the section renames it needs. Overlay
# objects live in their own directory and carry the overlay's section prefix.
function(object_image obj out_image out_flags)
    kz_overlay_for_object(${obj} overlay)
    set(image main)
    set(flags "")
    if(overlay)
        set(image ${overlay})
        kz_overlay_rename_flags(${overlay} rename_flags)
        list(APPEND flags ${rename_flags})
    endif()
    set(${out_image} ${image} PARENT_SCOPE)
    set(${out_flags} "${flags}" PARENT_SCOPE)
endfunction()

# Assemble a unit that has no decompiled source.
function(add_unit_asm_object obj src)
    object_image(${obj} image fixup_flags)
    # Retail game units use 16-byte text alignment; libraries use 4-byte alignment.
    if(src MATCHES "^${ASM_DIR}/lib/")
        list(APPEND fixup_flags --set-section-alignment .text=4)
    else()
        list(APPEND fixup_flags --set-section-alignment .text=16)
    endif()

    kz_asm_object(${obj} ${src}
        POST COMMAND sh ${SCRIPTS_DIR}/build/fixup_sections.sh ${obj} ${fixup_flags}
             COMMAND ${PYTHON_CMD} ${SCRIPTS_DIR}/build/literals.py
                     --resolve-names ${image} ${obj}
        DEPENDS ${INCLUDE_DIR}/macro.inc ${REF_STAMP}
                ${SCRIPTS_DIR}/build/literals.py
                ${CONFIG_DIR}/${image}.symbols.txt)
endfunction()

# The objdiff base for a compiled unit: the linked object's compile (kept as
# <obj>.raw), with the base's post-processing. The base waits for the linked
# object, which is what writes the raw file.
function(add_diff_base_object obj src)
    object_image(${obj} image fixup_flags)
    set(compiled ${BUILD_DIR}/${src}.o)
    add_custom_command(
        OUTPUT ${CMAKE_SOURCE_DIR}/${obj}
        COMMAND ${CMAKE_COMMAND} -E copy ${compiled}.raw ${obj}
        COMMAND ${PYTHON_CMD} ${SCRIPTS_DIR}/build/literals.py --bind ${obj}
        COMMAND ${PYTHON_CMD} ${SCRIPTS_DIR}/build/postprocess_object.py
                --source-only ${obj} ${src}
        COMMAND sh ${SCRIPTS_DIR}/build/fixup_sections.sh ${obj} ${fixup_flags}
        COMMAND ${PYTHON_CMD} ${SCRIPTS_DIR}/build/literals.py
                --resolve-names ${image} ${obj}
        DEPENDS ${CMAKE_SOURCE_DIR}/${compiled} ${REF_STAMP}
                ${CMAKE_SOURCE_DIR}/${SCRIPTS_DIR}/build/literals.py
                ${CMAKE_SOURCE_DIR}/${SCRIPTS_DIR}/build/postprocess_object.py
                ${CMAKE_SOURCE_DIR}/${CONFIG_DIR}/${image}.symbols.txt
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "CC (diff base) ${src}"
        VERBATIM)
endfunction()

# Build one object from its extension.
function(add_object obj)
    string(REGEX REPLACE "^${BUILD_DIR}/" "" rel ${obj})
    string(REGEX REPLACE "\\.o$" "" src ${rel})

    # Carved data sections are generated into the build tree.
    if(rel MATCHES "^generated/")
        set(src ${BUILD_DIR}/${src})
    endif()

    object_image(${obj} image fixup_flags)
    if(src MATCHES "^${SRC_DIR}/lib/")
        list(APPEND fixup_flags --set-section-alignment .text=4)
    endif()
    # The first BSS dump begins at retail's unaligned byte 0x2a3709.
    # GAS defaults .bss to 16-byte alignment even without an .align directive;
    # that padding would displace all later BSS allocations.
    if(src MATCHES "/main[.]bss[.]part1[.]s$")
        list(APPEND fixup_flags --set-section-alignment .bss=1)
    endif()
    if(OBJDIFF_TARGET_ONLY)
        list(APPEND fixup_flags -w --localize-symbol=.L*)
    endif()

    if(src MATCHES "\\.s$")
        kz_asm_object(${obj} ${src}
            POST COMMAND sh ${SCRIPTS_DIR}/build/fixup_sections.sh ${obj} ${fixup_flags}
                 COMMAND ${PYTHON_CMD} ${SCRIPTS_DIR}/build/literals.py
                         --resolve-names ${image} ${obj}
            DEPENDS ${INCLUDE_DIR}/macro.inc ${ASM_OBJECT_EXTRA_DEPENDS} ${REF_STAMP}
                    ${SCRIPTS_DIR}/build/literals.py
                    ${CONFIG_DIR}/${image}.symbols.txt)
    elseif(src MATCHES "\\.(c|cpp)$")
        # The linked object is compiled once. Its raw compile is kept for the objdiff base.
        kz_mwcc_object(${obj} ${src}
            FLAGS ${CC_MW_FLAGS}
            ENV DCDECOMP_REGION=${REGION} STATEFIX_SOURCE=${src}
            RAW ${obj}.raw
            POST COMMAND ${CMAKE_COMMAND} -E copy ${obj}.raw ${obj}
                 COMMAND ${PYTHON_CMD} ${SCRIPTS_DIR}/build/literals.py --bind ${obj}
                 COMMAND ${PYTHON_CMD} ${SCRIPTS_DIR}/build/postprocess_object.py ${obj} ${src}
                 COMMAND sh ${SCRIPTS_DIR}/build/fixup_sections.sh ${obj} ${fixup_flags}
                 COMMAND ${PYTHON_CMD} ${SCRIPTS_DIR}/build/literals.py
                         --resolve-names ${image} ${obj}
            DEPENDS ${REF_STAMP}
                    ${SCRIPTS_DIR}/build/literals.py
                    ${SCRIPTS_DIR}/build/postprocess_object.py
                    ${SCRIPTS_DIR}/build/statefix.py
                    ${SCRIPTS_DIR}/build/statefix-wibo.sh
                    ${CONFIG_DIR}/expression_node_overrides.json
                    ${CONFIG_DIR}/argument_read_overrides.json
                    ${CONFIG_DIR}/object_fixups.json
                    ${CONFIG_DIR}/${image}.symbols.txt)
    elseif(src MATCHES "\\.bin$")
        add_custom_command(
            OUTPUT ${CMAKE_SOURCE_DIR}/${obj}
            COMMAND ${OBJCOPY} ${BIN_FLAGS} ${src} ${obj}
            DEPENDS ${CMAKE_SOURCE_DIR}/${src}
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            COMMENT "BIN ${src}"
            VERBATIM)
    else()
        message(FATAL_ERROR "No rule to build ${obj}")
    endif()
endfunction()
