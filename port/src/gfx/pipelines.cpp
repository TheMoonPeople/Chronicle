#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <mutex>
#include <random>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

#include "context.hpp"
#include "platform/paths.hpp"
#include "shaders/draw.frag.spv.hpp"
#include "shaders/draw2d.vert.spv.hpp"
#include "shaders/mesh.vert.spv.hpp"

namespace gfx {

namespace detail {

namespace {

enum BlendInput : uint8_t {
    kInputSource = 0,
    kInputDest = 1,
    kInputZero = 2,
};

struct Equation {
    VkBlendFactor src;
    VkBlendFactor dst;
    VkBlendOp     op;
    uint8_t       transform;

    bool operator==(const Equation &) const = default;
};

constexpr Equation kPassThrough = {VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD, kSourceColor};

// ((A - B) * C) + D as Vulkan's src * Fs (op) dst * Fd. The shader writes C to the second
// dual-source output whenever it can compute it (source alpha, FIX), so those share one set of
// pipelines; destination alpha is the attachment's. README.md tabulates the result.
Equation Solve(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
    bool          dest = c == 1;
    VkBlendFactor f = dest ? VK_BLEND_FACTOR_DST_ALPHA : VK_BLEND_FACTOR_SRC1_ALPHA;
    VkBlendFactor f1 = dest ? VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA : VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA;

    constexpr VkBlendFactor kOne = VK_BLEND_FACTOR_ONE;
    constexpr VkBlendFactor kNone = VK_BLEND_FACTOR_ZERO;
    constexpr VkBlendOp     kAdd = VK_BLEND_OP_ADD;
    constexpr VkBlendOp     kSub = VK_BLEND_OP_SUBTRACT;
    constexpr VkBlendOp     kRev = VK_BLEND_OP_REVERSE_SUBTRACT;

    if (a == b) {
        switch (d) {
            case kInputSource:
                return kPassThrough;
            case kInputDest:
                return {kNone, kOne, kAdd, kSourceColor};
            default:
                return {kNone, kNone, kAdd, kSourceColor};
        }
    }
    if (a == kInputSource && b == kInputDest) {
        switch (d) {
            case kInputDest:
                return {f, f1, kAdd, kSourceColor};
            case kInputZero:
                return {f, f, kSub, kSourceColor};
            default:
                // Cs * (1 + C) - Cd * C: the shader scales the source, which saturates before the
                // subtraction instead of after. With C = Ad the source cannot be scaled at all.
                return {kOne, f, kSub, dest ? kSourceColor : kSourceTimesOnePlusC};
        }
    }
    if (a == kInputDest && b == kInputSource) {
        switch (d) {
            case kInputSource:
                return {f1, f, kAdd, kSourceColor};
            case kInputZero:
                return {f, f, kRev, kSourceColor};
            default:
                // Cd * (1 + C) - Cs * C has no destination factor above 1: Cd - Cs * C.
                return {f, kOne, kRev, kSourceColor};
        }
    }
    if (a == kInputSource) { // B is zero
        switch (d) {
            case kInputDest:
                return {f, kOne, kAdd, kSourceColor};
            case kInputZero:
                return {f, kNone, kAdd, kSourceColor};
            default:
                return {kOne, kNone, kAdd, dest ? kSourceColor : kSourceTimesOnePlusC};
        }
    }
    if (a == kInputDest) { // B is zero
        switch (d) {
            case kInputSource:
                return {kOne, f, kAdd, kSourceColor};
            case kInputZero:
                return {kNone, f, kAdd, kSourceColor};
            default:
                // Cd * (1 + C): the shader writes C as the source colour and the blend computes
                // C * Cd + Cd. Not expressible with C = Ad, where it stays Cd.
                if (dest) {
                    return {kNone, kOne, kAdd, kSourceColor};
                }
                return {VK_BLEND_FACTOR_DST_COLOR, kOne, kAdd, kSourceFactor};
        }
    }
    if (b == kInputSource) { // A is zero
        switch (d) {
            case kInputDest:
                return {f, kOne, kRev, kSourceColor};
            case kInputSource:
                return {f1, kNone, kAdd, kSourceColor};
            default:
                return {kNone, kNone, kAdd, kSourceColor};
        }
    }
    switch (d) { // A is zero, B is the destination
        case kInputSource:
            return {kOne, f, kSub, kSourceColor};
        case kInputDest:
            return {kNone, f1, kAdd, kSourceColor};
        default:
            return {kNone, kNone, kAdd, kSourceColor};
    }
}

std::vector<Equation> g_equations;

void BuildBlendTable() {
    g_equations.clear();
    g_equations.push_back(kPassThrough);
    for (uint8_t a = 0; a < 3; a++) {
        for (uint8_t b = 0; b < 3; b++) {
            for (uint8_t c = 0; c < 3; c++) {
                for (uint8_t d = 0; d < 3; d++) {
                    Equation equation = Solve(a, b, c, d);
                    auto     it = std::find(g_equations.begin(), g_equations.end(),
                                            Equation{equation.src, equation.dst, equation.op, kSourceColor});
                    uint16_t slot = 0;
                    if (it == g_equations.end()) {
                        g_equations.push_back(
                            Equation{equation.src, equation.dst, equation.op, kSourceColor});
                        slot = static_cast<uint16_t>(g_equations.size() - 1);
                    } else {
                        slot = static_cast<uint16_t>(it - g_equations.begin());
                    }
                    g.blend_map[a * 27 + b * 9 + c * 3 + d] = BlendMapping{slot, equation.transform, c == 2};
                }
            }
        }
    }
    g.blend_slot_count = static_cast<uint32_t>(g_equations.size());
}

VkShaderModule CreateShader(const uint32_t *code, size_t bytes) {
    VkShaderModuleCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.codeSize = bytes;
    info.pCode = code;
    VkShaderModule module = VK_NULL_HANDLE;
    Check(vkCreateShaderModule(g.device, &info, nullptr, &module), "vkCreateShaderModule");
    return module;
}

std::vector<uint8_t> LoadCache(const std::filesystem::path &path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return {};
    }
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    // Some drivers misbehave on a cache from another device or driver, so it is checked here first.
    VkPipelineCacheHeaderVersionOne header;
    if (data.size() < sizeof(header)) {
        return {};
    }
    std::memcpy(&header, data.data(), sizeof(header));
    if (header.headerSize < sizeof(header) || header.headerVersion != VK_PIPELINE_CACHE_HEADER_VERSION_ONE ||
        header.vendorID != g.properties.vendorID || header.deviceID != g.properties.deviceID ||
        std::memcmp(header.pipelineCacheUUID, g.properties.pipelineCacheUUID, VK_UUID_SIZE) != 0) {
        return {};
    }
    return data;
}

void SaveCache(const std::filesystem::path &path) {
    size_t size = 0;
    if (vkGetPipelineCacheData(g.device, g.pipeline_cache, &size, nullptr) != VK_SUCCESS || size == 0) {
        return;
    }
    std::vector<uint8_t> data(size);
    if (vkGetPipelineCacheData(g.device, g.pipeline_cache, &size, data.data()) != VK_SUCCESS) {
        return;
    }
    std::error_code error;
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path(), error);
    }
    // Written aside and renamed so a concurrent reader never sees half a file.
    std::filesystem::path temporary = path;
    temporary += ".tmp" + std::to_string(std::random_device{}());
    {
        std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
        if (!file ||
            !file.write(reinterpret_cast<const char *>(data.data()), static_cast<std::streamsize>(size))) {
            Error("cannot write the pipeline cache to %s", PathsDisplay(temporary).c_str());
            return;
        }
    }
    std::filesystem::rename(temporary, path, error);
    if (error) {
        Error("cannot write the pipeline cache to %s: %s", PathsDisplay(path).c_str(), error.message().c_str());
        std::filesystem::remove(temporary, error);
    }
}

