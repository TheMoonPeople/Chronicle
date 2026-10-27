// clang-format off
// nlohmann/json must precede libgraph.h, whose B macro breaks its templates.
#include <nlohmann/json.hpp>
// clang-format on

#include "render/texture_registry.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cctype>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <optional>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "data/data_read.hpp"
#include "platform/load_work_profile.hpp"
#include "platform/png_image.hpp"
#include "render/gs_state.hpp"
#include "render/screen.hpp"
#include "texture.hpp"

namespace dc::render {

using namespace dc::data;
using namespace dc::game;
using namespace dc::platform;

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

// A texture bundle commonly supplies several consecutive pictures. Keep only the last small document per loading
// thread, and still read and compare its complete bytes each time.
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
    cache_hit = enabled && cached.parsed && std::ranges::equal(bytes, cached.bytes);
    if (cache_hit) {
        return cached.parsed;
    }
    auto parsed =
        std::make_shared<const nlohmann::json>(nlohmann::json::parse(bytes.begin(), bytes.end(), nullptr, false));
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

// The timings and sizes of one baked texture load, printed when the load succeeds.
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

    BakedLoadTrace(std::string_view traced_document, std::string_view traced_name, unsigned traced_picture)
        : document(traced_document), name(traced_name), picture_index(traced_picture),
          started(BakedLoadClock::now()) {}

    BakedLoadTrace(const BakedLoadTrace &) = delete;
    BakedLoadTrace &operator=(const BakedLoadTrace &) = delete;

