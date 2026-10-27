#include "common.h"

#include <gtest/gtest.h>
#include <libvu0.h>

#include "edit.hpp"
#include "editpartsinfo.hpp"

namespace dc::test {

using namespace dc::game;

TEST(EditPartsInfo, RejectsCellGridLargerThanRetailRecord) {
    INIT_PARTSINFO     init{};
    EPARTS_INFO_HEADER header{};
    init.width = 33;
    init.height = 2;

    EXPECT_EQ(EdInitToEPInfo(&init, &header), 0);
    EXPECT_EQ(header.header_size, 0);
}

TEST(EditPartsInfo, RejectsMissingSourceOrDestination) {
    INIT_PARTSINFO     init{};
    EPARTS_INFO_HEADER header{};

    EXPECT_EQ(EdInitToEPInfo(nullptr, &header), 0);
    EXPECT_EQ(EdInitToEPInfo(&init, nullptr), 0);
}

} // namespace dc::test
