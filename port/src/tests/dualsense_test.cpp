#include <gtest/gtest.h>

#include "input/dualsense.hpp"
#include "platform/config.hpp"
#include "platform/touchpad.hpp"

namespace dc::test {

using namespace dc::input;
using namespace dc::platform;

namespace {

TouchpadFrame One(float x, float y) {
    TouchpadFrame frame;
    frame.finger[0] = {.down = true, .x = x, .y = y};
    return frame;
}

TouchpadFrame Two(float y0, float y1) {
    TouchpadFrame frame;
    frame.finger[0] = {.down = true, .x = 0.3f, .y = y0};
    frame.finger[1] = {.down = true, .x = 0.7f, .y = y1};
    return frame;
}

} // namespace

TEST(Touchpad, FirstTouchDoesNotJump) {
    TouchpadGestures pad;
    TouchpadStep     step = pad.Update(One(0.8f, 0.9f), 0);
    EXPECT_EQ(step.dx, 0.0f);
    EXPECT_EQ(step.dy, 0.0f);
}

TEST(Touchpad, OneFingerMovesThePointer) {
    TouchpadGestures pad;
    pad.Update(One(0.2f, 0.5f), 0);
    TouchpadStep step = pad.Update(One(0.3f, 0.6f), 10);
    EXPECT_NEAR(step.dx, 150.0f, 0.01f);
    EXPECT_NEAR(step.dy, 75.0f, 0.01f);
}

TEST(Touchpad, SensitivityScalesMotion) {
    TouchpadGestures pad;
    pad.SetSensitivity(2.0f);
    pad.Update(One(0.2f, 0.5f), 0);
    EXPECT_NEAR(pad.Update(One(0.3f, 0.5f), 10).dx, 300.0f, 0.01f);
}

TEST(Touchpad, LiftingAndTouchingAgainDoesNotJump) {
    TouchpadGestures pad;
    pad.Update(One(0.1f, 0.1f), 0);
    pad.Update(TouchpadFrame{}, 10);
    TouchpadStep step = pad.Update(One(0.9f, 0.9f), 20);
    EXPECT_EQ(step.dx, 0.0f);
}

TEST(Touchpad, ClickAndCreateAreMouseButtons) {
    TouchpadGestures pad;
    TouchpadFrame    frame;
    frame.click = true;
    EXPECT_EQ(pad.Update(frame, 0).held, 1u);
    frame.create = true;
    EXPECT_EQ(pad.Update(frame, 10).held, 3u);
    EXPECT_EQ(pad.Update(TouchpadFrame{}, 20).held, 0u);
}

TEST(Touchpad, TwoFingersScrollInWholeNotches) {
    TouchpadGestures pad;
    pad.Update(Two(0.4f, 0.4f), 0);
    // A tenth of the pad is 0.8 of a notch: none yet, then the remainder carries.
    EXPECT_EQ(pad.Update(Two(0.5f, 0.5f), 10).wheel, 0.0f);
    EXPECT_EQ(pad.Update(Two(0.6f, 0.6f), 20).wheel, 1.0f);
    EXPECT_EQ(pad.Update(Two(0.4f, 0.4f), 30).wheel, -1.0f);
}

TEST(Touchpad, TwoFingersDoNotMoveThePointer) {
    TouchpadGestures pad;
    pad.Update(Two(0.4f, 0.4f), 0);
    TouchpadStep step = pad.Update(Two(0.6f, 0.6f), 10);
    EXPECT_EQ(step.dx, 0.0f);
    EXPECT_EQ(step.dy, 0.0f);
}

TEST(Touchpad, QuickTwoFingerTapIsARightClick) {
    TouchpadGestures pad;
    pad.Update(Two(0.5f, 0.5f), 0);
    pad.Update(Two(0.5f, 0.5f), 50);
    EXPECT_EQ(pad.Update(TouchpadFrame{}, 100).tapped, 2u);
}

TEST(Touchpad, SlowOrSwipingTwoFingersAreNotATap) {
    TouchpadGestures slow;
    slow.Update(Two(0.5f, 0.5f), 0);
    EXPECT_EQ(slow.Update(TouchpadFrame{}, 600).tapped, 0u);

    TouchpadGestures swipe;
    swipe.Update(Two(0.2f, 0.2f), 0);
    swipe.Update(Two(0.6f, 0.6f), 40);
    EXPECT_EQ(swipe.Update(TouchpadFrame{}, 80).tapped, 0u);
}

TEST(Touchpad, OneFingerTapIsNotAClick) {
    TouchpadGestures pad;
    pad.Update(One(0.5f, 0.5f), 0);
    TouchpadStep step = pad.Update(TouchpadFrame{}, 50);
    EXPECT_EQ(step.tapped, 0u);
    EXPECT_EQ(step.held, 0u);
}

TEST(Lightbar, GreenWhenWholeRedWhenLow) {
    EXPECT_EQ(LightbarForLife(100, 100), (LightbarColour{0, 255, 0}));
    EXPECT_EQ(LightbarForLife(50, 100), (LightbarColour{255, 255, 0}));
    EXPECT_EQ(LightbarForLife(0, 100), (LightbarColour{255, 0, 0}));
}

TEST(Lightbar, ClampsAndIdlesWithoutALife) {
    EXPECT_EQ(LightbarForLife(500, 100), LightbarForLife(100, 100));
    EXPECT_EQ(LightbarForLife(-5, 100), LightbarForLife(0, 100));
    EXPECT_EQ(LightbarForLife(10, 0), LightbarIdle());
}

TEST(DualSenseConfig, DefaultsAndParsing) {
    Config config = ConfigParse("");
    EXPECT_TRUE(config.touchpad);
    EXPECT_TRUE(config.lightbar);
    EXPECT_EQ(config.touchpad_sensitivity, 1.0f);
    EXPECT_EQ(config.rumble_strength, 1.0f);

    config = ConfigParse(R"({"input":{"touchpad":false,"lightbar":false,"touchpad_sensitivity":1.5,"rumble_strength":0.5}})");
    EXPECT_FALSE(config.touchpad);
    EXPECT_FALSE(config.lightbar);
    EXPECT_EQ(config.touchpad_sensitivity, 1.5f);
    EXPECT_EQ(config.rumble_strength, 0.5f);
}

TEST(DualSenseConfig, BadValuesKeepTheDefault) {
    Config config = ConfigParse(R"({"input":{"touchpad_sensitivity":0,"rumble_strength":2}})");
    EXPECT_EQ(config.touchpad_sensitivity, 1.0f);
    EXPECT_EQ(config.rumble_strength, 1.0f);
}

} // namespace dc::test
