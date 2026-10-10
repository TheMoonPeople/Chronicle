#include "fish.hpp"

#include <cmath>

#include "fish_port.hpp"
#include "mathutil.hpp"

namespace {

// CFish::SetScale's deviation is the sum of 12 uniform draws less 6. For a sum x in [2, 3], this
// is 12! times the chance the sum is at most x; the sum is symmetric about 6, so it also gives
// the chance of the top of the roll.
double LowTail(double x) {
    return std::pow(x, 12) - 12.0 * std::pow(x - 1.0, 12) + 66.0 * std::pow(x - 2.0, 12);
}

} // namespace

float FishSizeLift(float range) {
    // A display centimetre is 0.1 of size, and the roll moves a quarter of the range per unit.
    double centimetre = 0.4 / range;
    if (!(centimetre < 1.0)) {
        return 0.0f;
    }
    double at_max = LowTail(2.0);                            // the deviation reaches 4: clamped
    double below = LowTail(2.0 + centimetre) - at_max;       // the centimetre below the largest
    return static_cast<float>(std::fmax(0.0, 1.0 - below / at_max));
}

float SmoothFishSize(float size, float base_size, float max_size) {
    if (max_size <= base_size || size <= base_size || size >= max_size) {
        return size;
    }
    float range = max_size - base_size;
    float t = (size - base_size) / range;
    float lifted = size + FishSizeLift(range) * range * std::pow(t, kFishSizeLiftExponent) * (1.0f - t);
    // Only a clamped roll is the largest size: a lifted one stays a hundredth of a centimetre under,
    // so FishingGetAngleFishSize's size * 10 cannot round up into the largest's centimetre. A roll
    // already that close keeps its own size, as retail.
    return std::fmax(size, std::fmin(lifted, max_size - 0.001f));
}

// Retail's roll (info.min_size is the kind's usual size, the roll's centre), with the sizes above it
// ramped into the largest (fish_port.hpp) before the model scales are taken.
PC_OVERRIDE void CFish::SetScale() {
    float deviation = nrnd();
    size = info.min_size;

    if (deviation >= 0.0f) {
        size += deviation * (info.max_size - info.min_size) / 4.0f;
    } else {
        size += deviation * (info.max_size - info.min_size) / 8.0f;
    }

    if (size < 0.5f * info.min_size) {
        size = 0.5f * info.min_size;
    }

    if (size > info.max_size) {
        size = info.max_size;
    }

    size = SmoothFishSize(size, info.min_size, info.max_size);

    angle_model_scale = size / info.model_size;
    model_scale = size / 25.0f;
}
