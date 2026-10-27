#pragma once

#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include "gfx_fixture.hpp"
#include "render/texture_registry.hpp"
#include "texture.hpp"
#include "tim2.hpp"

/**
 * Synthetic TIM2 pictures and IMG archives for the texture tests, and a renderer with a fresh
 * texture manager over a staging buffer of its own.
 */
namespace dc::test::texfix {

using Bytes = std::vector<uint8_t>;

/**
 * Writes the low 16 bits of the value little-endian at the offset.
 */
inline void Put16(Bytes &data, size_t offset, uint32_t value) {
    data[offset] = static_cast<uint8_t>(value);
    data[offset + 1] = static_cast<uint8_t>(value >> 8);
}

/**
 * Writes the value as a little-endian 32-bit word at the offset.
 */
inline void Put32(Bytes &data, size_t offset, uint32_t value) {
    Put16(data, offset, value & 0xFFFF);
    Put16(data, offset + 2, value >> 16);
}

/**
 * Appends the bytes of one buffer to another.
 */
inline void Append(Bytes &to, const Bytes &from) {
    to.insert(to.end(), from.begin(), from.end());
}

/**
 * One TIM2 picture to encode with Tim2.
 */
struct Picture {
    int                type = game::TIM2_RGB32; /**< TIM2 image type. */
    int                width = 0;
    int                height = 0;
    std::vector<Bytes> levels;          /**< Pixel data per mip level, base first. */
    Bytes              clut;            /**< 32-bit CLUT entries in file order. */
    int                clut_colors = 0; /**< Number of CLUT entries. */
};

/**
 * A TIM2 file with one picture: the 16-byte file header, a picture header (with the mipmap
 * header when there is more than one level), the levels and the CLUT.
 */
inline Bytes Tim2(const Picture &picture) {
    bool     mipmapped = picture.levels.size() > 1;
    uint32_t header_size = mipmapped ? 0x60 : 0x30;
    uint32_t image_size = 0;
    for (const Bytes &level : picture.levels) {
        image_size += static_cast<uint32_t>(level.size());
    }
    Bytes file(16 + header_size, 0);
    std::memcpy(file.data(), "TIM2", 4);
    file[4] = 4;
    Put16(file, 6, 1);
    constexpr size_t picture_header = 16;
    Put32(file, picture_header + 0x00, header_size + image_size + static_cast<uint32_t>(picture.clut.size()));
    Put32(file, picture_header + 0x04, static_cast<uint32_t>(picture.clut.size()));
    Put32(file, picture_header + 0x08, image_size);
    Put16(file, picture_header + 0x0C, header_size);
    Put16(file, picture_header + 0x0E, static_cast<uint32_t>(picture.clut_colors));
    file[picture_header + 0x11] = static_cast<uint8_t>(picture.levels.size());
    file[picture_header + 0x12] = picture.clut.empty() ? 0 : 3;
    file[picture_header + 0x13] = static_cast<uint8_t>(picture.type);
    Put16(file, picture_header + 0x14, static_cast<uint32_t>(picture.width));
    Put16(file, picture_header + 0x16, static_cast<uint32_t>(picture.height));
    if (mipmapped) {
        for (size_t level = 0; level < picture.levels.size(); level++) {
            Put32(file, picture_header + 0x40 + level * 4, static_cast<uint32_t>(picture.levels[level].size()));
        }
    }
    for (const Bytes &level : picture.levels) {
        Append(file, level);
    }
    Append(file, picture.clut);
    while (file.size() % 16 != 0) {
        file.push_back(0);
    }
    return file;
}

/**
 * An IMG archive ("IM2" when the 8-bit pixels are pre-swizzled): header, 48-byte entries, pictures.
 */
inline Bytes Img(const std::vector<std::pair<std::string, Bytes>> &pictures, bool im2 = false) {
    Bytes file(16 + 48 * pictures.size(), 0);
    std::memcpy(file.data(), im2 ? "IM2" : "IMG", 3);
    Put32(file, 4, static_cast<uint32_t>(pictures.size()));
    for (size_t index = 0; index < pictures.size(); index++) {
        const auto &[name, picture] = pictures[index];
        size_t entry = 16 + 48 * index;
        std::memcpy(&file[entry], name.c_str(), name.size() + 1);
        Put32(file, entry + 32, static_cast<uint32_t>(file.size()));
        Append(file, picture);
    }
    return file;
}

/**
 * The texels as little-endian 32-bit words.
 */
inline Bytes Rgba32(std::initializer_list<uint32_t> texels) {
    Bytes bytes;
    for (uint32_t texel : texels) {
        for (int shift = 0; shift < 32; shift += 8) {
            bytes.push_back(static_cast<uint8_t>(texel >> shift));
        }
    }
    return bytes;
}

/**
 * GS bytes r, g, b, a as a little-endian word.
 */
constexpr uint32_t Gs(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 0x80) {
    return static_cast<uint32_t>(r) | static_cast<uint32_t>(g) << 8 | static_cast<uint32_t>(b) << 16 |
           static_cast<uint32_t>(a) << 24;
}

/**
 * A 256-entry CLUT in file order, black except for the given file positions.
 */
inline Bytes Clut256(std::initializer_list<std::pair<int, uint32_t>> entries) {
    std::vector<uint32_t> words(256, Gs(0, 0, 0));
    for (auto [position, value] : entries) {
        words[position] = value;
    }
    Bytes bytes(1024);
    std::memcpy(bytes.data(), words.data(), 1024);
    return bytes;
}

/**
 * Texture staging buffer; static so it sits below 4 GiB, as SetBuffer aligns the pointer
 * through an int.
 */
alignas(128) inline u_long128 staging[1 << 16];

/**
 * A renderer and a freshly initialised texture manager staged in the fixture's buffer.
 */
struct TexEnv {
    dc::test::GfxFixture gfx;

    TexEnv() {
        game::TexManager.Initialize(16352);
        game::TexManager.SetBuffer(staging, static_cast<int>(std::size(staging)));
    }

    ~TexEnv() { game::TexManager.Initialize(16352); }

    /**
     * Draws the texture over a logical rect, texel rect (u0, v0)-(u1, v1), nearest.
     */
    void Draw(const render::PortTextureRef &ref, float x, float y, float w, float h, float u0, float v0, float u1, float v1,
              const gfx::DrawState &state = {}) {
        gfx::TextureBinding binding = ref.binding;
        binding.filter = gfx::Filter::Nearest;
        auto quad = dc::test::Quad(x, y, w, h, {0x80, 0x80, 0x80, 0x80}, u0, v0, u1, v1);
        gfx::Draw2D(gfx::Primitive::Quads, quad, binding, state);
    }
};

/**
 * A draw state blending (Cs - Cd) * As + Cd.
 */
inline gfx::DrawState AlphaBlend() {
    gfx::DrawState state;
    state.blend = true;
    state.alpha = gfx::GsBlend{0, 1, 0, 1, 0};
    return state;
}

} // namespace dc::test::texfix
