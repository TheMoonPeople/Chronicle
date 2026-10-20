// Drawing, clears, copies and the logical mappings, as Kazarov's draw.cpp does them (gfx/README.md
// tells what each means): the same mappings, scissors, full-frame flanks and covers, UI anchoring and
// copy rectangles, with GL's state set where the Vulkan renderer records dynamic state.
//
// GL and Vulkan agree on where things land in memory: clip y = -1 is row 0 of a target in both, and
// texture coordinate 0 samples row 0, so targets and textures are drawn and read with Vulkan's
// conventions unchanged. Only the window is the other way up, which the present's blit turns over
// (renderer.cpp). Faces are told apart in those memory rows, where Vulkan's counter-clockwise is
// GL's clockwise. Depth is Vulkan's [0, 1] through glClipControl.

#include <algorithm>
#include <cmath>
#include <cstring>

#include "gl.hpp"

namespace gfx {

namespace detail {

namespace {

struct Target {
    GlImage       *color = nullptr;
    GlImage       *depth = nullptr;
    GlTexture     *texture = nullptr;
    LogicalMapping mapping = {};
    bool           has_alpha = true;
};

// A pixel rectangle, rows from the top.
struct PixelRectangle {
    int32_t x = 0;
    int32_t y = 0;
    int32_t w = 0;
    int32_t h = 0;
};

Target ResolveTarget(TextureHandle handle) {
    Target target;
    if (handle == kMainTarget) {
        target.color = &MainColorTargetGl();
        target.depth = &MainDepthGl();
        target.mapping = MainMapping(target.color->width, target.color->height);
        return target;
    }
    GlTexture *texture = LookupGlTexture(handle);
    if (texture != nullptr && texture->render_target) {
        target.color = &texture->image;
        target.depth = texture->shares_main_depth ? &MainDepthGl() : &texture->depth;
        target.texture = texture;
        target.mapping = GlTextureMapping(*texture);
        target.has_alpha = texture->desc.has_alpha;
    }
    return target;
}

// With sample_depth the framebuffer leaves the depth texture unattached, for a depth_coverage draw
// to read.
void EnsureRendering(Target &target, bool sample_depth = false) {
    target = ResolveTarget(g.target);
    if (target.color == nullptr) {
        g.target = kMainTarget;
        target = ResolveTarget(g.target);
    }
    if (target.texture != nullptr) {
        target.texture->last_draw_use = g.frame_serial;
    }
    BindDrawFramebuffer(FramebufferOf(target.color->texture, sample_depth ? 0 : target.depth->texture),
                        target.color->width, target.color->height);
    g.rendering = true;
}

// dx and dy move the rect in pixels before the clamp, so an overhang past the target is shifted too.
PixelRectangle PixelRect(const LogicalMapping &mapping, const LogicalRect &rect, float dx = 0.0f, float dy = 0.0f) {
    float x0 = std::clamp(std::floor(rect.x * mapping.scale_x + mapping.offset_x + dx), 0.0f,
                          static_cast<float>(mapping.pixel_width));
    float y0 = std::clamp(std::floor(rect.y * mapping.scale_y + mapping.offset_y + dy), 0.0f,
                          static_cast<float>(mapping.pixel_height));
    float x1 = std::clamp(std::ceil((rect.x + rect.w) * mapping.scale_x + mapping.offset_x + dx), x0,
                          static_cast<float>(mapping.pixel_width));
    float y1 = std::clamp(std::ceil((rect.y + rect.h) * mapping.scale_y + mapping.offset_y + dy), y0,
                          static_cast<float>(mapping.pixel_height));
    return PixelRectangle{static_cast<int32_t>(x0), static_cast<int32_t>(y0), static_cast<int32_t>(x1 - x0),
                          static_cast<int32_t>(y1 - y0)};
}

PixelRectangle Intersect(const PixelRectangle &a, const PixelRectangle &b) {
    int32_t x0 = std::max(a.x, b.x);
    int32_t y0 = std::max(a.y, b.y);
    int32_t x1 = std::min(a.x + a.w, b.x + b.w);
    int32_t y1 = std::min(a.y + a.h, b.y + b.h);
    return PixelRectangle{x0, y0, std::max(x1 - x0, 0), std::max(y1 - y0, 0)};
}

// Letterboxed, the bars keep what they hold: nothing drawn or cleared on the main target reaches past the frame.
bool ClipsToFrame(TextureHandle target) { return target == kMainTarget && !FillsWindow(); }

PixelRectangle FramePixels(const LogicalMapping &mapping) { return PixelRect(mapping, LogicalFrame(kMainTarget)); }

// A span reaches an edge of the logical frame from inside it: what lies wholly in a bar (the host's
// overlay) is not carried anywhere.
bool ReachesLow(float low, float high, float edge) { return low <= edge && high > edge; }

bool ReachesHigh(float low, float high, float edge) { return high >= edge && low < edge; }

// A scissor or clear rect on the current target.
LogicalRect ExtendClearRect(const LogicalRect &rect, const LogicalMapping &mapping) {
    return ExtendRect(rect, ExtentAxes(g.target), LogicalFrame(g.target), mapping);
}

// A scissor with infinite extents reaches the current target's frame edge.
LogicalRect ScissorRect(LogicalRect rect) {
    LogicalRect frame = LogicalFrame(g.target);
    if (std::isinf(rect.w)) {
        rect.w = frame.x + frame.w - rect.x;
    }
    if (std::isinf(rect.h)) {
        rect.h = frame.y + frame.h - rect.y;
    }
    return rect;
}

// The pixels a clear reaches on the current target: its rect, or the whole target.
PixelRectangle ClearArea(const LogicalRect *rect, const Target &target) {
    PixelRectangle area = rect ? PixelRect(target.mapping, ExtendClearRect(*rect, target.mapping))
                               : PixelRectangle{0, 0, static_cast<int32_t>(target.mapping.pixel_width),
                                                static_cast<int32_t>(target.mapping.pixel_height)};
    return ClipsToFrame(g.target) ? Intersect(area, FramePixels(target.mapping)) : area;
}

// The pixels an anchored UI draw moves by: on each anchored axis, from the UI frame's edge to the
// edge of what the main target shows.
void UiSideShift(UiSide side, const LogicalMapping &logical, const LogicalMapping &ui, float &dx, float &dy) {
    LogicalRect visible = VisibleLogicalRect(kMainTarget);
    dx = 0.0f;
    dy = 0.0f;
    if (side.x < 0) {
        dx = visible.x * logical.scale_x + logical.offset_x - ui.offset_x;
    } else if (side.x > 0) {
        dx = (visible.x + visible.w) * logical.scale_x + logical.offset_x -
             (g.config.logical_width * ui.scale_x + ui.offset_x);
    }
    if (side.y < 0) {
        dy = visible.y * logical.scale_y + logical.offset_y - ui.offset_y;
    } else if (side.y > 0) {
        dy = (visible.y + visible.h) * logical.scale_y + logical.offset_y -
             (g.config.logical_height * ui.scale_y + ui.offset_y);
    }
    dx = std::round(dx);
    dy = std::round(dy);
}

// ---- GL state, set only where it changes ---------------------------------------------------------

void SetEnabled(GLenum capability, int &bound, bool enable) {
    int value = enable ? 1 : 0;
    if (bound != value) {
        bound = value;
        if (enable) {
            glEnable(capability);
        } else {
            glDisable(capability);
        }
    }
}

void SetScissor(const PixelRectangle &rect) {
    int values[4] = {rect.x, rect.y, rect.w, rect.h};
    if (std::memcmp(values, gl.bound.scissor, sizeof(values)) != 0) {
        std::memcpy(gl.bound.scissor, values, sizeof(values));
        glScissor(rect.x, rect.y, rect.w, rect.h);
    }
}

void SetColorMask(int mask) {
    if (gl.bound.color_write_mask != mask) {
        gl.bound.color_write_mask = mask;
        glColorMask((mask & kWriteRed) != 0, (mask & kWriteGreen) != 0, (mask & kWriteBlue) != 0,
                    (mask & kWriteAlpha) != 0);
    }
}

void SetDepthWrite(bool write) {
    int value = write ? 1 : 0;
    if (gl.bound.depth_write != value) {
        gl.bound.depth_write = value;
        glDepthMask(write ? GL_TRUE : GL_FALSE);
    }
}

void SetStencilWriteMask(uint32_t index, int mask) {
    if (gl.bound.stencil_write_mask[index] != mask) {
        gl.bound.stencil_write_mask[index] = mask;
        glStencilMaskSeparate(index == 0 ? GL_FRONT : GL_BACK, static_cast<GLuint>(mask));
    }
}

void BindTexture(uint32_t unit, GLuint texture, GLuint sampler) {
    if (gl.bound.textures[unit] != texture) {
        gl.bound.textures[unit] = texture;
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_2D, texture);
    }
    if (gl.bound.samplers[unit] != sampler) {
        gl.bound.samplers[unit] = sampler;
        glBindSampler(unit, sampler);
    }
}

constexpr GLenum kStencilOps[8] = {GL_KEEP, GL_ZERO, GL_REPLACE, GL_INCR, GL_DECR, GL_INVERT, GL_INCR_WRAP, GL_DECR_WRAP};

int PackStencilOps(const StencilFace &face) {
    return static_cast<int>(face.fail) | static_cast<int>(face.pass) << 4 | static_cast<int>(face.depth_fail) << 8;
}

void SetStencilFace(uint32_t index, const StencilFace &face) {
    GLenum gl_face = index == 0 ? GL_FRONT : GL_BACK;
    int    ops = PackStencilOps(face);
    if (gl.bound.stencil_ops[index] != ops) {
        gl.bound.stencil_ops[index] = ops;
        glStencilOpSeparate(gl_face, kStencilOps[static_cast<int>(face.fail) & 7],
                            kStencilOps[static_cast<int>(face.depth_fail) & 7],
                            kStencilOps[static_cast<int>(face.pass) & 7]);
    }
    // CompareOp is Vulkan's order, which is GL's from GL_NEVER.
    int func = static_cast<int>(face.compare) | face.reference << 4 | face.compare_mask << 12;
    if (gl.bound.stencil_func[index] != func) {
        gl.bound.stencil_func[index] = func;
        glStencilFuncSeparate(gl_face, GL_NEVER + static_cast<GLenum>(face.compare), face.reference,
                              face.compare_mask);
    }
    SetStencilWriteMask(index, face.write_mask);
}

GLenum DepthFunc(DepthTest test) {
    switch (test) {
        case DepthTest::Never:
            return GL_NEVER;
        case DepthTest::Always:
            return GL_ALWAYS;
        case DepthTest::GEqual:
            return GL_GEQUAL;
        default:
            return GL_GREATER;
    }
}

VertexKind KindOf(PipelineFamily family) {
    switch (family) {
        case kFamilyMesh:
            return kVertexMesh;
        case kFamilyMeshProg0:
            return kVertexProg0;
        case kFamilyMeshProg0Skinned:
            return kVertexProg0Skinned;
        default:
            return kVertex2D;
    }
}

// Resolves the binding and the state into the per-draw block and sets GL's state. ui places the draw
// by the target's UI mapping, moved to the edges side names; the scissor stays where the frame has it
// unless the side follows it. False when the draw must be dropped.
// A depth_coverage draw as the program takes it: none of the state it does without.
DrawState CoverageState(const DrawState &state) {
    DrawState plain = state;
    plain.blend = false;
    plain.premultiplied = false;
    plain.depth_test = DepthTest::Always;
    plain.depth_write = false;
    plain.alpha_test = false;
    plain.fog = false;
    plain.stencil_test = false;
    return plain;
}

bool Prepare(PipelineFamily family, const TextureBinding &binding, const DrawState &given, bool ui = false,
             UiSide side = {}) {
    if (!g.in_frame) {
        return false;
    }
    const bool coverage = given.depth_coverage != 0;
    if (coverage) {
        Target resolved = ResolveTarget(g.target);
        bool   shares = resolved.color == nullptr || resolved.texture == nullptr || resolved.texture->shares_main_depth;
        if (family != kFamily2DTriangles || given.depth_coverage > kCoveragePlanes || !shares) {
            Error("a depth_coverage draw needs 2D triangles, at most %u planes and the main target or one sharing "
                  "its depth",
                  kCoveragePlanes);
            return false;
        }
    }
    const DrawState state = coverage ? CoverageState(given) : given;
    PushConstants push;
    std::memset(&push, 0, sizeof(push));
    push.uv_xform[0] = 1.0f;
    push.uv_xform[1] = 1.0f;

    TextureMode mode = kTextureNone;
    bool        texa = false;
    GLuint      texture_name = 0;
    GLuint      palette_name = 0;
    if (binding.texture != kNullTexture) {
        if (binding.texture == kMainTarget || binding.texture == g.target) {
            Error("a draw samples the target it renders to; snapshot it first");
            return false;
        }
        if (binding.texture == kPreviousFrame) {
            const GlImage &image = PreviousFrameImageGl();
            LogicalMapping mapping = MainMapping(image.width, image.height);
            float          width = static_cast<float>(image.width);
            float          height = static_cast<float>(image.height);
            mode = kTextureRgba;
            texa = binding.ignore_alpha;
            texture_name = image.texture;
            if (family < kFamilyMesh) {
                push.uv_xform[0] = mapping.scale_x / width;
                push.uv_xform[1] = mapping.scale_y / height;
                push.uv_xform[2] = mapping.offset_x / width;
                push.uv_xform[3] = mapping.offset_y / height;
            }
        } else {
            GlTexture *texture = LookupGlTexture(binding.texture);
            if (texture == nullptr) {
                Error("a draw names texture %#x, which does not exist", binding.texture);
                return false;
            }
            texture->last_draw_use = g.frame_serial;
            texture_name = texture->image.texture;
            if (family < kFamilyMesh) {
                // Logical texels land on the image through its mapping, letterbox offset included.
                LogicalMapping mapping = GlTextureMapping(*texture);
                float          width = static_cast<float>(texture->image.width);
                float          height = static_cast<float>(texture->image.height);
                push.uv_xform[0] = mapping.scale_x / width;
                push.uv_xform[1] = mapping.scale_y / height;
                push.uv_xform[2] = mapping.offset_x / width;
                push.uv_xform[3] = mapping.offset_y / height;
            }
            if (texture->desc.format == TextureFormat::Index8) {
                GlTexture *palette = LookupGlTexture(binding.palette);
                if (palette == nullptr || palette->desc.format != TextureFormat::Rgba8) {
                    Error("index texture %#x is drawn without a palette", binding.texture);
                    return false;
                }
                TextureHandle expanded = PaletteExpansionGl(binding.texture, binding.palette);
                if (expanded != kNullTexture) {
                    GlTexture *rgba = LookupGlTexture(expanded);
                    rgba->last_draw_use = g.frame_serial;
                    texture_name = rgba->image.texture;
                    mode = kTextureRgba;
                } else {
                    palette = LookupGlTexture(binding.palette);
                    palette->last_draw_use = g.frame_serial;
                    texture_name = LookupGlTexture(binding.texture)->image.texture;
                    palette_name = palette->image.texture;
                    mode = kTexturePalette;
                }
            } else {
                mode = kTextureRgba;
                texa = !texture->desc.has_alpha || binding.ignore_alpha;
            }
        }
    }

    if (coverage && mode != kTextureRgba) {
        Error("a depth_coverage draw needs an RGBA texture");
        return false;
    }

    Target target;
    EnsureRendering(target, coverage);

    // The shader writes As as the factor when C is not FIX, which premultiplied takes as its own.
    BlendMapping blend = state.blend && state.premultiplied ? BlendMapping{g.premultiplied_slot, kSourceColor, false}
                                                            : MapBlend(state.alpha, state.blend);
    GLuint program = coverage ? gl.coverage_program : gl.programs[ProgramIndex(KindOf(family), mode, state.alpha_test)];
    if (gl.bound.program != program) {
        gl.bound.program = program;
        glUseProgram(program);
    }
    const GlBlend &equation = GlBlendOf(blend.slot);
    SetEnabled(GL_BLEND, gl.bound.blend_enable, equation.enable);
    if (equation.enable && (!gl.bound.blend_known || !(gl.bound.blend == equation))) {
        gl.bound.blend = equation;
        gl.bound.blend_known = true;
        glBlendFuncSeparate(equation.src, equation.dst, GL_ONE, GL_ZERO);
        glBlendEquationSeparate(equation.op, GL_FUNC_ADD);
    }

    const LogicalMapping mapping = ui && g.target == kMainTarget ? UiMapping(target.mapping) : target.mapping;
    push.xform[0] = 2.0f * mapping.scale_x / static_cast<float>(mapping.pixel_width);
    push.xform[1] = 2.0f * mapping.scale_y / static_cast<float>(mapping.pixel_height);
    push.xform[2] = 2.0f * mapping.offset_x / static_cast<float>(mapping.pixel_width) - 1.0f;
    push.xform[3] = 2.0f * mapping.offset_y / static_cast<float>(mapping.pixel_height) - 1.0f;
    float dx = 0.0f;
    float dy = 0.0f;
    if (ui && g.target == kMainTarget && (side.x != 0 || side.y != 0)) {
        UiSideShift(side, target.mapping, mapping, dx, dy);
        push.xform[2] += 2.0f * dx / static_cast<float>(mapping.pixel_width);
        push.xform[3] += 2.0f * dy / static_cast<float>(mapping.pixel_height);
    }
    // Index textures are looked up texel by texel in the shader, so their sampler never filters.
    Filter   filter = mode == kTexturePalette ? Filter::Nearest : binding.filter;
    uint32_t sampler = SamplerIndex(filter, binding.wrap_u, binding.wrap_v);
    if (family >= kFamilyMesh && filter == Filter::Linear && g.anisotropy_level > 0) {
        sampler = kBaseSamplerCount + (g.anisotropy_level - 1) * 4 + (sampler & 3);
    }
    if (mode != kTextureNone) {
        BindTexture(0, texture_name, gl.samplers[sampler]);
    }
    if (mode == kTexturePalette) {
        BindTexture(1, palette_name, gl.palette_sampler);
    }
    if (coverage) {
        BindTexture(1, target.depth->texture, gl.palette_sampler);
    } else if (mode != kTexturePalette && gl.bound.textures[1] == target.depth->texture) {
        // A depth texture left bound beside a framebuffer it is attached to is a feedback loop to Mesa.
        BindTexture(1, 0, gl.palette_sampler);
    }
    push.flags = static_cast<uint32_t>(blend.transform) << kPushTransformShift;
    if (state.fog) {
        push.flags |= kPushFog;
    }
    if (texa) {
        push.flags |= kPushTexa | (state.texa_aem ? kPushAem : 0);
    }
    if (!target.has_alpha) {
        push.flags |= kPushOpaqueTarget;
    }
    if (blend.fix) {
        push.flags |= kPushFactorFix;
    }
    if (binding.filter == Filter::Linear) {
        push.flags |= kPushLinear;
    }
    push.alpha = state.alpha_ref | (static_cast<uint32_t>(state.alpha_func) & 7) << 8 |
                 static_cast<uint32_t>(state.alpha.fix) << 16 | static_cast<uint32_t>(state.texa_ta0) << 24;
    push.fog_color = state.fog_color[0] | state.fog_color[1] << 8 | state.fog_color[2] << 16;
    for (int i = 0; i < 3; i++) {
        push.fog[i] = static_cast<float>(state.fog_color[i]) * (1.0f / 255.0f);
    }
    push.fog[3] = state.fog ? 1.0f : 0.0f;
    push.blend[0] = blend.fix ? 1.0f : 0.0f;
    push.blend[1] = static_cast<float>(state.alpha.fix) / 128.0f;
    push.blend[2] = blend.transform == 1 ? 1.0f : 0.0f;
    push.blend[3] = blend.transform == 2 ? 1.0f : 0.0f;
    push.target[0] = target.has_alpha ? 0.0f : 1.0f;
    push.target[1] = static_cast<float>(state.texa_ta0) / 128.0f;
    // AlphaFunc's order: never, always, less, lequal, equal, gequal, greater, notequal.
    constexpr uint32_t kAlphaPass[8] = {0b000, 0b111, 0b001, 0b011, 0b010, 0b110, 0b100, 0b101};
    push.alpha_pass = kAlphaPass[static_cast<uint32_t>(state.alpha_func) & 7];
    if (coverage) {
        push.pad = given.depth_coverage;
        for (uint32_t i = 0; i < 4; i++) {
            push.fog[i] = i < given.depth_coverage ? given.coverage_depths[i] : -1.0f;
            push.blend[i] = i + 4 < given.depth_coverage ? given.coverage_depths[i + 4] : -1.0f;
        }
    }
    StreamSpan block = StreamAllocate(sizeof(push), static_cast<size_t>(gl.uniform_alignment));
    std::memcpy(block.data, &push, sizeof(push));
    glBindBufferRange(GL_UNIFORM_BUFFER, 0, StreamBuffer(), block.offset, sizeof(push));

    int cull = static_cast<int>(state.cull);
    if (gl.bound.cull != cull) {
        gl.bound.cull = cull;
        if (state.cull == CullMode::None) {
            glDisable(GL_CULL_FACE);
        } else {
            glEnable(GL_CULL_FACE);
            glCullFace(state.cull == CullMode::Back ? GL_BACK : GL_FRONT);
        }
    }
    // An always-passing test that writes nothing is the same as no test, and cheaper. GL writes no
    // depth with its test off, so a write keeps the test on.
    bool test = !(state.depth_test == DepthTest::Always && !state.depth_write);
    SetEnabled(GL_DEPTH_TEST, gl.bound.depth_test, test);
    SetDepthWrite(state.depth_write);
    int func = static_cast<int>(DepthFunc(state.depth_test));
    if (gl.bound.depth_func != func) {
        gl.bound.depth_func = func;
        glDepthFunc(static_cast<GLenum>(func));
    }
    SetEnabled(GL_STENCIL_TEST, gl.bound.stencil_test, state.stencil_test);
    if (state.stencil_test) {
        SetStencilFace(0, state.stencil_front);
        SetStencilFace(1, state.stencil_back);
    }
    SetColorMask(state.color_write_mask & kWriteRgba);

    int clip_distances = family == kFamilyMesh ? 2 : family > kFamilyMesh ? 6 : 0;
    if (gl.bound.clip_distances != clip_distances) {
        for (int i = 0; i < 6; i++) {
            bool on = i < clip_distances;
            bool was = gl.bound.clip_distances < 0 || i < gl.bound.clip_distances;
            if (on != was || gl.bound.clip_distances < 0) {
                if (on) {
                    glEnable(GL_CLIP_DISTANCE0 + i);
                } else {
                    glDisable(GL_CLIP_DISTANCE0 + i);
                }
            }
        }
        gl.bound.clip_distances = clip_distances;
    }

    PixelRectangle scissor = state.scissor ? PixelRect(mapping, ExtendClearRect(ScissorRect(state.scissor_rect), mapping),
                                                       side.scissor ? dx : 0.0f, side.scissor ? dy : 0.0f)
                                           : PixelRectangle{0, 0, static_cast<int32_t>(mapping.pixel_width),
                                                            static_cast<int32_t>(mapping.pixel_height)};
    if (ClipsToFrame(g.target)) {
        scissor = Intersect(scissor, FramePixels(target.mapping));
    }
    SetScissor(scissor);
    return true;
}

// The vertex array for layout, reading vertices from buffer at offset and indices from elements.
void BindVertices(VertexLayout layout, GLuint buffer, GLintptr offset, GLsizei stride, GLuint elements) {
    if (gl.bound.layout != layout) {
        gl.bound.layout = layout;
        glBindVertexArray(gl.vertex_arrays[layout]);
    }
    if (gl.bound.vertex_buffer[layout] != buffer || gl.bound.vertex_offset[layout] != offset) {
        gl.bound.vertex_buffer[layout] = buffer;
        gl.bound.vertex_offset[layout] = offset;
        glBindVertexBuffer(0, buffer, offset, stride);
    }
    if (elements != 0 && gl.bound.element_buffer[layout] != elements) {
        gl.bound.element_buffer[layout] = elements;
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elements);
    }
}

// prog0 is the draw's Vu_prog0 block at binding 2, or nullptr for a plain draw.
void BindConstants(const MeshConstants &constants, const Prog0Constants *prog0) {
    StreamSpan span = StreamAllocate(sizeof(MeshConstants), static_cast<size_t>(gl.uniform_alignment));
    std::memcpy(span.data, &constants, sizeof(MeshConstants));
    glBindBufferRange(GL_UNIFORM_BUFFER, 1, StreamBuffer(), span.offset, sizeof(MeshConstants));
    if (prog0 == nullptr) {
        return;
    }
    StreamSpan block = StreamAllocate(sizeof(Prog0Constants), static_cast<size_t>(gl.uniform_alignment));
    std::memcpy(block.data, prog0, sizeof(Prog0Constants));
    glBindBufferRange(GL_UNIFORM_BUFFER, 2, StreamBuffer(), block.offset, sizeof(Prog0Constants));
}

VertexLayout MeshLayout(PipelineFamily family) {
    return family == kFamilyMeshProg0Skinned ? kLayoutSkinned : family == kFamilyMeshProg0 ? kLayoutProg0 : kLayoutMesh;
}

GLsizei MeshStride(PipelineFamily family) {
    return static_cast<GLsizei>(family == kFamilyMeshProg0Skinned ? 2 * sizeof(Vertex3D) : sizeof(Vertex3D));
}

// Pixel rectangle of a logical one on an image, for copies and blits, as x0, y0, x1, y1; along the
// axes in extend it runs edge to edge.
bool ResolveCopyRect(TextureHandle handle, const Rect &rect, uint32_t extend, GlImage *&image, GlTexture *&texture,
                     int32_t offsets[4]) {
    texture = nullptr;
    LogicalMapping mapping;
    if (handle == kMainTarget || handle == kPreviousFrame) {
        image = ColorImageOfGl(handle);
        mapping = MainMapping(image->width, image->height);
    } else {
        texture = LookupGlTexture(handle);
        if (texture == nullptr) {
            Error("copy names texture %#x, which does not exist", handle);
            return false;
        }
        image = &texture->image;
        mapping = GlTextureMapping(*texture);
    }
    auto x = [&](int32_t value) {
        return std::clamp(
            static_cast<int32_t>(std::lround(static_cast<float>(value) * mapping.scale_x + mapping.offset_x)), 0,
            static_cast<int32_t>(image->width));
    };
    auto y = [&](int32_t value) {
        return std::clamp(
            static_cast<int32_t>(std::lround(static_cast<float>(value) * mapping.scale_y + mapping.offset_y)), 0,
            static_cast<int32_t>(image->height));
    };
    offsets[0] = x(rect.x);
    offsets[1] = y(rect.y);
    offsets[2] = x(rect.x + rect.w);
    offsets[3] = y(rect.y + rect.h);
    // A mirrored rect stays mirrored.
    if (extend & kAxisX) {
        bool mirrored = rect.w < 0;
        offsets[mirrored ? 2 : 0] = 0;
        offsets[mirrored ? 0 : 2] = static_cast<int32_t>(image->width);
    }
    if (extend & kAxisY) {
        bool mirrored = rect.h < 0;
        offsets[mirrored ? 3 : 1] = 0;
        offsets[mirrored ? 1 : 3] = static_cast<int32_t>(image->height);
    }
    return offsets[0] != offsets[2] && offsets[1] != offsets[3];
}

bool Covers(int32_t start, int32_t length, float frame) {
    int32_t low = std::min(start, start + length);
    int32_t high = std::max(start, start + length);
    return low <= 0 && static_cast<float>(high) >= frame;
}

// Axes along which a copy runs edge to edge: both images show past their logical frames there and
// both rects cover them.
uint32_t CopyAxes(TextureHandle src, const Rect &src_rect, TextureHandle dst, const Rect &dst_rect) {
    uint32_t    axes = ExtentAxes(src) & ExtentAxes(dst);
    LogicalRect src_frame = LogicalFrame(src);
    LogicalRect dst_frame = LogicalFrame(dst);
    if (!Covers(src_rect.x, src_rect.w, src_frame.w) || !Covers(dst_rect.x, dst_rect.w, dst_frame.w)) {
        axes &= ~kAxisX;
    }
    if (!Covers(src_rect.y, src_rect.h, src_frame.h) || !Covers(dst_rect.y, dst_rect.h, dst_frame.h)) {
        axes &= ~kAxisY;
    }
    return axes;
}

struct Corners {
    float           x[2];
    float           y[2];
    const Vertex2D *at[2][2]; // [right][bottom]
};

// The quad's corners when its four vertices are those of an axis-aligned rectangle.
bool AxisAligned(const Vertex2D *quad, Corners &corners) {
    corners.x[0] = std::min({quad[0].x, quad[1].x, quad[2].x, quad[3].x});
    corners.x[1] = std::max({quad[0].x, quad[1].x, quad[2].x, quad[3].x});
    corners.y[0] = std::min({quad[0].y, quad[1].y, quad[2].y, quad[3].y});
    corners.y[1] = std::max({quad[0].y, quad[1].y, quad[2].y, quad[3].y});
    if (!(corners.x[1] > corners.x[0]) || !(corners.y[1] > corners.y[0])) {
        return false;
    }
    uint32_t seen = 0;
    for (int i = 0; i < 4; i++) {
        bool on_x = quad[i].x == corners.x[0] || quad[i].x == corners.x[1];
        bool on_y = quad[i].y == corners.y[0] || quad[i].y == corners.y[1];
        if (!on_x || !on_y) {
            return false;
        }
        int right = quad[i].x == corners.x[1] ? 1 : 0;
        int bottom = quad[i].y == corners.y[1] ? 1 : 0;
        corners.at[right][bottom] = &quad[i];
        seen |= 1u << (right + 2 * bottom);
    }
    return seen == 0xF;
}

float Bilerp(float a, float b, float c, float d, float tx, float ty) {
    return (a * (1.0f - tx) + b * tx) * (1.0f - ty) + (c * (1.0f - tx) + d * tx) * ty;
}

// The quad's attributes at (x, y): texture coordinates continue past its edges, colour, fog and
// depth stop at them.
Vertex2D Continue(const Corners &q, float x, float y) {
    float           tx = (x - q.x[0]) / (q.x[1] - q.x[0]);
    float           ty = (y - q.y[0]) / (q.y[1] - q.y[0]);
    float           cx = std::clamp(tx, 0.0f, 1.0f);
    float           cy = std::clamp(ty, 0.0f, 1.0f);
    const Vertex2D &a = *q.at[0][0];
    const Vertex2D &b = *q.at[1][0];
    const Vertex2D &c = *q.at[0][1];
    const Vertex2D &d = *q.at[1][1];
    Vertex2D        out = {};
    out.x = x;
    out.y = y;
    out.u = Bilerp(a.u, b.u, c.u, d.u, tx, ty);
    out.v = Bilerp(a.v, b.v, c.v, d.v, tx, ty);
    out.z = Bilerp(a.z, b.z, c.z, d.z, cx, cy);
    for (int i = 0; i < 4; i++) {
        out.color[i] = static_cast<uint8_t>(std::lround(Bilerp(a.color[i], b.color[i], c.color[i], d.color[i], cx, cy)));
    }
    out.fog = static_cast<uint8_t>(std::lround(Bilerp(a.fog, b.fog, c.fog, d.fog, cx, cy)));
    return out;
}

// The quads that carry each of the draw's rectangles past every edge of the logical frame it
// reaches, along the axes in axes, out to the target's edge (draw.cpp in Kazarov tells why).
std::vector<Vertex2D> FrameFlanks(Primitive primitive, std::span<const Vertex2D> vertices, uint32_t axes,
                                  const LogicalRect &frame, const LogicalMapping &mapping) {
    std::vector<Vertex2D> flanks;
    if (primitive != Primitive::Quads && !(primitive == Primitive::TriangleStrip && vertices.size() == 4)) {
        return flanks;
    }
    LogicalRect bounds = TargetBounds(mapping);
    float       left = bounds.x;
    float       right = bounds.x + bounds.w;
    float       top = bounds.y;
    float       bottom = bounds.y + bounds.h;
    for (size_t first = 0; first + 4 <= vertices.size(); first += 4) {
        Corners q;
        if (!AxisAligned(&vertices[first], q)) {
            continue;
        }
        bool  x = (axes & kAxisX) != 0;
        bool  y = (axes & kAxisY) != 0;
        float xs[4] = {std::min(left, q.x[0]), q.x[0], q.x[1], std::max(right, q.x[1])};
        float ys[4] = {std::min(top, q.y[0]), q.y[0], q.y[1], std::max(bottom, q.y[1])};
        int   first_column = x && ReachesLow(q.x[0], q.x[1], frame.x) ? 0 : 1;
        int   last_column = x && ReachesHigh(q.x[0], q.x[1], frame.x + frame.w) ? 2 : 1;
        int   first_row = y && ReachesLow(q.y[0], q.y[1], frame.y) ? 0 : 1;
        int   last_row = y && ReachesHigh(q.y[0], q.y[1], frame.y + frame.h) ? 2 : 1;
        for (int row = first_row; row <= last_row; row++) {
            for (int column = first_column; column <= last_column; column++) {
                if ((row == 1 && column == 1) || xs[column] == xs[column + 1] || ys[row] == ys[row + 1]) {
                    continue;
                }
                flanks.push_back(Continue(q, xs[column], ys[row]));
                flanks.push_back(Continue(q, xs[column + 1], ys[row]));
                flanks.push_back(Continue(q, xs[column + 1], ys[row + 1]));
                flanks.push_back(Continue(q, xs[column], ys[row + 1]));
            }
        }
    }
    return flanks;
}

// Whether 2D samples an image of the frame, which lies in the target's logical mapping.
bool SamplesFrame(const TextureBinding &binding) {
    if (binding.texture == kPreviousFrame) {
        return true;
    }
    const GlTexture *texture = LookupGlTexture(binding.texture);
    return texture != nullptr && (texture->shares_main_depth || texture->frame);
}

// Axes along which a 2D draw's texture continues past the logical frame: all of them untextured.
uint32_t TextureAxes(const TextureBinding &binding) {
    return binding.texture == kNullTexture ? kAxisX | kAxisY : ExtentAxes(binding.texture);
}

// The axes along which an axis-aligned rectangle covers the whole logical frame.
uint32_t CoveredAxes(const Corners &q, const LogicalRect &frame) {
    uint32_t axes = 0;
    if (q.x[0] <= frame.x && q.x[1] >= frame.x + frame.w) {
        axes |= kAxisX;
    }
    if (q.y[0] <= frame.y && q.y[1] >= frame.y + frame.h) {
        axes |= kAxisY;
    }
    return axes;
}

// A 2D draw that covers the whole logical frame with a texture that is an image of nothing is a
// cover: stretched to the target's edge on the axes it extends. Empty when nothing is stretched.
std::vector<Vertex2D> StretchFrameCovers(Primitive primitive, std::span<const Vertex2D> vertices,
                                         const TextureBinding &binding, uint32_t extend, const LogicalRect &frame,
                                         const LogicalMapping &mapping) {
    std::vector<Vertex2D> stretched;
    if (extend == 0 || binding.texture == kNullTexture || TextureAxes(binding) != 0 ||
        (primitive != Primitive::Quads && !(primitive == Primitive::TriangleStrip && vertices.size() == 4))) {
        return stretched;
    }
    LogicalRect bounds = TargetBounds(mapping);
    bool        any = false;
    stretched.assign(vertices.begin(), vertices.end());
    for (size_t first = 0; first + 4 <= vertices.size(); first += 4) {
        Corners q;
        if (!AxisAligned(&vertices[first], q) || CoveredAxes(q, frame) != (kAxisX | kAxisY)) {
            continue;
        }
        for (int i = 0; i < 4; i++) {
            Vertex2D &vertex = stretched[first + i];
            if (extend & kAxisX) {
                vertex.x = bounds.x + (vertex.x - q.x[0]) / (q.x[1] - q.x[0]) * bounds.w;
            }
            if (extend & kAxisY) {
                vertex.y = bounds.y + (vertex.y - q.y[0]) / (q.y[1] - q.y[0]) * bounds.h;
            }
        }
        any = true;
    }
    if (!any) {
        stretched.clear();
    }
    return stretched;
}

void DrawPrepared(Primitive primitive, std::span<const Vertex2D> vertices, const TextureBinding &binding,
                  const DrawState &state, bool ui, UiSide side = {});

// The sides the current anchor gives a draw: where the centre of its bounds lies.
UiSide AnchoredSide(std::span<const Vertex2D> vertices) {
    const UiAnchor &anchor = g.ui_anchor;
    float           low_x = INFINITY;
    float           low_y = INFINITY;
    float           high_x = -INFINITY;
    float           high_y = -INFINITY;
    for (const Vertex2D &vertex : vertices) {
        low_x = std::min(low_x, vertex.x);
        low_y = std::min(low_y, vertex.y);
        high_x = std::max(high_x, vertex.x);
        high_y = std::max(high_y, vertex.y);
    }
    float  x = (low_x + high_x) * 0.5f;
    float  y = (low_y + high_y) * 0.5f;
    UiSide side;
    side.x = x < anchor.left_until ? -1 : x >= anchor.right_from ? 1 : 0;
    side.y = y < anchor.top_until ? -1 : y >= anchor.bottom_from ? 1 : 0;
    side.scissor = (side.x != 0 || side.y != 0) && anchor.scissor;
    return side;
}

void MarkUsed(GlTexture *texture) {
    if (texture != nullptr && g.in_frame) {
        texture->last_draw_use = g.frame_serial;
    }
}

// A same-size copy of src's x0, y0, x1, y1 to dst's, as a draw.
void CopyByDraw(const GlImage &src, const int32_t from[4], const GlImage &dst, const int32_t to[4]) {
    glBindFramebuffer(GL_FRAMEBUFFER, FramebufferOf(dst.texture, 0));
    glViewport(0, 0, static_cast<GLsizei>(dst.width), static_cast<GLsizei>(dst.height));
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_CULL_FACE);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    for (int i = 0; i < 6; i++) {
        glDisable(GL_CLIP_DISTANCE0 + i);
    }
    glUseProgram(gl.copy_program);
    float w = static_cast<float>(dst.width);
    float h = static_cast<float>(dst.height);
    glUniform4f(0, 2.0f * static_cast<float>(to[0]) / w - 1.0f, 2.0f * static_cast<float>(to[1]) / h - 1.0f,
                2.0f * static_cast<float>(to[2]) / w - 1.0f, 2.0f * static_cast<float>(to[3]) / h - 1.0f);
    glUniform4f(1, static_cast<float>(from[0]), static_cast<float>(from[1]), static_cast<float>(from[2]),
                static_cast<float>(from[3]));
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, src.texture);
    glBindSampler(0, gl.samplers[0]);
    glBindVertexArray(gl.copy_vertex_array);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    ResetGlBound();
}

