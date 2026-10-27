#include "common.h"

#include <libgraph.h>
#include <libpkt.h>
#include <libvu0.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <span>

#include "camera.hpp"
#include "camerafollow.hpp"
#include "character.hpp"
#include "dataalloc.hpp"
#include "dataread.hpp"
#include "frame.hpp"
#include "framevu1.hpp"
#include "main.hpp"
#include "mathutil.hpp"
#include "mds.hpp"
#include "mdt.hpp"
#include "mglib.hpp"
#include "renderinfo.hpp"
#include "snd.hpp"
#include "sound.hpp"
#include "stubs/title/op_d.hpp"
#include "texture.hpp"
#include "title/op_a.hpp"
#include "title/op_b.hpp"
#include "title/op_d.hpp"
#include "title/opening.hpp"
#include "title/script.hpp"
#include "title/seireiking.hpp"
#include "visual.hpp"
#include "wind.hpp"

// rect.h's four-argument constructor assigns the fields in the opposite order to this one.
template <class T>
class CRect {
public:
    T x; /**< Left edge. */
    T y; /**< Top edge. */
    T w; /**< Width. */
    T h; /**< Height. */

    CRect(T left, T top, T width, T height) : x(left), y(top), w(width), h(height) {}
} __attribute__((aligned(16)));

void set2DSprite(sceVif1Packet *packet, CTexture *texture, const CRect<int> &src, const CRect<int> &dst, u_char alpha);

namespace {

/**
 * Sprite cues the script raises during the spirit king and meadow scenes, as CScript::sprite holds them.
 */
enum class OpDSprite : int {
    None = 0,       /**< No cue. */
    HallLight = 1,  /**< Raise the hall's ambient light. */
    RuinStill1 = 2, /**< First still of the ruined village. */
    RuinStill2 = 3, /**< Second still of the ruined village. */
    RuinStill3 = 4, /**< Third still of the ruined village. */
    Caption1 = 5,   /**< First explanation caption. */
    Caption2 = 6,   /**< Second explanation caption. */
};

CWind Wind;

// A rising star turns towards its target above the screen a step at a time and restarts from the
// bottom once it leaves the top.
void DrawRisingStar(float &x, float &y, float &rot, float target_x, float start_x) {
    const float heading = atan2f(target_x - x, -107.0f - y);

    if (heading < rot) {
        rot = rot - 0.05f;
    }

    if (heading > rot) {
        rot += 0.05f;
    }

    x = x + static_cast<float>(sin(rot) * 4.0);
    y = y + static_cast<float>(cos(rot) * 4.0);

    const int alpha = y < 32.0f ? std::max(static_cast<int>(128.0f - 4.0f * (32.0f - y)), 0) : 128;

    if (y <= 0.0f) {
        x = start_x;
        y = 245.0f;
    }

    const int cut = static_cast<int>((245.0f - y) / 2.0f);

    set2DSprite(GetVif1Packet(), TexManager.GetTexture("0519p", -1), CRect<int>(static_cast<int>(x), static_cast<int>(y), 178 - cut, 191 - cut),
                CRect<int>(210, 0, 178, 178), static_cast<u_char>(alpha));
}

// A falling star heads straight for its target and restarts from its start once it reaches it.
void DrawFallingStar(float &x, float &y, float target_x, float start_x, float start_y) {
    const float heading = atan2f(target_x - x, 193.0f - y);

    x = x + static_cast<float>(sin(heading) * 3.0);
    y = y + static_cast<float>(cos(heading) * 3.0);

    const int alpha = y > 148.0f ? std::max(static_cast<int>(128.0f - 4.0f * (y - 159.0f)), 0) : 128;

    if (y >= 191.0f) {
        x = start_x;
        y = start_y;
    }

    set2DSprite(GetVif1Packet(), TexManager.GetTexture("0519p", -1), CRect<int>(static_cast<int>(x), static_cast<int>(y), 210, 221),
                CRect<int>(0, 0, 210, 206), static_cast<u_char>(alpha));
}

} // namespace

CFrameVu1  *Hamon[4];
float       HScale[5];
CCharacter  Effect;
CSeireiKing SeireiKing;

