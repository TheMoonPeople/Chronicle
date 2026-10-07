#include "ebattle.hpp"

#include <cmath>

#include "character.hpp"
#include "dataread.hpp"
#include "editloop.hpp"
#include "gamepad.hpp"
#include "menu_save.hpp"
#include "snd.hpp"

#include "platform/config.hpp"
#include "platform/input.hpp"

struct EB_KEY {
    int frame;
    int buttons;
    int mode;
    int pressed;
    int hit;
    int passed;
    int highlight;
};

extern EB_KEY eb_key[64];
extern int    eb_cool_flag;
extern float  old_time;
extern float  speed;
extern int    ebattle_flag;
extern int    eb_count;
extern int    eb_finish_cnt;
extern int    eb_end_count;
extern int    eb_key_count;
extern int    fade_bgm;
extern int    play_fanfare;
extern int    debug_mode;
extern float  now_time;
extern int    eb_key_num;
extern int    eb_result;
extern int    ok_draw_cnt;
extern int    ok_type;
extern int    ok_effect_button;

void draw_ok_loop();

namespace {

void set_draw_ok(int type, int button) {
    ok_effect_button = button;
    ok_draw_cnt = 30;
    ok_type = type;
}

} // namespace

PC_OVERRIDE int EBLoop() {
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
        int loaded_size;
        StartReadBG();
        SndSPSeLoadBG(0x2F, read_buffer, &loaded_size);
    }

    ReadBG();

    int     i;
    float   frame_speed = speed;
    EB_KEY *active = NULL;
    int     cool = 0;

    for (i = 0; i < eb_key_num; i++) {
        EB_KEY *key = &eb_key[i];
        float   distance = eb_count - key->frame;
        distance *= frame_speed;
        int early = 0;

        if (key->mode > 0) {
            early = (6 - key->mode) << 6;
        }

        key->highlight = false;

        if (distance > 0.0f) {
            if (distance < 48.0f) {
                active = key;
            } else {
                key->passed = true;
            }

            if (distance < 24.0f) {
                cool = 1;
            }
        } else {
            if (distance > -16.0f) {
                active = key;
            }

            if (early > 0 && distance <= 16.0f - early) {
                key->highlight = true;
            }
        }

        if (debug_mode != 0 && eb_count == key->frame) {
            SndSePlay(SE_EB_HIT, -1, 0);
            SndSePlay(MENU_SOUND_CONFIRM, -1, 0);
        }
    }

    int failed = 0;

    if (active != NULL) {
        active->pressed |= GamePad.GetPadDown();

        if (active->pressed == active->buttons) {
            if (active->hit == 0) {
                if (cool) {
                    SndSePlay(SE_EB_HIT_COOL, -1, 0);
                } else {
                    SndSePlay(SE_EB_HIT, -1, 0);
                }

                set_draw_ok(cool, active - eb_key);
                eb_cool_flag &= cool;
                eb_key_count++;
            }

            active->hit = 1;
        }

        if (active->buttons != (active->buttons | active->pressed)) {
            failed = 1;
        }
    } else if (GamePad.GetPadDown()) {
        failed = 1;
    }

    for (i = 0; i < eb_key_num; i++) {
        EB_KEY *key = &eb_key[i];

        if (key->passed == 0) {
            break;
        }

        if (key->buttons != key->pressed) {
            failed = 1;
        }
    }

    if (failed && !ConfigGet().qte_always_win && debug_mode == 0 && eb_key_count < eb_key_num &&
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

            SndSPSePlay(0x2F, -1);
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

#include <libvu0.h>
#include "camera.hpp"
#include "camera_port.hpp"
#include "camerafollow.hpp"
#include "character.hpp"
#include "edit.hpp"

// Defined by ps2/src/ebattle.cpp without a header declaration.
extern int   viewMode;
extern int   chara_mode;
extern float viewAngleH;
extern float viewAngleV;

int PortEdCheckKeyMode(int mode);

// The right stick of the town's walk: EdMoveChara turns its camera by RX (0.03 a tick, unless a
// wall is on that side) and raises it by RY while it is under 30. RX stays the stick's: the mouse's
// turn goes to EditLoop's request (camera_port.hpp), which checks it against the walls itself; so
// the drift behind a walking character and R1 and L1, which wait for RX to rest, see only the stick.
// RY takes the mouse as camera_port.hpp describes. In an interior's first-person view EdMoveChara
// turns the character by RX only while the left stick rests, so the mouse turns it in EyeCamera.

PC_OVERRIDE float EdGetRXf(int mode) {
    if (PortEdCheckKeyMode(mode)) {
        CCameraFollow *camera = EdMoveCharaInfo.camera;
        if (viewMode == 0 && !EdMoveCharaInfo.interior && camera != NULL) {
            TownMouseRecord(camera, EdMoveCharaInfo.fishing != 0);
        }
        return GamePad.GetRXf();
    }

    return 0.0f;
}

// EdMoveChara reads it for the town's camera only; EyeCamera below reads the stick itself.
PC_OVERRIDE float EdGetRYf(int mode) {
    if (PortEdCheckKeyMode(mode)) {
        CCameraFollow *camera = EdMoveCharaInfo.camera;
        float          stick = GamePad.GetRYf();
        if (camera == NULL) {
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

// Retail's, with the mouse turning the view while it is the one shown. In an interior the view
// takes its heading from the character's (EdMoveChara), so the mouse turns the character too.
PC_OVERRIDE void EyeCamera(CCamera *camera, CCharacter *character, int right_stick) {
    float stick_x;
    float stick_y;
    bool  unlocked = (chara_mode & 3) == 0;

    if (right_stick != 0) {
        stick_x = 0.0f;
        stick_y = unlocked && PortEdCheckKeyMode(1) ? -GamePad.GetRYf() : 0.0f;
    } else {
        stick_x = unlocked ? EdGetLXf(1) : 0.0f;
        stick_y = unlocked ? -EdGetLYf(1) : 0.0f;
    }

    if (stick_x > 0.0f) {
        float rate = 0.02f;
        viewAngleH -= stick_x * rate;

        if (viewAngleH < -PI) {
            viewAngleH += TWO_PI;
        }
    }

    if (stick_x < -0.0f) {
        float rate = 0.02f;
        viewAngleH -= stick_x * rate;

        if (viewAngleH > PI) {
            viewAngleH -= TWO_PI;
        }
    }

    if (stick_y > 0.0f && viewAngleV < 0.65f) {
        float rate = 0.02f;
        viewAngleV += stick_y * rate;
    }

    if (stick_y < -0.0f && viewAngleV > -1.0f) {
        float rate = 0.02f;
        viewAngleV += stick_y * rate;
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
