// Parse JSON before the game headers introduce single-letter graphics macros.
// clang-format off
#include <nlohmann/json.hpp>
#include "texture_port.hpp"
// clang-format on

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>

#include "dataread_port.hpp"
#include "mglib_port.hpp"
#include "platform/load_work_profile.hpp"
#include "platform/png_image.hpp"
#include "screen.hpp"
#include "texture.hpp"

namespace {

constexpr unsigned kKeyCount = 1u << 14;
constexpr unsigned kFirstKey = 0x20;

constexpr unsigned kPsmT8 = SCE_GS_PSMT8;
constexpr unsigned kPsmT4 = SCE_GS_PSMT4;
constexpr unsigned kPsmT8H = 27;
constexpr unsigned kPsmT4HL = 36;
constexpr unsigned kPsmT4HH = 44;

std::unordered_map<std::string, std::vector<std::uint8_t>> texture_cluts;

using BakedLoadClock = std::chrono::steady_clock;

bool BakedLoadPerfEnabled() {
    static const bool enabled = [] {
        const char *value = std::getenv("DC_PERF_LOAD");
        return value != nullptr && std::strcmp(value, "1") == 0;
    }();
    return enabled;
}

double BakedLoadMilliseconds(BakedLoadClock::time_point start, BakedLoadClock::time_point end) {
    return std::chrono::duration<double, std::milli>(end - start).count();
}

// A texture bundle commonly supplies several consecutive pictures. Keep only the last
// small document per loading thread, and still read/compare its complete bytes each time.
std::shared_ptr<const nlohmann::json> ParseBakedDocument(std::span<const std::uint8_t> bytes, bool &cache_hit) {
    static const bool enabled = [] {
        const char *value = std::getenv("DC_TEXTURE_JSON_CACHE");
        return value == nullptr || std::strcmp(value, "1") == 0;
    }();

    struct CachedDocument {
        std::vector<std::uint8_t>             bytes;
        std::shared_ptr<const nlohmann::json> parsed;
    };

    thread_local CachedDocument cached;
    cache_hit = enabled && cached.parsed && cached.bytes.size() == bytes.size() &&
                std::equal(bytes.begin(), bytes.end(), cached.bytes.begin());
    if (cache_hit) {
        return cached.parsed;
    }
    auto parsed = std::make_shared<const nlohmann::json>(
        nlohmann::json::parse(bytes.begin(), bytes.end(), nullptr, false));
    if (enabled && bytes.size() <= 64 * 1024 && !parsed->is_discarded()) {
        try {
            std::vector<std::uint8_t> key(bytes.begin(), bytes.end());
            cached.bytes = std::move(key);
            cached.parsed = parsed;
        } catch (const std::bad_alloc &) {
            // Retaining a parsed document is optional; this load can still proceed.
        }
    }
    return parsed;
}

struct BakedLoadTrace {
    std::string_view           document;
    std::string_view           name;
    unsigned                   picture_index;
    std::size_t                json_bytes = 0;
    bool                       json_cache_hit = false;
    std::size_t                png_reads = 0;
    std::size_t                png_decodes = 0;
    std::size_t                png_cache_hits = 0;
    std::uint64_t              png_bytes = 0;
    std::uint64_t              rgba_bytes = 0;
    double                     json_read_ms = 0.0;
    double                     json_parse_schema_ms = 0.0;
    double                     png_read_ms = 0.0;
    double                     png_decode_ms = 0.0;
    bool                       success = false;
    BakedLoadClock::time_point started;

    BakedLoadTrace(std::string_view document, std::string_view name, unsigned picture_index)
        : document(document), name(name), picture_index(picture_index), started(BakedLoadClock::now()) {}