// A scratch image of src's format at least width by height, kept for the next copy within an image.
GlImage &Scratch(const GlImage &src, uint32_t width, uint32_t height) {
    GlImage &scratch = src.format == GL_R8 ? gl.scratch_index : gl.scratch_rgba;
    if (scratch.width < width || scratch.height < height) {
        uint32_t w = std::max(width, scratch.width);
        uint32_t h = std::max(height, scratch.height);
        DestroyGlImage(scratch);
        scratch = CreateGlImage(w, h, 1, src.format);
    }
    return scratch;
}

// A copy or blit of src's x0, y0, x1, y1 to dst's, through a scratch image when both are one image.
void Transfer(GlImage &src, const int32_t from[4], GlImage &dst, const int32_t to[4], GLenum filter) {
    auto width = [](const int32_t o[4]) { return std::abs(o[2] - o[0]); };
    auto height = [](const int32_t o[4]) { return std::abs(o[3] - o[1]); };

    if (&src == &dst) {
        GlImage &scratch = Scratch(src, static_cast<uint32_t>(width(from)), static_cast<uint32_t>(height(from)));
        int32_t  whole[4] = {0, 0, width(from), height(from)};
        int32_t ordered[4] = {std::min(from[0], from[2]), std::min(from[1], from[3]), std::max(from[0], from[2]),
                              std::max(from[1], from[3])};
        Transfer(src, ordered, scratch, whole, GL_NEAREST);
        // Mirroring, if any, happens on the way back.
        int32_t back[4] = {whole[0], whole[1], whole[2], whole[3]};
        if (from[2] < from[0]) {
            std::swap(back[0], back[2]);
        }
        if (from[3] < from[1]) {
            std::swap(back[1], back[3]);
        }
        Transfer(scratch, back, dst, to, filter);
        return;
    }

    bool same_size = width(from) == width(to) && height(from) == height(to) && from[2] > from[0] &&
                     from[3] > from[1] && to[2] > to[0] && to[3] > to[1];
    if (same_size) {
        CopyByDraw(src, from, dst, to);
        return;
    }
    glBindFramebuffer(GL_READ_FRAMEBUFFER, FramebufferOf(src.texture, 0));
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, FramebufferOf(dst.texture, 0));
    glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glBlitFramebuffer(from[0], from[1], from[2], from[3], to[0], to[1], to[2], to[3], GL_COLOR_BUFFER_BIT, filter);
    glEnable(GL_SCISSOR_TEST);
    gl.bound.framebuffer = UINT32_MAX;
    gl.bound.color_write_mask = -1;
}

} // namespace