int        amb3;
CFrameVu1 *SkyFrame;

void OpD_LoadDataBG() {
    while (ReadBGSync()) {
    }

    LoadFileBG("opdat/norn4/seirei.pak", reinterpret_cast<u_long128 *>(read_buffer), nullptr);
    CScript__2.load_no = OP_SCENE_NONE;
}

// The music bank is packed behind the scene in the same file.
void OpD_LoadDataBG2() {
    int size = 0;

    while (ReadBGSync()) {
    }

    LoadFileBG("opdat/norn4/seirei2.pak", reinterpret_cast<u_long128 *>(read_buffer), &size);
    SndBgmLoadBG(0, reinterpret_cast<u_int *>(reinterpret_cast<u_long128 *>(read_buffer) + (size >> 4) + 1), nullptr);
    CScript__2.load_no = OP_SCENE_NONE;
}

// The dome is thirty-two vertices around; the two colours alternate two vertices at a time.
void SkyColor(CFrameVu1 *frame) {
    if (frame == nullptr) {
        return;
    }

    CVisualVu1 *visual = frame->GetVisual();

    if (visual == nullptr) {
        return;
    }

    auto *model = reinterpret_cast<MDT_HEADER *>(visual->GetMDTDataAddress());

    if (model == nullptr || model->colour_count == 0) {
        return;
    }

    constexpr int vertex_count = 32;

    std::span     colors{reinterpret_cast<sceVu0FVECTOR *>(reinterpret_cast<char *>(model) + model->colour_ofs), vertex_count};
    sceVu0FVECTOR sky_color = {50.0f, 100.0f, 255.0f, 0.0f};
    sceVu0FVECTOR band_color = {255.0f, 255.0f, 255.0f, 0.0f};

    sky_color[0] = sky_color[0] / 128.0f;
    sky_color[1] = sky_color[1] / 128.0f;
    sky_color[2] = sky_color[2] / 128.0f;
    band_color[0] = band_color[0] / 128.0f;
    band_color[1] = band_color[1] / 128.0f;
    band_color[2] = band_color[2] / 128.0f;

    for (int i = 0; i < vertex_count; i++) {
        const sceVu0FVECTOR &source = i % 4 < 2 ? sky_color : band_color;

        colors[i][0] = source[0];
        colors[i][1] = source[1];
        colors[i][2] = source[2];
        colors[i][3] = 1.0f;
    }

    frame->attr.remake_pending = 1;
}

void OpD_MotionProcess() {
    constexpr int actor_count = 23;

    for (int i = 0; i < actor_count; i++) {
        if (CScript__2.obj[i].disp) {
            if (CScript__2.obj[i].motion_end != -1 &&
                Chara__3[i].motion_type.state.time > static_cast<float>(Chara__3[i].motion_type.motion_info[CScript__2.obj[i].motion].end - 1)) {
                CScript__2.obj[i].motion = CScript__2.obj[i].motion_end;
                CScript__2.obj[i].motion_end = -1;
            }

            Chara__3[i].motion_type.state.blend_step = CScript__2.obj[i].step;

            if (CScript__2.obj[i].step == 1.0f && CScript__2.obj[i].motion != Chara__3[i].motion_no) {
                Chara__3[i].motion_type.state.time = static_cast<float>(Chara__3[i].motion_type.motion_info[CScript__2.obj[i].motion].start);
            }

            Chara__3[i].motion_no = CScript__2.obj[i].motion;
            Chara__3[i].motion_flags = 0;
            Chara__3[i].motion_speed = -1.0f;
        }
    }

    // Frame of the camera's model each actor is placed on.
    constexpr std::array<const char *, actor_count> frame_names = {
        "", "", "", "", "", "tyou2", "", "", "c01d", "", "", "c01d", "", "", "", "", "", "", "", "", "", "c03c", "tyou"};
    sceVu0FMATRIX matrix;

    for (int i = 0; i < actor_count; i++) {
        if (CScript__2.obj[i].disp) {
            CFrame *frame = Cam__2[SceneNp__2].frame->SearchFrame(const_cast<char *>(frame_names[i]));

            if (frame) {
                frame->GetLWMatrix(matrix);
                Chara__3[i].SetRotation(0.0f, atan2f(matrix[2][0], matrix[2][2]), 0.0f);
                Chara__3[i].SetPosition(matrix[3][0], matrix[3][1], matrix[3][2]);
            }
        }
    }

    sceVu0FVECTOR dir;

    if (CScript__2.camera_start < 115) {
        dir[0] = 0.0f;
        dir[2] = 0.0f;
        dir[1] = 0.0f;
        dir[3] = 0.0f;
        Wind.SetDir(dir);
        Wind.SetVelocity(0.0f);
    } else {
        dir[0] = -0.1f;
        dir[2] = 0.1f;
        dir[1] = 0.0f;
        dir[3] = 0.0f;
        Wind.SetDir(dir);
        Wind.SetVelocity(0.2f);
    }

    const int cloth_actor = CScript__2.camera_start > 111 ? 11 : 8;

    Chara__3[cloth_actor].wind = static_cast<int>(reinterpret_cast<intptr_t>(&Wind));
    Wind.Step();

    static int camera = 0;

    if (camera != CScript__2.camera_start) {
        camera = CScript__2.camera_start;

        switch (camera) {
            case 101:
            case 104:
            case 123:
                Chara__3[cloth_actor].ClothStep(-1);

                for (int i = 0; i < 20; i++) {
                    Chara__3[cloth_actor].ClothStep(0);
                }

                break;

            case 106:
            case 109:
            case 112:
            case 115:
            case 125:
                Chara__3[cloth_actor].ClothStep(-1);

                for (int i = 0; i < 10; i++) {
                    Chara__3[cloth_actor].ClothStep(0);
                }

                break;

            default:
                break;
        }
    }
}

