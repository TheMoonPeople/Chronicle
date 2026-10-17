#include <gtest/gtest.h>
#include <algorithm>
#include "feather_toggle.hpp"
#include "options/rows.hpp"
#include "platform/config.hpp"

namespace {
FeatherToggleContext Ready() { return {true, true, 0, 1, true, 1, 900}; }
}

TEST(FeatherToggle, PressReleaseAndSecondPressToggleWithoutRepeatingWhileHeld) {
    FeatherToggleState state;
    state.Sync(Ready());
    EXPECT_FALSE(state.Use(false, false, true, true));
    EXPECT_TRUE(state.Use(true, true, true, true));
    EXPECT_TRUE(state.On(1));
    EXPECT_FALSE(state.On(2));
    for (int frame = 0; frame < 60; ++frame) EXPECT_TRUE(state.Use(true, false, true, true));
    EXPECT_TRUE(state.Use(false, false, true, true));
    EXPECT_FALSE(state.Use(true, true, true, true));
    EXPECT_FALSE(state.On(1));
    EXPECT_FALSE(state.Use(true, false, true, true));
    EXPECT_FALSE(state.Use(false, false, true, true));
}

TEST(FeatherToggle, OptOutAndOtherItemsRetainHeldBehavior) {
    for (bool feather : {false, true}) {
        auto context = Ready();
        context.enabled = false;
        context.feather = feather;
        FeatherToggleState state;
        state.Sync(context);
        EXPECT_TRUE(state.Use(true, true, true, true));
        EXPECT_FALSE(state.Use(false, false, true, true));
        EXPECT_FALSE(state.On(1));
        context.enabled = true;
        context.feather = false;
        state.Sync(context);
        EXPECT_TRUE(state.Use(true, true, true, true));
        EXPECT_FALSE(state.Use(false, false, true, true));
        EXPECT_FALSE(state.On(1));
    }
}

TEST(FeatherToggle, MenuSlotCharacterRemovalDepletionAndDisablingClearLatch) {
    const auto ready = Ready();
    for (int reason = 0; reason < 8; ++reason) {
        FeatherToggleState state;
        state.Sync(ready);
        ASSERT_TRUE(state.Use(true, true, true, true));
        auto changed = ready;
        switch (reason) {
            case 0: changed.gameplay = false; break;
            case 1: changed.slot = 2; break;
            case 2: changed.character = 1; break;
            case 3: changed.feather = false; break;
            case 4: changed.quantity = 0; break;
            case 5: changed.duration = 0; break;
            case 6: changed.enabled = false; break;
            case 7: changed.slot = 0; break;
        }
        state.Sync(changed);
        EXPECT_FALSE(state.On(1)) << reason;
        state.Sync(ready);
        EXPECT_FALSE(state.Use(false, false, true, true)) << reason;
    }
}

TEST(FeatherToggle, AttacksFreezeAndLockOnSuspendUseWithoutSpendingOrRetoggling) {
    FeatherToggleState state;
    state.Sync(Ready());
    EXPECT_FALSE(state.Use(true, true, false, true));
    EXPECT_FALSE(state.On(1));
    EXPECT_TRUE(state.Use(true, true, true, true));
    EXPECT_FALSE(state.Use(false, false, false, true));
    EXPECT_FALSE(state.Use(true, true, false, true));
    EXPECT_TRUE(state.On(1));
    EXPECT_TRUE(state.Use(false, false, true, true));
}

TEST(FeatherToggle, ActiveUsePreservesDurationOwnershipAndStopsAtExhaustion) {
    FeatherToggleState state;
    auto context = Ready();
    context.duration = 2;
    state.Sync(context);
    ASSERT_TRUE(state.Use(true, true, true, true));
    // The existing game loop owns decrement and deletion; the latch only supplies the held input.
    --context.duration;
    state.Sync(context);
    ASSERT_TRUE(state.Use(false, false, true, true));
    --context.duration;
    state.Sync(context);
    EXPECT_FALSE(state.Use(false, false, true, true));
    EXPECT_FALSE(state.On(1));
    context.duration = 900;
    state.Sync(context);
    EXPECT_FALSE(state.Use(false, false, true, true));
}

TEST(FeatherToggleConfig, QolTogglePersistsAndResetKeepsOtherSettingsIndependent) {
    Config config;
    EXPECT_FALSE(config.feather_toggle);
    auto pages = options::Pages();
    auto page = std::find_if(pages.begin(), pages.end(),
        [](const options::Page &p) { return std::string_view(p.name) == "QOL"; });
    ASSERT_NE(page, pages.end());
    auto row = std::find_if(page->rows.begin(), page->rows.end(),
        [](const options::Row &r) { return std::string_view(r.key) == "qol.feather_toggle"; });
    ASSERT_NE(row, page->rows.end());
    EXPECT_EQ(options::RowValue(*row, config), "Off");
    ASSERT_TRUE(options::StepRow(*row, config, 1, false));
    EXPECT_TRUE(config.feather_toggle);
    EXPECT_FALSE(config.inventory_auto_repair);
    EXPECT_TRUE(ConfigParse(ConfigSerialize(config)).feather_toggle);
    EXPECT_FALSE(ConfigParse(R"({"qol":{"feather_toggle":"bad"}})").feather_toggle);
    options::ResetPage(*page, config);
    EXPECT_FALSE(config.feather_toggle);
}

#include "gfx_fixture.hpp"

