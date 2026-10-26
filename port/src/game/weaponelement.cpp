#pragma argument_flag 0
#pragma argument_flag_ones 71, 99, 109, 124, 144, 182, 223, 240, 274, 305
#pragma literal_reload 0x3C23D70A

#include "weaponelement.hpp"

#include <libvu0.h>

#include <cstdlib>

#include "mglib.hpp"
#include "rect.hpp"
#include "snd.hpp"
#include "texture.hpp"

void CWeaponElement::Initialize() {
    int i;

    for (i = 0; i < WEAPON_ELEMENT_SPARK_MAX; i++) {
        size[i] = 1.0f;
        alpha[i] = 1.0f;
    }

    on = 0;
}

void CWeaponElement::Set(sceVu0FVECTOR *origin, float *position, float power, int kind, float spread) {
    this->power = 0.01f * (1.0f + power);
    this->kind = kind;
    this->spread = spread;
    this->origin = origin;

    switch (kind) {
        default:
        case WEAPON_ELEMENT_COLD:
            Init_Cold(position);
            break;
        case WEAPON_ELEMENT_WIND:
            Init_Wind(position);
            break;
        case WEAPON_ELEMENT_FIRE:
            Init_Fire(position);
            break;
        case WEAPON_ELEMENT_THUNDER:
            Init_Thunder(position);
            break;
        case WEAPON_ELEMENT_HOLY:
            Init_Holy(position);
            break;
    }

    on = 1;
}

void CWeaponElement::Step() {
    if (on != 0) {
        switch (kind) {
            default:
            case WEAPON_ELEMENT_COLD:
                Step_Cold();
                break;
            case WEAPON_ELEMENT_WIND:
                Step_Wind();
                break;
            case WEAPON_ELEMENT_FIRE:
                Step_Fire();
                break;
            case WEAPON_ELEMENT_THUNDER:
                Step_Thunder();
                break;
            case WEAPON_ELEMENT_HOLY:
                Step_Holy();
                break;
        }
    }
}

void CWeaponElement::Draw() {
    if (on != 0) {
        switch (kind) {
            default:
            case WEAPON_ELEMENT_COLD:
                Draw_Cold();
                break;
            case WEAPON_ELEMENT_WIND:
                Draw_Wind();
                break;
            case WEAPON_ELEMENT_FIRE:
                Draw_Fire();
                break;
            case WEAPON_ELEMENT_THUNDER:
                Draw_Thunder();
                break;
            case WEAPON_ELEMENT_HOLY:
                Draw_Holy();
                break;
        }
    }
}

void CWeaponElement::Init_Holy(float *position) {
    count = (int) (12.0f * power) + 2;
    spawn_budget = (int) (20.0f * power) + 10;
    spawn_delay_max = 6 - (int) (3.0f * power);
    spawn_delay = 0;
    frame_timer = 4;
    spread = spread * (float) (0.8 + 0.4f * power);
    scale = 0.5f + 0.7f * power;

    for (int i = 0; i < WEAPON_ELEMENT_SPARK_MAX; i++) {
        shrink[i] = 0.0f;
        alpha[i] = 0.0f;
    }

    for (int i = 0; i < count; i++) {
        size[i] = 2.0f + 6.0f * rand() / 2147483648.0f;
        shrink[i] = 1.0f;
        alpha[i] = 1.0f + 48.0f * rand() / 2147483648.0f;
        fading[i] = 0;
        offset[i][0] = 2.0f * (spread * rand()) / 2147483648.0f - spread;
        offset[i][1] = spread * rand() / 2147483648.0f - spread / 2.0f;
        offset[i][2] = 2.0f * (spread * rand()) / 2147483648.0f - spread;
        offset[i][3] = 1.0f;
        frame[i] = (int) (5.0f * rand() / 2147483648.0f) * 0x30;
    }
}