LogicalMapping MainMapping(uint32_t width, uint32_t height) {
    float       w = static_cast<float>(width);
    float       h = static_cast<float>(height);
    const float logical_w = g.config.logical_width;
    const float logical_h = g.config.logical_height;
    const float aspect = g.config.display_aspect;
    float       scale_x;
    float       scale_y;
    if (logical_w / logical_h == aspect) {
        // Square texels keep this expression: the general form below rounds differently.
        scale_x = std::min(w / logical_w, h / logical_h);
        scale_y = scale_x;
    } else {
        float shown_w = std::min(w, h * aspect);
        scale_x = shown_w / logical_w;
        scale_y = shown_w / aspect / logical_h;
    }
    return LogicalMapping{scale_x,
                          scale_y,
                          std::floor((w - logical_w * scale_x) * 0.5f),
                          std::floor((h - logical_h * scale_y) * 0.5f),
                          width,
                          height};
}

LogicalMapping UiMapping(const LogicalMapping &mapping) {
    float scale = g.layout.ui_scale;
    if (scale == 1.0f) {
        return mapping;
    }
    float logical_w = g.config.logical_width;
    float logical_h = g.config.logical_height;
    float centre_x = logical_w * 0.5f * mapping.scale_x + mapping.offset_x;
    float centre_y = logical_h * 0.5f * mapping.scale_y + mapping.offset_y;
    float scale_x = mapping.scale_x * scale;
    float scale_y = mapping.scale_y * scale;
    return LogicalMapping{scale_x,
                          scale_y,
                          std::round(centre_x - logical_w * 0.5f * scale_x),
                          std::round(centre_y - logical_h * 0.5f * scale_y),
                          mapping.pixel_width,
                          mapping.pixel_height};
}

