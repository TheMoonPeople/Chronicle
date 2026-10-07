#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "audio/mixer.hpp"
#include "battle_globals.hpp"
#include "dataread.hpp"
#include "dun/gameloop.hpp"
#include "exitcodes.hpp"
#include "gameloop.hpp"
#include "gamemode.hpp"
#include "gfx/gfx.hpp"
#include "langset.hpp"
#include "localize.hpp"
#include "menu_option.hpp"
#include "menu_save.hpp"
#include "mglib.hpp"
#include "nowload.hpp"
#include "platform/audio.hpp"
#include "platform/clock.hpp"
#include "platform/config.hpp"
#include "platform/discord.hpp"
#include "platform/display.hpp"
#include "platform/firstrun.hpp"
#include "platform/input.hpp"
#include "platform/input_script.hpp"
#include "platform/paths.hpp"
#include "platform/window.hpp"
#include "snd.hpp"
#include "title/opening.hpp"
#include "title/rushmovi.hpp"
#include "title/title.hpp"

int  EditInit(void *param);
int  EditLoop();
void SndInit();

namespace fs = std::filesystem;

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
    bool         fast_load = false;
    bool         show_fps = false;
    bool         screenshot_fps = false;
    const char  *export_text = nullptr;
};

Options g_options;

[[noreturn]] void Usage(const char *program) {
    std::fprintf(stderr,
                 "usage: %s [--data DIR] [--save DIR] [--headless] [--frames N] [--screenshot PATH]\n"
                 "          [--input FILE] [--width W] [--height H] [--offscreen]\n"
                 "          [--display-per-tick N] [--show-fps] [--jump MODE[:MAP]] [--fast-load]\n"
                 "  --data DIR         the extracted game data (default: DC_DATA, then ./data, then data/\n"
                 "                     beside the executable)\n"
                 "  --save DIR         saves, config.json and the pipeline cache (default: DC_SAVE, then\n"
                 "                     ./save, then save/ beside the executable)\n"
                 "  --headless         render offscreen (SDL offscreen driver, VK_EXT_headless_surface),\n"
                 "                     no audio device, the game clock unbounded\n"
                 "  --offscreen        --headless without a Vulkan surface: render to an image only (what\n"
                 "                     --headless does when there is no VK_EXT_headless_surface)\n"
                 "  --frames N         stop after N frames of the game's main loop\n"
                 "  --screenshot PATH  write the last tick's image (no FPS counter) to PATH on exit\n"
                 "  --input FILE       drive pad 1 from a script (default: DC_INPUT); see docs/PC.md\n"
                 "  --width, --height  window size in pixels (default: config.json, then the monitor's)\n"
                 "  --screenshot-fps   with --screenshot and --show-fps: the image as a window shows it, the\n"
                 "                     FPS counter over it\n"
                 "  --display-per-tick N  headless: also render N interpolated display frames per tick\n"
                 "  --export-text DIR  write the game's message files to DIR as language JSON (docs/LOCALIZATION.md)\n"
                 "  --show-fps         draw the FPS counter on presented frames when headless too\n"
                 "test hooks:\n"
                 "  --jump MODE[:MAP]  start in edit:<map>, dungeon:<0-6>, title, rush, opening or menu,\n"
                 "                     skipping the warm-up (DC_JUMP); see docs/PC.md\n"
                 "  --fast-load        loading-screen holds and fades of a few ticks (DC_FAST_LOAD=1)\n",
                 program);
    std::exit(kExitUsage);
}

Options ParseOptions(int argc, const char **argv) {
    Options options;
    for (int i = 1; i < argc; i++) {
        std::string_view arg = argv[i];
        auto             value = [&]() {
            if (i + 1 >= argc) {
                Usage(argv[0]);
            }
            return argv[++i];
        };
        auto number = [&]() {
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
        } else if (arg == "--width") {
            options.width = static_cast<int>(number());
        } else if (arg == "--height") {
            options.height = static_cast<int>(number());
        } else if (arg == "--jump") {
            options.jump = value();
        } else if (arg == "--fast-load") {
            options.fast_load = true;
        } else if (arg == "--export-text") {
            options.export_text = value();
        } else if (arg == "--show-fps") {
            options.show_fps = true;
        } else if (arg == "--screenshot-fps") {
            options.screenshot_fps = true;
        } else if (arg == "--display-per-tick") {
            options.display_per_tick = static_cast<int>(number());
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
                     "or pass --data <dir>\n",
                     PathsDisplay(root).c_str(), why, PathsDisplay(root).c_str());
        std::exit(kExitNoData);
    }
}

// The Options screen keeps the interface at 100%, so a size that takes the HUD past the window
// cannot take the screen's own controls with it.
gfx::FrameLayout Layout(const Config &config) {
    return {config.aspect == ConfigAspect::Auto ? gfx::AspectMode::Fill : gfx::AspectMode::Letterbox,
            MenuOptionOpen() ? 1.0f : config.ui_scale};
}

