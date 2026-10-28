#include <gtest/gtest.h>

#include <array>

#include "bits64_fixture.hpp"
#include "mds.hpp"
#include "mdt.hpp"
#include "visual.hpp"

namespace dc::test {

using namespace dc::game;

// CreateVisual: three quadwords for a visual that keeps its model, two for one that does not. Its VU
// block starts at the next 64-byte line, so the visual is placed at each quadword of a line.
TEST(Bits64Placement, CreateVisualKeepsTheVisualClearOfItsBlock) {
    for (const int attr : {0, 2}) {
        for (int lead = 0; lead < 4; lead++) {
            Arena arena(256);
            arena.alloc.Alloc(lead);
            alignas(16) std::array<u_int, 32> data{};
            auto                             *header = reinterpret_cast<MDT_HEADER *>(data.data());
            header->size = sizeof(data);
            header->vertex_ofs = 64;
            header->mesh_ofs = 64;

            CVisualMDTVu1 *visual = CreateVisual(data.data(), &arena.alloc, attr);

            ASSERT_NE(visual, nullptr);
            const std::size_t size = attr == 0 ? sizeof(CVisualVu1) : sizeof(CVisualMDTVu1);
            EXPECT_LE(reinterpret_cast<const u_char *>(visual) + size, reinterpret_cast<const u_char *>(visual->vu_data)) << "attr " << attr << ", lead " << lead;
        }
    }
}

} // namespace dc::test