bool FillsWindow() { return g.layout.aspect == AspectMode::Fill; }

LogicalRect TargetBounds(const LogicalMapping &mapping) {
    float left = -mapping.offset_x / mapping.scale_x;
    float top = -mapping.offset_y / mapping.scale_y;
    return {left, top, static_cast<float>(mapping.pixel_width) / mapping.scale_x,
            static_cast<float>(mapping.pixel_height) / mapping.scale_y};
}

uint32_t ExtentAxes(TextureHandle handle) {
    if (!FillsWindow()) {
        return 0;
    }
    if (handle == kMainTarget || handle == kPreviousFrame) {
        return kAxisX | kAxisY;
    }
    const GlTexture *texture = LookupGlTexture(handle);
    if (texture == nullptr || !texture->render_target) {
        return 0;
    }
    if (texture->shares_main_depth) {
        return kAxisX | kAxisY;
    }
    return texture->frame ? kAxisX : 0;
}

LogicalRect LogicalFrame(TextureHandle handle) {
    const GlTexture *texture = handle == kMainTarget ? nullptr : LookupGlTexture(handle);
    if (texture == nullptr) {
        return {0.0f, 0.0f, LogicalWidth(), LogicalHeight()};
    }
    return {0.0f, 0.0f, static_cast<float>(texture->logical_width), static_cast<float>(texture->logical_height)};
}

