#pragma once

#include <chrono>
#include <cstdint>
#include <string_view>

#include "gfx/gfx.hpp"

// A line of text over presented frames (the FPS counter), in the Dark Cloud Compendium Community Font (by Dayuppy MoonBunny), drawn with
// gfx::Draw2D at the window's top-left corner.

inline constexpr int kOverlayGlyphWidth = 5;
inline constexpr int kOverlayGlyphHeight = 7;
// Pen advance per character, in font pixels.
inline constexpr int kOverlayAdvance = 6;
// Backdrop around the text and its distance from the corner, in font pixels.
inline constexpr int kOverlayPadding = 2;

// The 5x7 bitmap font the first-run screen uses: the seven rows of c's glyph, bit 4 the leftmost column: upper case (lower case maps to it), digits
// and .,:;/()+-_%'[]?~. nullptr for a space; anything else is '?'.
const std::uint8_t *OverlayGlyph(char c);

// Target pixels per font pixel on a target logical space maps onto as mapping says: whole pixels,
// the nearest to one per logical unit, at least one.
int OverlayPixelSize(const gfx::LogicalMapping &mapping);

// Draws text on the current target, which mapping describes, with its top-left corner at target
// pixel (x, y): a translucent black backdrop kOverlayPadding font pixels around it, then white glyphs,
// each font pixel `pixel` target pixels square. Inside a frame or a recording.
void OverlayDrawText(std::string_view text, const gfx::LogicalMapping &mapping, int pixel, int x, int y);

// text at the top-left corner of the main target, as it is mapped now, recorded into a display list
// of its own for RenderOptions::overlay. Outside a frame or a recording.
gfx::DisplayListRef OverlayRecord(std::string_view text);

// Events per second (presented frames, logic ticks), measured over windows of at least `window`.
class OverlayRate {
public:
    using Clock = std::chrono::steady_clock;

    explicit OverlayRate(Clock::duration window = std::chrono::milliseconds(500)) : window_(window) {}

    // One event at now. True when it closed a window, so PerSecond changed.
    bool Count(Clock::time_point now);

    // The last closed window's rate; 0 until one has closed.
    double PerSecond() const { return rate_; }

private:
    Clock::duration   window_;
    Clock::time_point start_;
    bool              started_ = false;
    std::int64_t      events_ = 0;
    double            rate_ = 0.0;
};
