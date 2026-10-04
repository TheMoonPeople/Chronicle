#include "firstrun.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <format>
#include <mutex>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#include "config.hpp"
#include "exitcodes.hpp"
#include "gfx/gfx.hpp"
#include "overlay.hpp"
#include "paths.hpp"
#include "window.hpp"

namespace fs = std::filesystem;

namespace {

using Color = std::array<uint8_t, 4>;

constexpr Color kBackground = {0x14, 0x1a, 0x2a, 0x80};
constexpr Color kText = {0xe8, 0xe8, 0xe8, 0x80};
constexpr Color kDim = {0x90, 0x98, 0xa8, 0x80};
constexpr Color kAccent = {0xe8, 0xb0, 0x4a, 0x80};
constexpr Color kButton = {0x2c, 0x38, 0x56, 0x80};
constexpr Color kButtonLit = {0x46, 0x5c, 0x8c, 0x80};
constexpr Color kTrack = {0x2c, 0x38, 0x56, 0x80};

// Neither a dialog answer nor progress has a frame rate of its own; this bounds how long the screen
// goes without pumping events when nothing arrives.
constexpr int                       kIdleMilliseconds = 50;
constexpr std::chrono::milliseconds kIdle{kIdleMilliseconds};

FirstRunChooser chooser;

constexpr float kAdvance = kOverlayAdvance;

float TextWidth(std::string_view text, float scale) {
    return text.empty() ? 0.0f : (static_cast<float>(text.size()) * kAdvance - 1.0f) * scale;
}

// Keeps the end of a path, which names the file, when the whole does not fit.
std::string Fit(std::string_view text, float scale, float width) {
    std::size_t room = static_cast<std::size_t>((width / scale + 1.0f) / kAdvance);
    if (text.size() <= room) {
        return std::string(text);
    }
    return room <= 3 ? std::string() : "..." + std::string(text.substr(text.size() - (room - 3)));
}

// Everything a screen draws, in the game's logical 640x480, as one batch of untextured quads.
struct Canvas {
    std::vector<gfx::Vertex2D> quads;

    void Rect(float x, float y, float w, float h, const Color &color) {
        auto vertex = [&](float vx, float vy) {
            return gfx::Vertex2D{
                vx, vy, 0.0f, 0.0f, 0.0f, {color[0], color[1], color[2], color[3]}
            };
        };
        quads.push_back(vertex(x, y));
        quads.push_back(vertex(x + w, y));
        quads.push_back(vertex(x + w, y + h));
        quads.push_back(vertex(x, y + h));
    }

    void Text(float x, float y, float scale, std::string_view text, const Color &color) {
        for (char c : text) {
            if (const std::uint8_t *rows = OverlayGlyph(c)) {
                for (int row = 0; row < kOverlayGlyphHeight; row++) {
                    for (int column = 0; column < kOverlayGlyphWidth; column++) {
                        if (rows[row] & (0x10 >> column)) {
                            Rect(x + static_cast<float>(column) * scale, y + static_cast<float>(row) * scale, scale,
                                 scale, color);
                        }
                    }
                }
            }
            x += kAdvance * scale;
        }
    }

    void Centered(float y, float scale, std::string_view text, const Color &color) {
        Text((gfx::kLogicalWidth - TextWidth(text, scale)) * 0.5f, y, scale, text, color);
    }

