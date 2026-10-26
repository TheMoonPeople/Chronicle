#include "stubs/ebattle.hpp"
#include "common.h"

#include <libvu0.h>

#include <cmath>
#include <cstdlib>

#include "camerafollow.hpp"
#include "character.hpp"
#include "clsmes.hpp"
#include "dataalloc.hpp"
#include "dataread.hpp"
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
#include "mainselect.hpp"
#include "mapparts.hpp"
#include "mathutil.hpp"
#include "menu_save.hpp"
#include "mglib.hpp"
#include "npcharacter.hpp"
#include "rect.hpp"
#include "savedata.hpp"
#include "shop.hpp"
#include "snd.hpp"
#include "sysmes.hpp"
#include "texture.hpp"

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

STATIC_ASSERT(sizeof(EB_KEY) == 0x1C);

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

STATIC_ASSERT(sizeof(EB_MOTION) == 0x14);

/**
 * Stores the opening wipe's rectangle, zeroing its members from last to first.
 */
class CEbRect {
public:
    s32 x;      /**< Distance of the left edge from the left of the screen. */
    s32 y;      /**< Distance of the top edge from the top of the screen. */
    s32 width;  /**< Distance from the left edge to the right edge. */
    s32 height; /**< Distance from the top edge to the bottom edge. */

    CEbRect() {
        height = 0;
        width = 0;
        y = 0;
        x = 0;
    }
} __attribute__((aligned(16)));

/**
 * Stores one rectangle of a prompt's sprite set, zeroing its members from last to first.
 */
class CEbSpriteRect : public CRect_i_ {
public:
    CEbSpriteRect();
};

/** Storage of draw_rect, the part of the screen the opening wipe has reached. */
CEbRect draw_rect_store;

CRect_i_ Caution(256, 0, 56, 56);

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

/** Stores the key state for each event-battle prompt. */
EB_KEY eb_key[64];

/** Stores the motion sequence of the event battle; the first entry ends it. */
EB_MOTION eb_motion[32] = {{-1}};

/** Whether the prompts light up early. */
int eb_cool_flag = 1;
/** Battle time of the previous update. */
float old_time = -1.0f;
/** Battle frames advanced on each update. */
float speed = 1.0f;
/** Index of the prompt currently being answered. */
int now_eb_key = -1;

/** Whether an event battle is running. */
int ebattle_flag;
/** Whether the event battle's opening is running. */
int ebattle_intro_flag;
/** Frames since the event battle started. */
int eb_count;
/** Frames since the opening started. */
int eb_intro_cnt;
/** Frames since the last prompt finished. */
int eb_finish_cnt;
/** Frames since the event battle ended. */
int eb_end_count;
/** How the event battle ended. */
int eb_result;
/** Number of prompts answered. */
int eb_key_count;
/** Index of the button prompt being drawn. */
int now_button_no;
/** Character the event battle is played by. */
CCharacter *eb_chara;
/** Whether the background music fades when the battle ends. */
int fade_bgm;
/** Whether the fanfare plays when the battle ends. */
int play_fanfare;
/** Current battle time. */
float now_time;
/** Frames since the last sound was played. */
int sound_cnt;
/** Diagnostic display mode. */
int debug_mode;
/** Event-battle texture. */
CTexture *tex;
/** Second event-battle texture. */
CTexture *tex2;
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

/**
 * Stores one motion segment used to convert animation time into battle frames.
 */
struct EB_MOTION_ENTRY {
    int   motion_no; /**< Motion requested from the character. */
    float start;     /**< First frame of the motion. */
    float end;       /**< Frame after the motion ends. */
    float speed;     /**< Frames advanced on each update. */
    int   frames;    /**< Number of updates in the motion. */
};

/**
 * Stores one timed controller prompt in an event battle.
 */
struct EB_KEY_ENTRY {
    int frame;     /**< Battle frame the prompt reaches the hit mark on. */
    int buttons;   /**< Buttons the prompt asks for. */
    int mode;      /**< How early the prompt lights before the hit mark; 0 never. */
    int pressed;   /**< Buttons pressed so far while the prompt is live. */
    int hit;       /**< Whether the prompt has been answered. */
    int passed;    /**< Whether the prompt has gone past the hit mark. */
    int highlight; /**< Whether the prompt is drawn lit this frame. */
};

