#pragma helper_mask_gpr 0x30
#pragma helper_mask_fpr 0x1000

#include "fakepointlight.hpp"

CFakePointLight::CFakePointLight() {
    this->colour[0] = this->colour[1] = this->colour[2] = this->colour[3] = 128.0f;
    this->inner_range = 10.0f;
    this->outer_range = 70.0f;
    this->used = 0;
}
