#include "clock.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <thread>
#include <vector>

namespace {

using SteadyClock = std::chrono::steady_clock;

// sleep_until wakes late by the scheduler's slack and wake-up latency, so the
// sleep ends this far ahead of the deadline and the rest is spun.
constexpr auto kSpinMargin = std::chrono::microseconds(1500);

struct ClockState {
    double                  hertz = 60.0;
    bool                    unbounded = false;
    bool                    anchored = false;
    SteadyClock::time_point anchor = {};
    std::int64_t            anchor_count = 0;
    std::int64_t            count = 0;
    ClockTickCallback       callback = nullptr;
    ClockIdleHook           idle = nullptr;
    bool                    pumping = false;
};

std::vector<ClockIdleHook> g_pump_hooks;

ClockState g_clock;

void Anchor(SteadyClock::time_point now) {
    g_clock.anchor = now;
    g_clock.anchor_count = g_clock.count;
    g_clock.anchored = true;
}

void EnsureAnchored() {
    if (!g_clock.anchored) {
        Anchor(SteadyClock::now());
    }
}

SteadyClock::time_point Deadline(std::int64_t tick) {
    std::chrono::duration<double> offset(static_cast<double>(tick - g_clock.anchor_count) / g_clock.hertz);
    return g_clock.anchor + std::chrono::duration_cast<SteadyClock::duration>(offset);
}

// The last tick whose boundary is not after now, settled against Deadline so
// a wait that ends exactly on a boundary always counts that tick.
std::int64_t TickAt(SteadyClock::time_point now) {
    std::chrono::duration<double> elapsed = now - g_clock.anchor;
    std::int64_t                  tick = g_clock.anchor_count + static_cast<std::int64_t>(std::floor(elapsed.count() * g_clock.hertz));
    while (Deadline(tick + 1) <= now) {
        ++tick;
    }
    while (tick > g_clock.anchor_count && Deadline(tick) > now) {
        --tick;
    }
    return tick;
}

void SleepUntil(SteadyClock::time_point deadline) {
    if (deadline - SteadyClock::now() > kSpinMargin) {
        std::this_thread::sleep_until(deadline - kSpinMargin);
    }
    while (SteadyClock::now() < deadline) {
        std::this_thread::yield();
    }
}

} // namespace

void ClockSetTickRate(double hertz) {
    if (!(hertz > 0.0) || !std::isfinite(hertz)) {
        std::fprintf(stderr, "clock: ignoring tick rate %g\n", hertz);
        return;
    }
    g_clock.hertz = hertz;
    Anchor(SteadyClock::now());
}

double ClockTickRate() {
    return g_clock.hertz;
}

void ClockSetUnbounded(bool unbounded) {
    g_clock.unbounded = unbounded;
    Anchor(SteadyClock::now());
}

bool ClockUnbounded() {
    return g_clock.unbounded;
}

void ClockReset() {
    g_clock.count = 0;
    Anchor(SteadyClock::now());
}

std::int64_t ClockTickCount() {
    return g_clock.count;
}

void ClockSetTickCallback(ClockTickCallback callback) {
    g_clock.callback = callback;
}

ClockTickCallback ClockGetTickCallback() {
    return g_clock.callback;
}

void ClockSetIdleHook(ClockIdleHook hook) {
    g_clock.idle = hook;
}

ClockIdleHook ClockGetIdleHook() {
    return g_clock.idle;
}

void ClockAddPumpHook(ClockIdleHook hook) {
    if (hook != nullptr && std::ranges::find(g_pump_hooks, hook) == g_pump_hooks.end()) {
        g_pump_hooks.push_back(hook);
    }
}

void ClockRemovePumpHook(ClockIdleHook hook) {
    std::erase(g_pump_hooks, hook);
}

std::int64_t ClockPump() {
    if (g_clock.pumping) {
        return g_clock.count;
    }

    struct Guard {
        Guard() { g_clock.pumping = true; }

        ~Guard() { g_clock.pumping = false; }
    } guard;

    EnsureAnchored();
    std::int64_t target = g_clock.unbounded ? g_clock.count + 1 : TickAt(SteadyClock::now());
    while (g_clock.count < target) {
        ++g_clock.count;
        if (g_clock.callback != nullptr) {
            g_clock.callback(0);
        }
    }
    // Indexed: a hook may remove itself.
    for (std::size_t i = 0; i < g_pump_hooks.size(); ++i) {
        g_pump_hooks[i]();
    }
    if (g_clock.idle != nullptr) {
        g_clock.idle();
    }
    return g_clock.count;
}

void ClockWaitNextTick() {
    if (g_clock.unbounded) {
        return;
    }
    EnsureAnchored();
    SleepUntil(Deadline(g_clock.count + 1));
}

double ClockTickFraction() {
    if (g_clock.unbounded) {
        return 1.0;
    }
    EnsureAnchored();
    std::chrono::duration<double> elapsed = SteadyClock::now() - Deadline(g_clock.count);
    return std::clamp(elapsed.count() * g_clock.hertz, 0.0, std::nextafter(1.0, 0.0));
}

void ClockWaitNextTick(ClockWaitHook hook) {
    if (g_clock.unbounded) {
        return;
    }
    EnsureAnchored();
    SteadyClock::time_point deadline = Deadline(g_clock.count + 1);
    while (hook != nullptr && SteadyClock::now() < deadline && hook(ClockTickFraction(), deadline)) {
    }
    SleepUntil(deadline);
}

std::int64_t ClockSyncV() {
    ClockWaitNextTick();
    return ClockPump();
}