static void  init_draw_ok();
static void  set_draw_ok(int type, int button);
void         draw_ok_loop();
void         DrawButton(int buttons, int x, int y, float scale, int early);
static void  DrawButtonSub(int x, int y, int texture_x, int texture_y, float scale);
static void  draw_ok(int x);
static float button_scale(int button);

/* @ 0x168100 (0x10 bytes) -- EBInitialize__Fv */
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

/* @ 0x168110 (0xE0 bytes) -- EBInit__Ff */
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

/* @ 0x1681F0 (0x10 bytes) -- EBFinishSound__Fii */
void EBFinishSound(int do_fade_bgm, int do_play_fanfare) {
    fade_bgm = do_fade_bgm;
    play_fanfare = do_play_fanfare;
}

/**
 * Looks up the textures the event battle's opening needs.
 *
 * @mangled EBInitIntro__Fv
 * @address 0x168200
 * @size 0xA8
 */
void EBInitIntro() {
    tex = TexManager.GetTexture("ebat", -1);

    if (tex == NULL) {
        return;
    }

    tex2 = TexManager.GetTexture("ebat2", -1);

    if (tex2 == NULL) {
        return;
    }

    eb_intro_cnt = 0;
    ebattle_intro_flag = 1;
    // The wipe opens from the right edge, so it starts with no width.
    draw_rect = CRect_i_(0x280, 0, 0, 0x1C0);
}

void EBSetMotion(CCharacter *character, int *motions) {
    if (character == NULL) {
        return;
    }

    int i;

    for (i = 0;; i++) {
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

        if (info == NULL) {
            return;
        }

        float start = (float) info->start;
        float end = (float) info->end;
        eb_motion[i].start = start;
        eb_motion[i].end = end;
        eb_motion[i].speed = info->speed;
        int frames = (int) ((end - start) / info->speed);
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
        EB_KEY_ENTRY *key = (EB_KEY_ENTRY *) &eb_key[eb_key_num++];
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

        if (mode > 5) {
            mode = 5;
        }

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

    do {
    } while (SndSPSeSyncBG() != 0);

    InitReadBG();
}

/**
 * Runs the event battle's opening and reports when it ends.
 *
 * @mangled EBIntroLoop__Fv
 * @address 0x1685C0
 * @size 0xCC
 */
int EBIntroLoop() {
    int width;

    if (ebattle_intro_flag == 0) {
        return 1;
    }

    // The wipe crosses the screen over a hundred frames.
    width = eb_intro_cnt * 640 / 100;

    if (width > 640) {
        width = 640;
    }

    draw_rect.x = 640 - width;
    draw_rect.width = width;
    eb_intro_cnt++;

    if (eb_intro_cnt > 100) {
        draw_rect = CRect_i_(0, 0, 640, 448);
        return 1;
    }

    return 0;
}

/**
 * Runs one frame of the event battle and reports the result.
 *
 * @mangled EBLoop__Fv
 * @address 0x168690
 * @size 0x4E4
 */
int EBLoop();

/**
 * Draws the event battle's prompt strip, its opening caution mark and its result overlay.
 *
 * @mangled EBDraw__Fv
 * @address 0x168B80
 * @size 0x560
 */
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
        int   x = position;
        DrawButton(key->buttons, x, 0x140, button_scale(i), key->highlight);
    }

    if (eb_key_count > 0) {
        EB_KEY *key = &eb_key[eb_key_count - 1];
        float   delta = eb_count - key->frame;
        delta *= frame_speed;
        float position = 200.0f - delta;
        int   x = position;
        draw_ok(x);
    }
}

/**
 * Draws one button prompt of the event battle.
 *
 * @mangled DrawButton__Fiiifi
 * @address 0x1690E0
 * @size 0x260
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
 *
 * @mangled DrawButtonSub__Fiiiif
 * @address 0x169340
 * @size 0xE0
 */
