#include "firstrun.hpp"

#include <SDL3/SDL.h>
#ifdef __ANDROID__
#include <SDL3/SDL_system.h>
#include <android/log.h>
#include <jni.h>
#endif

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
#include <limits>
#include <mutex>
#include <print>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#include "app/exitcodes.hpp"
#include "config.hpp"
#include "gfx/gfx.hpp"
#include "platform/data_archive.hpp"
#include "platform/overlay.hpp"
#include "platform/paths.hpp"
#include "platform/settings.hpp"
#include "platform/window.hpp"
#include "render/screen.hpp"

namespace dc::platform {

using namespace dc::app;
using namespace dc::render;

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

#ifdef __ANDROID__
std::mutex              android_picker_mutex;
std::condition_variable android_picker_changed;
bool                    android_picker_answered = false;
bool                    android_first_run_cancelled = false;
std::optional<fs::path> android_picker_path;

class AndroidActivityCall {
public:
    AndroidActivityCall(const char *method_name, const char *signature)
        : env_(static_cast<JNIEnv *>(SDL_GetAndroidJNIEnv())),
          activity_(static_cast<jobject>(SDL_GetAndroidActivity())) {
        if (env_ == nullptr || activity_ == nullptr) {
            return;
        }
        ClearException();
        activity_class_ = env_->GetObjectClass(activity_);
        ClearException();
        if (activity_class_ != nullptr) {
            method_ = env_->GetMethodID(activity_class_, method_name, signature);
        }
        ClearException();
    }

    ~AndroidActivityCall() {
        ClearException();
        if (env_ != nullptr && activity_class_ != nullptr) {
            env_->DeleteLocalRef(activity_class_);
        }
        if (env_ != nullptr && activity_ != nullptr) {
            env_->DeleteLocalRef(activity_);
        }
    }

    explicit operator bool() const { return env_ != nullptr && activity_ != nullptr && method_ != nullptr; }

    void Call() {
        if (*this) {
            env_->CallVoidMethod(activity_, method_);
            ClearException();
        }
    }

    void Call(jstring argument) {
        if (*this) {
            env_->CallVoidMethod(activity_, method_, argument);
            ClearException();
        }
    }

private:
    void ClearException() const {
        if (env_ != nullptr && env_->ExceptionCheck()) {
            env_->ExceptionClear();
        }
    }