// Each camera cue fires once and re-arms when the camera moves off it.
void OpD_SoundProcess() {
    {
        static int fired = 0;

        if (CScript__2.camera_start == 111) {
            if (!fired) {
                OpPlayVolSE(MIDI_PORT_SE_DEFAULT, 40, 42, 1.0f);
                OpPlayVolSE(MIDI_PORT_SE_DEFAULT, 40, 44, 1.0f);
                OpPlayVolSE(MIDI_PORT_SE_DEFAULT, 40, 49, 1.0f);
                OpPlayVolSE(MIDI_PORT_SE_DEFAULT, 40, 51, 1.0f);
                OpPlayVolSE(MIDI_PORT_SE_DEFAULT, 40, 45, 1.0f);
                OpPlayVolSE(MIDI_PORT_SE_DEFAULT, 40, 47, 1.0f);
                OpPlayVolSE(MIDI_PORT_SE_DEFAULT, 40, 48, 1.0f);
                fired = 1;
            }
        } else {
            fired = 0;
        }
    }

    {
        static int fired = 0;

        if (CScript__2.camera_start == 101) {
            if (CScript__2.mes_no == 101 && !fired) {
                OpBgmPlay();
                CSnd.SQ_Play(MIDI_PORT_AMBIENT, 0);
                fired = 1;
            }
        } else {
            fired = 0;
        }
    }

    {
        static int fired = 0;

        if (CScript__2.camera_start == 110) {
            if (!fired) {
                CSnd.Fade(MIDI_PORT_BGM, -1.0f, 0);
                fired = 1;
            }
        } else {
            fired = 0;
        }
    }

    {
        static int fired = 0;

        if (CScript__2.camera_start == 115) {
            if (!fired) {
                OpBgmPlay();
                CSnd.SQ_Play(MIDI_PORT_AMBIENT, 0);
                fired = 1;
            }
        } else {
            fired = 0;
        }
    }

    {
        static int fired = 0;

        if (CScript__2.camera_start == 118) {
            if (!fired) {
                CSnd.Fade(MIDI_PORT_BGM, -1.0f, 0);
                fired = 1;
            }
        } else {
            fired = 0;
        }
    }

    {
        static int fired = 0;

        if (CScript__2.camera_start == 123) {
            if (!fired) {
                SndStopAllSe();
                CSnd.Stop(MIDI_PORT_BGM);
                CSnd.StopVoice(0);
                CSnd.SetReverb(0, 4, 30);
                CSnd.SetReverb(1, 2, 5);
                CSnd.LoadSoundFileFromPack("o04c.txt", read_buffer);
                CSnd.SetVol(MIDI_PORT_SE_TITLE, 256);
                CSnd.SetVol(MIDI_PORT_SE_DEFAULT, 256);
                CSnd.SetVol(MIDI_PORT_UNK_D, 256);
                CSnd.SetVol(MIDI_PORT_SE_SPECIAL, 256);

                while (SndSyncBG()) {
                }

                SndBgmPlay(0);
                CSnd.SQ_Play(MIDI_PORT_AMBIENT, 0);
                fired = 1;
            }
        } else {
            fired = 0;
        }
    }

    if (CScript__2.obj[21].disp) {
        static int    wait = 0;
        sceVu0FVECTOR position;

        sceVu0CopyVector(position, Chara__3[21].pos);
        const auto motion_frame = static_cast<float>(static_cast<int>(Chara__3[21].motion_type.state.time));

        // The wait keeps a motion that stalls inside a footstep window from repeating the step.
        if (wait == 0) {
            if (motion_frame > 43.0f && motion_frame < 45.0f) {
                OpPlayVolPanSE(position, 50.0f, 300.0f, MIDI_PORT_SE_DEFAULT, 21, 20);
                wait = 4;
            } else if (motion_frame > 53.0f && motion_frame < 55.0f) {
                OpPlayVolPanSE(position, 50.0f, 300.0f, MIDI_PORT_SE_DEFAULT, 21, 21);
                wait = 4;
            }
        } else {
            wait = wait - 1;
        }
    }
}