static void DrawButtonSub(int x, int y, int texture_x, int texture_y, float scale) {
    int      width = 32.0f * scale;
    int      height = 32.0f * scale;
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
 *
 * @mangled init_draw_ok__Fv
 * @address 0x169420
 * @size 0x14
 */
static void init_draw_ok() {
    ok_draw_cnt = 0;
    ok_effect_button = -1;
}

/**
 * Starts the confirmation effect for the selected button.
 *
 * @mangled set_draw_ok__Fii
 * @address 0x169440
 * @size 0x18
 */
static void set_draw_ok(int type, int button) {
    ok_effect_button = button;
    ok_draw_cnt = 30;
    ok_type = type;
}

/**
 * Counts down the active confirmation effect.
 *
 * @mangled draw_ok_loop__Fv
 * @address 0x169460
 * @size 0x2C
 */
void draw_ok_loop() {
    if (ok_draw_cnt > 0) {
        --ok_draw_cnt;

        if (ok_draw_cnt < 0) {
            ok_draw_cnt = 0;
        }
    }
}

/**
 * Draws the success flash after a button is pressed in time.
 *
 * @mangled draw_ok__Fi
 * @address 0x169490
 * @size 0x2A0
 */
static void draw_ok(int x) {
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
        int      left = 200 - (int) (width >> 1);
        int      height = texel->height;
        int      top = 318 - height;
        CRect_i_ result_screen(left, top, width, height);

        set2DSprite(GetVif1Packet(), tex2, result_screen, texel->x, texel->y);
    }

    texel = &spark_texel;
    int spark_x = x + 0x18;

    for (int i = 0; i < 8; ++i) {
        sceVu0Normalize(dir[i], dir[i]);
        sceVu0ScaleVector(offset, dir[i], 2.0f * (float) (30 - ok_draw_cnt));
        int half;
        int left;
        int top;
        int alpha;
        int height;
        int width;
        int bottom;
        width = texel->width;
        half = width >> 1;
        left = spark_x + (int) offset[0] - half;
        bottom = (int) offset[1] + 0x160;
        height = texel->height;
        top = bottom - height - 2;
        alpha = 0x80 - (int) ((float) (30 - ok_draw_cnt * 2) * 128.0 / 30.0);

        if (alpha > 0) {
            CRect_i_ screen(left, top, width, height);
            set2DSprite(GetVif1Packet(), tex, screen, spark_texel, (unsigned char) alpha);
        }
    }
}

/**
 * Gives the scale a button prompt draws at while it flashes.
 *
 * @mangled button_scale__Fi
 * @address 0x169730
 * @size 0x90
 */
static float button_scale(int button) {
    if (button != ok_effect_button) {
        return 1.0f;
    }

    // The prompt swells over the first five frames of the flash and settles
    // back over the next five.
    int   elapsed = 30 - ok_draw_cnt;
    float scale = 1.0f;

    if (elapsed < 5) {
        scale += 0.2f * (float) elapsed;
    } else if (elapsed < 10) {
        scale += 0.2f * (float) (10 - elapsed);
    }

    return scale;
}

/**
 * Zeroes a prompt's sprite rectangle from its last member to its first. PAL only.
 *
 * @mangled __ct__8CRect_i_Fv
 * @address 0x16a0a0
 * @size 0x1c
 */
CEbSpriteRect::CEbSpriteRect() {
    height = 0;
    width = 0;
    y = 0;
    x = 0;
}

static int key_mode = 0xFFFF;

/** Closest the editor camera comes to the character. */
float camera_near_dist = 70.0f;
/** Farthest the editor camera goes from the character. */
float camera_far_dist = 80.0f;

/**
 * Tests whether an editor input mode currently owns the controller.
 */
static int check_key_mode(int mode) {
    if (mode == 0xFFFF) {
        return 1;
    }

    return (key_mode & mode) != 0;
}

int EdSetKeyMode(int mode) {
    return key_mode = mode;
}

float EdGetRXf(int mode);

float EdGetRYf(int mode);

float EdGetLXf(int mode);

