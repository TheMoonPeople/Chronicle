#include <gtest/gtest.h>

#include <array>

#include "bits64_fixture.hpp"
#include "bound.hpp"
#include "cloth.hpp"
#include "clothread.hpp"
#include "collisionmdt.hpp"
#include "fish.hpp"
#include "frame.hpp"
#include "framevu1.hpp"
#include "objanime.hpp"
#include "sysmes.hpp"

namespace dc::test {

using namespace dc::game;

namespace {

// Forty bytes, so two and a half quadwords: a run of one or two is too small for it.
struct Wide {
    char bytes[40];
};

} // namespace

// The placement operators size the run from the object, not from the count retail wrote by hand.
TEST(Bits64Placement, GrowsARunTheObjectOutgrows) {
    Arena arena(64);
    auto *wide = new (RetailRun(arena, 1)) Wide;

    EXPECT_EQ(arena.alloc.used, 3);
    EXPECT_GE(arena.Next(), End(wide));
    EXPECT_GE(arena.alloc.Alloc(1), End(wide));
}

TEST(Bits64Placement, GrowsARunAnArrayOutgrows) {
    Arena arena(64);
    auto *wide = new (RetailRun(arena, 1)) Wide[3];

    EXPECT_EQ(reinterpret_cast<u_char *>(wide), arena.alloc.base);
    EXPECT_EQ(arena.alloc.used, 8);
    EXPECT_GE(arena.Next(), End(wide, 3));
}

TEST(Bits64Placement, GrowsAnAlignedRun) {
    Arena arena(64);
    arena.alloc.Alloc(1);
    auto *block = reinterpret_cast<u_long128 *>(arena.alloc.Alloc64(1));
    auto *wide = new (block) Wide;

    EXPECT_GE(arena.Next(), End(wide));
}

TEST(Bits64Placement, LeavesARunThatFits) {
    Arena arena(64);
    new (RetailRun(arena, 4)) Wide;

    EXPECT_EQ(arena.alloc.used, 4);
}

// Only the arena's last run can grow: one with a later run behind it is left as retail sized it.
TEST(Bits64Placement, LeavesARunThatIsNoLongerTheLast) {
    Arena      arena(64);
    u_long128 *block = RetailRun(arena, 1);
    arena.alloc.Alloc(1);
    new (block) Wide;

    EXPECT_EQ(arena.alloc.used, 2);
}

TEST(Bits64Placement, LeavesStorageNoArenaHandedOut) {
    Arena                           arena(64);
    static std::array<u_long128, 4> storage;
    arena.alloc.Alloc(1);
    auto *wide = new (storage.data()) Wide;

    EXPECT_EQ(static_cast<void *>(wide), static_cast<void *>(storage.data()));
    EXPECT_EQ(arena.alloc.used, 1);
}

// The runs below are the ones retail's own statements ask for, with retail's quadword counts.

// InitCloth: 0x856 quadwords for the cloth, sixteen bytes short of the host's.
TEST(Bits64Placement, ClothFitsItsRun) {
    Arena arena(4096);
    auto *cloth = new (RetailRun(arena, 0x856)) CCloth(16, 16, 1.0f);

    EXPECT_GE(arena.Next(), End(cloth));
}

TEST(Bits64Placement, InitClothKeepsTheClothClearOfTheNextRun) {
    Arena     arena(4096);
    input_str input;
    InitCloth(nullptr, input, &arena.alloc);

    EXPECT_GE(arena.alloc.used * 16, static_cast<int>(sizeof(CCloth)));
}

// CommandBOUND: 0x14 quadwords for a bound, sixteen bytes short of the host's.
TEST(Bits64Placement, BoundFitsItsRun) {
    Arena arena(64);
    auto *bound = new (RetailRun(arena, 0x14)) CBound(1.0f, 1.0f, 1.0f);

    EXPECT_GE(arena.Next(), End(bound));
}

// InitFishing: 0xD8C quadwords for six fish, which hold nine fewer than the host's six.
TEST(Bits64Placement, FishFitTheirRun) {
    Arena arena(8192);
    auto *fish = new (RetailRun(arena, 0xD8C)) CFish[6];

    EXPECT_GE(arena.Next(), End(fish, 6));
}

// CreateCollisionMDT: four quadwords for the collision, whose mesh_count is in the host's fifth.
TEST(Bits64Placement, CollisionFitsItsRun) {
    Arena arena(64);
    auto *collision = new (RetailRun(arena, 4)) CCollisionMDT;

    EXPECT_GE(arena.Next(), End(collision));
}

// LoadData: 0x600 quadwords for the interior's 128 function points.
TEST(Bits64Placement, FunctionPointsFitTheirRun) {
    Arena arena(4096);
    auto *points = new (RetailRun(arena, 0x600)) EPARTS_FUNC_DATA[128];

    EXPECT_GE(arena.Next(), End(points, 128));
}

// CopyFrame, CopyFrameVu1 and the frame arrays divide sizeof by sixteen, which drops a remainder.
TEST(Bits64Placement, FramesFitTheirRuns) {
    Arena arena(256);
    auto *frame = new (RetailRun(arena, sizeof(CFrame) / 16)) CFrame;
    EXPECT_GE(arena.Next(), End(frame));

    auto *drawn = new (RetailRun(arena, sizeof(CFrameVu1) / 16)) CFrameVu1;
    EXPECT_GE(arena.Next(), End(drawn));

    auto *frames = new (RetailRun(arena, 3 * sizeof(CFrameVu1) / 16)) CFrameVu1[3];
    EXPECT_EQ(reinterpret_cast<u_char *>(frames), reinterpret_cast<u_char *>(drawn + 1));
    EXPECT_GE(arena.Next(), End(frames, 3));
}

} // namespace dc::test
