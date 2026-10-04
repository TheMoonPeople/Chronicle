#include <gtest/gtest.h>

#include <random>

#include "tex_fixture.hpp"

using namespace dc::test;
using namespace texfix;

namespace {

constexpr std::array<uint8_t, 4> kWhite = {255, 255, 255, 0x80};
constexpr std::array<uint8_t, 4> kGreen = {0, 255, 0, 0x80};

void Enter(Bytes &img, int block, int mipmap = 0) {
    TexManager.BeginEnterTextureBlock(block);
    TexManager.EnterIMGFile(img.data(), block, mipmap, 0);
    TexManager.EndEnterTextureBlock(block);
}

PortTextureRef Resolve(const char *name) {
    std::string mutable_name(name);
    return PortTextureFromCTexture(TexManager.GetTexture(mutable_name.data(), -1));
}

// Retail's Conv8to32, PageConv8to32 and BlockConv8to32 (ps2/src/texture.cpp), the specification of
// the "IM2" layout; they are static there, so they are restated here.
void BlockConv8to32(const uint8_t *source, uint8_t *destination) {
    static const int lut[128] = {
        0, 36, 8, 44, 1, 37, 9, 45, 2, 38, 10, 46, 3, 39, 11, 47, 4, 32, 12, 40, 5, 33, 13, 41,
        6, 34, 14, 42, 7, 35, 15, 43, 16, 52, 24, 60, 17, 53, 25, 61, 18, 54, 26, 62, 19, 55, 27, 63,
        20, 48, 28, 56, 21, 49, 29, 57, 22, 50, 30, 58, 23, 51, 31, 59, 4, 32, 12, 40, 5, 33, 13, 41,
        6, 34, 14, 42, 7, 35, 15, 43, 0, 36, 8, 44, 1, 37, 9, 45, 2, 38, 10, 46, 3, 39, 11, 47,
        20, 48, 28, 56, 21, 49, 29, 57, 22, 50, 30, 58, 23, 51, 31, 59, 16, 52, 24, 60, 17, 53, 25, 61,
        18, 54, 26, 62, 19, 55, 27, 63};
    int out_index = 0;
    for (unsigned i = 0; i < 4; i++) {
        int lut_index = (i & 1) * 64;
        for (unsigned j = 0; j < 16; j++) {
            for (unsigned k = 0; k < 4; k++) {
                destination[out_index++] = source[lut[lut_index++]];
            }
        }
        source += 64;
    }
}

void PageConv8to32(const uint8_t *source, uint8_t *destination) {
    static const int block_table[32] = {0, 1, 4, 5, 16, 17, 20, 21, 2, 3, 6, 7, 18, 19, 22, 23,
                                        8, 9, 12, 13, 24, 25, 28, 29, 10, 11, 14, 15, 26, 27, 30, 31};
    int              block_column[32];
    int              block_row[32];
    int              entry = 0;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 8; j++) {
            block_column[block_table[entry]] = j;
            block_row[block_table[entry]] = i;
            entry++;
        }
    }
    uint8_t work8[256];
    uint8_t work32[256];
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 8; j++) {
            const uint8_t *source_cursor = source + i * 2048 + j * 16;
            for (int k = 0; k < 16; k++) {
                std::memcpy(work8 + k * 16, source_cursor + k * 128, 16);
            }
            BlockConv8to32(work8, work32);
            int      block_index = block_table[i * 8 + j];
            uint8_t *destination_cursor = destination + block_row[block_index] * 2048 + block_column[block_index] * 32;
            for (int k = 0; k < 8; k++) {
                std::memcpy(destination_cursor + k * 256, work32 + k * 32, 32);
            }
        }
    }
}

Bytes Conv8to32(int width, int height, const Bytes &source) {
    Bytes   destination(static_cast<size_t>(width) * height);
    uint8_t work8[8192] = {};
    uint8_t work32[8192] = {};
    int     pages_x = ((width - 1) >> 7) + 1;
    int     pages_y = ((height - 1) >> 6) + 1;
    int     row_bytes;
    int     row_count;
    if (pages_x == 1) {
        row_bytes = width * 2;
    } else {
        width = 128;
        row_bytes = 256;
    }
    if (pages_y == 1) {
        row_count = height >> 1;
    } else {
        height = 64;
        row_count = 32;
    }
    for (int i = 0; i < pages_y; i++) {
        for (int j = 0; j < pages_x; j++) {
            const uint8_t *source_cursor = source.data() + i * (pages_x * (width << 6)) + width * j;
            for (int k = 0; k < height; k++) {
                std::memcpy(work8 + k * 128, source_cursor, width);
                source_cursor += width * pages_x;
            }
            PageConv8to32(work8, work32);
            uint8_t *destination_cursor = destination.data() + i * (pages_x * (row_bytes * row_count)) + row_bytes * j;
            for (int k = 0; k < row_count; k++) {
                std::memcpy(destination_cursor, work32 + k * 256, row_bytes);
                destination_cursor += row_bytes * pages_x;
            }
        }
    }
    return destination;
}

} // namespace

