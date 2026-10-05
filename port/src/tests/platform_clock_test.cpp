#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <thread>
#include <vector>

#include "../platform/clock.hpp"

namespace {

int g_ticks;
int g_idles;
int g_pumps_inside;

int CountTick(int) {
    ++g_ticks;
    return 0;
}

void CountIdle() {
    ++g_idles;
}

int PumpFromTick(int) {
    ++g_ticks;
    ClockPump();
    ++g_pumps_inside;
    return 0;
}

void Start(double hertz, bool unbounded) {
    ClockSetTickRate(hertz);
    ClockSetUnbounded(unbounded);
    ClockSetTickCallback(CountTick);
    ClockSetIdleHook(CountIdle);
    ClockReset();
    g_ticks = 0;
    g_idles = 0;
}

} // namespace

TEST(PlatformClock, DefaultsToNtscRate) {
    ASSERT_TRUE(ClockTickRate() == 60.0);
    ASSERT_TRUE(!ClockUnbounded());
    ClockSetTickRate(-1.0);
    ASSERT_TRUE(ClockTickRate() == 60.0);
}

TEST(PlatformClock, UnboundedAdvancesOneTickPerPump) {
    Start(50.0, true);
    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < 100; ++i) {
        ClockWaitNextTick();
        ASSERT_TRUE(ClockPump() == i + 1);
    }
    ASSERT_TRUE(std::chrono::steady_clock::now() - start < std::chrono::milliseconds(500));
    ASSERT_TRUE(ClockTickCount() == 100);
    ASSERT_TRUE(g_ticks == 100);
    ASSERT_TRUE(g_idles == 100);
}

TEST(PlatformClock, PumpRunsCallbackPerElapsedTick) {
    Start(1000.0, false);
    ASSERT_TRUE(ClockPump() == 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    std::int64_t count = ClockPump();
    ASSERT_TRUE(count >= 30);
    ASSERT_TRUE(count < 300);
    ASSERT_TRUE(g_ticks == count);
    ASSERT_TRUE(g_idles == 2);
    ASSERT_TRUE(ClockPump() >= count);
}

TEST(PlatformClock, PumpIsNotReentrant) {
    Start(50.0, true);
    ClockSetTickCallback(PumpFromTick);
    g_pumps_inside = 0;
    ASSERT_TRUE(ClockPump() == 1);
    ASSERT_TRUE(g_ticks == 1);
    ASSERT_TRUE(g_pumps_inside == 1);
    ASSERT_TRUE(ClockGetTickCallback() == PumpFromTick);
}

TEST(PlatformClock, RateChangeKeepsCount) {
    Start(1000.0, false);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    std::int64_t before = ClockPump();
    ClockSetTickRate(10.0);
    ASSERT_TRUE(ClockTickCount() == before);
    ASSERT_TRUE(ClockPump() == before);
    // CI runners oversleep by more than these bounds allow.
    if (std::getenv("CI") != nullptr) {
        GTEST_SKIP() << "timing is not checked on CI";
    }
    auto start = std::chrono::steady_clock::now();
    ASSERT_TRUE(ClockSyncV() == before + 1);
    auto waited = std::chrono::steady_clock::now() - start;
    ASSERT_TRUE(waited >= std::chrono::milliseconds(99));
    ASSERT_TRUE(waited < std::chrono::milliseconds(150));
}