LogicalRect ExtendRect(LogicalRect rect, uint32_t axes, const LogicalRect &frame, const LogicalMapping &mapping) {
    LogicalRect bounds = TargetBounds(mapping);
    float       left = rect.x;
    float       right = rect.x + rect.w;
    float       top = rect.y;
    float       bottom = rect.y + rect.h;
    if (axes & kAxisX) {
        float low = left;
        float high = right;
        left = ReachesLow(low, high, frame.x) ? std::min(left, bounds.x) : left;
        right = ReachesHigh(low, high, frame.x + frame.w) ? std::max(right, bounds.x + bounds.w) : right;
    }
    if (axes & kAxisY) {
        float low = top;
        float high = bottom;
        top = ReachesLow(low, high, frame.y) ? std::min(top, bounds.y) : top;
        bottom = ReachesHigh(low, high, frame.y + frame.h) ? std::max(bottom, bounds.y + bounds.h) : bottom;
    }
    return {left, top, right - left, bottom - top};
}

void EndRendering() { g.rendering = false; }

void ResetGlBound() {
    GlBound fresh;
    // What the vertex arrays hold survives: it is their own state, not the context's.
    for (uint32_t layout = 0; layout < kLayoutCount; layout++) {
        fresh.vertex_buffer[layout] = gl.bound.vertex_buffer[layout];
        fresh.vertex_offset[layout] = gl.bound.vertex_offset[layout];
        fresh.element_buffer[layout] = gl.bound.element_buffer[layout];
    }
    gl.bound = fresh;
    glBindVertexArray(0);
    glUseProgram(0);
    // Front faces are counter-clockwise on the target with y down, which in GL's rows is clockwise.
    glFrontFace(GL_CW);
    // Every draw and clear sets its scissor; only blits and whole-image clears turn the test off.
    glEnable(GL_SCISSOR_TEST);
}