    ~BakedLoadTrace() {
        if (!success) {
            return;
        }
        const auto            finished = BakedLoadClock::now();
        const double          total_ms = BakedLoadMilliseconds(started, finished);
        const double          other_ms = total_ms - json_read_ms - json_parse_schema_ms - png_read_ms - png_decode_ms;
        const auto            cache = PngDecodeCacheGetStats();
        constexpr std::size_t max_text = 512;
        std::fprintf(stderr,
                     "perf load texture success picture %u mips %zu json_reads 1 json_bytes %zu "
                     "png_reads %zu png_bytes %llu png_decodes %zu rgba_bytes %llu total_wall_ms %.6f "
                     "json_read_wall_ms %.6f json_parse_schema_wall_ms %.6f png_read_wall_ms %.6f "
                     "png_decode_wall_ms %.6f selection_assembly_other_wall_ms %.6f png_cache_hits %zu "
                     "png_cache_resident_bytes %zu png_cache_entries %zu json_cache_hit %d "
                     "doc_len %zu name_len %zu doc %.*s name %.*s\n",
                     picture_index, png_reads, json_bytes, png_reads, static_cast<unsigned long long>(png_bytes),
                     png_decodes, static_cast<unsigned long long>(rgba_bytes), total_ms, json_read_ms,
                     json_parse_schema_ms, png_read_ms, png_decode_ms, other_ms, png_cache_hits,
                     cache.resident_bytes, cache.entries, json_cache_hit,
                     document.size(), name.size(), static_cast<int>(std::min(document.size(), max_text)),
                     document.empty() ? "" : document.data(), static_cast<int>(std::min(name.size(), max_text)),
                     name.empty() ? "" : name.data());
    }
};

enum class EntryKind : uint8_t {
    Free,
    Image,
    Palette,
};

struct Entry {
    EntryKind          kind = EntryKind::Free;
    PortTextureOwner   owner = PortTextureOwner::Other;
    gfx::TextureHandle texture = gfx::kNullTexture;
    bool               owns_texture = false;
    // A named render target is looked up by name on every use: gfx recreates it when asked for at
    // another size, and the handle this entry saw would die with the old one.
    std::string                                target;
    unsigned                                   width = 0;
    unsigned                                   height = 0;
    gfx::TextureFormat                         format = gfx::TextureFormat::Rgba8;
    bool                                       has_alpha = true;
    unsigned                                   palette = 0;
    bool                                       four_bit = false;
    std::unique_ptr<std::array<uint32_t, 256>> shadow;
};

struct Registry {
    std::vector<Entry> entries = std::vector<Entry>(kKeyCount);
    unsigned           cursor = kFirstKey;
};

// Function-local: the manager's constructor runs Initialize during static initialisation, in an
// order this unit does not control.
Registry &Reg() {
    static Registry registry;
    return registry;
}

bool RendererUp() {
    return gfx::PipelineCount() != 0;
}

bool Reserved(unsigned key) {
    return key < kFirstKey || key == kMGPortFrameTbp0 || key == kMGPortPreviousFrameTbp0;
}

Entry *Live(unsigned key) {
    if (key >= kKeyCount) {
        return nullptr;
    }
    Entry &entry = Reg().entries[key];
    return entry.kind == EntryKind::Free ? nullptr : &entry;
}

unsigned AllocateKey(EntryKind kind, PortTextureOwner owner) {
    Registry &reg = Reg();
    for (unsigned tries = 0; tries < kKeyCount; tries++) {
        unsigned key = reg.cursor;
        reg.cursor = reg.cursor + 1 >= kKeyCount ? kFirstKey : reg.cursor + 1;
        if (!Reserved(key) && reg.entries[key].kind == EntryKind::Free) {
            Entry &entry = reg.entries[key];
            entry = Entry{};
            entry.kind = kind;
            entry.owner = owner;
            return key;
        }
    }
    std::fprintf(stderr, "texture: every TBP0/CBP key is in use; textures are leaking\n");
    std::abort();
}

gfx::TextureHandle EntryTexture(const Entry &entry) {
    return entry.target.empty() ? entry.texture : gfx::FindNamedRenderTarget(entry.target);
}

unsigned RegisterPalette(const std::array<uint32_t, 256> &rgba, bool four_bit, PortTextureOwner owner) {
    unsigned key = AllocateKey(EntryKind::Palette, owner);
    Entry   &entry = Reg().entries[key];
    entry.shadow = std::make_unique<std::array<uint32_t, 256>>(rgba);
    entry.four_bit = four_bit;
    entry.width = four_bit ? 8 : 16;
    entry.height = four_bit ? 2 : 16;
    if (RendererUp()) {
        entry.texture = gfx::CreatePalette();
        entry.owns_texture = true;
        if (entry.texture != gfx::kNullTexture) {
            gfx::UpdatePalette(entry.texture, rgba.data());
        }
    }
    return key;
}

void ReportOnce(bool &reported, const char *what, unsigned src, unsigned dst) {
    if (!reported) {
        reported = true;
        std::fprintf(stderr, "texture: %s (TBP0 %#x -> %#x); further ones are not reported\n", what, src, dst);
    }
}

// 8-bit unswizzle tables: BlockConv8to32's lut inverted, per column parity.
struct InverseLut {
    uint8_t to_output[2][64];