TEST(FeatherToggleVisual, OnBadgeRendersInsideItsQuickSlot) {
    dc::test::GfxFixture fixture(640, 480, 1.0f, dc::test::GfxOptions{.offscreen = true});
    fixture.Frame({0, 0, 0, 128}, [] { DrawFeatherToggleIndicator(308, 22); });
    int gold = 0;
    int white = 0;
    for (unsigned y = 0; y < fixture.height; ++y) {
        for (unsigned x = 0; x < fixture.width; ++x) {
            auto pixel = fixture.Pixel(x, y);
            if (pixel[0] > 100 && pixel[1] > 80 && pixel[2] < 20) ++gold;
            if (pixel[0] > 100 && pixel[1] > 100 && pixel[2] > 80) ++white;
            if (pixel[0] || pixel[1] || pixel[2]) {
                EXPECT_GE(x, 308u);
                EXPECT_LT(x, 330u);
                EXPECT_GE(y, 22u);
                EXPECT_LT(y, 36u);
            }
        }
    }
    EXPECT_GT(gold, 50);
    EXPECT_GT(white, 50);
    if (const char *output = std::getenv("DC_FEATHER_BADGE_CAPTURE")) {
        EXPECT_TRUE(gfx::WritePng(output, fixture.pixels.data(), fixture.width, fixture.height));
    }
}

TEST(FeatherDuration, FixedScaleClampsAndKeepsPositiveTimeVisible) {
    EXPECT_EQ(FeatherDurationFill(-1), 0);
    EXPECT_EQ(FeatherDurationFill(0), 0);
    EXPECT_EQ(FeatherDurationFill(1), 1);
    EXPECT_EQ(FeatherDurationFill(180 * 60), 17);
    EXPECT_EQ(FeatherDurationFill(360 * 60), 34);
    EXPECT_EQ(FeatherDurationFill(999999), 34);
}

TEST(FeatherDurationVisual, BarFitsBelowIconAndKeepsSelectorColour) {
    dc::test::GfxFixture fixture(1280, 960, 1.0f, dc::test::GfxOptions{.offscreen = true});
    for (int ticks : {360 * 60, 180 * 60, 60 * 60, 20 * 60, 0}) {
        fixture.Frame({0, 0, 0, 128}, [ticks] { DrawFeatherDurationBar(294, 59, ticks); });
        int coloured = 0;
        for (unsigned y = 0; y < fixture.height; ++y) {
            for (unsigned x = 0; x < fixture.width; ++x) {
                auto p = fixture.Pixel(x, y);
                if (p[0] || p[1] || p[2]) {
                    EXPECT_GE(x, 588u); EXPECT_LT(x, 660u);
                    EXPECT_GE(y, 118u); EXPECT_LT(y, 130u);
                }
                if (p[0] > 100 && p[1] > 80 && p[2] < 20) ++coloured;
            }
        }
        EXPECT_EQ(coloured, FeatherDurationFill(ticks) * 4 * 4);
        if (const char *dir = std::getenv("DC_FEATHER_DURATION_CAPTURE")) {
            EXPECT_TRUE(gfx::WritePng(std::filesystem::path(dir) / ("duration-" + std::to_string(ticks / 60) + ".png"),
                fixture.pixels.data(), fixture.width, fixture.height));
        }
    }
}

TEST(FeatherToggle, IdleKeepsLatchWithoutUseAndMovementResumesAutomatically) {
    FeatherToggleState state;
    state.Sync(Ready());
    EXPECT_FALSE(state.Use(true, true, true, false));
    EXPECT_TRUE(state.On(1));
    for (int frame = 0; frame < 120; ++frame) {
        EXPECT_FALSE(state.Use(false, false, true, false));
        EXPECT_TRUE(state.On(1));
    }
    EXPECT_TRUE(state.Use(false, false, true, true));
    EXPECT_FALSE(state.Use(false, false, true, false));
    EXPECT_TRUE(state.On(1));
    EXPECT_FALSE(state.Use(true, true, true, false));
    EXPECT_FALSE(state.On(1));
    EXPECT_FALSE(state.Use(false, false, true, true));
}

TEST(FeatherToggle, IdleDoesNotChangeRetailHeldUseWhenDisabledOrOtherItem) {
    for (bool enabled : {false, true}) {
        auto context = Ready();
        context.enabled = enabled;
        context.feather = !enabled;
        FeatherToggleState state;
        state.Sync(context);
        EXPECT_TRUE(state.Use(true, true, true, false));
        EXPECT_FALSE(state.Use(false, false, true, false));
    }
}

TEST(QolDefaults, NewAndExistingConfigsKeepEveryModDisabledUnlessOptedIn) {
    for (const char *json : {"{}", R"({"game":{"language":"english"}})", R"({"qol":{}})"}) {
        const Config config = ConfigParse(json);
        EXPECT_FALSE(config.feather_toggle);
        EXPECT_FALSE(config.inventory_auto_repair);
    }
    Config config;
    config.feather_toggle = true;
    config.inventory_auto_repair = true;
    const auto pages = options::Pages();
    const auto page = std::find_if(pages.begin(), pages.end(),
        [](const options::Page &p) { return std::string_view(p.name) == "QOL"; });
    ASSERT_NE(page, pages.end());
    options::ResetPage(*page, config);
    EXPECT_FALSE(config.feather_toggle);
    EXPECT_FALSE(config.inventory_auto_repair);
    config = ConfigParse(ConfigSerialize(config));
    EXPECT_FALSE(config.feather_toggle);
    EXPECT_FALSE(config.inventory_auto_repair);
}
