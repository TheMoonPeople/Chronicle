#include "menu_dungeon.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "dataread.hpp"
#include "dun/gameloop.hpp"
#include "gamepad.hpp"
#include "itemdata.hpp"
#include "memcard.hpp"
#include "menu_draw.hpp"
#include "menu_dungeon_port.hpp"
#include "menu_inventory.hpp"
#include "menuitemstep.hpp"
#include "nowload.hpp"
#include "rect.hpp"
#include "savedata.hpp"
#include "snd.hpp"
#include "texture.hpp"
#include "userstatus.hpp"
#include "weaponelement.hpp"

/* The debug item preview (EnterItemPolygonView) copies a whole vector into each of these, one float
   more than retail declares. MWCC gave every one of them a 16-byte slot, so on the PS2 the fourth
   float lands in padding; the port gives them the fourth float, as zero, the padding's value. */
PC_OVERRIDE float menudebugrot[4] = {0.0f, 0.0f, 0.0f, 0.0f};

PC_OVERRIDE float menudebugrscale[4] = {3.0f, 3.0f, 3.0f, 0.0f};

/* A save/menu slot is caller supplied. Reject bad attachment IDs and slot
   indices before looking up data or forming a pointer into the inventory. */
PC_OVERRIDE int SetAttachMentValue(int item_no, int slot, short level, ATTACH_LIST *unused) {
    (void) unused;

    if (slot < 0 || slot >= 43 || GetAttachData(item_no) == 0) {
        return -1;
    }

    if (SaveData == 0) {
        return -1;
    }

    CDngStatusData *status = SaveData->GetDngStatus();
    if (status == 0) {
        return -1;
    }

    DNG_CONSUMABLE *items = status->consumable_items;
    ATTACH_LIST *attach = (ATTACH_LIST *) &items[slot];
    s16 held_no = attach->item_no;
    if (GetAttachData(held_no) == 0) {
        return -1;
    }

    ATTACH_DATA *data = GetAttachData(item_no);
    memset(attach, 0, sizeof(*attach));
    memcpy(attach, data, sizeof(*attach));

    if (level <= 0) {
        level = 1;
    } else if (level > 3) {
        level = 3;
    }

    if (held_no >= ITEM_ATTACH_ATTACK && held_no <= ITEM_ATTACH_MAGICAL_POWER) {
        attach->status[held_no - ITEM_ATTACH_ATTACK] += level;
    }

    return 0;
}

PC_OVERRIDE int GetAttachVolumeForMsg(ATTACH_LIST *attach) {
    if (attach == 0 || GetAttachData(attach->item_no) == 0) {
        return 0;
    }

    if (attach->item_no >= ITEM_ATTACH_STAT_START && attach->item_no < ITEM_ATTACH_GEM_START) {
        return attach->status[attach->item_no - ITEM_ATTACH_STAT_START];
    }

    if (attach->item_no == ITEM_ATTACH_SYNTHESIS_SPHERE) {
        return attach->sphere_level;
    }

    return 0;
}

/* The dungeon's quick-change menu, opened with D-pad Up, as a picker for the equipped weapon's
   element: the ring holds a stone for each element the weapon's attachments give it and, where
   the weapon may go without one, a grey synth sphere for none. X takes the highlighted element,
   circle leaves. SELECT still opens the party.

   The picker rides on the character menu: StartQuickChange reads the menu's pack and sets the pad
   up as it does for the party, and the dungeon calls CharaChangeLoop each frame until it returns
   non-zero. The pack's quickchr.img holds wepicon, the item icon sheet, whose attachment icons
   start at cell 120: the five element stones of items 81 to 85, an empty cell for dummy item 86,
   and from cell 129 the synth sphere. The empty cell takes a grey copy of the sphere before the
   sheet is loaded. The chosen element is set the way the weapon menu sets it, on the equipped
   weapon's record, and applied as the dungeon applies a weapon change: SetWeaponAttachStatus
   rebuilds the live weapon and SetWeaponColor its trail. Ruby's shot is a model for each element,
   reloaded as a character load does. */