    constexpr InverseLut() : to_output{} {
        constexpr uint8_t lut[128] = {
            0, 36, 8, 44, 1, 37, 9, 45, 2, 38, 10, 46, 3, 39, 11, 47, 4, 32, 12, 40, 5, 33, 13, 41,
            6, 34, 14, 42, 7, 35, 15, 43, 16, 52, 24, 60, 17, 53, 25, 61, 18, 54, 26, 62, 19, 55, 27, 63,
            20, 48, 28, 56, 21, 49, 29, 57, 22, 50, 30, 58, 23, 51, 31, 59, 4, 32, 12, 40, 5, 33, 13, 41,
            6, 34, 14, 42, 7, 35, 15, 43, 0, 36, 8, 44, 1, 37, 9, 45, 2, 38, 10, 46, 3, 39, 11, 47,
            20, 48, 28, 56, 21, 49, 29, 57, 22, 50, 30, 58, 23, 51, 31, 59, 16, 52, 24, 60, 17, 53, 25, 61,
            18, 54, 26, 62, 19, 55, 27, 63};
        for (unsigned parity = 0; parity < 2; parity++) {
            for (unsigned m = 0; m < 64; m++) {
                to_output[parity][lut[parity * 64 + m]] = static_cast<uint8_t>(m);
            }
        }
    }
};

constexpr InverseLut kInverseLut;

// Where Conv8to32 puts the 8-bit texel (x, y) of a width x height image, or -1 where its
// page-sized work buffers drop it (images narrower than a block).
long SwizzledOffset(int width, int height, int x, int y) {
    int pages_x = ((width - 1) >> 7) + 1;
    int pages_y = ((height - 1) >> 6) + 1;
    int page_width = pages_x == 1 ? width : 128;
    int row_bytes = pages_x == 1 ? width * 2 : 256;
    int row_count = pages_y == 1 ? height >> 1 : 32;
    int page_x = x / page_width;
    int page_y = y >> 6;
    int local_x = x - page_x * page_width;
    int local_y = y & 63;
    int block_x = local_x >> 4;
    int block_y = local_y >> 4;
    int source = (local_y & 15) * 16 + (local_x & 15);
    int column = source >> 6;
    int output = column * 64 + kInverseLut.to_output[column & 1][source & 63];
    int page_row = block_y * 8 + (output >> 5);
    int page_column = block_x * 32 + (output & 31);
    if (page_row >= row_count || page_column >= row_bytes) {
        return -1;
    }
    return static_cast<long>(page_y) * pages_x * row_bytes * row_count + static_cast<long>(row_bytes) * page_x +
           static_cast<long>(page_row) * row_bytes * pages_x + page_column;
}

uint32_t Load32(const u_char *p) {
    uint32_t value;
    std::memcpy(&value, p, 4);
    return value;
}

void DecodeLevel(int bpp, unsigned width, unsigned height, const u_char *src, bool swizzled,
                 std::vector<uint8_t> &out) {
    size_t texels = static_cast<size_t>(width) * height;
    switch (bpp) {
        case 0:
            out.resize(texels);
            for (size_t i = 0; i < texels; i++) {
                u_char pair = src[i >> 1];
                out[i] = (i & 1) ? pair >> 4 : pair & 0xF;
            }
            break;
        case 1:
            out.resize(texels);
            if (swizzled) {
                PortUnswizzle8(static_cast<int>(width), static_cast<int>(height), src, out.data());
            } else {
                std::memcpy(out.data(), src, texels);
            }
            break;
        case 2: {
            // ABGR1555. The GS widens a channel by shifting it and takes alpha from TEXA by the A
            // bit; the game never programs TEXA for 16-bit images, so the usual TA0 0 / TA1 0x80
            // is baked in.
            out.resize(texels * 4);
            for (size_t i = 0; i < texels; i++) {
                unsigned pixel = src[i * 2] | src[i * 2 + 1] << 8;
                out[i * 4 + 0] = static_cast<uint8_t>((pixel & 0x1F) << 3);
                out[i * 4 + 1] = static_cast<uint8_t>(((pixel >> 5) & 0x1F) << 3);
                out[i * 4 + 2] = static_cast<uint8_t>(((pixel >> 10) & 0x1F) << 3);
                out[i * 4 + 3] = (pixel & 0x8000) ? 0x80 : 0;
            }
            gfx::ConvertPs2Alpha(reinterpret_cast<uint32_t *>(out.data()), texels);
            break;
        }
        case 3:
            out.resize(texels * 4);
            for (size_t i = 0; i < texels; i++) {
                out[i * 4 + 0] = src[i * 3 + 0];
                out[i * 4 + 1] = src[i * 3 + 1];
                out[i * 4 + 2] = src[i * 3 + 2];
                out[i * 4 + 3] = 0xFF;
            }
            break;
        case 4:
            out.assign(src, src + texels * 4);
            gfx::ConvertPs2Alpha(reinterpret_cast<uint32_t *>(out.data()), texels);
            break;
    }
}

bool IndexPsm(unsigned psm) {
    return psm == kPsmT8 || psm == kPsmT4 || psm == kPsmT8H || psm == kPsmT4HL || psm == kPsmT4HH;
}

// Which palette entry a CLUT image position holds, or -1 off the CLUT.
int ClutEntry(const Entry &palette, int x, int y) {
    if (x < 0 || y < 0 || x >= static_cast<int>(palette.width) || y >= static_cast<int>(palette.height)) {
        return -1;
    }
    if (palette.four_bit) {
        return y * 8 + x;
    }
    return static_cast<int>(PortClutCsm1(static_cast<unsigned>(y * 16 + x)));
}

struct Side {
    gfx::TextureHandle texture = gfx::kNullTexture;
    gfx::TextureFormat format = gfx::TextureFormat::Rgba8;
    Entry             *palette = nullptr;
};

bool ResolveSide(unsigned tbp, unsigned psm, Side &side) {
    if (tbp == kMGPortFrameTbp0) {
        side.texture = gfx::kMainTarget;
        return true;
    }
    if (tbp == kMGPortPreviousFrameTbp0) {
        side.texture = gfx::kPreviousFrame;
        return true;
    }
    Entry *entry = Live(tbp);
    if (entry == nullptr) {
        return false;
    }
    if (entry->kind == EntryKind::Palette) {
        side.palette = entry;
        return true;
    }
    // A baked image is RGBA whatever PSM the game copies it as, so an index PSM may name either; a
    // colour PSM never names an index image.
    if (!IndexPsm(psm) && entry->format == gfx::TextureFormat::Index8) {
        return false;
    }
    side.texture = EntryTexture(*entry);
    side.format = entry->format;
    return side.texture != gfx::kNullTexture;
}

} // namespace

