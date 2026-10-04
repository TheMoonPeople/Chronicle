#pragma once

#include <SDL3/SDL.h>
#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <vector>

#include "gfx/gfx.hpp"
#include "platform/window.hpp"

namespace dc::test {

// DC_GFX_OFFSCREEN=1 runs every gfx test on the renderer's surface-less path.
inline bool OffscreenByDefault() {
    const char *setting = std::getenv("DC_GFX_OFFSCREEN");
    return setting != nullptr && *setting != '\0' && *setting != '0';
}

// What a fixture changes in gfx::RendererConfig beyond its size and scale.
struct GfxOptions {
    bool dynamic_color_write_mask = true;
    bool offscreen = OffscreenByDefault();
    bool triangle_fans = true;
    bool separate_stencil_masks = true;
    std::filesystem::path pipeline_cache = std::filesystem::temp_directory_path() / "dc_gfx_test" / "pipeline_cache.bin";
};

// A headless window and renderer with validation on. Any validation message fails the test.
struct GfxFixture {
    explicit GfxFixture(int width = 640, int height = 480, float render_scale = 1.0f,
                        bool dynamic_color_write_mask = true)
        : GfxFixture(width, height, render_scale, GfxOptions{.dynamic_color_write_mask = dynamic_color_write_mask}) {}

    GfxFixture(int width, int height, float render_scale, const GfxOptions &options) {
        // Synchronization validation too, unless the environment already chose layer features.
        setenv("VK_LAYER_ENABLES", "VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT", 0);
        WindowConfig window{width, height, true};
        window.vulkan = !options.offscreen;
        WindowInit(window);
        gfx::RendererConfig config;
        config.validation = true;
        config.render_scale = render_scale;
        config.dynamic_color_write_mask = options.dynamic_color_write_mask;
        config.offscreen = options.offscreen;
        config.triangle_fans = options.triangle_fans;
        config.separate_stencil_masks = options.separate_stencil_masks;
        config.pipeline_cache = options.pipeline_cache;
        config.progress = [this](uint32_t done, uint32_t total) {
            progress_calls++;
            progress_done = done;
            progress_total = total;
        };
        gfx::RendererInit(WindowHandle(), config);
    }

    ~GfxFixture() {
        gfx::RendererShutdown();
        WindowShutdown();
        if (gfx::ValidationMessageCount() != 0) {
            ADD_FAILURE() << gfx::ValidationMessageCount() << " validation messages";
        }
    }

    // Draws nothing but what record does, on a cleared frame, and reads the frame back.
    template <class Record>
    void Frame(const std::array<uint8_t, 4> &clear, Record record) {
        ASSERT_TRUE(gfx::BeginFrame());
        gfx::Clear(true, clear.data(), true, 0.0f);
        record();
        gfx::EndFrame();
        ASSERT_TRUE(gfx::ReadbackFrame(pixels, width, height));
    }

    std::array<uint8_t, 4> Pixel(uint32_t x, uint32_t y) const {
        const uint8_t *p = &pixels[(static_cast<size_t>(y) * width + x) * 4];
        return {p[0], p[1], p[2], p[3]};
    }

    bool PixelNear(uint32_t x, uint32_t y, int r, int g, int b, int tolerance = 2) const {
        std::array<uint8_t, 4> p = Pixel(x, y);
        bool                   near = std::abs(p[0] - r) <= tolerance && std::abs(p[1] - g) <= tolerance &&
                                      std::abs(p[2] - b) <= tolerance;
        if (!near) {
            std::fprintf(stderr, "pixel %u,%u is %d,%d,%d, expected %d,%d,%d\n", x, y, p[0], p[1], p[2], r, g,
                         b);
        }
        return near;
    }

    std::vector<uint8_t> pixels;
    uint32_t             width = 0;
    uint32_t             height = 0;
    uint32_t             progress_calls = 0;
    uint32_t             progress_done = 0;
    uint32_t             progress_total = 0;
};

inline gfx::Vertex2D Vertex(float x, float y, float u, float v, std::array<uint8_t, 4> color,
                            float z = 0.0f) {
    gfx::Vertex2D vertex = {};
    vertex.x = x;
    vertex.y = y;
    vertex.z = z;
    vertex.u = u;
    vertex.v = v;
    vertex.color[0] = color[0];
    vertex.color[1] = color[1];
    vertex.color[2] = color[2];
    vertex.color[3] = color[3];
    vertex.fog = 0xFF;
    return vertex;
}

// A rectangle as one quad, uv over [u0, u1] x [v0, v1].
inline std::array<gfx::Vertex2D, 4> Quad(float x, float y, float w, float h, std::array<uint8_t, 4> color,
                                         float u0 = 0, float v0 = 0, float u1 = 0, float v1 = 0,
                                         float z = 0.0f) {
    return {Vertex(x, y, u0, v0, color, z), Vertex(x + w, y, u1, v0, color, z),
            Vertex(x + w, y + h, u1, v1, color, z), Vertex(x, y + h, u0, v1, color, z)};
}

inline uint32_t Rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 0xFF) {
    return static_cast<uint32_t>(r) | static_cast<uint32_t>(g) << 8 | static_cast<uint32_t>(b) << 16 |
           static_cast<uint32_t>(a) << 24;
}

} // namespace dc::test
