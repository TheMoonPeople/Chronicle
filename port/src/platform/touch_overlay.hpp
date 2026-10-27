#pragma once

namespace dc::platform {

/**
 * Shows or hides Android's on-screen controls as the input layer asks and takes their state for the
 * next input poll.
 */
void TouchOverlayPoll();

/**
 * Sets the on-screen controls' opacity, from 0 to 1.
 */
void TouchOverlaySetOpacity(float opacity);

} // namespace dc::platform