bool PortLegacyTexturePath() {
    // An image the game read from its data was read through DataRead, which indexed it first. Before that
    // there are only images of the program's own (the texture tests' synthetic ones), which have no baked
    // document, and listing the files would stop the program when there is no data.
    if (!DataReadIndexed()) {
        return true;
    }
    static const bool legacy = [] {
        const std::vector<std::string> files = DataReadListFiles();
        const bool has_baked = std::any_of(files.begin(), files.end(), [](const std::string &path) {
            return path.starts_with("textures/") && path.ends_with(".json");
        });
        if (!has_baked) {
            std::fprintf(stderr, "texture: the data has no baked texture documents; decoding the raw textures\n");
        }
        return !has_baked;
    }();
    return legacy;
}

void PortRememberTextureClut(std::string_view name, const u_char *clut, std::size_t clut_bytes) {
    if (clut == nullptr || clut_bytes == 0) {
        return;
    }
    texture_cluts[std::string(name)] = std::vector<std::uint8_t>(clut, clut + clut_bytes);
}

bool PortLoadBakedTexture(std::string_view document, std::string_view name, unsigned picture_index,
                          unsigned width, unsigned height, unsigned level_count,
                          const u_char *clut, std::size_t clut_bytes,
                          PortDecodedTexture &out, std::string &error,
                          std::optional<unsigned> entry_index, int source_bpp) {
    // Declared before parsed/local buffers so successful timing includes their teardown.
    std::optional<BakedLoadTrace> trace;
    if (BakedLoadPerfEnabled()) {
        trace.emplace(document, name, picture_index);
    }
    (void)level_count;
    using Json = nlohmann::json;
    std::vector<std::uint8_t> json_bytes;
    const auto json_read_start = trace ? BakedLoadClock::now() : BakedLoadClock::time_point{};
    if (!DataReadReadFile(document, json_bytes)) {
        error = "missing texture document " + std::string(document);
        return false;
    }
    const auto json_read_end = trace ? BakedLoadClock::now() : BakedLoadClock::time_point{};
    bool        json_cache_hit = false;
    const auto  parsed = ParseBakedDocument(json_bytes, json_cache_hit);
    const Json &root = *parsed;
    if (root.is_discarded() || !root.is_object() || root.value("schema_version", 0) != 3 ||
        !root.contains("groups") || !root["groups"].is_array() || !root.contains("entries") ||
        !root["entries"].is_array()) {
        error = "invalid schema-3 texture document " + std::string(document);
        return false;
    }
    const auto json_schema_end = trace ? BakedLoadClock::now() : BakedLoadClock::time_point{};
    if (trace) {
        trace->json_bytes = json_bytes.size();
        trace->json_cache_hit = json_cache_hit;
        trace->json_read_ms = BakedLoadMilliseconds(json_read_start, json_read_end);
        trace->json_parse_schema_ms = BakedLoadMilliseconds(json_read_end, json_schema_end);
    }
    const Json *entry = nullptr;
    if (entry_index) {
        if (*entry_index >= root["entries"].size()) {
            error = "baked texture entry ordinal is out of range in " + std::string(document);
            return false;
        }
        const Json &candidate = root["entries"][*entry_index];
        if (!candidate.is_object() || candidate.value("name", std::string{}) != name) {
            error = "baked texture entry ordinal does not match " + std::string(name);
            return false;
        }
        entry = &candidate;
    } else {
        for (const Json &candidate : root["entries"]) {
            if (candidate.is_object() && candidate.value("name", std::string{}) == name) {
                if (entry != nullptr) {
                    error = "ambiguous texture entry " + std::string(name) + " in " + std::string(document);
                    return false;
                }
                entry = &candidate;
            }
        }
    }
    if (entry == nullptr || !entry->contains("pictures") || !(*entry)["pictures"].is_array() ||
        picture_index >= (*entry)["pictures"].size()) {
        error = "missing texture picture " + std::string(name) + " in " + std::string(document);
        return false;
    }
    const Json &picture = (*entry)["pictures"][picture_index];
    if (picture.is_object() && picture.contains("canvas") && !picture.contains("levels")) {
        error = "canvas texture keeps its runtime backing";
        return false;
    }
    if (!picture.is_object() || !picture.contains("levels") || !picture["levels"].is_array()) {
        error = "texture picture has no baked levels: " + std::string(document);
        return false;
    }
    const Json *levels = &picture["levels"];
    if (picture.contains("variants") && picture["variants"].is_array() && clut != nullptr && clut_bytes != 0) {
        const Json *selected = nullptr;
        std::string selected_name;
        for (const Json &variant : picture["variants"]) {
            if (!variant.is_object() || !variant.contains("selector") || !variant["selector"].is_string()) {
                continue;
            }
            const std::string selector = variant["selector"].get<std::string>();
            const auto palette = texture_cluts.find(selector);
            if (palette != texture_cluts.end() && palette->second.size() == clut_bytes &&
                std::memcmp(palette->second.data(), clut, clut_bytes) == 0) {
                if (selected != nullptr && selected_name != selector) {
                    error = "ambiguous baked palette variant for texture " + std::string(name);
                    return false;
                }
                selected = &variant;
                selected_name = selector;
            }
        }
        if (selected != nullptr) {
            if (!selected->contains("levels") || !(*selected)["levels"].is_array()) {
                error = "invalid baked palette variant for texture " + std::string(name);
                return false;
            }
            levels = &(*selected)["levels"];
        }
    }
    if (levels->empty()) {
        error = "baked texture has no mip levels: " + std::string(name);
        return false;
    }

    const std::size_t slash = document.rfind('/');
    const std::string parent = slash == std::string_view::npos ? std::string{} : std::string(document.substr(0, slash + 1));
    out = PortDecodedTexture{};
    out.format = gfx::TextureFormat::Rgba8;
    out.has_alpha = source_bpp < 0 || source_bpp != 3;
    for (unsigned level = 0; level < levels->size(); level++) {
        const Json &level_path = (*levels)[level];
        if (!level_path.is_string()) {
            error = "invalid baked level path in " + std::string(document);
            return false;
        }
        const std::string relative = level_path.get<std::string>();
        if (relative.empty() || relative.front() == '/' || relative.find('\\') != std::string::npos ||
            relative.find(':') != std::string::npos || relative.find("../") != std::string::npos ||
            relative == ".." || relative.starts_with("./")) {
            error = "unsafe baked level path " + relative;
            return false;
        }
        std::vector<std::uint8_t> png_bytes;
        const std::string png_path = parent + relative;
        const auto png_read_start = trace ? BakedLoadClock::now() : BakedLoadClock::time_point{};
        if (!DataReadReadFile(png_path, png_bytes)) {
            error = "missing baked PNG " + png_path;
            return false;
        }
        const auto png_read_end = trace ? BakedLoadClock::now() : BakedLoadClock::time_point{};
        if (trace) {
            ++trace->png_reads;
            trace->png_bytes += png_bytes.size();
            trace->png_read_ms += BakedLoadMilliseconds(png_read_start, png_read_end);
        }
        int decoded_width = 0;
        int decoded_height = 0;
        std::vector<std::uint8_t> rgba;
        const auto png_decode_start = trace ? BakedLoadClock::now() : BakedLoadClock::time_point{};
        bool                      png_cache_hit = false;
        if (!PngDecodeRgba(png_bytes, decoded_width, decoded_height, rgba, &png_cache_hit)) {
            error = "invalid baked PNG " + png_path;
            return false;
        }
        const auto png_decode_end = trace ? BakedLoadClock::now() : BakedLoadClock::time_point{};
        if (trace) {
            trace->png_decodes += !png_cache_hit;
            trace->png_cache_hits += png_cache_hit;
            trace->rgba_bytes += rgba.size();
            trace->png_decode_ms += BakedLoadMilliseconds(png_decode_start, png_decode_end);
        }
        if (level == 0) {
            out.width = width == 0 ? static_cast<unsigned>(decoded_width) : width;
            out.height = height == 0 ? static_cast<unsigned>(decoded_height) : height;
        }
        const unsigned expected_width = std::max(1u, out.width >> level);
        const unsigned expected_height = std::max(1u, out.height >> level);
        if (decoded_width != static_cast<int>(expected_width) ||
            decoded_height != static_cast<int>(expected_height)) {
            error = "baked PNG dimensions differ for " + png_path;
            return false;
        }
        out.levels.push_back(std::move(rgba));
    }
    if (trace) {
        trace->success = true;
    }
    return true;
}

