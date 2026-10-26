#include "common.h"

#include <libgraph.h>
#include <libpkt.h>
#include <libvu0.h>
#include <sifdev.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "battle_globals.hpp"
#include "battlemenu.hpp"
#include "boxvu0.hpp"
#include "btmisc.hpp"
#include "camera.hpp"
#include "character.hpp"
#include "clsmes.hpp"
#include "debugfont.hpp"
#include "dngstatusdata.hpp"
#include "edit.hpp"
#include "editground.hpp"
#include "editloop3.hpp"
#include "editmapscript.hpp"
#include "editpartsinfo.hpp"
#include "effectmacro.hpp"
#include "frame.hpp"
#include "framevu1.hpp"
#include "gamepad.hpp"
#include "mainselect.hpp"
#include "mapparts.hpp"
#include "mathutil.hpp"
#include "memcard.hpp"
#include "menu_misc.hpp"
#include "menu_save.hpp"
#include "mglib.hpp"
#include "npcharacter.hpp"
#include "objanime.hpp"
#include "rect.hpp"
#include "savedata.hpp"
#include "shop.hpp"
#include "snd.hpp"
#include "sound.hpp"
#include "texture.hpp"

/* Retail editetc.cpp: shared editor presentation, sound, menus and message helpers. */

static void DrawBound(CFrame *frame);
static void DrawLine(int *from, int *to, u_char r, u_char g, u_char b, u_char a);

/**
 * Stores one positional sound effect and every place it is heard from this frame.
 */
struct SOUND_SRC {
    int   se;         /**< Sound effect the slot plays; negative when the slot is free. */
    int   num;        /**< Number of places entered this frame. */
    float volume[16]; /**< Volume the sound arrives at from each place. */
    float pan[16];    /**< Pan the sound arrives at from each place. */
};

void EdSaveFrameImage(CTexture texture);

/* 28 bytes nothing reads other than a word at a time, so it is spelled as words rather than as a
   layout nothing supports. */
#include "dataset.hpp"
#include "editloop.hpp"
#include "editmenu.hpp"
#include "main.hpp"

void ClearSystemMes();
int  SystemMesCheck();
void SystemMesStep();
void SystemMesDraw();
void ItemGetMes(int item, int value, int duration, int input_key);

/* The map editor's own debug layer: a text overlay anything may append a line to, a free camera
   and a free character driven straight off the pad, and a wireframe box drawn through the GS by
   hand because nothing else in the game draws lines.
   Two switches gate all of it. Debug starts on and EdDDebug turns it off for good - the argument
   it is handed is not read - so a build reaches the overlay only until the first call. DebugFont
   is null until somebody hands one over, and every entry point checks it, because the overlay
   writes into that object's buffer rather than one of its own. */
int EdDebugEventEnable = 1;
int EdDebugCameraFlag;
int EdDebugParamDrawOff;
int EdDebugCharaDrawOff;
int EdDebugMoveFlag;
int EdDebugRunEventNo;

static int         Debug = 1;
static CDebugFont *DebugFont;

void EdDDebug(int on);

void EdDSetFont(CDebugFont *font);

/* The frame counter is printed first, so it stands at the top of whatever the frame appended
   after it. With the overlay switched off the buffer is still emptied every frame, which is what
   keeps a switched-off build from overrunning it. */
void EdDDrawFont();

/* The overflow guard for the frames nothing draws the overlay on: the buffer is 512 bytes and a
   line is short, so emptying it at 500 leaves room for whatever is already on its way in. */
void EdDCheck();

void EdOutPutFile();

/* The append is length-counted on the way out, and the length it adds goes to whatever DebugFont
   points at now rather than to the object it was handed. */
static int AddStr(CDebugFont *font, char *str) {
    int len;

    len = strlen(str);
    strcpy(&font->text[font->length], str);
    DebugFont->length += len;
    return len;
}

void EdDPrintChara(CMainChara *chara);

void EdDPrintCamera(CCamera *camera);

void EdDPrintVector(char *name, float *vector);

void EdDPrint(char *text);

/* The free camera: the sticks move the eye in the plane the eye already looks along, so pushing
   forward closes on whatever is being looked at whichever way the camera faces, and the shoulder
   buttons swing it around the reference point at a rate proportional to how far away it is.
   R1 drags the reference point along with the eye, which is what turns a swing into a pan, and
   the D-pad works the field of view. The box is drawn where the reference point is, so the point
   being orbited is visible. */
void EdDMoveCamera(float *position, float *reference);

/* The same camera driven from the other end: the sticks move the reference point and R1 drags the
   eye after it, which is what lets the point being orbited be placed before it is orbited. */
void EdDMoveCameraRef(float *position, float *reference);

/* The free character: the same plane-relative stick mapping as the camera, plus a facing the
   shoulder buttons turn and keep inside one revolution, and an ambient offset the face buttons
   work. Triangle re-aims the camera at the body rather than at the feet, which is why the height
   of the body is what the reference point is lifted by.
   The box is ten across and twenty tall - a stand-in for the body rather than the body's own
   size - and it is drawn with the character's own rotation so the facing can be seen. */
void EdDMoveChara(CCharacter *character, CCamera *camera);

void EdDebugMenu();

void DM_Main();

void DM_Sound();

/**
 * Raises the deepest floor reached in one dungeon to the given floor if it lies deeper.
 */
static inline void RaiseFloorReached(CDngStatusData *status, int dungeon, int floor) {
    if (floor > status->floor_reached[dungeon]) {
        status->floor_reached[dungeon] = floor;
    }
}

void DM_Flag();

/* The box drawn as twelve edges of the frame's own corner list, taken to the screen in one go and
   drawn only if every corner survived: a box with one corner behind the eye would otherwise be
   drawn through a projected point that means nothing. */
