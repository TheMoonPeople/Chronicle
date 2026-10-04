#include <gtest/gtest.h>
#include <unistd.h>

#include <fstream>
#include <iterator>
#include <string>

#include "gfx_fixture.hpp"

using namespace dc::test;

namespace {

uint32_t ReadBigEndian(const uint8_t *p) {
    return static_cast<uint32_t>(p[0]) << 24 | static_cast<uint32_t>(p[1]) << 16 |
           static_cast<uint32_t>(p[2]) << 8 | p[3];
}

uint32_t Crc(const uint8_t *data, size_t size) {
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < size; i++) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; bit++) {
            crc = (crc & 1) ? 0xEDB88320u ^ (crc >> 1) : crc >> 1;
        }
    }
    return ~crc;
}

} // namespace

TEST(GfxMisc, PipelineCache) {
    // Its own file: the other cases share theirs, and save over it while they run.
    std::filesystem::path cache = std::filesystem::temp_directory_path() / "dc_gfx_test" /
                                  ("pipeline_cache_" + std::to_string(getpid()) + ".bin");
    std::filesystem::remove(cache);
    GfxOptions options{.pipeline_cache = cache};
    {
        GfxFixture fixture(640, 480, 1.0f, options);
        ASSERT_TRUE(gfx::PipelineCount() > 0);
        ASSERT_TRUE(fixture.progress_calls > 0);
        ASSERT_TRUE(fixture.progress_done == fixture.progress_total);
        ASSERT_TRUE(fixture.progress_total == gfx::PipelineCount());
        ASSERT_TRUE(std::filesystem::exists(cache));
    }
    // A cache from another device or a corrupt file is ignored, not fed to the driver.
    {
        std::ofstream corrupt(cache, std::ios::binary | std::ios::trunc);
        corrupt << "not a pipeline cache at all, but long enough to have a header";
    }
    {
        GfxFixture fixture(640, 480, 1.0f, options);
        ASSERT_TRUE(gfx::PipelineCount() > 0);
    }
    ASSERT_GE(std::filesystem::file_size(cache), 32u);
    std::filesystem::remove(cache);
}

TEST(GfxMisc, TextureLimits) {
    GfxFixture fixture;
    // Past the device's maxImageDimension2D: an error, not a crash.
    ASSERT_TRUE(gfx::CreateTexture({1u << 20, 4, gfx::TextureFormat::Rgba8, 1, true}) == gfx::kNullTexture);
    ASSERT_TRUE(gfx::CreateTexture({0, 4, gfx::TextureFormat::Rgba8, 1, true}) == gfx::kNullTexture);
    ASSERT_TRUE(gfx::CreateTexture({4, 4, gfx::TextureFormat::Rgba8, 4, true}) == gfx::kNullTexture);
    ASSERT_TRUE(gfx::CreateRenderTarget(1u << 20, 4, true) == gfx::kNullTexture);

    gfx::TextureHandle texture = gfx::CreateTexture({4, 4, gfx::TextureFormat::Rgba8, 3, true});
    ASSERT_TRUE(texture != gfx::kNullTexture);
    uint32_t texel = Rgba(1, 2, 3);
    ASSERT_TRUE(gfx::UpdateTexture(texture, 2, 0, 0, 1, 1, &texel));
    ASSERT_TRUE(!gfx::UpdateTexture(texture, 2, 0, 0, 2, 1, &texel));
    ASSERT_TRUE(!gfx::UpdateTexture(texture, 3, 0, 0, 1, 1, &texel));
    gfx::DestroyTexture(texture);
    ASSERT_TRUE(!gfx::UpdateTexture(texture, 0, 0, 0, 1, 1, &texel));

    // Handles of destroyed textures stay dead after their slot is reused.
    for (int frame = 0; frame < 3; frame++) {
        ASSERT_TRUE(gfx::BeginFrame());
        gfx::EndFrame();
    }
    gfx::TextureHandle reused = gfx::CreateTexture({4, 4, gfx::TextureFormat::Rgba8, 1, true});
    ASSERT_TRUE(reused != texture);
    ASSERT_TRUE(!gfx::GetTextureInfo(texture).has_value());
    ASSERT_TRUE(gfx::GetTextureInfo(reused).has_value());

    // GS alpha to the renderer's: 0x80 is opaque.
    uint32_t alpha[3] = {Rgba(0, 0, 0, 0x80), Rgba(0, 0, 0, 0x40), Rgba(0, 0, 0, 0xFF)};
    gfx::ConvertPs2Alpha(alpha, 3);
    ASSERT_TRUE(alpha[0] >> 24 == 0xFF && alpha[1] >> 24 == 0x80 && alpha[2] >> 24 == 0xFF);
}

TEST(GfxMisc, DrawsOutsideAFrameAreDropped) {
    GfxFixture fixture;
    auto       quad = Quad(0, 0, 640, 480, {255, 255, 255, 0x80});
    gfx::Draw2D(gfx::Primitive::Quads, quad, {}, gfx::DrawState{});
    fixture.Frame({0, 0, 0, 0x80}, [] {});
    ASSERT_TRUE(fixture.PixelNear(320, 240, 0, 0, 0));
}