void PortUnswizzle8(int width, int height, const u_char *swizzled, u_char *linear) {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            long offset = SwizzledOffset(width, height, x, y);
            linear[static_cast<size_t>(y) * width + x] = offset < 0 ? 0 : swizzled[offset];
        }
    }
}

bool PortDecodeTexture(int bpp, int width, int height, const u_char *const *levels, int level_count,
                       const u_char *clut, int clut_colors, bool swizzled, PortDecodedTexture &out) {
    if (bpp < 0 || bpp > 4 || width <= 0 || height <= 0 || levels == nullptr || levels[0] == nullptr ||
        level_count <= 0) {
        return false;
    }
    out = PortDecodedTexture{};
    out.width = static_cast<unsigned>(width);
    out.height = static_cast<unsigned>(height);
    out.format = bpp < 2 ? gfx::TextureFormat::Index8 : gfx::TextureFormat::Rgba8;
    out.has_alpha = bpp != 3;
    out.four_bit = bpp == 0;
    for (int level = 0; level < level_count && levels[level] != nullptr; level++) {
        unsigned w = std::max(1u, out.width >> level);
        unsigned h = std::max(1u, out.height >> level);
        out.levels.emplace_back();
        DecodeLevel(bpp, w, h, levels[level], swizzled, out.levels.back());
    }
    if (bpp < 2) {
        if (clut == nullptr) {
            return false;
        }
        unsigned limit = bpp == 0 ? 16 : 256;
        unsigned colors = clut_colors > 0 && static_cast<unsigned>(clut_colors) < limit ? clut_colors : limit;
        // Retail sends the CLUT to the GS as is, as a 16x16 PSMCT32 image read with CSM1, so the
        // file's order is the CSM1 image order and the hardware's lookup is reproduced here. A
        // 4-bit CLUT is an 8x2 image, which CSM1 reads in plain order.
        for (unsigned position = 0; position < colors; position++) {
            unsigned index = bpp == 0 ? position : PortClutCsm1(position);
            out.palette[index] = Load32(clut + position * 4);
        }
        gfx::ConvertPs2Alpha(out.palette.data(), out.palette.size());
    }
    return true;
}