int CharaChangeLoopRetail();

extern CDngStatusData *ChangeStatusDataPt;
extern int             QuickCharaPos[2];
extern s16             CharaChangeReadFlag;
extern s16             CharaChangeTexBlock;
extern float           changeMenu_long;

namespace {

const int kIconSize = 32;
const int kCellSize = 48;
const int kStoneCell = 120;
const int kNoneCell = 125;
const int kSphereCell = 129;
const int kSheetWidth = 256;
const int kSheetHeight = 640;
const int kTurnFrames = 20;
// The character menu's exit after a change: its LOADING and LOADED steps count to 0x11 each frame.
const int kCloseFrames = 0x11;

enum PickerStep {
    PICKER_SELECT,
    PICKER_TURNING,
    PICKER_CLOSING,
};

struct ElementPicker {
    bool      requested;
    bool      loaded;
    bool      changed;
    int       count;
    s8        elements[6];
    int       selected;
    int       step;
    float     turn_frame;
    float     turn_direction;
    int       close_frame;
    int       ring_slot[6];
    float     ring_pos[6][2];
    CTexture *icons;
};

ElementPicker g_picker;

u32 Read32(const u8 *data) {
    return data[0] | (data[1] << 8) | (data[2] << 16) | ((u32) data[3] << 24);
}

u16 Read16(const u8 *data) {
    return data[0] | (data[1] << 8);
}

// Where a pixel of a PSMT8 image sits in its data, the GS's block order.
int Psmt8Offset(int x, int y, int width) {
    int block = (y & ~0xF) * width + (x & ~0xF) * 2;
    int swap = (((y + 2) >> 2) & 1) * 4;
    int row = (((y & ~3) >> 1) + (y & 1)) & 7;
    int column = row * width * 2 + ((x + swap) & 7) * 4;
    return block + column + ((y >> 1) & 1) + ((x >> 2) & 2);
}

// Where an index's colour sits in a PSMT8 image's CLUT, which the GS reads in CSM1 order.
int Csm1Index(int index) {
    return (index & ~0x18) | ((index & 0x08) << 1) | ((index & 0x10) >> 1);
}

// The wepicon sheet's pixels and CLUT in an IM2 bank, or false where the bank has no 256 by 640
// eight-bit sheet of that name.
bool FindIconSheet(u8 *bank, u8 **pixels, u8 **clut) {
    if (bank == NULL || memcmp(bank, "IM2", 3) != 0) {
        return false;
    }

    u32 count = Read32(bank + 4);

    for (u32 i = 0; i < count; i++) {
        u8 *entry = bank + 0x10 + i * 0x30;

        if (strncmp((char *) entry, "wepicon", 32) != 0) {
            continue;
        }

        u8 *tim = bank + Read32(entry + 0x20);
        u8 *picture = tim + 0x10;

        if (memcmp(tim, "TIM2", 4) != 0 || picture[0x13] != 5 || Read16(picture + 0x0E) != 256 || Read16(picture + 0x14) != kSheetWidth || Read16(picture + 0x16) != kSheetHeight) {
            return false;
        }

        *pixels = picture + Read16(picture + 0x0C);
        *clut = *pixels + kSheetWidth * kSheetHeight;
        return true;
    }

    return false;
}

// Copies the synth sphere's cell to the empty one with each pixel the nearest grey the CLUT has,
// alpha kept.
void MakeNoneIcon(u8 *bank) {
    u8 *pixels;
    u8 *clut;

    if (!FindIconSheet(bank, &pixels, &clut)) {
        printf("quickchr.img: no wepicon sheet; the element picker's none cell stays empty\n");
        return;
    }

    int from_x = kSphereCell % 8 * kIconSize;
    int from_y = kSphereCell / 8 * kIconSize;
    int to_x = kNoneCell % 8 * kIconSize;
    int to_y = kNoneCell / 8 * kIconSize;

    for (int y = 0; y < kIconSize; y++) {
        for (int x = 0; x < kIconSize; x++) {
            const u8 *colour = clut + Csm1Index(pixels[Psmt8Offset(from_x + x, from_y + y, kSheetWidth)]) * 4;
            int       grey = (colour[0] * 299 + colour[1] * 587 + colour[2] * 114) / 1000;
            int       alpha = colour[3];
            int       best = 0;
            int       best_distance = 0x7FFFFFFF;

            for (int i = 0; i < 256; i++) {
                const u8 *entry = clut + Csm1Index(i) * 4;
                int       r = entry[0] - grey;
                int       g = entry[1] - grey;
                int       b = entry[2] - grey;
                int       a = entry[3] - alpha;
                int       distance = r * r + g * g + b * b + 16 * a * a;

                if (distance < best_distance) {
                    best_distance = distance;
                    best = i;
                }
            }

            pixels[Psmt8Offset(to_x + x, to_y + y, kSheetWidth)] = (u8) best;
        }
    }
}

// The weapon menu's rule: Ruby's weapons and four of Osmond's guns cannot go without an element.
bool MayGoWithoutElement(int weapon_no) {
    if (WhoIsWeaponEquip(weapon_no) == CHARA_RUBY) {
        return false;
    }

    return weapon_no != ITEM_WEAPON_BLESSING_GUN && weapon_no != ITEM_WEAPON_SKUNK && weapon_no != ITEM_WEAPON_HEXA_BLASTER && weapon_no != ITEM_WEAPON_SUPERNOVA;
}

WEAPON_HAVE *EquippedWeapon() {
    CDngStatusData *status = SaveData->GetDngStatus();

    if (status == NULL) {
        return NULL;
    }

    int chara = status->cur_chara;
    int slot = status->equipped_weapon_slot[chara];
    return &status->chara_weapons[chara][slot];
}

void PlaceRing() {
    float step = TWO_PI / g_picker.count;
    float turn = 0.0f;

    if (g_picker.step == PICKER_CLOSING) {
        turn = step / kTurnFrames * g_picker.close_frame;
    } else if (g_picker.step == PICKER_TURNING) {
        turn = step / kTurnFrames * g_picker.turn_frame;

        if (g_picker.turn_direction < 0.0f) {
            turn = -turn;
        }
    }

    for (int i = 0; i < g_picker.count; i++) {
        float angle = PI + step * g_picker.ring_slot[i] + turn;
        g_picker.ring_pos[i][0] = changeMenu_long * cos(angle);
        g_picker.ring_pos[i][1] = changeMenu_long * sin(angle);
    }
}

// The elements on offer, in the order of the stones: those the weapon has, then none where the
// weapon may go without one or has nothing else. Returns how many.
int ElementChoices(s8 (&choices)[6]) {
    WEAPON_HAVE *weapon = EquippedWeapon();
    int          count = 0;

    for (int i = 0; i < 5; i++) {
        if (NowWeaponHave->elem[i] > 0) {
            choices[count++] = i;
        }
    }

    if (count == 0 || (weapon != NULL && MayGoWithoutElement(weapon->item_no))) {
        choices[count++] = WEAPON_ELEMENT_NONE;
    }

    return count;
}

// The elements on offer, with the weapon's own at the front of the ring.
void SetUpRing() {
    int current = NowWeaponHave->best_elem;

    g_picker.count = ElementChoices(g_picker.elements);

    g_picker.selected = 0;

    for (int i = 0; i < g_picker.count; i++) {
        if (g_picker.elements[i] == current || (g_picker.elements[i] == WEAPON_ELEMENT_NONE && (current < 0 || current > 4))) {
            g_picker.selected = i;
        }
    }

    for (int i = 0; i < g_picker.count; i++) {
        g_picker.ring_slot[i] = i - g_picker.selected;

        if (g_picker.ring_slot[i] < 0) {
            g_picker.ring_slot[i] += g_picker.count;
        }
    }

    g_picker.step = PICKER_SELECT;
    g_picker.turn_frame = 0.0f;
    g_picker.turn_direction = 0.0f;
    g_picker.close_frame = 0;
    g_picker.changed = false;
    PlaceRing();
}

// The menu's textures, once the pack is in: what the character menu's first frame loads, with the
// none cell made first.
bool LoadTextures() {
    if (ReadBGSync() != 0 || CharaChangeReadFlag != 0) {
        return CharaChangeReadFlag != 0;
    }

    BG_READ_INFO *file = GetReadBGFile(0);

    if (file == NULL) {
        return false;
    }

    LOADTEXTURE_INFO2 table[] = {
        {chara_change_frame_image, CharaChangeTexBlock, 0},
        {NULL,                     CharaChangeTexBlock, 0},
        {NULL,                     0,                   0},
    };
    table[1].name = (char *) GetPackFile((u_int *) file->buffer, (char *) "quickchr.img", NULL);
    MakeNoneIcon((u8 *) table[1].name);
    TexManager.DeleteTextureBlock(CharaChangeTexBlock);
    TexManager.CleanUpTextureList();
    TexManager.LoadTextureBlockEX(-1, table);
    g_picker.icons = TexManager.GetTexture((char *) "wepicon", -1);
    CharaChangeReadFlag = 1;
    return true;
}

void Turn(int direction) {
    g_picker.selected -= direction;

    if (g_picker.selected < 0) {
        g_picker.selected = g_picker.count - 1;
    }

    if (g_picker.selected >= g_picker.count) {
        g_picker.selected = 0;
    }

    g_picker.turn_direction = (float) direction;
    g_picker.turn_frame = 0.0f;
    g_picker.step = PICKER_TURNING;
    ComMenuSePlay(MENU_SOUND_CURSOR);
}

// 1 when the picker closes.
int PickerKey() {
    if (g_picker.step == PICKER_CLOSING) {
        g_picker.close_frame++;
        return g_picker.close_frame > kCloseFrames;
    }

    if (g_picker.step == PICKER_TURNING) {
        g_picker.turn_frame += 1.0f;

        if (g_picker.turn_frame > kTurnFrames) {
            for (int i = 0; i < g_picker.count; i++) {
                g_picker.ring_slot[i] += (int) g_picker.turn_direction;

                if (g_picker.ring_slot[i] >= g_picker.count) {
                    g_picker.ring_slot[i] = 0;
                }

                if (g_picker.ring_slot[i] < 0) {
                    g_picker.ring_slot[i] = g_picker.count - 1;
                }
            }

            g_picker.step = PICKER_SELECT;
            g_picker.turn_frame = 0.0f;
            g_picker.turn_direction = 0.0f;
        }

        PlaceRing();
        return 0;
    }

    if (GamePad.Down(PAD_CROSS)) {
        WEAPON_HAVE *weapon = EquippedWeapon();
        s8           element = g_picker.elements[g_picker.selected];

        if (weapon != NULL && weapon->best_elem != element) {
            weapon->best_elem = element;
            g_picker.changed = true;
        }

        // The ring leaves as the character menu's does after a change: spreading, turning and
        // fading out.
        ComMenuSePlay(MENU_SOUND_CONFIRM);
        g_picker.step = PICKER_CLOSING;
        g_picker.close_frame = 0;
        return 0;
    }

    if (GamePad.Down(PAD_CIRCLE)) {
        ComMenuSePlay(MENU_SOUND_REFUSE);
        return 1;
    }

    if (g_picker.count > 1) {
        if (GamePad.Down(PAD_UP | PAD_RIGHT)) {
            Turn(1);
        } else if (GamePad.Down(PAD_DOWN | PAD_LEFT)) {
            Turn(-1);
        }
    }

    return 0;
}

void PickerDraw() {
    setbilinear(0);
    FrameImageDraw(100, 0x80);

    if (CharaChangeReadFlag != 0 && g_picker.icons != NULL) {
        bool closing = g_picker.step == PICKER_CLOSING;
        int  alpha = closing ? std::max(0x80 - g_picker.close_frame * 4, 0) : 0x80;

        if (closing) {
            changeMenu_long += g_picker.close_frame;
            PlaceRing();
        }

        MenuTextureReload(CharaChangeTexBlock);

        for (int i = 0; i < g_picker.count; i++) {
            int element = g_picker.elements[i];
            int cell = element == WEAPON_ELEMENT_NONE ? kNoneCell : kStoneCell + element;
            int inset = (kCellSize - kIconSize) / 2;
            int x = QuickCharaPos[0] + g_picker.ring_pos[i][0] + inset;
            int y = QuickCharaPos[1] + g_picker.ring_pos[i][1] + inset;

            DrawMenu2DSprite(g_picker.icons, CRect_i_(x, y, kIconSize, kIconSize), CRect_i_(cell % 8 * kIconSize, cell / 8 * kIconSize, kIconSize, kIconSize), 0x80, 0x80, 0x80, alpha);
        }

        if (closing) {
            setbilinear(1);
            return;
        }

        DrawMenuWaku(QuickCharaPos[0] - changeMenu_long - 11.0f, QuickCharaPos[1] - 12, 0x34, 0x34, 0, StayTex, 0x80);
        CursorVibeCnt++;

        if (CursorVibeCnt > 1080000) {
            CursorVibeCnt = 0;
        }

        DrawMenuObjectVibe(QuickCharaPos[0] - changeMenu_long - 32.0f, QuickCharaPos[1] + 6, 1, 0x40);
    }

    setbilinear(1);
}

// What the character menu does on its way out, then the element applied to the live weapon.
void ClosePicker() {
    MenuTextureReload(CharaChangeTexBlock);
    DngActiveWeaponTextureCopy();
    CharaChangeReadFlag = 0;
    ItemVolumeStep.CheckItemVolume();
    GamePad.MenuModeOff();
    GamePad.AutoRepeatOff();
    TexManager.DeleteTextureBlock(CharaChangeTexBlock);
    TexManager.CleanUpTextureList();
    LOADTEXTURE_INFO2 restore[] = {
        {chara_change_frame_image, CharaChangeTexBlock, 0},
        {NULL,                     0,                   0},
    };
    TexManager.LoadTextureBlockEX(-1, restore);
    g_picker.requested = false;
    g_picker.loaded = false;

    if (!g_picker.changed) {
        return;
    }

    SetWeaponAttachStatus(NowWeaponHave);
    SetWeaponColor();
    int chara = UserStatus->cur_chara;

    if (chara == CHARA_RUBY) {
        char            path[64];
        BT_SHOT_EFFECT *effect = Get_Main_EffectPtr(chara, NowWeaponHave->best_elem);

        snprintf(path, sizeof(path), "dun/mainchara/wep_eff/%s.chr", (char *) effect);
        LoadFile(path, read_buffer, NULL);
        wait_now_loading_vsync();
        MainChara_Effect(effect, read_buffer, 0);
    }
}

} // namespace

bool ElementPickerHasChoice() {
    s8 choices[6];
    return ElementChoices(choices) >= 2;
}

void QuickChangeOpenElements(bool elements) {
    g_picker.requested = elements;
    g_picker.loaded = false;
}

int CharaChangeLoop() {
    if (!g_picker.requested) {
        return CharaChangeLoopRetail();
    }

    if (!g_picker.loaded) {
        SetUpRing();
        g_picker.loaded = true;
    }

    int result = 0;

    if (LoadTextures()) {
        result = PickerKey();
    }

    PickerDraw();
    ItemVolumeStep.LoopStep(60);

    if (result != 0) {
        ClosePicker();
    }

    return result;
}
