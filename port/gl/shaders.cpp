// The programs and the blend table. The shaders are Kazarov's own (tools/kazarov/port/gfx/shaders),
// embedded as text (gl_shaders.hpp, written by port/gl/gl.cmake) and turned into GLSL 4.30 here by
// rewriting what is Vulkan's alone: the version, the specialisation constants (set per program),
// push constants and descriptor sets (uniform blocks at bindings 0, 1 and 2) and the bindless
// texture array (one texture at unit 0, the palette at unit 1). Each rewrite must match, so a
// change to those declarations upstream stops here with its name instead of drawing wrongly.

#include <algorithm>
#include <chrono>
#include <regex>
#include <string>
#include <vector>

#include "gl.hpp"
#include "gl_shaders.hpp"

namespace gfx::detail {

namespace {

void Replace(std::string &source, const char *name, const std::regex &pattern, const std::string &with) {
    if (!std::regex_search(source, pattern)) {
        Fatal("GL: shader %s has no %s to rewrite", name, with.c_str());
    }
    source = std::regex_replace(source, pattern, with);
}

// constants: the specialisation constants' values, by name.
std::string Translate(const char *name, const char *vulkan,
                      const std::vector<std::pair<std::string, std::string>> &constants) {
    std::string source = vulkan;
    Replace(source, name, std::regex(R"(#version 460)"), "#version 430 core");
    for (const auto &[constant, value] : constants) {
        std::regex pattern(R"(layout\(constant_id\s*=\s*\d+\)\s*const\s+(\w+)\s+)" + constant + R"(\s*=[^;]*;)");
        Replace(source, name, pattern, "const $1 " + constant + " = " + value + ";");
    }
    if (std::regex_search(source, std::regex(R"(constant_id)"))) {
        Fatal("GL: shader %s has a specialisation constant the GL renderer does not set", name);
    }
    // The vertex shaders declare only the front of the fragment shader's block, which Vulkan's push
    // constants allow and GL's blocks do not under one name: theirs is another block at the same binding.
    bool vertex = std::regex_search(source, std::regex(R"(\bgl_Position\b)"));
    Replace(source, name, std::regex(R"(layout\(push_constant, std430\) uniform Push)"),
            vertex ? "layout(std140, binding = 0) uniform PushVertex" : "layout(std140, binding = 0) uniform Push");
    if (std::regex_search(source, std::regex(R"(set = 1, binding = 0, std140)"))) {
        Replace(source, name, std::regex(R"(layout\(set = 1, binding = 0, std140\))"), "layout(std140, binding = 1)");
    }
    if (std::regex_search(source, std::regex(R"(set = 2, binding = 0, std140)"))) {
        Replace(source, name, std::regex(R"(layout\(set = 2, binding = 0, std140\))"), "layout(std140, binding = 2)");
    }
    if (std::regex_search(source, std::regex(R"(texture2D u_textures)"))) {
        Replace(source, name, std::regex(R"(layout\(set = 0, binding = 0\) uniform texture2D u_textures\[\d+\];)"),
                "layout(binding = 0) uniform sampler2D u_texture;");
        Replace(source, name, std::regex(R"(layout\(set = 0, binding = 1\) uniform sampler u_samplers\[\d+\];)"),
                "layout(binding = 1) uniform sampler2D u_palette;");
        Replace(source, name, std::regex(R"(#define TEXTURE [^\n]*)"), "#define TEXTURE u_texture");
        Replace(source, name, std::regex(R"(#define PALETTE [^\n]*)"), "#define PALETTE u_palette");
    }
    if (std::regex_search(source, std::regex(R"(\bset = )"))) {
        Fatal("GL: shader %s has a descriptor set the GL renderer does not bind", name);
    }
    return source;
}

GLuint Compile(GLenum stage, const char *name, const std::string &source) {
    GLuint      shader = glCreateShader(stage);
    const char *text = source.c_str();
    glShaderSource(shader, 1, &text, nullptr);
    glCompileShader(shader);
    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (ok != GL_TRUE) {
        char log[4096] = {};
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        Fatal("GL: shader %s does not compile:\n%s", name, log);
    }
    return shader;
}

GLuint Link(GLuint vertex, GLuint fragment, const char *name) {
    GLuint program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);
    GLint ok = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (ok != GL_TRUE) {
        char log[4096] = {};
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        Fatal("GL: program %s does not link:\n%s", name, log);
    }
    return program;
}

// ---- Blending ----------------------------------------------------------------------------------

enum BlendInput : uint8_t {
    kInputSource = 0,
    kInputDest = 1,
    kInputZero = 2,
};

struct Equation {
    GlBlend blend;
    uint8_t transform;
};

constexpr GlBlend kPassThrough = {false, GL_ONE, GL_ZERO, GL_FUNC_ADD};

GlBlend On(GLenum src, GLenum dst, GLenum op) { return {true, src, dst, op}; }

// ((A - B) * C) + D as GL's src * Fs (op) dst * Fd, as Kazarov's pipelines.cpp solves it for
// Vulkan (README.md, "Blending"): the shader writes C to the second dual-source output whenever it
// can compute it (source alpha, FIX); destination alpha is the attachment's.
Equation Solve(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
    bool   dest = c == 1;
    GLenum f = dest ? GL_DST_ALPHA : GL_SRC1_ALPHA;
    GLenum f1 = dest ? GL_ONE_MINUS_DST_ALPHA : GL_ONE_MINUS_SRC1_ALPHA;

    constexpr GLenum kOne = GL_ONE;
    constexpr GLenum kNone = GL_ZERO;
    constexpr GLenum kAdd = GL_FUNC_ADD;
    constexpr GLenum kSub = GL_FUNC_SUBTRACT;
    constexpr GLenum kRev = GL_FUNC_REVERSE_SUBTRACT;

    if (a == b) {
        switch (d) {
            case kInputSource:
                return {kPassThrough, kSourceColor};
            case kInputDest:
                return {On(kNone, kOne, kAdd), kSourceColor};
            default:
                return {On(kNone, kNone, kAdd), kSourceColor};
        }
    }
    if (a == kInputSource && b == kInputDest) {
        switch (d) {
            case kInputDest:
                return {On(f, f1, kAdd), kSourceColor};
            case kInputZero:
                return {On(f, f, kSub), kSourceColor};
            default:
                return {On(kOne, f, kSub), dest ? kSourceColor : kSourceTimesOnePlusC};
        }
    }
    if (a == kInputDest && b == kInputSource) {
        switch (d) {
            case kInputSource:
                return {On(f1, f, kAdd), kSourceColor};
            case kInputZero:
                return {On(f, f, kRev), kSourceColor};
            default:
                return {On(f, kOne, kRev), kSourceColor};
        }
    }
    if (a == kInputSource) { // B is zero
        switch (d) {
            case kInputDest:
                return {On(f, kOne, kAdd), kSourceColor};
            case kInputZero:
                return {On(f, kNone, kAdd), kSourceColor};
            default:
                return {On(kOne, kNone, kAdd), dest ? kSourceColor : kSourceTimesOnePlusC};
        }
    }
    if (a == kInputDest) { // B is zero
        switch (d) {
            case kInputSource:
                return {On(kOne, f, kAdd), kSourceColor};
            case kInputZero:
                return {On(kNone, f, kAdd), kSourceColor};
            default:
                if (dest) {
                    return {On(kNone, kOne, kAdd), kSourceColor};
                }
                return {On(GL_DST_COLOR, kOne, kAdd), kSourceFactor};
        }
    }
    if (b == kInputSource) { // A is zero
        switch (d) {
            case kInputDest:
                return {On(f, kOne, kRev), kSourceColor};
            case kInputSource:
                return {On(f1, kNone, kAdd), kSourceColor};
            default:
                return {On(kNone, kNone, kAdd), kSourceColor};
        }
    }
    switch (d) { // A is zero, B is the destination
        case kInputSource:
            return {On(kOne, f, kSub), kSourceColor};
        case kInputDest:
            return {On(kNone, f1, kAdd), kSourceColor};
        default:
            return {On(kNone, kNone, kAdd), kSourceColor};
    }
}

std::vector<GlBlend> g_blends;

} // namespace