unsigned PortCreateTexture(const PortDecodedTexture &decoded, PortTextureOwner owner, unsigned *palette_key) {
    const dc::load_profile::Span profile(dc::load_profile::Kind::Texture);
    if (palette_key) {
        *palette_key = 0;
    }
    if (decoded.levels.empty()) {
        return 0;
    }
    unsigned       key = AllocateKey(EntryKind::Image, owner);
    Entry         &entry = Reg().entries[key];
    entry.width = decoded.width;
    entry.height = decoded.height;
    entry.format = decoded.format;
    entry.has_alpha = decoded.has_alpha;
    if (RendererUp()) {
        gfx::TextureDesc desc;
        desc.width = decoded.width;
        desc.height = decoded.height;
        desc.format = decoded.format;
        desc.mip_levels = static_cast<uint32_t>(decoded.levels.size());
        desc.has_alpha = decoded.has_alpha;
        entry.texture = gfx::CreateTexture(desc);
        entry.owns_texture = true;
        for (size_t level = 0; entry.texture != gfx::kNullTexture && level < decoded.levels.size(); level++) {
            uint32_t w = std::max(1u, decoded.width >> level);
            uint32_t h = std::max(1u, decoded.height >> level);
            gfx::UpdateTexture(entry.texture, static_cast<uint32_t>(level), 0, 0, w, h,
                               decoded.levels[level].data());
        }
    }
    if (decoded.format == gfx::TextureFormat::Index8) {
        unsigned palette = RegisterPalette(decoded.palette, decoded.four_bit, owner);
        Reg().entries[key].palette = palette;
        if (palette_key) {
            *palette_key = palette;
        }
    }
    return key;
}

