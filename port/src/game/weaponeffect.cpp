#include "weaponeffect.hpp"

#include <libvu0.h>

#include <array>

#include "frame.hpp"
#include "mglib.hpp"
#include "rect.hpp"
#include "snd.hpp"

namespace dc::game {

void CWeaponEffect::InitSet(CFrame *frame, char *root_name, char *tip_name) {
    tip = nullptr;
    root = nullptr;

    if (frame != nullptr) {
        root = frame->SearchFrame(root_name);
        tip = frame->SearchFrame(tip_name);
    }
}

void CWeaponEffect::Set(int hold, float fade, float fade_out) {
    sceVu0FVECTOR centre = {0.0f, 0.0f, 0.0f, 1.0f};

    if (root == nullptr || tip == nullptr) {
        return;
    }

    this->hold = hold;
    this->fade = fade;
    strength = 255.0f;
    timer = 0;
    this->fade_out = fade_out;

    root->GetWorldPosition(edge[cursor * 2], centre);
    tip->GetWorldPosition(edge[(cursor * 2) + 1], centre);
    edge[(cursor * 2) + 1][3] = 1.0f;
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

    if (root == nullptr || tip == nullptr) {
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
        tip->GetWorldPosition(edge[(cursor * 2) + 1], centre);
        edge[(cursor * 2) + 1][3] = 1.0f;
        edge[cursor * 2][3] = 1.0f;
        alpha[cursor] = strength;
        used[cursor] = 1;
    }

    for (int step = 0; step < WEAPON_EFFECT_STEP_MAX; step++) {
        if (used[step] != 0) {
            alpha[step] -= fade;

            if (alpha[step] <= 0.0f) {
                used[step] = 0;
            }
        }
    }
}

void CWeaponEffect::Draw() {
    std::array<int, 4> near_root{};
    std::array<int, 4> near_tip{};
    std::array<int, 4> far_root{};
    std::array<int, 4> far_tip{};

    if (root == nullptr || tip == nullptr) {
        return;
    }

    sceGsAlpha alpha_reg = mgAlpha;
    alpha_reg.bits.a = 0;
    alpha_reg.bits.b = 2;
    alpha_reg.bits.c = 0;
    alpha_reg.bits.d = 1;
    MGSetGsALPHA(&alpha_reg);

    sceGsZbuf zbuf = mgZBuffer;
    zbuf.bits.zmsk = 1;
    MGSetGsZBUF(&zbuf);

    // Every piece of the ribbon spans one step and the one before it, so the
    // first piece reaches back around to the last slot.
    for (int step = 0; step < WEAPON_EFFECT_STEP_MAX; step++) {
        const int prev = step == 0 ? WEAPON_EFFECT_STEP_MAX - 1 : step - 1;

        if (used[step] != 0 && used[prev] != 0 && MGRotTransPers(near_root.data(), edge[prev * 2], 0) != 0 &&
            MGRotTransPers(near_tip.data(), edge[(prev * 2) + 1], 0) != 0 &&
            MGRotTransPers(far_root.data(), edge[step * 2], 0) != 0 &&
            MGRotTransPers(far_tip.data(), edge[(step * 2) + 1], 0) != 0) {
            colour[0].a = static_cast<u8>(alpha[prev]);
            colour[1].a = static_cast<u8>(alpha[prev]);
            colour[2].a = static_cast<u8>(alpha[step]);
            colour[3].a = static_cast<u8>(alpha[step]);
            set3DColSprite(Vif1Packet, near_root.data(), far_root.data(), near_tip.data(), far_tip.data(), &colour[0],
                           &colour[2], &colour[1], &colour[3]);
        }
    }

    MGSetGsALPHA(nullptr);
    MGSetGsZBUF(nullptr);
}

} // namespace dc::game