// One model drawn twice, spun opposite ways about the frame the actor carries it on.
void EffectAtraPrizum() {
    if (CScript__2.obj[11].disp) {
        static float rot1 = 0.0f;
        static float rot2 = 0.0f;

        if (!Pause) {
            rot1 += 0.01f;

            if (rot1 > 3.14f) {
                rot1 = rot1 - 6.28f;
            }

            rot2 = rot2 - 0.01f;

            if (rot2 < -3.14f) {
                rot2 += 6.28f;
            }
        }

        sceVu0FMATRIX matrix;
        sceVu0FVECTOR ambient;

        sceVu0CopyVector(ambient, ambientlight);
        ambient[3] = 32.0f;
        MGSetAmbient(ambient);

        sceGsAlpha alpha = mgAlpha;

        alpha.bits.a = 0;
        alpha.bits.b = 2;
        alpha.bits.c = 0;
        alpha.bits.d = 1;
        setAlphaFlag(Vif1Packet, &alpha);
        TexManager.ReloadTexture(Vif1Packet, 4);

        CFrame *frame = Chara__3[11].frame->SearchFrame("atrp");

        if (frame) {
            frame->GetLWMatrix(matrix);

            if (CScript__2.camera_start == 112) {
                sceVu0FVECTOR position;
                sceVu0FMATRIX spin;

                sceVu0CopyVector(position, matrix[3]);
                sceVu0UnitMatrix(spin);
                sceVu0RotMatrixY(spin, spin, rot1);
                sceVu0MulMatrix(matrix, matrix, spin);
                sceVu0CopyVector(matrix[3], position);
                Effect.frame->SetTransMatrix(matrix);
                Effect.motion_no = 0;
                Effect.motion_flags = 0;
                Effect.motion_speed = -1.0f;

                if (!Pause) {
                    Effect.Step();
                }

                Effect.Draw();
                frame->GetLWMatrix(matrix);
                sceVu0CopyVector(position, matrix[3]);
                sceVu0UnitMatrix(spin);
                sceVu0RotMatrixY(spin, spin, rot2);
                sceVu0MulMatrix(matrix, matrix, spin);
                sceVu0CopyVector(matrix[3], position);
                Effect.frame->SetTransMatrix(matrix);
                MGDraw(Effect.frame);
            }
        }

        if (!Pause) {
            SeireiKing.Step();
        }

        SeireiKing.Draw(matrix[3], 0.2f, 1);

        sceVu0FVECTOR eye;

        sceVu0CopyVector(eye, OP_CharaFrame__2->position);
        SeireiKing.Draw2(&OP_MainCamera, matrix[3], eye);
        setAlphaFlag(Vif1Packet, &mgAlpha);
        MGSetAmbient(ambientlight);
    }
}