TEST(TexDecode, Rgb32Alpha) {
    TexEnv env;
    Bytes  img = Img({
        {"rgb32", Tim2({TIM2_RGB32, 2, 2, {Rgba32({Gs(255, 0, 0), Gs(0, 255, 0), Gs(0, 0, 255, 0x40), Gs(255, 0, 0, 0)})}, {}, 0})}
    });
    Enter(img, 0);
    PortTextureRef ref = Resolve("rgb32");
    ASSERT_TRUE(ref.valid && ref.width == 2 && ref.height == 2);
    ASSERT_TRUE(ref.binding.palette == gfx::kNullTexture);
    env.gfx.Frame(kWhite, [&] { env.Draw(ref, 0, 0, 200, 200, 0, 0, 2, 2, AlphaBlend()); });
    ASSERT_TRUE(env.gfx.PixelNear(50, 50, 255, 0, 0));
    ASSERT_TRUE(env.gfx.PixelNear(150, 50, 0, 255, 0));
    // GS alpha 0x40 is half: (Cs - Cd) * As + Cd over white.
    ASSERT_TRUE(env.gfx.PixelNear(50, 150, 128, 128, 255, 3));
    ASSERT_TRUE(env.gfx.PixelNear(150, 150, 255, 255, 255));
}

TEST(TexDecode, Rgb24TakesAlphaFromTexa) {
    TexEnv env;
    Bytes  img = Img({
        {"rgb24", Tim2({TIM2_RGB24, 2, 1, {{255, 128, 0, 0, 0, 0}}, {}, 0})}
    });
    Enter(img, 0);
    PortTextureRef ref = Resolve("rgb24");
    ASSERT_TRUE(ref.valid);
    auto info = gfx::GetTextureInfo(ref.binding.texture);
    ASSERT_TRUE(info && !info->has_alpha);
    sceGsTex0 tex0;
    std::memcpy(&tex0, &TexManager.GetTexture(1)->tex0, sizeof tex0);
    ASSERT_TRUE(tex0.PSM == SCE_GS_PSMCT24);
    // TEXA: TA0 0x80 for colour, AEM makes the black texel transparent.
    env.gfx.Frame(kWhite, [&] { env.Draw(ref, 0, 0, 200, 100, 0, 0, 2, 1, AlphaBlend()); });
    ASSERT_TRUE(env.gfx.PixelNear(50, 50, 255, 128, 0));
    ASSERT_TRUE(env.gfx.PixelNear(150, 50, 255, 255, 255));
}

TEST(TexDecode, Rgb16WidensAndTakesTheAlphaBit) {
    TexEnv env;
    Bytes  pixels(4);
    Put16(pixels, 0, 0x8000 | (31 << 10) | (16 << 5));
    Put16(pixels, 2, 0x001F);
    Bytes img = Img({
        {"rgb16", Tim2({TIM2_RGB16, 2, 1, {pixels}, {}, 0})}
    });
    Enter(img, 0);
    PortTextureRef ref = Resolve("rgb16");
    ASSERT_TRUE(ref.valid);
    env.gfx.Frame(kGreen, [&] { env.Draw(ref, 0, 0, 200, 100, 0, 0, 2, 1, AlphaBlend()); });
    ASSERT_TRUE(env.gfx.PixelNear(50, 50, 0, 128, 248));
    ASSERT_TRUE(env.gfx.PixelNear(150, 50, 0, 255, 0));
}

