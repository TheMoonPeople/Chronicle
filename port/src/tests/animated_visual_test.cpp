#include <SDL3/SDL.h>
#include <gtest/gtest.h>

#include <memory>

#include "draw3d.hpp"
#include "gfx/displaylist.hpp"
#include "gfx_fixture.hpp"
#include "mdt.hpp"
#include "mglib.hpp"
#include "renderinfo.hpp"
#include "visual.hpp"

TEST(ChroniclePresentation, AnimatedBodyMatchesAcrossDisplayBuffersAndStationaryPoses) {
    dc::test::GfxFixture fixture(64, 48, 1.0f, dc::test::GfxOptions{.offscreen = true});
    CVisualMDTVu1        visual;
    auto                 blocks = std::make_unique<unsigned int[]>(kDraw3DBlockQuads * std::size_t{8});
    auto                 data = std::make_unique<unsigned int[]>(64);
    auto                *header = reinterpret_cast<MDT_HEADER *>(data.get());
    header->vertex_ofs = 64;
    header->mesh_ofs = 112;
    auto *positions = reinterpret_cast<float (*)[4]>(data.get() + 16);
    auto *mesh_data = data.get() + 28;
    mesh_data[2] = 1;
    mesh_data[4] = 3;
    mesh_data[5] = 3;
    mesh_data[6] = static_cast<unsigned int>(-1);
    for (unsigned int v = 0; v < 3; ++v) {
        mesh_data[7 + v * 3] = v;
    }
    visual.data = data.get();
    for (int buffer = 0; buffer < 2; ++buffer) {
        visual.vu_data_buffer[buffer] = blocks.get() + static_cast<std::size_t>(buffer) * kDraw3DBlockQuads * 4;
        auto &draw = Draw3DRegisterVisual(visual.vu_data_buffer[buffer], false);
        draw.vertices.resize(3);
        draw.indices = {0, 1, 2};
        Draw3DStrip strip;
        strip.index_count = 3;
        draw.strips.push_back(strip);
        Draw3DFinishVisual(draw);
    }
    RenderInfo    info = {};
    sceVu0FMATRIX model;
    sceVu0UnitMatrix(model);
    sceVu0UnitMatrix(info.view_scaled);
    info.scale[0] = info.scale[1] = 1.0f;
    const int           saved_buffer = DBuffID;
    gfx::DisplayListRef previous;
    gfx::MeshHandle     previous_mesh = gfx::kNullMesh;
    for (int tick = 0; tick < 3; ++tick) {
        DBuffID = tick % 2;
        positions[0][0] = tick == 0 ? 0.0f : 2.0f;
        gfx::BeginRecording();
        visual.RemakeData(nullptr);
        {
            Draw3DIdentityScope identity(&visual, kDraw3DTeleportDistance);
            visual.DrawVu1(static_cast<u_int *>(nullptr), model, &info, VU1_PROGRAM_UNKNOWN6, nullptr, 0, 0);
        }
        auto list = gfx::EndRecording();
        EXPECT_EQ(list->records.size(), 1u);
        unsigned int poses = 0;
        for (const auto &entry : list->entries) {
            if (const auto *pose = std::get_if<gfx::detail::UpdateMeshEntry>(&entry)) {
                ++poses;
                EXPECT_EQ(pose->vertices.size(), 3u);
                if (!pose->vertices.empty()) {
                    EXPECT_FLOAT_EQ(pose->vertices.front().position[0], positions[0][0]);
                }
            }
            if (const auto *draw = std::get_if<gfx::detail::MeshEntry>(&entry)) {
                if (tick != 0) {
                    EXPECT_EQ(draw->mesh, previous_mesh);
                }
                previous_mesh = draw->mesh;
            }
        }
        EXPECT_EQ(poses, 1u);
        EXPECT_TRUE(gfx::RenderList(*list, 1.0f, {.canonical = true}));
        if (previous != nullptr) {
            EXPECT_TRUE(gfx::RenderList(*list, 0.5f, {.previous = previous.get(), .present = true}));
            EXPECT_EQ(list->cache.match.size(), 1u);
            if (!list->cache.match.empty()) {
                EXPECT_EQ(list->cache.match[0], 0);
            }
        }
        previous = std::move(list);
    }
    DBuffID = saved_buffer;
    for (auto *block : visual.vu_data_buffer) {
        auto *draw = Draw3DFindVisual(block);
        gfx::DestroyMesh(draw->mesh);
        draw->mesh = gfx::kNullMesh;
        block[8] = 0;
    }
    Draw3DSweepVisuals();
}