struct PipelineDesc {
    PipelineFamily family;
    uint32_t       blend;
    TextureMode    mode;
    bool           alpha_test;
};

} // namespace

void CreatePipelineLayout() {
    for (uint32_t i = 0; i < kSamplerCount; i++) {
        bool                linear = i & 4;
        VkSamplerCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        info.magFilter = linear ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
        info.minFilter = info.magFilter;
        info.mipmapMode = linear ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST;
        info.addressModeU = (i & 2) ? VK_SAMPLER_ADDRESS_MODE_REPEAT : VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        info.addressModeV = (i & 1) ? VK_SAMPLER_ADDRESS_MODE_REPEAT : VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        info.maxLod = VK_LOD_CLAMP_NONE;
        Check(vkCreateSampler(g.device, &info, nullptr, &g.samplers[i]), "vkCreateSampler");
    }

    VkDescriptorSetLayoutBinding texture_bindings[2] = {};
    texture_bindings[0].binding = 0;
    texture_bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    texture_bindings[0].descriptorCount = kMaxTextures;
    texture_bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    texture_bindings[1].binding = 1;
    texture_bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    texture_bindings[1].descriptorCount = kSamplerCount;
    texture_bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    texture_bindings[1].pImmutableSamplers = g.samplers;

    VkDescriptorBindingFlags bindless = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
                                        VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |
                                        VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT;
    VkDescriptorBindingFlags binding_flags[2] = {bindless, 0};

    VkDescriptorSetLayoutBindingFlagsCreateInfo flags_info = {};
    flags_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    flags_info.bindingCount = 2;
    flags_info.pBindingFlags = binding_flags;

    VkDescriptorSetLayoutCreateInfo texture_layout = {};
    texture_layout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    texture_layout.pNext = &flags_info;
    texture_layout.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
    texture_layout.bindingCount = 2;
    texture_layout.pBindings = texture_bindings;
    Check(vkCreateDescriptorSetLayout(g.device, &texture_layout, nullptr, &g.texture_set_layout),
          "vkCreateDescriptorSetLayout");

    VkDescriptorSetLayoutBinding constants_binding = {};
    constants_binding.binding = 0;
    constants_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    constants_binding.descriptorCount = 1;
    constants_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    VkDescriptorSetLayoutCreateInfo constants_layout = {};
    constants_layout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    constants_layout.bindingCount = 1;
    constants_layout.pBindings = &constants_binding;
    Check(vkCreateDescriptorSetLayout(g.device, &constants_layout, nullptr, &g.constants_set_layout),
          "vkCreateDescriptorSetLayout");

    VkDescriptorPoolSize texture_sizes[2] = {
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, kMaxTextures },
        {VK_DESCRIPTOR_TYPE_SAMPLER,       kSamplerCount}
    };
    VkDescriptorPoolCreateInfo texture_pool = {};
    texture_pool.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    texture_pool.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
    texture_pool.maxSets = 1;
    texture_pool.poolSizeCount = 2;
    texture_pool.pPoolSizes = texture_sizes;
    Check(vkCreateDescriptorPool(g.device, &texture_pool, nullptr, &g.texture_pool),
          "vkCreateDescriptorPool");

    VkDescriptorSetAllocateInfo allocate = {};
    allocate.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocate.descriptorPool = g.texture_pool;
    allocate.descriptorSetCount = 1;
    allocate.pSetLayouts = &g.texture_set_layout;
    Check(vkAllocateDescriptorSets(g.device, &allocate, &g.texture_set), "vkAllocateDescriptorSets");

    constexpr uint32_t         kConstantSets = 256;
    VkDescriptorPoolSize       constants_size = {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, kConstantSets};
    VkDescriptorPoolCreateInfo constants_pool = {};
    constants_pool.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    constants_pool.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    constants_pool.maxSets = kConstantSets;
    constants_pool.poolSizeCount = 1;
    constants_pool.pPoolSizes = &constants_size;
    Check(vkCreateDescriptorPool(g.device, &constants_pool, nullptr, &g.constants_pool),
          "vkCreateDescriptorPool");

    VkDescriptorSetLayout      layouts[2] = {g.texture_set_layout, g.constants_set_layout};
    VkPushConstantRange        push = {VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                                       sizeof(PushConstants)};
    VkPipelineLayoutCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    info.setLayoutCount = 2;
    info.pSetLayouts = layouts;
    info.pushConstantRangeCount = 1;
    info.pPushConstantRanges = &push;
    Check(vkCreatePipelineLayout(g.device, &info, nullptr, &g.pipeline_layout), "vkCreatePipelineLayout");
}