void CWeaponElement::Step_Holy() {
    int dead;
    int i;

    dead = 0;

    for (i = 0; i < WEAPON_ELEMENT_SPARK_MAX; i++) {
        if (alpha[i] <= 0.0f) {
            dead++;
        } else {
            offset[i][1] += 0.5f;
            shrink[i] -= 0.01f;

            if (shrink[i] <= 0.0f) {
                shrink[i] = 0.0f;
                alpha[i] = 0.0f;
            }

            if (frame_timer == 4) {
                frame[i] = (int) (5.0f * rand() / 2147483648.0f) * 0x30;
            }

            if (fading[i] != 0) {
                alpha[i] -= 8.0f;

                if (alpha[i] <= 0.0f) {
                    alpha[i] = 0.0f;
                }
            } else {
                alpha[i] += 16.0f;

                if (!(alpha[i] < 128.0f)) {
                    fading[i] = 1;
                }
            }
        }
    }

    frame_timer = frame_timer - 1;

    if (frame_timer == 0) {
        frame_timer = 4;
    }

    // While the budget lasts, a free slot is filled again every so often.
    if (spawn_budget > 0) {
        spawn_budget = spawn_budget - 1;
        spawn_delay = spawn_delay - 1;

        if (spawn_delay <= 0) {
            for (i = 0; i < WEAPON_ELEMENT_SPARK_MAX; i++) {
                if (alpha[i] == 0.0f) {
                    size[i] = 2.0f + 6.0f * rand() / 2147483648.0f;
                    shrink[i] = 1.0f;
                    alpha[i] = 1.0f + 48.0f * rand() / 2147483648.0f;
                    fading[i] = 0;
                    offset[i][0] = 2.0f * (spread * rand()) / 2147483648.0f - spread;
                    offset[i][1] = spread * rand() / 2147483648.0f - spread / 2.0f;
                    offset[i][2] = 2.0f * (spread * rand()) / 2147483648.0f - spread;
                    offset[i][3] = 1.0f;
                    frame[i] = (int) (5.0f * rand() / 2147483648.0f) * 0x30;
                    spawn_delay = (int) ((float) spawn_delay_max * rand() / 2147483648.0f) + 1;
                    break;
                }
            }
        }
    }

    if (dead >= WEAPON_ELEMENT_SPARK_MAX) {
        on = 0;
    }
}

void CWeaponElement::Draw_Holy() {
    // The four screen corners of one sprite: top left, top right, bottom
    // left, bottom right.
    int           corner[4][4];
    sceVu0FVECTOR base;
    sceVu0FVECTOR pos;
    CRect_i_      rect;
    CTexture     *texture;
    int           i;
    float         width;
    int           row;

    texture = TexManager.GetTexture("element", -1);
    sceVu0CopyVector(base, *origin);
    pos[3] = 1.0f;

    for (i = 0; i < WEAPON_ELEMENT_SPARK_MAX; i++) {
        row = frame[i];

        if (alpha[i] <= 0.0f) {
            continue;
        }

        width = scale * (size[i] * shrink[i]);
        pos[0] = base[0] + offset[i][0];
        pos[1] = base[1] + offset[i][1];
        pos[2] = base[2] + offset[i][2];

        if (MGRotTransPers3DSprite(corner[0], corner[3], pos, width, width / 2.0f, 0) != 1) {
            continue;
        }

        // The two corners that the sprite call gives back are opposite, so the
        // other two take one part from each.
        corner[1][0] = corner[3][0];
        corner[1][1] = corner[0][1];
        corner[1][2] = corner[0][2];
        corner[2][0] = corner[0][0];
        corner[2][1] = corner[3][1];
        corner[2][2] = corner[3][2];

        rect.x = 0xD0;
        rect.y = row;
        rect.width = 0x30;
        rect.height = 0x30;
        set3DSprite(Vif1Packet, texture, rect, corner[0], corner[1], corner[2], corner[3], (u8) alpha[i]);
    }
}

void CWeaponElement::Init_Cold(float *position) {
    count = (int) (12.0f * power) + 2;
    spawn_budget = (int) (20.0f * power) + 10;
    spawn_delay_max = 6 - (int) (3.0f * power);
    spawn_delay = 0;
    frame_timer = 4;
    spread = spread * (float) (0.8 + 0.4f * power);
    scale = 0.5f + 0.7f * power;

    for (int i = 0; i < WEAPON_ELEMENT_SPARK_MAX; i++) {
        shrink[i] = 0.0f;
        alpha[i] = 0.0f;
    }

    for (int i = 0; i < count; i++) {
        size[i] = 3.0f + 6.0f * rand() / 2147483648.0f;
        shrink[i] = 1.0f;
        alpha[i] = 1.0f + 48.0f * rand() / 2147483648.0f;
        fading[i] = 0;
        offset[i][0] = 2.0f * (spread * rand()) / 2147483648.0f - spread;
        offset[i][1] = spread / 2.0f + spread * rand() / 2147483648.0f;
        offset[i][2] = 2.0f * (spread * rand()) / 2147483648.0f - spread;
        offset[i][3] = 1.0f;
        frame[i] = (int) (5.0f * rand() / 2147483648.0f) * 0x30;
    }
}

