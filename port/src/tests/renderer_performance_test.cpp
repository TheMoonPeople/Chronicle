#include <SDL3/SDL.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <memory>
#include <thread>
#include <vector>

#include "app/gameloop.hpp"
#include "cloth.hpp"
#include "frame.hpp"
#include "gfx/displaylist.hpp"
#include "gfx_fixture.hpp"
#include "menu_draw.hpp"
#include "menu_inventory.hpp"
#include "platform/clock.hpp"
#include "render/draw3d.hpp"
#include "render/texture_registry.hpp"
#include "renderinfo.hpp"
#include "texture.hpp"

namespace dc::test {

using namespace dc::game;
using namespace dc::render;

TEST(ChroniclePresentation, FullSizePictureFillsWidescreenWithoutSamplingPastArtwork) {
    dc::test::GfxFixture fixture(1280, 720, 1.0f, dc::test::GfxOptions{.offscreen = true});
    PortDecodedTexture   image;
    image.width = 640;
    image.height = 448;
    image.levels.emplace_back(640 * 448 * 4, 255);
    for (unsigned y = 0; y < image.height; ++y) {
        for (unsigned x = 0; x < image.width; ++x) {
            const size_t pixel = (static_cast<size_t>(y) * image.width + x) * 4;
            image.levels[0][pixel] = static_cast<uint8_t>(y / 2);
            image.levels[0][pixel + 1] = static_cast<uint8_t>(x / 3);
        }
    }
    const unsigned key = PortCreateTexture(image, PortTextureOwner::Manager, nullptr);
    ASSERT_NE(key, 0u);
    CTexture texture;
    texture.tex0 = SCE_GS_SET_TEX0(key, 10, 0, 10, 9, 1, 0, 0, 0, 0, 0, 0);
    fixture.Frame({0, 0, 0, 128}, [&] { DrawFullSizePicture(&texture, 0, 0, 128); });
    EXPECT_TRUE(fixture.PixelNear(80, 360, 112, 13, 255));
    EXPECT_TRUE(fixture.PixelNear(1200, 360, 112, 200, 255));
    EXPECT_TRUE(fixture.PixelNear(640, 360, 112, 106, 255));
    EXPECT_TRUE(fixture.PixelNear(640, 684, 212, 106, 255));
    PortReleaseKey(key);
}

TEST(ChroniclePresentation, ClearedVisualBlocksAreRejectedBetweenSweeps) {
    alignas(16) std::array<unsigned int, 16> block = {};
    Draw3DRegisterVisual(block.data(), false);
    ASSERT_NE(Draw3DFindVisual(block.data()), nullptr);
    block[8] = 0;
    EXPECT_EQ(Draw3DFindVisual(block.data()), nullptr);
}

TEST(ChroniclePresentation, ClothTurnsWithItsAttachmentWithoutBlendingAcrossTheBody) {
    struct Cloth final : CCloth {
        using CCloth::CCloth;
    };

    dc::test::GfxFixture fixture(64, 48, 1.0f, dc::test::GfxOptions{.offscreen = true});
    auto                 cloth = std::make_unique<Cloth>(2, 2, 1.0f);
    auto                 block = std::make_unique<unsigned int[]>(static_cast<size_t>(kDraw3DBlockQuads) * 4);
    cloth->vu_block[0] = cloth->vu_block[1] = block.get();
    cloth->polygon_divide[0] = 0;
    CFrame attachment;
    cloth->frame = &attachment;
    RenderInfo    info = {};
    sceVu0FMATRIX identity;
    sceVu0UnitMatrix(identity);
    sceVu0UnitMatrix(info.view_scaled);
    info.scale[0] = info.scale[1] = 1.0f;

    std::vector<gfx::Vertex3D> previous;
    for (int tick = 0; tick < 2; ++tick) {
        attachment.SetPosition(static_cast<float>(tick * 4), 2.0f, 3.0f);
        attachment.SetRotation(0.0f, static_cast<float>(tick) * 1.57079632679f, 0.0f);
        attachment.SetScale(2.0f, 3.0f, 4.0f);
        sceVu0FMATRIX world;
        attachment.GetLWMatrix(world);
        for (int i = 0; i < 2; ++i) {
            for (int j = 0; j < 2; ++j) {
                sceVu0FVECTOR local = {2.0f, static_cast<float>(i), static_cast<float>(j), 1.0f};
                sceVu0ApplyMatrix(cloth->point[i][j], world, local);
                sceVu0CopyVector(cloth->normal_grid[i][j], world[0]);
            }
        }
        gfx::BeginRecording();
        cloth->DrawVu1(static_cast<u_int *>(nullptr), identity, &info, VU1_PROGRAM_UNKNOWN6, nullptr, 0, 0);
        const auto list = gfx::EndRecording();
        ASSERT_EQ(list->records.size(), 1u);
        const auto &record = list->records.front();
        EXPECT_EQ(record.key, reinterpret_cast<gfx::InterpKey>(cloth.get()));
        EXPECT_TRUE(record.blend_vertices);
        const auto &mesh = std::get<gfx::detail::MeshEntry>(list->entries[record.entry]);
        ASSERT_EQ(mesh.vertices.size(), 4u);
        for (size_t v = 0; v < mesh.vertices.size(); ++v) {
            const auto &vertex = mesh.vertices[v];
            const int   i = (v % 2 == 0) ? 0 : 1;
            const int   j = static_cast<int>(v / 2);
            for (int axis = 0; axis < 3; ++axis) {
                const float world_point = record.model[axis] * vertex.position[0] +
                                          record.model[4 + axis] * vertex.position[1] +
                                          record.model[8 + axis] * vertex.position[2] + record.model[12 + axis];
                EXPECT_NEAR(world_point, cloth->point[i][j][axis], 0.0001f);
                if (tick != 0) {
                    EXPECT_NEAR(vertex.position[axis], previous[v].position[axis], 0.0001f);
                    EXPECT_NEAR(vertex.normal[axis], previous[v].normal[axis], 0.0001f);
                }
            }
        }
        previous.assign(mesh.vertices.begin(), mesh.vertices.end());
    }
    Draw3DSweepVisuals();
}

TEST(ChroniclePresentation, AsyncPublishesAndStopsTheWorker) {
    dc::test::GfxFixture fixture(64, 48, 1.0f,
                                 dc::test::GfxOptions{.offscreen = true, .render_thread = gfx::RenderThreadMode::Async});
    ClockSetUnbounded(false);
    ClockSetTickRate(60);
    ClockReset();
    app::GameSetPresentSettings({});
    const auto before = app::GamePresentStatistics().display_frames;
    {
        gfx::RenderLease lease;
        gfx::BeginRecording();
        const std::array<uint8_t, 4> color = {20, 40, 60, 128};
        gfx::Clear(true, color.data(), true, 0);
        app::GameRenderTick(gfx::EndRecording());
    }
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (app::GamePresentStatistics().display_frames == before && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    app::GameStopPresentation();
    EXPECT_GT(app::GamePresentStatistics().display_frames, before);
    const auto stopped = app::GamePresentStatistics().display_frames;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_EQ(app::GamePresentStatistics().display_frames, stopped);
}

TEST(GfxGpuChroniclePresentation, AsyncPresentationReleasesDestroyedTexturesAtStop) {
    dc::test::GfxFixture fixture(64, 48, 1.0f,
                                 dc::test::GfxOptions{.offscreen = true, .texture_slots = 24, .render_thread = gfx::RenderThreadMode::Async});
    ClockSetUnbounded(false);
    ClockSetTickRate(60);
    ClockReset();
    app::GameSetPresentSettings({});
    std::vector<std::weak_ptr<const gfx::DisplayList>> frames;
    constexpr unsigned                                 count = 40;
    {
        gfx::RenderLease lease;
        for (unsigned tick = 0; tick < count; ++tick) {
            gfx::BeginRecording();
            const std::array<uint8_t, 4> clear = {0, 0, 0, 128};
            gfx::Clear(true, clear.data(), true, 0);
            auto cpu_list = gfx::EndRecording();
            frames.emplace_back(cpu_list);
            app::GameRenderTick(std::move(cpu_list));
            const auto texture = gfx::CreateTexture({1, 1});
            ASSERT_NE(texture, gfx::kNullTexture);
            const auto texel = dc::test::Rgba(static_cast<uint8_t>(40 + tick), 80, 120);
            ASSERT_TRUE(gfx::UpdateTexture(texture, 0, 0, 0, 1, 1, &texel));
            gfx::BeginRecording();
            gfx::Clear(true, clear.data(), true, 0);
            gfx::TextureBinding binding;
            binding.texture = texture;
            const auto quad = dc::test::Quad(0, 0, 640, 480, {128, 128, 128, 128}, 0, 0, 1, 1);
            gfx::Draw2D(gfx::Primitive::Quads, quad, binding, {});
            gfx::DestroyTexture(texture);
            auto list = gfx::EndRecording();
            frames.emplace_back(list);
            app::GameRenderTick(std::move(list));
        }
        EXPECT_LE(std::ranges::count_if(frames, [](const auto &frame) { return !frame.expired(); }), 8);
    }
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!frames.front().expired() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    EXPECT_TRUE(frames.front().expired());
    app::GameStopPresentation();
    for (size_t index = 0; index + 2 < frames.size(); ++index) {
        EXPECT_TRUE(frames[index].expired()) << index;
    }
    ASSERT_TRUE(gfx::ReadbackFrame(fixture.pixels, fixture.width, fixture.height));
    EXPECT_TRUE(fixture.PixelNear(fixture.width / 2, fixture.height / 2, 40 + count - 1, 80, 120, 1));
}

TEST(ChroniclePresentation, CachedAdjacentStripsRetainOrderAfterMaterialChanges) {
    dc::test::GfxFixture fixture(64, 48, 1.0f, dc::test::GfxOptions{.offscreen = true});
    Draw3DVisual         external_visual;
    external_visual.vertices.resize(3);
    external_visual.indices = {0, 1, 2};
    external_visual.strips.push_back({.first_index = 0, .index_count = 3});
    Draw3DFinishVisual(external_visual);
    EXPECT_FALSE(external_visual.strip_runs_cached);
    gfx::DestroyMesh(external_visual.mesh);

    Draw3DVisual visual;
    visual.cache_strip_runs_on_finish = true;
    visual.vertices.resize(3);
    visual.indices = {0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2};
    for (uint32_t first = 0; first < 12; first += 3) {
        Draw3DStrip strip;
        strip.first_index = first;
        strip.index_count = 3;
        if (first == 6) {
            strip.diffuse[0] = 0.5f;
        }
        visual.strips.push_back(strip);
    }
    Draw3DFinishVisual(visual);
    ASSERT_TRUE(visual.strip_runs_cached);
    ASSERT_EQ(visual.strip_runs.size(), 3u);

    RenderInfo info = {};
    float      model[4][4] = {};
    for (int axis = 0; axis < 4; ++axis) {
        model[axis][axis] = 1;
        info.view_scaled[axis][axis] = 1;
    }
    info.scale[0] = info.scale[1] = 1;

    struct Draw {
        uint32_t first_index;
        uint32_t index_count;
        float    diffuse;
    };

    auto record_draws = [&] {
        gfx::BeginRecording();
        Draw3DDrawVisual(visual, model, info, 0);
        auto              list = gfx::EndRecording();
        std::vector<Draw> draws;
        for (const auto &entry : list->entries) {
            if (const auto *mesh = std::get_if<gfx::detail::MeshEntry>(&entry)) {
                draws.push_back({.first_index = mesh->first_index, .index_count = mesh->index_count, .diffuse = mesh->constants.diffuse[0]});
            }
        }
        return draws;
    };

    const std::vector<Draw> expected_initial = {
        {.first_index = 0, .index_count = 6, .diffuse = 1.0f},
        {.first_index = 6, .index_count = 3, .diffuse = 0.5f},
        {.first_index = 9, .index_count = 3, .diffuse = 1.0f},
    };
    const auto first_draw = record_draws();
    ASSERT_EQ(first_draw.size(), expected_initial.size());
    for (size_t i = 0; i < first_draw.size(); i++) {
        EXPECT_EQ(first_draw[i].first_index, expected_initial[i].first_index);
        EXPECT_EQ(first_draw[i].index_count, expected_initial[i].index_count);
        EXPECT_FLOAT_EQ(first_draw[i].diffuse, expected_initial[i].diffuse);
    }
    const auto second_draw = record_draws();
    ASSERT_EQ(second_draw.size(), first_draw.size());
    for (size_t i = 0; i < first_draw.size(); i++) {
        EXPECT_EQ(second_draw[i].first_index, first_draw[i].first_index);
        EXPECT_EQ(second_draw[i].index_count, first_draw[i].index_count);
        EXPECT_FLOAT_EQ(second_draw[i].diffuse, first_draw[i].diffuse);
    }

    visual.strips[1].diffuse[0] = 0.25f;
    Draw3DInvalidateStripRuns(visual);
    ASSERT_FALSE(visual.strip_runs_cached);
    ASSERT_TRUE(visual.strip_runs.empty());
    const auto changed_draw = record_draws();
    ASSERT_EQ(changed_draw.size(), 4u);
    const std::array expected_diffuse = {1.0f, 0.25f, 0.5f, 1.0f};
    for (size_t i = 0; i < changed_draw.size(); i++) {
        EXPECT_EQ(changed_draw[i].first_index, i * 3);
        EXPECT_EQ(changed_draw[i].index_count, 3u);
        EXPECT_FLOAT_EQ(changed_draw[i].diffuse, expected_diffuse[i]);
    }
    gfx::DestroyMesh(visual.mesh);
}

} // namespace dc::test
