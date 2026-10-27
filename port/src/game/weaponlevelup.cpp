#include "weaponlevelup.hpp"

#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <print>
#include <string>
#include <string_view>

#include "battlemenu.hpp"
#include "clsmes.hpp"
#include "dataalloc.hpp"
#include "dataread.hpp"
#include "dngstatusdata.hpp"
#include "itemdata.hpp"
#include "mainselect.hpp"
#include "menu_draw.hpp"
#include "menu_inventory.hpp"
#include "menu_misc.hpp"
#include "menuitemstep.hpp"
#include "savedata.hpp"
#include "shop.hpp"
#include "snd.hpp"
#include "texture.hpp"
#include "weapon_buildup.hpp"

namespace dc::game {

CDataAlloc2<1> MenuEffectCashBuffer(-1);

CWeaponLevelUp MenuWepLevelUp;

CMenuItemStep ItemVolumeStep;

namespace {

/** Effect file each WepEffectKind loads, indexing the name tables below. */
constexpr std::array<int, WEP_EFFECT_KIND_COUNT> kEffectFileNo = {0, 1, 2, 4, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3};

/** Sound effect set each WepEffectKind loads, or -1 for none. */
constexpr std::array<int, WEP_EFFECT_KIND_COUNT> kEffectSeNo = {25, 26, 28, -1, -1, -1, -1, 20, 20, -1, 20, 20, 20, 20};

constexpr std::array<std::string_view, 5> kEffectFileNames = {"wlevelup.pak", "s_break.pak", "buildup.chr", "menu_ex.chr", "w_recover.chr"};
constexpr std::array<std::string_view, 5> kEffectImageNames = {"wlevelup", "s_break", "buildup", "menu_ex", "w_recover"};
constexpr std::array<std::string_view, 5> kEffectConfigNames = {"wlevelup", "info", "buildup", "info", "w_recover"};

} // namespace

void AttachMentValuePlus(ATTACH_LIST *total, ATTACH_LIST *attach, float scale) {
    int sum = 0;

    WEAPON_DATA *weapon_data = GetWeaponData(total->sphere_weapon_no);

    std::array<int, 4> limit = {999, 99, 99, 999};

    if (weapon_data != nullptr) {
        limit[0] = weapon_data->attack_max;
        limit[3] = weapon_data->magic_max;
    }

    for (int i = 0; i < 4; i++) {
        sum = total->status[i] + static_cast<s16>(static_cast<int>(static_cast<float>(attach->status[i]) * scale));

        if (sum >= limit[i]) {
            total->status[i] = limit[i];
        } else {
            total->status[i] = sum;
        }
    }

    for (int i = 0; i < 5; i++) {
        sum = total->elem[i] + static_cast<int>(static_cast<float>(attach->elem[i]) * scale);

        if (sum >= 99) {
            total->elem[i] = 99;
        } else {
            total->elem[i] = sum;
        }
    }

    for (int i = 0; i < 10; i++) {
        sum = total->vs_monster[i] + static_cast<int>(static_cast<float>(attach->vs_monster[i]) * scale);

        if (sum >= 99) {
            total->vs_monster[i] = 99;
        } else {
            total->vs_monster[i] = sum;
        }
    }
}

void WeaponLevelUpValueCalc(WEAPON_HAVE *src, WEAPON_HAVE *dst, int levels, int /*unused*/) {
    ATTACH_LIST total{};
    int         sum = 0;

    if (src == nullptr) {
        return;
    }

    memset(dst, 0, sizeof(*dst));
    int synth_count = 0;
    int flags = 1;
    flags |= src->flags;
    WEAPON_DATA *weapon_data = GetWeaponData(src->item_no);

    // The inner loops share the level counter, so each pass leaves it at 10.
    int i = 0;

    for (i = 0; i < levels; i++) {
        for (i = 0; i < 6; i++) {
            if (weapon_data->hole[i] > 0) {
                ATTACH_LIST *attach = &src->attach[i];

                if (attach->item_no >= ITEM_ATTACH_START) {
                    float scale = 1.0f;

                    if (src->attach_kind[i] == 3) {
                        scale = 2.0f;
                    }

                    AttachMentValuePlus(&total, attach, scale);

                    if (attach->item_no == ITEM_ATTACH_SYNTHESIS_SPHERE) {
                        synth_count++;

                        if (attach->sphere_flags != 0 && attach->sphere_flags != 1) {
                            flags |= attach->sphere_flags;
                        }
                    }
                }
            }
        }

        memset(dst->attach, 0, sizeof(dst->attach));
        memcpy(dst, src, sizeof(*dst));
        dst->attack += total.status[0] + 1;

        if (dst->attack > weapon_data->attack_max) {
            dst->attack = weapon_data->attack_max;
        }

        dst->endurance += total.status[1] + 1;

        if (dst->endurance >= 99) {
            dst->endurance = 99;
        }

        dst->speed += total.status[2] + 1;

        if (dst->speed >= 99) {
            dst->speed = 99;
        }

        dst->magic += total.status[3] + 1;

        if (dst->magic >= weapon_data->magic_max) {
            dst->magic = weapon_data->magic_max;
        }

        for (i = 0; i < 5; i++) {
            sum = dst->elem[i] + total.elem[i];

            if (sum >= 99) {
                dst->elem[i] = 99;
            } else {
                dst->elem[i] = sum;
            }
        }

        for (i = 0; i < 10; i++) {
            sum = dst->vs_monster[i] + total.vs_monster[i];

            if (sum >= 99) {
                dst->vs_monster[i] = 99;
            } else {
                dst->vs_monster[i] = sum;
            }
        }

        dst->durability = src->durability + 1 + rand() % 3;

        if (dst->durability > 99) {
            dst->durability = 99;
        }

        bool has_double = false;

        for (int hole = 0; hole < 6; hole++) {
            if (weapon_data->hole[hole] > 0 && weapon_data->hole[hole] != 2) {
                if (has_double) {
                    dst->attach_kind[hole] = 0;
                } else if (rand() % 50 == 5) {
                    dst->attach_kind[hole] = 3;
                    has_double = true;
                } else {
                    dst->attach_kind[hole] = 0;
                }
            }
        }
    }

    dst->flags |= flags;
    dst->synthesis_count += synth_count;
}

void CWeaponLevelUp::CMenuEffectDataLoad(CWeaponLevelUp *load_buffer, int kind) {
    int size = 0;

    effect_buffer = reinterpret_cast<u_long128 *>(load_buffer);
    effect_buffer = MenuCalcBufAlignment(effect_buffer);

    int se_no = kEffectSeNo[kind];

    if (kind == WEP_EFFECT_BUILDUP && IsLastWeapon(buildup_weapon_no) != 0) {
        se_no = 21;
        std::println("last weapon");
    }

    std::string path = "commenu/effect/";
    path += kEffectFileNames[kEffectFileNo[kind]];
    StartReadBG();
    LoadFileBG(path.data(), effect_buffer, &size);

    if (0 <= se_no) {
        effect_buffer += (size >> 4) + 1;
        effect_buffer = MenuCalcBufAlignment(effect_buffer);
        SndSPSeLoadBG(se_no, reinterpret_cast<u_int *>(effect_buffer), &size);
        std::println("effect snd size = {}", size);
        effect_buffer += (size >> 4) + 1;
    }

    ReadBG();
}

void CWeaponLevelUp::Initialize() {
    memset(&preview, -1, sizeof(preview));
    // The status fields through unk_117 form one ATTACH_LIST-shaped record.
    memset(&status_item_no, 0, sizeof(ATTACH_LIST));
    effect.frame = nullptr;
    effect_buffer = nullptr;
    weapon = nullptr;
    reserved_word = 0;
    effect_active = false;
    effect_timer = -1.0f;
    effect_state = WEP_EFFECT_STATE_IDLE;
    operation_kind = WEP_EFFECT_NONE;
    texture_block = -1;
    buildup_weapon_no = -1;
    message_no = 0;
    message_value = 0;
    synthesis_count = 0;
    reserved_half = 0;
    buildup_complete = 0;
    lost_default_weapon = false;
}

void CWeaponLevelUp::SetLevelUpValue(WEAPON_HAVE *have, CCharacter *character, CWeaponLevelUp *load_buffer, int tex_block) {
    ATTACH_LIST total{};
    int         sum = 0;

    if (have == nullptr) {
        std::println("src is NULL");
        return;
    }

    texture_block = tex_block;
    operation_kind = WEP_EFFECT_LEVELUP;
    effect_motion = 0;
    CMenuEffectDataLoad(load_buffer, 0);
    synthesis_count = 0;
    chara = character;
    weapon = have;
    memcpy(&preview, weapon, sizeof(preview));
    WEAPON_DATA *weapon_data = GetWeaponData(preview.item_no);
    int          flags = 1;
    flags |= preview.flags;

    for (int i = 0; i < 6; i++) {
        if (weapon_data->hole[i] > 0) {
            ATTACH_LIST *attach = &preview.attach[i];

            if (preview.attach[i].item_no >= ITEM_ATTACH_START) {
                float scale = 1.0f;

                if (preview.attach_kind[i] == 3) {
                    scale = 2.0f;
                }

                AttachMentValuePlus(&total, attach, scale);

                if (attach->item_no == ITEM_ATTACH_SYNTHESIS_SPHERE) {
                    synthesis_count++;

                    if (attach->sphere_flags != 0 && attach->sphere_flags != 1) {
                        flags |= attach->sphere_flags;
                    }
                }
            }
        }
    }

    flags = CheckWeaponOptionStatus(flags);
    preview.flags |= flags;
    preview.attack += total.status[0];

    if (preview.attack > weapon_data->attack_max) {
        preview.attack = weapon_data->attack_max;
    }

    preview.endurance += total.status[1];

    if (preview.endurance > 99) {
        preview.endurance = 99;
    }

    preview.speed += total.status[2];

    if (preview.speed > 99) {
        preview.speed = 99;
    }

    preview.magic += total.status[3];

    if (preview.magic > weapon_data->magic_max) {
        preview.magic = weapon_data->magic_max;
    }

    for (int j = 0; j < 5; j++) {
        sum = preview.elem[j] + total.elem[j];

        if (sum >= 99) {
            preview.elem[j] = 99;
        } else {
            preview.elem[j] = sum;
        }
    }

    for (int j = 0; j < 10; j++) {
        sum = preview.vs_monster[j] + total.vs_monster[j];

        if (sum >= 99) {
            preview.vs_monster[j] = 99;
        } else {
            preview.vs_monster[j] = sum;
        }
    }

    if (preview.durability >= 99) {
        preview.durability = 99;
    }

    memset(attachment_icons, 0, sizeof(attachment_icons));
    memset(attachment_values, 0, sizeof(attachment_values));
    int icon_count = 0;

    for (int slot = 0; slot < 5; slot++) {
        if (preview.attach[slot].item_no >= ITEM_ATTACH_START) {
            attachment_icons[icon_count] = preview.attach[slot].item_no;
            attachment_values[icon_count] = 0;

            if (attachment_icons[icon_count] >= ITEM_ATTACH_STAT_START && attachment_icons[icon_count] < ITEM_ATTACH_GEM_START) {
                attachment_values[icon_count] = preview.attach[slot].status[preview.attach[slot].item_no - ITEM_ATTACH_STAT_START];
            }

            if (attachment_icons[icon_count] == ITEM_ATTACH_SYNTHESIS_SPHERE) {
                attachment_values[icon_count] = preview.attach[slot].sphere_weapon_no;
            }

            icon_count++;
        }
    }

    memset(weapon->attach, 0, sizeof(weapon->attach));
    memset(preview.attach, 0, sizeof(preview.attach));

    for (ATTACH_LIST &attach : weapon->attach) {
        attach.item_no = -1;
    }

    bool has_double = false;

    for (int hole = 0; hole < 5; hole++) {
        if (weapon_data->hole[hole] > 0 && weapon_data->hole[hole] != 2) {
            if (has_double) {
                preview.attach_kind[hole] = 0;
            } else if (rand() % 50 == 5) {
                preview.attach_kind[hole] = 3;
                has_double = true;
            } else {
                preview.attach_kind[hole] = 0;
            }
        }
    }

    memcpy(weapon, &preview, sizeof(*weapon));
    effect_state = WEP_EFFECT_STATE_LEVELUP_LOAD;
}

void CWeaponLevelUp::SetLevelUpWeaponData() {
    WEAPON_DATA *weapon_data = GetWeaponData(weapon->item_no);

    weapon->attack = weapon->attack + 1;

    if (weapon->attack > weapon_data->attack_max) {
        weapon->attack = weapon_data->attack_max;
    }

    weapon->endurance = weapon->endurance + 1;

    if (weapon->endurance > 99) {
        weapon->endurance = 99;
    }

    weapon->speed = weapon->speed + 1;

    if (weapon->speed > 99) {
        weapon->speed = 99;
    }

    weapon->magic = weapon->magic + 1;

    if (weapon->magic > weapon_data->magic_max) {
        weapon->magic = weapon_data->magic_max;
    }

    weapon->durability = weapon->durability + 1 + rand() % 3;

    if (weapon->durability > 99) {
        weapon->durability = 99;
    }

    weapon->experience = 0;
}

void CWeaponLevelUp::SetStatusBreak(WEAPON_HAVE *have, CCharacter *character, CWeaponLevelUp *load_buffer, int tex_block) {
    ATTACH_LIST total{};
    int         page = 0;

    if (have == nullptr) {
        std::println("src is NULL");
        return;
    }

    texture_block = tex_block;
    operation_kind = WEP_EFFECT_STATUS_BREAK;
    effect_state = WEP_EFFECT_STATE_BREAK_LOAD;
    effect_motion = 0;
    CMenuEffectDataLoad(load_buffer, 1);
    chara = character;
    weapon = have;
    memset(&status_item_no, 0, sizeof(ATTACH_LIST));
    status_item_no = ITEM_ATTACH_SYNTHESIS_SPHERE;
    status_weapon_no = weapon->item_no;
    status_weapon_level = weapon->level;

    const std::array<s16, 4> base_stats = {weapon->attack, weapon->endurance, weapon->speed, weapon->magic};

    for (int i = 0; i < 4; i++) {
        status_stats[i] = 0.6f * static_cast<float>(base_stats[i]);
    }

    float scale = 0.6f;

    for (int i = 0; i < 5; i++) {
        status_elements[i] = weapon->elem[i] * scale;
    }

    for (int i = 0; i < 10; i++) {
        status_monster[i] = weapon->vs_monster[i] * scale;
    }

    s16 flags = 0;
    flags |= weapon->flags;
    total.sphere_weapon_no = status_weapon_no;

    for (int i = 0; i < 6; i++) {
        if (weapon->attach[i].item_no >= ITEM_ATTACH_START) {
            scale = 1.0f;

            if (weapon->attach_kind[i] == 3) {
                scale = 2.0f;
            }

            AttachMentValuePlus(&total, &have->attach[i], scale);

            if (weapon->attach[i].item_no == ITEM_ATTACH_SYNTHESIS_SPHERE && weapon->attach[i].sphere_flags != 0 && weapon->attach[i].sphere_flags != 1) {
                flags |= weapon->attach[i].sphere_flags;
            }
        }
    }

    AttachMentValuePlus(reinterpret_cast<ATTACH_LIST *>(&status_item_no), &total, 0.6f);
    flags = CheckWeaponOptionStatus(flags);
    CDngStatusData *dng_status = SaveData->GetDngStatus();
    int             slot_no = GetBoardSpace(0x5A, &page);
    auto           *items = reinterpret_cast<ATTACH_LIST *>(dng_status->consumable_items);
    ATTACH_LIST    *slot = &items[slot_no];
    memcpy(slot, &status_item_no, sizeof(ATTACH_LIST));
    slot->item_no = ITEM_ATTACH_SYNTHESIS_SPHERE;
    items[slot_no].sphere_level = weapon->level;
    items[slot_no].sphere_weapon_no = weapon->item_no;
    items[slot_no].sphere_flags = flags;
}

void CWeaponLevelUp::SetBuildUp(WEAPON_HAVE *have, CCharacter *character, CWeaponLevelUp *load_buffer, int tex_block) {
    if (have == nullptr) {
        return;
    }

    texture_block = tex_block;
    operation_kind = WEP_EFFECT_BUILDUP;
    effect_state = WEP_EFFECT_STATE_BUILDUP_LOAD;
    effect_motion = 0;
    CMenuEffectDataLoad(load_buffer, operation_kind);
    chara = character;
    weapon = have;
    memcpy(&preview, weapon, sizeof(preview));
    SetWeaponBuildValue(&preview, buildup_weapon_no);

    if (have->item_no == ITEM_WEAPON_MARDAN_EINS || have->item_no == ITEM_WEAPON_MARDAN_TWEI) {
        ClearFishMardanGarayanNum();
        std::println("mardan num clear!!");
    }
}

void CWeaponLevelUp::WepRecover(WEAPON_HAVE *have, CCharacter *character, CWeaponLevelUp *load_buffer, int tex_block) {
    if (have == nullptr || character == nullptr || load_buffer == nullptr) {
        return;
    }

    texture_block = tex_block;
    operation_kind = WEP_EFFECT_RECOVER;
    effect_state = WEP_EFFECT_STATE_RECOVER_LOAD;
    effect_motion = 0;
    CMenuEffectDataLoad(load_buffer, operation_kind);
    chara = character;
    weapon = have;
}

void CWeaponLevelUp::CureEffect(int x, int y, CWeaponLevelUp *load_buffer, int tex_block, int kind) {
    CMenuEffectDataLoad(load_buffer, kind);
    texture_block = tex_block;
    operation_kind = kind;
    effect_state = (operation_kind - WEP_EFFECT_CURE_HEAL_HP) * 2 + WEP_EFFECT_STATE_CURE_HEAL_HP_LOAD;
    effect_motion = operation_kind - WEP_EFFECT_CURE_HEAL_HP;

    if (kind == WEP_EFFECT_CURE_POCKET) {
        effect_motion = 2;
    } else {
        if (operation_kind >= WEP_EFFECT_UNK_7) {
            effect_motion = effect_motion - 1;
        }

        if (operation_kind >= WEP_EFFECT_CURE_REVIVAL_POWDER) {
            effect_motion = effect_motion - 1;
        }
    }

    effect_x = static_cast<float>(x);
    effect_y = static_cast<float>(y);
}

void CWeaponLevelUp::InitSnd() {
    snd_from = -1;
    snd_volume = 0;
    snd_to = 0;
    snd_step = 0;
}

void CWeaponLevelUp::SetSnd(int from, int to, int step) {
    snd_from = from;
    snd_volume = snd_from;
    snd_to = to;

    if (to < from) {
        snd_step = -step;
    }

    if (from < to) {
        snd_step = step;
    }

    if (DebugMode) {
        std::println("menu snd para:");
        std::println("        start:{}", snd_from);
        std::println("          now:{}", snd_volume);
        std::println("       limmit:{}", snd_to);
        std::println("         step:{}", snd_step);
    }
}

void CWeaponLevelUp::StepSnd() {
    if (snd_step != 0) {
        snd_volume = snd_volume + snd_step;
        SndSetBgmVol(snd_volume);

        if (snd_step > 0 && snd_to <= snd_volume) {
            snd_volume = snd_to;
            snd_step = 0;
        }

        if (snd_step < 0 && snd_to >= snd_volume) {
            snd_volume = snd_to;
            snd_step = 0;
        }
    }
}

void CWeaponLevelUp::CheckSnd() {
    std::println("snd check");

    if (snd_from > 0 || snd_step != 0) {
        const int volume = SndGetBgmVol();

        if (volume != snd_volume) {
            SndSetBgmVol(volume);
        }
    }

    InitSnd();
}

void CWeaponLevelUp::Step() {
    if (operation_kind == WEP_EFFECT_NONE) {
        return;
    }

    int motion_state = 0;

    if (effect.frame != nullptr) {
        motion_state = effect.motion_state;
    }

    switch (effect_state) {
        default:
            break;
        case WEP_EFFECT_STATE_IDLE:
            return;
        case WEP_EFFECT_STATE_LEVELUP_LOAD:
        case WEP_EFFECT_STATE_BREAK_LOAD:
        case WEP_EFFECT_STATE_BUILDUP_LOAD:
        case WEP_EFFECT_STATE_RECOVER_LOAD:
        case WEP_EFFECT_STATE_CURE_HEAL_HP_LOAD:
        case WEP_EFFECT_STATE_CURE_DRINK_LOAD:
        case WEP_EFFECT_STATE_CURE_REPAIR_LOAD:
        case WEP_EFFECT_STATE_UNK_12:
        case WEP_EFFECT_STATE_CURE_STATUS_LOAD:
        case WEP_EFFECT_STATE_CURE_AILMENT_LOAD:
        case WEP_EFFECT_STATE_CURE_REVIVAL_POWDER_LOAD:
        case WEP_EFFECT_STATE_CURE_GOURD_LOAD:
        case WEP_EFFECT_STATE_CURE_FRUIT_OF_EDEN_LOAD:
        case WEP_EFFECT_STATE_CURE_POCKET_LOAD: {
            ReadBG();
            int ready_count = 0;

            if (ReadBGSync() == 0) {
                ready_count++;
            }

            if (SndSPSeSyncBG() == 0) {
                ready_count++;

                if (operation_kind != WEP_EFFECT_CURE_REPAIR && operation_kind != WEP_EFFECT_RECOVER && operation_kind != WEP_EFFECT_CURE_AILMENT && operation_kind != WEP_EFFECT_CURE_HEAL_HP && operation_kind != WEP_EFFECT_CURE_DRINK) {
                    const int volume = BtlMenuBGMvol;
                    SetSnd(volume, volume >> 2, 7);
                }
            }

            if (ready_count >= 2) {
                std::array<LOADTEXTURE_INFO2, 3> textures = {
                    {
                     {.name = const_cast<char *>("#frame_menu_level#640#" SCREEN_HEIGHT_STR "#4"), .block_no = 0, .mipmap = 0},
                     {.name = nullptr, .block_no = 0, .mipmap = 0},
                     {.name = nullptr, .block_no = 0, .mipmap = 0},
                     }
                };
                textures[0].block_no = texture_block;
                textures[1].block_no = texture_block;

                const int   file_no = kEffectFileNo[operation_kind];
                std::string config_name{kEffectConfigNames[file_no]};
                std::string image_name{kEffectImageNames[file_no]};
                image_name += ".img";
                config_name += ".cfg";
                BG_READ_INFO *read_info = GetReadBGFile(0);
                int           size = 0;
                textures[1].name = reinterpret_cast<char *>(GetPackFile(reinterpret_cast<u_int *>(read_info->buffer), image_name.data(), &size));
                TexManager.DeleteTextureBlock(texture_block);
                TexManager.CleanUpTextureList();
                TexManager.LoadTextureBlockEX(-1, textures.data());
                auto      *pack_data = reinterpret_cast<u_int *>(read_info->buffer);
                u_long128 *cache_base = read_info->buffer + (read_info->size >> 4) + 1;
                cache_base = MenuCalcBufAlignment(cache_base);
                effect.Initialize();
                MenuEffectCashBuffer.base = reinterpret_cast<u8 *>(cache_base);
                MenuEffectCashBuffer.limit = 0x9100;
                MenuEffectCashBuffer.used = 0;
                effect.LoadPackData(pack_data, config_name.data(), &MenuEffectCashBuffer, &MenuEffectCashBuffer);
                effect_buffer = reinterpret_cast<u_long128 *>((MenuEffectCashBuffer.used << 4) + MenuEffectCashBuffer.base);
                effect_state++;
                effect_timer = 0.0f;
                effect_active = true;

                std::array<float, 4> position = {0.0f, 0.0f, 1.0f, 1.0f};

                switch (operation_kind) {
                    case WEP_EFFECT_CURE_HEAL_HP:
                    case WEP_EFFECT_CURE_DRINK:
                    case WEP_EFFECT_CURE_REPAIR:
                    case WEP_EFFECT_CURE_STATUS:
                    case WEP_EFFECT_CURE_AILMENT:
                    case WEP_EFFECT_CURE_REVIVAL_POWDER:
                    case WEP_EFFECT_CURE_GOURD:
                    case WEP_EFFECT_CURE_FRUIT_OF_EDEN:
                    case WEP_EFFECT_CURE_POCKET:
                        position[0] = effect_x;
                        position[1] = effect_y;
                        break;
                    default:
                        break;
                }

                effect.SetPosition(position.data());

                std::array<float, 3> scale = {1.0f, 1.0f, 1.0f};

                switch (operation_kind) {
                    case WEP_EFFECT_STATUS_BREAK:
                        scale.fill(1.38f);
                        break;
                    case WEP_EFFECT_BUILDUP:
                        scale.fill(2.0f);
                        break;
                    default:
                        break;
                }

                effect.SetScale(scale.data());
                effect.motion_no = effect_motion;
                effect.motion_flags = 6;
                effect.motion_speed = -1.0f;
                effect.Step();
                int se_no = -1;

                switch (operation_kind) {
                    case WEP_EFFECT_STATUS_BREAK:
                        se_no = 0x1A;
                        break;
                    case WEP_EFFECT_LEVELUP:
                        se_no = 0x19;
                        break;
                    case WEP_EFFECT_BUILDUP:
                        se_no = 0x1C;

                        if (IsLastWeapon(buildup_weapon_no) != 0) {
                            se_no = 0x15;
                        }

                        break;
                    case WEP_EFFECT_CURE_GOURD:
                    case WEP_EFFECT_CURE_FRUIT_OF_EDEN:
                    case WEP_EFFECT_CURE_POCKET:
                    case WEP_EFFECT_CURE_STATUS:
                    case WEP_EFFECT_UNK_7:
                    case WEP_EFFECT_CURE_REVIVAL_POWDER:
                        se_no = 0x14;
                        break;
                    default:
                        break;
                }

                if (se_no >= 0) {
                    SndSPSePlay(se_no, -1);
                }

                for (int i = 0; i < 4; i++) {
                    CommonMenuMes1.mes_no[i] = -1;
                    CommonMenuMes1.line_pos[i].x = -1;
                    CommonMenuMes1.line_pos[i].y = -1;
                }

                return;
            }

            break;
        }
        case WEP_EFFECT_STATE_LEVELUP_ORBIT:
            effect_timer += 1.0f;

            if (!(effect_timer < 140.0f)) {
                effect_state = WEP_EFFECT_STATE_LEVELUP_DONE;
                return;
            }

            break;
        case WEP_EFFECT_STATE_LEVELUP_DONE:
            effect_timer += 1.0f;

            if (motion_state == 3) {
                effect_timer = 200.0f;
                effect_active = false;
                return;
            }

            break;
        case WEP_EFFECT_STATE_BREAK_PLAY:
            effect_timer += 1.0f;

            if (motion_state == 3) {
                effect_motion++;
                effect.motion_no = effect_motion;
                effect.motion_flags = 6;
                effect.motion_speed = -1.0f;
                effect_state++;
                effect_timer = 0.0f;
                bool discard = true;

                if (IsDefaultWeapon(weapon->item_no) >= 0) {
                    discard = false;
                    weapon->level = 0;
                    lost_default_weapon = true;
                }

                if (discard) {
                    memset(weapon, 0, sizeof(*weapon));
                    weapon->item_no = -1;
                    return;
                }

                WepDataListToHaveCopy(weapon->item_no, weapon);
                return;
            }

            break;
        case WEP_EFFECT_STATE_BREAK_DONE:
            effect_timer += 1.0f;

            if (motion_state == 3) {
                effect_timer = 12.0f;
                effect_active = false;
                return;
            }

            break;
        case WEP_EFFECT_STATE_BUILDUP_PLAY:
            if (motion_state == 3) {
                effect_motion++;
                SetWeaponBuildValue(weapon, buildup_weapon_no);
                weapon->item_no = buildup_weapon_no;
                weapon->level = 0;
                weapon->experience = 0;
                int option = DefaultWeaponOptionSet(buildup_weapon_no);

                if (option != 1) {
                    option = CheckWeaponOptionStatus(option);
                    weapon->flags = weapon->flags | option;
                }

                MenuExTextureReadFlag = MENU_EX_TEX_UNREAD;
                buildup_complete = 1;
                effect.motion_no = effect_motion;
                effect.motion_flags = 6;
                effect.motion_speed = -1.0f;
                std::println("effect setmotion tuuka ");
                effect_state++;
                return;
            }

            break;
        case WEP_EFFECT_STATE_BUILDUP_DONE:
            if (motion_state == 3) {
                effect_active = false;
                return;
            }

            break;
        case WEP_EFFECT_STATE_RECOVER_DONE:
            if (motion_state == 3) {
                effect_active = false;
                return;
            }

            break;
        case WEP_EFFECT_STATE_CURE_DRINK_PLAY: {
            const float motion_time = effect.motion_type.state.time;

            if (motion_time > 52.0 && motion_time < 52.3) {
                ComMenuSePlay(SE_EFFECT_CHIME);
            }
        }
            [[fallthrough]];
        case WEP_EFFECT_STATE_CURE_HEAL_HP_PLAY:
        case WEP_EFFECT_STATE_CURE_REPAIR_PLAY:
        case WEP_EFFECT_STATE_UNK_13:
        case WEP_EFFECT_STATE_CURE_AILMENT_PLAY:
            if (motion_state == 3) {
                if (message_no >= 0x190) {
                    effect_active = false;
                    return;
                }

                effect_active = true;
                Initialize();
                return;
            }

            break;
        case WEP_EFFECT_STATE_CURE_STATUS_PLAY:
        case WEP_EFFECT_STATE_CURE_REVIVAL_POWDER_PLAY:
        case WEP_EFFECT_STATE_CURE_GOURD_PLAY:
        case WEP_EFFECT_STATE_CURE_FRUIT_OF_EDEN_PLAY:
        case WEP_EFFECT_STATE_CURE_POCKET_PLAY:
            if (motion_state == 3) {
                effect_active = false;
            }

            break;
    }
}

void CWeaponLevelUp::Draw() {
    if (effect_state == WEP_EFFECT_STATE_IDLE) {
        return;
    }

    if (effect_timer < 0.0f) {
        return;
    }

    int alpha = 128;

    if (effect_state == WEP_EFFECT_STATE_BREAK_DONE && !(effect.GetNowTime() < 134.0f)) {
        MenuTextureReload(MenuShadowReadBlock);
        DrawIconParts(ITEM_ATTACH_SYNTHESIS_SPHERE, 0x132, 0xD0, 0, 0x280, alpha, status_weapon_no);
    }

    if (effect.frame != nullptr) {
        MenuTextureReload(texture_block);
        effect.Step();
        effect.Draw();
    }

    MenuTextureReload(BtlMenuReadBlock);

    switch (effect_state) {
        case WEP_EFFECT_STATE_IDLE:
        case WEP_EFFECT_STATE_LEVELUP_LOAD:
        case WEP_EFFECT_STATE_BREAK_LOAD:
        case WEP_EFFECT_STATE_BUILDUP_LOAD:
            return;
        case WEP_EFFECT_STATE_LEVELUP_ORBIT: {
            std::array<s16, 5> icons{};
            std::array<s16, 5> values{};
            int                count = 0;

            for (int i = 0; i < 5; i++) {
                if (attachment_icons[i] < ITEM_ATTACH_START) {
                    break;
                }

                icons[i] = attachment_icons[i];
                values[i] = attachment_values[i];
                count++;
            }

            if (count <= 0) {
                return;
            }

            const float angle_step = TWO_PI / static_cast<float>(count);
            const float angular_rate = angle_step / 140.0f;
            const float orbit_radius = 128.0f - 128.0f * (effect_timer / 140.0f);

            if (!(effect_timer < 100.0f)) {
                alpha = 128 - static_cast<int>(effect_timer - 100.0f) * 4;

                if (alpha < 0) {
                    alpha = 0;
                }
            }

            MenuTextureReload(MenuShadowReadBlock);

            for (int i = 0; i < count; i++) {
                const float angle = angular_rate * effect_timer + angle_step * static_cast<float>(i);
                float       x = orbit_radius * cosf(angle);
                float       y = orbit_radius * sinf(angle);
                x = 320.0f + x - 12.0f;
                y = SCREEN_HALF_HEIGHT_F + y - 12.0f;
                DrawIconParts(icons[i], static_cast<int>(x), static_cast<int>(y), 0, 0x280, alpha, values[i]);
            }

            break;
        }
        case WEP_EFFECT_STATE_LEVELUP_DONE:
        case WEP_EFFECT_STATE_BREAK_PLAY:
        case WEP_EFFECT_STATE_BREAK_DONE:
        case WEP_EFFECT_STATE_BUILDUP_PLAY:
        case WEP_EFFECT_STATE_BUILDUP_DONE:
        default:
            break;
    }

    DrawMes();
}

void CWeaponLevelUp::DrawMes() {
    if (effect_active == 0 && operation_kind != WEP_EFFECT_NONE) {
        switch (effect_state) {
            case WEP_EFFECT_STATE_IDLE:
            case WEP_EFFECT_STATE_LEVELUP_LOAD:
            case WEP_EFFECT_STATE_LEVELUP_ORBIT:
            case WEP_EFFECT_STATE_BREAK_LOAD:
            case WEP_EFFECT_STATE_BREAK_PLAY:
            case WEP_EFFECT_STATE_BUILDUP_LOAD:
                return;
            default:
                break;
        }

        int                mes_id = 0;
        std::array<int, 2> mes_no = {-1, -1};
        std::array<int, 2> values = {-1, -1};

        switch (effect_state) {
            case WEP_EFFECT_STATE_LEVELUP_DONE:
                mes_id = 0x190;
                mes_no[0] = GetCommonItemInfo(preview.item_no)->msg + 100;
                values[0] = preview.level + 1;
                break;
            case WEP_EFFECT_STATE_BREAK_DONE:
                mes_id = 0x191;
                mes_no[0] = GetCommonItemInfo(status_weapon_no)->msg + 100;
                values[0] = status_weapon_level;
                break;
            case WEP_EFFECT_STATE_BUILDUP_DONE:
                mes_id = 0x192;
                mes_no[0] = GetCommonItemInfo(weapon->item_no)->msg + 100;
                values[0] = 0;
                break;
            case WEP_EFFECT_STATE_RECOVER_DONE:
                mes_id = 0x193;
                mes_no[0] = GetCommonItemInfo(weapon->item_no)->msg + 100;
                values[0] = weapon->level;
                break;
            case WEP_EFFECT_STATE_CURE_STATUS_PLAY:
                mes_id = 0x194;
                mes_no[0] = message_no + 50;
                values[0] = message_value;
                break;
            case WEP_EFFECT_STATE_CURE_AILMENT_PLAY:
                mes_id = message_no;
                break;
            case WEP_EFFECT_STATE_CURE_FRUIT_OF_EDEN_PLAY:
                mes_id = 0x195;
                mes_no[0] = message_no + 50;
                break;
            case WEP_EFFECT_STATE_CURE_GOURD_PLAY:
                mes_id = 0x196;
                mes_no[0] = message_no + 50;
                break;
            case WEP_EFFECT_STATE_CURE_POCKET_PLAY:
                mes_id = 0x197;
                values[0] = message_value;
                break;
            case WEP_EFFECT_STATE_CURE_REVIVAL_POWDER_PLAY:
                mes_id = 0x198;
                mes_no[0] = message_no + 50;
                break;
            default:
                break;
        }

        if (CommonMenuMes1.mes_made != mes_id || CommonMenuMes1.mes_no[0] != mes_no[0] || CommonMenuMes1.values[0] != values[0]) {
            CommonMenuMes1.value_signed = true;
            CommonMenuMes1.value_show = false;
            CommonMenuMes1.value_narrow = false;
            CommonMenuMes1.auto_pos = MES_POS_AUTO;

            if (effect_state == WEP_EFFECT_STATE_CURE_FRUIT_OF_EDEN_PLAY || effect_state == WEP_EFFECT_STATE_CURE_GOURD_PLAY || effect_state == WEP_EFFECT_STATE_CURE_POCKET_PLAY || effect_state == WEP_EFFECT_STATE_CURE_STATUS_PLAY || effect_state == WEP_EFFECT_STATE_CURE_AILMENT_PLAY) {
                CommonMenuMes1.value_signed = false;
                CommonMenuMes1.value_show = true;
                CommonMenuMes1.auto_pos = MES_POS_CENTRE;
            }

            CommonMenuMes1.mes_no[0] = mes_no[0];
            CommonMenuMes1.values[0] = values[0];
            CommonMenuMes1.mes_made = -1;
            CommonMenuMes1.MakeMesWin(mes_id);
        }

        CommonMenuMes1.stay_frame = true;
        MenuTextureReload(CommonMenuMes1.tex_block);
        int x = 0x164;
        int y = 0x96;

        if (effect_state == WEP_EFFECT_STATE_CURE_FRUIT_OF_EDEN_PLAY || effect_state == WEP_EFFECT_STATE_CURE_GOURD_PLAY || effect_state == WEP_EFFECT_STATE_CURE_POCKET_PLAY || effect_state == WEP_EFFECT_STATE_CURE_AILMENT_PLAY || effect_state == WEP_EFFECT_STATE_CURE_STATUS_PLAY) {
            x = 0x100;
            y = 0x96;
        }

        if (operation_kind == WEP_EFFECT_RECOVER) {
            CommonMenuMes1.value_signed = true;
            CommonMenuMes1.value_show = false;
            CommonMenuMes1.value_narrow = false;
            x = 0x100;
            y = 0x96;
            CommonMenuMes1.auto_pos = MES_POS_CENTRE;
        }

        DrawMenuClsMes(&CommonMenuMes1, x, y);
    }
}

} // namespace dc::game