void CWeaponElement::Step_Cold() {
    int dead;
    int i;

    dead = 0;

    for (i = 0; i < WEAPON_ELEMENT_SPARK_MAX; i++) {
        if (alpha[i] <= 0.0f) {
            dead++;
        } else {
            offset[i][1] -= 0.1f + 0.3f * rand() / 2147483648.0f;
            shrink[i] -= 0.01f;

            if (shrink[i] <= 0.0f) {
                shrink[i] = 0.0f;
                alpha[i] = 0.0f;
            }

            if (frame_timer == 4) {
                frame[i] = (int) (5.0f * rand() / 2147483648.0f) * 0x30;
            }

            if (fading[i] != 0) {
                alpha[i] -= 6.0f;

                if (alpha[i] <= 0.0f) {
                    alpha[i] = 0.0f;
                }
            } else {
                alpha[i] += 12.0f;

                if (!(alpha[i] < 128.0f)) {
                    fading[i] = 1;
                }
            }
        }
    }

    frame_timer = frame_timer - 1;

    if (frame_timer == 0) {
        frame_timer = 4;
    }

    if (spawn_budget > 0) {
        spawn_budget = spawn_budget - 1;
        spawn_delay = spawn_delay - 1;

        if (spawn_delay <= 0) {
            for (i = 0; i < WEAPON_ELEMENT_SPARK_MAX; i++) {
                if (alpha[i] == 0.0f) {
                    size[i] = 3.0f + 6.0f * rand() / 2147483648.0f;
                    shrink[i] = 1.0f;
                    alpha[i] = 1.0f + 48.0f * rand() / 2147483648.0f;
                    fading[i] = 0;
                    offset[i][0] = 2.0f * (spread * rand()) / 2147483648.0f - spread;
                    offset[i][1] = spread / 2.0f + spread * rand() / 2147483648.0f;
                    offset[i][2] = 2.0f * (spread * rand()) / 2147483648.0f - spread;
                    offset[i][3] = 1.0f;
                    frame[i] = (int) (5.0f * rand() / 2147483648.0f) * 0x30;
                    spawn_delay = (int) ((float) spawn_delay_max * rand() / 2147483648.0f) + 1;
                    break;
                }
            }
        }
    }

    if (dead >= WEAPON_ELEMENT_SPARK_MAX) {
        on = 0;
    }
}

void CWeaponElement::Draw_Cold() {
    // The four screen corners of one sprite: top left, top right, bottom
    // left, bottom right.
    int           corner[4][4];
    sceVu0FVECTOR base;
    sceVu0FVECTOR pos;
    CRect_i_      rect;
    CTexture     *texture;
    int           i;
    float         width;
    int           row;

    texture = TexManager.GetTexture("element", -1);
    sceVu0CopyVector(base, *origin);
    pos[3] = 1.0f;

    for (i = 0; i < WEAPON_ELEMENT_SPARK_MAX; i++) {
        row = frame[i];

        if (alpha[i] <= 0.0f) {
            continue;
        }

        width = scale * (size[i] * shrink[i]);
        pos[0] = base[0] + offset[i][0];
        pos[1] = base[1] + offset[i][1];
        pos[2] = base[2] + offset[i][2];

        if (MGRotTransPers3DSprite(corner[0], corner[3], pos, width, width / 2.0f, 0) != 1) {
            continue;
        }

        // The two corners that the sprite call gives back are opposite, so the
        // other two take one part from each.
        corner[1][0] = corner[3][0];
        corner[1][1] = corner[0][1];
        corner[1][2] = corner[0][2];
        corner[2][0] = corner[0][0];
        corner[2][1] = corner[3][1];
        corner[2][2] = corner[3][2];

        rect.x = 0xA0;
        rect.y = row;
        rect.width = 0x30;
        rect.height = 0x30;
        set3DSprite(Vif1Packet, texture, rect, corner[0], corner[1], corner[2], corner[3], (u8) alpha[i]);
    }
}