// The GS reads an 8-bit CLUT in CSM1 with bits 3 and 4 of the index swapped: entry 8 is the
// file's ninth-from-sixteen and entry 16 the file's eighth-from-eight.
TEST(TexDecode, Idtex8ReadsTheClutInCsm1Order) {
    TexEnv env;
    Bytes  clut = Clut256({
        {8, Gs(255, 0, 0)},
        {16, Gs(0, 255, 0)},
        {0, Gs(0, 0, 255)},
        {255, Gs(255, 255, 255)},
        {31, Gs(255, 255, 0, 0x40)}
    });
    Bytes  img = Img({
        {"idx8", Tim2({TIM2_IDTEX8, 4, 1, {{8, 16, 0, 255}}, clut, 256})}
    });
    Enter(img, 0);
    PortTextureRef ref = Resolve("idx8");
    ASSERT_TRUE(ref.valid && ref.binding.palette != gfx::kNullTexture);
    CTexture *texture = TexManager.GetTexture(1);
    sceGsTex0 tex0;
    std::memcpy(&tex0, &texture->tex0, sizeof tex0);
    ASSERT_TRUE(tex0.PSM == SCE_GS_PSMT8 && tex0.CLD == 1 && tex0.CBP != 0);

    uint32_t palette[256];
    ASSERT_TRUE(PortPaletteEntries(tex0.CBP, palette));
    ASSERT_TRUE(palette[8] == Rgba(0, 255, 0));
    ASSERT_TRUE(palette[16] == Rgba(255, 0, 0));
    ASSERT_TRUE(palette[0] == Rgba(0, 0, 255));
    ASSERT_TRUE(palette[255] == Rgba(255, 255, 255));
    ASSERT_TRUE(palette[31] == Rgba(255, 255, 0, 0x80));
    for (unsigned i = 0; i < 256; i++) {
        ASSERT_TRUE(PortClutCsm1(PortClutCsm1(i)) == i);
    }
    ASSERT_TRUE(PortClutCsm1(8) == 16 && PortClutCsm1(15) == 23 && PortClutCsm1(40) == 48 && PortClutCsm1(7) == 7);

    env.gfx.Frame({0, 0, 0, 0x80}, [&] { env.Draw(ref, 0, 0, 400, 100, 0, 0, 4, 1); });
    ASSERT_TRUE(env.gfx.PixelNear(50, 50, 0, 255, 0));
    ASSERT_TRUE(env.gfx.PixelNear(150, 50, 255, 0, 0));
    ASSERT_TRUE(env.gfx.PixelNear(250, 50, 0, 0, 255));
    ASSERT_TRUE(env.gfx.PixelNear(350, 50, 255, 255, 255));
}

TEST(TexDecode, Idtex4ExpandsToIndex8) {
    TexEnv env;
    Bytes  clut = Rgba32({Gs(255, 255, 255), Gs(255, 0, 0), Gs(0, 255, 0), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                          Gs(0, 0, 255)});
    Bytes  img = Img({
        {"idx4", Tim2({TIM2_IDTEX4, 4, 1, {{0x21, 0x0F}}, clut, 16})}
    });
    Enter(img, 0);
    PortTextureRef ref = Resolve("idx4");
    ASSERT_TRUE(ref.valid && ref.width == 4);
    auto info = gfx::GetTextureInfo(ref.binding.texture);
    ASSERT_TRUE(info && info->format == gfx::TextureFormat::Index8);
    std::vector<uint8_t> indices;
    uint32_t             width;
    uint32_t             height;
    ASSERT_TRUE(gfx::ReadbackTexture(ref.binding.texture, indices, width, height));
    ASSERT_TRUE(width == 4 && indices[0] == 1 && indices[1] == 2 && indices[2] == 15 && indices[3] == 0);
    env.gfx.Frame({0, 0, 0, 0x80}, [&] { env.Draw(ref, 0, 0, 400, 100, 0, 0, 4, 1); });
    EXPECT_TRUE(env.gfx.PixelNear(50, 50, 255, 0, 0));
    EXPECT_TRUE(env.gfx.PixelNear(150, 50, 0, 255, 0));
    EXPECT_TRUE(env.gfx.PixelNear(250, 50, 0, 0, 255));
    EXPECT_TRUE(env.gfx.PixelNear(350, 50, 255, 255, 255));
}

TEST(TexDecode, Im2UnswizzleInvertsConv8to32) {
    std::mt19937 random(7);
    const int    sizes[][2] = {
        {16,  16 },
        {32,  16 },
        {64,  32 },
        {128, 64 },
        {64,  128},
        {256, 64 },
        {256, 256},
        {512, 128}
    };
    for (auto [width, height] : sizes) {
        Bytes linear(static_cast<size_t>(width) * height);
        for (uint8_t &texel : linear) {
            texel = static_cast<uint8_t>(random());
        }
        Bytes swizzled = Conv8to32(width, height, linear);
        Bytes back(linear.size());
        PortUnswizzle8(width, height, swizzled.data(), back.data());
        if (back != linear) {
            std::fprintf(stderr, "unswizzle differs at %dx%d\n", width, height);
        }
        ASSERT_TRUE(back == linear);
    }
}

