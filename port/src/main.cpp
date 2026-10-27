#include <SDL3/SDL_stdinc.h>
#ifdef __SWITCH__
#include <SDL3/SDL.h>

#include "switch_main.hpp"
#endif

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#ifdef __ANDROID__
#include <android/log.h>
#define DC_ANDROID_TRACE(message) __android_log_write(ANDROID_LOG_INFO, "Chronicle", message)
#else
#define DC_ANDROID_TRACE(message) ((void) 0)
#endif

#include "app/exitcodes.hpp"
#include "app/gameloop.hpp"
#include "app/identity.hpp"
#include "audio/mixer.hpp"
#include "audio/soundtrack.hpp"
#include "battle_globals.hpp"
#include "dun/gameloop.hpp"
#include "gamemode.hpp"
#include "gfx/gfx.hpp"
#include "item/host.hpp"
#include "langset.hpp"
#include "localize/localize.hpp"
#include "menu_save.hpp"
#include "nowload.hpp"
#include "options/menu_option.hpp"
#include "platform/audio.hpp"
#include "platform/clock.hpp"
#include "platform/config.hpp"
#include "platform/data_archive.hpp"
#include "platform/datapacks.hpp"
#include "platform/discord.hpp"
#include "platform/display.hpp"
#include "platform/firstrun.hpp"
#include "platform/input.hpp"
#include "platform/input_script.hpp"
#include "platform/paths.hpp"
#include "platform/settings.hpp"
#include "platform/touch_overlay.hpp"
#include "platform/window.hpp"
#include "render/screen.hpp"
#include "save/save_state.hpp"
#include "snd.hpp"
#include "title/opening.hpp"
#include "title/rushmovi.hpp"

namespace fs = std::filesystem;

