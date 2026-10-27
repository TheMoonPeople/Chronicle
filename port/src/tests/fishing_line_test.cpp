#include <gtest/gtest.h>

#include <cmath>

#include "fishing.hpp"

namespace dc::test {

using namespace dc::game;

namespace {

bool Finite(const float *vector) {
    return std::isfinite(vector[0]) && std::isfinite(vector[1]) && std::isfinite(vector[2]);
}

} // namespace

// Holding the float in flight puts the line's float point and the float's first point on one spot,
// so the spring between them has no length; the EE's divide keeps that from becoming a NaN.
TEST(FishingLine, AFloatHeldInFlightStaysFinite) {
    sceVu0FVECTOR rod = {10.0f, 20.0f, 30.0f, 1.0f};
    sceVu0FVECTOR flight = {40.0f, 10.0f, 60.0f, 1.0f};
    FishLineInit(rod);
    cpoly = nullptr;
    WaterLevel = -100.0f;
    GroundLevel = -1000.0f;
    UkiGroundLevel = HookGroundLevel = LineGroundLevel = GroundLevel;

    for (int frame = 0; frame < 3; frame++) {
        FishLineSetUki(flight, 1.0f);
        FishLineStep(rod, nullptr);
    }
    FishLineStep(rod, nullptr);

    for (int i = 0; i < 24; i++) {
        EXPECT_TRUE(Finite(point[i])) << "line point " << i;
    }
    for (int i = 0; i < 4; i++) {
        EXPECT_TRUE(Finite(ukip[i])) << "float point " << i;
    }
    sceVu0FVECTOR uki_position;
    FishLineGetUki(uki_position);
    EXPECT_TRUE(Finite(uki_position));
}

} // namespace dc::test
