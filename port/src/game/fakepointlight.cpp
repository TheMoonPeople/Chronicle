#include "fakepointlight.hpp"

namespace dc::game {

CFakePointLight::CFakePointLight()
    : colour{128.0f, 128.0f, 128.0f, 128.0f}, inner_range(10.0f), outer_range(70.0f), used(0) {}

} // namespace dc::game