float EdGetLYf(int mode);

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

/**
 * Returns the low two character-control lock bits.
 *
 * @mangled keylock__Fv
 * @address 0x1699E0
 * @size 0x10
 */
static int keylock() {
    return chara_mode & 3;
}

/**
 * Returns the right stick's horizontal input, or zero while editor input is locked.
 */
static float GetRXf() {
    if (keylock() != 0) {
        return 0.0f;
    }

    return EdGetRXf(1);
}

/**
 * Returns the right stick's vertical input, or zero while editor input is locked.
 */
static float GetRYf() {
    if (keylock() != 0) {
        return 0.0f;
    }

    return EdGetRYf(1);
}

/**
 * Returns the left stick's horizontal input, or zero while editor input is locked.
 */
static float GetLXf() {
    if (keylock() != 0) {
        return 0.0f;
    }

    return EdGetLXf(1);
}

/**
 * Returns the left stick's vertical input, or zero while editor input is locked.
 */
static float GetLYf() {
    if (keylock() != 0) {
        return 0.0f;
    }

    return EdGetLYf(1);
}

/**
 * Reports whether a button is held, unless the pad is locked.
 *
 * @mangled PadOn__Fi
 * @address 0x169AF0
 * @size 0x40
 */
static int PadOn(int keys) {
    if (keylock() != 0) {
        return 0;
    }

    return EdPadOn(keys, 1);
}

/**
 * Reports whether a button was just pressed, unless the pad is locked.
 *
 * @mangled PadDown__Fi
 * @address 0x169B30
 * @size 0x40
 */
static int PadDown(int keys) {
    if (keylock() != 0) {
        return 0;
    }

    return EdPadDown(keys, 1);
}

/**
 * Moves the camera towards a point, keeping it clear of the collision.
 *
 * @mangled CameraAutoMove__FP13CCameraFollowP6CCPolyPfff
 * @address 0x169B70
 * @size 0x20C
 */