void ResetDrawState() { ResetGlBound(); }

void ReleaseTarget() {}

void BindDrawFramebuffer(GLuint framebuffer, uint32_t width, uint32_t height) {
    if (gl.bound.framebuffer != framebuffer) {
        gl.bound.framebuffer = framebuffer;
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    }
    if (gl.bound.viewport_w != width || gl.bound.viewport_h != height) {
        gl.bound.viewport_w = width;
        gl.bound.viewport_h = height;
        glViewport(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height));
    }
}

} // namespace detail

using namespace detail;

void SetRenderTarget(TextureHandle handle) {
    if (handle == (RecordingCalls() ? g.record_target : g.target)) {
        return;
    }
    if (handle != kMainTarget) {
        GlTexture *texture = LookupGlTexture(handle);
        if (texture == nullptr || !texture->render_target) {
            Error("SetRenderTarget: %#x is not a render target", handle);
            return;
        }
    }
    if (RecordingCalls()) {
        RecordEntry(TargetEntry{handle});
        g.record_target = handle;
        return;
    }
    if (g.in_frame) {
        EndRendering();
    }
    g.target = handle;
}

TextureHandle CurrentRenderTarget() { return RecordingCalls() ? g.record_target : g.target; }

LogicalMapping GetLogicalMapping(TextureHandle handle) {
    if (handle == kMainTarget || handle == kPreviousFrame) {
        const GlImage &image = *ColorImageOfGl(handle);
        return MainMapping(image.width, image.height);
    }
    GlTexture *texture = LookupGlTexture(handle);
    if (texture == nullptr) {
        return LogicalMapping{1.0f, 1.0f, 0.0f, 0.0f, 0, 0};
    }
    return GlTextureMapping(*texture);
}

LogicalMapping GetUiMapping(TextureHandle handle) {
    LogicalMapping mapping = GetLogicalMapping(handle);
    return handle == kMainTarget ? UiMapping(mapping) : mapping;
}