static void DrawBound(CFrame *frame) {
    int           screen[8][4];
    sceVu0FVECTOR world[8];
    sceVu0FMATRIX matrix;
    int           i;
    int           visible;

    if (frame == 0) {
        return;
    }

    frame->GetLWMatrix(matrix);
    visible = true;
    ApplyMatrixN(world, matrix, frame->corner, 8);

    for (i = 0; i < 8; i++) {
        visible &= MGRotTransPers(screen[i], world[i], 0);
    }

    if (visible == 0) {
        return;
    }

    DrawLine(screen[0], screen[1], 128, 64, 64, 80);
    DrawLine(screen[1], screen[5], 128, 64, 64, 80);
    DrawLine(screen[5], screen[4], 128, 64, 64, 80);
    DrawLine(screen[4], screen[0], 128, 64, 64, 80);
    DrawLine(screen[2], screen[3], 128, 64, 64, 80);
    DrawLine(screen[3], screen[7], 128, 64, 64, 80);
    DrawLine(screen[7], screen[6], 128, 64, 64, 80);
    DrawLine(screen[6], screen[2], 128, 64, 64, 80);
    DrawLine(screen[0], screen[2], 128, 64, 64, 80);
    DrawLine(screen[4], screen[6], 128, 64, 64, 80);
    DrawLine(screen[5], screen[7], 128, 64, 64, 80);
    DrawLine(screen[1], screen[3], 128, 64, 64, 80);
}

/* One line straight into the frame's packet as an A+D list, which is the only way this game draws
   one at all. The depth test is left enabled with a comparison that never rejects, so the line
   comes out over whatever is already in front of it, and the two registers that costs are put back
   at the end of the same list, so whatever draws next finds the state this found. */
static void DrawLine(int *from, int *to, u_char r, u_char g, u_char b, u_char a) {
    sceVif1Packet *packet;
    sceGsTest      test;
    sceGsZbuf      zbuf;
    float          q;

    q = 1.0f;
    packet = GetVif1Packet();

    test = mgPixelTest;
    test.bits.ate = 0;
    test.bits.aref = 0;
    test.bits.atst = SCE_GS_ALWAYS;
    test.bits.zte = 1;
    test.bits.ztst = SCE_GS_ALWAYS;

    zbuf = mgZBuffer;

    sceVif1PkCnt(packet, 0);
    sceVif1PkOpenDirectCode(packet, 0);
    sceVif1PkOpenGifTag(packet, *(u_long128 *) &GiftagAD);

    sceVif1PkAddGsAD(packet, SCE_GS_TEST_1, *(u_long *) &test);
    sceVif1PkAddGsAD(packet, SCE_GS_ZBUF_1, *(u_long *) &zbuf);
    sceVif1PkAddGsAD(packet, SCE_GS_PRIM, SCE_GS_SET_PRIM(SCE_GS_PRIM_LINE, 0, 0, 0, 1, 0, 1, 0, 0));
    sceVif1PkAddGsAD(packet, SCE_GS_RGBAQ, SCE_GS_SET_RGBAQ(r, g, b, a, *(u_int *) &q));
    sceVif1PkAddGsAD(packet, SCE_GS_XYZF2, SCE_GS_SET_XYZF2(from[0], from[1], from[2], 0));
    sceVif1PkAddGsAD(packet, SCE_GS_XYZF2, SCE_GS_SET_XYZF2(to[0], to[1], to[2], 0));
    sceVif1PkAddGsAD(packet, SCE_GS_TEST_1, *(u_long *) &mgPixelTest);
    sceVif1PkAddGsAD(packet, SCE_GS_ZBUF_1, *(u_long *) &mgZBuffer);

    sceVif1PkCloseGifTag(packet);
    sceVif1PkCloseDirectCode(packet);
}

void EdSetBgmVol(float time) {
    float level = 1.0f;

    // The town music plays at night and is silent through the day, fading out
    // over the hour before four.
    if (EdCheckTime(time, 4.0f, 11.0f) != 0) {
        level = 0.0f;
    }

    if (!(time < 3.0f) && time < 4.0f) {
        level = 4.0f - time;
    }

    int volume = (int) (level * (float) SndGetDefaultBgmVol());

    if (volume < 2) {
        level = 0.0f;
    }

    if (SndBgmCheck() == 1) {
        if (volume > 0) {
            SndSetBgmVol(volume);
        } else {
            SndBgmStop();
        }
    } else if (level > 0.0) {
        SndBgmPlay(0);
        SndSetBgmVol(volume);
    }
}

void EdAmbientPlay(float time) {
    // The ambient sets run one behind the four times of day, and roll over.
    int ambient_no = EdGetTime(time);

    if (++ambient_no >= 4) {
        ambient_no = 0;
    }

    SndAmbientPlay(ambient_no);
}

void EdSetAmbientVol(float volume) {
    SndAmbientSetVolf(volume);
}

/* The sound the map editor puts in the world: every effect a map part carries that names a sound
   is measured against the camera each frame, and what survives is entered into one of four
   sources. A source is one sound effect and the set of places it is coming from at once, so a
   fountain heard from two sides occupies one slot and arrives as a single volume and a single
   pan. */
static SOUND_SRC sound_src[4];
static int       now_play_se[4];
static int       now_play_se_flag[4];

static void init_sound_src() {
    int i;

    for (i = 0; i < 4; i++) {
        sound_src[i].se = -1;
        sound_src[i].num = 0;
    }
}

/* The slot a sound effect already occupies, or a free one, or nothing - which is what caps the
   world at four sounds at once. */
static SOUND_SRC *get_sound_src(int se) {
    int i;

    for (i = 0; i < 4; i++) {
        if (se == sound_src[i].se) {
            return &sound_src[i];
        }
    }

    for (i = 0; i < 4; i++) {
        if (sound_src[i].se < 0) {
            return &sound_src[i];
        }
    }

    return 0;
}

/* Whether the effect is already sounding, answered by claiming a channel for it when it is not:
   one for a sound that was playing, zero for one the caller now has to start, and -1 when every
   channel is spoken for. The flag beside the channel is what keeps this frame's sounds alive past
   the sweep at the end of the frame. */
static int check_se_play(int se) {
    int i;

    if (se < 0) {
        return 0;
    }

    for (i = 0; i < 4; i++) {
        if (se == now_play_se[i]) {
            now_play_se_flag[i] = 1;
            return 1;
        }
    }

    for (i = 0; i < 4; i++) {
        if (now_play_se[i] < 0) {
            now_play_se[i] = se;
            now_play_se_flag[i] = 1;
            return 0;
        }
    }

    return -1;
}

void EdInitSoundSrc() {
    int i;

    for (i = 0; i < 4; i++) {
        now_play_se[i] = -1;
        now_play_se_flag[i] = 0;
    }

    init_sound_src();
}

/* The frame's sources turned into commands. A sound gets one volume and one pan out of however
   many places it was entered from: the volumes add, and the pan is their average weighted by the
   same volumes, so the nearer place decides where the sound sits. Whatever was playing and was not
   entered this frame is stopped at the end, which is what a source leaving the camera behind comes
   to. */
