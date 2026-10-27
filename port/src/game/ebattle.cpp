#include "common.h"

#include <libvu0.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <numbers>
#include <print>

#include "camera.hpp"
#include "camerafollow.hpp"
#include "character.hpp"
#include "clsmes.hpp"
#include "dataalloc.hpp"
#include "dataread.hpp"
#include "dataset.hpp"
#include "ebattle.hpp"
#include "edit.hpp"
#include "editground.hpp"
#include "editloop.hpp"
#include "editloop3.hpp"
#include "effectgroup.hpp"
#include "effectmacro.hpp"
#include "fish.hpp"
#include "fishing.hpp"
#include "gamepad.hpp"
#include "gameutil.hpp"
#include "input/mouse_look.hpp"
#include "mainselect.hpp"
#include "mapparts.hpp"
#include "mathutil.hpp"
#include "menu_save.hpp"
#include "mglib.hpp"
#include "npcharacter.hpp"
#include "platform/config.hpp"
#include "platform/input.hpp"
#include "rect.hpp"
#include "savedata.hpp"
#include "shop.hpp"
#include "snd.hpp"
#include "sysmes.hpp"
#include "texture.hpp"

namespace dc::game {

using namespace dc::input;
using namespace dc::platform;

/**
 * Stores one event-battle key and its timing state.
 */
struct EB_KEY {
    int frame;     /**< Battle frame the prompt reaches the hit mark on. */
    int buttons;   /**< Buttons the prompt asks for. */
    int mode;      /**< How early the prompt lights before the hit mark; 0 never. */
    int pressed;   /**< Buttons pressed so far while the prompt is live. */
    int hit;       /**< Whether the prompt has been answered. */
    int passed;    /**< Whether the prompt has gone past the hit mark. */
    int highlight; /**< Whether the prompt is drawn lit this frame. */
};

static_assert(sizeof(EB_KEY) == 0x1C);

CRect_i_ draw_rect;

CRect_i_ Caution(256, 0, 56, 56);

/** Stores the key state for each event-battle prompt. */
EB_KEY eb_key[64];

/** Whether the prompts light up early. */
int eb_cool_flag = 1;
/** Battle time of the previous update. */
float old_time = -1.0f;
/** Battle frames advanced on each update. */
float speed = 1.0f;

/** Whether an event battle is running. */
int ebattle_flag;
/** Frames since the event battle started. */
int eb_count;
/** Frames since the last prompt finished. */
int eb_finish_cnt;
/** Frames since the event battle ended. */
int eb_end_count;
/** How the event battle ended. */
int eb_result;
/** Number of prompts answered. */
int eb_key_count;
/** Whether the background music fades when the battle ends. */
int fade_bgm;
/** Whether the fanfare plays when the battle ends. */
int play_fanfare;
/** Current battle time. */
float now_time;
/** Diagnostic display mode. */
int debug_mode;
/** Number of prompts in the sequence. */
int eb_key_num;
/** Frames the answer mark has been drawn. */
int ok_draw_cnt;
/** Kind of answer mark drawn. */
int ok_type;
/** Button the answer mark is drawn over. */
int ok_effect_button;
/** Whether the editor camera looks from the character's eyes. */
int viewMode;
/** Movement mode of the editor character. */
int chara_mode;
/** Step of the editor character's fishing. @see EdFishingState. */
int chara_fishing;
/** Step of the fishing catch message. @see EdFishingMessageStep. */
int fishing_mes;
/** Horizontal angle of the first-person view. */
float viewAngleH;
/** Vertical angle of the first-person view. */
float viewAngleV;

ED_MOVE_CHARA_INFO EdMoveCharaInfo;

/** Closest the editor camera comes to the character. */
float camera_near_dist = 70.0f;
/** Farthest the editor camera goes from the character. */
float camera_far_dist = 80.0f;

void draw_ok_loop();

namespace {

/** Streamed sound slot of the event battle's victory fanfare. */
constexpr int kFanfareSound = 0x2F;

/** First-person view's turn per tick at full stick deflection, in radians. */
constexpr float kTurnRate = 0.02f;

/**
 * Stores one motion and its frame range for the event-battle sequence.
 */
struct EB_MOTION {
    int   motion_no; /**< Motion requested from the character. */
    float start;     /**< First frame of the motion. */
    float end;       /**< Frame after the motion ends. */
    float speed;     /**< Frames advanced on each update. */
    int   frames;    /**< Number of updates in the motion. */
};

/**
 * Stores one rectangle of a prompt's sprite set, zeroing its members from last to first.
 */
class CEbSpriteRect : public CRect_i_ {
public:
    CEbSpriteRect() {
        height = 0;
        width = 0;
        y = 0;
        x = 0;
    }
};

/** Texture rectangles the caution prompt draws from. */
CEbSpriteRect CautionRect[7];
/** Texture rectangles the first result banner draws from. */
CEbSpriteRect EbResult0[7];
/** Texture rectangles the third result banner draws from. */
CEbSpriteRect EbResult2[7];
/** Texture rectangles the second result banner draws from. */
CEbSpriteRect EbResult1[7];
/** Texture rectangles the OK prompt draws from. */
CEbSpriteRect OkRect[7];
/** Texture rectangles the COOL prompt draws from. */
CEbSpriteRect CoolRect[7];

/** Motion sequence of the event battle; a negative motion number ends it. */
EB_MOTION eb_motion[32] = {{.motion_no = -1}};

/** Index of the prompt currently being answered. */
int now_eb_key = -1;
/** Whether the event battle's opening is running. */
int ebattle_intro_flag;
/** Frames since the opening started. */
int eb_intro_cnt;
/** Index of the button prompt being drawn. */
int now_button_no;
/** Character the event battle is played by. */
CCharacter *eb_chara;
/** Frames since the last sound was played. */
int sound_cnt;
/** Event-battle texture. */
CTexture *tex;
/** Second event-battle texture. */
CTexture *tex2;

/** Editor input modes that currently own the pad. */
int key_mode = 0xFFFF;

void  init_draw_ok();
void  set_draw_ok(int type, int button);
void  DrawButton(int buttons, int x, int y, float scale, int early);
void  DrawButtonSub(int x, int y, int texture_x, int texture_y, float scale);
void  draw_ok(int x);
float button_scale(int button);

} // namespace

void EBInitialize() {
    ebattle_flag = 0;

    // Each prompt's sprite set holds one texture rectangle per language.
    CautionRect[0].x = 256;
    CautionRect[0].y = 0;
    CautionRect[0].width = 56;
    CautionRect[0].height = 56;
    CautionRect[1].x = 256;
    CautionRect[1].y = 0;
    CautionRect[1].width = 56;
    CautionRect[1].height = 56;
    CautionRect[2].x = 256;
    CautionRect[2].y = 0;
    CautionRect[2].width = 56;
    CautionRect[2].height = 56;
    CautionRect[3].x = 256;
    CautionRect[3].y = 0;
    CautionRect[3].width = 56;
    CautionRect[3].height = 56;
    CautionRect[4].x = 256;
    CautionRect[4].y = 0;
    CautionRect[4].width = 56;
    CautionRect[4].height = 56;
    CautionRect[5].x = 215;
    CautionRect[5].y = 0;
    CautionRect[5].width = 56;
    CautionRect[5].height = 56;
    CautionRect[6].x = 256;
    CautionRect[6].y = 0;
    CautionRect[6].width = 56;
    CautionRect[6].height = 56;

    EbResult2[0].x = 0;
    EbResult2[0].y = 0;
    EbResult2[0].width = 256;
    EbResult2[0].height = 70;
    EbResult2[1].x = 0;
    EbResult2[1].y = 0;
    EbResult2[1].width = 256;
    EbResult2[1].height = 70;
    EbResult2[2].x = 0;
    EbResult2[2].y = 0;
    EbResult2[2].width = 256;
    EbResult2[2].height = 70;
    EbResult2[3].x = 0;
    EbResult2[3].y = 0;
    EbResult2[3].width = 256;
    EbResult2[3].height = 70;
    EbResult2[4].x = 0;
    EbResult2[4].y = 0;
    EbResult2[4].width = 256;
    EbResult2[4].height = 70;
    EbResult2[5].x = 0;
    EbResult2[5].y = 0;
    EbResult2[5].width = 214;
    EbResult2[5].height = 70;
    EbResult2[6].x = 0;
    EbResult2[6].y = 0;
    EbResult2[6].width = 256;
    EbResult2[6].height = 70;

    EbResult0[0].x = 0;
    EbResult0[0].y = 80;
    EbResult0[0].width = 146;
    EbResult0[0].height = 60;
    EbResult0[1].x = 0;
    EbResult0[1].y = 80;
    EbResult0[1].width = 146;
    EbResult0[1].height = 60;
    EbResult0[2].x = 0;
    EbResult0[2].y = 80;
    EbResult0[2].width = 146;
    EbResult0[2].height = 60;
    EbResult0[3].x = 0;
    EbResult0[3].y = 70;
    EbResult0[3].width = 140;
    EbResult0[3].height = 70;
    EbResult0[4].x = 0;
    EbResult0[4].y = 70;
    EbResult0[4].width = 172;
    EbResult0[4].height = 70;
    EbResult0[5].x = 0;
    EbResult0[5].y = 70;
    EbResult0[5].width = 274;
    EbResult0[5].height = 70;
    EbResult0[6].x = 0;
    EbResult0[6].y = 70;
    EbResult0[6].width = 182;
    EbResult0[6].height = 70;

    EbResult1[0].x = 0;
    EbResult1[0].y = 146;
    EbResult1[0].width = 184;
    EbResult1[0].height = 64;
    EbResult1[1].x = 0;
    EbResult1[1].y = 146;
    EbResult1[1].width = 184;
    EbResult1[1].height = 64;
    EbResult1[2].x = 0;
    EbResult1[2].y = 146;
    EbResult1[2].width = 184;
    EbResult1[2].height = 64;
    EbResult1[3].x = 0;
    EbResult1[3].y = 140;
    EbResult1[3].width = 180;
    EbResult1[3].height = 70;
    EbResult1[4].x = 0;
    EbResult1[4].y = 140;
    EbResult1[4].width = 168;
    EbResult1[4].height = 70;
    EbResult1[5].x = 0;
    EbResult1[5].y = 140;
    EbResult1[5].width = 298;
    EbResult1[5].height = 70;
    EbResult1[6].x = 0;
    EbResult1[6].y = 140;
    EbResult1[6].width = 230;
    EbResult1[6].height = 70;

    OkRect[0].x = 0;
    OkRect[0].y = 208;
    OkRect[0].width = 26;
    OkRect[0].height = 16;
    OkRect[1].x = 0;
    OkRect[1].y = 208;
    OkRect[1].width = 26;
    OkRect[1].height = 16;
    OkRect[2].x = 0;
    OkRect[2].y = 208;
    OkRect[2].width = 26;
    OkRect[2].height = 16;
    OkRect[3].x = 0;
    OkRect[3].y = 208;
    OkRect[3].width = 26;
    OkRect[3].height = 16;
    OkRect[4].x = 0;
    OkRect[4].y = 208;
    OkRect[4].width = 26;
    OkRect[4].height = 16;
    OkRect[5].x = 0;
    OkRect[5].y = 208;
    OkRect[5].width = 32;
    OkRect[5].height = 20;
    OkRect[6].x = 0;
    OkRect[6].y = 208;
    OkRect[6].width = 42;
    OkRect[6].height = 16;

    CoolRect[0].x = 0;
    CoolRect[0].y = 224;
    CoolRect[0].width = 50;
    CoolRect[0].height = 16;
    CoolRect[1].x = 0;
    CoolRect[1].y = 224;
    CoolRect[1].width = 50;
    CoolRect[1].height = 16;
    CoolRect[2].x = 0;
    CoolRect[2].y = 224;
    CoolRect[2].width = 50;
    CoolRect[2].height = 16;
    CoolRect[3].x = 0;
    CoolRect[3].y = 224;
    CoolRect[3].width = 50;
    CoolRect[3].height = 16;
    CoolRect[4].x = 0;
    CoolRect[4].y = 224;
    CoolRect[4].width = 50;
    CoolRect[4].height = 16;
    CoolRect[5].x = 0;
    CoolRect[5].y = 230;
    CoolRect[5].width = 60;
    CoolRect[5].height = 20;
    CoolRect[6].x = 0;
    CoolRect[6].y = 224;
    CoolRect[6].width = 62;
    CoolRect[6].height = 16;
}

void EBInit(float speed_mult) {
    tex = TexManager.GetTexture("ebat", -1);

    if (tex) {
        tex2 = TexManager.GetTexture("ebat2", -1);

        if (tex2) {
            eb_count = 0;
            old_time = -1.0;
            eb_key_num = 0;
            speed = 4.0f * speed_mult;
            eb_end_count = 0;
            now_eb_key = -1;
            sound_cnt = 0;
            debug_mode = 0;
            eb_motion[0].motion_no = -1;
            eb_key_count = 0;
            eb_finish_cnt = 0;
            now_button_no = 0;
            init_draw_ok();
            fade_bgm = 1;
            play_fanfare = 1;
        }
    }
}

void EBFinishSound(int do_fade_bgm, int do_play_fanfare) {
    fade_bgm = do_fade_bgm;
    play_fanfare = do_play_fanfare;
}

void EBInitIntro() {
    tex = TexManager.GetTexture("ebat", -1);

    if (tex == nullptr) {
        return;
    }

    tex2 = TexManager.GetTexture("ebat2", -1);

    if (tex2 == nullptr) {
        return;
    }

    eb_intro_cnt = 0;
    ebattle_intro_flag = 1;
    // The wipe opens from the right edge, so it starts with no width.
    draw_rect = CRect_i_(0x280, 0, 0, 0x1C0);
}

void EBSetMotion(CCharacter *character, int *motions) {
    if (character == nullptr) {
        return;
    }

    int i = 0;

    for (;; i++) {
        int motion_no = motions[i];

        if (motion_no < 0) {
            break;
        }

        eb_motion[i].motion_no = motion_no;
    }

    eb_motion[i].motion_no = -1;

    eb_chara = character;

    for (i = 0;; i++) {
        if (eb_motion[i].motion_no < 0) {
            break;
        }

        MOTION_INFO *info = eb_chara->GetMotionInfo(eb_motion[i].motion_no);

        if (info == nullptr) {
            return;
        }

        auto start = static_cast<float>(info->start);
        auto end = static_cast<float>(info->end);
        eb_motion[i].start = start;
        eb_motion[i].end = end;
        eb_motion[i].speed = info->speed;
        auto frames = static_cast<int>((end - start) / info->speed);
        eb_end_count += frames;
        eb_motion[i].frames = frames;
    }

    ebattle_flag = 1;
    eb_cool_flag = 1;
    GamePad.MenuModeOn(0x50);
}

void EBDebug(int mode) {
    debug_mode = mode;
}

void EBSetKey(float time, int buttons, int mode) {
    if (eb_key_num < 64) {
        EB_KEY *key = &eb_key[eb_key_num++];
        key->frame = 0;

        for (int i = 0;; i++) {
            EB_MOTION *motion = &eb_motion[i];

            if (motion->motion_no < 0) {
                break;
            }

            if (time >= motion->start && time < motion->end) {
                key->frame += (time - motion->start) / motion->speed;
                break;
            }

            key->frame += motion->frames;
        }

        key->buttons = buttons;
        key->pressed = 0;

        mode = std::min(mode, 5);

        key->mode = mode;
        key->passed = false;
        key->hit = 0;
        key->highlight = false;
    }
}

void EBExit() {
    ebattle_flag = 0;
    eb_intro_cnt = 0;
    GamePad.MenuModeOff();
    ebattle_intro_flag = 0;
    eb_finish_cnt = 0;
    init_draw_ok();

    while (SndSPSeSyncBG() != 0) {
    }

    InitReadBG();
}

int EBIntroLoop() {
    if (ebattle_intro_flag == 0) {
        return 1;
    }

    // The wipe crosses the screen over a hundred frames.
    int width = eb_intro_cnt * 640 / 100;

    width = std::min(width, 640);

    draw_rect.x = 640 - width;
    draw_rect.width = width;
    eb_intro_cnt++;

    if (eb_intro_cnt > 100) {
        draw_rect = CRect_i_(0, 0, 640, 448);
        return 1;
    }

    return 0;
}

int EBLoop() {
    if (ebattle_flag == 0) {
        return 1;
    }

    if (eb_finish_cnt > 0) {
        if (eb_finish_cnt == 1) {
            EBExit();
            return eb_result;
        }

        eb_finish_cnt--;
        return 0;
    }

    if (eb_count == 0 && play_fanfare != 0) {
        int loaded_size = 0;
        StartReadBG();
        SndSPSeLoadBG(kFanfareSound, read_buffer, &loaded_size);
    }

    ReadBG();

    const int   key_count = std::clamp(eb_key_num, 0, 64);
    const float frame_speed = speed;
    int         active_index = -1;
    int         cool = 0;

    for (int i = 0; i < key_count; i++) {
        EB_KEY &key = eb_key[i];
        auto    distance = static_cast<float>(eb_count - key.frame);
        distance *= frame_speed;
        const int early = key.mode > 0 ? (6 - key.mode) << 6 : 0;

        key.highlight = 0;

        if (distance > 0.0f) {
            if (distance < 48.0f) {
                active_index = i;
            } else {
                key.passed = 1;
            }

            if (distance < 24.0f) {
                cool = 1;
            }
        } else {
            if (distance > -16.0f) {
                active_index = i;
            }

            if (early > 0 && distance <= 16.0f - static_cast<float>(early)) {
                key.highlight = 1;
            }
        }

        if (debug_mode != 0 && eb_count == key.frame) {
            SndSePlay(SE_EB_HIT, -1, 0);
            SndSePlay(MENU_SOUND_CONFIRM, -1, 0);
        }
    }

    bool failed = false;

    if (active_index >= 0) {
        EB_KEY &active = eb_key[active_index];
        active.pressed |= GamePad.GetPadDown();

        if (active.pressed == active.buttons) {
            if (active.hit == 0) {
                if (cool) {
                    SndSePlay(SE_EB_HIT_COOL, -1, 0);
                } else {
                    SndSePlay(SE_EB_HIT, -1, 0);
                }

                set_draw_ok(cool, active_index);
                eb_cool_flag &= cool;
                eb_key_count++;
            }

            active.hit = 1;
        }

        if (active.buttons != (active.buttons | active.pressed)) {
            failed = true;
        }
    } else if (GamePad.GetPadDown()) {
        failed = true;
    }

    for (int i = 0; i < key_count; i++) {
        const EB_KEY &key = eb_key[i];

        if (key.passed == 0) {
            break;
        }

        if (key.buttons != key.pressed) {
            failed = true;
        }
    }

    if (failed && !ConfigGet().qte_always_win && debug_mode == 0 && eb_key_count < key_count &&
        EdDebugParamDrawOff == 0) {
        SndBgmFadeOut(40, 0);
        eb_finish_cnt = 80;
        eb_result = -1;
        eb_count++;
        return 0;
    }

    if (eb_count == eb_end_count - 100 && fade_bgm != 0) {
        SndBgmFadeOut(100, 0);
    }

    if (eb_count >= eb_end_count) {
        if (play_fanfare != 0) {
            while (SndSPSeSyncBG() != 0) {
            }

            SndSPSePlay(kFanfareSound, -1);
        }

        eb_finish_cnt = 160;
        eb_result = ConfigGet().qte_always_win ? 2 : (eb_cool_flag != 0) + 1;
        return 0;
    }

    old_time = now_time;
    draw_ok_loop();
    eb_count++;
    return 0;
}

void EBDraw() {
    if (ebattle_intro_flag == 0 && ebattle_flag == 0) {
        return;
    }

    setbilinear(0);

    int lang = LanguageCode;

    if (lang < LANG_JAPANESE || lang >= LANG_COUNT) {
        lang = LANG_ENGLISH_US;
    }

    if (EdDebugParamDrawOff != 0) {
        return;
    }

    if (eb_finish_cnt > 0) {
        TexManager.ReloadTexture(GetVif1Packet(), 0x2D);
        CRect_i_ texel;
        texel.x = texel.y = texel.width = texel.height = 0;

        if (eb_result < 0) {
            texel = EbResult0[lang];
        }

        if (eb_result == 2) {
            texel = EbResult2[lang];
        }

        if (eb_result == 1) {
            texel = EbResult1[lang];
        }

        int left = 0x140 - (texel.width >> 1);
        int top = SCREEN_HALF_HEIGHT - (texel.height >> 1);

        if (eb_result > 0 || (eb_finish_cnt >> 2) % 2 != 0) {
            set2DSprite(GetVif1Packet(), tex2, CRect_i_(left, top, texel.width, texel.height), texel.x, texel.y);
        }

        return;
    }

    CRect_i_ bar(0, 0xA00, 0x2800, 0x100);
    CRect_i_ left_edge(0xA80, 0xA00, 0x400, 0x100);
    CRect_i_ right_edge(0xC00, 0xA00, 0x100, 0x100);

    if (ebattle_intro_flag != 0) {
        int caution_lang = LanguageCode;

        if (caution_lang < 0 || caution_lang >= 7) {
            caution_lang = 0;
        }

        Caution = CautionRect[caution_lang];
        TexManager.ReloadTexture(GetVif1Packet(), 0x2D);
        int shift = draw_rect.x << 4;
        bar.x += shift;
        left_edge.x += shift;
        right_edge.x += shift;
        MGFillBox(bar, 0, 0x28, 0xA0, 0x40);
        MGFillBox(left_edge, 0xFF, 0xFF, 0xFF, 0x20);
        MGFillBox(right_edge, 0xFF, 0xFF, 0xFF, 0x20);

        if ((eb_intro_cnt >> 2) % 2 != 0) {
            int left = 0x140 - (Caution.width >> 1);
            int top = SCREEN_HALF_HEIGHT - (Caution.height >> 1);
            set2DSprite(GetVif1Packet(), tex2, CRect_i_(left, top, Caution.width, Caution.height), Caution.x, Caution.y);
        }

        if (eb_intro_cnt % 8 == 0) {
            SndSePlay(SE_EB_INTRO_TICK, -1, 0);
        }

        return;
    }

    if (ebattle_flag == 0) {
        return;
    }

    TexManager.ReloadTexture(GetVif1Packet(), 0x2D);
    MGFillBox(bar, 0, 0x28, 0xA0, 0x40);
    MGFillBox(left_edge, 0xFF, 0xFF, 0xFF, 0x20);
    MGFillBox(right_edge, 0xFF, 0xFF, 0xFF, 0x20);
    eb_chara->GetMotionInfo(eb_chara->motion_no);

    float frame_speed = speed;

    for (int i = eb_key_count; i < eb_key_num; i++) {
        EB_KEY *key = &eb_key[i];
        float   delta = eb_count - key->frame;
        delta *= frame_speed;
        float position = 200.0f - delta;
        auto  x = static_cast<int>(position);
        DrawButton(key->buttons, x, 0x140, button_scale(i), key->highlight);
    }

    if (eb_key_count > 0) {
        EB_KEY *key = &eb_key[eb_key_count - 1];
        float   delta = eb_count - key->frame;
        delta *= frame_speed;
        float position = 200.0f - delta;
        auto  x = static_cast<int>(position);
        draw_ok(x);
    }
}

namespace {

/**
 * Draws one button prompt of the event battle.
 */
void DrawButton(int buttons, int x, int y, float scale, int early) {
    if (x < -0x20) {
        return;
    }

    if (x > 0x280) {
        return;
    }

    if (early != 0) {
        DrawButtonSub(x, y, 0, 0x60, scale);
    } else if ((buttons & 0x20) != 0) {
        DrawButtonSub(x, y, 0, 0, scale);
    } else if ((buttons & 0x10) != 0) {
        DrawButtonSub(x, y, 0x20, 0, scale);
    } else if ((buttons & 0x80) != 0) {
        DrawButtonSub(x, y, 0x40, 0, scale);
    } else if ((buttons & 0x40) != 0) {
        DrawButtonSub(x, y, 0x60, 0, scale);
    } else if ((buttons & 0x1000) != 0) {
        if ((buttons & 0x8000) != 0) {
            DrawButtonSub(x, y, 0x20, 0x40, scale);
        } else if ((buttons & 0x2000) != 0) {
            DrawButtonSub(x, y, 0, 0x40, scale);
        } else {
            DrawButtonSub(x, y, 0, 0x20, scale);
        }
    } else if ((buttons & 0x8000) != 0) {
        DrawButtonSub(x, y, 0x60, 0x20, scale);
    } else if ((buttons & 0x2000) != 0) {
        DrawButtonSub(x, y, 0x40, 0x20, scale);
    } else if ((buttons & 0x4000) != 0) {
        if ((buttons & 0x8000) != 0) {
            DrawButtonSub(x, y, 0x60, 0x40, scale);
        } else if ((buttons & 0x2000) != 0) {
            DrawButtonSub(x, y, 0x40, 0x40, scale);
        } else {
            DrawButtonSub(x, y, 0x20, 0x20, scale);
            return;
        }
    }
}

/**
 * Draws one button prompt at a scale.
 */
void DrawButtonSub(int x, int y, int texture_x, int texture_y, float scale) {
    auto     width = static_cast<int>(32.0f * scale);
    auto     height = static_cast<int>(32.0f * scale);
    CRect_i_ screen;
    CRect_i_ texel;

    // The prompt grows about its own centre.
    x -= (width - 32) >> 1;
    y -= (height - 32) >> 1;
    texel.x = texture_x;
    texel.y = texture_y;
    texel.width = 32;
    texel.height = 32;
    screen.x = x;
    screen.y = y;
    screen.width = width;
    screen.height = height;
    set2DSprite(GetVif1Packet(), tex, screen, texel);
}

/**
 * Clears the enemy-battle confirmation effect.
 */
void init_draw_ok() {
    ok_draw_cnt = 0;
    ok_effect_button = -1;
}

void set_draw_ok(int type, int button) {
    ok_effect_button = button;
    ok_draw_cnt = 30;
    ok_type = type;
}

} // namespace

/**
 * Counts down the active confirmation effect.
 */
void draw_ok_loop() {
    if (ok_draw_cnt > 0) {
        --ok_draw_cnt;

        ok_draw_cnt = std::max(ok_draw_cnt, 0);
    }
}

namespace {

/**
 * Draws the success flash after a button is pressed in time.
 */
void draw_ok(int x) {
    // clang-format off
    static sceVu0FVECTOR dir[8] = {
        {1.0f, 0.0f, 0.0f, 0.0f},   {-1.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f, 0.0f},   {0.0f, -1.0f, 0.0f, 0.0f},
        {1.0f, 1.0f, 0.0f, 0.0f},   {1.0f, -1.0f, 0.0f, 0.0f},
        {-1.0f, 1.0f, 0.0f, 0.0f},  {-1.0f, -1.0f, 0.0f, 0.0f},
    };
    // clang-format on

    if (ok_draw_cnt <= 0) {
        return;
    }

    CRect_i_ success_texel(0, 0xD0, 0x1A, 0x10);
    CRect_i_ cool_texel(0, 0xE0, 0x32, 0x10);
    int      lang = LanguageCode;

    if (lang < LANG_JAPANESE || lang >= LANG_COUNT) {
        lang = LANG_ENGLISH_US;
    }

    success_texel = OkRect[lang];
    cool_texel = CoolRect[lang];
    CRect_i_      spark_texel(0x20, 0x60, 0x20, 0x20);
    sceVu0FVECTOR offset;
    CRect_i_     *texel = &success_texel;

    if (ok_type != 0) {
        texel = &cool_texel;
    }

    if ((ok_draw_cnt / 3) % 2 != 0) {
        int      width = texel->width;
        int      left = 200 - (width >> 1);
        int      height = texel->height;
        int      top = 318 - height;
        CRect_i_ result_screen(left, top, width, height);

        set2DSprite(GetVif1Packet(), tex2, result_screen, texel->x, texel->y);
    }

    texel = &spark_texel;
    int spark_x = x + 0x18;

    for (auto &direction : dir) {
        sceVu0Normalize(direction, direction);
        sceVu0ScaleVector(offset, direction, 2.0f * static_cast<float>(30 - ok_draw_cnt));
        int width = texel->width;
        int half = width >> 1;
        int left = spark_x + static_cast<int>(offset[0]) - half;
        int bottom = static_cast<int>(offset[1]) + 0x160;
        int height = texel->height;
        int top = bottom - height - 2;
        int alpha = 0x80 - static_cast<int>(static_cast<float>(30 - ok_draw_cnt * 2) * 128.0 / 30.0);

        if (alpha > 0) {
            CRect_i_ screen(left, top, width, height);
            set2DSprite(GetVif1Packet(), tex, screen, spark_texel, static_cast<unsigned char>(alpha));
        }
    }
}

/**
 * Gives the scale a button prompt draws at while it flashes.
 */
float button_scale(int button) {
    if (button != ok_effect_button) {
        return 1.0f;
    }

    // The prompt swells over the first five frames of the flash and settles
    // back over the next five.
    int   elapsed = 30 - ok_draw_cnt;
    float scale = 1.0f;

    if (elapsed < 5) {
        scale += 0.2f * static_cast<float>(elapsed);
    } else if (elapsed < 10) {
        scale += 0.2f * static_cast<float>(10 - elapsed);
    }

    return scale;
}

/**
 * Tests whether an editor input mode currently owns the controller.
 */
int check_key_mode(int mode) {
    if (mode == 0xFFFF) {
        return 1;
    }

    return (key_mode & mode) != 0;
}

/**
 * Reports whether an editor input mode currently owns the pad.
 */
int PortEdCheckKeyMode(int mode) {
    return check_key_mode(mode);
}

} // namespace

int EdSetKeyMode(int mode) {
    return key_mode = mode;
}

// The right stick of the town's walk: EdMoveChara turns its camera by RX (0.03 a tick, unless a
// wall is on that side) and raises it by RY while it is under 30. RX stays the stick's: the mouse's
// turn goes to EditLoop's request (input/mouse_look.hpp), which checks it against the walls itself; so
// the drift behind a walking character and R1 and L1, which wait for RX to rest, see only the stick.
// RY takes the mouse as input/mouse_look.hpp describes. In an interior's first-person view EdMoveChara
// turns the character by RX only while the left stick rests, so the mouse turns it in EyeCamera.

float EdGetRXf(int mode) {
    if (PortEdCheckKeyMode(mode)) {
        CCameraFollow *camera = EdMoveCharaInfo.camera;
        if (viewMode == 0 && !EdMoveCharaInfo.interior && camera != nullptr) {
            TownMouseRecord(camera, EdMoveCharaInfo.fishing != 0);
        }
        return GamePad.GetRXf();
    }

    return 0.0f;
}

// EdMoveChara reads it for the town's camera only; EyeCamera below reads the stick itself.
float EdGetRYf(int mode) {
    if (PortEdCheckKeyMode(mode)) {
        CCameraFollow *camera = EdMoveCharaInfo.camera;
        float          stick = GamePad.GetRYf();
        if (camera == nullptr) {
            return stick;
        }
        float reading = MouseLookRise(camera, stick, EdDebugCameraFlag == 0 ? 30.0f : INFINITY, 5.0f);
        // Retail ignores RY at the ceiling, including a request to lower the camera. Let a
        // native mouse reversal leave that ceiling; the subsequent floor checks still apply.
        if (EdDebugCameraFlag == 0 && camera->GetHeight() >= 30.0f && InputGetMouseLook().pitch > 0.0f) {
            camera->AddHeight(-reading);
            return 0.0f;
        }
        return reading;
    }

    return 0.0f;
}

// Suppress the interior's keyboard steering; the collision seam supplies camera-relative WASD.
float EdGetLXf(int mode) {
    auto move = InputGetKeyboardMovement();
    if (!PortEdCheckKeyMode(mode) || (viewMode && (move.x || move.y))) {
        return 0.0f;
    }
    return GamePad.GetLXf();
}

float EdGetLYf(int mode) {
    auto move = InputGetKeyboardMovement();
    if (!PortEdCheckKeyMode(mode) || (viewMode && (move.x || move.y))) {
        return 0.0f;
    }
    return GamePad.GetLYf();
}

int EdPadOn(int keys, int mode) {
    if (check_key_mode(mode)) {
        return GamePad.On(keys);
    }

    return 0;
}

int EdPadDown(int keys, int mode) {
    if (check_key_mode(mode)) {
        return GamePad.Down(keys);
    }

    return 0;
}

namespace {

/**
 * Returns the low two character-control lock bits.
 */
int keylock() {
    return chara_mode & 3;
}

/**
 * Returns the right stick's horizontal input, or zero while editor input is locked.
 */
float GetRXf() {
    if (keylock() != 0) {
        return 0.0f;
    }

    return EdGetRXf(1);
}

/**
 * Returns the right stick's vertical input, or zero while editor input is locked.
 */
float GetRYf() {
    if (keylock() != 0) {
        return 0.0f;
    }

    return EdGetRYf(1);
}

/**
 * Returns the left stick's horizontal input, or zero while editor input is locked.
 */
float GetLXf() {
    if (keylock() != 0) {
        return 0.0f;
    }

    return EdGetLXf(1);
}

/**
 * Returns the left stick's vertical input, or zero while editor input is locked.
 */
float GetLYf() {
    if (keylock() != 0) {
        return 0.0f;
    }

    return EdGetLYf(1);
}

/**
 * Reports whether a button is held, unless the pad is locked.
 */
int PadOn(int keys) {
    if (keylock() != 0) {
        return 0;
    }

    return EdPadOn(keys, 1);
}

/**
 * Reports whether a button was just pressed, unless the pad is locked.
 */
int PadDown(int keys) {
    if (keylock() != 0) {
        return 0;
    }

    return EdPadDown(keys, 1);
}

/**
 * Moves the camera towards a point, keeping it clear of the collision.
 */
void CameraAutoMove(CCameraFollow *camera, CCPoly * /*poly*/, float *position, float left_distance, float right_distance) {
    float reference[4];
    float offset[4];

    camera->GetRef(reference);
    offset[0] = position[0] - reference[0];
    offset[2] = position[2] - reference[2];
    offset[1] = 0.0f;
    camera->SetDistance(0.1f + DistVector(offset));

    float step = 0.0f;
    float excess = camera->GetDistance() - camera_near_dist;

    if (excess < step) {
        step = -excess / 10.0f;
    }

    if (excess > 0.0f) {
        step = excess / 15.0f;
    }

    step = std::min(step, 2.0f);

    step = std::max(step, 0.05f);

    if (excess < 0.0f) {
        step *= 2.0f;
    }

    camera->SetAngle(atan2f(offset[0], offset[2]));

    if (left_distance < right_distance) {
        camera->AddAngle(0.2f * -step);
    } else {
        camera->AddAngle(0.2f * step);
    }

    if (camera->GetDistance() < 0.8f * camera_near_dist) {
        camera->AddHeight(1.0f);
    }
}

/**
 * Points the eye camera the way a character faces.
 */
void InitEyeCamera(CCharacter *character) {
    viewAngleH = character->GetRotation()->y;
    viewAngleV = 0.0f;
}

/**
 * Reports whether a villager's model is set up and enabled for drawing.
 */
int IsVillagerActive(CNPCharacter *villager) {
    return villager->initialized != 0 && villager->draw_enabled != 0;
}

} // namespace

// Retail's, with the mouse turning the view while it is the one shown. In an interior the view
// takes its heading from the character's (EdMoveChara), so the mouse turns the character too.
void EyeCamera(CCamera *camera, CCharacter *character, int right_stick) {
    float      stick_x = 0.0f;
    float      stick_y = 0.0f;
    const bool unlocked = (chara_mode & 3) == 0;
    const auto keyboard = InputGetKeyboardMovement();
    const bool walking = viewMode != 0 && (keyboard.x || keyboard.y);

    if (right_stick != 0) {
        stick_x = 0.0f;
        stick_y = unlocked && PortEdCheckKeyMode(1) ? -GamePad.GetRYf() : 0.0f;
    } else {
        stick_x = unlocked && !walking ? EdGetLXf(1) : 0.0f;
        stick_y = unlocked && !walking ? -EdGetLYf(1) : 0.0f;
    }

    if (stick_x > 0.0f) {
        viewAngleH -= stick_x * kTurnRate;

        if (viewAngleH < -PI) {
            viewAngleH += TWO_PI;
        }
    }

    if (stick_x < -0.0f) {
        viewAngleH -= stick_x * kTurnRate;

        if (viewAngleH > PI) {
            viewAngleH -= TWO_PI;
        }
    }

    if (stick_y > 0.0f && viewAngleV < 0.65f) {
        viewAngleV += stick_y * kTurnRate;
    }

    if (stick_y < -0.0f && viewAngleV > -1.0f) {
        viewAngleV += stick_y * kTurnRate;
    }

    if (viewMode != 0 && unlocked && PortEdCheckKeyMode(1)) {
        float heading = MouseLookEyeAngleH(viewAngleH);

        if (right_stick != 0 && heading != viewAngleH) {
            sceVu0FVECTOR rotation;
            character->GetRotation(rotation);
            rotation[1] = heading;
            character->SetRotation(rotation);
        }

        viewAngleH = heading;
        viewAngleV = MouseLookEyeAngleV(viewAngleV);
    }

    EdEyeCamera(camera, character);
}

void EdViewModeOff() {
    viewMode = 0;
}

void EdInitCameraParam(CCameraFollow *camera) {
    if (camera != nullptr) {
        camera->SetDistance(camera_near_dist);
        camera->SetHeight(5.0f);
    }
}

void EdMoveCharaInit() {
    viewMode = 0;
    chara_mode = 0;
    chara_fishing = ED_FISHING_INIT;
    fishing_mes = ED_FISHING_MES_EVALUATE;
}

void EdEyeCamera(CCamera *camera, CCharacter *character) {
    sceVu0FVECTOR position;
    sceVu0FVECTOR reference;
    sceVu0FMATRIX rotation;
    sceVu0FMATRIX unit;

    // The point the camera looks at sits ten units ahead of the head, turned
    // by the two view angles.
    reference[0] = 0.0f;
    reference[1] = 0.0f;
    reference[2] = 10.0f;
    reference[3] = 0.0f;
    sceVu0UnitMatrix(unit);
    sceVu0RotMatrixX(rotation, unit, viewAngleV);
    sceVu0RotMatrixY(rotation, rotation, viewAngleH);
    sceVu0ApplyMatrix(reference, rotation, reference);
    character->GetPosition(position);
    position[1] += 14.0f;
    reference[0] += position[0];
    reference[1] += position[1];
    reference[2] += position[2];
    camera->SetPos(position);
    camera->SetRef(reference);
}

int EdCheckViewMode() {
    return viewMode;
}

float EdAGetViewAngleH() {
    return viewAngleH;
}

float EdAGetViewAngleV() {
    return viewAngleV;
}

void EdASetViewAngle(float horizontal, float vertical) {
    viewAngleH = horizontal;
    viewAngleV = vertical;
}

namespace {

/** Distance walked since the last footstep sound. */
float walked = 0.0f;
/** Which foot the next footstep sound plays for. */
int foot = 0;

} // namespace

int PortEdMoveCheck(float *pos, float *velocity, float *out_pos, MoveCheckInfo *out_info, CCPoly *polys, int poly_num, int mode) {
    auto move = InputGetKeyboardMovement();
    bool walking = viewMode && (move.x || move.y) && !(chara_mode & 3) &&
                   !EdMoveCharaInfo.key_lock && PortEdCheckKeyMode(1) && chara_fishing <= ED_FISHING_STAND;
    if (walking) {
        float heading = MouseLookEyeAngleH(viewAngleH);
        velocity[0] = 0.6f * (move.x * cosf(heading) - move.y * sinf(heading));
        velocity[2] = 0.6f * (-move.y * cosf(heading) - move.x * sinf(heading));
        auto *chara = static_cast<CMainChara *>(EdMoveCharaInfo.chara);
        if (chara->move_info.landed) {
            sceVu0FVECTOR normal;
            sceVu0Normalize(normal, chara->move_info.ground_poly.normal);
            velocity[0] *= fabsf(normal[1]);
            velocity[2] *= fabsf(normal[1]);
        }
        sceVu0FVECTOR rotation;
        chara->GetRotation(rotation);
        rotation[1] = heading;
        chara->SetRotation(rotation);
    }
    const int result = MoveCheck(pos, velocity, out_pos, out_info, polys, poly_num, mode);
    if (walking && !EdMoveCharaInfo.interior && out_info->landed) {
        walked += hypotf(out_pos[0] - pos[0], out_pos[2] - pos[2]);
        if (walked >= 10.0f) {
            SndPlayFootSound(out_info->poly.attr.foot_sound, foot, out_pos);
            foot ^= 1;
            walked = fmodf(walked, 10.0f);
        }
    } else {
        walked = 0.0f;
    }
    return result;
}

void EdMoveChara() {
    static sceVu0FVECTOR reference = {0.0f, 0.0f, 0.0f, 0.0f};

    EdMoveCharaInfo.event_no = -1;
    EdMoveCharaInfo.system_event_no = -1;
    float          time = EdMoveCharaInfo.time;
    CCameraFollow *camera = EdMoveCharaInfo.camera;
    CCamera       *view_camera = EdMoveCharaInfo.view_camera;
    int            key_lock = EdMoveCharaInfo.key_lock;
    auto          *chara = static_cast<CMainChara *>(EdMoveCharaInfo.chara);
    int            interior = EdMoveCharaInfo.interior;
    CEditGround   *ground = EdMoveCharaInfo.ground;

    if (key_lock != 0) {
        chara_mode |= 1;
    } else {
        chara_mode &= ~1;
    }

    sceVu0FVECTOR pos;
    sceVu0FVECTOR rot;
    chara->GetPosition(pos);
    chara->GetRotation(rot);
    sceVu0FVECTOR velocity = {0.0f, 0.0f, 0.0f, 0.0f};
    sceVu0FVECTOR move;

    float angle = camera->GetAngleH();
    float lx = GetLXf();
    float ly = GetLYf();

    if (key_lock != 0 || chara_fishing > ED_FISHING_STAND) {
        lx = ly = 0.0f;
    }

    if (PadOn(0x80) != 0 && EdDebugMoveFlag > 0) {
        if (EdDebugMoveFlag > 0) {
            lx *= 2.0f;
            ly *= 2.0f;
        } else {
            lx *= 1.5f;
            ly *= 1.5f;
        }
    }

    int   motion = 0;
    float motion_speed = -1.0f;

    if (viewMode == 0) {
        float move_x = lx * cosf(angle) + ly * sinf(angle);
        float move_z = ly * cosf(angle) - lx * sinf(angle);

        if (interior != 0) {
            velocity[0] = move_x;
            velocity[2] = move_z;
        } else {
            velocity[0] = 1.6f * move_x;
            velocity[2] = 1.6f * move_z;
        }

        // A slope slows the step by how far its floor leans.
        if (chara->move_info.landed != 0) {
            sceVu0FVECTOR normal;
            sceVu0Normalize(normal, chara->move_info.ground_poly.normal);
            normal[1] = normal[1] < 0.0f ? -normal[1] : normal[1];
            velocity[0] *= normal[1];
            velocity[2] *= normal[1];
        }

        if (lx != 0.0f || ly != 0.0f) {
            float stick = sqrtf(lx * lx + ly * ly);

            if (stick > 0.85f && EdMoveCharaInfo.fishing == 0) {
                motion = 1;
            } else {
                motion = 2;
                motion_speed = 0.8f * (0.2f + stick);

                motion_speed = std::min(motion_speed, 0.85f);
            }
        } else {
            motion = 0;
        }

        if (move_x != 0.0f || move_z != 0.0f) {
            chara->SetRotation(0.0f, AngleInterpolate(rot[1], atan2f(move_x, move_z), 0.2f, INTERPOLATE_STEP), 0.0f);
        }
    } else if (interior != 0) {
        if (lx == 0.0f) {
            lx = GetRXf();
        }

        sceVu0FVECTOR eye_rot;
        chara->GetRotation(eye_rot);
        eye_rot[1] -= 0.04f * lx;

        if (eye_rot[1] > PI) {
            eye_rot[1] -= TWO_PI;
        }

        if (eye_rot[1] < -PI) {
            eye_rot[1] += TWO_PI;
        }

        angle = eye_rot[1];
        viewAngleH = angle;
        chara->SetRotation(eye_rot);
        float move_x = ly * sinf(angle);
        float move_z = ly * cosf(angle);
        velocity[0] = 0.6f * -move_x;
        velocity[2] = 0.6f * -move_z;

        if (chara->move_info.landed != 0) {
            sceVu0FVECTOR normal;
            sceVu0Normalize(normal, chara->move_info.ground_poly.normal);
            normal[1] = normal[1] < 0.0f ? -normal[1] : normal[1];
            velocity[0] *= normal[1];
            velocity[2] *= normal[1];
        }
    }

    if ((chara_mode & 6) == 0 && chara_fishing < ED_FISHING_CAST_START) {
        chara->SetMotion(motion, 0);
        chara->SetMotionSpeed(motion_speed);
    }

    if (PadDown(0x20) != 0 && EdDebugMoveFlag != 0 && key_lock == 0 && chara_fishing < ED_FISHING_CAST_START) {
        CVector3_f_ jump;
        chara->GetVelocity(&jump);
        jump.y = 2.0f;
        chara->SetVelocity(jump);
    }

    sceVu0FVECTOR next;
    float         time_before = chara->GetNowTime();
    WorkBuffer->used = 0;
    auto *polys = reinterpret_cast<CCPoly *>(WorkBuffer->Alloc(0x7D0));
    int   poly_count = 0;

    if (interior == 0) {
        poly_count = ground->PickUpPoly(polys, pos[0], pos[1], pos[2]);
    } else {
        CBoxVu0 box;
        box.max[0] = 20.0f + pos[0];
        box.min[0] = pos[0] - 20.0f;
        box.max[2] = 20.0f + pos[2];
        box.min[2] = pos[2] - 20.0f;
        box.max[1] = 1000.0f + pos[1];
        box.min[1] = pos[1] - 1000.0f;

        for (int i = 0; i < EdMoveCharaInfo.parts_count; i++) {
            CFrame *frame = EdMoveCharaInfo.parts[i].GetCollisionFrame();

            if (frame != nullptr) {
                poly_count += frame->PickUpNearPoly(&polys[poly_count], box);
            }
        }
    }

    poly_count += EdEventPointCpPoly(pos, EdMoveCharaInfo.points, EdMoveCharaInfo.point_count, &polys[poly_count], time);

    if (EdMoveCharaInfo.fishing != 0) {
        poly_count += FishingPickUpPoly(&polys[poly_count]);
    }

    for (CNPCharacter &villager : EdVillager) {
        if (IsVillagerActive(&villager)) {
            poly_count += villager.PickUpPoly(pos, &polys[poly_count]);
        }
    }

    if (poly_count > 200) {
        std::println("cpoly over!!!! {}", poly_count);
    }

    CVector3_f_ fall;
    chara->GetVelocity(&fall);
    velocity[1] = fall.y;
    PortEdMoveCheck(pos, velocity, next, &chara->move_info, polys, poly_count, 0);

    if (EdDebugMoveFlag >= 2) {
        next[0] = pos[0] + velocity[0];
        next[2] = pos[2] + velocity[2];
    }

    CCPoly        event_poly;
    sceVu0FVECTOR ahead;
    sceVu0FVECTOR event_hit;
    sceVu0FVECTOR eye;
    int           event_index = -1;
    sceVu0CopyVector(eye, pos);
    eye[1] += 0.5f * chara->body_height;
    ahead[0] = 7.5f * sinf(rot[1]);
    ahead[1] = 0.0f;
    ahead[2] = 7.5f * cosf(rot[1]);
    int poly_event = GetEventPoly(eye, ahead, &event_poly, &event_index, event_hit, polys, poly_count, 0);
    sceVu0SubVector(move, next, pos);

    if (next[1] < -1000.0f) {
        next[1] = 1000.0f;
    }

    if (interior != 0 && next[1] < 0.0f) {
        next[1] = 0.0f;
        chara->move_info.landed = 1;
    }

    chara->SetPosition(next);
    fall.y += -0.1f;

    fall.y = std::max(fall.y, -4.0f);

    chara->SetVelocity(fall);

    if ((chara_mode & 2) != 0 && (chara->motion_state == 3 || viewMode != 0)) {
        chara_mode &= ~2;
    }

    if (chara->move_info.landed != 0) {
        if (fall.y < -1.0f) {
            chara_mode |= 2;
            chara->SetMotion(9, 6);
        }

        chara->SetVelocity(CVector3_f_(0.0f, 0.0f, 0.0f));
        chara_mode &= ~4;
    } else {
        chara_mode |= 4;

        if (fall.y < -0.5f) {
            chara->SetMotion(8, 0);
        }
    }

    if (viewMode == 0 && interior == 0) {
        camera->FollowOn();
        sceVu0FVECTOR target;
        sceVu0CopyVector(target, next);
        target[0] += 7.0f * move[0];
        target[1] += 14.0f + (2.0f * move[1] + reference[1]);
        target[2] += 7.0f * move[2];
        camera->SetFollow(target[0], target[1], target[2]);
    } else {
        EyeCamera(view_camera, chara, interior);
    }

    if (EdMoveCharaInfo.fishing == 0 && PadDown(2) != 0) {
        if (viewMode == 0) {
            sceVu0FVECTOR eye_pos;
            sceVu0FVECTOR eye_ref;
            camera->GetPos(eye_pos);
            camera->GetRef(eye_ref);
            view_camera->SetPos(eye_pos);
            view_camera->SetRef(eye_ref);
            viewMode = !viewMode;
            InitEyeCamera(chara);
        } else {
            viewMode = !viewMode;
        }
    }

    int shallow = 0;

    if (chara->move_info.ground_found != 0 && chara->move_info.poly.attr.area_kind == AREA_KIND_HIGH_CAMERA) {
        shallow = 1;
    }

    int keep_distance = 1;
    int left_clear = 1;
    int right_clear = 1;

    if (viewMode == 0 && interior == 0 && EdDebugMoveFlag < 2) {
        sceVu0FVECTOR last_hit;
        sceVu0FVECTOR eye_pos;
        sceVu0FVECTOR hit;
        sceVu0FVECTOR look;
        sceVu0FVECTOR behind;
        sceVu0FVECTOR forward;
        sceVu0FVECTOR towards;
        sceVu0FVECTOR left_end;
        sceVu0FVECTOR right_end;
        sceVu0FVECTOR left;
        sceVu0FVECTOR right;
        sceVu0FVECTOR reach;
        sceVu0FVECTOR direction;
        sceVu0FVECTOR wall;
        CBoxVu0       box;
        sceVu0FVECTOR to_eye;
        sceVu0FVECTOR to_wall;
        sceVu0FVECTOR cross;
        int           hit_poly[32];
        float         hit_point[32][4];
        sceVu0FVECTOR origin;
        sceVu0FVECTOR facing;
        sceVu0FVECTOR facing_first;
        sceVu0FVECTOR floor_normal;

        float left_distance = 1000000;
        float right_distance = 1000000;
        camera->GetPos(eye_pos);
        camera->GetRef(look);
        sceVu0SubVector(forward, look, eye_pos);
        sceVu0SubVector(behind, eye_pos, forward);
        sceVu0Normalize(direction, forward);
        WorkBuffer->used = 0;
        auto *walls = reinterpret_cast<CCPoly *>(WorkBuffer->Alloc(0x7D0));
        float span = DistVector(behind, look);
        box.max[0] = eye_pos[0] + span;
        box.min[0] = eye_pos[0] - span;
        box.max[2] = eye_pos[2] + span;
        box.min[2] = eye_pos[2] - span;
        box.max[1] = 100.0f + eye_pos[1];
        box.min[1] = eye_pos[1] - 100.0f;

        int wall_count = 0;

        if (EdMoveCharaInfo.fishing != 0) {
            wall_count = ground->PickUpCameraPoly(walls, box, 1);
        } else {
            wall_count = ground->PickUpCameraPoly(walls, box, 0xFFFF);
        }

        // A wall beside the eye turns the camera along it and stops it turning into it.
        if (CheckCameraWidth(walls, wall_count, eye_pos, 10.0f, wall, 0) != 0) {
            sceVu0SubVector(to_eye, look, eye_pos);
            sceVu0SubVector(to_wall, look, wall);
            to_eye[1] = to_wall[1] = 0.0f;
            sceVu0Normalize(to_eye, to_eye);
            sceVu0Normalize(to_wall, to_wall);
            sceVu0OuterProduct(cross, to_eye, to_wall);
            camera->AddAngle(0.5f * cross[1]);
            camera->SetAngleSoon(atan2f(-to_wall[0], -to_wall[2]));

            if (cross[1] > 0.0f) {
                left_clear = 0;
            }

            if (cross[1] < -0.0f) {
                right_clear = 0;
            }
        }

        int hits = CheckHits(walls, wall_count, look, behind, 32, hit_poly, hit_point, 1, 0);
        sceVu0CopyVector(origin, eye_pos);
        left[0] = forward[2];
        left[1] = 0.0f;
        left[2] = -forward[0];
        sceVu0Normalize(left, left);
        sceVu0ScaleVector(reach, left, 100.0f);
        sceVu0AddVector(left_end, origin, reach);
        int wall_hit = CheckHit(walls, wall_count, origin, left_end, hit, 1, 0);

        if (wall_hit >= 0) {
            float *normal = walls[wall_hit].normal;
            sceVu0Normalize(normal, normal);

            if (sceVu0InnerProduct(left, normal) > 0.001f) {
                left_distance = DistVector(origin, hit);
            } else if (DistVector(origin, hit) < 5.0f) {
                left_clear = 0;
            }
        }

        right[0] = -forward[2];
        right[1] = 0.0f;
        right[2] = forward[0];
        sceVu0Normalize(right, right);
        sceVu0ScaleVector(reach, right, 100.0f);
        sceVu0AddVector(right_end, origin, reach);
        wall_hit = CheckHit(walls, wall_count, origin, right_end, hit, 1, 0);

        if (wall_hit >= 0) {
            float *normal = walls[wall_hit].normal;
            sceVu0Normalize(normal, normal);

            if (sceVu0InnerProduct(right, normal) > 0.001f) {
                right_distance = DistVector(origin, hit);
            } else if (DistVector(origin, hit) < 5.0f) {
                right_clear = 0;
            }
        }

        if (hits > 0) {
            camera_far_dist = 80.0f;
            int last = -1;

            for (int i = 0; i < hits; i++) {
                sceVu0SubVector(towards, hit_point[i], eye_pos);

                if (sceVu0InnerProduct(forward, towards) < 0.0f) {
                    break;
                }

                last = i;
            }

            if (last >= 0) {
                keep_distance = 0;
                sceVu0CopyVector(facing, walls[hit_poly[last]].normal);

                if (sceVu0InnerProduct(direction, facing) > 0.0f) {
                    if (last + 1 < hits) {
                        float away = DistVector(eye_pos, hit_point[last + 1]);

                        if (away - DistVector(eye_pos, hit_point[last]) < 0.0f) {
                            CameraAutoMove(camera, &walls[hit_poly[last + 1]], hit_point[last + 1], left_distance, right_distance);
                        } else {
                            CameraAutoMove(camera, &walls[hit_poly[last]], hit_point[last], left_distance, right_distance);
                        }
                    } else {
                        CameraAutoMove(camera, &walls[hit_poly[last]], hit_point[last], left_distance, right_distance);
                        sceVu0CopyVector(last_hit, hit_point[last]);
                    }
                }
            } else {
                sceVu0CopyVector(facing_first, walls[hit_poly[0]].normal);

                if (sceVu0InnerProduct(forward, facing_first) < 0.0f) {
                    CameraAutoMove(camera, &walls[hit_poly[0]], hit_point[0], left_distance, right_distance);
                    keep_distance = 0;
                }
            }
        }

        // The eye keeps its height above whatever floor is under it.
        sceVu0CopyVector(behind, eye_pos);
        behind[1] += 15.0f;
        int floor_poly = CheckHitVertical(walls, wall_count, behind, -100.0f, hit, 0);

        if (floor_poly >= 0) {
            sceVu0Normalize(floor_normal, walls[floor_poly].normal);

            if (eye_pos[1] - hit[1] < 18.0f && (floor_normal[1] < 0.0f ? -floor_normal[1] : floor_normal[1]) > 0.5f) {
                eye_pos[1] = 18.0f + hit[1];
                camera->SetHeight(eye_pos[1] - look[1] - 0.01f);
            }
        }

        float half = 0.5f;

        if ((camera->GetDistance() < camera_near_dist * half || camera->GetHeight() > 60.0f) && shallow == 0 && hits > 0) {
            camera->SetDistance(camera_near_dist);
            camera->SetHeight(10.0f);
            camera->SetAngleSoon(camera->GetAngle());
            camera->AddAngle(3.14f);

            if (MapNo == 35 && eye_pos[2] > 200.0f) {
                camera->SetAngleSoon(3.14f);
            }

            camera->Step(-1);
        }
    }

    if (viewMode == 0 && interior == 0) {
        int   drift = 0;
        float rx = GetRXf();
        float ry = GetRYf();

        if (EdDebugCameraFlag == 0) {
            if (camera->GetHeight() < 30.0f) {
                camera->AddHeight(-ry);
            }
        } else {
            camera->AddHeight(-ry);
        }

        if (rx > 0.0f && left_clear != 0) {
            camera->AddAngle(0.03f * -rx);
        }

        if (rx < 0.0f && right_clear != 0) {
            camera->AddAngle(0.03f * -rx);
        }

        if (rx == 0.0f) {
            if (left_clear != 0 && PadOn(8) != 0) {
                camera->AddAngle(-DEG_TO_RAD);
            } else if (right_clear != 0 && PadOn(4) != 0) {
                camera->AddAngle(DEG_TO_RAD);
            } else {
                drift = 1;
            }
        }

        if (camera->GetDistance() > camera_far_dist) {
            camera->AddDistance(-(camera->GetDistance() - camera_far_dist) / 10.0f);
        }

        if (camera->GetDistance() < camera_near_dist) {
            camera->AddDistance(-(camera->GetDistance() - camera_near_dist) / 10.0f);
        }

        // With the right stick at rest the camera swings round behind a walking character.
        if (drift != 0) {
            float turn = lx;

            if (ly > 0.05f) {
                if (!(lx < 0.0f) && lx < 0.5f) {
                    lx = 0.5f;
                }

                if (lx <= 0.0f && lx > -0.5f) {
                    lx = -0.5f;
                }

                if (left_clear == 0) {
                    lx = 0.5f;
                }

                if (right_clear == 0) {
                    lx = -0.5f;
                }
            }

            lx *= DistVector(move) / 1.6f / 2.0f;

            if (lx > 0.1f && left_clear != 0) {
                turn = std::max(turn, 0.4f);

                turn = std::min(turn, 1.0f);

                camera->AddAngle(2.0f * (-DEG_TO_RAD * turn));
            }

            if (lx < -0.1f && right_clear != 0) {
                turn = std::min(turn, -0.4f);

                turn = std::max(turn, -1.0f);

                camera->AddAngle(2.0f * (-DEG_TO_RAD * turn));
            }
        }

        if (keep_distance != 0) {
            camera->SetDistance(camera_near_dist);
        }

        if (EdDebugCameraFlag == 0) {
            if (camera->GetHeight() > 60.0f) {
                camera->SetHeight(60.0f);
            }

            float lowest = 5.0f;

            if (shallow != 0) {
                lowest = 35.0f;
            }

            if (camera->GetHeight() < lowest) {
                camera->SetHeight(lowest);
            }

            if (camera->GetHeight() > 5.0f) {
                float drop = camera->GetHeight() - lowest;
                drop *= 0.05f;

                drop = std::max(drop, 0.15f);

                drop = std::min(drop, 0.5f);

                camera->AddHeight(-drop);
            }
        } else {
            if (PadOn(0x1000) != 0) {
                reference[1] += 2.0f;
            }

            if (PadOn(0x4000) != 0) {
                reference[1] -= 2.0f;
            }
        }

        // The button swings the camera round behind the character over thirty frames.
        static int rot_count = 0;

        if (PadOn(0x20) != 0 && EdDebugMoveFlag == 0) {
            rot_count = 30;
        }

        if (rot_count > 0) {
            float behind_angle = chara->GetRotation()->y - std::numbers::pi;
            int   turn_side = AngleCmp(behind_angle, camera->GetAngle(), 0.1f);

            if (turn_side < 0) {
                if (left_clear != 0) {
                    camera->AddAngle(-0.1f);
                } else {
                    rot_count = 0;
                }
            }

            if (turn_side > 0) {
                if (right_clear != 0) {
                    camera->AddAngle(0.1f);
                } else {
                    rot_count = 0;
                }
            }

            if (turn_side == 0) {
                rot_count = 0;
            }
        }

        rot_count--;

        rot_count = std::max(rot_count, 0);
    }

    int near_villager = EdSearchNearNPC(chara, EdVillager, 10);

    if (near_villager >= 0) {
        EdVillager[near_villager].talk_target = true;
    }

    if (EdMoveCharaInfo.fishing == 0) {
        int           acted = false;
        sceVu0FVECTOR here;
        sceVu0FVECTOR heading;
        chara->GetPosition(here);
        chara->GetRotation(heading);
        int             event_no = -1;
        int             system_event_no = -1;
        ED_EVENT_POINT *points = EdMoveCharaInfo.points;
        int             point_count = EdMoveCharaInfo.point_count;
        EdMoveCharaInfo.event_ready = false;
        ED_EVENT_PARAM *param = &EdMoveCharaInfo.param;

        if (EdGetEvent(points, point_count, param, here, heading, time) != 0) {
            if (param->kind == ED_EVENT_POINT_ITEM_BOX && PadDown(0x40) != 0 && (viewMode == 0 || interior != 0)) {
                int item = param->point->side;
                int refused = EdCheckGetItem(item);

                if (refused == 0) {
                    EdSetMapFlag(param->point->completion_flag, 1);
                    int attach = -1;

                    if (GetAddAttachItem(param->point->side) != 0) {
                        attach = rand() % 3 + 1;
                    }

                    EdGetItem(item, param->point->linked_value, attach);
                    EdItemGetMes(item, param->point->linked_value, attach, 40);
                    EdSetOpenItemBox(param->position, param->rotation);
                    SndSePlay(SE_BOX_OPEN, -1, 0);
                } else {
                    DontGetItemMes(refused);
                    SndSePlay(SE_BOX_OPEN, -1, 0);
                }
            }

            if (param->kind == ED_EVENT_POINT_EVENT && param->point->side > 0) {
                event_no = param->point->side;
            }

            if (param->kind == ED_EVENT_POINT_LADDER_BOTTOM || param->kind == ED_EVENT_POINT_LADDER_TOP) {
                EdEventInfo.draw_exclamation_mark = true;

                if (PadDown(0x40) != 0 && (interior != 0 || viewMode == 0)) {
                    EdInitHashigo(&EdEventInfo, param);
                    system_event_no = 1;
                }
            }

            EdMoveCharaInfo.event_ready = acted = true;
        }

        if (near_villager >= 10) {
            near_villager = -1;
        }

        if (acted == 0 && (viewMode == 0 || interior != 0) && near_villager >= 0 && EdVillagerInfo[near_villager].talk_event_no > 0 && EdVillagerInfo[near_villager].talk_event_level != 0 && EdTalkModeInit(&EdVillager[near_villager], -1) != 0) {
            event_no = EdVillagerInfo[near_villager].talk_event_no;
            acted = true;
        }

        if (acted == 0 && PadDown(0x40) != 0 && (viewMode == 0 || interior != 0) && key_lock == 0) {
            if (near_villager >= 0) {
                if (EdTalkModeInit(&EdVillager[near_villager], -1) != 0) {
                    event_no = 0x100;
                    acted = true;
                }
            } else if (poly_event > 0) {
                event_no = poly_event;
                acted = true;
            }
        }

        EdMoveCharaInfo.event_no = event_no;
        EdMoveCharaInfo.system_event_no = system_event_no;
        EdMoveCharaInfo.acted = acted;
    }

    float      float_weight = -1.0f;
    float      hook_weight = -1.0f;
    int        follow_line = 0;
    static int bgm_vol = 0;

    if (EdMoveCharaInfo.fishing != 0) {
        EdViewModeOff();

        if (EdDebugMoveFlag == 0) {
            camera->SetHeight(40.0f);
        }

        sceVu0FVECTOR stand;
        sceVu0FVECTOR facing;
        sceVu0FVECTOR float_pos;
        chara->GetPosition(stand);
        chara->GetRotation(facing);
        FishLineGetUki(float_pos);
        int        fish_no;
        int        fish_status = FishingFishStatus(&fish_no);
        static int st_cnt = 0;
        static int wait_cnt = 0;

        switch (chara_fishing) {
            case ED_FISHING_INIT:
                chara_fishing = ED_FISHING_STAND;
                FishingInitFishStatus();
                FishingAngleFish(-1);
                wait_cnt = 0;
                st_cnt = 0;
                break;
            case ED_FISHING_STAND:
                if (st_cnt > 120) {
                    EdFishingWalkHelpMes(FishingGetEsaItemNo());
                }

                FishingAngleFish(-1);

                if (EdPadDown(0x40, 0xFFFF) != 0) {
                    chara_fishing = ED_FISHING_CAST_START;
                    st_cnt = 0;
                }

                if (EdPadDown(0x20, 0xFFFF) != 0) {
                    EdMoveCharaInfo.event_no = 0x85;
                    EdMoveCharaInfo.system_event_no = -1;
                    EdMoveCharaInfo.acted = true;
                    chara->SetMotion(0, 0);
                }

                if (EdPadDown(0x80, 0xFFFF) != 0) {
                    EdMoveCharaInfo.event_no = 0x86;
                    EdMoveCharaInfo.system_event_no = -1;
                    EdMoveCharaInfo.acted = true;
                    chara->SetMotion(0, 0);
                }

                float_weight = 1.0f;
                break;
            case ED_FISHING_CAST_START:
                chara->SetMotion(4, 6);
                chara_fishing = ED_FISHING_CASTING;
                st_cnt = 0;
                break;
            case ED_FISHING_CASTING:
                chara_mode = 1;

                if (st_cnt == 90) {
                    SndSePlay(SE_FISHING_CAST, pos, -1.0f, -1.0f);
                }

                if (chara->motion_state == 3) {
                    chara->SetMotion(5, 0);
                    chara_fishing = ED_FISHING_WAIT_BITE;
                    st_cnt = 0;
                    wait_cnt = 0;
                }

                break;
            case ED_FISHING_WAIT_BITE: {
                EdFishingAngleHelpMEs(FishingGetEsaItemNo());
                chara_mode = 1;

                if (wait_cnt < 600) {
                    chara->SetMotion(5, 0);
                } else if (wait_cnt < 1200) {
                    chara->SetMotion(14, 0);
                } else {
                    if (wait_cnt == 1200) {
                        chara->SetMotion(15, 6);
                    }

                    if (wait_cnt > 1250 && chara->motion_state == 3) {
                        chara->SetMotion(5, 4);
                        wait_cnt = 0;
                    }
                }

                float reel_turn = -GamePad.GetLXf();
                facing[1] = AngleInterpolate(facing[1], facing[1] + 0.1f * reel_turn, 0.01f, INTERPOLATE_STEP);
                chara->SetRotation(facing);

                if (reel_turn != 0.0f) {
                    wait_cnt = 0;
                    chara->SetMotion(13, 0);
                }

                if (EdPadDown(0x40, 0xFFFF) != 0 || (st_cnt > 30 && FishingCheckUkiHook() != 0)) {
                    chara_fishing = ED_FISHING_REEL_IN;
                    chara->SetMotion(7, 6);
                    st_cnt = 0;
                } else {
                    if (fish_status == 7) {
                        chara_fishing = ED_FISHING_BITE_CHECK;
                    }

                    if (fish_status == 8) {
                        chara_fishing = ED_FISHING_HOOKED;
                    }
                }

                wait_cnt++;
                follow_line = 1;
                break;
            }
            case ED_FISHING_REEL_IN:
                if (st_cnt == 40) {
                    sceVu0FVECTOR hook;
                    FishLineGetHook(hook);

                    if (hook[1] < 5.0f + FishingGetWaterLevel()) {
                        SndSePlay(SE_FISHING_SINK, float_pos, -1.0f, -1.0f);
                    }
                }

                if (chara->motion_state == 3) {
                    chara->SetMotion(0, 0);
                    chara_fishing = ED_FISHING_STAND;
                    wait_cnt = 0;
                }

                if (st_cnt > 30) {
                    float_weight = 0.02f * static_cast<float>(st_cnt - 30);
                }

                float_weight = std::min(float_weight, 1.0f);

                break;
            case ED_FISHING_BITE_CHECK: {
                EdFishingAngleHelpMEs(FishingGetEsaItemNo());

                if (fish_status == 8) {
                    chara_fishing = ED_FISHING_HOOKED;
                } else if (fish_status != 7) {
                    chara_fishing = ED_FISHING_WAIT_BITE;
                    st_cnt = 0;
                    wait_cnt = 0;
                }

                if (EdPadDown(0x40, 0xFFFF) != 0) {
                    chara_fishing = ED_FISHING_SINK;
                    chara->SetMotion(6, 6);
                    st_cnt = 0;
                    SndSePlay(SE_FISHING_SINK, float_pos, -1.0f, -1.0f);

                    if (rand() % 100 < 20) {
                        EdFishingLostEsaMes();
                        FishingDeleteEsa();
                    }
                }

                float pull = -0.15f * static_cast<float>(rand()) / 2.1474836e9f;

                if (pull < -0.1f) {
                    sceVu0FVECTOR ripple;
                    FishLineGetUki(ripple);
                    ripple[1] = FishingGetWaterLevel();
                    GamePad.SetVibration(1, 80, 2);
                    EffectHamon(&EdEffectGroup, ripple, 10.0f);
                }

                FishPullHook(pull);
                follow_line = 1;
                break;
            }
            case ED_FISHING_HOOKED: {
                EdFishingAngleHelpMEs(FishingGetEsaItemNo());

                if (FishingFishStatus(nullptr) != 8) {
                    chara_fishing = ED_FISHING_WAIT_BITE;
                    wait_cnt = 0;

                    if (rand() % 100 < 30) {
                        EdFishingLostEsaMes();
                        FishingDeleteEsa();
                    }

                    st_cnt = 0;
                } else {
                    if (EdPadDown(0x40, 0xFFFF) != 0 || DebugMode) {
                        SndSePlay(SE_FISHING_CAST, pos, -1.0f, -1.0f);
                        chara_fishing = ED_FISHING_BATTLE;
                        chara->SetMotion(12, 2);
                        FishingBattleFish(fish_no);
                        char *fish_file = GetFishFileName(FishingFishKind(fish_no));

                        if (fish_file != nullptr) {
                            StartReadBG();
                            LoadFileBG(fish_file, reinterpret_cast<u_long128 *>(read_buffer), nullptr);
                        }

                        st_cnt = 0;
                    }

                    float tug = -(0.1f + 0.05f * static_cast<float>(rand()) / 2.1474836e9f);

                    if (tug < -0.12f) {
                        GamePad.SetVibration(1, 100, 10);
                    }

                    FishPullHook(tug);
                    follow_line = 1;
                }

                break;
            }
            case ED_FISHING_BATTLE:
                FishingDeleteEsa();

                if (st_cnt > 120 && ReadBGSync() == 0) {
                    BG_READ_INFO  *read_info = GetReadBGFile(0);
                    u_int         *fish_data = nullptr;
                    CDataAlloc2<1> arena(-1);

                    if (read_info != nullptr) {
                        fish_data = reinterpret_cast<u_int *>(read_info->buffer);
                        int free_blocks = EdVillagerBuffer.limit - EdVillagerBuffer.used;
                        arena.base = EdVillagerBuffer.base + static_cast<std::ptrdiff_t>(EdVillagerBuffer.used) * 16;
                        arena.limit = free_blocks;
                        arena.used = 0;
                    }

                    chara_fishing = ED_FISHING_LANDED;
                    chara->SetMotion(6, 6);
                    FishingBattleToAngleFish(fish_data, &arena);
                    SndSePlay(SE_FISHING_BITE, -1, 0);
                }

                GamePad.SetVibration(1, rand() % 40 + 80, 10);
                follow_line = 1;
                break;
            case ED_FISHING_LANDED:
                chara_fishing = ED_FISHING_RESULT;
                chara->SetMotion(10, 6);
                fishing_mes = ED_FISHING_MES_EVALUATE;
                follow_line = 1;
                bgm_vol = SndGetBgmVol();
                SndBgmFadeOut(60, 0);
                break;
            case ED_FISHING_SINK:
                if (chara->motion_state == 3) {
                    chara_fishing = ED_FISHING_STAND;
                    chara->SetMotion(0, 0);
                    FishingAngleFish(fish_no);
                    FishingInitFishStatus();
                    wait_cnt = 0;
                }

                if (st_cnt > 40) {
                    float_weight = 0.02f * static_cast<float>(st_cnt - 40);
                }

                float_weight = std::min(float_weight, 1.0f);

                break;
            case ED_FISHING_RESULT:
                camera->SetAngle(AngleLimit(facing[1]));
                camera->SetHeight(10.0f);
                camera->SetDistance(40.0f);
                camera->Step(-1);

                if (chara->motion_no == 10) {
                    if (chara->motion_state == 3) {
                        chara->SetMotion(8, 0);
                        SndSPSePlay(0x2F, -1);
                    }
                } else {
                    switch (fishing_mes) {
                        case ED_FISHING_MES_EVALUATE: {
                            int size;
                            int fish_points;
                            int fish_kind = FishingGetAngleFishSize(&size, &fish_points);

                            if (fish_kind < 0) {
                                fishing_mes = ED_FISHING_MES_DONE;
                            } else {
                                if (fish_kind == 5 || fish_kind == 17) {
                                    int *caught = &SaveData->mardan_garayan_caught;
                                    (*caught)++;
                                    SetFishMardanGarayanNum(1);
                                }

                                SaveData->AddFishingPoint(fish_points);
                                SaveData->SetFishingRank(fish_kind, static_cast<float>(size));
                                fishing_mes++;
                                EditMes1.mes_no[0] = GetFishMsgNo(fish_kind);
                                EditMes1.values[0] = size;
                                EditMes1.values[1] = fish_points;
                                EditMes1.values[2] = SaveData->GetFishingPoint();
                                EditMes1.tail_on = false;
                                EditMes1.auto_pos = MES_POS_TOP_LEFT;
                                EditMes1.MakeMesWin(2000);
                                EditMes1.auto_pos = MES_POS_TOP_LEFT;
                                int window[4];
                                EditMes1.AutoSet(window);
                            }

                            break;
                        }
                        case ED_FISHING_MES_WAIT:
                            if (EdMenuLoop(&EditMes1) != 0) {
                                EditMes1.auto_pos = MES_POS_TOP_LEFT;
                                fishing_mes++;
                            }

                            break;
                        case ED_FISHING_MES_DONE:
                            SndBgmFadeIn(60, bgm_vol, 0);
                            chara->SetMotion(0, 0);
                            chara_fishing = ED_FISHING_FINISH;
                            chara->SetMotion(9, 6);
                            st_cnt = 0;
                            break;
                        default:
                            break;
                    }
                }

                hook_weight = 1.0f;
                break;
            case ED_FISHING_FINISH:
                camera->SetAngle(AngleLimit(facing[1]));
                camera->SetHeight(10.0f);
                camera->SetDistance(30.0f);
                camera->Step(-1);

                if (st_cnt == 120) {
                    FishingDeleteAngleFish();
                    SndSePlay(SE_FISHING_LANDED, -1, 0);
                }

                if (chara->motion_state == 3) {
                    chara->SetMotion(0, 0);
                    chara_fishing = ED_FISHING_INIT;
                    FishingDeleteAngleFish();
                    FishingAngleFish(-1);
                    FishingInitFishStatus();
                }

                hook_weight = float_weight = 1.0f;
                break;
            default:
                break;
        }

        // The camera watches the point halfway between the angler and the float.
        if (follow_line != 0) {
            sceVu0FVECTOR centre;
            FishLineGetUki(centre);
            sceVu0AddVector(centre, centre, pos);
            sceVu0ScaleVector(centre, centre, 0.5f);
            camera->SetFollow(centre[0], centre[1], centre[2]);
        }

        st_cnt++;
    } else {
        chara_fishing = ED_FISHING_INIT;
    }

    chara->Step();

    if (viewMode == 0) {
        chara->ShadowStep();
    }

    if (viewMode == 0) {
        chara->ClothStep(0);
    }

    if (viewMode != 0) {
        chara->SetMotion(0, 0);
    }

    if (EdMoveCharaInfo.fishing != 0) {
        sceVu0FVECTOR rod;
        chara->GetPosition(rod);

        if (chara->frame != nullptr) {
            sceVu0FVECTOR origin = {0.0f, 0.0f, 0.0f, 1.0f};
            sceVu0FVECTOR hook_pos;
            sceVu0FVECTOR float_pos;
            CFrame       *frame = chara->frame->SearchFrame("sao");

            if (frame != nullptr) {
                frame->GetWorldPosition(rod, origin);
            }

            frame = chara->frame->SearchFrame("uki");

            if (frame != nullptr) {
                frame->GetWorldPosition(float_pos, origin);
            }

            frame = chara->frame->SearchFrame("hari");

            if (frame != nullptr) {
                frame->GetWorldPosition(hook_pos, origin);
            }

            frame = chara->frame->SearchFrame("maru_uki");

            if (frame != nullptr) {
                frame->attr.draw_on = 2;
            }

            frame = chara->frame->SearchFrame("turibari");

            if (frame != nullptr) {
                frame->attr.draw_on = 2;
            }

            FishLineSetUki(float_pos, float_weight);
            FishLineSetHook(hook_pos, hook_weight);
        }

        CFish *fish = FishingGetBattleFish();

        if (fish != nullptr) {
            sceVu0FVECTOR fish_pos;
            fish->GetPosition(fish_pos);
            FishLineSetHook(fish_pos, 1.0f);
        }

        sceVu0FVECTOR before;
        sceVu0FVECTOR after;
        FishLineGetUki(before);
        FishLineStep(rod, rod);
        FishingStepFish();
        FishLineGetUki(after);
        // The float throws up ripples where it crosses the surface.
        float water_level = FishingGetWaterLevel();

        if (after[1] < water_level && !(before[1] < water_level)) {
            float fall_height = after[1] - before[1];

            if ((fall_height < 0.0f ? -fall_height : fall_height) > 0.8f) {
                SndSePlay(SE_FISHING_SPLASH, after, -1.0f, -1.0f);
                after[1] = FishingGetWaterLevel();
                EffectHamon(&EdEffectGroup, after, 15.0f);
                EffectHamon(&EdEffectGroup, after, 20.0f);
                EffectHamon(&EdEffectGroup, after, 25.0f);
                EffectHamon(&EdEffectGroup, after, 30.0f);
            }
        }

        if (after[1] > water_level && before[1] <= water_level) {
            float rise = after[1] - before[1];

            if ((rise < 0.0f ? -rise : rise) > 0.1f) {
                after[1] = FishingGetWaterLevel();
                EffectHamon(&EdEffectGroup, after, 10.0f);
                EffectHamon(&EdEffectGroup, after, 15.0f);
                EffectHamon(&EdEffectGroup, after, 20.0f);
            }
        }
    }

    EdStepOpenItemBox();
    float time_after = chara->motion_type.state.time;
    chara->FootSoundEnable(chara->move_info.landed);
    chara->SetFootSoundID(chara->move_info.ground_poly.attr.foot_sound);

    // In the first-person view the feet sound by the distance walked instead of the motion.
    if (viewMode != 0 && interior != 0 && chara->move_info.landed != 0) {
        static float cnt = 0.0f;
        float        walked = cnt;
        velocity[1] = 0.0f;
        cnt += DistVector(velocity);

        if (cnt > 10.0f && walked <= 10.0f) {
            SndPlayFootSound(chara->move_info.ground_poly.attr.foot_sound, 1, pos);
        }

        if (cnt > 20.0f && walked <= 20.0f) {
            SndPlayFootSound(chara->move_info.ground_poly.attr.foot_sound, 0, pos);
        }

        if (cnt > 20.0f) {
            cnt = 0.0f;
        }
    }

    sceVu0CopyVector(chara->ground_ambient[0], EditMapInfo->character_ambient[0]);
    sceVu0CopyVector(chara->ground_ambient[1], EditMapInfo->character_ambient[1]);
    chara->ground_ambient_no = -1;
    chara->fade_out = 0;
    sceVu0FVECTOR unused = {0.0f, 0.0f, 0.0f, 0.0f};

    if (chara->move_info.ground_found != 0) {
        s16 floor_kind = chara->move_info.poly.attr.area_kind;

        switch (floor_kind) {
            case AREA_KIND_FADE_OUT:
                chara->fade_out = 1;
                break;
            case AREA_KIND_AMBIENT_0:
            case AREA_KIND_AMBIENT_1:
                chara->ground_ambient_no = floor_kind - 3;
                break;
            default:
                break;
        }
    }

    EdMoveCharaInfo.motion_time_before = time_before;
    EdMoveCharaInfo.motion_time_after = time_after;
}

void EdInitHashigo(ED_EVENT_INFO *info, ED_EVENT_PARAM *param) {
    if (param->kind == ED_EVENT_POINT_LADDER_BOTTOM) {
        sceVu0CopyVector(info->vector_arguments[1], param->position);
        sceVu0CopyVector(info->vector_arguments[0], param->camera_pos);
        info->integer_arguments[0] = 0;
    } else {
        sceVu0CopyVector(info->vector_arguments[0], param->position);
        sceVu0CopyVector(info->vector_arguments[1], param->camera_pos);
        info->integer_arguments[0] = 1;
    }

    sceVu0CopyVector(info->vector_arguments[2], param->rotation);
    info->flag_arguments[0] = static_cast<int>(param->point->trigger_range[3]);
    info->integer_arguments[1] = param->point->side;
    info->integer_arguments[2] = param->point->linked_value;
}

int EdInitGotoInterior(ED_EVENT_INFO *info, ED_EVENT_PARAM *param) {
    sceVu0CopyVector(info->vector_arguments[0], param->position);
    sceVu0CopyVector(info->vector_arguments[1], param->rotation);
    sceVu0CopyVector(info->vector_arguments[2], param->camera_pos);
    return param->point->minimum_progress;
}

} // namespace dc::game