namespace dc::app {

using namespace dc::game;
using namespace dc::localize;
using namespace dc::options;
using namespace dc::platform;
using namespace dc::render;

namespace {

struct Options {
    bool         headless = false;
    bool         offscreen = false;
    std::int64_t frames = -1;
    const char  *screenshot = nullptr;
    const char  *input = nullptr;
    int          width = 0;
    int          height = 0;
    int          display_per_tick = 0;
    const char  *jump = nullptr;
    const char  *give = nullptr;
    bool         fast_load = false;
    bool         show_fps = false;
    bool         screenshot_fps = false;
    bool         perf_log = false;
    const char  *export_text = nullptr;
    const char  *export_gameplay = nullptr;
    const char  *font = nullptr;
    const char  *seed_save = nullptr;
#ifdef __SWITCH__
    bool switch_core_affinity = true;
#endif
};

Options g_options;
#ifdef __SWITCH__
float g_switch_render_scale = 0.0f;
#endif

[[noreturn]] void Usage(const char *program) {
    std::fprintf(stderr,
                 "usage: %s [--data DIR] [--save DIR] [--headless] [--frames N] [--screenshot PATH]\n"
                 "          [--input FILE]"
#ifndef __SWITCH__
                 " [--width W] [--height H]"
#endif
                 " [--offscreen]\n"
#ifndef __SWITCH__
                 "          [--display-per-tick N]"
#endif
                 " [--show-fps] [--jump MODE[:MAP]] [--fast-load]\n"
                 "          [--give ITEM,...]\n"
                 "  --data DIR         extracted game data or its sibling data.zip (default: DC_DATA,\n"
                 "                     then ./data, then data/ beside the executable)\n"
                 "  --save DIR         saves, config.json and the pipeline cache (default: DC_SAVE, then\n"
                 "                     ./save, then save/ beside the executable)\n"
                 "  --seed-save DIR    copy a save-root fixture before startup; requires explicit --save\n"
                 "                     pointing to a new or empty directory (omit on subsequent runs)\n"
                 "  --headless         render offscreen (SDL offscreen driver, VK_EXT_headless_surface),\n"
                 "                     no audio device, the game clock unbounded\n"
                 "  --offscreen        --headless without a Vulkan surface: render to an image only (what\n"
                 "                     --headless does when there is no VK_EXT_headless_surface)\n"
                 "  --frames N         stop after N frames of the game's main loop\n"
                 "  --screenshot PATH  write the last tick's image (no FPS counter) to PATH on exit\n"
                 "  --input FILE       drive pad 1 from a script (default: DC_INPUT); see docs/PC.md\n"
#ifndef __SWITCH__
                 "  --width, --height  window size in pixels (default: config.json, then the monitor's)\n"
#endif
                 "  --screenshot-fps   with --screenshot and --show-fps: the image as a window shows it, the\n"
                 "                     FPS counter over it\n"
#ifndef __SWITCH__
                 "  --display-per-tick N  headless: also render N interpolated display frames per tick\n"
#endif
                 "  --export-text DIR  write the game's message files to DIR as language JSON (docs/LOCALIZATION.md)\n"
                 "  --export-gameplay DIR  write the item tables to DIR as gameplay documents, a starting point\n"
                 "                     for a datapack (docs/ITEM_FRAMEWORK.md)\n"
                 "  --font FILE        draw message text from this TrueType font (else lang/font.ttf)\n"
                 "  --show-fps         draw the FPS counter on presented frames when headless too\n"
                 "  --perf-log         log tick, render and GPU timings once a second\n"
                 "test hooks:\n"
                 "  --jump MODE[:MAP]  start in edit:<map>, e01/e03/e04 (populated presets), dungeon:<0-6>, title, rush, opening or menu,\n"
                 "                     skipping the warm-up (DC_JUMP); see docs/PC.md\n"
                 "  --fast-load        loading-screen holds and fades of a few ticks (DC_FAST_LOAD=1)\n"
                 "  --give ITEM,...    give the party these item and weapon numbers when a town or dungeon\n"
                 "                     starts, unless it holds them (DC_GIVE); see docs/PC.md\n",
                 program);
#ifdef __SWITCH__
    std::fprintf(stderr, "  --switch-core-affinity separate|auto  pin game and render to separate cores "
                         "(default: separate)\n");
#endif
    std::exit(kExitUsage);
}

Options ParseOptions(int argc, const char **argv) {
    Options options;
    for (int i = 1; i < argc; i++) {
        std::string_view arg = argv[i];
        auto             value = [&] {
            if (i + 1 >= argc) {
                Usage(argv[0]);
            }
            return argv[++i];
        };
        auto number = [&] {
            char *end = nullptr;
            long  result = std::strtol(value(), &end, 10);
            if (end == nullptr || *end != '\0' || result < 0) {
                Usage(argv[0]);
            }
            return result;
        };
        if (arg == "--headless") {
            options.headless = true;
        } else if (arg == "--offscreen") {
            options.headless = true;
            options.offscreen = true;
        } else if (arg == "--frames") {
            options.frames = number();
        } else if (arg == "--screenshot") {
            options.screenshot = value();
        } else if (arg == "--input") {
            options.input = value();
        } else if (arg == "--seed-save") {
            options.seed_save = value();
#ifndef __SWITCH__
        } else if (arg == "--width") {
            options.width = static_cast<int>(number());
        } else if (arg == "--height") {
            options.height = static_cast<int>(number());
#endif
        } else if (arg == "--jump") {
            options.jump = value();
        } else if (arg == "--give") {
            options.give = value();
        } else if (arg == "--fast-load") {
            options.fast_load = true;
        } else if (arg == "--export-text") {
            options.export_text = value();
        } else if (arg == "--export-gameplay") {
            options.export_gameplay = value();
        } else if (arg == "--font") {
            options.font = value();
        } else if (arg == "--show-fps") {
            options.show_fps = true;
        } else if (arg == "--screenshot-fps") {
            options.screenshot_fps = true;
        } else if (arg == "--perf-log") {
            options.perf_log = true;
#ifndef __SWITCH__
        } else if (arg == "--display-per-tick") {
            options.display_per_tick = static_cast<int>(number());
#endif
#ifdef __SWITCH__
        } else if (arg == "--switch-core-affinity") {
            std::string_view mode = value();
            if (mode != "separate" && mode != "auto") {
                Usage(argv[0]);
            }
            options.switch_core_affinity = mode == "separate";
#endif
        } else {
            Usage(argv[0]);
        }
    }
    return options;
}

// InitCDFile stops on the same conditions, but checking first gives the one-line message and its
// own exit status before a window or a Vulkan device exists.
void RequireData() {
    const fs::path &root = PathsDataRoot();
    if (DataArchivePresent(root)) {
        return;
    }
    std::error_code error;
    const char     *why = nullptr;
    if (!fs::is_directory(root, error)) {
        why = "is not a directory";
    } else {
        why = "is empty";
        auto options = fs::directory_options::follow_directory_symlink |
                       fs::directory_options::skip_permission_denied;
        for (fs::recursive_directory_iterator it(root, options, error), end; !error && it != end;
             it.increment(error)) {
            std::error_code entry_error;
            if (it->is_regular_file(entry_error)) {
                why = nullptr;
                break;
            }
        }
    }
    if (why != nullptr) {
        std::fprintf(stderr,
                     "no game data: %s %s; extract the disc with `dcdata extract <disc image> %s` "
                     "or place data.zip beside it\n",
                     PathsDisplay(root).c_str(), why, PathsDisplay(root).c_str());
#ifdef __SWITCH__
        SwitchShowMissingData();
#endif
        std::exit(kExitNoData);
    }
}

// The Options screen keeps the interface at 100%, so a size that takes the HUD past the window
// cannot take the screen's own controls with it.
gfx::FrameLayout Layout(const Config &config) {
#ifdef __SWITCH__
    return {.aspect = gfx::AspectMode::Fill, .ui_scale = MenuOptionOpen() ? 1.0f : config.ui_scale};
#else
    return {.aspect = config.aspect == ConfigAspect::Auto ? gfx::AspectMode::Fill : gfx::AspectMode::Letterbox,
            .ui_scale = MenuOptionOpen() ? 1.0f : config.ui_scale};
#endif
}

#ifdef __SWITCH__
void UpdateSwitchRenderScale() {
    if (gfx::InFrame()) {
        return;
    }
    int width = 0;
    int height = 0;
    SwitchDisplaySize(width, height);
    if (height <= 0) {
        return;
    }
    const float scale = static_cast<float>(height) / static_cast<float>(kScreenHeight);
    if (scale != g_switch_render_scale) {
        if (!SDL_SetWindowSize(WindowHandle(), width, height)) {
            std::fprintf(stderr, "switch: cannot set SDL output size to %dx%d: %s\n", width, height,
                         SDL_GetError());
        }
        gfx::RendererResize();
        int window_width = 0;
        int window_height = 0;
        if (!SDL_GetWindowSizeInPixels(WindowHandle(), &window_width, &window_height)) {
            std::fprintf(stderr, "switch: cannot query SDL output size: %s\n", SDL_GetError());
        }
        std::fprintf(stderr, "switch: mode target=%dx%d, SDL=%dx%d, render scale=%.2f\n", width, height,
                     window_width, window_height, static_cast<double>(scale));
        gfx::SetRenderScale(scale);
        g_switch_render_scale = scale;
    }
}
#endif

// DC_BGM_TEST=<set>:<track>: a test aid. From frame 150 of the run (the title has started) the music
// stops and sound set <set> plays its sequence number <track> on the music port, so DC_AUDIO_WAV
// records what that sequence sounds like. Run it with --jump menu, which is silent.
void BgmTestPump() {
    static const char *spec = std::getenv("DC_BGM_TEST");
    static bool        done = false;
    if (spec == nullptr || done || GameFrameCount() < 150) {
        return;
    }
    done = true;
    int set = 0;
    int track = 0;
    if (std::sscanf(spec, "%d:%d", &set, &track) < 1) {
        return;
    }
    for (int port = 0; port < audio::kPorts; ++port) {
        audio::DefaultMixer().Stop(port);
    }
    SndBgmInit();
    SndBgmLoad(set);
    SndBgmPlay(track);
}

void PumpHost() {
    BgmTestPump();
    if (!WindowPollEvents()) {
        GameRequestStop();
    }
    DisplayPump();
#ifdef __SWITCH__
    UpdateSwitchRenderScale();
#endif
    // A change of video.aspect or video.ui_scale, or the Options screen opening or closing, waits
    // for a pump outside a frame.
    if (!gfx::InFrame()) {
        gfx::FrameLayout layout = Layout(ConfigGet());
        gfx::FrameLayout current = gfx::CurrentFrameLayout();
        if (layout.aspect != current.aspect || layout.ui_scale != current.ui_scale) {
            gfx::SetFrameLayout(layout);
        }
    }
#ifdef __ANDROID__
    platform::TouchOverlayPoll();
#endif
    InputPoll();
    InputScriptApply(GameFrameCount());
}

void LoadInputScript(const char *path) {
#ifdef _WIN32
    std::string environment;
#endif
    if (path == nullptr) {
#ifdef _WIN32
        if (const wchar_t *value = ::_wgetenv(L"DC_INPUT")) {
            environment = PathsDisplay(fs::path(value));
            path = environment.c_str();
        }
#else
        path = std::getenv("DC_INPUT");
#endif
    }
    if (path == nullptr || *path == '\0') {
        return;
    }
    InputScript script;
    std::string error;
    if (!InputScriptLoad(PathsFromUtf8(path), script, error)) {
        std::fprintf(stderr, "bad input script: %s\n", error.c_str());
        std::exit(kExitUsage);
    }
    InputScriptInstall(std::move(script));
}

void RenderAudio(void * /*user*/, float *out, int frames) {
    audio::DefaultMixer().Render(out, frames);
}

void ReportShaderProgress(uint32_t done, uint32_t total) {
    static uint32_t    reported = 0;
    constexpr uint32_t steps = 4;
    if (total == 0) {
        return;
    }
    uint32_t step = done * steps / total;
    if (done == 0 || step > reported) {
        reported = step;
        std::fprintf(stderr, "compiling shaders %u/%u\n", done, total);
    }
}

gfx::PresentMode PresentMode(ConfigPresentMode mode) {
#ifdef __SWITCH__
    static_cast<void>(mode);
    return gfx::PresentMode::Fifo;
#else
    switch (mode) {
        case ConfigPresentMode::Mailbox:
            return gfx::PresentMode::Mailbox;
        case ConfigPresentMode::Immediate:
            return gfx::PresentMode::Immediate;
        default:
            return gfx::PresentMode::Fifo;
    }
#endif
}

GamePresentSettings PresentSettings(const Config &config) {
#ifdef __SWITCH__
    return {.interpolation = false,
            .max_fps = 60.0,
            .display_per_tick = 0,
            .show_fps = g_options.show_fps || (config.show_fps && !g_options.headless),
            .fps_detail = config.fps_detail};
#else
    return {.interpolation = config.interpolation,
            .max_fps = config.max_fps,
            .display_per_tick = g_options.display_per_tick,
            .show_fps = g_options.show_fps || (config.show_fps && !g_options.headless),
            .fps_detail = config.fps_detail};
#endif
}

void ApplyDiscord(const Config &config) {
    DiscordStop();
    if (config.discord_rich_presence && !g_options.headless) {
        DiscordStart(kDiscordClientId);
    }
}

// Called mid-tick, from a settings screen: what cannot change inside a frame (the frame layout)
// waits for PumpHost.
void ApplyConfigChange(const Config &before, const Config &after) {
    audio::DefaultMixer().SetMasterGain(after.master_volume);
    AudioSetSurround(after.surround);
    InputApplyConfig(after);
#ifdef __SWITCH__
    ClockSetTickRate(60.0);
#else
    if (after.tick_rate != before.tick_rate) {
        ClockSetTickRate(after.tick_rate);
    }
#endif
    if (after.interpolation != before.interpolation || after.max_fps != before.max_fps ||
        after.show_fps != before.show_fps || after.fps_detail != before.fps_detail) {
        GameSetPresentSettings(PresentSettings(after));
    }
    gfx::SetPresentMode(PresentMode(after.present_mode));
    gfx::SetAnisotropy(after.anisotropy);
#ifdef __ANDROID__
    if (after.render_scale != before.render_scale) {
        gfx::SetRenderScale(after.render_scale);
    }
#endif
#ifdef __ANDROID__
    if (after.touch_overlay_opacity != before.touch_overlay_opacity) {
        platform::TouchOverlaySetOpacity(after.touch_overlay_opacity / 100.0f);
    }
#endif
    if (after.discord_rich_presence != before.discord_rich_presence) {
        ApplyDiscord(after);
    }
}

// DC_PRESENT_STATS=1: what the ticks drew and what rendering them cost.
void ReportPresentStats() {
    const char *setting = std::getenv("DC_PRESENT_STATS");
    if (setting == nullptr || *setting == '\0' || *setting == '0') {
        return;
    }
    GamePresentStats stats = GamePresentStatistics();
    if (stats.ticks == 0) {
        return;
    }
    auto ticks = static_cast<double>(stats.ticks);
    auto displays = static_cast<double>(std::max<std::uint64_t>(stats.display_frames, 1));
    std::fprintf(stderr,
                 "present: %.0f ticks, per tick %.1f mesh draws (%.1f keyed), %.1f 2D draws, %.1f stateful, "
                 "at most %.0f draws; canonical %.2f ms per tick; %.0f display frames, %.2f ms each\n",
                 ticks, static_cast<double>(stats.mesh_draws) / ticks,
                 static_cast<double>(stats.keyed_mesh_draws) / ticks, static_cast<double>(stats.draws_2d) / ticks,
                 static_cast<double>(stats.stateful) / ticks, static_cast<double>(stats.max_draws),
                 stats.canonical_seconds * 1000.0 / ticks, static_cast<double>(stats.display_frames),
                 stats.display_seconds * 1000.0 / displays);
    if (GameShowingFps()) {
        std::fprintf(stderr, "fps counter: %s\n", GameFpsText().c_str());
    }
}

int Screenshot(const char *path) {
    std::vector<uint8_t> pixels;
    uint32_t             width = 0;
    uint32_t             height = 0;
    bool                 grabbed = g_options.screenshot_fps ? GameScreenshotWithFps(pixels, width, height)
                                                            : GameScreenshot(pixels, width, height);
    if (!grabbed || !gfx::WritePng(PathsFromUtf8(path), pixels.data(), width, height)) {
        std::fprintf(stderr, "cannot write the screenshot to %s\n", path);
        return kExitFailure;
    }
    const char *rgba = std::getenv("DC_SCREENSHOT_RGBA");
    if (rgba != nullptr && std::string_view(rgba) == "1") {
        auto raw_path = PathsFromUtf8(path);
        raw_path += ".rgba";
        std::ofstream raw(raw_path, std::ios::binary);
        raw.write(reinterpret_cast<const char *>(pixels.data()), static_cast<std::streamsize>(pixels.size()));
        raw.close();
        if (!raw) {
            std::fprintf(stderr, "cannot write screenshot RGBA to %s.rgba\n", path);
            return kExitFailure;
        }
    }
    return kExitOk;
}

int Run(int argc, const char **argv) {
    DC_ANDROID_TRACE("Run: enter");
    GameSetIdentity();
    DC_ANDROID_TRACE("Run: identity set");
    const char *explicit_save = nullptr;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        if (arg == "--save" && i + 1 < argc) {
            explicit_save = argv[++i];
        } else if (arg.starts_with("--save=")) {
            explicit_save = argv[i] + std::string_view("--save=").size();
        }
    }
    argc = PathsConsumeArgs(argc, argv);
    g_options = ParseOptions(argc, argv);
    GameLoadTraceLaunch();
    Options &options = g_options;
    if (options.seed_save != nullptr) {
        if (explicit_save == nullptr || *explicit_save == '\0') {
            std::fprintf(stderr, "--seed-save requires an explicit nonempty --save directory\n");
            return kExitUsage;
        }
        std::string error;
        if (!save::SaveStateSeed(PathsFromUtf8(options.seed_save), PathsFromUtf8(explicit_save), error)) {
            std::fprintf(stderr, "cannot seed --save %s: %s\n", explicit_save, error.c_str());
            return kExitFailure;
        }
        std::fprintf(stderr, "seeded --save %s from %s\n", explicit_save, options.seed_save);
    }
    if (options.export_gameplay != nullptr) {
        // The tables are compiled in, so this needs no disc data, no window and no game.
        return item::Export(PathsFromUtf8(options.export_gameplay)) ? kExitOk : kExitFailure;
    }
    FirstRunIfNoData(options.headless);
    DC_ANDROID_TRACE("Run: first-run check passed");
    if (options.jump == nullptr) {
        options.jump = std::getenv("DC_JUMP");
    }
    if (options.give == nullptr) {
        options.give = std::getenv("DC_GIVE");
    }
    if (options.give != nullptr && *options.give != '\0' && !GameSetGive(options.give)) {
        std::fprintf(stderr, "bad --give: %s\n", options.give);
        std::exit(kExitUsage);
    }
    if (options.jump != nullptr && *options.jump != '\0' && !GameSetJump(options.jump)) {
        std::fprintf(stderr, "bad --jump: %s\n", options.jump);
        std::exit(kExitUsage);
    }
    GameSetAutosave(!options.headless && (options.jump == nullptr || *options.jump == '\0'));
    const char *fast_load = std::getenv("DC_FAST_LOAD");
    GameSetFastLoad(options.fast_load || (fast_load != nullptr && *fast_load != '\0' && *fast_load != '0'));
    RequireData();
    GameLoadTraceCheckpoint("data_ready");
    DC_ANDROID_TRACE("Run: data available");
    datapacks::Load(PathsDataRoot());
    item::Load();
    if (!options.headless) {
        GameSetDataIcon();
    }
    if (options.font != nullptr) {
        LocalizeSetFontPath(PathsFromUtf8(options.font));
    }
    if (options.export_text != nullptr) {
        // The game's message files as language JSON for translators; no window, no game.
        return LocalizeExport(options.export_text, true) >= 0 ? kExitOk : kExitUsage;
    }
    LoadInputScript(options.input);