void CWeaponElement::Init_Wind(float *position) {
    count = (int) (10.0f * power) + 1;
    spawn_budget = (int) (20.0f * power) + 10;
    spawn_delay_max = 8 - (int) (4.0f * power);
    spawn_delay = 0;
    frame_timer = 4;
    spread = spread * (float) (0.8 + 0.4f * power);
    scale = 0.5f + 1.3f * (0.7f * power);

    for (int i = 0; i < WEAPON_ELEMENT_SPARK_MAX; i++) {
        shrink[i] = 0.0f;
        alpha[i] = 0.0f;
    }

    for (int i = 0; i < count; i++) {
        size[i] = 2.0f + 4.0f * rand() / 2147483648.0f;
        shrink[i] = 1.0f;
        alpha[i] = 1.0f + 48.0f * rand() / 2147483648.0f;
        fading[i] = 0;
        offset[i][0] = 2.0f * (spread * rand()) / 2147483648.0f - spread;
        offset[i][1] = 2.0f * (spread * rand()) / 2147483648.0f - spread;
        offset[i][2] = 2.0f * (spread * rand()) / 2147483648.0f - spread;
        offset[i][3] = 1.0f;

        // Each spark blows along the line it starts on, at its own speed.
        sceVu0CopyVector(velocity[i], offset[i]);
        sceVu0Normalize(velocity[i], velocity[i]);
        sceVu0ScaleVector(velocity[i], velocity[i], 0.3f * rand() / 2147483648.0f);
        spin[i] = 2.0f * (PI_SHORT * rand()) / 2147483648.0f - PI_SHORT;
        spin_speed[i] = 0.09817475f + SIXTEENTH_PI_SHORT * rand() / 2147483648.0f;
        frame[i] = (int) (5.0f * rand() / 2147483648.0f) * 0x30;
    }
}

void CWeaponElement::Step_Wind() {
    int dead;
    int i;

    dead = 0;

    for (i = 0; i < WEAPON_ELEMENT_SPARK_MAX; i++) {
        if (alpha[i] <= 0.0f) {
            dead++;
        } else {
            if (fading[i] != 0) {
                alpha[i] -= 4.0f;

                if (alpha[i] <= 0.0f) {
                    alpha[i] = 0.0f;
                }
            } else {
                alpha[i] += 32.0f;

                if (!(alpha[i] < 128.0f)) {
                    fading[i] = 1;
                }
            }

            if (frame_timer == 4) {
                frame[i] = (int) (5.0f * rand() / 2147483648.0f) * 0x30;
            }

            spin[i] += spin_speed[i];

            if (!(spin[i] <= PI_SHORT)) {
                spin[i] -= TWO_PI_SHORT;
            }

            offset[i][0] += velocity[i][0];
            offset[i][1] += 0.2f;
            offset[i][2] += velocity[i][2];
        }
    }

    frame_timer = frame_timer - 1;

    if (frame_timer == 0) {
        frame_timer = 4;
    }

    if (spawn_budget > 0) {
        spawn_budget = spawn_budget - 1;
        spawn_delay = spawn_delay - 1;

        if (spawn_delay <= 0) {
            for (i = 0; i < WEAPON_ELEMENT_SPARK_MAX; i++) {
                if (alpha[i] == 0.0f) {
                    size[i] = 2.0f + 4.0f * rand() / 2147483648.0f;
                    shrink[i] = 1.0f;
                    alpha[i] = 1.0f + 48.0f * rand() / 2147483648.0f;
                    fading[i] = 0;
                    offset[i][0] = 2.0f * (spread * rand()) / 2147483648.0f - spread;
                    offset[i][1] = 2.0f * (spread * rand()) / 2147483648.0f - spread;
                    offset[i][2] = 2.0f * (spread * rand()) / 2147483648.0f - spread;
                    offset[i][3] = 1.0f;
                    sceVu0CopyVector(velocity[i], offset[i]);
                    sceVu0Normalize(velocity[i], velocity[i]);
                    sceVu0ScaleVector(velocity[i], velocity[i], 0.3f * rand() / 2147483648.0f);
                    spin[i] = 2.0f * (PI_SHORT * rand()) / 2147483648.0f - PI_SHORT;
                    spin_speed[i] = 0.09817475f + SIXTEENTH_PI_SHORT * rand() / 2147483648.0f;
                    frame[i] = (int) (5.0f * rand() / 2147483648.0f) * 0x30;
                    spawn_delay = (int) ((float) spawn_delay_max * rand() / 2147483648.0f) + 1;
                    break;
                }
            }
        }
    }

    if (dead >= WEAPON_ELEMENT_SPARK_MAX) {
        on = 0;
    }
}