    ~BakedLoadTrace() {
        if (!success) {
            return;
        }
        const double          total_ms = BakedLoadMilliseconds(started, BakedLoadClock::now());
        const double          other_ms = total_ms - json_read_ms - json_parse_schema_ms - png_read_ms - png_decode_ms;
        const auto            cache = PngDecodeCacheGetStats();
        constexpr std::size_t kMaxText = 512;
        std::fprintf(stderr,
                     "perf load texture success picture %u mips %zu json_reads 1 json_bytes %zu "
                     "png_reads %zu png_bytes %llu png_decodes %zu rgba_bytes %llu total_wall_ms %.6f "
                     "json_read_wall_ms %.6f json_parse_schema_wall_ms %.6f png_read_wall_ms %.6f "
                     "png_decode_wall_ms %.6f selection_assembly_other_wall_ms %.6f png_cache_hits %zu "
                     "png_cache_resident_bytes %zu png_cache_entries %zu json_cache_hit %d "
                     "doc_len %zu name_len %zu doc %.*s name %.*s\n",
                     picture_index, png_reads, json_bytes, png_reads, static_cast<unsigned long long>(png_bytes),
                     png_decodes, static_cast<unsigned long long>(rgba_bytes), total_ms, json_read_ms,
                     json_parse_schema_ms, png_read_ms, png_decode_ms, other_ms, png_cache_hits, cache.resident_bytes,
                     cache.entries, json_cache_hit, document.size(), name.size(),
                     static_cast<int>(std::min(document.size(), kMaxText)), document.empty() ? "" : document.data(),
                     static_cast<int>(std::min(name.size(), kMaxText)), name.empty() ? "" : name.data());
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
    std::println(stderr, "texture: every TBP0/CBP key is in use; textures are leaking");
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
    std::array<std::array<uint8_t, 64>, 2> to_output{};

    constexpr InverseLut() {
        constexpr std::array<uint8_t, 128> lut = {
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

uint32_t Load32(const u_char *bytes) {
    uint32_t value = 0;
    std::memcpy(&value, bytes, sizeof value);
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
                out[i] = static_cast<uint8_t>((i & 1) != 0 ? pair >> 4 : pair & 0xF);
            }
            break;
        case 1:
            out.resize(texels);
            if (swizzled) {
                PortUnswizzle8(static_cast<int>(width), static_cast<int>(height), src, out.data());
            } else {
                std::copy_n(src, texels, out.data());
            }
            break;
        case 2: {
            // ABGR1555. The GS widens a channel by shifting it and takes alpha from TEXA by the A
            // bit; the game never programs TEXA for 16-bit images, so the usual TA0 0 / TA1 0x80
            // is baked in.
            out.resize(texels * 4);
            for (size_t i = 0; i < texels; i++) {
                const unsigned pixel = src[i * 2] | (src[i * 2 + 1] << 8);
                out[i * 4 + 0] = static_cast<uint8_t>((pixel & 0x1F) << 3);
                out[i * 4 + 1] = static_cast<uint8_t>(((pixel >> 5) & 0x1F) << 3);
                out[i * 4 + 2] = static_cast<uint8_t>(((pixel >> 10) & 0x1F) << 3);
                out[i * 4 + 3] = (pixel & 0x8000) != 0 ? 0x80 : 0;
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
        default:
            break;
    }
}

bool IndexPsm(unsigned psm) {
    return psm == kPsmT8 || psm == kPsmT4 || psm == kPsmT8H || psm == kPsmT4HL || psm == kPsmT4HH;
}

// Which palette entry a CLUT image position holds, or -1 off the CLUT.
int ClutEntry(const Entry &palette, int x, int y) {
    if (x < 0 || y < 0 || std::cmp_greater_equal(x, palette.width) || std::cmp_greater_equal(y, palette.height)) {
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
    // An image the game read from its data was read through DataRead, which indexed it first. Before that there are
    // only the program's own images (the texture tests' synthetic ones), which have no baked document, and listing the
    // files would stop the program when there is no data.
    if (!DataReadIndexed()) {
        return true;
    }
    static const bool legacy = [] {
        const std::vector<std::string> files = DataReadListFiles();
        const bool                     has_baked = std::ranges::any_of(files, [](const std::string &path) {
            return path.starts_with("textures/") && path.ends_with(".json");
        });
        if (!has_baked) {
            std::println(stderr, "texture: the data has no baked texture documents; decoding the raw textures");
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
                          unsigned width, unsigned height,
                          [[maybe_unused]] unsigned level_count, const u_char *clut, std::size_t clut_bytes,
                          PortDecodedTexture &out, std::string &error,
                          std::optional<unsigned> entry_index, int source_bpp) {
    // Declared before the parsed document and local buffers so a successful load's timing includes their teardown.
    std::optional<BakedLoadTrace> trace;
    if (BakedLoadPerfEnabled()) {
        trace.emplace(document, name, picture_index);
    }
    using Json = nlohmann::json;
    std::vector<std::uint8_t> json_bytes;
    const auto                json_read_start = trace ? BakedLoadClock::now() : BakedLoadClock::time_point{};
    if (!DataReadReadFile(document, json_bytes)) {
        error = "missing texture document " + std::string(document);
        return false;
    }
    const auto  json_read_end = trace ? BakedLoadClock::now() : BakedLoadClock::time_point{};
    bool        json_cache_hit = false;
    const auto  parsed = ParseBakedDocument(json_bytes, json_cache_hit);
    const Json &root = *parsed;
    if (root.is_discarded() || !root.is_object() || root.value("schema_version", 0) != 3 ||
        !root.contains("groups") || !root["groups"].is_array() || !root.contains("entries") ||
        !root["entries"].is_array()) {
        error = "invalid schema-3 texture document " + std::string(document);
        return false;
    }
    if (trace) {
        trace->json_bytes = json_bytes.size();
        trace->json_cache_hit = json_cache_hit;
        trace->json_read_ms = BakedLoadMilliseconds(json_read_start, json_read_end);
        trace->json_parse_schema_ms = BakedLoadMilliseconds(json_read_end, BakedLoadClock::now());
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
            const auto        palette = texture_cluts.find(selector);
            if (palette != texture_cluts.end() &&
                std::ranges::equal(palette->second, std::span<const u_char>(clut, clut_bytes))) {
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
    out.has_alpha = source_bpp != 3;
    for (unsigned level = 0; level < levels->size(); level++) {
        const Json &level_path = (*levels)[level];
        if (!level_path.is_string()) {
            error = "invalid baked level path in " + std::string(document);
            return false;
        }
        const std::string relative = level_path.get<std::string>();
        if (relative.empty() || relative.front() == '/' || relative.contains('\\') || relative.contains(':') ||
            relative.contains("../") || relative == ".." || relative.starts_with("./")) {
            error = "unsafe baked level path " + relative;
            return false;
        }
        std::vector<std::uint8_t> png_bytes;
        const std::string         png_path = parent + relative;
        const auto                png_read_start = trace ? BakedLoadClock::now() : BakedLoadClock::time_point{};
        if (!DataReadReadFile(png_path, png_bytes)) {
            error = "missing baked PNG " + png_path;
            return false;
        }
        if (trace) {
            ++trace->png_reads;
            trace->png_bytes += png_bytes.size();
            trace->png_read_ms += BakedLoadMilliseconds(png_read_start, BakedLoadClock::now());
        }
        int                       decoded_width = 0;
        int                       decoded_height = 0;
        std::vector<std::uint8_t> rgba;
        const auto                png_decode_start = trace ? BakedLoadClock::now() : BakedLoadClock::time_point{};
        bool                      png_cache_hit = false;
        if (!PngDecodeRgba(png_bytes, decoded_width, decoded_height, rgba, &png_cache_hit)) {
            error = "invalid baked PNG " + png_path;
            return false;
        }
        if (trace) {
            trace->png_decodes += png_cache_hit ? 0 : 1;
            trace->png_cache_hits += png_cache_hit ? 1 : 0;
            trace->rgba_bytes += rgba.size();
            trace->png_decode_ms += BakedLoadMilliseconds(png_decode_start, BakedLoadClock::now());
        }
        if (level == 0) {
            out.width = width == 0 ? static_cast<unsigned>(decoded_width) : width;
            out.height = height == 0 ? static_cast<unsigned>(decoded_height) : height;
        }
        const unsigned expected_width = std::max(1u, out.width >> level);
        const unsigned expected_height = std::max(1u, out.height >> level);
        if (std::cmp_not_equal(decoded_width, expected_width) || std::cmp_not_equal(decoded_height, expected_height)) {
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
            const long offset = SwizzledOffset(width, height, x, y);
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
        const unsigned level_width = std::max(1u, out.width >> level);
        const unsigned level_height = std::max(1u, out.height >> level);
        out.levels.emplace_back();
        DecodeLevel(bpp, level_width, level_height, levels[level], swizzled, out.levels.back());
    }
    if (bpp < 2) {
        if (clut == nullptr) {
            return false;
        }
        const unsigned limit = bpp == 0 ? 16 : 256;
        const unsigned colors =
            clut_colors > 0 && std::cmp_less(clut_colors, limit) ? static_cast<unsigned>(clut_colors) : limit;
        // Retail sends the CLUT to the GS as is, as a 16x16 PSMCT32 image read with CSM1, so the
        // file's order is the CSM1 image order and the hardware's lookup is reproduced here. A
        // 4-bit CLUT is an 8x2 image, which CSM1 reads in plain order.
        for (unsigned position = 0; position < colors; position++) {
            const unsigned index = bpp == 0 ? position : PortClutCsm1(position);
            out.palette[index] = Load32(&clut[static_cast<size_t>(position) * 4]);
        }
        gfx::ConvertPs2Alpha(out.palette.data(), out.palette.size());
    }
    return true;
}

unsigned PortCreateTexture(const PortDecodedTexture &decoded, PortTextureOwner owner, unsigned *palette_key) {
    const platform::load_profile::Span profile(platform::load_profile::Kind::Texture);
    if (palette_key != nullptr) {
        *palette_key = 0;
    }
    if (decoded.levels.empty()) {
        return 0;
    }
    unsigned key = AllocateKey(EntryKind::Image, owner);
    Entry   &entry = Reg().entries[key];
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
            const uint32_t level_width = std::max(1u, decoded.width >> level);
            const uint32_t level_height = std::max(1u, decoded.height >> level);
            gfx::UpdateTexture(entry.texture, static_cast<uint32_t>(level), 0, 0, level_width, level_height,
                               decoded.levels[level].data());
        }
    }
    if (decoded.format == gfx::TextureFormat::Index8) {
        unsigned palette = RegisterPalette(decoded.palette, decoded.four_bit, owner);
        Reg().entries[key].palette = palette;
        if (palette_key != nullptr) {
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
    if (palette_key != nullptr) {
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
    const auto     tbp = static_cast<unsigned>(tex0 & 0x3FFF);
    const auto     cbp = static_cast<unsigned>((tex0 >> 37) & 0x3FFF);
    ref.binding.filter = ((tex1 >> 5) & 1) != 0 ? gfx::Filter::Linear : gfx::Filter::Nearest;
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
        ref.binding.palette = palette != nullptr ? palette->texture : gfx::kNullTexture;
        ref.valid = ref.binding.texture != gfx::kNullTexture && ref.binding.palette != gfx::kNullTexture;
        return ref;
    }
    ref.valid = ref.binding.texture != gfx::kNullTexture;
    return ref;
}

PortTextureRef PortTextureFromTex0(const sceGsTex0 &tex0, u_long tex1) {
    return PortTextureFromTex0(std::bit_cast<u_long>(tex0), tex1);
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
    if (src.palette != nullptr && dst.palette != nullptr) {
        std::array<uint32_t, 256>      &to = *dst.palette->shadow;
        const std::array<uint32_t, 256> from = *src.palette->shadow;
        for (int row = 0; row < height; row++) {
            for (int column = 0; column < width; column++) {
                const int source = ClutEntry(*src.palette, x + column, y + row);
                const int target = ClutEntry(*dst.palette, dst_x + column, dst_y + row);
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
    if (src.palette != nullptr || dst.palette != nullptr || src.format != dst.format) {
        static bool reported;
        ReportOnce(reported, "move between a palette and an image, or an index and a colour image", src_tbp,
                   dst_tbp);
        return false;
    }
    return gfx::CopyTexture(src.texture, gfx::Rect{.x = x, .y = y, .w = width, .h = height}, dst.texture, dst_x, dst_y);
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
            const int index = ClutEntry(*entry, x, y);
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
    std::ranges::copy(*entry->shadow, rgba);
    return true;
}

namespace {

std::string Sha256Hex(std::string_view input) {
    constexpr std::array<std::uint32_t, 64> round_constants = {
        0x428a2f98,
        0x71374491,
        0xb5c0fbcf,
        0xe9b5dba5,
        0x3956c25b,
        0x59f111f1,
        0x923f82a4,
        0xab1c5ed5,
        0xd807aa98,
        0x12835b01,
        0x243185be,
        0x550c7dc3,
        0x72be5d74,
        0x80deb1fe,
        0x9bdc06a7,
        0xc19bf174,
        0xe49b69c1,
        0xefbe4786,
        0x0fc19dc6,
        0x240ca1cc,
        0x2de92c6f,
        0x4a7484aa,
        0x5cb0a9dc,
        0x76f988da,
        0x983e5152,
        0xa831c66d,
        0xb00327c8,
        0xbf597fc7,
        0xc6e00bf3,
        0xd5a79147,
        0x06ca6351,
        0x14292967,
        0x27b70a85,
        0x2e1b2138,
        0x4d2c6dfc,
        0x53380d13,
        0x650a7354,
        0x766a0abb,
        0x81c2c92e,
        0x92722c85,
        0xa2bfe8a1,
        0xa81a664b,
        0xc24b8b70,
        0xc76c51a3,
        0xd192e819,
        0xd6990624,
        0xf40e3585,
        0x106aa070,
        0x19a4c116,
        0x1e376c08,
        0x2748774c,
        0x34b0bcb5,
        0x391c0cb3,
        0x4ed8aa4a,
        0x5b9cca4f,
        0x682e6ff3,
        0x748f82ee,
        0x78a5636f,
        0x84c87814,
        0x8cc70208,
        0x90befffa,
        0xa4506ceb,
        0xbef9a3f7,
        0xc67178f2,
    };
    std::array<std::uint32_t, 8> state = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                                          0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    const auto                   rotate = [](std::uint32_t value, unsigned amount) {
        return (value >> amount) | (value << (32 - amount));
    };
    std::vector<std::uint8_t> bytes(input.begin(), input.end());
    const std::uint64_t       bit_length = static_cast<std::uint64_t>(bytes.size()) * 8;
    bytes.push_back(0x80);
    while (bytes.size() % 64 != 56) {
        bytes.push_back(0);
    }
    for (int shift = 56; shift >= 0; shift -= 8) {
        bytes.push_back(static_cast<std::uint8_t>(bit_length >> shift));
    }
    for (std::size_t block = 0; block < bytes.size(); block += 64) {
        std::array<std::uint32_t, 64> words{};
        for (unsigned index = 0; index < 16; index++) {
            const std::size_t at = block + (static_cast<std::size_t>(index) * 4);
            words[index] = (static_cast<std::uint32_t>(bytes[at]) << 24) |
                           (static_cast<std::uint32_t>(bytes[at + 1]) << 16) |
                           (static_cast<std::uint32_t>(bytes[at + 2]) << 8) | bytes[at + 3];
        }
        for (unsigned index = 16; index < 64; index++) {
            const std::uint32_t s0 = rotate(words[index - 15], 7) ^ rotate(words[index - 15], 18) ^
                                     (words[index - 15] >> 3);
            const std::uint32_t s1 = rotate(words[index - 2], 17) ^ rotate(words[index - 2], 19) ^
                                     (words[index - 2] >> 10);
            words[index] = words[index - 16] + s0 + words[index - 7] + s1;
        }
        std::uint32_t a = state[0];
        std::uint32_t b = state[1];
        std::uint32_t c = state[2];
        std::uint32_t d = state[3];
        std::uint32_t e = state[4];
        std::uint32_t f = state[5];
        std::uint32_t g = state[6];
        std::uint32_t h = state[7];
        for (unsigned index = 0; index < 64; index++) {
            const std::uint32_t s1 = rotate(e, 6) ^ rotate(e, 11) ^ rotate(e, 25);
            const std::uint32_t choose = (e & f) ^ (~e & g);
            const std::uint32_t temp1 = h + s1 + choose + round_constants[index] + words[index];
            const std::uint32_t s0 = rotate(a, 2) ^ rotate(a, 13) ^ rotate(a, 22);
            const std::uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
            const std::uint32_t temp2 = s0 + majority;
            h = g;
            g = f;
            f = e;
            e = d + temp1;
            d = c;
            c = b;
            b = a;
            a = temp1 + temp2;
        }
        state[0] += a;
        state[1] += b;
        state[2] += c;
        state[3] += d;
        state[4] += e;
        state[5] += f;
        state[6] += g;
        state[7] += h;
    }
    constexpr char hex_digits[] = "0123456789abcdef";
    std::string    result;
    result.reserve(64);
    for (std::uint32_t word : state) {
        for (int shift = 28; shift >= 0; shift -= 4) {
            result.push_back(hex_digits[(word >> shift) & 0xF]);
        }
    }
    return result;
}

std::string Dc1SourceTag(std::string_view alias) {
    std::uint64_t hash = 0xcbf29ce484222325ULL;
    for (unsigned char byte : alias) {
        hash ^= byte;
        hash *= 0x100000001b3ULL;
    }
    constexpr char hex_digits[] = "0123456789abcdef";
    std::string    result(16, '0');
    for (int index = 15; index >= 0; index--) {
        result[static_cast<std::size_t>(index)] = hex_digits[hash & 0xF];
        hash >>= 4;
    }
    return result;
}

bool TextureSourceExtension(std::string_view extension) {
    if (extension == "img" || extension == "tm2" || extension == "im2" || extension == "im3" ||
        extension == "tim2") {
        return true;
    }
    return std::ranges::any_of(std::array<std::string_view, 2>{"img", "tm2"}, [extension](std::string_view base) {
        return extension.starts_with(base) && extension.size() > base.size() &&
               extension.substr(base.size()).find_first_not_of('_') == std::string_view::npos;
    });
}

std::string SafeTextureComponent(std::string_view component) {
    if (component == "." || component == "..") {
        std::string escaped;
        for (std::size_t index = 0; index < component.size(); index++) {
            escaped += "%2E";
        }
        return escaped;
    }
    constexpr char hex[] = "0123456789ABCDEF";
    std::string    result;
    for (unsigned char byte : component) {
        if ((byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z') ||
            (byte >= '0' && byte <= '9') || byte == '.' || byte == '_' || byte == '-') {
            result.push_back(static_cast<char>(byte));
        } else {
            result.push_back('%');
            result.push_back(hex[byte >> 4]);
            result.push_back(hex[byte & 0xF]);
        }
    }
    return result;
}

std::string LegacyTextureDocument(std::string_view key) {
    std::vector<std::string> raw;
    for (size_t start = 0; start < key.size();) {
        const size_t end = key.find('/', start);
        const size_t stop = end == std::string_view::npos ? key.size() : end;
        if (stop > start) {
            raw.emplace_back(key.substr(start, stop - start));
        }
        if (end == std::string_view::npos) {
            break;
        }
        start = end + 1;
    }
    for (size_t index = 0; index < raw.size();) {
        const size_t offset = raw[index].find("#offset:");
        if (offset != std::string::npos) {
            const std::string name = raw[index].substr(0, offset);
            const std::string value = raw[index].substr(offset + 8);
            raw[index] = name;
            raw.insert(raw.begin() + static_cast<std::ptrdiff_t>(index + 1), "embedded-offset-" + value + ".json");
            continue;
        }
        const size_t entry = raw[index].find("#entry:");
        if (entry != std::string::npos) {
            const std::string name = raw[index].substr(0, entry);
            const std::string ordinal = raw[index].substr(entry + 7);
            const size_t      dot = name.rfind('.');
            raw[index] = (dot == std::string::npos ? name : name.substr(0, dot)) + "-entry-" + ordinal + ".json";
        }
        index++;
    }
    if (raw.empty()) {
        return {};
    }
    std::vector<std::string> parts;
    parts.reserve(raw.size() + 1);
    for (size_t index = 0; index < raw.size(); index++) {
        std::string part = SafeTextureComponent(raw[index]);
        if (index + 1 < raw.size()) {
            if (part.size() >= 4 && part.substr(part.size() - 4) == ".img") {
                part.resize(part.size() - 4);
                part += "-img";
            }
        } else {
            std::string lower = part;
            std::ranges::transform(lower, lower.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            if (lower.ends_with(".tm2") || lower.ends_with(".img")) {
                part.resize(part.size() - 4);
                part += ".json";
            } else if (!lower.ends_with(".json")) {
                parts.push_back(std::move(part));
                parts.emplace_back("embedded.json");
                continue;
            }
        }
        parts.push_back(std::move(part));
    }
    std::string result;
    for (const std::string &part : parts) {
        if (!result.empty()) {
            result.push_back('/');
        }
        result += part;
    }
    return result;
}

std::string Dc1TextureDocument(std::string_view alias) {
    const size_t           slash = alias.rfind('/');
    const std::string_view leaf = alias.substr(slash == std::string_view::npos ? 0 : slash + 1);
    const size_t           marker = leaf.find('#');
    const std::string_view source_name = leaf.substr(0, marker);
    const size_t           dot = source_name.rfind('.');
    std::string            extension = dot == std::string_view::npos ? std::string{} : std::string(source_name.substr(dot + 1));
    std::ranges::transform(extension, extension.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    std::string path;
    if (!TextureSourceExtension(extension)) {
        path = LegacyTextureDocument(alias);
    } else {
        std::string projected(alias);
        if (extension != "img" && extension != "tm2") {
            const size_t alias_marker = projected.find('#');
            const size_t source_end = alias_marker == std::string::npos ? projected.size() : alias_marker;
            const size_t source_dot = projected.rfind('.', source_end);
            if (source_dot == std::string::npos) {
                return {};
            }
            projected.replace(source_dot, source_end - source_dot, ".img");
        }
        path = LegacyTextureDocument(projected);
        if (path.size() < 5 || path.substr(path.size() - 5) != ".json") {
            return {};
        }
        path.resize(path.size() - 5);
        path += "_" + extension + ".json";
    }

    std::vector<std::string> raw;
    for (size_t start = 0; start < alias.size();) {
        const size_t end = alias.find('/', start);
        const size_t stop = end == std::string_view::npos ? alias.size() : end;
        if (stop > start) {
            raw.emplace_back(alias.substr(start, stop - start));
        }
        if (end == std::string_view::npos) {
            break;
        }
        start = end + 1;
    }
    std::vector<std::string>   folders;
    std::optional<std::string> character;
    bool                       has_container = false;
    for (size_t index = 0; index < raw.size(); index++) {
        const std::string_view component(raw[index]);
        const size_t           mark = component.find('#');
        const std::string      base(component.substr(0, mark));
        const size_t           base_dot = base.rfind('.');
        if (index + 1 < raw.size() && base_dot != std::string::npos) {
            has_container = true;
        }
        if (base == "." || base == "..") {
            folders.push_back(SafeTextureComponent(base));
        } else if (base_dot != std::string::npos) {
            std::string suffix = base.substr(base_dot + 1);
            std::ranges::transform(suffix, suffix.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            if (suffix == "chr" || suffix == "chr_") {
                character = SafeTextureComponent(base);
            }
        } else if (base != "." && base != ".." && index + 1 < raw.size()) {
            folders.push_back(SafeTextureComponent(base));
        }
    }
    const size_t path_slash = path.rfind('/');
    std::string  leaf_path = path.substr(path_slash == std::string::npos ? 0 : path_slash + 1);
    if (character) {
        std::string owner = *character;
        const bool  underscore = owner.size() >= 5 && owner.substr(owner.size() - 5) == ".chr_";
        owner.resize(owner.size() - (underscore ? 5 : 4));
        leaf_path = owner + (underscore ? "_" : "") + "-" + leaf_path;
    }
    if (has_container && leaf_path.ends_with(".json")) {
        leaf_path.insert(leaf_path.size() - 5, "-" + Dc1SourceTag(alias));
    }
    std::string result = "textures";
    for (const std::string &folder : folders) {
        result += "/" + folder;
    }
    result += "/" + leaf_path;
    if (result.size() > 160) {
        return "textures/long-path-" + Sha256Hex(alias).substr(0, 24) + ".json";
    }
    return result;
}

uint16_t ReadLe16(const std::uint8_t *bytes) {
    return static_cast<uint16_t>(bytes[0] | (static_cast<uint16_t>(bytes[1]) << 8));
}

uint32_t ReadLe32(const std::uint8_t *bytes) {
    return static_cast<uint32_t>(bytes[0]) | (static_cast<uint32_t>(bytes[1]) << 8) |
           (static_cast<uint32_t>(bytes[2]) << 16) | (static_cast<uint32_t>(bytes[3]) << 24);
}

std::optional<std::size_t> EmbeddedTextureOffset(const std::vector<std::uint8_t> &bytes, std::size_t container_start,
                                                 std::size_t container_size, std::size_t image_offset) {
    if (container_start > bytes.size() || container_size > bytes.size() - container_start ||
        image_offset < container_start || image_offset - container_start >= container_size) {
        return std::nullopt;
    }
    const std::size_t          container_end = container_start + container_size;
    std::optional<std::size_t> result;
    auto                       record = [&](std::size_t offset) {
        if (result && *result != offset) {
            result = std::numeric_limits<std::size_t>::max();
        } else {
            result = offset;
        }
    };
    for (std::size_t at = container_start; at + 16 <= container_end; at++) {
        if (std::memcmp(bytes.data() + at, "IMG\0", 4) == 0) {
            const uint32_t count = ReadLe32(bytes.data() + at + 4);
            if (count == 0 || count > 256 || static_cast<std::size_t>(count) > (container_end - at - 16) / 48) {
                continue;
            }
            for (uint32_t entry = 0; entry < count; entry++) {
                const std::size_t table_entry = at + 16 + static_cast<std::size_t>(entry) * 48;
                const uint32_t    head_offset = ReadLe32(bytes.data() + table_entry + 32);
                if (head_offset > container_end - at || head_offset + 32 > container_end - at) {
                    continue;
                }
                const std::size_t picture = at + head_offset + 16;
                const uint16_t    header_size = ReadLe16(bytes.data() + picture + 12);
                if (header_size < 0x30 || header_size > container_end - picture ||
                    picture + header_size != image_offset) {
                    continue;
                }
                record(at - container_start);
            }
        }
        if (std::memcmp(bytes.data() + at, "TIM2", 4) != 0 || at + 16 > container_end) {
            continue;
        }
        const uint16_t count = ReadLe16(bytes.data() + at + 6);
        if (count == 0 || count > 256) {
            continue;
        }
        std::size_t picture = at + (bytes[at + 5] != 0 ? 0x80 : 16);
        for (uint16_t index = 0; index < count && picture + 0x30 <= container_end; index++) {
            const uint32_t total = ReadLe32(bytes.data() + picture);
            const uint32_t clut_size = ReadLe32(bytes.data() + picture + 4);
            const uint32_t image_size = ReadLe32(bytes.data() + picture + 8);
            const uint16_t header_size = ReadLe16(bytes.data() + picture + 12);
            if (header_size < 0x30 || header_size > container_end - picture ||
                image_size > container_end - picture - header_size || picture + header_size != image_offset) {
                if (header_size >= 0x30 && header_size <= container_end - picture &&
                    image_size <= container_end - picture - header_size) {
                    picture += header_size + image_size + clut_size;
                    continue;
                }
                break;
            }
            record(at - container_start);
            if (total < header_size + image_size + clut_size || total > container_end - picture) {
                break;
            }
            picture += total;
        }
    }
    if (result && *result == std::numeric_limits<std::size_t>::max()) {
        return std::nullopt;
    }
    return result;
}

std::optional<unsigned> Dc1ImgEntryIndex(const DataReadSource &source) {
    std::size_t member_start = 0;
    auto        member_size = static_cast<std::size_t>(source.range_size);
    if (!source.member.empty()) {
        if (source.member_source_offsets.empty() || source.member_sizes.empty()) {
            return std::nullopt;
        }
        member_start = static_cast<std::size_t>(source.member_source_offsets.back());
        member_size = static_cast<std::size_t>(source.member_sizes.back());
    }
    if (source.offset < member_start || source.offset - member_start >= member_size) {
        return std::nullopt;
    }
    const auto snapshot = DataReadResolvedMetadataSnapshot(source.path);
    if (snapshot == nullptr || member_start > snapshot->size() || member_size > snapshot->size() - member_start ||
        member_size < 16) {
        return std::nullopt;
    }
    const std::uint8_t *member = snapshot->data() + member_start;
    const bool          img3 = std::memcmp(member, "IM3\0", 4) == 0;
    if (!img3 && std::memcmp(member, "IMG\0", 4) != 0 && std::memcmp(member, "IM2\0", 4) != 0) {
        return std::nullopt;
    }
    const unsigned    count = ReadLe32(member + (img3 ? 8 : 4));
    const std::size_t stride = img3 ? 64 : 48;
    if (count == 0 || count > 16384 || count > (member_size - 16) / stride) {
        return std::nullopt;
    }
    const auto              image_offset = static_cast<std::size_t>(source.offset - member_start);
    std::optional<unsigned> result;
    for (unsigned index = 0; index < count; index++) {
        const std::size_t table_entry = 16 + static_cast<std::size_t>(index) * stride;
        const std::size_t head_offset = ReadLe32(member + table_entry + (img3 ? 36 : 32));
        bool              matches = false;
        if (img3) {
            const std::size_t image_size = ReadLe32(member + table_entry + 52);
            matches = head_offset <= member_size && image_size <= member_size - head_offset &&
                      image_offset >= head_offset && image_offset - head_offset < image_size;
        } else if (head_offset <= member_size && 32 <= member_size - head_offset) {
            const std::size_t picture = head_offset + 16;
            const std::size_t header_size = ReadLe16(member + picture + 12);
            matches = header_size >= 0x30 && header_size <= member_size - picture &&
                      picture + header_size == image_offset;
        }
        if (matches) {
            if (result) {
                return std::nullopt;
            }
            result = index;
        }
    }
    return result;
}

std::optional<std::string> BakedTextureDocument(const DataReadSource &source) {
    std::string alias = source.path;
    if (!source.member.empty()) {
        alias += '/';
        alias += source.member;
    }
    const size_t           last_slash = alias.rfind('/');
    const std::string_view leaf(alias.data() + (last_slash == std::string::npos ? 0 : last_slash + 1),
                                alias.size() - (last_slash == std::string::npos ? 0 : last_slash + 1));
    const size_t           hash = leaf.find('#');
    const std::string_view basename = leaf.substr(0, hash);
    const size_t           dot = basename.rfind('.');
    std::string            extension = dot == std::string_view::npos ? std::string{} : std::string(basename.substr(dot + 1));
    std::ranges::transform(extension, extension.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    if ((extension == "chr" || extension == "chr_") && !alias.contains("#offset:")) {
        std::size_t container_start = 0;
        std::size_t container_size = 0;
        if (!source.member.empty()) {
            if (source.member_source_offsets.empty() || source.member_sizes.empty()) {
                return std::nullopt;
            }
            container_start = static_cast<std::size_t>(source.member_source_offsets.back());
            container_size = static_cast<std::size_t>(source.member_sizes.back());
        } else {
            container_size = static_cast<std::size_t>(source.range_size);
        }
        const auto snapshot = DataReadResolvedMetadataSnapshot(source.path);
        if (snapshot == nullptr) {
            return std::nullopt;
        }
        const auto offset = EmbeddedTextureOffset(*snapshot, container_start, container_size,
                                                  static_cast<std::size_t>(source.offset));
        if (!offset) {
            return std::nullopt;
        }
        if (*offset != 0) {
            alias += "#offset:" + std::to_string(*offset);
        }
    }
    std::string path = Dc1TextureDocument(alias);
    if (path.empty()) {
        return std::nullopt;
    }
    return path;
}

} // namespace

std::optional<std::string> PortTextureDocumentFromSource(const DataReadSource &source) {
    return BakedTextureDocument(source);
}

std::string PortTextureDocumentFromAlias(std::string_view source_alias) {
    return Dc1TextureDocument(source_alias);
}

std::optional<unsigned> PortTextureEntryIndexFromSource(const DataReadSource &source) {
    return Dc1ImgEntryIndex(source);
}

} // namespace dc::render