void EdPlaySoundSrc() {
    int   i;
    int   j;
    int   se;
    int   playing;
    float volume;
    float pan;

    for (i = 0; i < 4; i++) {
        now_play_se_flag[i] = 0;
    }

    for (i = 0; i < 4; i++) {
        se = sound_src[i].se;

        if (se < 0) {
            continue;
        }

        playing = check_se_play(se);

        if (playing < 0) {
            continue;
        }

        if (playing == 0) {
            SndSePlay(se, 0, 0);
        }

        volume = 0.0f;

        for (j = 0; j < sound_src[i].num; j++) {
            volume += sound_src[i].volume[j];
        }

        pan = 0.0f;

        for (j = 0; j < sound_src[i].num; j++) {
            pan += sound_src[i].pan[j] * sound_src[i].volume[j] / volume;
        }

        if (volume > 1.0f) {
            volume = 1.0f;
        }

        SndSetSeVolf(se, volume, 0);

        if (pan > 1.0f) {
            pan = 1.0f;
        }

        if (pan < -1.0f) {
            pan = -1.0f;
        }

        SndSetSePanf(se, pan, 0);
    }

    for (i = 0; i < 4; i++) {
        if (now_play_se_flag[i]) {
            continue;
        }

        if (now_play_se[i] < 0) {
            continue;
        }

        SndSeStop(now_play_se[i], 0);
        now_play_se[i] = -1;
    }
}

void EdStopSoundSrc() {
    int i;

    for (i = 0; i < 4; i++) {
        if (now_play_se[i] < 0) {
            continue;
        }

        SndSeStop(now_play_se[i], 0);
        now_play_se[i] = -1;
    }

    SndSeStop(SE_AMBIENT_TOWN_PATTER, 0);
    SndSeStop(SE_AMBIENT_TOWN_NOISE, 0);
}

/* How far a point is from a segment, and where on the segment it is nearest to. The projection is
   taken first and answers both when it falls inside the segment; outside it, the nearer end is the
   answer and the distance is the distance to that end. */
static float GetDistLine(float *point, float *from, float *to, float *near_point) {
    sceVu0FVECTOR offset;
    sceVu0FVECTOR to_offset;
    sceVu0FVECTOR projection;
    sceVu0FVECTOR line;
    float         length;
    float         from_dist;
    float         to_dist;
    float         fraction;

    sceVu0SubVector(offset, from, point);
    sceVu0SubVector(to_offset, to, point);
    sceVu0SubVector(line, to_offset, offset);
    length = DistVector(line);
    length = length * length;
    fraction = -sceVu0InnerProduct(offset, line) / length;

    if (fraction < 0.0f || fraction > 1.0f) {
        from_dist = DistVector(point, from);
        to_dist = DistVector(point, to);

        if (from_dist < to_dist) {
            sceVu0CopyVector(near_point, from);
            return from_dist;
        }

        sceVu0CopyVector(near_point, to);
        return to_dist;
    }

    sceVu0ScaleVector(projection, line, fraction);
    sceVu0AddVector(projection, offset, projection);
    sceVu0AddVector(near_point, projection, point);
    return DistVector(projection);
}

/* Every effect of every part measured against the camera, and the sources rebuilt from scratch:
   nothing survives a frame, so a part that stopped being audible simply is not entered again. The
   part's own frame is moved first because the effects hang off it, and a door is the one source
   the part itself carries rather than one of its effects. */
void EdSetSoundSrcVol(float time, CMapParts **parts, int count, float *camera_pos, float *camera_dir) {
    int               i;
    int               j;
    CFrame           *frame;
    EDIT_EFFECT_INFO *effect;
    int               se;
    SOUND_SRC        *source;
    SOUND_SRC        *door_source;
    float             distance;
    float             far_dist;
    float             near_dist;
    sceVu0FVECTOR     position;
    sceVu0FVECTOR     rotation;
    sceVu0FVECTOR     from;
    sceVu0FVECTOR     to;
    sceVu0FVECTOR     line;
    float             volume;
    float             pan;
    float             door_volume;
    float             door_pan;

    SndSetCamera(camera_pos, camera_dir);
    init_sound_src();

    for (i = 0; i < count; i++) {
        if (parts[i] == 0) {
            continue;
        }

        frame = parts[i]->frame[0];

        if (frame == 0) {
            continue;
        }

        parts[i]->GetPosition(position);
        frame->SetPosition(position);
        parts[i]->GetRotation(rotation);
        frame->SetRotation(rotation[0], rotation[1], rotation[2]);

        for (j = 0; j < 24; j++) {
            effect = parts[i]->effect[j];

            if (parts[i]->effect_on[j] == 0) {
                continue;
            }

            if (effect == 0) {
                continue;
            }

            if (effect->kind != EDIT_EFFECT_SOUND && effect->kind != EDIT_EFFECT_FIRE_LARGE && effect->kind != EDIT_EFFECT_FIRE_SMALL) {
                continue;
            }

            if (effect->frame == 0) {
                continue;
            }

            if (CheckEditEffect(effect, time) == 0) {
                continue;
            }

            effect->frame->GetWorldPosition(from, effect->offset);

            if (effect->colour[3] == 1.0f) {
                effect->frame->GetWorldPosition(to, effect->colour);
                sceVu0CopyVector(line, from);
                distance = GetDistLine(camera_pos, line, to, from);
            } else {
                distance = DistVector(camera_pos, from);
            }

            far_dist = effect->far_distance;
            near_dist = effect->near_distance;
            se = (int) effect->sound_no;

            /* A torch and a fire are one sound at one range whatever the description says. */
            if (effect->kind == EDIT_EFFECT_FIRE_LARGE || effect->kind == EDIT_EFFECT_FIRE_SMALL) {
                se = 54;
                near_dist = 20.0f;
                far_dist = 150.0f;
            }

            if (distance > far_dist) {
                continue;
            }

            source = get_sound_src(se);

            if (source == 0) {
                continue;
            }

            SndGetVolPan(&volume, &pan, from, near_dist, far_dist);
            source->se = se;

            if (source->num < 16) {
                source->volume[source->num] = volume;
                source->pan[source->num] = pan;
                source->num++;
            }
        }

        if (parts[i]->subtype != MAP_PARTS_SUBTYPE_RIVER) {
            continue;
        }

        if (DistVector(camera_pos, position) >= 300.0f) {
            continue;
        }

        door_source = get_sound_src(52);

        if (door_source == 0) {
            continue;
        }

        SndGetVolPan(&door_volume, &door_pan, position, 100.0f, 300.0f);
        door_source->se = 52;

        if (door_source->num < 16) {
            door_source->volume[door_source->num] = door_volume;
            door_source->pan[door_source->num] = door_pan;
            door_source->num++;
        }
    }

    EdPlaySoundSrc();
}