uint32_t PipelineIndex(PipelineFamily family, uint32_t blend_slot, TextureMode mode, bool alpha_test) {
    return ((family * g.blend_slot_count + blend_slot) * kTextureModeCount + mode) * 2 + (alpha_test ? 1 : 0);
}

uint32_t NoColorPipelineIndex(PipelineFamily family, TextureMode mode, bool alpha_test) {
    uint32_t families = kFamilyCount;
    uint32_t modes = kTextureModeCount;
    return families * g.blend_slot_count * modes * 2 + (family * modes + mode) * 2 + (alpha_test ? 1 : 0);
}

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

void CreatePipelines() {
    auto start = std::chrono::steady_clock::now();
    BuildBlendTable();

    std::vector<uint8_t>      initial = LoadCache(g.config.pipeline_cache);
    VkPipelineCacheCreateInfo cache = {};
    cache.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    cache.initialDataSize = initial.size();
    cache.pInitialData = initial.data();
    Check(vkCreatePipelineCache(g.device, &cache, nullptr, &g.pipeline_cache), "vkCreatePipelineCache");

    VkShaderModule draw2d_module = CreateShader(draw2d_vert, sizeof(draw2d_vert));
    VkShaderModule mesh_module = CreateShader(mesh_vert, sizeof(mesh_vert));
    VkShaderModule frag_module = CreateShader(draw_frag, sizeof(draw_frag));

    VkVertexInputBindingDescription   binding_2d = {0, sizeof(Vertex2D), VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription attributes_2d[4] = {
        {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex2D, x)    },
        {1, 0, VK_FORMAT_R32G32_SFLOAT,    offsetof(Vertex2D, u)    },
        {2, 0, VK_FORMAT_R8G8B8A8_UNORM,   offsetof(Vertex2D, color)},
        {3, 0, VK_FORMAT_R8_UNORM,         offsetof(Vertex2D, fog)  },
    };
    VkVertexInputBindingDescription   binding_mesh = {0, sizeof(Vertex3D), VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription attributes_mesh[4] = {
        {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, position)},
        {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, normal)  },
        {2, 0, VK_FORMAT_R32G32_SFLOAT,    offsetof(Vertex3D, uv)      },
        {3, 0, VK_FORMAT_R8G8B8A8_UNORM,   offsetof(Vertex3D, color)   },
    };
    VkPipelineVertexInputStateCreateInfo vertex_2d = {};
    vertex_2d.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_2d.vertexBindingDescriptionCount = 1;
    vertex_2d.pVertexBindingDescriptions = &binding_2d;
    vertex_2d.vertexAttributeDescriptionCount = 4;
    vertex_2d.pVertexAttributeDescriptions = attributes_2d;
    VkPipelineVertexInputStateCreateInfo vertex_mesh = vertex_2d;
    vertex_mesh.pVertexBindingDescriptions = &binding_mesh;
    vertex_mesh.pVertexAttributeDescriptions = attributes_mesh;

    VkPipelineInputAssemblyStateCreateInfo triangles = {};
    triangles.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    triangles.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkPipelineInputAssemblyStateCreateInfo lines = triangles;
    lines.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;

    VkPipelineViewportStateCreateInfo viewport = {};
    viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport.viewportCount = 1;
    viewport.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo raster = {};
    raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisample = {};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depth = {};
    depth.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth.depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL;

    // Depth and stencil state, cull mode and topology within a class are core dynamic state, and
    // so is the colour write mask where VK_EXT_extended_dynamic_state3 offers it: they multiply no
    // pipelines.
    std::vector<VkDynamicState> dynamic_states = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
        VK_DYNAMIC_STATE_CULL_MODE,
        VK_DYNAMIC_STATE_FRONT_FACE,
        VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY,
        VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE,
        VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE,
        VK_DYNAMIC_STATE_DEPTH_COMPARE_OP,
        VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE,
        VK_DYNAMIC_STATE_STENCIL_OP,
        VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK,
        VK_DYNAMIC_STATE_STENCIL_WRITE_MASK,
        VK_DYNAMIC_STATE_STENCIL_REFERENCE,
    };
    if (g.dynamic_color_write_mask) {
        dynamic_states.push_back(VK_DYNAMIC_STATE_COLOR_WRITE_MASK_EXT);
    }
    VkPipelineDynamicStateCreateInfo dynamic = {};
    dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic.dynamicStateCount = static_cast<uint32_t>(dynamic_states.size());
    dynamic.pDynamicStates = dynamic_states.data();

    VkFormat                      color_format = kColorFormat;
    VkPipelineRenderingCreateInfo rendering = {};
    rendering.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    rendering.colorAttachmentCount = 1;
    rendering.pColorAttachmentFormats = &color_format;
    rendering.depthAttachmentFormat = g.depth_format;
    rendering.stencilAttachmentFormat = g.depth_format;

    std::vector<VkPipelineColorBlendAttachmentState> attachments(g.blend_slot_count);
    std::vector<VkPipelineColorBlendStateCreateInfo> blends(g.blend_slot_count);
    for (uint32_t i = 0; i < g.blend_slot_count; i++) {
        const Equation                      &equation = g_equations[i];
        VkPipelineColorBlendAttachmentState &attachment = attachments[i];
        attachment.blendEnable = equation == kPassThrough ? VK_FALSE : VK_TRUE;
        attachment.srcColorBlendFactor = equation.src;
        attachment.dstColorBlendFactor = equation.dst;
        attachment.colorBlendOp = equation.op;
        // The GS never blends alpha: the source alpha is what lands in the frame buffer.
        attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        attachment.alphaBlendOp = VK_BLEND_OP_ADD;
        attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                    VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        blends[i] = {};
        blends[i].sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        blends[i].attachmentCount = 1;
        blends[i].pAttachments = &attachments[i];
    }

    struct Specialization {
        int32_t  mode;
        VkBool32 alpha_test;
    };

    VkSpecializationMapEntry spec_entries[2] = {
        {0, offsetof(Specialization, mode),       sizeof(int32_t) },
        {1, offsetof(Specialization, alpha_test), sizeof(VkBool32)},
    };
    Specialization       spec_data[kTextureModeCount][2];
    VkSpecializationInfo spec_info[kTextureModeCount][2];
    for (uint32_t mode = 0; mode < kTextureModeCount; mode++) {
        for (uint32_t test = 0; test < 2; test++) {
            spec_data[mode][test] = {static_cast<int32_t>(mode), test ? VK_TRUE : VK_FALSE};
            spec_info[mode][test] = {2, spec_entries, sizeof(Specialization), &spec_data[mode][test]};
        }
    }

    // Without a dynamic mask, writing no colour is a pipeline of its own: blending is moot then,
    // so one per family, texture mode and alpha test (the test still discards).
    VkPipelineColorBlendAttachmentState no_color_attachment = {};
    VkPipelineColorBlendStateCreateInfo no_color = {};
    no_color.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    no_color.attachmentCount = 1;
    no_color.pAttachments = &no_color_attachment;

    uint32_t blended = kFamilyCount * g.blend_slot_count * kTextureModeCount * 2;
    uint32_t families = kFamilyCount;
    uint32_t modes = kTextureModeCount;
    uint32_t total = blended + (g.dynamic_color_write_mask ? 0 : families * modes * 2);

    std::vector<VkPipelineShaderStageCreateInfo> stages(total * 2);
    std::vector<VkGraphicsPipelineCreateInfo>    infos(total);
    uint32_t                                     blend_variants = g.blend_slot_count + (g.dynamic_color_write_mask ? 0 : 1);
    for (uint32_t family = 0; family < kFamilyCount; family++) {
        for (uint32_t blend = 0; blend < blend_variants; blend++) {
            bool writes_color = blend < g.blend_slot_count;
            for (uint32_t mode = 0; mode < kTextureModeCount; mode++) {
                for (uint32_t test = 0; test < 2; test++) {
                    uint32_t index = writes_color
                                         ? PipelineIndex(static_cast<PipelineFamily>(family), blend,
                                                         static_cast<TextureMode>(mode), test != 0)
                                         : NoColorPipelineIndex(static_cast<PipelineFamily>(family),
                                                                static_cast<TextureMode>(mode), test != 0);

                    VkPipelineShaderStageCreateInfo *stage = &stages[index * 2];
                    stage[0] = {};
                    stage[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
                    stage[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
                    stage[0].module = family == kFamilyMesh ? mesh_module : draw2d_module;
                    stage[0].pName = "main";
                    stage[1] = stage[0];
                    stage[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
                    stage[1].module = frag_module;
                    stage[1].pSpecializationInfo = &spec_info[mode][test];

                    VkGraphicsPipelineCreateInfo &info = infos[index];
                    info = {};
                    info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
                    info.pNext = &rendering;
                    info.stageCount = 2;
                    info.pStages = stage;
                    info.pVertexInputState = family == kFamilyMesh ? &vertex_mesh : &vertex_2d;
                    info.pInputAssemblyState = family == kFamily2DLines ? &lines : &triangles;
                    info.pViewportState = &viewport;
                    info.pRasterizationState = &raster;
                    info.pMultisampleState = &multisample;
                    info.pDepthStencilState = &depth;
                    info.pColorBlendState = writes_color ? &blends[blend] : &no_color;
                    info.pDynamicState = &dynamic;
                    info.layout = g.pipeline_layout;
                }
            }
        }
    }

    // Pipeline creation is thread-safe against one cache, and on a cold cache it is the whole
    // cost of starting up, so it is spread over the cores while the caller reports progress.
    g.pipelines.assign(total, VK_NULL_HANDLE);
    std::atomic<uint32_t>   next{0};
    std::atomic<uint32_t>   done{0};
    std::atomic<VkResult>   failure{VK_SUCCESS};
    std::mutex              mutex;
    std::condition_variable progress;
    uint32_t                workers = std::clamp(std::thread::hardware_concurrency(), 1u, 8u);
    {
        std::vector<std::jthread> threads;
        for (uint32_t i = 0; i < workers; i++) {
            threads.emplace_back([&]() {
                for (uint32_t index = next++; index < total; index = next++) {
                    VkResult result = vkCreateGraphicsPipelines(g.device, g.pipeline_cache, 1, &infos[index],
                                                                nullptr, &g.pipelines[index]);
                    if (result != VK_SUCCESS) {
                        failure = result;
                    }
                    {
                        std::lock_guard lock(mutex);
                        done++;
                    }
                    progress.notify_one();
                }
            });
        }
        uint32_t reported = UINT32_MAX;
        while (true) {
            std::unique_lock lock(mutex);
            progress.wait(lock, [&]() { return done != reported; });
            reported = done;
            lock.unlock();
            if (g.config.progress) {
                g.config.progress(reported, total);
            }
            if (reported == total) {
                break;
            }
        }
    }
    Check(failure, "vkCreateGraphicsPipelines");

    vkDestroyShaderModule(g.device, draw2d_module, nullptr);
    vkDestroyShaderModule(g.device, mesh_module, nullptr);
    vkDestroyShaderModule(g.device, frag_module, nullptr);
    SaveCache(g.config.pipeline_cache);
    g.pipeline_seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    std::fprintf(stderr, "gfx: %u pipelines ready in %.2f s\n", total, g.pipeline_seconds);
}

void DestroyPipelines() {
    for (VkPipeline pipeline : g.pipelines) {
        vkDestroyPipeline(g.device, pipeline, nullptr);
    }
    g.pipelines.clear();
    if (g.pipeline_cache != VK_NULL_HANDLE) {
        vkDestroyPipelineCache(g.device, g.pipeline_cache, nullptr);
        g.pipeline_cache = VK_NULL_HANDLE;
    }
}

} // namespace detail

uint32_t PipelineCount() { return static_cast<uint32_t>(detail::g.pipelines.size()); }

double PipelineCompileSeconds() { return detail::g.pipeline_seconds; }

} // namespace gfx
