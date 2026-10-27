#include "app/presentation.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <format>
#include <mutex>
#include <thread>

#include "platform/clock.hpp"
#include "platform/input.hpp"
#include "platform/overlay.hpp"
#include "text/caption_text.hpp"
#include "text/textwipe.hpp"

namespace {

GamePresentSettings g_present;
gfx::DisplayListRef g_list;
gfx::DisplayListRef g_previous_list;
bool                g_tick_shown = true;
GamePresentStats    g_stats;

using PresentClock = std::chrono::steady_clock;
PresentClock::time_point g_last_present;

struct PublishedFrame {
    gfx::DisplayListRef      list;
    gfx::DisplayListRef      previous;
    gfx::DisplayListRef      overlay;
    GamePresentSettings      settings;
    PresentClock::time_point start;
    PresentClock::duration   length{};
    std::uint64_t            serial = 0;
};

std::mutex                 g_publish_mutex;
PublishedFrame             g_published;
std::atomic<std::uint64_t> g_async_frames{0};
std::atomic<double>        g_async_seconds{0};
std::atomic<double>        g_async_fps{0};
OverlayRate                g_render_rate;
PresentClock::time_point   g_render_last_present;
std::uint64_t              g_render_serial = 0;
bool                       g_render_tick_shown = false;

bool PresentAsync() {
    return gfx::ActiveRenderThreadMode() == gfx::RenderThreadMode::Async;
}

// Runs only on the worker, using immutable lists and settings published by the game thread.
PresentClock::duration PresentRenderIdle(void * /*user*/) {
    PublishedFrame frame;
    {
        std::scoped_lock lock(g_publish_mutex);
        frame = g_published;
    }
    if (!frame.list) {
        return std::chrono::milliseconds(10);
    }
    if (frame.serial != g_render_serial) {
        g_render_serial = frame.serial;
        g_render_tick_shown = false;
    }
    if (!frame.settings.interpolation && g_render_tick_shown) {
        return std::chrono::milliseconds(2);
    }
    const auto now = PresentClock::now();
    if (frame.settings.max_fps > 0) {
        const auto due = g_render_last_present + std::chrono::duration_cast<PresentClock::duration>(
                                                     std::chrono::duration<double>(1.0 / frame.settings.max_fps));
        if (now < due) {
            return due - now;
        }
    }
    if (frame.settings.interpolation && now >= frame.start + frame.length && g_render_tick_shown) {
        return std::chrono::milliseconds(1);
    }
    bool shown = false;
    if (frame.settings.interpolation) {
        const float        alpha = std::clamp(std::chrono::duration<float>(now - frame.start).count() /
                                                  std::chrono::duration<float>(frame.length).count(),
                                              0.0f, std::nextafter(1.0f, 0.0f));
        gfx::RenderOptions options;
        options.previous = frame.previous.get();
        options.present = true;
        options.overlay = frame.overlay.get();
        shown = gfx::RenderList(*frame.list, alpha, options);
        if (shown) {
            g_async_frames.fetch_add(1);
            g_async_seconds.fetch_add(std::chrono::duration<double>(PresentClock::now() - now).count());
        }
    } else {
        shown = frame.overlay ? gfx::RenderList(*frame.overlay, 1.0f, {.present = true, .host = true})
                              : gfx::PresentCanonical();
    }
    if (!shown) {
        return std::chrono::milliseconds(1);
    }
    g_render_last_present = now;
    if (g_render_rate.Count(PresentClock::now())) {
        g_async_fps.store(g_render_rate.PerSecond());
    }
    g_render_tick_shown = true;
    return PresentClock::duration::zero();
}

double SecondsSince(PresentClock::time_point start) {
    return std::chrono::duration<double>(PresentClock::now() - start).count();
}

struct FpsOverlay {
    bool                on = false;
    OverlayRate         frames;
    OverlayRate         ticks;
    std::string         text;
    gfx::LogicalMapping mapping = {};
    gfx::DisplayListRef list;
    bool                font = false;
};

FpsOverlay g_fps;

void PollFpsToggle() {
    textwipe::Update();
    if (InputHostPressed(InputHostAction::FpsToggle)) {
        g_fps.on = !g_fps.on;
    }
}

bool SameMapping(const gfx::LogicalMapping &a, const gfx::LogicalMapping &b) {
    return a.scale_x == b.scale_x && a.scale_y == b.scale_y && a.offset_x == b.offset_x &&
           a.offset_y == b.offset_y && a.pixel_width == b.pixel_width && a.pixel_height == b.pixel_height;
}

// Recorded again only when the text or the window's mapping changed. Always drawn in the overlay's
// own 5x7 font, so the badge looks the same on every screen: the game's message font is only
// loaded inside a mode, which made a second, different-looking badge for the intro and loading.
const gfx::DisplayList *FpsOverlayList() {
    if (!g_fps.on) {
        return nullptr;
    }
    std::string         text = GameFpsText();
    gfx::LogicalMapping mapping = gfx::GetLogicalMapping(gfx::kMainTarget);
    const bool          font = caption::Ready();
    if (!g_fps.list || text != g_fps.text || !SameMapping(mapping, g_fps.mapping) || font != g_fps.font) {
        g_fps.font = font;
        g_fps.list = OverlayRecord(text);
        g_fps.text = std::move(text);
        g_fps.mapping = mapping;
    }
    return g_fps.list.get();
}

void NotePresented() {
    g_fps.frames.Count(PresentClock::now());
}

bool DisplayFrame(float alpha, bool present) {
    PresentClock::time_point start = PresentClock::now();
    gfx::RenderOptions       options = {.previous = g_previous_list.get(),
                                        .present = present,
                                        .overlay = present ? FpsOverlayList() : nullptr};
    bool                     shown = gfx::RenderList(*g_list, alpha, options);
    if (shown) {
        g_stats.display_frames++;
        g_stats.display_seconds += SecondsSince(start);
        if (present) {
            NotePresented();
        }
    }
    return shown;
}

// The canonical image as it is, or a display render of the overlay alone, which starts from it.
bool PresentCanonicalFrame() {
    const gfx::DisplayList *overlay = FpsOverlayList();
    bool                    shown = overlay != nullptr ? gfx::RenderList(*overlay, 1.0f, {.present = true, .host = true})
                                                       : gfx::PresentCanonical();
    if (shown) {
        NotePresented();
    }
    return shown;
}

} // namespace