void CWeaponElement::Draw_Wind() {
    // The four screen corners of one sprite: top left, top right, bottom
    // left, bottom right.
    int           corner[4][4];
    sceVu0FVECTOR base;
    sceVu0FVECTOR pos;
    sceVu0FMATRIX unit;
    sceVu0FMATRIX turn;
    CRect_i_      rect;
    CTexture     *texture;
    int           i;
    float         width;
    int           row;

    texture = TexManager.GetTexture("element", -1);
    sceVu0CopyVector(base, *origin);
    pos[3] = 1.0f;

    for (i = 0; i < WEAPON_ELEMENT_SPARK_MAX; i++) {
        row = frame[i];

        if (alpha[i] <= 0.0f) {
            continue;
        }

        width = scale * (size[i] * shrink[i]);

        // A wind spark is turned about the vertical by the spin it carries.
        sceVu0UnitMatrix(unit);
        sceVu0RotMatrixY(turn, unit, spin[i]);
        sceVu0ApplyMatrix(pos, turn, offset[i]);
        pos[0] += base[0];
        pos[1] += base[1];
        pos[2] += base[2];

        if (MGRotTransPers3DSprite(corner[0], corner[3], pos, width, width / 2.0f, 0) != 1) {
            continue;
        }

        // The two corners that the sprite call gives back are opposite, so the
        // other two take one part from each.
        corner[1][0] = corner[3][0];
        corner[1][1] = corner[0][1];
        corner[1][2] = corner[0][2];
        corner[2][0] = corner[0][0];
        corner[2][1] = corner[3][1];
        corner[2][2] = corner[3][2];

        rect.x = 0x60;
        rect.y = row;
        rect.width = 0x40;
        rect.height = 0x30;
        set3DSprite(Vif1Packet, texture, rect, corner[0], corner[1], corner[2], corner[3], (u8) alpha[i]);
    }
}

void CWeaponElement::Init_Fire(float *position) {
    count = (int) (12.0f * power) + 2;
    spawn_budget = (int) (20.0f * power) + 10;
    spawn_delay_max = 6 - (int) (3.0f * power);
    spawn_delay = 0;
    frame_timer = 4;
    spread = spread * (float) (0.8 + 0.4f * power);
    scale = 0.5f + 0.7f * power;

    // Fire stays where it was lit rather than following the weapon.
    sceVu0CopyVector(fire_pos, position);

    for (int i = 0; i < WEAPON_ELEMENT_SPARK_MAX; i++) {
        shrink[i] = 0.0f;
        alpha[i] = 0.0f;
    }

    for (int i = 0; i < count; i++) {
        size[i] = 2.0f + 6.0f * rand() / 2147483648.0f;
        shrink[i] = 1.0f;
        alpha[i] = 1.0f + 48.0f * rand() / 2147483648.0f;
        fading[i] = 0;
        offset[i][0] = 2.0f * (spread * rand()) / 2147483648.0f - spread;
        offset[i][1] = spread * rand() / 2147483648.0f - spread / 2.0f;
        offset[i][2] = 2.0f * (spread * rand()) / 2147483648.0f - spread;
        offset[i][3] = 1.0f;
        frame[i] = (int) (5.0f * rand() / 2147483648.0f) * 0x30;
    }
}