    ConfigLoad();
    audio::SoundtrackEnsure();
    const Config &config = ConfigGet();

#ifdef __SWITCH__
    DisplaySetOverrides(0, 0, options.headless);
#else
    DisplaySetOverrides(options.width, options.height, options.headless);
#endif
    WindowConfig window = DisplayWindowConfig(config);
#ifdef __SWITCH__
    SwitchDisplaySize(window.width, window.height);
    window.fullscreen = false;
#endif
    bool offscreen = options.offscreen || (options.headless && !gfx::HeadlessSurfaceAvailable());
    window.vulkan = !offscreen;
    WindowInit(window);
    DC_ANDROID_TRACE("Run: window initialized");
    InputInit();
#ifdef __ANDROID__
    platform::TouchOverlaySetOpacity(config.touch_overlay_opacity / 100.0f);
#endif

    gfx::RendererConfig renderer;
    renderer.app_name = "Chronicle: Dark Cloud";
    renderer.logical_width = kScreenWidth;
    renderer.logical_height = kScreenHeight;
    renderer.display_aspect = 4.0f / 3.0f;
    renderer.present_mode = PresentMode(config.present_mode);
    renderer.pipeline_cache = PathsSaveRoot() / "pipeline_cache.bin";
#ifdef __SWITCH__
    SwitchConfigureRenderer(renderer);
    g_switch_render_scale = renderer.render_scale;
#endif
    renderer.progress = ReportShaderProgress;
    renderer.offscreen = offscreen;
    renderer.render_thread = config.render_thread;
    if (const char *mode = SettingsEnv("RENDER_THREAD")) {
        if (const auto parsed = gfx::ParseRenderThreadMode(mode)) {
            renderer.render_thread = *parsed;
        }
    }
    if (options.headless || options.screenshot != nullptr) {
        renderer.render_thread = gfx::RenderThreadMode::Off;
        // The renderer also reads this override; deterministic runs take precedence over it.
        SDL_setenv_unsafe("KZ_RENDER_THREAD", "off", 1);
    }
    renderer.layout = Layout(config);
#ifdef __ANDROID__
    renderer.render_scale = config.render_scale;
#endif
    renderer.validation = renderer.validation || SettingsEnv("VULKAN_VALIDATION") != nullptr;
    gfx::RendererInit(WindowHandle(), renderer);
    GameLoadTraceCheckpoint("renderer_ready");
    DC_ANDROID_TRACE("Run: renderer initialized");
#ifdef __SWITCH__
    if (options.switch_core_affinity) {
        SwitchConfigureThreads();
    }
#endif
    gfx::SetAnisotropy(config.anisotropy);
#ifdef __ANDROID__
    gfx::SetRenderScale(config.render_scale);
#endif