void GameSetPresentSettings(const GamePresentSettings &settings) {
    g_present = settings;
    g_fps.on = settings.show_fps;
    if (PresentAsync()) {
        gfx::SetRenderIdleHook(PresentRenderIdle, nullptr);
    }
}

void GameRenderTick(gfx::DisplayListRef list) {
    PresentClock::time_point start = PresentClock::now();
    gfx::RenderList(*list, 1.0f, {.canonical = true});
    g_stats.canonical_seconds += SecondsSince(start);
    g_stats.ticks++;
    gfx::DisplayListStats counts = gfx::ListStats(*list);
    g_stats.mesh_draws += counts.mesh_draws;
    g_stats.keyed_mesh_draws += counts.keyed_mesh_draws;
    g_stats.draws_2d += counts.draws_2d;
    g_stats.stateful += counts.stateful;
    g_stats.max_draws = std::max<std::uint64_t>(g_stats.max_draws, counts.mesh_draws + counts.draws_2d);
    g_stats.last_draws = counts.mesh_draws + counts.draws_2d;
    g_fps.ticks.Count(PresentClock::now());
    g_previous_list = std::move(g_list);
    g_list = std::move(list);
    g_tick_shown = false;
    if (PresentAsync()) {
        FpsOverlayList();
        const auto length = std::chrono::duration_cast<PresentClock::duration>(
            std::chrono::duration<double>(1.0 / ClockTickRate()));
        const auto elapsed = std::chrono::duration_cast<PresentClock::duration>(
            std::chrono::duration<double>(ClockTickFraction() / ClockTickRate()));
        {
            std::scoped_lock lock(g_publish_mutex);
            g_published.list = g_list;
            g_published.previous = g_previous_list;
            g_published.overlay = g_fps.on ? g_fps.list : nullptr;
            g_published.settings = g_present;
            g_published.start = PresentClock::now() - elapsed;
            g_published.length = length;
            ++g_published.serial;
        }
        gfx::WakeRenderThread();
    }
}

