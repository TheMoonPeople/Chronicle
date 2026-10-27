#include "common.h"

#include <gtest/gtest.h>
#include <libvu0.h>

#include <array>
#include <cstddef>
#include <cstring>

#include "edit_in.hpp"
#include "editpartsinfo.hpp"
#include "objanime.hpp"

namespace dc::test {

using namespace dc::game;

TEST(EditInterior, RejectsFunctionTablePastFixedCapacity) {
    alignas(16) std::array<std::byte, 0x100> archive{};
    auto                                    *words = reinterpret_cast<u_int *>(archive.data());
    EPartsDiscHeader                         header{};
    header.func_count = 129;
    words[1] = 2;
    std::memcpy(&words[2], &header, sizeof(header));
    std::array<EPARTS_FUNC_DATA, 128> points{};

    EXPECT_EQ(GetFuncPoint(0, words, points.data()), 0);
}

} // namespace dc::test