unsigned PortRegisterNamedTarget(std::string_view name, unsigned width, unsigned height, bool has_alpha,
                                 PortTextureOwner owner) {
    unsigned key = AllocateKey(EntryKind::Image, owner);
    Entry   &entry = Reg().entries[key];
    entry.target = name;
    entry.width = width;
    entry.height = height;
    entry.has_alpha = has_alpha;
    if (RendererUp()) {
        gfx::NamedRenderTarget(name, width, height, has_alpha, false, MGPortFrameTarget(name));
    }
    return key;
}

unsigned PortCreatePlaceholder(std::string_view name, unsigned width, unsigned height, int bpp,
                               PortTextureOwner owner, unsigned *palette_key) {
    if (palette_key) {
        *palette_key = 0;
    }
    if (bpp < 2) {
        PortDecodedTexture blank;
        blank.width = width;
        blank.height = height;
        blank.format = gfx::TextureFormat::Index8;
        blank.four_bit = bpp == 0;
        blank.levels.emplace_back(static_cast<size_t>(width) * height, 0);
        return PortCreateTexture(blank, owner, palette_key);
    }
    bool        has_alpha = bpp != 3;
    std::string target(name);
    // gfx keeps one target per name and recreates it at a new size, so a second live placeholder
    // under the same name at another size gets the placeholder's full spelling as its own name.
    for (const Entry &entry : Reg().entries) {
        if (entry.kind == EntryKind::Image && entry.target == target &&
            (entry.width != width || entry.height != height || entry.has_alpha != has_alpha)) {
            target = "#" + std::string(name) + "#" + std::to_string(width) + "#" + std::to_string(height) + "#" +
                     std::to_string(bpp);
            break;
        }
    }
    return PortRegisterNamedTarget(target, width, height, has_alpha, owner);
}

void PortReleaseKey(unsigned key) {
    Entry *entry = Live(key);
    if (entry == nullptr) {
        return;
    }
    if (entry->owns_texture && entry->texture != gfx::kNullTexture && RendererUp()) {
        gfx::DestroyTexture(entry->texture);
    }
    *entry = Entry{};
}

void PortReleaseOwner(PortTextureOwner owner) {
    for (unsigned key = 0; key < kKeyCount; key++) {
        if (Reg().entries[key].kind != EntryKind::Free && Reg().entries[key].owner == owner) {
            PortReleaseKey(key);
        }
    }
}

bool PortKeyLive(unsigned key) {
    return Live(key) != nullptr;
}

