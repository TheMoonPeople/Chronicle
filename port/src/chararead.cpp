#include "chararead.hpp"

#include <cstring>
#include <iterator>

#include "dataalloc.hpp"
#include "dataread.hpp"
#include "dataread_port.hpp"
#include "stubs/cloth.hpp"
#include "vu0_ops.hpp"

CDataAlloc2<1> *img_buffer;
u_int          *pack_data;
u_int          *load_img[4];

void CommandIMG(void **argv) {
    if (img_buffer == nullptr) {
        return;
    }

    const int slot = *static_cast<int *>(argv[0]);
    if (slot < 0 || slot >= std::ssize(load_img)) {
        return;
    }

    int    size = 0;
    u_int *image_file = GetPackFile(pack_data, static_cast<char *>(argv[1]), &size);
    if (image_file == nullptr) {
        return;
    }

    DataReadSource source;
    const bool     has_source = size > 0 && DataReadSourceRange(image_file, static_cast<std::size_t>(size), source);
    load_img[slot] = reinterpret_cast<u_int *>(img_buffer->Alloc((size >> 4) + 1));
    std::memcpy(load_img[slot], image_file, static_cast<std::size_t>(size));

    if (has_source) {
        DataReadRegisterCopiedSource(load_img[slot], static_cast<std::size_t>(size), img_buffer, source);
    }
}

void StretchBind2(float *point_a, float *point_b, float *spring) {
    float delta[3] = {point_a[0] - point_b[0], point_a[1] - point_b[1], point_a[2] - point_b[2]};
    float length_squared = delta[0] * delta[0];
    length_squared = length_squared + delta[1] * delta[1];
    length_squared = length_squared + delta[2] * delta[2];
    float inverse_scale = vu0::Rsqrt(spring[0], length_squared);
    float pull_a = spring[1];
    float pull_b = spring[2];

    for (int i = 0; i < 3; i++) {
        float slack = delta[i] - vu0::Mul(delta[i], inverse_scale);
        point_a[i] = point_a[i] - slack * pull_a;
        point_b[i] = point_b[i] + slack * pull_b;
    }
}

float vuabs(float *vector) {
    return vu0::Sqrt(vu0::LengthSquared(vector));
}
