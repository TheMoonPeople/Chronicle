#pragma once

namespace dc::render {

/**
 * Width of Dark Cloud's 2D coordinate space, which the
 * renderer is told at RendererInit.
 */
inline constexpr float kScreenWidth = 640.0f;

/**
 * Height of Dark Cloud's 2D coordinate space, which the
 * renderer is told at RendererInit.
 */
inline constexpr float kScreenHeight = 480.0f;

} // namespace dc::render

// DC_TRANSITIONAL_BEGIN
#ifndef DC_NO_TRANSITIONAL_USING
using namespace dc::render;
#endif
// DC_TRANSITIONAL_END