LogicalRect VisibleLogicalRect(TextureHandle handle) {
    LogicalRect frame = LogicalFrame(handle);
    uint32_t    axes = ExtentAxes(handle);
    if (axes == 0) {
        return frame;
    }
    return ExtendRect(frame, axes, frame, GetLogicalMapping(handle));
}

void SetUiAnchor(const UiAnchor &anchor) { g.ui_anchor = anchor; }

UiAnchor CurrentUiAnchor() { return g.ui_anchor; }

void Draw2D(Primitive primitive, std::span<const Vertex2D> vertices, const TextureBinding &binding,
            const DrawState &state, const Presentation2D *presentation) {
    if (vertices.empty()) {
        return;
    }
    Draw2DSided(primitive, vertices, binding, state, AnchoredSide(vertices), presentation);
}

namespace detail {

void Draw2DSided(Primitive primitive, std::span<const Vertex2D> vertices, const TextureBinding &binding,
                 const DrawState &state, UiSide side, const Presentation2D *presentation) {
    if (RecordingCalls()) {
        Draw2DEntry entry{primitive, std::vector<Vertex2D>(vertices.begin(), vertices.end()), binding, state, side};
        if (presentation != nullptr) {
            entry.presentation_vertices.assign(presentation->vertices.begin(), presentation->vertices.end());
            entry.presentation_state = presentation->state;
        }
        RecordEntry(std::move(entry));
        return;
    }
    // Depth places a draw among the meshes, and an image of the frame lies where the frame does.
    bool ui = state.depth_test == DepthTest::Always && !state.depth_write && !SamplesFrame(binding);
    if (ui && g.target == kMainTarget && (side.x != 0 || side.y != 0)) {
        // An anchored draw is a piece of the HUD, never the frame's cover: nothing flanks it.
        DrawPrepared(primitive, vertices, binding, state, ui, side);
        return;
    }
    uint32_t       extend = ExtentAxes(g.target);
    LogicalMapping mapping = GetLogicalMapping(g.target);
    if (ui && g.target == kMainTarget) {
        mapping = UiMapping(mapping);
    }
    LogicalRect               frame = LogicalFrame(g.target);
    std::vector<Vertex2D>     stretched;
    std::span<const Vertex2D> draw = vertices;
    if (!g.host_draws) {
        stretched = StretchFrameCovers(primitive, vertices, binding, extend, frame, mapping);
        if (!stretched.empty()) {
            draw = stretched;
        }
    }
    DrawPrepared(primitive, draw, binding, state, ui);
    if (uint32_t axes = extend & TextureAxes(binding); axes != 0 && g.in_frame && !g.host_draws) {
        std::vector<Vertex2D> flanks = FrameFlanks(primitive, vertices, axes, frame, mapping);
        if (!flanks.empty()) {
            // Flanks sample past the frame's edge, where a repeating texture would wrap in its far side.
            TextureBinding clamped = binding;
            clamped.wrap_u = Wrap::Clamp;
            clamped.wrap_v = Wrap::Clamp;
            DrawPrepared(Primitive::Quads, flanks, clamped, state, ui);
        }
    }
}

namespace {

void DrawPrepared(Primitive primitive, std::span<const Vertex2D> vertices, const TextureBinding &binding,
                  const DrawState &state, bool ui, UiSide side) {
    bool   lines = primitive == Primitive::Lines || primitive == Primitive::LineStrip;
    GLenum mode = GL_TRIANGLES;
    switch (primitive) {
        case Primitive::TriangleStrip:
            mode = GL_TRIANGLE_STRIP;
            break;
        case Primitive::TriangleFan:
            mode = GL_TRIANGLE_FAN;
            break;
        case Primitive::Lines:
            mode = GL_LINES;
            break;
        case Primitive::LineStrip:
            mode = GL_LINE_STRIP;
            break;
        default:
            break;
    }
    if (!Prepare(lines ? kFamily2DLines : kFamily2DTriangles, binding, state, ui, side)) {
        return;
    }
    StreamSpan data = StreamAllocate(vertices.size_bytes(), 16);
    std::memcpy(data.data, vertices.data(), vertices.size_bytes());
    if (primitive != Primitive::Quads) {
        BindVertices(kLayout2D, StreamBuffer(), data.offset, sizeof(Vertex2D), 0);
        StreamFlush();
        glDrawArrays(mode, 0, static_cast<GLsizei>(vertices.size()));
        return;
    }
    uint32_t quads = static_cast<uint32_t>(vertices.size() / 4);
    if (quads == 0) {
        return;
    }
    StreamSpan indices = StreamAllocate(quads * 6 * sizeof(uint32_t), sizeof(uint32_t));
    uint32_t  *index = reinterpret_cast<uint32_t *>(indices.data);
    for (uint32_t i = 0; i < quads; i++) {
        uint32_t base = i * 4;
        index[0] = base;
        index[1] = base + 1;
        index[2] = base + 2;
        index[3] = base;
        index[4] = base + 2;
        index[5] = base + 3;
        index += 6;
    }
    BindVertices(kLayout2D, StreamBuffer(), data.offset, sizeof(Vertex2D), StreamBuffer());
    StreamFlush();
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(quads * 6), GL_UNSIGNED_INT,
                   reinterpret_cast<const void *>(indices.offset));
}

} // namespace
} // namespace detail

namespace {

// A plain draw that sets kMeshProg0 is drawn plain; DrawMeshProg0 and DrawMeshImmediateProg0 are
// the way to Vu_prog0.
void ReportPlainProg0Flag(const MeshConstants &constants) {
    static bool reported = false;
    if ((constants.flags & kMeshProg0) != 0 && !reported) {
        reported = true;
        Error("a plain mesh draw sets kMeshProg0; it draws as a plain mesh");
    }
}

// prog0 is the draw's Vu_prog0 block, or nullptr for a plain draw.
void DrawIndexedMesh(MeshHandle handle, uint32_t first_index, uint32_t index_count, const MeshConstants &constants,
                     const Prog0Constants *prog0, const TextureBinding &binding, const DrawState &state,
                     const MeshTransform *transform) {
    GlMesh *mesh = LookupGlMesh(handle);
    if (mesh == nullptr || first_index + index_count > mesh->index_count) {
        Error("DrawMesh: bad mesh %#x or index range", handle);
        return;
    }
    if (RecordingCalls()) {
        if (index_count != 0) {
            RecordMesh(MeshEntry{handle, first_index, index_count, {}, {}, constants, binding, state, 0}, transform,
                       prog0);
        }
        return;
    }
    PipelineFamily family = MeshFamily(prog0);
    if (index_count == 0 || !Prepare(family, binding, state)) {
        return;
    }
    BindConstants(constants, family > kFamilyMesh ? prog0 : nullptr);
    BindVertices(MeshLayout(family), mesh->buffer, 0, MeshStride(family), mesh->buffer);
    StreamFlush();
    GLintptr offset = static_cast<GLintptr>(mesh->vertex_count * sizeof(Vertex3D) + first_index * sizeof(uint32_t));
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(index_count), GL_UNSIGNED_INT,
                   reinterpret_cast<const void *>(offset));
}

} // namespace

void DrawMesh(MeshHandle handle, uint32_t first_index, uint32_t index_count, const MeshConstants &constants,
              const TextureBinding &binding, const DrawState &state, const MeshTransform *transform) {
    ReportPlainProg0Flag(constants);
    DrawIndexedMesh(handle, first_index, index_count, constants, nullptr, binding, state, transform);
}

void DrawMeshProg0(MeshHandle handle, uint32_t first_index, uint32_t index_count, const MeshConstants &constants,
                   const Prog0Constants &prog0, const TextureBinding &binding, const DrawState &state,
                   const MeshTransform *transform) {
    DrawIndexedMesh(handle, first_index, index_count, constants, &prog0, binding, state, transform);
}