    audio::DefaultMixer().SetMasterGain(config.master_volume);
    AudioSetSurround(config.surround);
    AudioOutputStart(audio::DefaultMixer().Rate(), RenderAudio, nullptr);

#ifdef __SWITCH__
    ClockSetTickRate(60.0);
#else
    ClockSetTickRate(config.tick_rate);
#endif
    const char *bounded_headless = std::getenv("DC_HEADLESS_BOUNDED");
    ClockSetUnbounded(options.headless && (bounded_headless == nullptr || std::string_view(bounded_headless) != "1"));
    ClockAddPumpHook(PumpHost);
    GameSetFrameBudget(options.frames);
    GameSetPerfLog(options.perf_log);
    if (options.perf_log) {
        std::fprintf(stderr,
                     "perf config: layout %d scale %.3f thread %d interpolation %d detail %.3f shadow %.3f "
                     "anisotropy %d soft_focus %d\n",
                     static_cast<int>(renderer.layout.aspect), gfx::RenderScale(),
                     static_cast<int>(gfx::ActiveRenderThreadMode()), PresentSettings(config).interpolation,
                     ConfigDetailDistance(), ConfigShadowDistance(), config.anisotropy, config.options.soft_focus);
    }
    ApplyDiscord(config);
    GameSetPresentSettings(PresentSettings(config));
    ConfigAddChangeHook(ApplyConfigChange);
    ConfigAddChangeHook(ConfigDisplayChanged);
    ConfigAddChangeHook(GameOptionsChanged);
    ConfigAddChangeHook(LocalizeConfigChanged);