PortTextureRef PortTextureFromTex0(u_long tex0, u_long tex1) {
    PortTextureRef ref;
    unsigned       tbp = tex0 & 0x3FFF;
    unsigned       cbp = (tex0 >> 37) & 0x3FFF;
    ref.binding.filter = (tex1 >> 5) & 1 ? gfx::Filter::Linear : gfx::Filter::Nearest;
    if (tbp == kMGPortFrameTbp0 || tbp == kMGPortPreviousFrameTbp0) {
        ref.binding.texture = tbp == kMGPortFrameTbp0 ? gfx::kMainTarget : gfx::kPreviousFrame;
        ref.width = static_cast<unsigned>(kScreenWidth);
        ref.height = static_cast<unsigned>(kScreenHeight);
        ref.valid = true;
        return ref;
    }
    const Entry *entry = Live(tbp);
    if (entry == nullptr || entry->kind != EntryKind::Image) {
        return ref;
    }
    ref.binding.texture = EntryTexture(*entry);
    ref.width = entry->width;
    ref.height = entry->height;
    if (entry->format == gfx::TextureFormat::Index8) {
        // The game draws one image with another's CLUT by changing CBP alone (text colours,
        // SetClut), so CBP picks the palette; the image's own is the fallback.
        const Entry *palette = Live(cbp);
        if (palette == nullptr || palette->kind != EntryKind::Palette) {
            palette = Live(entry->palette);
        }
        ref.binding.palette = palette ? palette->texture : gfx::kNullTexture;
        ref.valid = ref.binding.texture != gfx::kNullTexture && ref.binding.palette != gfx::kNullTexture;
        return ref;
    }
    ref.valid = ref.binding.texture != gfx::kNullTexture;
    return ref;
}

PortTextureRef PortTextureFromTex0(const sceGsTex0 &tex0, u_long tex1) {
    u_long bits;
    std::memcpy(&bits, &tex0, sizeof bits);
    return PortTextureFromTex0(bits, tex1);
}

PortTextureRef PortTextureFromHandle(int handle) {
    return PortTextureFromCTexture(TexManager.GetTexture(handle));
}

PortTextureRef PortTextureFromCTexture(const CTexture *texture) {
    if (texture == nullptr) {
        return {};
    }
    return PortTextureFromTex0(texture->tex0, texture->tex1);
}

bool PortMoveImage(unsigned src_tbp, unsigned src_psm, int x, int y, int width, int height, unsigned dst_tbp,
                   unsigned dst_psm, int dst_x, int dst_y) {
    if (width <= 0 || height <= 0) {
        return false;
    }
    Side src;
    Side dst;
    if (!ResolveSide(src_tbp, src_psm, src) || !ResolveSide(dst_tbp, dst_psm, dst)) {
        static bool reported;
        ReportOnce(reported, "move between images that are not registered or not of their PSM", src_tbp, dst_tbp);
        return false;
    }
    if (src.palette && dst.palette) {
        std::array<uint32_t, 256>      &to = *dst.palette->shadow;
        const std::array<uint32_t, 256> from = *src.palette->shadow;
        for (int row = 0; row < height; row++) {
            for (int column = 0; column < width; column++) {
                int source = ClutEntry(*src.palette, x + column, y + row);
                int target = ClutEntry(*dst.palette, dst_x + column, dst_y + row);
                if (source >= 0 && target >= 0) {
                    to[target] = from[source];
                }
            }
        }
        if (dst.palette->texture != gfx::kNullTexture) {
            gfx::UpdatePalette(dst.palette->texture, to.data());
        }
        return true;
    }
    if (src.palette || dst.palette || src.format != dst.format) {
        static bool reported;
        ReportOnce(reported, "move between a palette and an image, or an index and a colour image", src_tbp,
                   dst_tbp);
        return false;
    }
    return gfx::CopyTexture(src.texture, gfx::Rect{x, y, width, height}, dst.texture, dst_x, dst_y);
}

bool PortMoveImage(const sceGsTex0 &src, int x, int y, int width, int height, const sceGsTex0 &dst, int dst_x,
                   int dst_y) {
    return PortMoveImage(src.TBP0, src.PSM, x, y, width, height, dst.TBP0, dst.PSM, dst_x, dst_y);
}

bool PortLoadClut(unsigned cbp, const u_int *clut) {
    Entry *entry = Live(cbp);
    if (entry == nullptr || entry->kind != EntryKind::Palette || clut == nullptr) {
        return false;
    }
    std::array<uint32_t, 256> &rgba = *entry->shadow;
    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 16; x++) {
            int index = ClutEntry(*entry, x, y);
            if (index >= 0) {
                uint32_t entry_rgba = clut[y * 16 + x];
                gfx::ConvertPs2Alpha(&entry_rgba, 1);
                rgba[index] = entry_rgba;
            }
        }
    }
    if (entry->texture != gfx::kNullTexture) {
        gfx::UpdatePalette(entry->texture, rgba.data());
    }
    return true;
}

bool PortPaletteEntries(unsigned cbp, uint32_t *rgba) {
    const Entry *entry = Live(cbp);
    if (entry == nullptr || entry->kind != EntryKind::Palette) {
        return false;
    }
    std::memcpy(rgba, entry->shadow->data(), 256 * sizeof(uint32_t));
    return true;
}