    void Present() const {
        if (!gfx::BeginFrame()) {
            return;
        }
        gfx::Clear(true, kBackground.data(), true, 0.0f);
        if (!quads.empty()) {
            gfx::Draw2D(gfx::Primitive::Quads, quads, {}, gfx::DrawState{});
        }
        gfx::EndFrame();
    }
};

// ---- Input --------------------------------------------------------------------------------------

enum class Action {
    None,
    DiscImage,
    Folder,
    Quit,
};

struct Button {
    gfx::LogicalRect rect;
    Action           action;
    const char      *label;
    const char      *key;
};

constexpr Button kButtons[] = {
    {{80.0f, 236.0f, 480.0f, 44.0f}, Action::DiscImage, "DISC IMAGE (.ISO .BIN .IMG)",       "ENTER"},
    {{80.0f, 292.0f, 480.0f, 44.0f}, Action::Folder,    "FOLDER WITH DATA.DAT AND DATA.HD2", "F"    },
    {{80.0f, 348.0f, 480.0f, 44.0f}, Action::Quit,      "QUIT",                              "ESC"  },
};

struct Input {
    Action pressed = Action::None;
    bool   escape = false;
    int    hover = -1;
} input;

int ButtonAt(float window_x, float window_y) {
    SDL_Window         *window = WindowHandle();
    gfx::LogicalMapping mapping = gfx::GetLogicalMapping(gfx::kMainTarget);
    float               density = window ? SDL_GetWindowPixelDensity(window) : 1.0f;
    float               x = (window_x * density - mapping.offset_x) / mapping.scale_x;
    float               y = (window_y * density - mapping.offset_y) / mapping.scale_y;
    for (int i = 0; i < static_cast<int>(std::size(kButtons)); i++) {
        const gfx::LogicalRect &rect = kButtons[i].rect;
        if (x >= rect.x && x < rect.x + rect.w && y >= rect.y && y < rect.y + rect.h) {
            return i;
        }
    }
    return -1;
}

void OnEvent(const SDL_Event &event) {
    switch (event.type) {
        case SDL_EVENT_KEY_DOWN:
            switch (event.key.key) {
                case SDLK_RETURN:
                case SDLK_KP_ENTER:
                case SDLK_1:
                    input.pressed = Action::DiscImage;
                    break;
                case SDLK_F:
                case SDLK_2:
                    input.pressed = Action::Folder;
                    break;
                case SDLK_ESCAPE:
                    input.pressed = Action::Quit;
                    input.escape = true;
                    break;
                default:
                    break;
            }
            break;
        case SDL_EVENT_MOUSE_MOTION:
            input.hover = ButtonAt(event.motion.x, event.motion.y);
            break;
        case SDL_EVENT_MOUSE_BUTTON_UP:
            if (event.button.button == SDL_BUTTON_LEFT) {
                if (int at = ButtonAt(event.button.x, event.button.y); at >= 0) {
                    input.pressed = kButtons[at].action;
                }
            }
            break;
        default:
            break;
    }
}

// ---- The file selector --------------------------------------------------------------------------

// SDL calls back from its event pump with the portal and from a thread of its own with zenity.
struct Dialog {
    std::mutex              mutex;
    bool                    open = false;
    bool                    answered = false;
    std::optional<fs::path> path;
    std::string             error;
} dialog;

void SDLCALL OnDialog(void *, const char *const *files, int) {
    std::lock_guard lock(dialog.mutex);
    dialog.answered = true;
    if (files == nullptr) {
        dialog.error = SDL_GetError();
    } else if (files[0] != nullptr) {
        dialog.path = PathsFromUtf8(files[0]);
    }
}

void OpenDialog(FirstRunSource source) {
    static const SDL_DialogFileFilter kImages[] = {
        {"Disc images (.iso, .bin, .img)", "iso;ISO;bin;BIN;img;IMG"},
    };
    {
        std::lock_guard lock(dialog.mutex);
        dialog.open = true;
        dialog.answered = false;
        dialog.path.reset();
        dialog.error.clear();
    }
    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetPointerProperty(props, SDL_PROP_FILE_DIALOG_WINDOW_POINTER, WindowHandle());
    SDL_FileDialogType type;
    if (source == FirstRunSource::DiscImage) {
        type = SDL_FILEDIALOG_OPENFILE;
        SDL_SetPointerProperty(props, SDL_PROP_FILE_DIALOG_FILTERS_POINTER, const_cast<SDL_DialogFileFilter *>(kImages));
        SDL_SetNumberProperty(props, SDL_PROP_FILE_DIALOG_NFILTERS_NUMBER, std::size(kImages));
        SDL_SetStringProperty(props, SDL_PROP_FILE_DIALOG_TITLE_STRING,
                              "Choose your PAL disc image: the game data is extracted from it once");
    } else {
        type = SDL_FILEDIALOG_OPENFOLDER;
        SDL_SetStringProperty(props, SDL_PROP_FILE_DIALOG_TITLE_STRING,
                              "Choose the folder that holds DATA.DAT and DATA.HD2 from your PAL disc");
    }
    SDL_SetStringProperty(props, SDL_PROP_FILE_DIALOG_ACCEPT_STRING, "Extract");
    SDL_ShowFileDialogWithProperties(type, OnDialog, nullptr, props);
    SDL_DestroyProperties(props);
}

// ---- Screens ------------------------------------------------------------------------------------

std::string MegaBytes(std::uint64_t bytes) {
    return std::format("{:.1f}", static_cast<double>(bytes) / 1e6);
}

void DrawChooser(const fs::path &root, bool waiting) {
    Canvas canvas;
    canvas.Centered(64.0f, 4.0f, "GAME DATA NEEDED", kAccent);
    canvas.Centered(122.0f, 2.0f, "THIS PORT PLAYS FROM THE FILES OF YOUR OWN", kText);
    canvas.Centered(142.0f, 2.0f, "PAL DISC. CHOOSE IT AND THEY ARE COPIED TO", kText);
    canvas.Centered(176.0f, 1.0f, Fit(PathsDisplay(root), 1.0f, 600.0f), kDim);
    for (int i = 0; i < static_cast<int>(std::size(kButtons)); i++) {
        const Button &button = kButtons[i];
        canvas.Rect(button.rect.x, button.rect.y, button.rect.w, button.rect.h,
                    i == input.hover ? kButtonLit : kButton);
        canvas.Text(button.rect.x + 16.0f, button.rect.y + 15.0f, 2.0f, button.label, kText);
        canvas.Text(button.rect.x + button.rect.w - 16.0f - TextWidth(button.key, 1.0f), button.rect.y + 18.0f,
                    1.0f, button.key, kDim);
    }
    if (waiting) {
        canvas.Centered(424.0f, 2.0f, "WAITING FOR THE FILE SELECTOR...", kDim);
    }
    canvas.Present();
}

void DrawProgress(const fs::path &source, const dcdata::Progress &progress) {
    constexpr float kBarX = 60.0f;
    constexpr float kBarY = 228.0f;
    constexpr float kBarW = 520.0f;
    constexpr float kBarH = 28.0f;
    Canvas          canvas;
    canvas.Centered(96.0f, 4.0f, "EXTRACTING GAME DATA", kAccent);
    canvas.Centered(160.0f, 1.0f, Fit(PathsDisplay(source), 1.0f, 600.0f), kDim);
    float done = progress.total_bytes == 0
                     ? 0.0f
                     : static_cast<float>(static_cast<double>(progress.bytes) / static_cast<double>(progress.total_bytes));
    canvas.Rect(kBarX - 4.0f, kBarY - 4.0f, kBarW + 8.0f, kBarH + 8.0f, kTrack);
    canvas.Rect(kBarX, kBarY, kBarW * std::clamp(done, 0.0f, 1.0f), kBarH, kAccent);
    canvas.Centered(284.0f, 2.0f, std::format("{} / {} FILES", progress.files, progress.total_files), kText);
    canvas.Centered(310.0f, 2.0f,
                    std::format("{} / {} MB", MegaBytes(progress.bytes), MegaBytes(progress.total_bytes)), kText);
    canvas.Centered(424.0f, 1.0f, "CLOSE THE WINDOW TO STOP; THE NEXT START PICKS UP WHERE THIS ONE LEFT OFF", kDim);
    canvas.Present();
}

void SetTitle(const dcdata::Progress &progress) {
    static std::size_t shown = static_cast<std::size_t>(-1);
    if (progress.files != shown) {
        shown = progress.files;
        std::string title = std::format("Extracting game data: {}/{} files, {}/{} MB", progress.files,
                                        progress.total_files, MegaBytes(progress.bytes), MegaBytes(progress.total_bytes));
        SDL_SetWindowTitle(WindowHandle(), title.c_str());
    }
}

// Waits on the chooser screen for a source; none when the person quits or no selector works.
std::optional<fs::path> Choose(const fs::path &root) {
    auto ask = [&](FirstRunSource source) -> std::optional<std::optional<fs::path>> {
        if (chooser) {
            return chooser(source);
        }
        OpenDialog(source);
        return std::nullopt;
    };
    if (std::optional<std::optional<fs::path>> chosen = ask(FirstRunSource::DiscImage)) {
        return *chosen;
    }
    for (;;) {
        SDL_WaitEventTimeout(nullptr, kIdleMilliseconds);
        if (!WindowPollEvents()) {
            return std::nullopt;
        }
        bool waiting;
        {
            std::lock_guard lock(dialog.mutex);
            if (dialog.open && dialog.answered) {
                dialog.open = false;
                if (dialog.path) {
                    return dialog.path;
                }
                if (!dialog.error.empty()) {
                    std::fprintf(stderr, "no file selector: %s\n", dialog.error.c_str());
                    return std::nullopt;
                }
            }
            waiting = dialog.open;
        }
        Action action = std::exchange(input.pressed, Action::None);
        if (action == Action::Quit) {
            return std::nullopt;
        }
        if (!waiting && action != Action::None) {
            FirstRunSource source = action == Action::Folder ? FirstRunSource::Folder : FirstRunSource::DiscImage;
            if (std::optional<std::optional<fs::path>> chosen = ask(source)) {
                return *chosen;
            }
        }
        DrawChooser(root, waiting);
    }
}

fs::path Partial(const fs::path &root) {
    fs::path clean = root.has_filename() ? root : root.parent_path();
    fs::path partial = clean;
    partial += ".partial";
    return partial;
}

void PrintFlatpakHint(const fs::path &root) {
    if (const char *id = std::getenv("FLATPAK_ID"); id != nullptr && *id != '\0') {
        std::fprintf(stderr,
                     "inside the Flatpak, from a shell: flatpak run --filesystem=<folder of the image>:ro "
                     "--command=dcdata %s extract <disc image> %s\n",
                     id, PathsDisplay(root).c_str());
    }
}

} // namespace

