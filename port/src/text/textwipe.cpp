#include "text/textwipe.hpp"

#include <SDL3/SDL.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <format>
#include <fstream>
#include <map>
#include <optional>
#include <print>
#include <string>
#include <string_view>
#include <vector>

#include "gfx/gfx.hpp"
#include "platform/config.hpp"
#include "platform/input.hpp"
#include "platform/overlay.hpp"
#include "platform/paths.hpp"

namespace dc::text::textwipe {

using namespace dc::platform;

namespace {

constexpr double kSweepSeconds = 4.0; // one way
constexpr double kRestSeconds = 0.8;  // at each end

bool                                  g_on = false;
Mode                                  g_mode = Mode::Sweep;
std::chrono::steady_clock::time_point g_started;
float                                 g_divider = 0.0f;

bool DevKeyHeld(std::string_view name) {
    const int   scancode = InputScancodeFromName(name);
    int         count = 0;
    const bool *keys = SDL_GetKeyboardState(&count);
    return keys != nullptr && scancode >= 0 && scancode < count && keys[scancode];
}

bool DevKeyPressed(std::string_view name) {
    static std::map<int, bool> held;
    const int                  scancode = InputScancodeFromName(name);
    if (scancode < 0) {
        return false;
    }
    bool      &before = held[scancode];
    const bool now = DevKeyHeld(name);
    const bool pressed = now && !before;
    before = now;
    return pressed;
}

std::optional<gfx::LogicalRect>     g_clip;
std::optional<std::array<float, 4>> g_tint;

void SetDrawClip(const gfx::LogicalRect *clip) {
    if (clip != nullptr) {
        g_clip = *clip;
    } else {
        g_clip.reset();
    }
}

void SetDrawTint(const std::array<float, 4> *tint) {
    if (tint != nullptr) {
        g_tint = *tint;
    } else {
        g_tint.reset();
    }
}

float Smooth(double t) {
    t = std::clamp(t, 0.0, 1.0);
    return static_cast<float>(t * t * (3.0 - 2.0 * t));
}

const char *ModeName() {
    switch (g_mode) {
        case Mode::Sweep:
            return "SWEEP";
        case Mode::ShowBefore:
            return "ALL BEFORE";
        case Mode::ShowAfter:
            return "ALL AFTER";
        case Mode::Overlay:
            return "OVERLAY";
    }
    return "";
}

std::array<Tuning, static_cast<size_t>(Kind::Count)> g_tuning;
bool                                                 g_tuning_loaded = false;
int                                                  g_kind = 0;
int                                                  g_value = 0;

constexpr std::array<const char *, 4> kKindNames = {"message", "caption", "card", "options"};
constexpr std::array<const char *, 4> kKindLabels = {"MESSAGES", "MENU CAPTIONS", "AREA CARDS", "OPTIONS SCREEN"};

Kind g_context = Kind::Message;

bool Enabled() {
    return ConfigGet().text_wipe;
}

struct Value {
    const char *key;
    const char *label;
    float Tuning::*field;
    float          step;
    float          low;
    float          high;
};

constexpr auto kValues = std::to_array<Value>({
    {.key = "size",    .label = "SIZE",                 .field = &Tuning::size,    .step = 0.01f, .low = 0.2f,   .high = 3.0f },
    {.key = "xscale",  .label = "WIDTH",                .field = &Tuning::xscale,  .step = 0.01f, .low = 0.2f,   .high = 3.0f },
    {.key = "yscale",  .label = "HEIGHT",               .field = &Tuning::yscale,  .step = 0.01f, .low = 0.2f,   .high = 3.0f },
    {.key = "letter",  .label = "LETTER SPACING",       .field = &Tuning::letter,  .step = 0.01f, .low = -10.0f, .high = 10.0f},
    {.key = "word",    .label = "WORD SPACING",         .field = &Tuning::word,    .step = 0.01f, .low = -20.0f, .high = 40.0f},
    {.key = "dx",      .label = "MOVE RIGHT",           .field = &Tuning::dx,      .step = 0.01f, .low = -40.0f, .high = 40.0f},
    {.key = "dy",      .label = "MOVE DOWN",            .field = &Tuning::dy,      .step = 0.01f, .low = -40.0f, .high = 40.0f},
    {.key = "weight",  .label = "WEIGHT (BOLDER)",      .field = &Tuning::weight,  .step = 0.01f, .low = 0.0f,   .high = 3.0f },
    {.key = "opacity", .label = "OPACITY",              .field = &Tuning::opacity, .step = 0.01f, .low = 0.0f,   .high = 1.0f },
    {.key = "shadow",  .label = "SHADOW",               .field = &Tuning::shadow,  .step = 0.01f, .low = 0.0f,   .high = 3.0f },
    {.key = "align",   .label = "ALIGN IN CELLS",       .field = &Tuning::align,   .step = 0.01f, .low = 0.0f,   .high = 1.0f },
    {.key = "lock",    .label = "CELL LOCK (NO DRIFT)", .field = &Tuning::lock,    .step = 0.01f, .low = 0.0f,   .high = 1.0f },
    {.key = "fit",     .label = "FIT TO CELLS",         .field = &Tuning::fit,     .step = 1.0f,  .low = 0.0f,   .high = 1.0f },
});
constexpr int  kValueCount = static_cast<int>(kValues.size());

std::filesystem::path TuningFile() {
    return PathsSaveRoot() / "lang" / "textwipe_tuning.json";
}

void SaveTuning() {
    nlohmann::json json = nlohmann::json::object();
    for (size_t k = 0; k < g_tuning.size(); k++) {
        const Tuning locked = Locked(static_cast<Kind>(k));
        for (const Value &value : kValues) {
            const float now = std::round(g_tuning[k].*value.field * 1000.0f) / 1000.0f;
            if (std::fabs(now - locked.*value.field) > 0.0005f) {
                json[kKindNames[k]][value.key] = now;
            }
        }
    }
    std::error_code error;
    std::filesystem::create_directories(TuningFile().parent_path(), error);
    std::ofstream out(TuningFile());
    if (out) {
        out << json.dump(2) << "\n";
    }
}

void LoadTuning() {
    g_tuning_loaded = true;
    for (size_t k = 0; k < g_tuning.size(); k++) {
        g_tuning[k] = Locked(static_cast<Kind>(k));
    }
    std::ifstream in(TuningFile());
    if (!in) {
        return;
    }
    try {
        const nlohmann::json json = nlohmann::json::parse(in);
        for (size_t k = 0; k < g_tuning.size(); k++) {
            if (!json.contains(kKindNames[k])) {
                continue;
            }
            const nlohmann::json &entry = json[kKindNames[k]];
            for (const Value &value : kValues) {
                if (entry.contains(value.key) && entry[value.key].is_number()) {
                    g_tuning[k].*value.field = std::clamp(entry[value.key].get<float>(), value.low, value.high);
                }
            }
        }
    } catch (const std::exception &error) {
        std::println(stderr, "textwipe: cannot read {}: {}", TuningFile().string(), error.what());
    }
}

struct Repeater {
    bool                                  down = false;
    int                                   fired = 0;
    std::chrono::steady_clock::time_point next;