bool GamePresentBetweenTicks(double fraction, std::chrono::steady_clock::time_point next_tick) {
    if (PresentAsync()) {
        PollFpsToggle();
        return false;
    }
    if (!g_list) {
        return false;
    }
    PollFpsToggle();
    if (!g_present.interpolation) {
        if (!g_tick_shown) {
            PresentCanonicalFrame();
            g_tick_shown = true;
        }
        return false;
    }
    if (g_present.max_fps > 0.0) {
        auto due = g_last_present + std::chrono::duration_cast<PresentClock::duration>(
                                        std::chrono::duration<double>(1.0 / g_present.max_fps));
        if (PresentClock::now() < due) {
            if (due >= next_tick && g_tick_shown) {
                return false;
            }
            std::this_thread::sleep_until(std::min(due, next_tick));
            return true;
        }
    }
    // The loading screen presenting from the pump, or a window that cannot show anything, ends the
    // tick's display frames.
    if (!DisplayFrame(static_cast<float>(fraction), true)) {
        return false;
    }
    g_last_present = PresentClock::now();
    g_tick_shown = true;
    return true;
}

void GamePresentTickEnd() {
    if (PresentAsync()) {
        return;
    }
    if (g_tick_shown || !g_list) {
        return;
    }
    PollFpsToggle();
    if (ClockUnbounded()) {
        for (int frame = 0; frame < g_present.display_per_tick; frame++) {
            DisplayFrame(static_cast<float>(frame) / static_cast<float>(g_present.display_per_tick), false);
        }
    }
    PresentCanonicalFrame();
    g_last_present = PresentClock::now();
    g_tick_shown = true;
}

GamePresentStats GamePresentStatistics() {
    auto stats = g_stats;
    stats.display_frames += g_async_frames.load();
    stats.display_seconds += g_async_seconds.load();
    return stats;
}

void GameStopPresentation() {
    gfx::SetRenderIdleHook(nullptr, nullptr);
    gfx::RenderLease lease;
    std::scoped_lock lock(g_publish_mutex);
    g_published = {};
}

bool GameShowingFps() {
    return g_fps.on;
}

std::string GameFpsText() {
    std::string text = std::format("FPS {:.1f}", PresentAsync() ? g_async_fps.load() : g_fps.frames.PerSecond());
    if (g_present.fps_detail != ConfigFpsDetail::Fps) {
        text += std::format("  TICK {:.1f}/{:g}", g_fps.ticks.PerSecond(), ClockTickRate());
    }
    if (g_present.fps_detail == ConfigFpsDetail::All) {
        text += std::format("  DRAWS {}", g_stats.last_draws);
    }
    return text;
}

// kPreviousFrame outside a frame is the newest main image: the canonical render, or the loading
// screen's frame after it. ReadbackFrame would read a display frame, overlay and all.
bool GameScreenshot(std::vector<std::uint8_t> &rgba, std::uint32_t &width, std::uint32_t &height) {
    return gfx::ReadbackTexture(gfx::kPreviousFrame, rgba, width, height);
}

bool GameScreenshotWithFps(std::vector<std::uint8_t> &rgba, std::uint32_t &width, std::uint32_t &height) {
    const gfx::DisplayList *overlay = FpsOverlayList();
    return overlay != nullptr && gfx::RenderList(*overlay, 1.0f, {.present = false, .host = true}) &&
           gfx::ReadbackFrame(rgba, width, height);
}
