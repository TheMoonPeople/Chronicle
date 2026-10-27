#include "common.h"

#include <libgraph.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <limits>
#include <memory>
#include <optional>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "data/data_read.hpp"
#include "dataalloc.hpp"
#include "dataread.hpp"
#include "localize/localize_texture.hpp"
#include "mglib.hpp"
#include "render/texture_registry.hpp"
#include "text/caption_text.hpp"
#include "texture.hpp"
#include "tim2.hpp"

namespace dc::game {

using namespace dc::data;
using namespace dc::localize;
using namespace dc::render;
using namespace dc::text;

namespace {

constexpr int kBlockCount = 72;
constexpr int kTextureCount = 196;

struct DummyTexture {
    std::string name;
    int         width = 32;
    int         height = 32;
    int         bpp = 4;
};

// Copies one '#'-separated field. A field ended by the terminator counts the terminator twice in
// the returned advance, so a caller stepping past it reads beyond the string.
int GetStr(const char *text, char *out) {
    if (text[0] == 0) {
        return 0;
    }

    int length = 0;

    while (text[length] != '#') {
        out[length] = text[length];
        length++;

        if (text[length - 1] == 0) {
            break;
        }
    }

    out[length] = 0;
    return length + 1;
}

// Parses "#name#width#height#bpp", where an empty field keeps its default.
std::optional<DummyTexture> GetDummyInfo(const char *text) {
    if (text[0] != '#') {
        return std::nullopt;
    }

    int                  cursor = 1;
    DummyTexture         dummy;
    std::array<char, 32> name{};
    std::array<char, 8>  field{};

    int length = GetStr(&text[cursor], name.data());

    if (length < 2) {
        name[0] = 0;
    }

    cursor += length;

    length = GetStr(&text[cursor], field.data());
    cursor += length;

    if (length > 1) {
        dummy.width = std::atoi(field.data());
    }

    length = GetStr(&text[cursor], field.data());
    cursor += length;

    if (length > 1) {
        dummy.height = std::atoi(field.data());
    }

    length = GetStr(&text[cursor], field.data());

    if (length > 1) {
        dummy.bpp = std::atoi(field.data());
    }

    dummy.name = name.data();
    return dummy;
}

// Builds every requested block from the table entries naming it, in table order. A table ends at
// the first entry whose name is null or empty.
template <typename Info, typename EnterDummy, typename EnterFile>
int LoadBlocks(CTextureManager &manager, int block, Info *table, EnterDummy enter_dummy, EnterFile enter_file) {
    for (int block_no = 0; block_no < kBlockCount; block_no++) {
        if (block >= 0 && block != block_no) {
            continue;
        }

        bool begun = false;

        for (int entry = 0; table[entry].name != nullptr && table[entry].name[0] != 0; entry++) {
            Info &info = table[entry];

            if (info.block_no != block_no) {
                continue;
            }

            if (!begun) {
                manager.BeginEnterTextureBlock(block_no);
            }

            begun = true;

            if (std::optional<DummyTexture> dummy = GetDummyInfo(info.name)) {
                enter_dummy(info, *dummy);
            } else {
                enter_file(info);
            }
        }

        if (begun) {
            manager.EndEnterTextureBlock(block_no);
        }
    }

    return 0;
}

} // namespace

CTexture::CTexture() {
    Initialize();
}

void CTexture::Initialize() {
    name[0] = 0;
    block = 0;
    image[3] = nullptr;
    image[2] = nullptr;
    image[1] = nullptr;
    image[0] = nullptr;
    clut = nullptr;
    tex1 = 0;
    tex0 = 0;
    bpp = 0;
    height = 0;
    width = 0;
    swizzled = false;
}

CTextureBlock::CTextureBlock() {
    Initialize();
}

void CTextureBlock::Initialize() {
    name[0] = 0;
    vram_end = 0;
    vram_top = 0;
    vram_dirty = 0;
    buffer_end = nullptr;
    buffer = nullptr;
    loaded = false;
    extend = false;
}

namespace {

enum class EnterMode {
    Block,
    Extended,
    Fixed,
};

u_long Tex0(unsigned tbp, int tbw, int psm, int tw, int th, unsigned cbp, int cld) {
    return static_cast<u_long>(tbp) | (static_cast<u_long>(tbw) << 14) | (static_cast<u_long>(psm) << 20) |
           (static_cast<u_long>(tw) << 26) | (static_cast<u_long>(th) << 30) | (static_cast<u_long>(1) << 34) |
           (static_cast<u_long>(cbp) << 37) | (static_cast<u_long>(cld) << 61);
}

int CeilLog2(int value) {
    int log = 0;
    for (int power = value; power >= 2; power >>= 1) {
        log++;
    }
    return (1 << log) == value ? log : log + 1;
}

int Psm(int bpp) {
    switch (bpp) {
        case 0:
            return SCE_GS_PSMT4;
        case 1:
            return SCE_GS_PSMT8;
        case 2:
            return SCE_GS_PSMCT16;
        case 3:
            return SCE_GS_PSMCT24;
        case 4:
            return SCE_GS_PSMCT32;
        default:
            break;
    }
    return -1;
}

int ImageBytes(int width, int height, int bpp) {
    return bpp == 0 ? width * height / 2 : width * height * bpp;
}

[[noreturn]] void Stop(const char *what, const char *name, int value) {
    std::println(stderr, "texture: {} ({} {})", what, name, value);
    std::abort();
}

[[noreturn]] void StopBakedTexture(const char *name, std::string_view message) {
    std::println(stderr, "texture: baked asset resolution failed for {}: {}", name, message);
    std::abort();
}

void CopyTextureName(std::span<char, 32> destination, std::string_view name) {
    const std::size_t length = std::min(name.size(), destination.size() - 1);
    std::ranges::copy(name.substr(0, length), destination.begin());
    destination[length] = '\0';
}

void ReleaseKeys(const CTexture &texture) {
    if (texture.name[0] == 0) {
        return;
    }
    const auto tex0 = std::bit_cast<sceGsTex0>(texture.tex0);
    PortReleaseKey(tex0.TBP0);
    if (tex0.CBP != 0) {
        PortReleaseKey(tex0.CBP);
    }
}

uint8_t NearestPaletteColor(const std::array<uint32_t, 256> &palette, uint32_t color) {
    unsigned best_distance = ~0u;
    uint8_t  best_index = 0;
    for (unsigned index = 0; index < palette.size(); index++) {
        unsigned distance = 0;
        for (unsigned shift = 0; shift < 32; shift += 8) {
            int difference = static_cast<int>((palette[index] >> shift) & 0xff) -
                             static_cast<int>((color >> shift) & 0xff);
            distance += static_cast<unsigned>(difference * difference);
        }
        if (distance < best_distance) {
            best_distance = distance;
            best_index = static_cast<uint8_t>(index);
        }
    }
    return best_index;
}

uint32_t BlendColors(uint32_t first, uint32_t second, unsigned first_weight, unsigned total_weight) {
    uint32_t blended = 0;
    for (unsigned shift = 0; shift < 32; shift += 8) {
        unsigned first_channel = (first >> shift) & 0xff;
        unsigned second_channel = (second >> shift) & 0xff;
        unsigned channel = (first_channel * first_weight + second_channel * (total_weight - first_weight) +
                            total_weight / 2) /
                           total_weight;
        blended |= channel << shift;
    }
    return blended;
}

// DC_TEXTURE_DUMP=<dir>: every entered texture is written there as <name>.png, indexed textures
// through their palette.
void DumpTexture(const char *name, const PortDecodedTexture &texture) {
    static const char *dir = std::getenv("DC_TEXTURE_DUMP");
    if (dir == nullptr || texture.levels.empty()) {
        return;
    }
    const unsigned       width = texture.width;
    const unsigned       height = texture.height;
    std::vector<uint8_t> rgba(static_cast<size_t>(width) * height * 4);
    for (size_t pixel = 0; pixel < static_cast<size_t>(width) * height; pixel++) {
        if (texture.format == gfx::TextureFormat::Index8) {
            const uint32_t color = texture.palette[texture.levels[0][pixel]];
            std::memcpy(&rgba[pixel * 4], &color, 4);
        } else {
            std::memcpy(&rgba[pixel * 4], &texture.levels[0][pixel * 4], 4);
        }
    }
    // A PNG with stored (uncompressed) deflate blocks.
    auto crc = [](std::span<const uint8_t> data) {
        uint32_t value = ~0u;
        for (const uint8_t byte : data) {
            value ^= byte;
            for (int bit = 0; bit < 8; bit++) {
                value = (value >> 1) ^ (0xEDB88320u & (0u - (value & 1)));
            }
        }
        return ~value;
    };
    std::vector<uint8_t> out = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    auto                 put32 = [](std::vector<uint8_t> &target, uint32_t value) {
        for (int shift = 24; shift >= 0; shift -= 8) {
            target.push_back(static_cast<uint8_t>(value >> shift));
        }
    };
    auto chunk = [&](std::string_view type, const std::vector<uint8_t> &body) {
        put32(out, static_cast<uint32_t>(body.size()));
        std::vector<uint8_t> typed(type.begin(), type.end());
        typed.insert(typed.end(), body.begin(), body.end());
        out.insert(out.end(), typed.begin(), typed.end());
        put32(out, crc(typed));
    };
    std::vector<uint8_t> ihdr;
    put32(ihdr, width);
    put32(ihdr, height);
    ihdr.insert(ihdr.end(), {8, 6, 0, 0, 0});
    chunk("IHDR", ihdr);
    std::vector<uint8_t> raw;
    for (unsigned y = 0; y < height; y++) {
        raw.push_back(0);
        raw.insert(raw.end(), rgba.begin() + static_cast<size_t>(y) * width * 4,
                   rgba.begin() + static_cast<size_t>(y + 1) * width * 4);
    }
    std::vector<uint8_t> zlib = {0x78, 0x01};
    uint32_t             adler_low = 1;
    uint32_t             adler_high = 0;
    for (const uint8_t byte : raw) {
        adler_low = (adler_low + byte) % 65521;
        adler_high = (adler_high + adler_low) % 65521;
    }
    for (size_t position = 0; position < raw.size() || position == 0; position += 65535) {
        const size_t length = std::min<size_t>(65535, raw.size() - position);
        zlib.push_back(position + length >= raw.size() ? 1 : 0);
        zlib.push_back(static_cast<uint8_t>(length));
        zlib.push_back(static_cast<uint8_t>(length >> 8));
        zlib.push_back(static_cast<uint8_t>(~length));
        zlib.push_back(static_cast<uint8_t>((~length) >> 8));
        const auto block_begin = raw.begin() + static_cast<std::ptrdiff_t>(position);
        zlib.insert(zlib.end(), block_begin, block_begin + static_cast<std::ptrdiff_t>(length));
    }
    put32(zlib, (adler_high << 16) | adler_low);
    chunk("IDAT", zlib);
    chunk("IEND", {});
    std::filesystem::create_directories(dir);
    std::string path = std::string(dir) + "/" + name + ".png";
    if (FILE *file = std::fopen(path.c_str(), "wb")) {
        std::fwrite(out.data(), 1, out.size(), file);
        std::fclose(file);
    }
}

// Ground models repeat these images across UV islands and cells. Their source edges differ, so
// blend a narrow indexed border into a periodic image before uploading it.
void MakeGroundPeriodic(std::string_view name, PortDecodedTexture &texture) {
    if ((name != "d02b05" && name != "d02b06" && name != "e04b01") ||
        (texture.format != gfx::TextureFormat::Index8 && texture.format != gfx::TextureFormat::Rgba8) ||
        texture.width < 64 || texture.height < 64 ||
        texture.levels.empty()) {
        return;
    }

    const unsigned        horizontal_band = name == "d02b05" ? 32 : 8;
    const unsigned        vertical_band = name == "d02b06" ? 32 : 8;
    std::vector<uint8_t> &pixels = texture.levels[0];
    const unsigned        width = texture.width;
    const unsigned        height = texture.height;
    const auto            color_at = [&](unsigned pixel) {
        if (texture.format == gfx::TextureFormat::Index8) {
            return texture.palette[pixels[pixel]];
        }
        uint32_t color = 0;
        std::memcpy(&color, pixels.data() + static_cast<std::size_t>(pixel) * 4, sizeof(color));
        return color;
    };
    const auto set_color = [&](unsigned pixel, uint32_t color) {
        if (texture.format == gfx::TextureFormat::Index8) {
            pixels[pixel] = NearestPaletteColor(texture.palette, color);
        } else {
            std::memcpy(pixels.data() + static_cast<std::size_t>(pixel) * 4, &color, sizeof(color));
        }
    };

    for (unsigned y = 0; y < height; y++) {
        for (unsigned offset = 0; offset < horizontal_band; offset++) {
            unsigned left = y * width + offset;
            unsigned right = y * width + width - 1 - offset;
            uint32_t left_color = color_at(left);
            uint32_t right_color = color_at(right);
            unsigned left_weight = horizontal_band + offset;
            unsigned right_weight = horizontal_band - offset;
            set_color(left, BlendColors(left_color, right_color, left_weight, 2 * horizontal_band));
            set_color(right, BlendColors(left_color, right_color, right_weight, 2 * horizontal_band));
        }
    }

    for (unsigned x = 0; x < width; x++) {
        for (unsigned offset = 0; offset < vertical_band; offset++) {
            unsigned top = offset * width + x;
            unsigned bottom = (height - 1 - offset) * width + x;
            uint32_t top_color = color_at(top);
            uint32_t bottom_color = color_at(bottom);
            unsigned top_weight = vertical_band + offset;
            unsigned bottom_weight = vertical_band - offset;
            set_color(top, BlendColors(top_color, bottom_color, top_weight, 2 * vertical_band));
            set_color(bottom, BlendColors(top_color, bottom_color, bottom_weight, 2 * vertical_band));
        }
    }
}

// EnterTexture, EnterTextureEX and EnterFixTexture share everything but where pixels are staged and
// which VRAM end moves. The staging and VRAM arithmetic must match retail: the game sizes later
// allocations from buffer_used (editloop's LoadTexture). The image goes to the renderer, and
// TBP0/CBP are registry keys, not VRAM addresses.
void Enter(CTextureManager &manager, EnterMode mode, int block, char *name, u_char *image, int width, int height,
           int bpp, u_char *clut, int clut_colors, int mipmap, u_char *mip1, u_char *mip2, u_long tex1,
           int swizzled) {
    if (name[0] == 0) {
        return;
    }
    CTexture *tex = manager.SearchTexture(name);
    if (image == nullptr || mip1 == nullptr) {
        mipmap = false;
    }
    int psm = Psm(bpp);
    if (psm < 0) {
        return;
    }
    bool indexed = bpp < 2;
    int  tw = CeilLog2(width);
    int  th = CeilLog2(height);
    int  tbw = width >> 6;
    if (tbw <= 0) {
        tbw = 1;
    }
    int image_size = ImageBytes(width, height, bpp);
    int image_blocks = image_size >> 8;
    int clut_bytes = bpp == 0 ? 64 : 1024;

    if (mode == EnterMode::Fixed) {
        int fix_blocks = image_blocks;
        if (mipmap) {
            int mip_blocks = image_blocks >> 2;
            fix_blocks += mip_blocks + (mip_blocks >> 2);
        }
        if (fix_blocks % 32) {
            fix_blocks += 32 - fix_blocks % 32;
        }
        manager.vram_fix -= fix_blocks;
        if (image == nullptr ? psm != SCE_GS_PSMCT32 : indexed) {
            manager.vram_fix -= 32;
        }
        if (image != nullptr && !indexed) {
            tex->clut = nullptr;
        }
    } else {
        CTextureBlock &entry = manager.blocks[block];
        int            vram_top = entry.vram_top;
        int            vram_end = entry.vram_end;
        bool           staged = mode == EnterMode::Block;
        if (!staged) {
            entry.extend = true;
        }
        if (image == nullptr) {
            vram_top += image_blocks;
            if (indexed) {
                vram_top += 4;
            }
            vram_end = vram_top;
        } else {
            auto stage = [&](u_char *source, int bytes, int blocks) -> u_int * {
                auto *destination = reinterpret_cast<u_int *>(manager.buffer + manager.buffer_used);
                if (staged) {
                    // A picture with fewer levels than the block asks for leaves the last one null,
                    // which retail copies from all the same.
                    if (source != nullptr) {
                        std::memcpy(destination, source, bytes);
                    }
                    manager.buffer_used += blocks * 16;
                }
                vram_end += blocks;
                return staged ? destination : reinterpret_cast<u_int *>(source);
            };
            tex->image[0] = stage(image, image_size, image_blocks);
            if (mipmap) {
                tex->image[1] = stage(mip1, image_size >> 2, image_blocks >> 2);
                tex->image[2] = stage(mip2, image_size >> 4, (image_blocks >> 2) >> 2);
            }
            if (indexed) {
                auto *staged_clut = reinterpret_cast<u_int *>(manager.buffer + manager.buffer_used);
                if (staged) {
                    std::memcpy(staged_clut, clut, clut_bytes);
                    manager.buffer_used += 64;
                }
                tex->clut = staged ? staged_clut : reinterpret_cast<u_int *>(clut);
                vram_end += 4;
            } else {
                tex->clut = nullptr;
            }
            tex->swizzled = staged ? indexed : swizzled;
        }
        if (vram_end % 32) {
            int pad = 32 - vram_end % 32;
            vram_end += pad;
            if (staged) {
                manager.buffer_used += pad * 16;
            }
        }
        entry.vram_top = vram_top;
        entry.vram_end = vram_end;
        if (staged && manager.buffer_used >= manager.buffer_size) {
            Stop("texture buffer over", name, manager.buffer_used);
        }
        if (entry.vram_end > manager.vram_max) {
            Stop("VRAM is not enough", name, entry.vram_end);
        }
    }

    unsigned           tbp = 0;
    unsigned           cbp = 0;
    bool               baked = false;
    bool               canvas_backing = false;
    bool               unsourced = false;
    PortDecodedTexture decoded;
    if (image != nullptr && !PortLegacyTexturePath()) {
        if (indexed) {
            PortRememberTextureClut(name, clut, static_cast<std::size_t>(clut_bytes));
        }
        DataReadSource             source;
        std::string                resolution_error;
        std::optional<std::string> document;
        if (image_size > 0 && DataReadSourceRange(image, static_cast<std::size_t>(image_size), source)) {
            document = PortTextureDocumentFromSource(source);
            if (!document) {
                StopBakedTexture(name, "unsupported source identity " + source.path +
                                           (source.member.empty() ? std::string{} : "/" + source.member));
            }
            const std::optional<unsigned> entry_index = PortTextureEntryIndexFromSource(source);
            baked = PortLoadBakedTexture(*document, name, 0, static_cast<unsigned>(width),
                                         static_cast<unsigned>(height), 0, clut,
                                         clut == nullptr ? 0 : static_cast<std::size_t>(clut_bytes), decoded,
                                         resolution_error, entry_index, bpp);
            canvas_backing = !baked && resolution_error == "canvas texture keeps its runtime backing";
            if (!baked && !canvas_backing) {
                StopBakedTexture(name, resolution_error);
            }
        } else {
            // Pixels outside any data source (a synthetic or runtime image) have no baked document
            // and decode from the bytes given.
            resolution_error = "texture pixels have no validated source range";
            unsourced = true;
        }
        if (!baked && indexed && !canvas_backing && !unsourced) {
            StopBakedTexture(name, resolution_error);
        }
        if (!baked && !canvas_backing && !resolution_error.empty()) {
            std::println(stderr, "texture: using source decoder for non-indexed {}: {}", name, resolution_error);
        }
    }
    if (image == nullptr) {
        tbp = PortCreatePlaceholder(name, width, height, bpp, PortTextureOwner::Manager, &cbp);
    } else {
        if (!baked) {
            const std::array<const u_char *, 3> levels = {image, mipmap ? mip1 : nullptr, mipmap ? mip2 : nullptr};
            if (!PortDecodeTexture(bpp, width, height, levels.data(), mipmap ? 3 : 1, clut, clut_colors,
                                   indexed && swizzled != 0, decoded)) {
                return;
            }
        }
        LocalizeTexture(name, decoded);
        caption::PreparePainted(name, decoded);
        MakeGroundPeriodic(name, decoded);
        DumpTexture(name, decoded);
        if (baked) {
            tex->clut = nullptr;
            cbp = 0;
            psm = SCE_GS_PSMCT32;
        }
        tbp = PortCreateTexture(decoded, PortTextureOwner::Manager, &cbp);
    }
    tex->tex0 = Tex0(tbp, tbw, psm, tw, th, cbp, indexed ? 1 : 0);

    CopyTextureName(tex->name, name);
    tex->width = width;
    tex->height = height;
    tex->bpp = bpp;
    tex->block = mode == EnterMode::Fixed ? -1 : block;

    if (bpp > 0 && mipmap) {
        tex->tex1 = SCE_GS_SET_TEX1(0, 2, 1, 5, 1, 0, -120);
        if (tex1 != 0) {
            const auto lod = std::bit_cast<sceGsTex1>(tex1);
            tex->tex1 = SCE_GS_SET_TEX1(0, 2, 1, 5, 1, lod.L, lod.K);
        }
    }
    manager.texture_max = 195;
}

} // namespace

void CTextureManager::Initialize(int size) {
    PortReleaseOwner(PortTextureOwner::Manager);

    TextureData.used = 0;
    SetBuffer(reinterpret_cast<u_long128 *>(TextureData.base + (static_cast<std::ptrdiff_t>(TextureData.used) * 16)), TextureData.limit);

    for (CTextureBlock &block : blocks) {
        block.Initialize();
        block.vram_top = mgTopVRAM;
        block.vram_end = mgTopVRAM;
    }
    for (CTexture &texture : textures) {
        texture.Initialize();
    }

    texture_max = 1;
    vram_work = 8960;
    vram_size = size;
    vram_max = vram_size;
    vram_fix = size;

    // The frame-copy area water samples, a field of the frame on PS2: the water unit fills it.
    unsigned work = PortRegisterNamedTarget("work", 640, SCREEN_HALF_HEIGHT, true, PortTextureOwner::Manager);
    CopyTextureName(textures[0].name, "work");
    textures[0].tex0 = SCE_GS_SET_TEX0(work, 10, 0, 10, 8, 1, 0, 0, 0, 0, 0, 0);
    textures[0].block = -1;
    last_block = -1;
}

void CTextureManager::SetBuffer(u_long128 *buffer, int size) {
    if (size < 0 || (buffer == nullptr && size != 0)) {
        buffer = nullptr;
        size = 0;
    }
    this->buffer = buffer;
    buffer_size = size;
    const int misalignment = buffer == nullptr ? 0 : static_cast<int>(reinterpret_cast<std::uintptr_t>(buffer) & 0x7f);
    if (misalignment != 0) {
        const int skipped_quads = (128 - misalignment) >> 4;
        if (skipped_quads > buffer_size) {
            this->buffer = nullptr;
            buffer_size = 0;
        } else {
            this->buffer += skipped_quads;
            buffer_size -= skipped_quads;
        }
    }

    buffer_used = 0;
}

int CTextureManager::SearchTextureName(char *name, int block) {
    for (int index = texture_max; index >= 0; index--) {
        const CTexture &texture = textures[index];

        if (texture.name[0] == 0) {
            continue;
        }

        if (block >= 0 && block != texture.block) {
            continue;
        }

        if (std::string_view{name} == texture.name) {
            return index;
        }
    }

    return -1;
}

int CTextureManager::GetTextureHandle(char *name, int block) {
    int handle = SearchTextureName(name, block);

    if (handle < 0) {
        return -1;
    }

    return handle;
}

CTexture *CTextureManager::GetTexture(int handle) {
    if (handle < 0 || handle >= texture_max) {
        return &textures[0];
    }

    return &textures[handle];
}

CTexture *CTextureManager::GetTexture(char *name, int block) {
    int handle = GetTextureHandle(name, block);

    if (handle < 0) {
        return nullptr;
    }

    return GetTexture(handle);
}

CTexture *CTextureManager::SearchTexture(char *name) {
    int index = 1;

    for (; index < kTextureCount; index++) {
        if (textures[index].name[0] == 0) {
            break;
        }
    }

    SearchTextureName(name, -1);
    return textures + index;
}

void CTextureManager::EnterTexture(int block, char *name, u_char *image, int width, int height, int bpp,
                                   u_char *clut, int clut_colors, int mipmap, u_char *mip1, u_char *mip2,
                                   u_char *mip3, u_long tex1, int swizzled) {
    Enter(*this, EnterMode::Block, block, name, image, width, height, bpp, clut, clut_colors, mipmap, mip1, mip2,
          tex1, swizzled);
}

void CTextureManager::EnterTextureEX(int block, char *name, u_char *image, int width, int height, int bpp,
                                     u_char *clut, int clut_colors, int mipmap, u_char *mip1, u_char *mip2,
                                     u_char *mip3, u_long tex1, int swizzled) {
    Enter(*this, EnterMode::Extended, block, name, image, width, height, bpp, clut, clut_colors, mipmap, mip1,
          mip2, tex1, swizzled);
}

void CTextureManager::EnterFixTexture(char *name, u_char *image, int width, int height, int bpp, u_char *clut,
                                      int clut_colors, int mipmap, u_char *mip1, u_char *mip2, u_char *mip3,
                                      u_long tex1, int swizzled) {
    Enter(*this, EnterMode::Fixed, -1, name, image, width, height, bpp, clut, clut_colors, mipmap, mip1, mip2,
          tex1, swizzled);
}

namespace {

// GS pixel storage mode PSMT8H: 8-bit indices in the upper bytes of a 32-bit word.
constexpr unsigned kPsmT8H = 27;

// Decodes the fixed-Z sheet from the TIM2 in the buffer, for data without baked documents.
void EnterRawFixTextureZ(CTextureManager &manager, u_char *buffer) {
    char     *name = reinterpret_cast<char *>(buffer + 16);
    auto     *head = reinterpret_cast<TM2_head *>(buffer + *reinterpret_cast<int *>(buffer + 48));
    auto     *picture = reinterpret_cast<TM2_picture *>(reinterpret_cast<u_char *>(head) + 16);
    const int width = head->image_width;
    const int height = head->image_height;
    if (width != 640 || height > SCREEN_HALF_HEIGHT || picture->image_type != TIM2_IDTEX8) {
        return;
    }

    u_char   *image = reinterpret_cast<u_char *>(picture) + picture->header_size;
    u_char   *clut = image + picture->image_size;
    CTexture *tex = manager.SearchTexture(name);
    CopyTextureName(tex->name, name);
    tex->width = width;
    tex->height = height;
    tex->bpp = 1;
    tex->block = 73;

    const std::array<const u_char *, 1> levels = {image};
    PortDecodedTexture                  decoded;
    unsigned                            tbp = 0;
    unsigned                            cbp = 0;
    if (PortDecodeTexture(1, width, height, levels.data(), 1, clut, 256, false, decoded)) {
        LocalizeTexture(name, decoded);
        caption::PreparePainted(name, decoded);
        DumpTexture(name, decoded);
        tbp = PortCreateTexture(decoded, PortTextureOwner::Manager, &cbp);
    }
    tex->tex0 = SCE_GS_SET_TEX0(tbp, 10, kPsmT8H, 10, 8, 1, 0, cbp, 0, 0, 0, 1);
}

} // namespace

// Retail parks "stayframe", the menus' 640-wide 8-bit sheet of frames and digits, in the upper
// bytes of the Z buffer as PSMT8H. Here it is an index texture like any other; the menus reach it
// by name and draw pieces of it with set2DSprite.
void CTextureManager::EnterFixTextureZ(u_char *buffer) {
    if (PortLegacyTexturePath()) {
        EnterRawFixTextureZ(*this, buffer);
        return;
    }
    DataReadSource source;
    if (!DataReadSourceRange(buffer, 64, source)) {
        StopBakedTexture("<unknown>", "fixed-Z texture header has no validated source range");
    }
    char             *name = reinterpret_cast<char *>(buffer + 16);
    const std::size_t name_size = std::string_view(name, 32).find('\0');
    if (name_size == 0 || name_size == std::string_view::npos) {
        StopBakedTexture("<unknown>", "fixed-Z texture name is not terminated");
    }
    const std::optional<std::string> document = PortTextureDocumentFromSource(source);
    if (!document) {
        StopBakedTexture(name, "fixed-Z source has no deterministic baked document path");
    }
    PortDecodedTexture decoded;
    std::string        error;
    if (!PortLoadBakedTexture(*document, name, 0, 640, 0, 0, nullptr, 0, decoded, error)) {
        StopBakedTexture(name, error);
    }
    if (decoded.height > SCREEN_HALF_HEIGHT) {
        StopBakedTexture(name, "fixed-Z baked texture exceeds the logical height limit");
    }
    CTexture *tex = SearchTexture(name);
    if (tex == nullptr) {
        StopBakedTexture(name, "fixed-Z texture has no manager entry");
    }
    CopyTextureName(tex->name, name);
    tex->width = static_cast<int>(decoded.width);
    tex->height = static_cast<int>(decoded.height);
    tex->bpp = 1;
    tex->block = 73;
    LocalizeTexture(name, decoded);
    caption::PreparePainted(name, decoded);
    DumpTexture(name, decoded);
    const unsigned tbp = PortCreateTexture(decoded, PortTextureOwner::Manager, nullptr);
    tex->tex0 = SCE_GS_SET_TEX0(tbp, 10, SCE_GS_PSMCT32, 10, 8, 1, 0, 0, 0, 0, 0, 0);
}

void CTextureManager::EnterIMGFile(u_char *buffer, int block, int mipmap, int extend) {
    if (buffer == nullptr) {
        return;
    }

    const bool swizzled = std::string_view{reinterpret_cast<const char *>(buffer), 3} == "IM2";
    auto      *header = reinterpret_cast<IMG_head *>(buffer);
    auto      *entries = reinterpret_cast<IMG_entry *>(header + 1);

    for (u_int index = 0; index < header->pictures; index++) {
        IMG_entry &entry = entries[index];
        auto      *head = reinterpret_cast<TM2_head *>(buffer + entry.offset);
        auto      *picture = reinterpret_cast<TM2_picture *>(reinterpret_cast<u_char *>(head) + 16);
        int        width = head->image_width;
        int        height = head->image_height;
        int        bpp = 0;

        switch (head->image_type) {
            case TIM2_RGB16:
                bpp = 2;
                break;
            case TIM2_RGB24:
                bpp = 3;
                break;
            case TIM2_RGB32:
                bpp = 4;
                break;
            case TIM2_IDTEX4:
                bpp = 0;
                break;
            case TIM2_IDTEX8:
                bpp = 1;
                break;
            default:
                continue;
        }

        u_char *image = reinterpret_cast<u_char *>(picture) + picture->header_size;
        u_char *clut = bpp < 2 ? image + picture->image_size : nullptr;

        std::array<u_char *, 4> mip{};

        if (picture->mipmap_count > 1) {
            u_char *mip_image = image + picture->mipmap_size[0];

            for (int level = 1; level < static_cast<int>(picture->mipmap_count); level++) {
                mip[level] = mip_image;
                mip_image += picture->mipmap_size[level];
            }
        }

        if (extend == 0) {
            if (block >= 0) {
                EnterTexture(block, entry.name, image, width, height, bpp, clut, picture->clut_colors, mipmap, mip[1], mip[2], mip[3], picture->tex1, swizzled);
            } else {
                EnterFixTexture(entry.name, image, width, height, bpp, clut, picture->clut_colors, mipmap, mip[1], mip[2], mip[3], picture->tex1, swizzled);
            }
        } else if (block >= 0) {
            EnterTextureEX(block, entry.name, image, width, height, bpp, clut, picture->clut_colors, mipmap, mip[1], mip[2], mip[3], picture->tex1, swizzled);
        }
    }
}

int LoadImage(u_int *packet, int dbp, int dpsm, int dbw, u_long128 *source, int qwc, int dsax, int dsay, int rrw,
              int rrh) {
    return 0;
}

// Every texture is resident on the renderer from the moment it is entered, so a block never needs
// uploading; only its bookkeeping remains.
void CTextureManager::ReloadTexture(sceVif1Packet *packet, int block) {
    if (block < 0 || block >= 72) {
        last_block = -1;
        return;
    }
    last_block = block;
    blocks[block].loaded = true;
}

void CTextureManager::BeginEnterTextureBlock(int block) {
    if (block < 0 || block >= kBlockCount) {
        return;
    }

    CTextureBlock &entry = blocks[block];

    if (entry.vram_top == 0) {
        entry.vram_top = mgTopVRAM;
    }

    if (entry.vram_end == 0) {
        entry.vram_end = mgTopVRAM;
    }

    entry.buffer = buffer + buffer_used;
    entry.buffer_end = buffer + buffer_used;
    entry.loaded = false;
}

// Native textures occupy independent storage, so simulated VRAM overlap never overwrites either
// image; only the staging bounds check and block finalization remain.
void CTextureManager::EndEnterTextureBlock(int block) {
    if (block < 0 || block >= 72) {
        return;
    }
    if (buffer_used > buffer_size) {
        Stop("texture buffer over", "quadwords", buffer_used);
    }
    CTextureBlock &entry = blocks[block];
    entry.buffer_end = buffer + buffer_used;
    if (entry.buffer_end == entry.buffer) {
        entry.buffer = nullptr;
        entry.buffer_end = nullptr;
    }
}

int CTextureManager::DeleteTextureBlock(int block) {
    if (block >= 72) {
        return 0;
    }
    if (block >= 0) {
        blocks[block].Initialize();
    }
    if (block < 0) {
        block = -1;
    }
    for (CTexture &texture : textures) {
        if (block < 0 || texture.block == block) {
            ReleaseKeys(texture);
            texture.Initialize();
        }
    }
    return 1;
}

int CTextureManager::CleanUpBuffer() {
    constexpr int block_count = static_cast<int>(std::extent_v<decltype(blocks)>);
    auto          block_start = std::make_unique<u_long128 *[]>(block_count);
    auto          block_order = std::make_unique<int[]>(block_count);
    auto          block_quads = std::make_unique<int[]>(block_count);

    for (int i = 0; i < block_count; i++) {
        block_start[i] = blocks[i].buffer;
        block_order[i] = i;
    }

    for (int i = 0; i < block_count - 1; i++) {
        for (int j = i + 1; j < block_count; j++) {
            if (reinterpret_cast<std::uintptr_t>(block_start[i]) > reinterpret_cast<std::uintptr_t>(block_start[j])) {
                std::swap(block_start[i], block_start[j]);
                std::swap(block_order[i], block_order[j]);
            }
        }
    }

    buffer_used = 0;
    if (buffer == nullptr || buffer_size < 0) {
        return 0;
    }

    const auto           buffer_address = reinterpret_cast<std::uintptr_t>(buffer);
    const std::uintptr_t buffer_bytes = static_cast<std::uintptr_t>(buffer_size) * sizeof(u_long128);
    if (buffer_bytes > std::numeric_limits<std::uintptr_t>::max() - buffer_address) {
        return 0;
    }
    const std::uintptr_t buffer_end = buffer_address + buffer_bytes;
    int                  required = 0;
    for (int i = 0; i < block_count; i++) {
        const int block_no = block_order[i];
        if (block_no <= 0 || block_start[i] == nullptr) {
            continue;
        }
        const auto start = reinterpret_cast<std::uintptr_t>(blocks[block_no].buffer);
        const auto end = reinterpret_cast<std::uintptr_t>(blocks[block_no].buffer_end);
        if (start < buffer_address || end < start || end > buffer_end || (end - start) % sizeof(u_long128) != 0) {
            return 0;
        }
        const std::uintptr_t quads = (end - start) / sizeof(u_long128);
        if (quads > static_cast<std::uintptr_t>(buffer_size - required)) {
            return 0;
        }
        block_quads[block_no] = static_cast<int>(quads);
        required += static_cast<int>(quads);
        const int padding = (8 - required % 8) % 8;
        if (padding > buffer_size - required) {
            return 0;
        }
        required += padding;
    }
    for (int i = 0; i < block_count; i++) {
        const int block_no = block_order[i];
        if (block_no <= 0) {
            continue;
        }

        u_long128 *source = block_start[i];
        if (source == nullptr) {
            continue;
        }

        const int            block_size = block_quads[block_no];
        u_long128           *destination = buffer + buffer_used;
        const std::ptrdiff_t shift = (source - destination) * static_cast<std::ptrdiff_t>(sizeof(u_long128));
        blocks[block_no].buffer = destination;
        blocks[block_no].buffer_end = destination + block_size;

        // Blocks are compacted towards the buffer start in address order, so a forward copy is safe.
        std::copy_n(source, block_size, destination);

        for (CTexture &tex : textures) {
            if (shift == 0) {
                break;
            }
            if (tex.block != block_no) {
                continue;
            }
            for (u_int *&level : tex.image) {
                if (level != nullptr) {
                    level = reinterpret_cast<u_int *>(reinterpret_cast<char *>(level) - shift);
                }
            }
            if (tex.clut != nullptr) {
                tex.clut = reinterpret_cast<u_int *>(reinterpret_cast<char *>(tex.clut) - shift);
            }
        }

        buffer_used += block_size;
        const int misalignment = block_size % 8;

        if (misalignment != 0) {
            const int padding = 8 - misalignment;
            if (padding > buffer_size - buffer_used) {
                return 0;
            }
            buffer_used += padding;
        }
    }

    return 1;
}

int CTextureManager::CleanUpTextureList() {
    // Entries are never moved: a native visual keeps a table index (Draw3DStrip::texture) that a
    // move would retarget. SearchTexture fills empty entries, so a reloaded block takes the first
    // available holes.
    return 1;
}

int CTextureManager::LoadTextureBlock(int block, u_int *buffer) {
    return LoadTextureBlock(block, file, buffer);
}

int CTextureManager::LoadTextureBlock(int block, LOADTEXTURE_INFO *table, u_int *buffer) {
    return LoadBlocks(
        *this, block, table,
        [](LOADTEXTURE_INFO &info, DummyTexture &dummy) {
            TexManager.EnterTexture(info.block_no, dummy.name.data(), nullptr, dummy.width, dummy.height, dummy.bpp, nullptr, 0, 0, nullptr, nullptr, nullptr, 0, 0);
        },
        [this, buffer](LOADTEXTURE_INFO &info) {
            if (LoadFile2(info.name, buffer, nullptr, 0) != 0) {
                EnterIMGFile(reinterpret_cast<u_char *>(buffer), info.block_no, info.mipmap, 0);
            }
        });
}

int CTextureManager::LoadTextureBlock(int block, LOADTEXTURE_INFO2 *table) {
    return LoadBlocks(
        *this, block, table,
        [](LOADTEXTURE_INFO2 &info, DummyTexture &dummy) {
            TexManager.EnterTexture(info.block_no, dummy.name.data(), nullptr, dummy.width, dummy.height, dummy.bpp, nullptr, 0, 0, nullptr, nullptr, nullptr, 0, 0);
        },
        [this](LOADTEXTURE_INFO2 &info) { EnterIMGFile(reinterpret_cast<u_char *>(info.name), info.block_no, info.mipmap, 0); });
}

int CTextureManager::LoadTextureBlockEX(int block, LOADTEXTURE_INFO2 *table) {
    return LoadBlocks(
        *this, block, table,
        [](LOADTEXTURE_INFO2 &info, DummyTexture &dummy) {
            TexManager.EnterTextureEX(info.block_no, dummy.name.data(), nullptr, dummy.width, dummy.height, dummy.bpp, nullptr, 0, 0, nullptr, nullptr, nullptr, 0, 0);
        },
        [this](LOADTEXTURE_INFO2 &info) { EnterIMGFile(reinterpret_cast<u_char *>(info.name), info.block_no, info.mipmap, 1); });
}

int CTextureManager::EnterTextureFile(LOADTEXTURE_INFO *table) {
    file = table;
    return 1;
}

void CTextureManager::print_buff_info() {
}

} // namespace dc::game