/**
 * Plays the sound one kind of door makes when it opens.
 *
 * @mangled EdDoorOpenSe__FiPf
 * @address 0x172100
 * @size 0x58
 */
void EdDoorOpenSe(int door_no, float *position) {
    static int se_open[8] = {0x6C, 0x7E, 0x6E, 0x80, 0x74, 0x82, 0x70, 0x72};

    if (door_no < 0 || door_no >= 8) {
        return;
    }

    SndSePlay(se_open[door_no], -1, 0);
}

/**
 * Plays the sound one kind of door makes when it closes.
 *
 * @mangled EdDoorCloseSe__FiPf
 * @address 0x172160
 * @size 0x58
 */
void EdDoorCloseSe(int door_no, float *position) {
    static int se_close[8] = {0x6D, 0x7F, 0x6F, 0x81, 0x75, 0x83, 0x71, 0x73};

    if (door_no < 0 || door_no >= 8) {
        return;
    }

    SndSePlay(se_close[door_no], -1, 0);
}

int EdGetDoorMotion(int door_no, int state) {
    static int motion[8][2] = {
        {3,  4 },
        {10, 10},
        {3,  4 },
        {10, 10},
        {3,  4 },
        {10, 10},
        {10, 10},
        {10, 10},
    };

    if (door_no < 0 || door_no >= 8) {
        return 0;
    }

    return motion[door_no][state != 0];
}

/* The map editor's depth of field: one description at a time, taken from the map's own data or
   replaced by a default set, and handed every frame to the effect the rest of the game draws. */
static DEPTH_OF_FIELD_INFO dof;

void EdSetDOFLevel(int level) {
    dof.level = level;

    if (level == 0) {
        dof.start = ConvertTime(0.0f);
        dof.end = ConvertTime(24.0f);
        dof.distance[0] = 400.0f;
        dof.distance[1] = 1000.0f;
        dof.level = 2;
        dof.alpha = 64;
        dof.blur = 0;
    }
}

void EdSetDOF(DEPTH_OF_FIELD_INFO *info) {
    dof = *info;
    EdSetDOFLevel(info->level);
}

/* The argument is a cap rather than the level to draw at, so a configuration that asks for less
   than the map does gets less and one that asks for more does not get more. */
void EdDrawDOF(int level) {
    int drawn_level;

    drawn_level = dof.level;

    if (drawn_level < 0) {
        return;
    }

    if (level < drawn_level) {
        drawn_level = level;
    }

    DepthOfField(dof.distance, drawn_level, dof.alpha, dof.blur);
}

/* The lightning three of the game's towns get, which is a model node shown for a few frames and a
   flash added to the parallel lights rather than anything drawn over the frame. The node is named
   in the ground's own model, so a map without it is simply a map without lightning. */
static int           thunder_count;
static int           start_thunder;
static int           next_thunder_cnt;
static sceVu0FMATRIX thd_light;
static sceVu0FMATRIX thd_color;

void EdInitThunderEffect() {
    thunder_count = 0;
    next_thunder_cnt = 60;
    start_thunder = 0;
}

void EdThunderEffect(int map, CEditGround *ground) {
    CFrame       *frame;
    sceVu0FMATRIX color;
    sceVu0FMATRIX light;
    int           i;
    int           j;
    int           light_no;
    int           channel;

    if (map == 40 || map == 50 || map == 24) {
        frame = ground->fixed_parts[1].frame[0];

        if (frame) {
            frame = frame->SearchFrame("inazuma");
        }

        if (frame == 0) {
            return;
        }

        /* The lights the map itself is lit by, kept from the first flash so that every later one
           decays back to them rather than to whatever the previous flash left. */
        if (start_thunder == 0) {
            MGGetPLight(thd_light, thd_color);
        }

        MGGetPLight(light, color);

        if (thunder_count > 0) {
            frame->attr.draw_on = 1;
        } else {
            thunder_count = 0;

            if (next_thunder_cnt == 0) {
                thunder_count = 4;

                for (light_no = 0; light_no < 2; light_no++) {
                    for (channel = 0; channel < 3; channel++) {
                        thd_color[light_no][channel] = 255.0f;
                    }
                }

                SndSePlay(rand() % 6 + SE_THUNDER_RUMBLE, -1, 0);

                if (start_thunder == 0) {
                    SndSePlay(SE_THUNDER_CLAP, -1, 0);
                    start_thunder = 1;
                }

                next_thunder_cnt = rand() % 200 + 10;
            }

            frame->attr.draw_on = 2;
        }

        thunder_count--;
        next_thunder_cnt--;

        for (i = 0; i < 2; i++) {
            for (j = 0; j < 3; j++) {
                thd_color[i][j] -= 6.0f;

                if (thd_color[i][j] < color[i][j]) {
                    thd_color[i][j] = color[i][j];
                }
            }
        }

        MGSetPLight(light, thd_color);
    }
}

/**
 * Reports whether a villager's model is set up and enabled for drawing.
 */
static inline int IsVisible(CNPCharacter *npc) {
    return (bool) (npc->initialized != 0 && npc->draw_enabled != 0);
}

/**
 * Reports whether a villager's model is set up and enabled for its shadow pass.
 */
static inline int IsShadowVisible(CNPCharacter *npc) {
    bool visible = false;

    if (npc->initialized != 0 && npc->draw_enabled != 0) {
        visible = true;
    }

    return visible;
}

/**
 * Draws one editor character with its shadow, cursor and event state.
 *
 * @mangled EdDrawCharacter__FP10CCharacteriiP12CNPCharacterPiiP13ED_EVENT_INFO
 * @address 0x1725F0
 * @size 0x4C8
 */
