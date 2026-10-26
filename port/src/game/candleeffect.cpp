#include "candleeffect.hpp"

#include <libvu0.h>

#include "mglib.hpp"
#include "rect.hpp"
#include "snd.hpp"
#include "texture.hpp"

void CCandleEffect::Initialize() {
    this->enabled = true;
    this->animation_frame = 0.0f;
    this->half_height = 1.0f;
    this->half_width = 1.0f;
    this->texture = NULL;
}

CCandleEffect::CCandleEffect() {
    Initialize();
}

void CCandleEffect::SetTexture(CTexture *flame_texture) {
    this->texture = flame_texture;
}

void CCandleEffect::SetScale(float half_width, float half_height) {
    this->half_width = half_width;
    this->half_height = half_height;
}

void CCandleEffect::SetPosition(float *world) {
    sceVu0CopyVector(this->position, (float *) world);
}

void CCandleEffect::Step() {
    this->animation_frame += 0.3f;

    if (this->animation_frame > 8.0f) {
        this->animation_frame = 0.0f;
    }
}

void CCandleEffect::Draw() {
    if (texture == NULL || enabled == 0) {
        return;
    }

    float world[4];
    int   top_left[4];
    int   bottom_right[4];

    world[0] = position[0];
    world[1] = position[1];
    world[2] = position[2];
    world[3] = 1.0f;

    if (MGRotTransPers3DSprite(top_left, bottom_right, world, half_width, half_height, 0) == 0) {
        return;
    }

    bottom_right[1] = top_left[1] + ((bottom_right[1] - top_left[1]) >> 1);

    spRGBA     color = {128, 128, 128, 128};
    sceGsAlpha alpha = mgAlpha;
    alpha.bits.a = 0;
    alpha.bits.b = 2;
    alpha.bits.c = 0;
    alpha.bits.d = 1;
    sceGsZbuf zbuf = mgZBuffer;
    zbuf.bits.zmsk = 1;
    MGSetGsZBUF(&zbuf);

    int frame = 7 - (int) animation_frame;
    int column = (frame % 4) * 32;
    int row = (frame / 4) * 32;
    MGSetGsALPHA(&alpha);
    CRect_i_ source(column, row, 32, 32);
    set3DSprite(GetVif1Packet(), texture, source, top_left, bottom_right, &color);

    MGSetGsALPHA(&mgAlpha);
    MGSetGsZBUF(&mgZBuffer);
}
