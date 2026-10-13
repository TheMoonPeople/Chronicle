#pragma once

#include <functional>
#include <span>

#include "gfx/gfx.hpp"

namespace textwipe {

enum class Mode {
    Sweep,
    ShowBefore,
    ShowAfter,
    Overlay,
};

enum class Kind {
    Message,
    Caption,
    Card,
    Options,
    Count,
};

/**
 * Text transforms in logical units; fit and lock blend retail cell alignment.
 */
struct Tuning {
    float size = 1.0f;
    float xscale = 1.0f;
    float yscale = 1.0f;
    float letter = 0.0f;
    float word = 0.0f;
    float dx = 0.0f;
    float dy = 0.0f;
    float fit = 1.0f;
    float lock = 0.0f;
    float align = 0.5f;
    float weight = 0.0f;
    float opacity = 1.0f;
    float shadow = 1.0f;
};

/**
 * Returns shipped defaults with optional developer tuning applied.
 */
const Tuning &Get(Kind kind);

/**
 * Returns the shipped fit used by the reset action.
 */
Tuning Locked(Kind kind);

Kind MessageKind();

/**
 * Restores the previous message tuning context on scope exit.
 */
class MessageContext {
public:
    explicit MessageContext(Kind kind);
    ~MessageContext();
    MessageContext(const MessageContext &) = delete;
    MessageContext &operator=(const MessageContext &) = delete;

private:
    Kind previous_;
};

/**
 * Returns the repeating divider position in logical pixels, seconds after activation.
 */
float SweepPosition(double seconds);

void Update();

bool Active();

bool Overlaying();

float Divider();

/**
 * Draws false for painted text and true for runtime text; inactive mode draws only true.
 */
void Both(const std::function<void(bool runtime)> &draw);

void DrawDivider();

/**
 * Applies the active comparison clip or tint before submitting text geometry.
 */
void Draw2D(gfx::Primitive primitive, std::span<const gfx::Vertex2D> vertices, const gfx::TextureBinding &binding,
            const gfx::DrawState &state);

} // namespace textwipe