void EdDrawCharacter(CCharacter *player, int player_draw_mask, int npc_count, CNPCharacter *npcs, int *npc_draw_masks, int draw_shadows, ED_EVENT_INFO *event) {
    int i;

    if (EdDebugCharaDrawOff != 0) {
        player_draw_mask = 0;
    }

    if (event != NULL) {
        draw_shadows = 0;
        player_draw_mask = 3;

        if (event->player_draw == 0) {
            player_draw_mask &= ~1;
        }

        if (event->player_shadow_draw == 0) {
            player_draw_mask &= ~2;
        } else {
            draw_shadows |= 1;
        }

        if (npc_draw_masks != NULL) {
            for (i = 0; i < npc_count; i++) {
                if (event->npc_draw[i] == 0) {
                    npc_draw_masks[i] = 0;
                }

                if (event->npc_shadow_draw[i] == 0) {
                    npc_draw_masks[i] &= ~2;
                } else {
                    draw_shadows |= 1;
                }
            }
        }

        // Villagers the event wants in front are drawn before the shadows.
        for (i = 0; i < npc_count && npc_draw_masks != NULL; i++) {
            CNPCharacter *npc = &npcs[i];

            if (npc->CheckDraw() != 0 && event->npc_draw_before[i] != 0 && (npc_draw_masks[i] & 1) != 0) {
                if (IsVisible(&npcs[i])) {
                    npc_draw_masks[i] = 0;
                    TexManager.ReloadTexture(Vif1Packet, npcs[i].texture_block);
                    npc->TextureAnime(npcs[i].texture_block);
                    CCharacter *chara = npc;
                    chara->Draw();
                }
            }
        }
    }

    if (draw_shadows != 0) {
        sceVu0FMATRIX light_direction;
        sceVu0FMATRIX shadow_direction;
        sceVu0FMATRIX light_colour;
        sceVu0FMATRIX saved_colour;

        MGGetPLight(light_direction, light_colour);
        sceVu0CopyMatrix(shadow_direction, light_direction);
        sceVu0CopyMatrix(saved_colour, light_colour);
        EdLimitShadowLight(shadow_direction, 3.0f);
        MGSetPLight(shadow_direction, light_colour);
        TexManager.ReloadTexture(Vif1Packet, 22);
        MGBeginDrawShadow(*(sceGsTex0 *) &TexManager.GetTexture("shadow_buff", -1)->tex0);

        if (player_draw_mask & 2) {
            player->DrawShadow();
        }

        for (i = 0; i < npc_count && npc_draw_masks != NULL; i++) {
            if (npc_draw_masks[i] & 2) {
                if (IsShadowVisible(&npcs[i])) {
                    CCharacter *chara = &npcs[i];
                    chara->DrawShadow();
                }
            }
        }

        MGEndDrawShadow(0x34);
        MGSetPLight(light_direction, light_colour);
    }

    if (player_draw_mask & 1) {
        TexManager.ReloadTexture(Vif1Packet, 8);
        player->TextureAnime(8);
        player->Draw();
    }

    for (i = 0; i < npc_count && npc_draw_masks != NULL; i++) {
        if (npcs[i].CheckDraw() != 0 && (npc_draw_masks[i] & 1) != 0) {
            if (IsVisible(&npcs[i])) {
                TexManager.ReloadTexture(Vif1Packet, npcs[i].texture_block);
                npcs[i].TextureAnime(npcs[i].texture_block);
                CCharacter *chara = &npcs[i];
                chara->Draw();
            }
        }
    }
}

/* Giving a map part one more effect: the description table is scanned from the front for an entry
   nothing has claimed, the effect is built on the part's own frame, and the part is told which
   entry it now owns. The part's slot table is what the sound and the drawing walk later, so an
   effect that is built and not registered here is an effect nothing ever reaches. */
void EnterPartsEffect(CMapParts *parts, EPARTS_FUNC_DATA *func_data, EDIT_EFFECT_INFO *effects, int count) {
    int i;
    int j;

    /* The last entry is never handed out, so a full table refuses rather than overruns. */
    for (i = 1; i <= count; i++, effects++) {
        if (i == count) {
            return;
        }

        if (effects->kind <= EDIT_EFFECT_NONE) {
            break;
        }
    }

    if (InitEditEffect(parts->frame[0], func_data, effects) == 0) {
        return;
    }

    for (j = 0; j < 24; j++) {
        if (parts->effect_on[j] == 0) {
            parts->effect_on[j] = 1;
            parts->effect[j] = effects;
            return;
        }
    }
}

/* The map editor's gate in front of the menus the town runs: one mode word says which menu is up,
   and the same four calls — enter, finish entering, step, leave — reach whichever it is. The menus
   themselves live elsewhere and are only ever named from here.
   Entering takes four frames rather than one and nothing here says what the other three are for:
   the counter below is set by the first call and only the fourth reaches a menu. */
static int init_menu_cnt;
static int menu_mode;
static int use_item;
static int shop_no;
static int name_reg_chara;
static int use_item_list[32];

/* The item numbering the menus use is not the one the rest of the game passes around: weapons are
   renumbered on the way in and back again on the way out, so every item number crossing this gate
   goes through one of these two. */
static int ConvertItemNo(int item) {
    return TransWepNo(item);
}

static int InvertItemNo(int item) {
    return TransWepNoNewToOld(item);
}

void EdUseItemInit() {
    use_item_list[0] = ITEM_ATTACH_PEARL;
    use_item_list[1] = -1;
    use_item = -1;
}

void EdSetUseItem(int *items) {
    int i;

    for (i = 0; i < 32; i++) {
        use_item_list[i] = ConvertItemNo(items[i]);

        if (use_item_list[i] < 0) {
            break;
        }
    }
}

int EdGetUseItem() {
    return InvertItemNo(use_item);
}

void EdSetNameRegChara(int character) {
    name_reg_chara = character;
}

void EdSetShopNo(int shop) {
    shop_no = shop;
}

int EdInitMenu(int mode) {
    menu_mode = mode;
    init_menu_cnt = 0;

    if (mode <= 0) {
        return 0;
    }

    MGFlipWaitVSync(1);
    EdClearSystemMes();
    ClearSystemMes();
    SetInteriorOutFlag(0);
    return 1;
}

/* The frame the menu is actually built on: the picture behind it is kept first, because the menus
   draw over a still of the last game frame rather than over the game. */