void CWeaponElement::Step_Fire() {
    int dead;
    int i;

    dead = 0;

    for (i = 0; i < WEAPON_ELEMENT_SPARK_MAX; i++) {
        if (alpha[i] <= 0.0f) {
            dead++;
        } else {
            offset[i][1] += 0.01f + 0.6f * rand() / 2147483648.0f;
            shrink[i] -= 0.01f;

            if (shrink[i] <= 0.0f) {
                shrink[i] = 0.0f;
                alpha[i] = 0.0f;
            }

            if (frame_timer == 4) {
                frame[i] = (int) (5.0f * rand() / 2147483648.0f) * 0x30;
            }

            if (fading[i] != 0) {
                alpha[i] -= 8.0f;

                if (alpha[i] <= 0.0f) {
                    alpha[i] = 0.0f;
                }
            } else {
                alpha[i] += 16.0f;

                if (!(alpha[i] < 128.0f)) {
                    fading[i] = 1;
                }
            }
        }
    }

    frame_timer = frame_timer - 1;

    if (frame_timer == 0) {
        frame_timer = 4;
    }

    if (spawn_budget > 0) {
        spawn_budget = spawn_budget - 1;
        spawn_delay = spawn_delay - 1;

        if (spawn_delay <= 0) {
            for (i = 0; i < WEAPON_ELEMENT_SPARK_MAX; i++) {
                if (alpha[i] == 0.0f) {
                    size[i] = 2.0f + 6.0f * rand() / 2147483648.0f;
                    shrink[i] = 1.0f;
                    alpha[i] = 1.0f + 48.0f * rand() / 2147483648.0f;
                    fading[i] = 0;
                    offset[i][0] = 2.0f * (spread * rand()) / 2147483648.0f - spread;
                    offset[i][1] = spread * rand() / 2147483648.0f - spread / 2.0f;
                    offset[i][2] = 2.0f * (spread * rand()) / 2147483648.0f - spread;
                    offset[i][3] = 1.0f;
                    frame[i] = (int) (5.0f * rand() / 2147483648.0f) * 0x30;
                    spawn_delay = (int) ((float) spawn_delay_max * rand() / 2147483648.0f) + 1;
                    break;
                }
            }
        }
    }

    if (dead >= WEAPON_ELEMENT_SPARK_MAX) {
        on = 0;
    }
}

void CWeaponElement::Draw_Fire() {
    // The four screen corners of one sprite: top left, top right, bottom
    // left, bottom right.
    int           corner[4][4];
    sceVu0FVECTOR base;
    sceVu0FVECTOR pos;
    CRect_i_      rect;
    CTexture     *texture;
    int           i;
    float         width;
    int           row;

    texture = TexManager.GetTexture("element", -1);
    sceVu0CopyVector(base, fire_pos);
    pos[3] = 1.0f;

    for (i = 0; i < WEAPON_ELEMENT_SPARK_MAX; i++) {
        row = frame[i];

        if (alpha[i] <= 0.0f) {
            continue;
        }

        width = scale * (size[i] * shrink[i]);
        pos[0] = base[0] + offset[i][0];
        pos[1] = base[1] + offset[i][1];
        pos[2] = base[2] + offset[i][2];

        if (MGRotTransPers3DSprite(corner[0], corner[3], pos, width, width / 2.0f, 0) != 1) {
            continue;
        }

        // The two corners that the sprite call gives back are opposite, so the
        // other two take one part from each.
        corner[1][0] = corner[3][0];
        corner[1][1] = corner[0][1];
        corner[1][2] = corner[0][2];
        corner[2][0] = corner[0][0];
        corner[2][1] = corner[3][1];
        corner[2][2] = corner[3][2];

        rect.x = 0x30;
        rect.y = row;
        rect.width = 0x30;
        rect.height = 0x30;
        set3DSprite(Vif1Packet, texture, rect, corner[0], corner[1], corner[2], corner[3], (u8) alpha[i]);
    }
}

void CWeaponElement::Init_Thunder(float *position) {
    sceVu0FVECTOR reach;
    sceVu0FVECTOR dir;

    count = (int) (18.0f * power) + 6;
    bolt_count = (int) (7.0f * power) + 1;
    spread = spread * (float) (0.8 + 0.4f * power);

    for (int i = 0; i < count; i++) {
        velocity[i][0] = 8.0f * rand() / 2147483648.0f - 4.0f;
        velocity[i][1] = 4.0f * rand() / 2147483648.0f - 2.0f;
        velocity[i][2] = 8.0f * rand() / 2147483648.0f - 4.0f;

        // Each spark is thrown somewhere along the line it will travel.
        sceVu0Normalize(dir, velocity[i]);
        sceVu0ScaleVectorXYZ(reach, dir, spread);
        offset[i][0] = position[0] + velocity[i][0] + reach[0] * rand() / 2147483648.0f;
        offset[i][1] = position[1] + velocity[i][1] + reach[1] * rand() / 2147483648.0f;
        offset[i][2] = position[2] + velocity[i][2] + reach[2] * rand() / 2147483648.0f;
        offset[i][3] = 1.0f;
        sceVu0ScaleVectorXYZ(velocity[i], dir, 0.3f * rand() / 2147483648.0f);
        size[i] = 0.5f + 2.5f * rand() / 2147483648.0f;
        shrink[i] = 1.0f;
        alpha[i] = 96.0f + (int) (64.0f * rand() / 2147483648.0f);
    }

    for (int i = 0; i < bolt_count; i++) {
        bolt_head[i] = (int) ((float) count * rand() / 2147483648.0f);
        bolt_tail[i] = (int) ((float) count * rand() / 2147483648.0f);
        bolt_timer[i] = (int) (6.0f * rand() / 2147483648.0f) * 3 + 3;
        bolt_frame[i] = (int) (4.0f * rand() / 2147483648.0f);
    }
}