void BuildGlBlendTable() {
    g_blends.clear();
    g_blends.push_back(kPassThrough);
    for (uint8_t a = 0; a < 3; a++) {
        for (uint8_t b = 0; b < 3; b++) {
            for (uint8_t c = 0; c < 3; c++) {
                for (uint8_t d = 0; d < 3; d++) {
                    Equation equation = Solve(a, b, c, d);
                    auto     it = std::find(g_blends.begin(), g_blends.end(), equation.blend);
                    if (it == g_blends.end()) {
                        g_blends.push_back(equation.blend);
                        it = g_blends.end() - 1;
                    }
                    g.blend_map[a * 27 + b * 9 + c * 3 + d] =
                        BlendMapping{static_cast<uint16_t>(it - g_blends.begin()), equation.transform, c == 2};
                }
            }
        }
    }
    g_blends.push_back(On(GL_ONE, GL_ONE_MINUS_SRC1_ALPHA, GL_FUNC_ADD));
    g.premultiplied_slot = static_cast<uint16_t>(g_blends.size() - 1);
    g.blend_slot_count = static_cast<uint32_t>(g_blends.size());
}

const GlBlend &GlBlendOf(uint16_t slot) { return g_blends[slot]; }

BlendMapping MapBlend(const GsBlend &blend, bool enable) {
    if (!enable) {
        return BlendMapping{0, kSourceColor, false};
    }
    // Field value 3 is reserved for A, B and D (taken as zero) and for C (taken as FIX).
    uint32_t a = std::min<uint32_t>(blend.a, 2);
    uint32_t b = std::min<uint32_t>(blend.b, 2);
    uint32_t c = std::min<uint32_t>(blend.c, 2);
    uint32_t d = std::min<uint32_t>(blend.d, 2);
    return g.blend_map[a * 27 + b * 9 + c * 3 + d];
}