int EdInitModeFinish(CCamera *camera, CTexture *texture) {
    init_menu_cnt++;

    if (init_menu_cnt > 3) {
        init_menu_cnt = 4;
    }

    if (init_menu_cnt < 4) {
        return 0;
    }

    if (texture) {
        EdSaveFrameImage(*texture);
    }

    GamePad.MenuModeOn(120);

    int texture_block[6] = {16, 17, 18, 25, 32, 33};

    switch (menu_mode) {
        case ED_MENU_EDIT:
            EditMenuInit(texture_block, EditMenuStatus.mode == EDIT_MENU_MODE_PLACE);
            return ED_MODE_GEORAMA_MENU;
        case ED_MENU_BATTLE:
            BattleMenuInit(texture_block, BATTLE_MENU_MODE_TOWN);
            break;
        case ED_MENU_SHOP:
        case ED_MENU_UNK_4:
            ShopNoInput(texture_block, shop_no, 1);
            break;
        case ED_MENU_USE_ITEM:
            InitEventItemSelect(16, use_item_list, &SaveData->GetDngStatus()->item_pack, 180, 210, 1, 0);
            break;
        case ED_MENU_CHANGE_ESA:
            InitEventItemSelect(16, use_item_list, &SaveData->GetDngStatus()->item_pack, 180, 210, 1, 1);
            break;
        case ED_MENU_NAME_REGISTRY:
            InitNameRegist(name_reg_chara, 16, 0);
            break;
        case ED_MENU_WORLD_MAP:
            InitMenuMove(MENU_MOVE_WORLD_MAP_DIRECT, 16, (u_long128 *) read_buffer);
            break;
        case ED_MENU_FISHING_EXCHANGE:
            InitFishingExchange(0, texture_block, 0);
            break;
        case ED_MENU_FISH_RANKING:
            InitFishRecordView(0, texture_block, 0);
            break;
    }

    return ED_MODE_MENU;
}

void EdExitMenu() {
    GamePad.AutoRepeatOff();
    GamePad.MenuModeOff();
    MGFlipWaitVSync(0);
}

int EdMenuMode() {
    int finished;

    finished = 0;

    switch (menu_mode) {
        case ED_MENU_BATTLE:
            finished = !BattleMenuCursor();
            BattleMenuDraw();

            if (finished) {
                EdClearItemOverFlag();
            }

            break;
        case ED_MENU_SHOP:
        case ED_MENU_UNK_4:
            finished = CommonShopLoop();
            break;
        case ED_MENU_USE_ITEM:
        case ED_MENU_CHANGE_ESA:
            finished = EventItemSelectLoop(&use_item);
            break;
        case ED_MENU_NAME_REGISTRY:
            finished = NameEnterKey();
            NameEnterDraw();
            break;
        case ED_MENU_WORLD_MAP:
            finished = !MenuMoveKey();
            DrawMenuMove();
            break;
        case ED_MENU_FISHING_EXCHANGE:
            finished = FishingExchangeLoop();
            break;
        case ED_MENU_FISH_RANKING:
            finished = FishRecordViewLoop();
            break;
    }

    return finished;
}

/* A count below zero means one, so a caller that has nothing to say about how many gets one. */
void EdGetItem(int item, int count, int attachment) {
    CDngStatusData *status;
    int             item_no;
    int             i;

    if (count < 0) {
        count = 1;
    }

    status = SaveData->GetDngStatus();
    item_no = ConvertItemNo(item);

    if (item_no < 0) {
        return;
    }

    for (i = 0; i < count; i++) {
        if (attachment > 0) {
            status->GetItem(item_no, attachment);
        } else {
            status->GetItem(item_no, 0);
        }
    }
}

/* Growing the pack clears the slots it opens rather than leaving what was in them, because a slot
   is read up to the count and an old number inside the new range would be an item again. */
int EdAddMaxItem(int amount) {
    ITEM_PACK *pack;
    int        new_count;
    int        i;

    pack = &SaveData->GetDngStatus()->item_pack;
    new_count = pack->num + amount;

    if (new_count < 0) {
        new_count = 0;
    }

    if (new_count > 100) {
        new_count = 100;
    }

    for (i = pack->num; i < new_count; i++) {
        pack->item[i] = -1;
    }

    pack->num = (char) new_count;
    return new_count;
}

int EdCheckItemOver() {
    return SaveData->GetDngStatus()->overflow_flag;
}

void EdClearItemOverFlag() {
    SaveData->GetDngStatus()->overflow_flag = false;
}

int EdCheckItem(int item) {
    CDngStatusData *status;
    int             item_no;

    status = SaveData->GetDngStatus();
    item_no = ConvertItemNo(item);

    if (item_no >= 0) {
        return status->SearchItemIndexNo(item_no);
    }

    return -1;
}

int EdCheckGetItem(int item) {
    CDngStatusData *status;
    int             item_no;

    status = SaveData->GetDngStatus();
    item_no = ConvertItemNo(item);

    if (item_no >= 0) {
        return status->CheckItemGet(item_no);
    }

    return 0;
}

void EdGetItemFile(int item_no, char *model_path, char *texture_path) {
    BtGetItemNamePath(model_path, texture_path, item_no);
}

/**
 * Draws the item the player is holding up.
 *
 * @mangled EdDrawItem__Fv
 * @address 0x173380
 * @size 0x58
 */
void EdDrawItem() {
    if (EdEventInfo.item_frame[0] != NULL) {
        TexManager.ReloadTexture(GetVif1Packet(), 0x28);
        MGDraw((CFrame *) EdEventInfo.item_frame[0]);
    }
}

/* The map editor's own system and help messages, in front of the ones the town runs: every call
   here reaches the town's message code first and then does the same thing again to a window of the
   editor's own, so both are up at once and the editor's is the one drawn last.
   A help message is a system message that has been handed over: it is built through
   EdSetSystemMes and its number and count are then moved into the pair below, which leaves the
   system slot free for the next real message. */
static int SystemMesNo = -1;
static int HelpMesNo = -1;
static int SystemMesCount;
static int SystemMesPosition;
static int SystemMesW;
static int SystemMesH;
static int SystemMesX;
static int SystemMesY;
static int SystemMesWait;
static int SystemMesInputKey;
static int HelpMesCount;
static int cnt1;
static int cnt2;

void EdSetSystemMes(int mes_no, int count, int position, int input_key, int *args, int value) {
    int i;

    SystemMesInputKey = input_key;
    SystemMesPosition = position;

    if (args) {
        for (i = 0; i < 4; i++) {
            EditSystemMes.mes_no[i] = -1;

            if (*args >= 0) {
                EditSystemMes.mes_no[i] = *args;
            }

            args++;
        }
    }

    if (value >= 0) {
        EditSystemMes.value = value;
    }

    EditSystemMes.MakeMesWin(mes_no);
    SystemMesNo = mes_no;
    SystemMesCount = count;
}