void CWeaponElement::Step_Thunder() {
    int dead;

    dead = 0;

    for (int i = 0; i < count; i++) {
        if (alpha[i] <= 0.0f) {
            dead++;
        } else {
            offset[i][0] += velocity[i][0];
            offset[i][1] += velocity[i][1];
            offset[i][2] += velocity[i][2];
            shrink[i] -= 0.01f;
            alpha[i] -= 4.0f;

            if (alpha[i] <= 3.0f) {
                alpha[i] = 0.0f;
            }
        }
    }

    if (dead >= count) {
        on = 0;
        return;
    }

    for (int i = 0; i < bolt_count; i++) {
        bolt_timer[i] = bolt_timer[i] - 1;

        if (bolt_timer[i] <= 0) {
            bolt_head[i] = (int) ((float) count * rand() / 2147483648.0f);
            bolt_tail[i] = (int) ((float) count * rand() / 2147483648.0f);
            bolt_timer[i] = (int) (6.0f * rand() / 2147483648.0f) * 3 + 3;
            bolt_frame[i] = (int) (4.0f * rand() / 2147483648.0f);
        } else if (bolt_timer[i] % 3 == 0) {
            bolt_frame[i] = bolt_frame[i] + 1;

            if (bolt_frame[i] >= 4) {
                bolt_frame[i] = 0;
            }
        }
    }
}

void CWeaponElement::Draw_Thunder() {
    // The four screen corners of one sprite: top left, top right, bottom
    // left, bottom right. A bolt uses the same four as the ends of its arc.
    int           corner[4][4];
    sceVu0FVECTOR base;
    CTexture     *texture;
    int           i;
    float         width;

    texture = TexManager.GetTexture("element", -1);
    sceVu0CopyVector(base, *origin);

    for (i = 0; i < count; i++) {
        if (alpha[i] <= 0.0f) {
            continue;
        }

        width = size[i] * shrink[i];

        if (MGRotTransPers3DSprite(corner[0], corner[3], offset[i], width, width / 2.0f, 0) != 1) {
            continue;
        }

        // The two corners that the sprite call gives back are opposite, so the
        // other two take one part from each.
        corner[1][0] = corner[3][0];
        corner[1][1] = corner[0][1];
        corner[1][2] = corner[0][2];
        corner[2][0] = corner[0][0];
        corner[2][1] = corner[3][1];
        corner[2][2] = corner[3][2];

        set3DSprite(Vif1Packet, texture, CRect_i_(0, 0, 0x30, 0x30), corner[0], corner[1], corner[2], corner[3], (u8) alpha[i]);
    }

    int bolt_uv[4][2] = {
        {0x00, 0x30},
        {0x18, 0x30},
        {0x00, 0x98},
        {0x00, 0x98}
    };
    sceVu0FVECTOR pos;

    for (i = 0; i < bolt_count; i++) {
        // A bolt is one tall sprite stood between the two sparks it arcs
        // across, so each end gives a point above and a point below it.
        sceVu0CopyVector(pos, offset[bolt_head[i]]);
        pos[1] += 1.0f;
        MGRotTransPers(corner[0], pos, 0);
        pos[1] -= 2.0f;
        MGRotTransPers(corner[1], pos, 0);
        sceVu0CopyVector(pos, offset[bolt_tail[i]]);
        pos[1] += 1.0f;
        MGRotTransPers(corner[2], pos, 0);
        pos[1] -= 2.0f;
        MGRotTransPers(corner[3], pos, 0);

        int shape = bolt_frame[i];
        int u = bolt_uv[shape][0];
        int v = bolt_uv[shape][1];

        set3DSprite(Vif1Packet, texture, CRect_i_(u, v, 0x18, 0x68), corner[0], corner[1], corner[2], corner[3], (u8) (1.6f * alpha[bolt_head[i]]));
    }
}
