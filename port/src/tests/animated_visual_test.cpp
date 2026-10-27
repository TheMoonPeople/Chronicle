#include <SDL3/SDL.h>
#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "gfx/displaylist.hpp"
#include "gfx_fixture.hpp"
#include "mdt.hpp"
#include "mglib.hpp"
#include "render/draw3d.hpp"
#include "renderinfo.hpp"
#include "visual.hpp"

namespace dc::test {

using namespace dc::game;
using namespace dc::render;

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
        draw.cache_strip_runs_on_finish = true;
        Draw3DFinishVisual(draw);
        ASSERT_TRUE(draw.strip_runs_cached);
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
        ASSERT_FALSE(Draw3DFindVisual(visual.vu_data_buffer[0])->strip_runs_cached);
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

TEST(ChroniclePresentation, MdtRemakeTracksMaterialChangesAndAlwaysUpdatesVertices) {
    dc::test::GfxFixture fixture(64, 48, 1.0f, dc::test::GfxOptions{.offscreen = true});
    CVisualVu1           visual;
    auto                 block = std::make_unique<unsigned int[]>(static_cast<std::size_t>(kDraw3DBlockQuads) * 4);
    auto                 data = std::make_unique<unsigned int[]>(512);
    auto                *header = reinterpret_cast<MDT_HEADER *>(data.get());
    header->vertex_num = 15;
    header->vertex_ofs = 64;
    header->normal[1] = 304;
    header->mesh_ofs = 544;
    header->info_ofs = 816;
    auto *positions = reinterpret_cast<float (*)[4]>(data.get() + header->vertex_ofs / 4);
    auto *mesh = data.get() + header->mesh_ofs / 4;
    mesh[2] = 5;
    for (unsigned strip = 0; strip < 5; ++strip) {
        const unsigned word = 4 + strip * 12;
        mesh[word] = 4;
        mesh[word + 1] = 3;
        int material = -1;
        if (strip == 0 || strip == 4) {
            material = 0;
        } else if (strip == 2) {
            material = 1;
        }
        mesh[word + 2] = material;
        for (unsigned corner = 0; corner < 3; ++corner) {
            mesh[word + 3 + corner * 3] = strip * 3 + corner;
        }
    }
    auto *materials = reinterpret_cast<MDT_MATERIAL *>(data.get() + header->info_ofs / 4);
    materials[0].diffuse[0] = 1.0f;
    materials[0].diffuse[3] = 1.0f;
    materials[1].diffuse[1] = 1.0f;
    materials[1].diffuse[3] = 1.0f;
    for (unsigned vertex = 0; vertex < 15; ++vertex) {
        positions[vertex][0] = static_cast<float>(vertex);
        positions[vertex][3] = 1.0f;
    }

    auto *const block_address = block.get();
    ASSERT_NE(visual.CreateVUdataFromMDT(block_address, data.get(), 0, 0), 0);
    Draw3DVisual *draw_visual = Draw3DFindVisual(block_address);
    ASSERT_NE(draw_visual, nullptr);
    ASSERT_TRUE(draw_visual->strip_runs_cached);

    RenderInfo info = {};
    float      model[4][4] = {};
    for (int axis = 0; axis < 4; ++axis) {
        model[axis][axis] = 1.0f;
        info.view_scaled[axis][axis] = 1.0f;
    }
    info.scale[0] = info.scale[1] = 1.0f;

    struct Draw {
        std::uint32_t first_index;
        std::uint32_t index_count;
        float         diffuse_red;
        float         diffuse_green;
    };

    auto remake_and_record = [&] {
        gfx::BeginRecording();
        visual.CreateVUdataFromMDTRemake(block_address, data.get(), 1);
        Draw3DDrawVisual(*draw_visual, model, info, 0);
        auto              list = gfx::EndRecording();
        std::vector<Draw> draws;
        unsigned          updates = 0;
        for (const auto &entry : list->entries) {
            if (const auto *update = std::get_if<gfx::detail::UpdateMeshEntry>(&entry)) {
                ++updates;
                EXPECT_EQ(update->vertices.size(), 15u);
                EXPECT_FLOAT_EQ(update->vertices[0].position[0], positions[0][0]);
            }
            if (const auto *mesh_entry = std::get_if<gfx::detail::MeshEntry>(&entry)) {
                draws.push_back({mesh_entry->first_index, mesh_entry->index_count,
                                 mesh_entry->constants.diffuse[0], mesh_entry->constants.diffuse[1]});
            }
        }
        EXPECT_EQ(updates, 1u);
        return draws;
    };
    auto expect_draws = [](const std::vector<Draw> &actual, std::initializer_list<Draw> expected) {
        ASSERT_EQ(actual.size(), expected.size());
        std::size_t index = 0;
        for (const Draw &draw : expected) {
            EXPECT_EQ(actual[index].first_index, draw.first_index);
            EXPECT_EQ(actual[index].index_count, draw.index_count);
            EXPECT_FLOAT_EQ(actual[index].diffuse_red, draw.diffuse_red);
            EXPECT_FLOAT_EQ(actual[index].diffuse_green, draw.diffuse_green);
            ++index;
        }
    };

    expect_draws(remake_and_record(), {
                                          {0,  6, 1.0f, 0.0f},
                                          {6,  6, 0.0f, 1.0f},
                                          {12, 3, 1.0f, 0.0f}
    });

    materials[1].diffuse[0] = 0.25f;
    materials[1].diffuse[1] = 0.5f;
    positions[0][0] = 20.0f;
    expect_draws(remake_and_record(), {
                                          {0,  6, 1.0f,  0.0f},
                                          {6,  6, 0.25f, 0.5f},
                                          {12, 3, 1.0f,  0.0f}
    });

    mesh[4 + 2 * 12 + 2] = 0;
    positions[0][0] = 21.0f;
    expect_draws(remake_and_record(), {
                                          {0, 15, 1.0f, 0.0f}
    });

    mesh[4 + 2 * 12 + 2] = 1;
    positions[0][0] = 22.0f;
    expect_draws(remake_and_record(), {
                                          {0,  6, 1.0f,  0.0f},
                                          {6,  6, 0.25f, 0.5f},
                                          {12, 3, 1.0f,  0.0f}
    });

    expect_draws(remake_and_record(), {
                                          {0,  6, 1.0f,  0.0f},
                                          {6,  6, 0.25f, 0.5f},
                                          {12, 3, 1.0f,  0.0f}
    });

    gfx::DestroyMesh(draw_visual->mesh);
    draw_visual->mesh = gfx::kNullMesh;
    block[8] = 0;
    Draw3DSweepVisuals();
}

} // namespace dc::test