/* Message 120 is the only one that offers a choice, and its two positions are placed rather than
   authored: the first is indented by however much narrower than the window the laid-out text came
   out, and the second a fixed distance to the right of it. Every other message says there is no
   choice by putting both off screen. */
void EdSetHelpMes(int mes_no, int count, int position, int *args, int value) {
    int x;
    int y;
    int indent;

    if (mes_no == 120) {
        indent = EditSystemMes.text_columns - 30;
        indent = 16 - indent;

        if (indent < 0) {
            indent = 0;
        }

        y = SystemMesY;
        x = SystemMesX + ((indent * EditSystemMes.char_width) >> 1);
        EditSystemMes.line_pos[0].x = x;
        EditSystemMes.line_pos[0].y = y;
        x = SystemMesX + 220;
        EditSystemMes.line_pos[1].x = x;
        EditSystemMes.line_pos[1].y = y;
    } else {
        EditSystemMes.line_pos[0].x = -1;
        EditSystemMes.line_pos[0].y = -1;
        EditSystemMes.line_pos[1].x = -1;
        EditSystemMes.line_pos[1].y = -1;
    }

    EditSystemMes.auto_pos = SystemMesPosition;
    EditSystemMes.text_x = SystemMesX;
    EditSystemMes.text_y = SystemMesY;
    EditSystemMes.stay_width = SystemMesW;
    EditSystemMes.stay_height = SystemMesH;
    EditSystemMes.auto_pos = SystemMesPosition;
    EdSetSystemMes(mes_no, count, position, 0, args, value);
    EditSystemMes.auto_pos = SystemMesPosition;
    HelpMesNo = SystemMesNo;
    HelpMesCount = SystemMesCount;
    SystemMesNo = -1;
    SystemMesCount = 0;
}

void EdClearSystemMes() {
    ClearSystemMes();
    EditSystemMes.MakeMesWin(-1);
    SystemMesNo = -1;
    SystemMesCount = 0;
    SystemMesInputKey = 0;
    HelpMesNo = -1;
    HelpMesCount = 0;
    SystemMesWait = 0;
    SystemMesW = 0;
    SystemMesH = 0;
    SystemMesX = 0;
    SystemMesY = 0;
}

int EdSystemMesCheck() {
    return SystemMesCheck();
}

/* A message counts down to nothing and is cleared at one rather than at zero, so the frame that
   would have shown it empty is the frame it goes away on. A message that asked for a button holds
   at two until the button is pressed. */
void EdSystemMesStep() {
    if (SystemMesWait > 0) {
        SystemMesWait--;
        return;
    }

    SystemMesStep();

    if (SystemMesCount == 1 || HelpMesCount == 1) {
        EdClearSystemMes();
    }

    if (SystemMesNo > 0 || HelpMesCount >= 0) {
        EditSystemMes.Step();
    }

    if (SystemMesInputKey == 0 || SystemMesCount != 2 || GamePad.Down(PAD_CROSS)) {
        if (SystemMesCount > 0) {
            SystemMesCount--;
        }

        if (HelpMesCount > 0) {
            HelpMesCount--;
        }
    }
}

/* The window's own textures are put back every frame it is drawn, because whatever the game drew
   in between is free to have taken the video memory they were in. */
void EdSystemMesDraw() {
    if (SystemMesWait > 0) {
        return;
    }

    SystemMesDraw();

    if (HelpMesNo <= 0) {
        return;
    }

    TexManager.ReloadTexture(Vif1Packet, EditSystemMes.tex_block);
    setbilinear(0);
    EditSystemMes.DrawMesWin();
}

/* The width the window will come to is not known before the text is laid out, so the messages
   below place it by hand: this one centres a forty-character line on the screen and the rest give
   a left edge outright. */
void EdEditMainHelpMes(int building) {
    SystemMesW = 0;
    SystemMesH = 0;
    SystemMesX = 320 - ((EditSystemMes.char_width * 40) >> 1);
    SystemMesY = 380;
    EdSetHelpMes(100 + (building != 0), 2, -1, 0, -1);
}

void EdWalkToEditMes(int wait) {
    SystemMesW = 0;
    SystemMesH = 0;
    SystemMesX = 40;
    SystemMesY = 380;
    SystemMesWait = wait;
    EdSetHelpMes(110, 120, -1, 0, -1);
}

/* The build and move prompts take turns: each counts down the other's delay before showing itself
   and then arms the other, so the two alternate rather than both being up. */
void EdEditBuildHelpMes(int part_no) {
    if (part_no < 0) {
        return;
    }

    if (cnt1 > 0) {
        cnt1--;
        return;
    }

    cnt1 = 0;
    cnt2 = 1;

    int args[4] = {-1, -1, -1, -1};

    args[0] = GetAtraMsgNo(MapNo, part_no);
    SystemMesW = 600;
    SystemMesH = 50;
    SystemMesX = 30;
    SystemMesY = 380;
    EdSetHelpMes(120, 2, -1, args, -1);
}

void EdEditMoveHelpMes() {
    if (cnt2 > 0) {
        cnt2--;
        return;
    }

    cnt2 = 0;
    cnt1 = 1;
    SystemMesW = 0;
    SystemMesH = 0;
    SystemMesX = 230;
    SystemMesY = 412;
    EdSetHelpMes(130, 2, -1, 0, -1);
}

/* Two messages a hundred apart for the same two states, since the argument names the fish and the
   message number says whether there is one. */
void EdFishingWalkHelpMes(int bait) {
    int mes_no;

    SystemMesW = 0;
    SystemMesH = 0;

    int args[4] = {-1, -1, -1, -1};

    args[0] = bait + 100;
    mes_no = 200;

    if (bait >= 0) {
        mes_no++;
    }

    EdSetHelpMes(mes_no, 2, 9, args, -1);
}

void EdFishingAngleHelpMEs(int bait) {
}

void EdFishingLostEsaMes() {
    SystemMesW = 0;
    SystemMesH = 0;
    EdSetHelpMes(210, 180, 8, 0, -1);
}

/* The second argument is dropped rather than forwarded, so a caller that passes one is passing it
   to nothing. */