void PumpHost() {
    if (!WindowPollEvents()) {
        GameRequestStop();
    }
    DisplayPump();
    // A change of video.aspect or video.ui_scale, or the Options screen opening or closing, waits
    // for a pump outside a frame.
    if (!gfx::InFrame()) {
        gfx::FrameLayout layout = Layout(ConfigGet());
        gfx::FrameLayout current = gfx::CurrentFrameLayout();
        if (layout.aspect != current.aspect || layout.ui_scale != current.ui_scale) {
            gfx::SetFrameLayout(layout);
        }
    }
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

void RenderAudio(void *, float *out, int frames) {
    audio::DefaultMixer().Render(out, frames);
}

void ReportShaderProgress(uint32_t done, uint32_t total) {
    static uint32_t    reported = 0;
    constexpr uint32_t kSteps = 4;
    if (total == 0) {
        return;
    }
    uint32_t step = done * kSteps / total;
    if (done == 0 || step > reported) {
        reported = step;
        std::fprintf(stderr, "compiling shaders %u/%u\n", done, total);
    }
}

gfx::PresentMode PresentMode(ConfigPresentMode mode) {
    switch (mode) {
        case ConfigPresentMode::Mailbox:
            return gfx::PresentMode::Mailbox;
        case ConfigPresentMode::Immediate:
            return gfx::PresentMode::Immediate;
        default:
            return gfx::PresentMode::Fifo;
    }
}

GamePresentSettings PresentSettings(const Config &config) {
    return {.interpolation = config.interpolation,
            .max_fps = config.max_fps,
            .display_per_tick = g_options.display_per_tick,
            .show_fps = g_options.show_fps || (config.show_fps && !g_options.headless),
            .fps_detail = config.fps_detail};
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
    InputApplyConfig(after);
    if (after.tick_rate != before.tick_rate) {
        ClockSetTickRate(after.tick_rate);
    }
    if (after.interpolation != before.interpolation || after.max_fps != before.max_fps ||
        after.show_fps != before.show_fps || after.fps_detail != before.fps_detail) {
        GameSetPresentSettings(PresentSettings(after));
    }
    gfx::SetPresentMode(PresentMode(after.present_mode));
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
    double ticks = static_cast<double>(stats.ticks);
    double displays = static_cast<double>(std::max<std::uint64_t>(stats.display_frames, 1));
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
    return kExitOk;
}

int Run(int argc, const char **argv) {
    argc = PathsConsumeArgs(argc, argv);
    g_options = ParseOptions(argc, argv);
    Options &options = g_options;
    FirstRunIfNoData(options.headless);
    if (options.jump == nullptr) {
        options.jump = std::getenv("DC_JUMP");
    }
    if (options.jump != nullptr && *options.jump != '\0' && !GameSetJump(options.jump)) {
        std::fprintf(stderr, "bad --jump: %s\n", options.jump);
        std::exit(kExitUsage);
    }
    const char *fast_load = std::getenv("DC_FAST_LOAD");
    GameSetFastLoad(options.fast_load || (fast_load != nullptr && *fast_load != '\0' && *fast_load != '0'));
    RequireData();
    if (options.export_text == nullptr) {
        // Once per save folder: the extracted data's text as JSON for translators (docs/LOCALIZATION.md).
        std::filesystem::path seed = PathsSaveRoot() / "lang-export";
        std::error_code       error;
        if (!std::filesystem::exists(seed / "en_gb.json", error)) {
            std::fprintf(stderr, "exporting the game text to %s\n", PathsDisplay(seed).c_str());
            if (LocalizeExport(seed, false) < 0) {
                std::fprintf(stderr, "the game text could not be exported\n");
            }
        }
    }
    if (options.export_text != nullptr) {
        // The game's message files as language JSON for translators; no window, no game.
        return LocalizeExport(options.export_text, true) >= 0 ? kExitOk : kExitUsage;
    }
    LoadInputScript(options.input);

    ConfigLoad();
    const Config &config = ConfigGet();

    DisplaySetOverrides(options.width, options.height, options.headless);
    WindowConfig window = DisplayWindowConfig(config);
    bool         offscreen = options.offscreen || (options.headless && !gfx::HeadlessSurfaceAvailable());
    window.vulkan = !offscreen;
    WindowInit(window);
    InputInit();

    gfx::RendererConfig renderer;
    renderer.present_mode = PresentMode(config.present_mode);
    renderer.pipeline_cache = PathsSaveRoot() / "pipeline_cache.bin";
    renderer.progress = ReportShaderProgress;
    renderer.offscreen = offscreen;
    renderer.layout = Layout(config);
    gfx::RendererInit(WindowHandle(), renderer);

    audio::DefaultMixer().SetMasterGain(config.master_volume);
    AudioOutputStart(audio::DefaultMixer().Rate(), RenderAudio, nullptr);

    ClockSetTickRate(config.tick_rate);
    ClockSetUnbounded(options.headless);
    ClockAddPumpHook(PumpHost);
    GameSetFrameBudget(options.frames);
    ApplyDiscord(config);
    GameSetPresentSettings(PresentSettings(config));
    ConfigAddChangeHook(ApplyConfigChange);
    ConfigAddChangeHook(DisplayChanged);
    ConfigAddChangeHook(GameOptionsChanged);
    ConfigAddChangeHook(LocalizeConfigChanged);

    int status = RunGame(argc, const_cast<char **>(argv));
    if (status == kExitOk && options.screenshot != nullptr) {
        status = Screenshot(options.screenshot);
    }
    ReportPresentStats();

    ConfigRemoveChangeHook(LocalizeConfigChanged);
    ConfigRemoveChangeHook(GameOptionsChanged);
    ConfigRemoveChangeHook(DisplayChanged);
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

PC_OVERRIDE int main(int argc, const char **argv, const char **envp) {
    return Run(argc, argv);
}

extern "C" {
void init_all__Fv() {
    init_all();
}

void initialize_data__Fv() {
    initialize_data();
}

void GlobalNameInit__Fv() {
    GlobalNameInit();
}

void InitReadBG__Fv() {
    InitReadBG();
}

void SndInit__Fv() {
    SndInit();
}

void LoadOverlay__Fi(int mode) {
    LoadOverlay(mode);
}

void MGSetRenderInfo__Ffff(float scale, float near_z, float far_z) {
    MGSetRenderInfo(scale, near_z, far_z);
}

void init_now_loading__Fi(int title_number) {
    init_now_loading(title_number);
}

void LoadSystemMessage__Fv() {
    LoadSystemMessage();
}

void SndInitialize__Fiiii(int unused0, int unused1, int unused2, int unused3) {
    SndInitialize(unused0, unused1, unused2, unused3);
}

int InitExistData__Fv() {
    return InitExistData();
}

void MapJump__Fii(int map_no, int event_no) {
    MapJump(map_no, event_no);
}

void EditInit__FPv(void *param) {
    EditInit(param);
}

void MenuInit__Fv(int mode) {
    MenuInit();
}

void MemCheckInit__Fv(int mode) {
    MemCheckInit();
}

void TrialEndInit__Fv(int mode) {
    TrialEndInit();
}

void InitSave__Fv(int mode) {
    InitSave();
}

void LangsetInit__Fv(int mode) {
    LangsetInit();
}

int check_now_loading__Fv() {
    return check_now_loading();
}

void MGInitVSyncCallBack__FPFi_i(int (*callback)(int)) {
    MGInitVSyncCallBack(callback);
}

void PlayTimeCount__Fi(int add) {
    PlayTimeCount(add);
}

void MGBeginFrame__Fv() {
    MGBeginFrame();
}

void SetEnv__FP13sceVif1Packet(sceVif1Packet *vif1_packet) {
    SetEnv(vif1_packet);
}

int EditLoop__Fv() {
    return EditLoop();
}

int MenuLoop__Fv() {
    return MenuLoop();
}

int MemCheckLoop__Fv() {
    return MemCheckLoop();
}

int TrialEndLoop__Fv() {
    return TrialEndLoop();
}

int LoopSave__Fv() {
    return LoopSave();
}

int LangsetLoop__Fv() {
    return LangsetLoop();
}

void MGEndFrame__Fv() {
    MGEndFrame();
}

int CheckTrialEnd__Fv() {
    return CheckTrialEnd();
}

int ReadBGSync__Fv() {
    return ReadBGSync();
}

void TrialStart__Fv() {
    TrialStart();
}

void func_01DAC1C0() {
    GameInit();
}

int func_01DAD980(int mode) {
    return GameLoop();
}

void func_01DAF1C0() {
    OpeningInit();
}

int func_01DAF970() {
    return OpeningLoop();
}

void func_01DC1420(int mode) {
    LoaderInit();
}

int func_01DC1510() {
    return LoaderLoop();
}

void func_01DC8C50() {
    RushInit();
}

int func_01DC8EB0() {
    return RushLoop();
}

void func_01DD1AB0(int inited) {
    TitleInit(inited);
}

int func_01DD2220() {
    return TitleLoop();
}
}

#ifdef _WIN32
int wmain(int argc, wchar_t **wide_argv) {
    std::vector<std::string>  args;
    std::vector<const char *> argv;
    for (int i = 0; i < argc; ++i) {
        args.push_back(PathsDisplay(fs::path(wide_argv[i])));
    }
    for (const std::string &arg : args) {
        argv.push_back(arg.c_str());
    }
    argv.push_back(nullptr);
    return Run(argc, argv.data());
}
#endif
