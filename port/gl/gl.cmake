# The GL renderer (port/gl/README.md), with DC_RENDERER=gl: Kazarov's host library with port/gl in
# place of its Vulkan units, keeping the display lists (displaylist.cpp) and TIM2 decoding (tim2.cpp)
# they share. Any platform builds it; the Switch uses it with devkitPro's Mesa (switch-gl preset), a
# PC build with DC_RENDERER=gl runs it against its own driver to compare with the Vulkan renderer.

set(DC_GL_DIR ${CMAKE_CURRENT_LIST_DIR})

# The shaders are Kazarov's, as text: shaders.cpp rewrites them for GL when the renderer starts.
function(_dc_gl_shader_header out)
    set(header "${CMAKE_BINARY_DIR}/generated/gl/gl_shaders.hpp")
    set(content "#pragma once\n// Kazarov's shaders as text, for port/gl/shaders.cpp. Written by port/gl/gl.cmake.\n")
    foreach(pair "draw2d.vert;kGlslDraw2dVert" "mesh.vert;kGlslMeshVert" "mesh_prog0.vert;kGlslMeshProg0Vert"
            "draw.frag;kGlslDrawFrag")
        list(GET pair 0 file)
        list(GET pair 1 symbol)
        set(path ${KZ_ROOT}/port/gfx/shaders/${file})
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${path})
        file(READ ${path} source)
        string(APPEND content "inline constexpr const char ${symbol}[] = R\"glsl(${source})glsl\";\n")
    endforeach()
    file(CONFIGURE OUTPUT ${header} CONTENT "${content}" @ONLY)
    set(${out} ${CMAKE_BINARY_DIR}/generated/gl PARENT_SCOPE)
endfunction()

function(dc_gl_host target)
    file(GLOB vulkan_units ${KZ_ROOT}/port/gfx/*.cpp)
    list(FILTER vulkan_units EXCLUDE REGEX "/(displaylist|tim2)\\.cpp$")
    set_source_files_properties(${vulkan_units} PROPERTIES HEADER_FILE_ONLY ON)
    file(GLOB gl_units CONFIGURE_DEPENDS ${DC_GL_DIR}/*.cpp)
    _dc_gl_shader_header(shader_dir)
    target_sources(${target} PRIVATE ${gl_units})
    target_include_directories(${target} PRIVATE ${KZ_ROOT}/port/gfx ${shader_dir})
    if(NINTENDO_SWITCH)
        # GL/glcorearb.h, from devkitPro's Mesa.
        target_include_directories(${target} SYSTEM PRIVATE $ENV{DEVKITPRO}/portlibs/switch/include)
    endif()
endfunction()