void EdItemGetMes(int item, int kind, int attachment, int duration) {
    ItemGetMes(item, attachment, duration, 1);
}

/* The treasure box the map editor opens in front of the player: one box at a time, placed where the
   caller says, with its lid turned by a count that runs down from 40 and the ambient light lifted
   over the last twenty of them. The lid is a node of the box's own model called `top`, so the
   opening is a rotation of one frame rather than anything the model file carries. */
static int           ibox_open_flag;
static int           ibox_open_close_flag;
static int           ibox_open_cnt;
static CFrameVu1    *ibox_frame;
static CFrameVu1    *ibox_base_frame;
static sceVu0FVECTOR ibox_pos;
static sceVu0FVECTOR ibox_rot;

void EdInitOpenItemBox(CFrameVu1 *frame, CFrameVu1 *base) {
    ibox_open_close_flag = 0;
    ibox_open_flag = 0;
    ibox_open_cnt = 0;
    ibox_frame = frame;
    ibox_base_frame = base;
}

void EdSetOpenItemBox(float *position, float *rotation) {
    if (ibox_frame) {
        ibox_open_flag = 1;
        ibox_open_close_flag = 0;
        ibox_open_cnt = 40;
        sceVu0CopyVector(ibox_pos, position);
        sceVu0CopyVector(ibox_rot, rotation);
    }
}

void EdStepOpenItemBox() {
    CFrame *lid;
    float   open_angle;
    float   close_angle;

    if (ibox_open_flag == 0 && ibox_open_close_flag == 0) {
        return;
    }

    ibox_open_cnt--;

    if (ibox_open_cnt < 0) {
        ibox_open_flag = 0;
        ibox_open_close_flag = 0;
        return;
    }

    if (ibox_frame == 0) {
        return;
    }

    ibox_frame->SetPosition(ibox_pos);
    ibox_frame->SetRotation(ibox_rot[0], ibox_rot[1], ibox_rot[2]);
    lid = ibox_frame->SearchFrame("top");
    lid->SetRotType(2);

    if (ibox_open_flag) {
        /* The lid swings over the first half of the count and then stands open through the second,
           which is the half the ambient is faded over. */
        if (ibox_open_cnt <= 20) {
            return;
        }

        open_angle = -(HALF_PI - HALF_PI * (float) (ibox_open_cnt - 20) / 20.0f);
        lid->SetRotation(open_angle, 0.0f, 0.0f);
    } else {
        if (ibox_open_cnt >= 6) {
            close_angle = HALF_PI - HALF_PI * (float) (ibox_open_cnt - 5) / 5.0f;
        } else {
            close_angle = HALF_PI * (float) ibox_open_cnt / 5.0f;
        }

        lid->SetRotation(0.2f * -close_angle, 0.0f, 0.0f);
    }
}

void EdDrawOpenItemBox() {
    sceVu0FVECTOR ambient;
    sceVu0FVECTOR saved_ambient;

    if (ibox_open_flag == 0 && ibox_open_close_flag == 0) {
        return;
    }

    if (ibox_frame) {
        ibox_frame->SetPosition(ibox_pos);
        ibox_frame->SetRotation(ibox_rot[0], ibox_rot[1], ibox_rot[2]);
    }

    MGGetAmbient(ambient);
    sceVu0CopyVector(saved_ambient, ambient);

    if (ibox_open_cnt < 20) {
        ambient[3] = 128.0f * (float) ibox_open_cnt / 20.0f;
    }

    if (ibox_open_flag) {
        MGSetAmbient(ambient);
    }

    MGDraw(ibox_frame);
    MGSetAmbient(saved_ambient);
}

int      frame_image_flag;
CTexture frame_image_tex;

/**
 * Copies the drawn frame into a texture, for the menu to show behind itself.
 *
 * @mangled EdSaveFrameImage__F8CTexture
 * @address 0x173E00
 * @size 0x70
 */
void EdSaveFrameImage(CTexture texture) {
    frame_image_flag = 1;
    frame_image_tex = texture;
}

void EdSaveFrameImageTask();

void EdSaveFrameImageInit() {
    frame_image_tex.Initialize();
    frame_image_flag = 0;
}

int EdMenuLoop(ClsMes *message) {
    if (message == NULL) {
        return 1;
    }

    message->Step();

    if (GamePad.Down(PAD_CIRCLE | PAD_CROSS) != 0) {
        // A confirm on the last page closes the window and leaves it ready
        // for the next one.
        if (message->State() == CLSMES_SHOWN) {
            message->text_rate = message->text_rate_set;
            message->mes_made = -1;
            message->fade_in = false;
            message->text_columns = MES_WIN_COLUMNS;
            message->text_rows = 0xA;
            message->text_len = 0;
            message->text_width = 0;
            message->text_height = 0;
            message->fade = 0.0f;
            message->fade_in = true;
            message->text_rate = message->text_rate_set;
            message->waiting = false;
            message->text_at = 0.0f;
            message->text_no = 0;
            message->text_from = 0;
            message->page_from = 0;
            message->InitMesWinTbl();
            message->clut_now = message->clut_default;
            message->wait = 0;
            message->blink = 0;
            message->auto_page_wait = 0;
            message->mes_made = -1;
            message->edge_alpha = 0x80;

            for (int i = 0; i < 10; i++) {
                message->mes_no[i] = -1;
            }

            for (int i = 0; i < 8; i++) {
                message->values[i] = 0;
            }

            message->value = 0;
            message->value_signed = false;
            message->value_show = true;
            message->value_narrow = false;
            message->space_width = -1;
            message->space_area = -1;
            message->cursor_row = -1;
            message->cursor_y = 0;
            message->cursor_lit = false;

            for (int i = 0; i < 10; i++) {
                message->line_pos[i].x = -1;
                message->line_pos[i].y = -1;
            }

            return 1;
        }

        if (message->State() == CLSMES_PAGE_WAIT) {
            message->GoNextPage();
        } else {
            message->text_rate = 0.0f;
        }
    }

    return 0;
}

float ConvertTime(float hour) {
    hour -= 10.0f;

    if (hour < 0.0f) {
        hour += 24.0f;
    }

    hour /= 2.0f;
    return hour;
}

float InvertTime(float time) {
    time *= 2.0f;
    time += 10.0f;

    if (time > 24.0f) {
        time -= 24.0f;
    }

    return time;
}