    int Fire(bool held) {
        const auto now = std::chrono::steady_clock::now();
        if (!held) {
            down = false;
            fired = 0;
            return 0;
        }
        if (!down) {
            down = true;
            fired = 1;
            next = now + std::chrono::milliseconds(400);
            return 1;
        }
        if (now >= next) {
            next = now + std::chrono::milliseconds(50);
            fired++;
            if (fired > 40) {
                return 10;
            }
            return fired > 15 ? 3 : 1;
        }
        return 0;
    }
};

Repeater g_minus;
Repeater g_plus;

void Adjust(int direction, int steps) {
    const bool   shift = DevKeyHeld("Left Shift") || DevKeyHeld("Right Shift");
    const bool   ctrl = DevKeyHeld("Left Ctrl") || DevKeyHeld("Right Ctrl");
    const Value &value = kValues[static_cast<size_t>(g_value)];
    float       &field = g_tuning[static_cast<size_t>(g_kind)].*value.field;
    if (value.field == &Tuning::fit) {
        field = direction > 0 ? 1.0f : 0.0f;
    } else {
        float scale = 1.0f;
        if (shift) {
            scale = 10.0f;
        } else if (ctrl) {
            scale = 0.1f;
        }
        const float step = value.step * static_cast<float>(steps) * scale;
        const float grid = value.step * 0.1f;
        field = std::clamp(std::round((field + static_cast<float>(direction) * step) / grid) * grid, value.low, value.high);
    }
    SaveTuning();
}

} // namespace

float SweepPosition(double seconds) {
    const double cycle = 2.0 * (kSweepSeconds + kRestSeconds);
    double       t = std::fmod(std::max(0.0, seconds), cycle);
    if (t < kRestSeconds) {
        return gfx::LogicalWidth();
    }
    t -= kRestSeconds;
    if (t < kSweepSeconds) {
        return gfx::LogicalWidth() * (1.0f - Smooth(t / kSweepSeconds));
    }
    t -= kSweepSeconds;
    if (t < kRestSeconds) {
        return 0.0f;
    }
    t -= kRestSeconds;
    return gfx::LogicalWidth() * Smooth(t / kSweepSeconds);
}

void Update() {
    if (!Enabled()) {
        g_on = false;
        return;
    }
    if (DevKeyPressed("F9")) {
        g_on = !g_on;
        g_started = std::chrono::steady_clock::now();
        g_mode = Mode::Sweep;
        std::println(stderr, "textwipe: {}", g_on ? "on (F10 steps the mode)" : "off");
    }
    if (DevKeyPressed("F10") && g_on) {
        switch (g_mode) {
            case Mode::Sweep:
                g_mode = Mode::ShowBefore;
                break;
            case Mode::ShowBefore:
                g_mode = Mode::ShowAfter;
                break;
            case Mode::ShowAfter:
                g_mode = Mode::Overlay;
                break;
            case Mode::Overlay:
                g_mode = Mode::Sweep;
                break;
        }
        g_started = std::chrono::steady_clock::now();
        std::println(stderr, "textwipe: mode {}", ModeName());
    }
    if (!g_on) {
        return;
    }
    if (DevKeyPressed("F11")) {
        g_kind = (g_kind + 1) % static_cast<int>(Kind::Count);
    }
    if (DevKeyPressed("F12")) {
        const bool back = DevKeyHeld("Left Shift") || DevKeyHeld("Right Shift");
        g_value = (g_value + (back ? kValueCount - 1 : 1)) % kValueCount;
    }
    if (DevKeyPressed("F8")) {
        g_tuning[static_cast<size_t>(g_kind)] = Locked(static_cast<Kind>(g_kind));
        SaveTuning();
    }
    if (const int steps = g_minus.Fire(DevKeyHeld("-")); steps > 0) {
        Adjust(-1, steps);
    }
    if (const int steps = g_plus.Fire(DevKeyHeld("=")); steps > 0) {
        Adjust(1, steps);
    }
    switch (g_mode) {
        case Mode::Sweep:
            g_divider = SweepPosition(std::chrono::duration<double>(std::chrono::steady_clock::now() - g_started).count());
            break;
        case Mode::ShowBefore:
            g_divider = gfx::LogicalWidth();
            break;
        case Mode::ShowAfter:
            g_divider = 0.0f;
            break;
        case Mode::Overlay:
            g_divider = gfx::LogicalWidth();
            break;
    }
}

Tuning Locked(Kind kind) {
    Tuning tuning;
    switch (kind) {
        case Kind::Message:
            tuning.size = 1.15f;
            tuning.xscale = 0.80f;
            tuning.yscale = 1.02f;
            tuning.dx = 0.35f;
            tuning.align = 1.0f;
            tuning.lock = 1.0f;
            break;
        case Kind::Options:
            tuning.size = 0.76f;
            tuning.yscale = 1.17f;
            tuning.dx = -1.10f;
            tuning.dy = -0.15f;
            tuning.lock = 1.0f;
            break;
        case Kind::Card:
        case Kind::Caption:
        case Kind::Count:
            break;
    }
    return tuning;
}

Kind MessageKind() {
    return g_context;
}

MessageContext::MessageContext(Kind kind) : previous(g_context) {
    g_context = kind;
}

MessageContext::~MessageContext() {
    g_context = previous;
}

const Tuning &Get(Kind kind) {
    if (!Enabled()) {
        static const std::array<Tuning, static_cast<size_t>(Kind::Count)> locked = [] {
            std::array<Tuning, static_cast<size_t>(Kind::Count)> all;
            for (size_t k = 0; k < all.size(); k++) {
                all[k] = Locked(static_cast<Kind>(k));
            }
            return all;
        }();
        return locked[static_cast<size_t>(kind)];
    }
    if (!g_tuning_loaded) {
        LoadTuning();
    }
    return g_tuning[static_cast<size_t>(kind)];
}

bool Overlaying() {
    return g_on && g_mode == Mode::Overlay;
}

bool Active() {
    return g_on;
}

float Divider() {
    return g_divider;
}

void Both(const std::function<void(bool runtime)> &draw) {
    if (!g_on) {
        draw(true);
        return;
    }
    if (g_mode == Mode::Overlay) {
        static constexpr std::array<float, 4> old_tint = {1.0f, 0.25f, 0.25f, 0.75f};
        static constexpr std::array<float, 4> new_tint = {0.3f, 1.0f, 1.0f, 0.75f};
        SetDrawTint(&old_tint);
        draw(false);
        SetDrawTint(&new_tint);
        draw(true);
        SetDrawTint(nullptr);
        return;
    }
    const float edge = std::clamp(g_divider, 0.0f, gfx::LogicalWidth());
    // Scissors touching the logical frame boundary extend through widescreen margins.
    if (edge > 0.0f) {
        const gfx::LogicalRect left = {.x = 0.0f, .y = 0.0f, .w = edge, .h = gfx::LogicalHeight()};
        SetDrawClip(&left);
        draw(false);
    }
    if (edge < gfx::LogicalWidth()) {
        const gfx::LogicalRect right = {
            .x = edge, .y = 0.0f, .w = gfx::LogicalWidth() - edge, .h = gfx::LogicalHeight()};
        SetDrawClip(&right);
        draw(true);
    }
    SetDrawClip(nullptr);
}

void Draw2D(gfx::Primitive primitive, std::span<const gfx::Vertex2D> vertices, const gfx::TextureBinding &binding,
            const gfx::DrawState &state) {
    if (g_tint) {
        std::vector<gfx::Vertex2D> tinted(vertices.begin(), vertices.end());
        for (gfx::Vertex2D &vertex : tinted) {
            for (size_t channel = 0; channel < g_tint->size(); channel++) {
                vertex.color[channel] =
                    static_cast<uint8_t>(std::clamp(vertex.color[channel] * (*g_tint)[channel] + 0.5f, 0.0f, 255.0f));
            }
        }
        gfx::Draw2D(primitive, tinted, binding, state);
        return;
    }
    if (g_clip) {
        gfx::DrawState         clipped = state;
        const gfx::LogicalRect own = state.scissor ? state.scissor_rect : gfx::LogicalFrame();
        const float            left = std::max(own.x, g_clip->x);
        const float            top = std::max(own.y, g_clip->y);
        const float            right = std::min(own.x + own.w, g_clip->x + g_clip->w);
        const float            bottom = std::min(own.y + own.h, g_clip->y + g_clip->h);
        if (right <= left || bottom <= top) {
            return;
        }
        clipped.scissor = true;
        clipped.scissor_rect = {.x = left, .y = top, .w = right - left, .h = bottom - top};
        gfx::Draw2D(primitive, vertices, binding, clipped);
        return;
    }
    gfx::Draw2D(primitive, vertices, binding, state);
}

void DrawDivider() {
    if (!g_on || !(gfx::Recording() || gfx::InFrame())) {
        return;
    }
    const gfx::LogicalMapping mapping = gfx::GetUiMapping(gfx::kMainTarget);
    const int                 pixel = OverlayPixelSize(gfx::GetLogicalMapping(gfx::kMainTarget));
    const float               x = std::clamp(g_divider, 0.0f, gfx::LogicalWidth());
    const float               half = 1.0f / std::max(0.5f, mapping.scale_x) * static_cast<float>(pixel);
    auto                      vertex = [](float vx, float vy, unsigned char grey) {
        return gfx::Vertex2D{
            .x = vx, .y = vy, .z = 0.0f, .u = 0.0f, .v = 0.0f, .color = {grey, grey, grey, 0x80}
        };
    };
    gfx::DrawState state;
    state.blend = true;
    std::vector<gfx::Vertex2D> line;
    for (const auto &[width, grey] : {
             std::pair{2.5f, static_cast<unsigned char>(0)   },
             std::pair{1.0f, static_cast<unsigned char>(0x7F)}
    }) {
        const float half_width = half * width;
        line.push_back(vertex(x - half_width, 0.0f, grey));
        line.push_back(vertex(x + half_width, 0.0f, grey));
        line.push_back(vertex(x + half_width, gfx::LogicalHeight(), grey));
        line.push_back(vertex(x - half_width, gfx::LogicalHeight(), grey));
    }
    if (g_mode != Mode::Overlay) {
        gfx::Draw2D(gfx::Primitive::Quads, line, {}, state);
    }

    const bool        overlay = g_mode == Mode::Overlay;
    const std::string before = overlay ? "RED: BAKED PICTURE" : "BEFORE: BAKED PICTURE";
    const std::string after = overlay ? std::string("CYAN: RUNTIME TEXT") : std::format("AFTER: RUNTIME TEXT ({})", ModeName());
    const int         y = mapping.pixel_height - (kOverlayGlyphHeight + 4 * kOverlayPadding) * pixel;
    const int         gap = 6 * pixel;
    const int         line_px = static_cast<int>(std::lround(x * mapping.scale_x + mapping.offset_x));
    const int         before_width = static_cast<int>(before.size()) * kOverlayAdvance * pixel;
    OverlayDrawText(before, mapping, pixel, std::max(gap, line_px - gap - before_width), y);
    OverlayDrawText(after, mapping, pixel, std::min(line_px + gap, static_cast<int>(mapping.pixel_width) - gap - static_cast<int>(after.size()) * kOverlayAdvance * pixel), y);

    const Tuning &tuning = Get(static_cast<Kind>(g_kind));
    int           row_y = 16 * pixel;
    const int     row_step = (kOverlayGlyphHeight + 2 * kOverlayPadding + 1) * pixel;
    auto          line_of = [&](const std::string &text) {
        OverlayDrawText(text, mapping, pixel, 2 * pixel, row_y);
        row_y += row_step;
    };
    line_of(std::format("TUNING {}  F11 SET  F12 VALUE  -/= CHANGE  F8 RESET", kKindLabels[static_cast<size_t>(g_kind)]));
    for (int index = 0; index < kValueCount; index++) {
        const Value      &value = kValues[static_cast<size_t>(index)];
        const std::string number = value.field == &Tuning::fit ? std::string(tuning.fit >= 0.5f ? "ON" : "OFF")
                                                               : std::format("{:.3f}", tuning.*value.field);
        line_of(std::format("{}{} {}", index == g_value ? "> " : "  ", value.label, number));
    }
}

} // namespace dc::text::textwipe