TEST(TexDecode, Im2ArchiveDrawsUnswizzled) {
    TexEnv env;
    Bytes  linear(32 * 32);
    for (int y = 0; y < 32; y++) {
        for (int x = 0; x < 32; x++) {
            linear[y * 32 + x] = static_cast<uint8_t>(1 + x / 16 + 2 * (y / 16));
        }
    }
    Bytes mip(16 * 16);
    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 16; x++) {
            mip[y * 16 + x] = static_cast<uint8_t>(1 + x / 8 + 2 * (y / 8));
        }
    }
    Bytes clut = Clut256({
        {1, Gs(255, 0,   0)  },
        {2, Gs(0,   255, 0)  },
        {3, Gs(0,   0,   255)},
        {4, Gs(255, 255, 255)}
    });
    Bytes img = Img({
                        {"swz", Tim2({TIM2_IDTEX8, 32, 32, {Conv8to32(32, 32, linear), Conv8to32(16, 16, mip)}, clut, 256})}
    },
                    true);
    Enter(img, 0, 1);
    PortTextureRef ref = Resolve("swz");
    ASSERT_TRUE(ref.valid);
    std::vector<uint8_t> indices;
    uint32_t             width;
    uint32_t             height;
    ASSERT_TRUE(gfx::ReadbackTexture(ref.binding.texture, indices, width, height));
    ASSERT_TRUE(Bytes(indices.begin(), indices.end()) == linear);
    auto info = gfx::GetTextureInfo(ref.binding.texture);
    ASSERT_TRUE(info && info->mip_levels == 2);
    env.gfx.Frame({0, 0, 0, 0x80}, [&] { env.Draw(ref, 0, 0, 320, 320, 0, 0, 32, 32); });
    EXPECT_TRUE(env.gfx.PixelNear(80, 80, 255, 0, 0));
    EXPECT_TRUE(env.gfx.PixelNear(240, 80, 0, 255, 0));
    EXPECT_TRUE(env.gfx.PixelNear(80, 240, 0, 0, 255));
    EXPECT_TRUE(env.gfx.PixelNear(240, 240, 255, 255, 255));
}

TEST(TexDecode, MipmapsAsSupplied) {
    TexEnv env;
    Bytes  base(8 * 8 * 4, 0x40);
    Bytes  half(4 * 4 * 4, 0x80);
    Bytes  quarter(2 * 2 * 4, 0xC0);
    Bytes  img = Img({
        {"mips",  Tim2({TIM2_RGB32, 8, 8, {base, half, quarter}, {}, 0})},
        {"plain", Tim2({TIM2_RGB32, 8, 8, {base, half, quarter}, {}, 0})}
    });
    Enter(img, 0, 1);
    PortTextureRef ref = Resolve("mips");
    ASSERT_TRUE(ref.valid);
    auto info = gfx::GetTextureInfo(ref.binding.texture);
    ASSERT_TRUE(info && info->mip_levels == 3);
    std::string name = "mips";
    CTexture   *texture = TexManager.GetTexture(name.data(), -1);
    ASSERT_TRUE(texture->tex1 == SCE_GS_SET_TEX1(0, 2, 1, 5, 1, 0, -120));
    ASSERT_TRUE(texture->image[1] != nullptr && texture->image[2] != nullptr);
    // TEX1 MMAG picks the filter.
    ASSERT_TRUE(PortTextureFromTex0(texture->tex0, texture->tex1).binding.filter == gfx::Filter::Linear);
    ASSERT_TRUE(PortTextureFromTex0(texture->tex0, 0).binding.filter == gfx::Filter::Nearest);

    Bytes again = img;
    TexManager.BeginEnterTextureBlock(1);
    TexManager.EnterIMGFile(again.data(), 1, 0, 0);
    TexManager.EndEnterTextureBlock(1);
    name = "plain";
    CTexture *plain = TexManager.GetTexture(name.data(), 1);
    ASSERT_TRUE(plain != nullptr && plain->tex1 == 0);
    auto plain_info = gfx::GetTextureInfo(PortTextureFromCTexture(plain).binding.texture);
    ASSERT_TRUE(plain_info && plain_info->mip_levels == 1);
}