// On camera 113 the source circles the effect, so the light sweeps while nothing else moves.
void RollLight(float *target) {
    if (CScript__2.camera_start >= 111 && CScript__2.camera_start < 114) {
        CFrame *frame = CScript__2.camera_start == 111 ? Cam__2[SceneNp__2].frame->SearchFrame("hikari")
                                                       : Chara__3[11].frame->SearchFrame("atrp");

        sceVu0FMATRIX matrix;
        sceVu0FMATRIX dirs;
        sceVu0FMATRIX colors;
        sceVu0FVECTOR dir;

        frame->GetLWMatrix(matrix);

        switch (CScript__2.camera_start) {
            case 111:
            case 112:
                dir[0] = matrix[3][0] - target[0];
                dir[1] = matrix[3][1];
                dir[2] = matrix[3][2] - target[2];
                sceVu0Normalize(dir, dir);
                sceVu0CopyMatrix(dirs, light);
                sceVu0CopyMatrix(colors, lightcolor);
                dirs[0][1] = dir[0];
                dirs[1][1] = dir[1];
                dirs[2][1] = dir[2];
                colors[1][0] = 255.0f;
                colors[1][1] = 255.0f;
                colors[1][2] = 255.0f;
                break;

            case 113: {
                static float angle = 0.0f;

                angle += 0.03f;

                if (angle > 3.14f) {
                    angle = angle - 6.28f;
                }

                dir[0] = static_cast<float>(matrix[3][0] + sin(angle) * 25.0 - target[0]);
                dir[1] = static_cast<float>(matrix[3][1] + 5.0);
                dir[2] = static_cast<float>(matrix[3][2] + cos(angle) * 25.0 - target[2]);
                sceVu0Normalize(dir, dir);
                sceVu0CopyMatrix(dirs, light);
                sceVu0CopyMatrix(colors, lightcolor);
                dirs[0][1] = dir[0];
                dirs[1][1] = dir[1];
                dirs[2][1] = dir[2];
                colors[1][0] = 128.0f;
                colors[1][1] = 128.0f;
                colors[1][2] = 128.0f;
            } break;

            default:
                break;
        }

        MGSetPLight(dirs, colors);
    }
}

// The trail is stepped every other tick so it drifts rather than darts.
void EffectSeireiKing(float size) {
    CFrame *frame = Cam__2[SceneNp__2].frame->SearchFrame("hikari");

    if (frame) {
        sceVu0FMATRIX matrix;

        frame->GetLWMatrix(matrix);
        TexManager.ReloadTexture(Vif1Packet, 4);

        sceVu0FVECTOR head;

        head[0] = matrix[3][0];
        head[1] = matrix[3][1];
        head[2] = matrix[3][2];

        if (!Pause) {
            static int tick = 0;

            tick = tick + 1;

            if (tick > 1) {
                tick = 0;
            }

            if (tick == 0) {
                SeireiKing.Step();
            }
        }

        SeireiKing.Draw(head, size, 2);
    }
}

void LensFreaProcess() {
    sceVu0FVECTOR      world;
    std::array<int, 4> screen{};
    sceVu0FVECTOR      local = {0.0f, 0.0f, 0.0f, 1.0f};
    CFrame            *frame = SkyFrame->SearchFrame("sun1");

    if (frame) {
        frame->GetWorldPosition(world, local);

        if (MGRotTransPers2D(screen.data(), world, 0) && TexManager.GetTexture("lensfler", -1)) {
            LensFlare(TexManager.GetTexture("lensfler", -1), world, 255, 255, 255);
        }
    }
}

