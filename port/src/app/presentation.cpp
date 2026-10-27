#include "app/presentation.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <format>
#include <mutex>
#include <thread>

#include "main.hpp"
#include "platform/clock.hpp"
#include "platform/input.hpp"
#include "platform/overlay.hpp"
#include "text/caption_text.hpp"
#include "text/textwipe.hpp"

#if defined(__SWITCH__)
#include "switch_main.hpp"
#endif

namespace dc::app {
namespace {

GamePresentSettings g_present;
gfx::DisplayListRef g_list;
gfx::DisplayListRef g_previous_list;
bool                g_tick_shown = true;
GamePresentStats    g_stats;

using PresentClock = std::chrono::steady_clock;
using Span = gfx::FrameTimings::Span;

// The game thread's share of each tick, reported with the renderer's timings once a second.
struct PerfLog {
    bool                     on = false;
    PresentClock::time_point window_start;
    PresentClock::time_point tick_end;
    PresentClock::time_point render_end;
    Span                     interval; // tick end to tick end
    Span                     work;     // tick end to the canonical render: the mode's logic and recording
    Span                     render;   // the canonical render, waiting for the render thread included
    Span                     wait;     // the render's end to the tick's end: the wait for the next tick
    std::uint32_t            late = 0; // intervals longer than 1.25 ticks
    Span                     tail_total;
    Span                     tail_stats;
    Span                     tail_refs;
    Span                     tail_overlay;
    Span                     tail_publish_lock;
    Span                     tail_publish_assign;
    Span                     tail_publish_list;
    Span                     tail_publish_previous;
    Span                     tail_publish_overlay;
    Span                     tail_publish_settings;
#if defined(__SWITCH__)
    std::uint64_t thread_tick_end = 0;
    std::uint64_t thread_render_end = 0;
    bool          thread_tick_valid = false;
    bool          thread_render_valid = false;
    Span          thread_interval;
    Span          thread_work;
    Span          thread_render;
    Span          thread_tail;
#endif
};

PerfLog g_perf;

double Milliseconds(PresentClock::duration duration) {
    return std::chrono::duration<double, std::milli>(duration).count();
}

std::string PerfSpan(const Span &span) {
    return std::format("{:.2f}/{:.2f}", span.Average(), span.max_ms);
}

void WriteLine(const std::string &line) {
    // One write, so the line reaches the log whole.
    std::fwrite(line.data(), 1, line.size(), stderr);
}

bool PerfTickTailEnabled() {
    static const bool enabled = [] {
        const char *value = std::getenv("DC_PERF_TICK_TAIL");
        return value != nullptr && std::strcmp(value, "1") == 0;
    }();
    return enabled;
}

bool PerfWindowOpen() {
    return g_perf.on && g_perf.window_start != PresentClock::time_point{};
}

// KZ_GPU_SCOPES: the costliest segments of the window, per scoped frame, one line each.
void PerfScopes(const gfx::FrameTimings &gpu) {
    if (gpu.scopes.empty() || gpu.scope_frames == 0) {
        return;
    }
    std::vector<const gfx::FrameTimings::Scope *> order;
    for (const gfx::FrameTimings::Scope &scope : gpu.scopes) {
        order.push_back(&scope);
    }
    std::ranges::sort(order, [](const auto *a, const auto *b) { return a->span.total_ms > b->span.total_ms; });
    const double frames = gpu.scope_frames;
    WriteLine(std::format("perf scopes: {} frames, {:.1f} passes {:.1f} barriers per frame\n", gpu.scope_frames,
                          static_cast<double>(gpu.passes) / frames, static_cast<double>(gpu.barriers) / frames));
    for (std::size_t index = 0; index < order.size() && index < 40; index++) {
        const gfx::FrameTimings::Scope &scope = *order[index];
        WriteLine(std::format("perf scope {:6.3f} ms/f x{:.1f} draws {:.1f} barriers {:.1f} max {:.3f} | {}\n",
                              scope.span.total_ms / frames, scope.span.count / frames,
                              static_cast<double>(scope.draws) / frames, static_cast<double>(scope.barriers) / frames,
                              scope.span.max_ms, scope.name));
    }
}

void PerfReport(PresentClock::time_point now) {
    const double              seconds = std::chrono::duration<double>(now - g_perf.window_start).count();
    const gfx::FrameTimings   gpu = gfx::TakeFrameTimings();
    const gfx::LogicalMapping target = gfx::GetLogicalMapping(gfx::kMainTarget);
    const double              busy = seconds > 0.0 ? (gpu.canonical_gpu.total_ms + gpu.display_gpu.total_ms) / (seconds * 10.0) : 0.0;
    WriteLine(std::format(
        "perf {:.2f}s mode {} target {}x{}: ticks {} late {} interval {} work {} render {} wait {} | rt canon {} disp {} | "
        "gpu{} canon {} disp {} busy {:.0f}% | acquire {} present {} fence {} | presents {} [{} {} {} {}] interval {} | "
        "syncs {}\n",
        seconds, game::mode, target.pixel_width, target.pixel_height, g_perf.interval.count, g_perf.late,
        PerfSpan(g_perf.interval), PerfSpan(g_perf.work), PerfSpan(g_perf.render), PerfSpan(g_perf.wait),
        PerfSpan(gpu.canonical_cpu), PerfSpan(gpu.display_cpu), gpu.gpu_timestamps ? "" : " (none)",
        PerfSpan(gpu.canonical_gpu), PerfSpan(gpu.display_gpu), busy, PerfSpan(gpu.acquire), PerfSpan(gpu.queue_present),
        PerfSpan(gpu.fence_wait), gpu.queue_present.count, gpu.interval_buckets[0], gpu.interval_buckets[1],
        gpu.interval_buckets[2], gpu.interval_buckets[3], PerfSpan(gpu.present_interval), gpu.sync_waits));
    PerfScopes(gpu);
    if (g_perf.tail_total.count != 0) {
        WriteLine(std::format("cpu tick tail wall ms: total {} stats {} refs {} overlay {} publish_lock {} "
                              "publish_assign {} | samples {} {} {} {} {} {}\n",
                              PerfSpan(g_perf.tail_total), PerfSpan(g_perf.tail_stats), PerfSpan(g_perf.tail_refs),
                              PerfSpan(g_perf.tail_overlay), PerfSpan(g_perf.tail_publish_lock),
                              PerfSpan(g_perf.tail_publish_assign), g_perf.tail_total.count, g_perf.tail_stats.count,
                              g_perf.tail_refs.count, g_perf.tail_overlay.count, g_perf.tail_publish_lock.count,
                              g_perf.tail_publish_assign.count));
        WriteLine(std::format("cpu tick publish wall ms: list {} previous {} overlay {} settings {} | samples {} {} {} {}\n",
                              PerfSpan(g_perf.tail_publish_list), PerfSpan(g_perf.tail_publish_previous),
                              PerfSpan(g_perf.tail_publish_overlay), PerfSpan(g_perf.tail_publish_settings),
                              g_perf.tail_publish_list.count, g_perf.tail_publish_previous.count,
                              g_perf.tail_publish_overlay.count, g_perf.tail_publish_settings.count));
    }
#if defined(__SWITCH__)
    if (g_perf.thread_interval.count != 0) {
        WriteLine(std::format("cpu main scheduled ms: interval {} work {} render {} tail {} | wall work {} render {} "
                              "tail {} | samples {} {} {}\n",
                              PerfSpan(g_perf.thread_interval), PerfSpan(g_perf.thread_work),
                              PerfSpan(g_perf.thread_render), PerfSpan(g_perf.thread_tail), PerfSpan(g_perf.work),
                              PerfSpan(g_perf.render), PerfSpan(g_perf.wait), g_perf.thread_work.count,
                              g_perf.thread_render.count, g_perf.thread_tail.count));
    }
    platform::SwitchLogClockRates();
    const std::uint64_t thread_tick_end = g_perf.thread_tick_end;
    const bool          thread_tick_valid = g_perf.thread_tick_valid;
#endif
    const bool on = g_perf.on;
    g_perf = {};
    g_perf.on = on;
    g_perf.window_start = now;
    g_perf.tick_end = now;
#if defined(__SWITCH__)
    g_perf.thread_tick_end = thread_tick_end;
    g_perf.thread_tick_valid = thread_tick_valid;
#endif
}

// The end of a tick: MGEndFrame's wait for the next one is over.
void PerfTickEnd() {
    if (!g_perf.on) {
        return;
    }
    const PresentClock::time_point now = PresentClock::now();
#if defined(__SWITCH__)
    std::uint64_t thread_now = 0;
    const bool    thread_valid = platform::SwitchReadThreadTicks(thread_now);
#endif
    if (g_perf.window_start == PresentClock::time_point{}) {
        g_perf.window_start = now;
        g_perf.tick_end = now;
#if defined(__SWITCH__)
        g_perf.thread_tick_end = thread_now;
        g_perf.thread_tick_valid = thread_valid;
#endif
        gfx::TakeFrameTimings();
        return;
    }
    const double interval = Milliseconds(now - g_perf.tick_end);
    g_perf.interval.Add(interval);
    if (interval > 1.25 * 1000.0 / ClockTickRate()) {
        ++g_perf.late;
    }
    if (g_perf.render_end > g_perf.tick_end) {
        g_perf.wait.Add(Milliseconds(now - g_perf.render_end));
#if defined(__SWITCH__)
        if (thread_valid && g_perf.thread_render_valid && thread_now >= g_perf.thread_render_end) {
            g_perf.thread_tail.Add(platform::SwitchThreadTicksMilliseconds(thread_now - g_perf.thread_render_end));
        }
#endif
    }
#if defined(__SWITCH__)
    if (thread_valid && g_perf.thread_tick_valid && thread_now >= g_perf.thread_tick_end) {
        g_perf.thread_interval.Add(platform::SwitchThreadTicksMilliseconds(thread_now - g_perf.thread_tick_end));
    }
    g_perf.thread_tick_end = thread_now;
    g_perf.thread_tick_valid = thread_valid;
#endif
    g_perf.tick_end = now;
    if (now - g_perf.window_start >= std::chrono::seconds(1)) {
        PerfReport(now);
    }
}

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
    dc::text::textwipe::Update();
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
    const bool          font = dc::text::caption::Ready();
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
#if defined(__SWITCH__)
    std::uint64_t thread_start = 0;
    const bool    thread_start_valid = PerfWindowOpen() && platform::SwitchReadThreadTicks(thread_start);
#endif
    gfx::RenderList(*list, 1.0f, {.canonical = true});
    g_stats.canonical_seconds += SecondsSince(start);
    if (PerfWindowOpen()) {
        g_perf.render_end = PresentClock::now();
        g_perf.work.Add(Milliseconds(start - g_perf.tick_end));
        g_perf.render.Add(Milliseconds(g_perf.render_end - start));
#if defined(__SWITCH__)
        std::uint64_t thread_end = 0;
        const bool    thread_end_valid = platform::SwitchReadThreadTicks(thread_end);
        if (thread_start_valid && g_perf.thread_tick_valid && thread_start >= g_perf.thread_tick_end) {
            g_perf.thread_work.Add(platform::SwitchThreadTicksMilliseconds(thread_start - g_perf.thread_tick_end));
        }
        if (thread_start_valid && thread_end_valid && thread_end >= thread_start) {
            g_perf.thread_render.Add(platform::SwitchThreadTicksMilliseconds(thread_end - thread_start));
        }
        g_perf.thread_render_end = thread_end;
        g_perf.thread_render_valid = thread_end_valid;
#endif
    }
    const bool               measure_tail = PerfWindowOpen() && PerfTickTailEnabled();
    PresentClock::time_point tail_start;
    PresentClock::time_point phase_start;
    if (measure_tail) {
        tail_start = phase_start = PresentClock::now();
    }
    const auto note_tail = [&](Span &span) {
        if (measure_tail) {
            const auto end = PresentClock::now();
            span.Add(Milliseconds(end - phase_start));
            phase_start = end;
        }
    };
    const auto stamp = [&](PresentClock::time_point &point) {
        if (measure_tail) {
            point = PresentClock::now();
        }
    };
    g_stats.ticks++;
    gfx::DisplayListStats counts = gfx::ListStats(*list);
    g_stats.mesh_draws += counts.mesh_draws;
    g_stats.keyed_mesh_draws += counts.keyed_mesh_draws;
    g_stats.draws_2d += counts.draws_2d;
    g_stats.stateful += counts.stateful;
    g_stats.max_draws = std::max<std::uint64_t>(g_stats.max_draws, counts.mesh_draws + counts.draws_2d);
    g_stats.last_draws = counts.mesh_draws + counts.draws_2d;
    g_fps.ticks.Count(PresentClock::now());
    note_tail(g_perf.tail_stats);
    g_previous_list = std::move(g_list);
    g_list = std::move(list);
    g_tick_shown = false;
    note_tail(g_perf.tail_refs);
    if (PresentAsync()) {
        FpsOverlayList();
        note_tail(g_perf.tail_overlay);
        const auto length = std::chrono::duration_cast<PresentClock::duration>(
            std::chrono::duration<double>(1.0 / ClockTickRate()));
        const auto elapsed = std::chrono::duration_cast<PresentClock::duration>(
            std::chrono::duration<double>(ClockTickFraction() / ClockTickRate()));
        PresentClock::time_point lock_start;
        PresentClock::time_point locked;
        PresentClock::time_point list_published;
        PresentClock::time_point previous_published;
        PresentClock::time_point overlay_published;
        PresentClock::time_point published;
        stamp(lock_start);
        {
            std::scoped_lock lock(g_publish_mutex);
            stamp(locked);
            g_published.list = g_list;
            stamp(list_published);
            g_published.previous = g_previous_list;
            stamp(previous_published);
            g_published.overlay = g_fps.on ? g_fps.list : nullptr;
            stamp(overlay_published);
            g_published.settings = g_present;
            g_published.start = PresentClock::now() - elapsed;
            g_published.length = length;
            ++g_published.serial;
            stamp(published);
        }
        if (measure_tail) {
            g_perf.tail_publish_lock.Add(Milliseconds(locked - lock_start));
            g_perf.tail_publish_assign.Add(Milliseconds(published - locked));
            g_perf.tail_publish_list.Add(Milliseconds(list_published - locked));
            g_perf.tail_publish_previous.Add(Milliseconds(previous_published - list_published));
            g_perf.tail_publish_overlay.Add(Milliseconds(overlay_published - previous_published));
            g_perf.tail_publish_settings.Add(Milliseconds(published - overlay_published));
        }
        gfx::WakeRenderThread();
    }
    if (measure_tail) {
        g_perf.tail_total.Add(Milliseconds(PresentClock::now() - tail_start));
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
    PerfTickEnd();
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

void GameSetPerfLog(bool on) {
    g_perf = {};
    g_perf.on = on;
    gfx::SetFrameTimingEnabled(on);
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
    if (g_perf.on && std::getenv("DC_PERF_HOLD") != nullptr && g_list) {
        PresentCanonicalFrame();
    }
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

} // namespace dc::app