    int status = RunGame(argc, const_cast<char **>(argv));
    GameStopPresentation();
    if (status == kExitOk && options.screenshot != nullptr) {
        status = Screenshot(options.screenshot);
    }
    ReportPresentStats();
#ifdef __SWITCH__
    if (options.perf_log && std::getenv("DC_PERF_HOLD") != nullptr) {
        for (;;) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
#endif

    ConfigRemoveChangeHook(LocalizeConfigChanged);
    ConfigRemoveChangeHook(GameOptionsChanged);
    ConfigRemoveChangeHook(ConfigDisplayChanged);
    ConfigRemoveChangeHook(ApplyConfigChange);
    ClockRemovePumpHook(PumpHost);
    DiscordStop();
    AudioOutputStop();
    InputShutdown();
    gfx::RendererShutdown();
    WindowShutdown();
    return status;
}

} // namespace
} // namespace dc::app

int main(int argc, const char **argv) {
#ifdef __SWITCH__
    return dc::platform::SwitchMain(argc, argv, dc::app::Run);
#else
    return dc::app::Run(argc, argv);
#endif
}

#ifdef __ANDROID__
// SDLActivity resolves SDL_main from the native application library.
extern "C" int SDL_main(int argc, char **argv) {
    DC_ANDROID_TRACE("SDL_main: entry");
    return dc::app::Run(argc, const_cast<const char **>(argv));
}
#endif

#ifdef _WIN32
int wmain(int argc, wchar_t **wide_argv) {
    std::vector<std::string>  args;
    std::vector<const char *> argv;
    args.reserve(static_cast<std::size_t>(argc));
    argv.reserve(static_cast<std::size_t>(argc) + 1);
    for (int i = 0; i < argc; ++i) {
        args.push_back(PathsDisplay(fs::path(wide_argv[i])));
    }
    for (const std::string &arg : args) {
        argv.push_back(arg.c_str());
    }
    argv.push_back(nullptr);
    return dc::app::Run(argc, argv.data());
}
#endif