// Two counters cross-fade groups of caption pieces; each star trail steers towards a point off the
// screen and restarts when it leaves.
void Setsumei() {
    static int tick_a = 0;
    static int phase_a = 0;

    tick_a = tick_a + 1;

    if (tick_a > 30) {
        tick_a = 0;
        phase_a = phase_a + 1;

        if (phase_a > 1) {
            phase_a = 0;
        }
    }

    static int tick_b = 0;
    static int phase_b = 0;

    tick_b = tick_b + 1;

    if (tick_b > 30) {
        tick_b = 0;
        phase_b = phase_b + 1;

        if (phase_b > 2) {
            phase_b = 0;
        }
    }

    static int fade_a1 = 128;
    static int fade_a2 = 0;

    if (phase_a == 0) {
        fade_a1 = 128;

        if (fade_a2 > 0) {
            fade_a2 = fade_a2 - 8;
        }
    } else {
        fade_a2 = 128;

        if (fade_a1 > 0) {
            fade_a1 = fade_a1 - 8;
        }
    }

    static int fade_b1 = 128;
    static int fade_b2 = 0;
    static int fade_b3 = 0;

    switch (phase_b) {
        case 0:
            fade_b1 = 128;

            if (fade_b2 > 0) {
                fade_b2 = fade_b2 - 8;
            }

            if (fade_b3 > 0) {
                fade_b3 = fade_b3 - 8;
            }

            break;

        case 1:
            fade_b2 = 128;

            if (fade_b1 > 0) {
                fade_b1 = fade_b1 - 8;
            }

            if (fade_b3 > 0) {
                fade_b3 = fade_b3 - 8;
            }

            break;

        case 2:
            fade_b3 = 128;

            if (fade_b1 > 0) {
                fade_b1 = fade_b1 - 8;
            }

            if (fade_b2 > 0) {
                fade_b2 = fade_b2 - 8;
            }

            break;

        default:
            break;
    }

    set2DSprite(GetVif1Packet(), TexManager.GetTexture("0519", -1), CRect<int>(0, 0, 640, SCREEN_HEIGHT), CRect<int>(0, 0, 640, 448), 128);
    set2DSprite(GetVif1Packet(), TexManager.GetTexture("0519p", -1), CRect<int>(540, 254, 88, 128), CRect<int>(89, 206, 88, 119), static_cast<u_char>(fade_a1));
    set2DSprite(GetVif1Packet(), TexManager.GetTexture("0519p", -1), CRect<int>(540, 254, 88, 128), CRect<int>(0, 206, 88, 119), static_cast<u_char>(fade_a2));
    set2DSprite(GetVif1Packet(), TexManager.GetTexture("0519p", -1), CRect<int>(450, 27, 105, 113), CRect<int>(117, 448, 105, 105), static_cast<u_char>(fade_b1));
    set2DSprite(GetVif1Packet(), TexManager.GetTexture("0519p", -1), CRect<int>(450, 27, 105, 113), CRect<int>(222, 448, 105, 105), static_cast<u_char>(fade_b2));
    set2DSprite(GetVif1Packet(), TexManager.GetTexture("0519p", -1), CRect<int>(450, 27, 105, 113), CRect<int>(327, 448, 105, 105), static_cast<u_char>(fade_b3));
    set2DSprite(GetVif1Packet(), TexManager.GetTexture("0519p", -1), CRect<int>(350, 139, 208, 62), CRect<int>(432, 448, 208, 58), static_cast<u_char>(fade_b1));
    set2DSprite(GetVif1Packet(), TexManager.GetTexture("0519p", -1), CRect<int>(350, 139, 208, 62), CRect<int>(432, 506, 208, 58), static_cast<u_char>(fade_b2));
    set2DSprite(GetVif1Packet(), TexManager.GetTexture("0519p", -1), CRect<int>(350, 139, 208, 62), CRect<int>(432, 564, 208, 58), static_cast<u_char>(fade_b3));
    set2DSprite(GetVif1Packet(), TexManager.GetTexture("0519p", -1), CRect<int>(100, 188, 117, 60), CRect<int>(0, 325, 117, 56), static_cast<u_char>(fade_a1));
    set2DSprite(GetVif1Packet(), TexManager.GetTexture("0519p", -1), CRect<int>(100, 188, 117, 60), CRect<int>(0, 381, 117, 56), static_cast<u_char>(fade_a2));

    if (static_cast<OpDSprite>(CScript__2.sprite) == OpDSprite::Caption1) {
        set2DSprite(GetVif1Packet(), TexManager.GetTexture("0519p", -1), CRect<int>(6, 245, 178, 191), CRect<int>(388, 0, 178, 178), static_cast<u_char>(fade_a1));
        set2DSprite(GetVif1Packet(), TexManager.GetTexture("0519p", -1), CRect<int>(184, 245, 178, 191), CRect<int>(388, 0, 178, 178), static_cast<u_char>(fade_a1));
        set2DSprite(GetVif1Packet(), TexManager.GetTexture("0519p", -1), CRect<int>(362, 245, 178, 191), CRect<int>(388, 0, 178, 178), static_cast<u_char>(fade_a1));
        set2DSprite(GetVif1Packet(), TexManager.GetTexture("0519p", -1), CRect<int>(6, 245, 178, 191), CRect<int>(210, 0, 178, 178), static_cast<u_char>(fade_a2));
        set2DSprite(GetVif1Packet(), TexManager.GetTexture("0519p", -1), CRect<int>(184, 245, 178, 191), CRect<int>(210, 0, 178, 178), static_cast<u_char>(fade_a2));
        set2DSprite(GetVif1Packet(), TexManager.GetTexture("0519p", -1), CRect<int>(362, 245, 178, 191), CRect<int>(210, 0, 178, 178), static_cast<u_char>(fade_a2));
    } else {
        static float x1 = 6.0f;
        static float y1 = 245.0f;
        static float rot1 = -1.57f;
        static float x2 = 184.0f;
        static float y2 = 245.0f;
        static float rot2 = 1.57f;
        static float x3 = 362.0f;
        static float y3 = 245.0f;
        static float rot3 = 1.57f;

        DrawRisingStar(x1, y1, rot1, -100.0f, 6.0f);
        DrawRisingStar(x2, y2, rot2, 500.0f, 184.0f);
        DrawRisingStar(x3, y3, rot3, 900.0f, 362.0f);
    }

    static float x1 = 426.0f;
    static float y1 = -221.0f;
    static float x2 = 726.0f;
    static float y2 = -328.0f;

    DrawFallingStar(x1, y1, 40.0f, 426.0f, -221.0f);
    DrawFallingStar(x2, y2, 240.0f, 726.0f, -328.0f);

    if (static_cast<OpDSprite>(CScript__2.sprite) == OpDSprite::Caption2) {
        set2DSprite(GetVif1Packet(), TexManager.GetTexture("0519p", -1), CRect<int>(40, 193, 463, 221), CRect<int>(177, 206, 463, 242), static_cast<u_char>(fade_a1));
    }
}