namespace {

// prog0 is the draw's Vu_prog0 block, or nullptr for a plain draw.
void DrawImmediateMesh(std::span<const Vertex3D> vertices, std::span<const uint32_t> indices,
                       const MeshConstants &constants, const Prog0Constants *prog0, const TextureBinding &binding,
                       const DrawState &state, const MeshTransform *transform) {
    if (RecordingCalls()) {
        if (!vertices.empty() && !indices.empty()) {
            MeshEntry entry{kNullMesh, 0, 0, {}, {}, constants, binding, state, 0};
            entry.index_count = static_cast<uint32_t>(indices.size());
            entry.vertices.assign(vertices.begin(), vertices.end());
            entry.indices.assign(indices.begin(), indices.end());
            RecordMesh(std::move(entry), transform, prog0);
        }
        return;
    }
    PipelineFamily family = MeshFamily(prog0);
    if (vertices.empty() || indices.empty() || !Prepare(family, binding, state)) {
        return;
    }
    StreamSpan vertex_data = StreamAllocate(vertices.size_bytes(), 16);
    std::memcpy(vertex_data.data, vertices.data(), vertices.size_bytes());
    StreamSpan index_data = StreamAllocate(indices.size_bytes(), sizeof(uint32_t));
    std::memcpy(index_data.data, indices.data(), indices.size_bytes());
    BindConstants(constants, family > kFamilyMesh ? prog0 : nullptr);
    BindVertices(MeshLayout(family), StreamBuffer(), vertex_data.offset, MeshStride(family), StreamBuffer());
    StreamFlush();
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indices.size()), GL_UNSIGNED_INT,
                   reinterpret_cast<const void *>(index_data.offset));
}

} // namespace

void DrawMeshImmediate(std::span<const Vertex3D> vertices, std::span<const uint32_t> indices,
                       const MeshConstants &constants, const TextureBinding &binding, const DrawState &state,
                       const MeshTransform *transform) {
    ReportPlainProg0Flag(constants);
    DrawImmediateMesh(vertices, indices, constants, nullptr, binding, state, transform);
}

void DrawMeshImmediateProg0(std::span<const Vertex3D> vertices, std::span<const uint32_t> indices,
                            const MeshConstants &constants, const Prog0Constants &prog0,
                            const TextureBinding &binding, const DrawState &state, const MeshTransform *transform) {
    DrawImmediateMesh(vertices, indices, constants, &prog0, binding, state, transform);
}

void Clear(bool clear_color, const uint8_t color[4], bool clear_depth, float depth, const LogicalRect *rect,
           const LogicalRect *presentation_rect) {
    if (RecordingCalls() && (clear_color || clear_depth)) {
        ClearEntry entry;
        entry.color = clear_color;
        entry.depth = clear_depth;
        if (color != nullptr) {
            std::memcpy(entry.rgba, color, sizeof(entry.rgba));
        }
        entry.z = depth;
        entry.has_presentation_rect = presentation_rect != nullptr;
        entry.presentation_rect = presentation_rect ? *presentation_rect : LogicalRect{};
        entry.has_rect = rect != nullptr;
        entry.rect = rect ? *rect : LogicalRect{};
        RecordClear(entry);
        return;
    }
    if (!g.in_frame || (!clear_color && !clear_depth)) {
        return;
    }
    Target target;
    EnsureRendering(target);
    PixelRectangle area = ClearArea(rect, target);
    if (area.w == 0 || area.h == 0) {
        return;
    }
    SetScissor(area);
    if (clear_color) {
        float alpha = target.has_alpha ? std::min(static_cast<float>(color[3]) / 128.0f, 1.0f) : 1.0f;
        float rgba[4] = {color[0] / 255.0f, color[1] / 255.0f, color[2] / 255.0f, alpha};
        SetColorMask(kWriteRgba);
        glClearBufferfv(GL_COLOR, 0, rgba);
    }
    if (clear_depth) {
        float value = std::clamp(depth, 0.0f, 1.0f);
        SetDepthWrite(true);
        glClearBufferfv(GL_DEPTH, 0, &value);
    }
}

void ClearStencil(uint8_t value, const LogicalRect *rect) {
    if (RecordingCalls()) {
        ClearEntry entry;
        entry.stencil = true;
        entry.stencil_value = value;
        entry.has_rect = rect != nullptr;
        entry.rect = rect ? *rect : LogicalRect{};
        RecordClear(entry);
        return;
    }
    if (!g.in_frame) {
        return;
    }
    Target target;
    EnsureRendering(target);
    PixelRectangle area = ClearArea(rect, target);
    if (area.w == 0 || area.h == 0) {
        return;
    }
    SetScissor(area);
    SetStencilWriteMask(0, 0xFF);
    SetStencilWriteMask(1, 0xFF);
    GLint stencil = value;
    glClearBufferiv(GL_STENCIL, 0, &stencil);
}

bool CopyTexture(TextureHandle src, Rect src_rect, TextureHandle dst, int32_t dst_x, int32_t dst_y) {
    GlImage   *src_image;
    GlImage   *dst_image;
    GlTexture *src_texture;
    GlTexture *dst_texture;
    int32_t    from[4];
    int32_t    to[4];
    Rect       dst_rect = {dst_x, dst_y, src_rect.w, src_rect.h};
    uint32_t   extend = CopyAxes(src, src_rect, dst, dst_rect);
    if (src_rect.w <= 0 || src_rect.h <= 0 || !ResolveCopyRect(src, src_rect, extend, src_image, src_texture, from) ||
        !ResolveCopyRect(dst, dst_rect, extend, dst_image, dst_texture, to)) {
        return false;
    }
    if (src_image->format != dst_image->format) {
        Error("CopyTexture between an index and a colour texture");
        return false;
    }
    // Between images of one scale the copy is exact, clipped by whichever edge comes first;
    // between a scaled render target and a plain texture it becomes a nearest blit.
    LogicalMapping src_mapping = GetLogicalMapping(src);
    LogicalMapping dst_mapping = GetLogicalMapping(dst);
    if (src_mapping.scale_x == dst_mapping.scale_x && src_mapping.scale_y == dst_mapping.scale_y) {
        int32_t width = std::min(from[2] - from[0], to[2] - to[0]);
        int32_t height = std::min(from[3] - from[1], to[3] - to[1]);
        from[2] = from[0] + width;
        from[3] = from[1] + height;
        to[2] = to[0] + width;
        to[3] = to[1] + height;
    }
    if (RecordingCalls()) {
        RecordEntry(CopyEntry{CopyEntry::Copy, src, src_rect, dst, dst_rect, Filter::Nearest});
        return true;
    }
    EndRendering();
    MarkUsed(src_texture);
    MarkUsed(dst_texture);
    Transfer(*src_image, from, *dst_image, to, GL_NEAREST);
    ForgetContentsGl(dst_texture);
    return true;
}

bool BlitTexture(TextureHandle src, Rect src_rect, TextureHandle dst, Rect dst_rect, Filter filter) {
    GlImage   *src_image;
    GlImage   *dst_image;
    GlTexture *src_texture;
    GlTexture *dst_texture;
    int32_t    from[4];
    int32_t    to[4];
    uint32_t   extend = CopyAxes(src, src_rect, dst, dst_rect);
    if (!ResolveCopyRect(src, src_rect, extend, src_image, src_texture, from) ||
        !ResolveCopyRect(dst, dst_rect, extend, dst_image, dst_texture, to)) {
        return false;
    }
    if (src_image->format != dst_image->format) {
        Error("BlitTexture between an index and a colour texture");
        return false;
    }
    if (RecordingCalls()) {
        RecordEntry(CopyEntry{CopyEntry::Blit, src, src_rect, dst, dst_rect, filter});
        return true;
    }
    GLenum gl_filter = filter == Filter::Linear && src_image->format == GL_RGBA8 ? GL_LINEAR : GL_NEAREST;
    EndRendering();
    MarkUsed(src_texture);
    MarkUsed(dst_texture);
    Transfer(*src_image, from, *dst_image, to, gl_filter);
    ForgetContentsGl(dst_texture);
    return true;
}

bool SnapshotFrame(TextureHandle dst) {
    std::optional<TextureInfo> info = GetTextureInfo(dst);
    if (!info || dst == kMainTarget || dst == kPreviousFrame) {
        Error("SnapshotFrame: %#x is not a texture", dst);
        return false;
    }
    return BlitTexture(kMainTarget,
                       Rect{0, 0, static_cast<int32_t>(LogicalWidth()), static_cast<int32_t>(LogicalHeight())}, dst,
                       Rect{0, 0, static_cast<int32_t>(info->width), static_cast<int32_t>(info->height)},
                       Filter::Linear);
}

} // namespace gfx