bool FirstRunDataMissing(const fs::path &root) {
    std::error_code error;
    if (!fs::is_directory(root, error)) {
        return true;
    }
    auto options = fs::directory_options::follow_directory_symlink | fs::directory_options::skip_permission_denied;
    for (fs::recursive_directory_iterator it(root, options, error), end; !error && it != end; it.increment(error)) {
        std::error_code entry_error;
        if (it->is_regular_file(entry_error)) {
            return false;
        }
    }
    return true;
}

FirstRunOutcome FirstRunExtract(const fs::path &source, const fs::path &root,
                                const std::function<bool(const dcdata::Progress &)> &tick) {
    // A symlinked data directory is filled in place: replacing it would drop the link.
    std::error_code         error;
    bool                    in_place = fs::is_symlink(root.has_filename() ? root : root.parent_path(), error);
    fs::path                target = in_place ? root : Partial(root);
    std::mutex              mutex;
    std::condition_variable changed;
    dcdata::Progress        latest;
    bool                    fresh = false;
    bool                    finished = false;
    std::atomic<bool>       cancel = false;
    FirstRunOutcome         outcome;

    std::thread worker([&] {
        try {
            dcdata::Archive archive = dcdata::OpenArchive(source);
            dcdata::Extract(archive, target, nullptr, [&](const dcdata::Progress &progress) {
                {
                    std::lock_guard lock(mutex);
                    latest = progress;
                    fresh = true;
                }
                changed.notify_one();
                return !cancel.load();
            });
            std::vector<unsigned char>          hd2 = dcdata::ReadExtent(archive.hd2);
            std::vector<dcdata::Record>         records = dcdata::ParseIndex(hd2);
            std::vector<const dcdata::Record *> bad = dcdata::Mismatched(records, target);
            if (!bad.empty()) {
                dcdata::Fail("{} files are missing or the wrong size after extraction, {} among them",
                             bad.size(), bad.front()->path);
            }
            if (!in_place) {
                // The data check found root missing or without a file, so anything there is empty
                // directories.
                if (fs::exists(root)) {
                    if (!FirstRunDataMissing(root)) {
                        dcdata::Fail("{} gained files during the extraction; the extracted data is in {}",
                                     PathsDisplay(root), PathsDisplay(target));
                    }
                    fs::remove_all(root);
                }
                fs::rename(target, root);
            }
            outcome.ok = true;
        } catch (const std::exception &failure) {
            outcome.error = failure.what();
        }
        {
            std::lock_guard lock(mutex);
            finished = true;
        }
        changed.notify_one();
    });

    std::unique_lock lock(mutex);
    while (!finished) {
        changed.wait_for(lock, kIdle, [&] { return fresh || finished; });
        dcdata::Progress snapshot = latest;
        fresh = false;
        lock.unlock();
        if (!tick(snapshot)) {
            cancel = true;
        }
        lock.lock();
    }
    lock.unlock();
    worker.join();
    outcome.cancelled = !outcome.ok && cancel.load();
    return outcome;
}