PipelineFamily MeshFamily(const Prog0Constants *prog0) {
    if (prog0 == nullptr) {
        return kFamilyMesh;
    }
    if (!g.config.prog0_meshes) {
        static bool reported = false;
        if (!reported) {
            reported = true;
            Error("a Vu_prog0 mesh draw on a renderer without prog0_meshes draws as a plain mesh");
        }
        return kFamilyMesh;
    }
    return (prog0->flags & kProg0Skinned) != 0 ? kFamilyMeshProg0Skinned : kFamilyMeshProg0;
}

// ---- Programs ----------------------------------------------------------------------------------

uint32_t ProgramIndex(VertexKind kind, TextureMode mode, bool alpha_test) {
    return (static_cast<uint32_t>(kind) * static_cast<uint32_t>(kTextureModeCount) + static_cast<uint32_t>(mode)) * 2 +
           (alpha_test ? 1 : 0);
}

void CreatePrograms() {
    auto     start = std::chrono::steady_clock::now();
    uint32_t kinds = g.config.prog0_meshes ? kVertexKindCount : kVertexProg0;
    uint32_t total = kinds * kTextureModeCount * 2;
    uint32_t done = 0;
    if (g.config.progress) {
        g.config.progress(0, total);
    }

    GLuint vertex[kVertexKindCount] = {};
    vertex[kVertex2D] = Compile(GL_VERTEX_SHADER, "draw2d.vert", Translate("draw2d.vert", kGlslDraw2dVert, {}));
    vertex[kVertexMesh] = Compile(GL_VERTEX_SHADER, "mesh.vert", Translate("mesh.vert", kGlslMeshVert, {}));
    if (g.config.prog0_meshes) {
        vertex[kVertexProg0] = Compile(GL_VERTEX_SHADER, "mesh_prog0.vert",
                                       Translate("mesh_prog0.vert", kGlslMeshProg0Vert, {{"kSkinned", "false"}}));
        vertex[kVertexProg0Skinned] = Compile(
            GL_VERTEX_SHADER, "mesh_prog0.vert", Translate("mesh_prog0.vert", kGlslMeshProg0Vert, {{"kSkinned", "true"}}));
    }
    GLuint fragment[kTextureModeCount][2] = {};
    for (uint32_t mode = 0; mode < kTextureModeCount; mode++) {
        for (uint32_t test = 0; test < 2; test++) {
            fragment[mode][test] = Compile(
                GL_FRAGMENT_SHADER, "draw.frag",
                Translate("draw.frag", kGlslDrawFrag,
                          {{"kTextureMode", std::to_string(mode)}, {"kAlphaTest", test != 0 ? "true" : "false"}}));
        }
    }

    gl.programs.assign(static_cast<size_t>(kVertexKindCount) * kTextureModeCount * 2, 0);
    for (uint32_t kind = 0; kind < kinds; kind++) {
        for (uint32_t mode = 0; mode < kTextureModeCount; mode++) {
            for (uint32_t test = 0; test < 2; test++) {
                gl.programs[ProgramIndex(static_cast<VertexKind>(kind), static_cast<TextureMode>(mode), test != 0)] =
                    Link(vertex[kind], fragment[mode][test], "draw");
                done++;
                if (g.config.progress) {
                    g.config.progress(done, total);
                }
            }
        }
    }
    for (GLuint shader : vertex) {
        if (shader != 0) {
            glDeleteShader(shader);
        }
    }
    for (auto &modes : fragment) {
        for (GLuint shader : modes) {
            glDeleteShader(shader);
        }
    }
    gl.program_count = done;
    gl.program_seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
}

void DestroyPrograms() {
    for (GLuint program : gl.programs) {
        if (program != 0) {
            glDeleteProgram(program);
        }
    }
    gl.programs.clear();
}

} // namespace gfx::detail