// Each ripple plays its sound the first time its scale grows past zero; only the first four draw.
void HamonProcess() {
    sceVu0FVECTOR ambient;

    sceVu0CopyVector(ambient, ambientlight);

    constexpr std::array<float, 5> volumes = {1.0f, 0.85f, 0.6f, 0.45f, 0.3f};

    for (int i = 0; i < 5; i++) {
        static std::array<int, 5> sound_played = {0, 0, 0, 0, 0};

        if (!Pause && HScale[i] < 3.2f) {
            HScale[i] += 0.005f;
        }

        if (HScale[i] > 0.0f) {
            if (!sound_played[i]) {
                OpPlayVolSE(MIDI_PORT_SE_DEFAULT, 40, 46, volumes[i]);
                sound_played[i] = 1;
            }

            if (i < 4) {
                Hamon[i]->SetScale(HScale[i], HScale[i], HScale[i]);
                Hamon[i]->SetPosition(0.0f, 0.01f + static_cast<float>(i) / 100.0f, 0.0f);
                ambient[3] = std::max(128.0f - 40.0f * HScale[i], 0.0f);

                MGSetAmbient(ambient);

                sceGsAlpha alpha = mgAlpha;

                alpha.bits.a = 0;
                alpha.bits.b = 2;
                alpha.bits.c = 0;
                alpha.bits.d = 1;
                setAlphaFlag(Vif1Packet, &alpha);
                MGDraw(Hamon[i]);
                setAlphaFlag(Vif1Packet, &mgAlpha);
            }
        }
    }

    MGSetAmbient(ambientlight);
}