TEST(GfxMisc, Png) {
    GfxFixture fixture(64, 48);
    fixture.Frame({10, 20, 30, 0x80}, [] {
        auto quad = Quad(0, 0, 320, 480, {200, 100, 50, 0x80});
        gfx::Draw2D(gfx::Primitive::Quads, quad, {}, gfx::DrawState{});
    });
    std::filesystem::path path = std::filesystem::temp_directory_path() / "dc_gfx_test" / "frame.png";
    ASSERT_TRUE(gfx::WritePng(path, fixture.pixels.data(), fixture.width, fixture.height));

    std::ifstream        file(path, std::ios::binary);
    std::vector<uint8_t> png((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    const uint8_t        signature[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    ASSERT_TRUE(png.size() > 8 && std::equal(signature, signature + 8, png.begin()));

    std::vector<uint8_t> zlib;
    size_t               pos = 8;
    while (pos + 12 <= png.size()) {
        uint32_t       length = ReadBigEndian(&png[pos]);
        const uint8_t *type = &png[pos + 4];
        ASSERT_TRUE(ReadBigEndian(&png[pos + 8 + length]) == Crc(type, length + 4));
        if (std::equal(type, type + 4, "IHDR")) {
            ASSERT_TRUE(ReadBigEndian(type + 4) == 64 && ReadBigEndian(type + 8) == 48);
            ASSERT_TRUE(type[12] == 8 && type[13] == 2);
        } else if (std::equal(type, type + 4, "IDAT")) {
            zlib.insert(zlib.end(), type + 4, type + 4 + length);
        }
        pos += 12 + length;
    }
    ASSERT_TRUE(pos == png.size());

    // Stored deflate blocks only: undo them by hand and compare with the frame.
    std::vector<uint8_t> raw;
    size_t               at = 2;
    bool                 last;
    do {
        last = zlib[at] & 1;
        ASSERT_TRUE((zlib[at] & 6) == 0);
        uint32_t len = zlib[at + 1] | zlib[at + 2] << 8;
        ASSERT_TRUE((len ^ (zlib[at + 3] | zlib[at + 4] << 8)) == 0xFFFF);
        raw.insert(raw.end(), zlib.begin() + static_cast<std::ptrdiff_t>(at + 5),
                   zlib.begin() + static_cast<std::ptrdiff_t>(at + 5 + len));
        at += 5 + len;
    } while (!last);
    ASSERT_TRUE(raw.size() == (64 * 3 + 1) * 48);
    ASSERT_TRUE(raw[0] == 0 && raw[1] == 200 && raw[2] == 100 && raw[3] == 50);
    size_t right = 10 * (64 * 3 + 1) + 1 + 60 * 3;
    ASSERT_TRUE(raw[right] == 10 && raw[right + 1] == 20 && raw[right + 2] == 30);
}

TEST(GfxMisc, TextureReusingFrameTargetKeepsTexelCoordinates) {
    GfxFixture fixture(1280, 960);
    gfx::SetFrameLayout({gfx::AspectMode::Fill, 1.0f});
    gfx::TextureHandle frame = gfx::CreateRenderTarget(640, 256, true, false, true);
    ASSERT_NE(frame, gfx::kNullTexture);
    gfx::DestroyTexture(frame);
    for (int i = 0; i < 3; i++) {
        ASSERT_TRUE(gfx::BeginFrame());
        gfx::EndFrame();
    }

    gfx::TextureHandle texture = gfx::CreateTexture({4, 1, gfx::TextureFormat::Rgba8, 1, true});
    ASSERT_NE(texture, gfx::kNullTexture);
    uint32_t texels[] = {Rgba(255, 0, 0), Rgba(0, 255, 0), Rgba(0, 0, 255), Rgba(255, 255, 255)};
    ASSERT_TRUE(gfx::UpdateTexture(texture, 0, 0, 0, 4, 1, texels));
    fixture.Frame({0, 0, 0, 0x80}, [&] {
        gfx::TextureBinding binding;
        binding.texture = texture;
        binding.filter = gfx::Filter::Nearest;
        auto quad = Quad(100, 100, 40, 10, {0x80, 0x80, 0x80, 0x80}, 0, 0, 4, 1);
        gfx::Draw2D(gfx::Primitive::Quads, quad, binding, gfx::DrawState{});
    });
    EXPECT_TRUE(fixture.PixelNear(210, 210, 255, 0, 0));
    EXPECT_TRUE(fixture.PixelNear(230, 210, 0, 255, 0));
    EXPECT_TRUE(fixture.PixelNear(250, 210, 0, 0, 255));
    EXPECT_TRUE(fixture.PixelNear(270, 210, 255, 255, 255));
    EXPECT_FALSE(gfx::GetTextureInfo(texture)->frame_target);
}