static void CameraAutoMove(CCameraFollow *camera, CCPoly *poly, float *position, float left_distance, float right_distance) {
    float reference[4];
    float offset[4];
    float step;

    camera->GetRef(reference);
    offset[0] = position[0] - reference[0];
    offset[2] = position[2] - reference[2];
    offset[1] = 0.0f;
    camera->SetDistance(0.1f + DistVector(offset));

    step = 0.0f;
    float excess = camera->GetDistance() - camera_near_dist;

    if (excess < step) {
        step = -excess / 10.0f;
    }

    if (excess > 0.0f) {
        step = excess / 15.0f;
    }

    if (step > 2.0f) {
        step = 2.0f;
    }

    if (step < 0.05f) {
        step = 0.05f;
    }

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
 * Disables the editor camera-view mode.
 *
 * @mangled EdViewModeOff__Fv
 * @address 0x169D80
 * @size 0xC
 */
void EdViewModeOff() {
    viewMode = 0;
}

/**
 * Places the eye camera on a character's head.
 *
 * @mangled InitEyeCamera__FP10CCharacter
 * @address 0x169D90
 * @size 0x34
 */
static void InitEyeCamera(CCharacter *character) {
    // The camera looks the way the character faces.
    viewAngleH = character->GetRotation()->y;
    viewAngleV = 0.0f;
}

/**
 * Aims the eye camera from a character's head.
 *
 * @mangled EyeCamera__FP7CCameraP10CCharacteri
 * @address 0x169DD0
 * @size 0x1B0
 */
void EyeCamera(CCamera *camera, CCharacter *character, int right_stick);

void EdInitCameraParam(CCameraFollow *camera) {
    if (camera != 0) {
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

/**
 * Aims the editor's eye camera from a character's head.
 *
 * @mangled EdEyeCamera__FP7CCameraP10CCharacter
 * @address 0x169FF0
 * @size 0x128
 */
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

/**
 * Returns the horizontal editor camera angle.
 *
 * @mangled EdAGetViewAngleH__Fv
 * @address 0x16A130
 * @size 0xC
 */
float EdAGetViewAngleH() {
    return viewAngleH;
}

/**
 * Returns the vertical editor camera angle.
 *
 * @mangled EdAGetViewAngleV__Fv
 * @address 0x16A140
 * @size 0xC
 */
float EdAGetViewAngleV() {
    return viewAngleV;
}

/**
 * Sets both editor camera angles.
 *
 * @mangled EdASetViewAngle__Fff
 * @address 0x16A150
 * @size 0x10
 */
void EdASetViewAngle(float horizontal, float vertical) {
    viewAngleH = horizontal;
    viewAngleV = vertical;
}

/**
 * Reports whether a villager's model is set up and enabled for drawing.
 */
static inline int IsVillagerActive(CNPCharacter *villager) {
    bool active = false;

    if (villager->initialized != 0 && villager->draw_enabled != 0) {
        active = true;
    }

    return active;
}

void EdMoveChara() {
    int             near_villager;
    CCamera        *view_camera;
    int             key_lock;
    CEditGround    *ground;
    float           angle;
    float           time;
    int             motion;
    float           move_z;
    float           move_x;
    float           ly;
    float           lx;
    float           time_before;
    CCPoly         *polys;
    int             poly_count;
    int             poly_event;
    int             shallow;
    int             left_clear;
    float          *normal;
    CCameraFollow  *camera;
    int             keep_distance;
    CMainChara     *chara;
    int             interior;
    float           float_weight;
    float           hook_weight;
    int             follow_line;
    float           time_after;
    float           motion_speed;
    float           stick;
    CFrame         *frame;
    float           left_distance;
    float           right_distance;
    float           span;
    int             wall_count;
    int             hits;
    int             wall_hit;
    int             right_clear;
    CCPoly         *walls;
    int             last;
    float           away;
    int             floor_poly;
    int             drift;
    float           rx;
    float           ry;
    float           turn;
    float           lowest;
    float           drop;
    float           behind_angle;
    int             turn_side;
    int             acted;
    int             event_no;
    int             system_event_no;
    ED_EVENT_POINT *points;
    int             point_count;
    ED_EVENT_PARAM *param;
    int             item;
    int             refused;
    int             attach;
    int             fish_status;
    float           reel_turn;
    float           pull;
    char           *fish_file;
    float           tug;
    BG_READ_INFO   *read_info;
    u_int          *fish_data;
    int             free_blocks;
    int             fish_kind;
    int            *caught;
    CFish          *fish;
    float           water_level;
    float           fall_height;
    float           rise;
    float           walked;
    s16             floor_kind;
    int             i;

    static sceVu0FVECTOR reference = {0.0f, 0.0f, 0.0f, 0.0f};

    EdMoveCharaInfo.event_no = -1;
    EdMoveCharaInfo.system_event_no = -1;
    time = EdMoveCharaInfo.time;
    camera = EdMoveCharaInfo.camera;
    view_camera = EdMoveCharaInfo.view_camera;
    key_lock = EdMoveCharaInfo.key_lock;
    chara = (CMainChara *) EdMoveCharaInfo.chara;
    interior = EdMoveCharaInfo.interior;
    ground = EdMoveCharaInfo.ground;

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

    angle = camera->GetAngleH();
    lx = GetLXf();
    ly = GetLYf();

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

    motion = 0;
    motion_speed = -1.0f;

    if (viewMode == 0) {
        move_x = lx * cosf(angle) + ly * sinf(angle);
        move_z = ly * cosf(angle) - lx * sinf(angle);

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
            stick = sqrtf(lx * lx + ly * ly);

            if (stick > 0.85f && EdMoveCharaInfo.fishing == 0) {
                motion = 1;
            } else {
                motion = 2;
                motion_speed = 0.8f * (0.2f + stick);

                if (motion_speed > 0.85f) {
                    motion_speed = 0.85f;
                }
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
        move_x = ly * sinf(angle);
        move_z = ly * cosf(angle);
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
    time_before = chara->GetNowTime();
    WorkBuffer__2->used = 0;
    polys = (CCPoly *) WorkBuffer__2->Alloc(0x7D0);
    poly_count = 0;

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

        for (i = 0; i < EdMoveCharaInfo.parts_count; i++) {
            frame = EdMoveCharaInfo.parts[i].GetCollisionFrame();

            if (frame != NULL) {
                poly_count += frame->PickUpNearPoly(&polys[poly_count], box);
            }
        }
    }

    poly_count += EdEventPointCpPoly(pos, EdMoveCharaInfo.points, EdMoveCharaInfo.point_count, &polys[poly_count], time);

    if (EdMoveCharaInfo.fishing != 0) {
        poly_count += FishingPickUpPoly(&polys[poly_count]);
    }

    for (i = 0; i < 10; i++) {
        if (IsVillagerActive(&EdVillager[i])) {
            poly_count += EdVillager[i].PickUpPoly(pos, &polys[poly_count]);
        }
    }

    if (poly_count > 200) {
        printf("cpoly over!!!! %d\n", poly_count);
    }

    CVector3_f_ fall;
    chara->GetVelocity(&fall);
    velocity[1] = fall.y;
    MoveCheck(pos, velocity, next, &chara->move_info, polys, poly_count, 0);

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
    poly_event = GetEventPoly(eye, ahead, &event_poly, &event_index, event_hit, polys, poly_count, 0);
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

    if (fall.y < -4.0f) {
        fall.y = -4.0f;
    }

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

    shallow = 0;

    if (chara->move_info.ground_found != 0 && chara->move_info.poly.attr.area_kind == AREA_KIND_HIGH_CAMERA) {
        shallow = 1;
    }

    right_clear = left_clear = keep_distance = 1;

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

        right_distance = left_distance = 1000000;
        camera->GetPos(eye_pos);
        camera->GetRef(look);
        sceVu0SubVector(forward, look, eye_pos);
        sceVu0SubVector(behind, eye_pos, forward);
        sceVu0Normalize(direction, forward);
        WorkBuffer__2->used = 0;
        walls = (CCPoly *) WorkBuffer__2->Alloc(0x7D0);
        span = DistVector(behind, look);
        box.max[0] = eye_pos[0] + span;
        box.min[0] = eye_pos[0] - span;
        box.max[2] = eye_pos[2] + span;
        box.min[2] = eye_pos[2] - span;
        box.max[1] = 100.0f + eye_pos[1];
        box.min[1] = eye_pos[1] - 100.0f;

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

        hits = CheckHits(walls, wall_count, look, behind, 32, hit_poly, hit_point, 1, 0);
        sceVu0CopyVector(origin, eye_pos);
        left[0] = forward[2];
        left[1] = 0.0f;
        left[2] = -forward[0];
        sceVu0Normalize(left, left);
        sceVu0ScaleVector(reach, left, 100.0f);
        sceVu0AddVector(left_end, origin, reach);
        wall_hit = CheckHit(walls, wall_count, origin, left_end, hit, 1, 0);

        if (wall_hit >= 0) {
            normal = walls[wall_hit].normal;
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
            normal = walls[wall_hit].normal;
            sceVu0Normalize(normal, normal);

            if (sceVu0InnerProduct(right, normal) > 0.001f) {
                right_distance = DistVector(origin, hit);
            } else if (DistVector(origin, hit) < 5.0f) {
                right_clear = 0;
            }
        }

        if (hits > 0) {
            camera_far_dist = 80.0f;
            last = -1;

            for (i = 0; i < hits; i++) {
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
                        away = DistVector(eye_pos, hit_point[last + 1]);

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
        floor_poly = CheckHitVertical(walls, wall_count, behind, -100.0f, hit, 0);

        if (floor_poly >= 0) {
            sceVu0Normalize(floor_normal, walls[floor_poly].normal);

            if (eye_pos[1] - hit[1] < 18.0f) {
                if ((floor_normal[1] < 0.0f ? -floor_normal[1] : floor_normal[1]) > 0.5f) {
                    eye_pos[1] = 18.0f + hit[1];
                    camera->SetHeight(eye_pos[1] - look[1] - 0.01f);
                }
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
        drift = 0;
        rx = GetRXf();
        ry = GetRYf();

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
            turn = lx;

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
                if (turn < 0.4f) {
                    turn = 0.4f;
                }

                if (turn > 1.0f) {
                    turn = 1.0f;
                }

                camera->AddAngle(2.0f * (-DEG_TO_RAD * turn));
            }

            if (lx < -0.1f && right_clear != 0) {
                if (turn > -0.4f) {
                    turn = -0.4f;
                }

                if (turn < -1.0f) {
                    turn = -1.0f;
                }

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

            lowest = 5.0f;

            if (shallow != 0) {
                lowest = 35.0f;
            }

            if (camera->GetHeight() < lowest) {
                camera->SetHeight(lowest);
            }

            if (camera->GetHeight() > 5.0f) {
                drop = camera->GetHeight() - lowest;
                drop *= 0.05f;

                if (drop < 0.15f) {
                    drop = 0.15f;
                }

                if (drop > 0.5f) {
                    drop = 0.5f;
                }

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
            behind_angle = chara->GetRotation()->y - PI_D;
            turn_side = AngleCmp(behind_angle, camera->GetAngle(), 0.1f);

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

        if (rot_count < 0) {
            rot_count = 0;
        }
    }

    near_villager = EdSearchNearNPC(chara, EdVillager, 10);

    if (near_villager >= 0) {
        EdVillager[near_villager].talk_target = true;
    }

    if (EdMoveCharaInfo.fishing == 0) {
        acted = false;
        sceVu0FVECTOR here;
        sceVu0FVECTOR heading;
        chara->GetPosition(here);
        chara->GetRotation(heading);
        system_event_no = event_no = -1;
        points = EdMoveCharaInfo.points;
        point_count = EdMoveCharaInfo.point_count;
        EdMoveCharaInfo.event_ready = false;
        param = &EdMoveCharaInfo.param;

        if (EdGetEvent(points, point_count, param, here, heading, time) != 0) {
            if (param->kind == ED_EVENT_POINT_ITEM_BOX && PadDown(0x40) != 0 && (viewMode == 0 || interior != 0)) {
                item = param->point->side;
                refused = EdCheckGetItem(item);

                if (refused == 0) {
                    EdSetMapFlag(param->point->completion_flag, 1);
                    attach = -1;

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

        if (acted == 0 && (viewMode == 0 || interior != 0) && near_villager >= 0) {
            if (EdVillagerInfo[near_villager].talk_event_no > 0 && EdVillagerInfo[near_villager].talk_event_level != 0 && EdTalkModeInit(&EdVillager[near_villager], -1) != 0) {
                event_no = EdVillagerInfo[near_villager].talk_event_no;
                acted = true;
            }
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

    hook_weight = float_weight = -1.0f;
    follow_line = 0;
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
        int fish_no;
        fish_status = FishingFishStatus(&fish_no);
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

                reel_turn = -GamePad.GetLXf();
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
                    float_weight = 0.02f * (float) (st_cnt - 30);
                }

                if (float_weight > 1.0f) {
                    float_weight = 1.0f;
                }

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

                pull = -0.15f * (float) rand() / 2.1474836e9f;

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
            case ED_FISHING_HOOKED:
                EdFishingAngleHelpMEs(FishingGetEsaItemNo());

                if (FishingFishStatus(NULL) != 8) {
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
                        fish_file = GetFishFileName(FishingFishKind(fish_no));

                        if (fish_file != NULL) {
                            StartReadBG();
                            LoadFileBG(fish_file, (u_long128 *) read_buffer, NULL);
                        }

                        st_cnt = 0;
                    }

                    tug = -(0.1f + 0.05f * (float) rand() / 2.1474836e9f);

                    if (tug < -0.12f) {
                        GamePad.SetVibration(1, 100, 10);
                    }

                    FishPullHook(tug);
                    follow_line = 1;
                }

                break;
            case ED_FISHING_BATTLE:
                FishingDeleteEsa();

                if (st_cnt > 120 && ReadBGSync() == 0) {
                    read_info = GetReadBGFile(0);
                    fish_data = NULL;
                    CDataAlloc2<1> arena(-1);

                    if (read_info != NULL) {
                        fish_data = (u_int *) read_info->buffer;
                        free_blocks = EdVillagerBuffer.limit - EdVillagerBuffer.used;
                        arena.base = EdVillagerBuffer.base + EdVillagerBuffer.used * 16;
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
                    float_weight = 0.02f * (float) (st_cnt - 40);
                }

                if (float_weight > 1.0f) {
                    float_weight = 1.0f;
                }

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
                            fish_kind = FishingGetAngleFishSize(&size, &fish_points);

                            if (fish_kind < 0) {
                                fishing_mes = ED_FISHING_MES_DONE;
                            } else {
                                if (fish_kind == 5 || fish_kind == 17) {
                                    caught = &SaveData->mardan_garayan_caught;
                                    (*caught)++;
                                    SetFishMardanGarayanNum(1);
                                }

                                SaveData->AddFishingPoint(fish_points);
                                SaveData->SetFishingRank(fish_kind, (float) size);
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

        if (chara->frame != NULL) {
            sceVu0FVECTOR origin = {0.0f, 0.0f, 0.0f, 1.0f};
            sceVu0FVECTOR hook_pos;
            sceVu0FVECTOR float_pos;
            frame = chara->frame->SearchFrame("sao");

            if (frame != NULL) {
                frame->GetWorldPosition(rod, origin);
            }

            frame = chara->frame->SearchFrame("uki");

            if (frame != NULL) {
                frame->GetWorldPosition(float_pos, origin);
            }

            frame = chara->frame->SearchFrame("hari");

            if (frame != NULL) {
                frame->GetWorldPosition(hook_pos, origin);
            }

            frame = chara->frame->SearchFrame("maru_uki");

            if (frame != NULL) {
                frame->attr.draw_on = 2;
            }

            frame = chara->frame->SearchFrame("turibari");

            if (frame != NULL) {
                frame->attr.draw_on = 2;
            }

            FishLineSetUki(float_pos, float_weight);
            FishLineSetHook(hook_pos, hook_weight);
        }

        fish = FishingGetBattleFish();

        if (fish != NULL) {
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
        water_level = FishingGetWaterLevel();

        if (after[1] < water_level && !(before[1] < water_level)) {
            fall_height = after[1] - before[1];

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
            rise = after[1] - before[1];

            if ((rise < 0.0f ? -rise : rise) > 0.1f) {
                after[1] = FishingGetWaterLevel();
                EffectHamon(&EdEffectGroup, after, 10.0f);
                EffectHamon(&EdEffectGroup, after, 15.0f);
                EffectHamon(&EdEffectGroup, after, 20.0f);
            }
        }
    }

    EdStepOpenItemBox();
    time_after = chara->motion_type.state.time;
    chara->FootSoundEnable(chara->move_info.landed);
    chara->SetFootSoundID(chara->move_info.ground_poly.attr.foot_sound);

    // In the first-person view the feet sound by the distance walked instead of the motion.
    if (viewMode != 0 && interior != 0 && chara->move_info.landed != 0) {
        static float cnt = 0.0f;
        walked = cnt;
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
        floor_kind = chara->move_info.poly.attr.area_kind;

        switch (floor_kind) {
            case AREA_KIND_FADE_OUT:
                chara->fade_out = 1;
                break;
            case AREA_KIND_AMBIENT_0:
            case AREA_KIND_AMBIENT_1:
                chara->ground_ambient_no = floor_kind - 3;
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
    info->flag_arguments[0] = (int) param->point->trigger_range[3];
    info->integer_arguments[1] = param->point->side;
    info->integer_arguments[2] = param->point->linked_value;
}

int EdInitGotoInterior(ED_EVENT_INFO *info, ED_EVENT_PARAM *param) {
    sceVu0CopyVector(info->vector_arguments[0], param->position);
    sceVu0CopyVector(info->vector_arguments[1], param->rotation);
    sceVu0CopyVector(info->vector_arguments[2], param->camera_pos);
    return param->point->minimum_progress;
}
