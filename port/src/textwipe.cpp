#include "textwipe.hpp"

#include <SDL3/SDL.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "gfx/gfx.hpp"
#include "platform/config.hpp"
#include "platform/input.hpp"
#include "platform/overlay.hpp"
#include "platform/paths.hpp"

namespace textwipe {

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

void SetDrawTint(const float (*tint)[4]) {
    if (tint != nullptr) {
        g_tint = std::array<float, 4>{(*tint)[0], (*tint)[1], (*tint)[2], (*tint)[3]};
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

constexpr const char *kKindNames[] = {"message", "caption", "card", "options"};
constexpr const char *kKindLabels[] = {"MESSAGES", "MENU CAPTIONS", "AREA CARDS", "OPTIONS SCREEN"};

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

constexpr Value kValues[] = {
    {"size",    "SIZE",                 &Tuning::size,    0.01f, 0.2f,   3.0f },
    {"xscale",  "WIDTH",                &Tuning::xscale,  0.01f, 0.2f,   3.0f },
    {"yscale",  "HEIGHT",               &Tuning::yscale,  0.01f, 0.2f,   3.0f },
    {"letter",  "LETTER SPACING",       &Tuning::letter,  0.01f, -10.0f, 10.0f},
    {"word",    "WORD SPACING",         &Tuning::word,    0.01f, -20.0f, 40.0f},
    {"dx",      "MOVE RIGHT",           &Tuning::dx,      0.01f, -40.0f, 40.0f},
    {"dy",      "MOVE DOWN",            &Tuning::dy,      0.01f, -40.0f, 40.0f},
    {"weight",  "WEIGHT (BOLDER)",      &Tuning::weight,  0.01f, 0.0f,   3.0f },
    {"opacity", "OPACITY",              &Tuning::opacity, 0.01f, 0.0f,   1.0f },
    {"shadow",  "SHADOW",               &Tuning::shadow,  0.01f, 0.0f,   3.0f },
    {"align",   "ALIGN IN CELLS",       &Tuning::align,   0.01f, 0.0f,   1.0f },
    {"lock",    "CELL LOCK (NO DRIFT)", &Tuning::lock,    0.01f, 0.0f,   1.0f },
    {"fit",     "FIT TO CELLS",         &Tuning::fit,     1.0f,  0.0f,   1.0f },
};
constexpr int kValueCount = static_cast<int>(std::size(kValues));

std::filesystem::path TuningFile() {
    return PathsSaveRoot() / "lang" / "textwipe_tuning.json";
}

void SaveTuning() {
    nlohmann::json json = nlohmann::json::object();
    for (size_t k = 0; k < g_tuning.size(); k++) {
        const Tuning locked = Locked(static_cast<Kind>(k));
        for (const Value &value : kValues) {
            const float now = std::round(g_tuning[k].*(value.field) * 1000.0f) / 1000.0f;
            if (std::fabs(now - locked.*(value.field)) > 0.0005f) {
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
                    g_tuning[k].*(value.field) = std::clamp(entry[value.key].get<float>(), value.low, value.high);
                }
            }
        }
    } catch (const std::exception &error) {
        std::fprintf(stderr, "textwipe: cannot read %s: %s\n", TuningFile().string().c_str(), error.what());
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
            return fired > 40 ? 10 : fired > 15 ? 3
                                                : 1;
        }
        return 0;
    }
};

Repeater g_minus;
Repeater g_plus;

void Adjust(int direction, int steps) {
    const bool   shift = DevKeyHeld("Left Shift") || DevKeyHeld("Right Shift");
    const bool   ctrl = DevKeyHeld("Left Ctrl") || DevKeyHeld("Right Ctrl");
    const Value &value = kValues[g_value];
    float       &field = g_tuning[static_cast<size_t>(g_kind)].*(value.field);
    if (value.field == &Tuning::fit) {
        field = direction > 0 ? 1.0f : 0.0f;
    } else {
        const float step = value.step * static_cast<float>(steps) * (shift ? 10.0f : ctrl ? 0.1f
                                                                                          : 1.0f);
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
        std::fprintf(stderr, "textwipe: %s\n", g_on ? "on (F10 steps the mode)" : "off");
    }
    if (DevKeyPressed("F10") && g_on) {
        g_mode = g_mode == Mode::Sweep        ? Mode::ShowBefore
                 : g_mode == Mode::ShowBefore ? Mode::ShowAfter
                 : g_mode == Mode::ShowAfter  ? Mode::Overlay
                                              : Mode::Sweep;
        g_started = std::chrono::steady_clock::now();
        std::fprintf(stderr, "textwipe: mode %s\n", ModeName());
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
    Tuning t;
    switch (kind) {
        case Kind::Message:
            t.size = 1.15f;
            t.xscale = 0.80f;
            t.yscale = 1.02f;
            t.dx = 0.35f;
            t.align = 1.0f;
            t.lock = 1.0f;
            break;
        case Kind::Options:
            t.size = 0.76f;
            t.yscale = 1.17f;
            t.dx = -1.10f;
            t.dy = -0.15f;
            t.lock = 1.0f;
            break;
        case Kind::Card:
            break;
        case Kind::Caption:
        case Kind::Count:
            break;
    }
    return t;
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

        static const float old_tint[4] = {1.0f, 0.25f, 0.25f, 0.75f};
        static const float new_tint[4] = {0.3f, 1.0f, 1.0f, 0.75f};
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
        const gfx::LogicalRect left = {0.0f, 0.0f, edge, gfx::LogicalHeight()};
        SetDrawClip(&left);
        draw(false);
    }
    if (edge < gfx::LogicalWidth()) {
        const gfx::LogicalRect right = {edge, 0.0f, gfx::LogicalWidth() - edge, gfx::LogicalHeight()};
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
            for (int c = 0; c < 4; c++) {
                vertex.color[c] = static_cast<uint8_t>(std::clamp(vertex.color[c] * (*g_tint)[c] + 0.5f, 0.0f, 255.0f));
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
        clipped.scissor_rect = {left, top, right - left, bottom - top};
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
            vx, vy, 0.0f, 0.0f, 0.0f, {grey, grey, grey, 0x80}
        };
    };
    gfx::DrawState state;
    state.blend = true;
    std::vector<gfx::Vertex2D> line;
    for (const auto &[w, grey] : {
             std::pair{2.5f, static_cast<unsigned char>(0)   },
             std::pair{1.0f, static_cast<unsigned char>(0x7F)}
    }) {
        const float h = half * w;
        line.push_back(vertex(x - h, 0.0f, grey));
        line.push_back(vertex(x + h, 0.0f, grey));
        line.push_back(vertex(x + h, gfx::LogicalHeight(), grey));
        line.push_back(vertex(x - h, gfx::LogicalHeight(), grey));
    }
    if (g_mode != Mode::Overlay) {
        gfx::Draw2D(gfx::Primitive::Quads, line, {}, state);
    }

    const bool        overlay = g_mode == Mode::Overlay;
    const std::string before = overlay ? "RED: BAKED PICTURE" : "BEFORE: BAKED PICTURE";
    const std::string after = overlay ? std::string("CYAN: RUNTIME TEXT") : std::string("AFTER: RUNTIME TEXT (") + ModeName() + ")";
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
    line_of(std::string("TUNING ") + kKindLabels[g_kind] + "  F11 SET  F12 VALUE  -/= CHANGE  F8 RESET");
    for (int v = 0; v < kValueCount; v++) {
        std::string number;
        if (kValues[v].field == &Tuning::fit) {
            number = tuning.fit >= 0.5f ? "ON" : "OFF";
        } else {
            std::ostringstream text;
            text << std::fixed << std::setprecision(3) << tuning.*(kValues[v].field);
            number = text.str();
        }
        line_of(std::string(v == g_value ? "> " : "  ") + kValues[v].label + " " + number);
    }
}

} // namespace textwipe