    JNIEnv   *env_ = nullptr;
    jobject   activity_ = nullptr;
    jclass    activity_class_ = nullptr;
    jmethodID method_ = nullptr;
};

void AndroidShowStatus(std::string_view message) {
    JNIEnv *env = static_cast<JNIEnv *>(SDL_GetAndroidJNIEnv());
    if (env == nullptr) {
        return;
    }
    jstring             text = env->NewStringUTF(std::string(message).c_str());
    AndroidActivityCall call("showImportStatus", "(Ljava/lang/String;)V");
    call.Call(text);
    if (text != nullptr) {
        env->DeleteLocalRef(text);
    }
}

void AndroidShowError(std::string_view message) {
    JNIEnv *env = static_cast<JNIEnv *>(SDL_GetAndroidJNIEnv());
    if (env == nullptr) {
        return;
    }
    jstring             text = env->NewStringUTF(std::string(message).c_str());
    AndroidActivityCall call("showImportError", "(Ljava/lang/String;)V");
    call.Call(text);
    if (text != nullptr) {
        env->DeleteLocalRef(text);
    }
}

void AndroidHideStatus() {
    AndroidActivityCall call("hideImportStatus", "()V");
    call.Call();
}

std::optional<fs::path> ChooseAndroidDiscImage() {
    {
        AndroidActivityCall call("requestDiscImage", "()V");
        if (!call) {
            std::fprintf(stderr, "Android file picker is unavailable\n");
            return std::nullopt;
        }
        {
            std::scoped_lock lock(android_picker_mutex);
            android_picker_answered = false;
            android_picker_path.reset();
        }
        call.Call();
    }
    std::unique_lock lock(android_picker_mutex);
    android_picker_changed.wait(lock, [] { return android_picker_answered || android_first_run_cancelled; });
    if (android_first_run_cancelled) {
        return std::nullopt;
    }
    std::optional<fs::path> selected = std::move(android_picker_path);
    android_picker_path.reset();
    android_picker_answered = false;
    return selected;
}

bool AndroidFirstRunCancelled() {
    std::scoped_lock lock(android_picker_mutex);
    return android_first_run_cancelled;
}

std::optional<fs::path> WaitForAndroidRetry() {
    std::unique_lock lock(android_picker_mutex);
    android_picker_changed.wait(lock, [] { return android_picker_answered || android_first_run_cancelled; });
    if (android_first_run_cancelled) {
        return std::nullopt;
    }
    std::optional<fs::path> selected = std::move(android_picker_path);
    android_picker_path.reset();
    android_picker_answered = false;
    return selected;
}

#endif

constexpr float kAdvance = kOverlayAdvance;

float TextWidth(std::string_view text, float scale) {
    return text.empty() ? 0.0f : (static_cast<float>(text.size()) * kAdvance - 1.0f) * scale;
}

// Keeps the end of a path, which names the file, when the whole does not fit.
std::string Fit(std::string_view text, float scale, float width) {
    auto room = static_cast<std::size_t>((width / scale + 1.0f) / kAdvance);
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
        Text((kScreenWidth - TextWidth(text, scale)) * 0.5f, y, scale, text, color);
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

constexpr std::array kButtons = {
    Button{{80.0f, 236.0f, 480.0f, 44.0f}, Action::DiscImage, "DISC IMAGE (.ISO .BIN .IMG)",       "ENTER"},
    Button{{80.0f, 292.0f, 480.0f, 44.0f}, Action::Folder,    "FOLDER WITH DATA.DAT AND DATA.HD2", "F"    },
    Button{{80.0f, 348.0f, 480.0f, 44.0f}, Action::Quit,      "QUIT",                              ""     },
};

struct Input {
    Action pressed = Action::None;
    int    hover = -1;
} input;

int ButtonAt(float window_x, float window_y) {
    SDL_Window         *window = WindowHandle();
    gfx::LogicalMapping mapping = gfx::GetLogicalMapping(gfx::kMainTarget);
    float               density = window != nullptr ? SDL_GetWindowPixelDensity(window) : 1.0f;
    float               x = (window_x * density - mapping.offset_x) / mapping.scale_x;
    float               y = (window_y * density - mapping.offset_y) / mapping.scale_y;
    for (std::size_t i = 0; i < kButtons.size(); i++) {
        const gfx::LogicalRect &rect = kButtons[i].rect;
        if (x >= rect.x && x < rect.x + rect.w && y >= rect.y && y < rect.y + rect.h) {
            return static_cast<int>(i);
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

// SDL calls back from its event pump with the portal and from a thread of its own with zenity.
struct Dialog {
    std::mutex              mutex;
    bool                    open = false;
    bool                    answered = false;
    std::optional<fs::path> path;
    std::string             error;
} dialog;

void SDLCALL OnDialog(void * /*userdata*/, const char *const *files, int /*filter*/) {
    std::scoped_lock lock(dialog.mutex);
    dialog.answered = true;
    if (files == nullptr) {
        dialog.error = SDL_GetError();
    } else if (files[0] != nullptr) {
        dialog.path = PathsFromUtf8(files[0]);
    }
}

void OpenDialog(FirstRunSource source) {
    static const std::array kImages = {
        SDL_DialogFileFilter{"Disc images (.iso, .bin, .img)", "iso;ISO;bin;BIN;img;IMG"},
    };
    {
        std::scoped_lock lock(dialog.mutex);
        dialog.open = true;
        dialog.answered = false;
        dialog.path.reset();
        dialog.error.clear();
    }
    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetPointerProperty(props, SDL_PROP_FILE_DIALOG_WINDOW_POINTER, WindowHandle());
    SDL_FileDialogType type = SDL_FILEDIALOG_OPENFILE;
    if (source == FirstRunSource::DiscImage) {
        SDL_SetPointerProperty(props, SDL_PROP_FILE_DIALOG_FILTERS_POINTER, const_cast<SDL_DialogFileFilter *>(kImages.data()));
        SDL_SetNumberProperty(props, SDL_PROP_FILE_DIALOG_NFILTERS_NUMBER, kImages.size());
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

std::string MegaBytes(std::uint64_t bytes) {
    return std::format("{:.1f}", static_cast<double>(bytes) / 1e6);
}

void DrawChooser(const fs::path &root, bool waiting) {
    Canvas canvas;
    canvas.Centered(64.0f, 4.0f, "GAME DATA NEEDED", kAccent);
    canvas.Centered(122.0f, 2.0f, "THIS PORT PLAYS FROM THE FILES OF YOUR OWN", kText);
    canvas.Centered(142.0f, 2.0f, "PAL DISC. CHOOSE IT AND THEY ARE COPIED TO", kText);
    canvas.Centered(176.0f, 1.0f, Fit(PathsDisplay(root), 1.0f, 600.0f), kDim);
    for (std::size_t i = 0; i < kButtons.size(); i++) {
        const Button &button = kButtons[i];
        canvas.Rect(button.rect.x, button.rect.y, button.rect.w, button.rect.h,
                    std::cmp_equal(i, input.hover) ? kButtonLit : kButton);
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
    constexpr float bar_x = 60.0f;
    constexpr float bar_y = 228.0f;
    constexpr float bar_w = 520.0f;
    constexpr float bar_h = 28.0f;
    Canvas          canvas;
    canvas.Centered(96.0f, 4.0f, "EXTRACTING GAME DATA", kAccent);
    canvas.Centered(160.0f, 1.0f, Fit(PathsDisplay(source), 1.0f, 600.0f), kDim);
    float done = progress.total_bytes == 0
                     ? 0.0f
                     : static_cast<float>(static_cast<double>(progress.bytes) / static_cast<double>(progress.total_bytes));
    canvas.Rect(bar_x - 4.0f, bar_y - 4.0f, bar_w + 8.0f, bar_h + 8.0f, kTrack);
    canvas.Rect(bar_x, bar_y, bar_w * std::clamp(done, 0.0f, 1.0f), bar_h, kAccent);
    canvas.Centered(284.0f, 2.0f, std::format("{} / {} FILES", progress.files, progress.total_files), kText);
    canvas.Centered(310.0f, 2.0f,
                    std::format("{} / {} MB", MegaBytes(progress.bytes), MegaBytes(progress.total_bytes)), kText);
    canvas.Centered(424.0f, 1.0f, "CLOSE THE WINDOW TO STOP; THE NEXT START PICKS UP WHERE THIS ONE LEFT OFF", kDim);
    canvas.Present();
}

void SetTitle(const dcdata::Progress &progress) {
    static std::size_t shown = std::numeric_limits<std::size_t>::max();
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
        bool waiting = false;
        {
            std::scoped_lock lock(dialog.mutex);
            if (dialog.open && dialog.answered) {
                dialog.open = false;
                if (dialog.path) {
                    return dialog.path;
                }
                if (!dialog.error.empty()) {
                    std::println(stderr, "no file selector: {}", dialog.error);
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
        std::println(stderr,
                     "inside the Flatpak, from a shell: flatpak run --filesystem=<folder of the image>:ro "
                     "--command=dcdata {} extract <disc image> {}",
                     id, PathsDisplay(root));
    }
}

} // namespace

#ifdef __ANDROID__
extern "C" JNIEXPORT void JNICALL
Java_org_themoonpeople_chronicle_ChronicleActivity_nativeDiscImageResult(JNIEnv *env, jobject, jstring path) {
    __android_log_write(ANDROID_LOG_INFO, "Chronicle", "native document result received");
    std::optional<fs::path> chosen;
    if (path != nullptr) {
        const char *utf8 = env->GetStringUTFChars(path, nullptr);
        if (utf8 != nullptr) {
            chosen = PathsFromUtf8(utf8);
            env->ReleaseStringUTFChars(path, utf8);
        }
    }
    {
        std::scoped_lock lock(android_picker_mutex);
        android_picker_path = std::move(chosen);
        android_picker_answered = true;
    }
    android_picker_changed.notify_all();
}

extern "C" JNIEXPORT void JNICALL
Java_org_themoonpeople_chronicle_ChronicleActivity_nativeCancelFirstRun(JNIEnv *, jobject) {
    {
        std::scoped_lock lock(android_picker_mutex);
        android_first_run_cancelled = true;
        android_picker_path.reset();
        android_picker_answered = false;
    }
    android_picker_changed.notify_all();
}
#endif

bool FirstRunDataMissing(const fs::path &root) {
    if (DataArchivePresent(root)) {
        return false;
    }
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
                    std::scoped_lock lock(mutex);
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
            std::scoped_lock lock(mutex);
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
#ifdef __ANDROID__
    // SDL's desktop file dialog is not supported by Android. Ask through SAF,
    // copy the selected document into app cache, and run the same extractor.
    if (!headless) {
        std::optional<fs::path> retry_source;
        while (FirstRunDataMissing(root) && !AndroidFirstRunCancelled()) {
            std::optional<fs::path> source = retry_source ? std::exchange(retry_source, std::nullopt)
                                                          : ChooseAndroidDiscImage();
            if (!source) {
                AndroidHideStatus();
                AndroidShowError("No disc image was selected. Choose your Dark Cloud image to continue.");
                retry_source = WaitForAndroidRetry();
                continue;
            }
            AndroidShowStatus("Extracting game data…");
            __android_log_write(ANDROID_LOG_INFO, "Chronicle", "Android extraction started");
            FirstRunOutcome outcome = FirstRunExtract(*source, root, [](const dcdata::Progress &progress) {
                static std::size_t last_files = std::numeric_limits<std::size_t>::max();
                static auto        last_update = std::chrono::steady_clock::time_point{};
                const auto         now = std::chrono::steady_clock::now();
                if (progress.files != last_files && now - last_update >= std::chrono::milliseconds(500)) {
                    last_files = progress.files;
                    last_update = now;
                    AndroidShowStatus(std::format("Extracting game data… {}/{} files", progress.files,
                                                  progress.total_files));
                }
                return true;
            });
            if (!outcome.ok && !outcome.cancelled) {
                __android_log_print(ANDROID_LOG_ERROR, "Chronicle", "Android extraction failed: %s", outcome.error.c_str());
                std::fprintf(stderr, "extracting %s into %s failed: %s\n", PathsDisplay(*source).c_str(),
                             PathsDisplay(root).c_str(), outcome.error.c_str());
                AndroidHideStatus();
                AndroidShowError(outcome.error);
                retry_source = WaitForAndroidRetry();
            } else {
                AndroidHideStatus();
                std::error_code cleanup_error;
                fs::remove(*source, cleanup_error);
                __android_log_print(ANDROID_LOG_INFO, "Chronicle", "Android extraction completed; cached ISO cleanup %s",
                                    cleanup_error ? cleanup_error.message().c_str() : "complete");
            }
        }
        return;
    }
#endif
    // No display at all: leave it to main's data check, which says what to run instead.
    if (!headless && !SDL_InitSubSystem(SDL_INIT_VIDEO)) {
        std::println(stderr, "no display for the first-run screen: {}", SDL_GetError());
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
    renderer.app_name = "Dark Cloud";
    renderer.logical_width = kScreenWidth;
    renderer.logical_height = kScreenHeight;
    renderer.display_aspect = 4.0f / 3.0f;
    renderer.pipeline_cache = PathsSaveRoot() / "pipeline_cache.bin";
    renderer.offscreen = offscreen;
    renderer.validation = renderer.validation || SettingsEnv("VULKAN_VALIDATION") != nullptr;
    gfx::RendererInit(WindowHandle(), renderer);

    FirstRunOutcome         outcome;
    std::optional<fs::path> source = Choose(root);
    if (source) {
        input.pressed = Action::None;
        outcome = FirstRunExtract(*source, root, [&](const dcdata::Progress &progress) {
            if (!WindowPollEvents()) {
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
        std::println(stderr, "{}", message);
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
    if (outcome.ok && source) {
        std::println(stderr, "extracted {} into {}", PathsDisplay(*source), PathsDisplay(root));
    } else {
        if (outcome.cancelled) {
            std::println(stderr, "extraction stopped; {} holds what was written so far", PathsDisplay(Partial(root)));
        }
        PrintFlatpakHint(root);
    }
}

} // namespace dc::platform