void FirstRunSetChooser(FirstRunChooser replacement) {
    chooser = std::move(replacement);
}

void FirstRunIfNoData(bool headless) {
    const fs::path &root = PathsDataRoot();
    if (!FirstRunDataMissing(root) || (headless && !chooser)) {
        return;
    }
    // No display at all: leave it to main's data check, which says what to run instead.
    if (!headless && !SDL_InitSubSystem(SDL_INIT_VIDEO)) {
        std::fprintf(stderr, "no display for the first-run screen: %s\n", SDL_GetError());
        return;
    }

    ConfigLoad();
    const Config &config = ConfigGet();
    WindowConfig  window;
    window.width = config.window_width;
    window.height = config.window_height;
    window.fullscreen = config.fullscreen;
    window.headless = headless;
    bool offscreen = headless && !gfx::HeadlessSurfaceAvailable();
    window.vulkan = !offscreen;
    WindowInit(window);
    if (!headless) {
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
    }
    WindowAddEventHook(OnEvent);
    SDL_SetWindowTitle(WindowHandle(), "Game data needed");

    gfx::RendererConfig renderer;
    renderer.pipeline_cache = PathsSaveRoot() / "pipeline_cache.bin";
    renderer.offscreen = offscreen;
    gfx::RendererInit(WindowHandle(), renderer);

    FirstRunOutcome         outcome;
    std::optional<fs::path> source = Choose(root);
    if (source) {
        input.pressed = Action::None;
        outcome = FirstRunExtract(*source, root, [&](const dcdata::Progress &progress) {
            if (!WindowPollEvents() || std::exchange(input.escape, false)) {
                return false;
            }
            SetTitle(progress);
            DrawProgress(*source, progress);
            return true;
        });
    }

    if (source && !outcome.ok && !outcome.cancelled) {
        std::string message = std::format("Extracting {} into {} failed:\n\n{}", PathsDisplay(*source),
                                          PathsDisplay(root), outcome.error);
        std::fprintf(stderr, "%s\n", message.c_str());
        if (!headless) {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "The game data could not be extracted", message.c_str(),
                                     WindowHandle());
        }
        gfx::RendererShutdown();
        WindowShutdown();
        std::exit(kExitFailure);
    }
    gfx::RendererShutdown();
    WindowShutdown();
    if (outcome.ok) {
        std::fprintf(stderr, "extracted %s into %s\n", PathsDisplay(*source).c_str(), PathsDisplay(root).c_str());
    } else {
        if (outcome.cancelled) {
            std::fprintf(stderr, "extraction stopped; %s holds what was written so far\n",
                         PathsDisplay(Partial(root)).c_str());
        }
        PrintFlatpakHint(root);
    }
}
