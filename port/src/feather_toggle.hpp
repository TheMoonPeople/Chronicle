#pragma once

// Session-only latch. The save keeps feather duration, never the toggle's on/off state.
struct FeatherToggleContext {
    bool enabled = false;
    bool gameplay = false;
    int character = -1;
    int slot = 0; // retail quick slots are 1-based
    bool feather = false;
    int quantity = 0;
    int duration = 0;
};

class FeatherToggleState {
public:
    void Sync(const FeatherToggleContext &context);
    bool Use(bool held, bool pressed, bool allowed, bool moving);
    bool On(int slot) const;
private:
    FeatherToggleContext context_;
    bool on_ = false;
};

void FeatherToggleSync();
bool FeatherUseRequested(bool held, bool pressed, bool allowed, bool moving);
bool FeatherToggleOn(int slot);
void DrawFeatherToggleIndicator(int x, int y);

// Fixed six-minute scale: retail saves remaining ticks, not the randomly assigned starting lifetime.
constexpr int kFeatherDurationCapacity = 360 * 60;
int FeatherDurationFill(int ticks);
int FeatherQuickSlotDuration(int slot);
void DrawFeatherDurationBar(int x, int y, int ticks);
