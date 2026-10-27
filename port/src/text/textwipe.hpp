#pragma once

#include <functional>
#include <span>

#include "gfx/gfx.hpp"

namespace dc::text::textwipe {

/**
 * Developer comparison view between baked text pictures and runtime text.
 */
enum class Mode {
    Sweep,      /**< Divider sweeps back and forth across the screen. */
    ShowBefore, /**< Only the baked picture is shown. */
    ShowAfter,  /**< Only the runtime text is shown. */
    Overlay,    /**< Both are drawn tinted on top of each other. */
};

/**
 * Category of on-screen text, each with its own tuning.
 */
enum class Kind {
    Message, /**< Dialogue and message windows. */
    Caption, /**< Menu captions. */
    Card,    /**< Area title cards. */
    Options, /**< Options screen text. */
    Count,   /**< Number of kinds. */
};

/**
 * Text transforms in logical units; fit and lock blend retail cell alignment.
 */
struct Tuning {
    float size = 1.0f;    /**< Overall glyph scale. */
    float xscale = 1.0f;  /**< Horizontal glyph scale. */
    float yscale = 1.0f;  /**< Vertical glyph scale. */
    float letter = 0.0f;  /**< Extra spacing between letters. */
    float word = 0.0f;    /**< Extra spacing between words. */
    float dx = 0.0f;      /**< Horizontal offset. */
    float dy = 0.0f;      /**< Vertical offset. */
    float fit = 1.0f;     /**< 1 fits text to retail cells, 0 leaves it free. */
    float lock = 0.0f;    /**< Blend towards retail cell positions. */
    float align = 0.5f;   /**< Alignment within retail cells. */
    float weight = 0.0f;  /**< Extra stroke weight. */
    float opacity = 1.0f; /**< Text opacity. */
    float shadow = 1.0f;  /**< Drop shadow strength. */
};

/**
 * Returns shipped defaults with optional developer tuning applied.
 */
const Tuning &Get(Kind kind);

/**
 * Returns the shipped fit used by the reset action.
 */
Tuning Locked(Kind kind);

/**
 * Returns the kind currently applied to message text.
 */
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
    Kind previous;
};

/**
 * Returns the repeating divider position in logical pixels, seconds after activation.
 */
float SweepPosition(double seconds);

/**
 * Polls the developer hotkeys and advances the comparison divider.
 */
void Update();

/**
 * Returns whether the comparison view is on.
 */
bool Active();

/**
 * Returns whether the comparison view is in overlay mode.
 */
bool Overlaying();

/**
 * Returns the current divider position in logical pixels.
 */
float Divider();

/**
 * Draws false for painted text and true for runtime text; inactive mode draws only true.
 */
void Both(const std::function<void(bool runtime)> &draw);

/**
 * Draws the divider line, labels and tuning readout while the view is on.
 */
void DrawDivider();

/**
 * Applies the active comparison clip or tint before submitting text geometry.
 */
void Draw2D(gfx::Primitive primitive, std::span<const gfx::Vertex2D> vertices, const gfx::TextureBinding &binding,
            const gfx::DrawState &state);

} // namespace dc::text::textwipe
