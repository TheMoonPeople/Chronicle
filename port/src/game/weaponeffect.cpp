#include "weaponeffect.hpp"

#include <libvu0.h>

#include "frame.hpp"
#include "mglib.hpp"
#include "rect.hpp"
#include "snd.hpp"

void CWeaponEffect::InitSet(CFrame *frame, char *root_name, char *tip_name) {
    tip = NULL;
    root = NULL;

    if (frame != NULL) {
        root = frame->SearchFrame(root_name);
        tip = frame->SearchFrame(tip_name);
    }
}

void CWeaponEffect::Set(int hold, float fade, float fade_out) {
    sceVu0FVECTOR centre = {0.0f, 0.0f, 0.0f, 1.0f};

    if (root == NULL || tip == NULL) {
        return;
    }

    this->hold = hold;
    this->fade = fade;
    strength = 255.0f;
    timer = 0;
    this->fade_out = fade_out;

    root->GetWorldPosition(edge[cursor * 2], centre);
    tip->GetWorldPosition(edge[cursor * 2 + 1], centre);
    edge[cursor * 2 + 1][3] = 1.0f;
    edge[cursor * 2][3] = 1.0f;
    alpha[cursor] = 128.0f;
    used[cursor] = 1;

    cursor++;

    if (cursor >= WEAPON_EFFECT_STEP_MAX) {
        cursor = 0;
    }
}

void CWeaponEffect::Step() {
    sceVu0FVECTOR centre = {0.0f, 0.0f, 0.0f, 1.0f};
    int           i;

    if (root == NULL || tip == NULL) {
        return;
    }

    // The trail is laid down at full strength while the swing is held, and
    // weakens once it is over.
    if (timer < hold) {
        timer = timer + 1;
    } else {
        strength -= fade_out;

        if (strength <= 0.0f) {
            strength = 0.0f;
        }
    }

    if (!(strength <= 0.0f)) {
        cursor++;

        if (cursor >= WEAPON_EFFECT_STEP_MAX) {
            cursor = 0;
        }

        root->GetWorldPosition(edge[cursor * 2], centre);
        tip->GetWorldPosition(edge[cursor * 2 + 1], centre);
        edge[cursor * 2 + 1][3] = 1.0f;
        edge[cursor * 2][3] = 1.0f;
        alpha[cursor] = strength;
        used[cursor] = 1;
    }

    for (i = 0; i < WEAPON_EFFECT_STEP_MAX; i++) {
        if (used[i] != 0) {
            alpha[i] -= fade;

            if (alpha[i] <= 0.0f) {
                used[i] = 0;
            }
        }
    }
}

void CWeaponEffect::Draw() {
    int        near_root[4];
    int        near_tip[4];
    int        far_root[4];
    int        far_tip[4];
    sceGsAlpha alpha_reg;
    sceGsZbuf  zbuf;
    int        prev;
    int        i;

    if (root == NULL || tip == NULL) {
        return;
    }

    alpha_reg = mgAlpha;
    alpha_reg.bits.a = 0;
    alpha_reg.bits.b = 2;
    alpha_reg.bits.c = 0;
    alpha_reg.bits.d = 1;
    MGSetGsALPHA(&alpha_reg);

    zbuf = mgZBuffer;
    zbuf.bits.zmsk = 1;
    MGSetGsZBUF(&zbuf);

    // Every piece of the ribbon spans one step and the one before it, so the
    // first piece reaches back around to the last slot.
    for (i = 0; i < WEAPON_EFFECT_STEP_MAX; i++) {
        if (i == 0) {
            prev = WEAPON_EFFECT_STEP_MAX - 1;
        } else {
            prev = i - 1;
        }

        if (used[i] != 0 && used[prev] != 0 && MGRotTransPers(near_root, edge[prev * 2], 0) != 0 && MGRotTransPers(near_tip, edge[prev * 2 + 1], 0) != 0 && MGRotTransPers(far_root, edge[i * 2], 0) != 0 && MGRotTransPers(far_tip, edge[i * 2 + 1], 0) != 0) {
            colour[0].a = (u8) alpha[prev];
            colour[1].a = (u8) alpha[prev];
            colour[2].a = (u8) alpha[i];
            colour[3].a = (u8) alpha[i];
            set3DColSprite(Vif1Packet, near_root, far_root, near_tip, far_tip, &colour[0], &colour[2], &colour[1], &colour[3]);
        }
    }

    MGSetGsALPHA(NULL);
    MGSetGsZBUF(NULL);
}
