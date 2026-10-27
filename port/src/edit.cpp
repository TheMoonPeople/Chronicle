#include "common.h"

#include <libgraph.h>
#include <libpkt.h>
#include <libvu0.h>
#include <sifdev.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <format>
#include <memory>
#include <string_view>

#include "battle_globals.hpp"
#include "battlemenu.hpp"
#include "boxvu0.hpp"
#include "btmisc.hpp"
#include "camera.hpp"
#include "character.hpp"
#include "clsmes.hpp"
#include "dataset.hpp"
#include "debugfont.hpp"
#include "dngstatusdata.hpp"
#include "draw2d_port.hpp"
#include "draw3d.hpp"
#include "edit.hpp"
#include "editground.hpp"
#include "editloop.hpp"
#include "editloop3.hpp"
#include "editmapscript.hpp"
#include "editmenu.hpp"
#include "editpartsinfo.hpp"
#include "effectmacro.hpp"
#include "frame.hpp"
#include "framevu1.hpp"
#include "gamepad.hpp"
#include "main.hpp"
#include "mainselect.hpp"
#include "mapparts.hpp"
#include "mathutil.hpp"
#include "memcard.hpp"
#include "menu_misc.hpp"
#include "menu_save.hpp"
#include "mglib.hpp"
#include "mglib_port.hpp"
#include "npcharacter.hpp"
#include "objanime.hpp"
#include "rect.hpp"
#include "savedata.hpp"
#include "shop.hpp"
#include "snd.hpp"
#include "sound.hpp"
#include "texture.hpp"

namespace {
void DrawBound(CFrame *frame);
void DrawLine(const int *from, const int *to, u_char r, u_char g, u_char b, u_char a);

/* The map editor's debug layer: a text overlay anything may append a line to, a free camera and a
   free character driven straight off the pad, and a wireframe box drawn at the point being moved.
   Two switches gate all of it. Debug starts on and EdDDebug turns it off for good - the argument
   it is handed is not read - so a build reaches the overlay only until the first call. DebugFont
   is null until somebody hands one over, and every entry point checks it, because the overlay
   writes into that object's buffer rather than one of its own. */
int         Debug = 1;
CDebugFont *DebugFont;
int         AddStr(CDebugFont *font, std::string_view text);
} // namespace

void EdDDebug(int /*on*/) {
    Debug = 0;
}

void EdDSetFont(CDebugFont *font) {
    DebugFont = font;
}

/* The frame counter is printed first, so it stands at the top of whatever the frame appended
   after it. With the overlay switched off the buffer is still emptied every frame, which is what
   keeps a switched-off build from overrunning it. */
void EdDDrawFont() {
    static int count = 0;

    if (DebugFont == nullptr) {
        return;
    }

    AddStr(DebugFont, std::format("{}\n", count++));

    if (Debug == 0) {
        DebugFont->length = 0;
        return;
    }

    TexManager.ReloadTexture(GetVif1Packet(), 31);
    DebugFont->Draw();
}

/* The overflow guard for the frames nothing draws the overlay on: the buffer is 512 bytes and a
   line is short, so emptying it at 500 leaves room for whatever is already on its way in. */
void EdDCheck() {
    if (DebugFont == nullptr) {
        return;
    }

    if (DebugFont->length > 500) {
        DebugFont->length = 0;
    }
}

void EdOutPutFile() {
    if (Debug == 0) {
        return;
    }

    if (DebugFont == nullptr) {
        return;
    }

    const int fd = sceOpen("host0:debug.txt", SCE_WRONLY | SCE_CREAT | SCE_TRUNC);

    if (fd < 0) {
        return;
    }

    sceWrite(fd, DebugFont->text, DebugFont->length);
    sceClose(fd);
}

/* The append is length-counted on the way out, and the length it adds goes to whatever DebugFont
   points at now rather than to the object it was handed. */
namespace {
int AddStr(CDebugFont *font, std::string_view text) {
    if (font == nullptr || DebugFont == nullptr || font->length < 0 || DebugFont->length < 0) {
        return 0;
    }

    const int available = std::min(static_cast<int>(sizeof(font->text)) - font->length,
                                   static_cast<int>(sizeof(DebugFont->text)) - DebugFont->length);
    const int len = std::min(static_cast<int>(text.size()), available);

    if (len <= 0) {
        return 0;
    }

    memmove(&font->text[font->length], text.data(), len);
    DebugFont->length += len;
    return len;
}
} // namespace

void EdDPrintChara(CMainChara *chara) {
    sceVu0FVECTOR vector;

    if (Debug == 0) {
        return;
    }

    if (DebugFont == nullptr) {
        return;
    }

    chara->GetPosition(vector);
    AddStr(DebugFont, std::format("chara\n pos = {:7.2f},{:7.2f},{:7.2f}\n", vector[0], vector[1], vector[2]));
    chara->GetRotation(vector);
    AddStr(DebugFont, std::format(" rot = {:7.2f},{:7.2f},{:7.2f}\n", vector[0], vector[1], vector[2]));
    const auto text = std::format(" ({} {} {})\n", chara->move_info.ground_poly.attr.ground_kind,
                                  chara->move_info.ground_poly.attr.foot_sound,
                                  chara->move_info.ground_poly.attr.area_kind);

    if (chara->move_info.landed) {
        AddStr(DebugFont, text);
    }
}

void EdDPrintCamera(CCamera *camera) {
    sceVu0FVECTOR vector;

    if (Debug == 0) {
        return;
    }

    if (DebugFont == nullptr) {
        return;
    }

    camera->GetPos(vector);
    AddStr(DebugFont, std::format("camera\n pos = {:7.2f},{:7.2f},{:7.2f}\n", vector[0], vector[1], vector[2]));
    camera->GetRef(vector);
    AddStr(DebugFont, std::format(" ref = {:7.2f},{:7.2f},{:7.2f}\n", vector[0], vector[1], vector[2]));
    AddStr(DebugFont, std::format(" projection = {:7.1f}\n", MGGetProjection()));
}

void EdDPrintVector(char *name, float *vector) {

    if (Debug == 0) {
        return;
    }

    if (DebugFont == nullptr) {
        return;
    }

    AddStr(DebugFont, std::format("{} {:7.2f},{:7.2f},{:7.2f}\n", name, vector[0], vector[1], vector[2]));
}

void EdDPrint(char *text) {
    if (Debug == 0) {
        return;
    }

    if (DebugFont == nullptr) {
        return;
    }

    AddStr(DebugFont, text);
}

/* The free camera: the sticks move the eye in the plane the eye already looks along, so pushing
   forward closes on whatever is being looked at whichever way the camera faces, and the shoulder
   buttons swing it around the reference point at a rate proportional to how far away it is.
   R1 drags the reference point along with the eye, which is what turns a swing into a pan, and
   the D-pad works the field of view. The box is drawn where the reference point is, so the point
   being orbited is visible. */
void EdDMoveCamera(float *position, float *reference) {
    sceVu0FVECTOR offset;
    sceVu0FVECTOR move;

    if (Debug == 0) {
        return;
    }

    sceVu0SubVector(offset, reference, position);
    const float distance = sqrtf(offset[0] * offset[0] + offset[2] * offset[2]);
    const float angle = atan2f(offset[0], offset[2]);

    float x = -GamePad.GetLXf();

    if (GamePad.On(PAD_R1)) {
        x = 0.02f * distance;
    }

    if (GamePad.On(PAD_L1)) {
        x = 0.02f * -distance;
    }

    const float y = -GamePad.GetRYf();
    const float z = -GamePad.GetLYf();

    move[0] = x * cosf(angle) + z * sinf(angle);
    move[1] = y;
    move[2] = z * cosf(angle) - x * sinf(angle);

    if (GamePad.On(PAD_CROSS)) {
        sceVu0ScaleVector(move, move, 3.0f);
    }

    sceVu0AddVector(position, position, move);

    if (GamePad.On(PAD_SQUARE) && !GamePad.On(PAD_L1 | PAD_R1)) {
        sceVu0AddVector(reference, reference, move);
    }

    float projection = MGGetProjection();

    if (GamePad.On(PAD_RIGHT)) {
        projection += 1.0f;
    }

    if (GamePad.On(PAD_LEFT)) {
        projection -= 1.0f;
    }

    projection = std::max(projection, 100.0f);
    projection = std::min(projection, 2000.0f);

    MGSetProjection(projection);

    {
        CFrame                         frame;
        constexpr std::array<float, 2> unit = {-1.0f, 1.0f};

        for (int i = 0; i < 8; i++) {
            frame.corner[i][0] = unit[(i & 1) != 0];
            frame.corner[i][1] = unit[(i & 2) != 0];
            frame.corner[i][2] = unit[(i & 4) != 0];
            frame.corner[i][3] = 1.0f;
        }

        frame.SetPosition(reference);
        DrawBound(&frame);
    }
}

/* The same camera driven from the other end: the sticks move the reference point and R1 drags the
   eye after it, which is what lets the point being orbited be placed before it is orbited. */
void EdDMoveCameraRef(float *position, float *reference) {
    sceVu0FVECTOR offset;
    sceVu0FVECTOR move;

    if (Debug == 0) {
        return;
    }

    sceVu0SubVector(offset, reference, position);
    const float distance = sqrtf(offset[0] * offset[0] + offset[2] * offset[2]);
    const float angle = atan2f(offset[0], offset[2]);

    float x = -GamePad.GetLXf();

    if (GamePad.On(PAD_R1)) {
        x = 0.02f * -distance;
    }

    if (GamePad.On(PAD_L1)) {
        x = 0.02f * distance;
    }

    const float y = GamePad.GetRYf();
    const float z = -GamePad.GetLYf();

    move[0] = x * cosf(angle) + z * sinf(angle);
    move[1] = y;
    move[2] = z * cosf(angle) - x * sinf(angle);

    sceVu0AddVector(reference, reference, move);

    if (GamePad.On(PAD_SQUARE) && !GamePad.On(PAD_L1 | PAD_R1)) {
        sceVu0AddVector(position, position, move);
    }

    {
        CFrame                         frame;
        constexpr std::array<float, 2> unit = {-1.0f, 1.0f};

        for (int i = 0; i < 8; i++) {
            frame.corner[i][0] = unit[(i & 1) != 0];
            frame.corner[i][1] = unit[(i & 2) != 0];
            frame.corner[i][2] = unit[(i & 4) != 0];
            frame.corner[i][3] = 1.0f;
        }

        frame.SetPosition(reference);
        DrawBound(&frame);
    }
}

/* The free character: the same plane-relative stick mapping as the camera, plus a facing the
   shoulder buttons turn and keep inside one revolution, and an ambient offset the face buttons
   work. Triangle re-aims the camera at the body rather than at the feet, which is why the height
   of the body is what the reference point is lifted by.
   The box is ten across and twenty tall - a stand-in for the body rather than the body's own
   size - and it is drawn with the character's own rotation so the facing can be seen. */
void EdDMoveChara(CCharacter *character, CCamera *camera) {
    sceVu0FVECTOR pos;
    sceVu0FVECTOR camera_pos;
    sceVu0FVECTOR camera_ref;
    sceVu0FVECTOR rot;
    sceVu0FVECTOR view;
    sceVu0FVECTOR move;

    if (character == nullptr) {
        return;
    }

    character->GetPosition(pos);
    character->GetRotation(rot);
    camera->GetPos(camera_pos);
    camera->GetRef(camera_ref);

    sceVu0SubVector(view, camera_ref, camera_pos);
    const float angle = atan2f(view[0], view[2]);
    float       speed = 0.5f;

    const float x = -GamePad.GetLXf();
    const float y = -GamePad.GetRYf();
    const float z = -GamePad.GetLYf();

    if (GamePad.On(PAD_R1)) {
        rot[1] += -0.06f;
    }

    if (GamePad.On(PAD_L1)) {
        rot[1] += 0.06f;
    }

    if (rot[1] > PI_SHORT) {
        rot[1] -= TWO_PI_SHORT;
    }

    if (rot[1] < -PI_SHORT) {
        rot[1] += TWO_PI_SHORT;
    }

    move[0] = x * cosf(angle) + z * sinf(angle);
    move[1] = y;
    move[2] = z * cosf(angle) - x * sinf(angle);

    if (GamePad.On(PAD_SQUARE)) {
        speed = 1.0f;
    }

    sceVu0ScaleVector(move, move, speed);
    sceVu0AddVector(pos, pos, move);
    character->SetPosition(pos);
    character->SetRotation(rot);

    if (GamePad.Down(PAD_TRIANGLE)) {
        sceVu0CopyVector(camera_ref, pos);
        camera_ref[1] += 0.7f * character->body_height;
        camera->SetRef(camera_ref);
    }

    float ambient = character->ambient_offset[0];

    if (GamePad.On(PAD_R2)) {
        ambient += 0.1f;
    }

    if (GamePad.On(PAD_L2)) {
        ambient -= 0.1f;
    }

    if (ambient != character->ambient_offset[0]) {
        character->ambient_offset[0] = ambient;
        character->ambient_offset[1] = ambient;
        character->ambient_offset[2] = ambient;
    }

    {
        CFrame                         frame;
        constexpr std::array<float, 2> ground = {-5.0f, 5.0f};
        constexpr std::array<float, 2> height = {0.0f, 20.0f};

        for (int i = 0; i < 8; i++) {
            frame.corner[i][0] = ground[(i & 1) != 0];
            frame.corner[i][1] = height[(i & 2) != 0];
            frame.corner[i][2] = ground[(i & 4) != 0];
            frame.corner[i][3] = 1.0f;
        }

        frame.SetPosition(pos);
        frame.SetRotation(rot[0], rot[1], rot[2]);
        DrawBound(&frame);
    }
}

void EdDebugMenu() {
    static int mode = 0;

    switch (mode) {
        case 0:
            DM_Main();
            break;
        case 2:
            DM_Sound();
            break;
        case 1:
            DM_Flag();
            break;
        default:
            break;
    }

    if (GamePad.Down(PAD_R2)) {
        mode++;
    }

    if (GamePad.Down(PAD_L2)) {
        mode--;
    }

    mode = std::max(mode, 0);

    if (mode >= 3) {
        mode = 2;
    }

    TexManager.ReloadTexture(GetVif1Packet(), 31);
    DebugFont->Draw();
}

void DM_Main() {
    static int                                select = 0;
    static int                                run_event = 150;
    static int                                talk_chara = 0;
    constexpr std::array<std::string_view, 2> on_off = {"OFF", "ON"};
    constexpr std::array<std::string_view, 2> cursor = {"  ", "->"};

    AddStr(DebugFont, "    MAIN    ->R2\n");
    AddStr(DebugFont, std::format("{}DEBUG CAMERA {}\n", cursor[select == 0], on_off[EdDebugCameraFlag]));
    AddStr(DebugFont, std::format("{}PARAMETER {}\n", cursor[select == 1], on_off[!EdDebugParamDrawOff]));
    AddStr(DebugFont, std::format("{}CHARACTER {}\n", cursor[select == 2], on_off[!EdDebugCharaDrawOff]));
    AddStr(DebugFont, std::format("{}MESSAGE {}\n", cursor[select == 3], on_off[!MesAbsDrawOff]));
    AddStr(DebugFont, std::format("{}DEBUG MOVE {}\n", cursor[select == 4], EdDebugMoveFlag));
    AddStr(DebugFont, std::format("{}RUN EVENT {}\n", cursor[select == 5], run_event));
    AddStr(DebugFont, std::format("{}TALK EVENT {}\n", cursor[select == 6], talk_chara));
    AddStr(DebugFont, std::format("{}EVENT {}\n", cursor[select == 7], on_off[EdDebugEventEnable]));
    AddStr(DebugFont, std::format("{}LANGUAGE {}\n", cursor[select == 8], LanguageCode));

    EdDebugRunEventNo = -1;

    switch (select) {
        case 0:
            if (GamePad.Down(PAD_RIGHT)) {
                EdDebugCameraFlag = 1;
            }

            if (GamePad.Down(PAD_LEFT)) {
                EdDebugCameraFlag = 0;
            }

            break;
        case 1:
            if (GamePad.Down(PAD_RIGHT)) {
                EdDebugParamDrawOff = 0;
            }

            if (GamePad.Down(PAD_LEFT)) {
                EdDebugParamDrawOff = 1;
            }

            break;
        case 2:
            if (GamePad.Down(PAD_RIGHT)) {
                EdDebugCharaDrawOff = 0;
            }

            if (GamePad.Down(PAD_LEFT)) {
                EdDebugCharaDrawOff = 1;
            }

            break;
        case 3:
            if (GamePad.Down(PAD_RIGHT)) {
                MesAbsDrawOff = 0;
            }

            if (GamePad.Down(PAD_LEFT)) {
                MesAbsDrawOff = 1;
            }

            break;
        case 4:
            if (GamePad.Down(PAD_RIGHT)) {
                EdDebugMoveFlag++;
            }

            if (GamePad.Down(PAD_LEFT)) {
                EdDebugMoveFlag--;
            }

            EdDebugMoveFlag = std::max(EdDebugMoveFlag, 0);

            EdDebugMoveFlag = std::min(EdDebugMoveFlag, 2);

            break;
        case 5:
            if (GamePad.Down(PAD_RIGHT)) {
                run_event++;
            }

            if (GamePad.Down(PAD_LEFT)) {
                run_event--;
            }

            if (GamePad.Down(PAD_L1)) {
                run_event -= 10;
            }

            if (GamePad.Down(PAD_R1)) {
                run_event += 10;
            }

            if (GamePad.On(PAD_CROSS)) {
                EdDebugRunEventNo = run_event;
            }

            break;
        case 6:
            if (GamePad.Down(PAD_RIGHT)) {
                talk_chara++;
            }

            if (GamePad.Down(PAD_LEFT)) {
                talk_chara--;
            }

            if (GamePad.Down(PAD_L1)) {
                talk_chara -= 10;
            }

            if (GamePad.Down(PAD_R1)) {
                talk_chara += 10;
            }

            if (GamePad.On(PAD_CROSS)) {
                EdTalkModeInit(EdVillager, talk_chara);
                EdDebugRunEventNo = 256;
            }

            break;
        case 7:
            if (GamePad.Down(PAD_RIGHT)) {
                EdDebugEventEnable = 1;
            }

            if (GamePad.Down(PAD_LEFT)) {
                EdDebugEventEnable = 0;
            }

            break;
        case 8:
            if (GamePad.Down(PAD_RIGHT)) {
                LanguageCode++;
            }

            if (GamePad.Down(PAD_LEFT)) {
                LanguageCode--;
            }

            LanguageCode = std::max<s32>(LanguageCode, LANG_JAPANESE);

            LanguageCode = std::min<s32>(LanguageCode, LANG_SPANISH);

            break;
        default:
            break;
    }

    if (GamePad.Down(PAD_DOWN)) {
        select++;
    }

    if (GamePad.Down(PAD_UP)) {
        select--;
    }

    if (select < 0) {
        select = 8;
    }

    if (select >= 9) {
        select = 0;
    }
}

void DM_Sound() {
    static int                                select = 0;
    static int                                bgm_no = 0;
    static int                                se_no = 0;
    static int                                set_no = 0;
    static int                                bgm_seq = 0;
    constexpr std::array<std::string_view, 2> on_off = {"OFF", "ON"};
    constexpr std::array<std::string_view, 2> cursor = {"  ", "->"};

    AddStr(DebugFont, "L2<-SOUND   ->\n");
    AddStr(DebugFont, std::format("{}BGM PLAY {} SEQ = {} x:PLAY start:STOP\n", cursor[select == 0], bgm_no, bgm_seq));
    AddStr(DebugFont, std::format("{}BGM OFF = {}\n", cursor[select == 1], on_off[SndGetBgmDisableFlag()]));
    AddStr(DebugFont, std::format("{}SE PLAY  {}  O:PLAY X:STOP\n", cursor[select == 2], se_no));
    AddStr(DebugFont, std::format("{}SOUND SET  {}  O:PLAY X:STOP\n", cursor[select == 3], set_no));

    switch (select) {
        case 0:
            if (GamePad.Down(PAD_RIGHT)) {
                bgm_no++;
            }

            if (GamePad.Down(PAD_LEFT)) {
                bgm_no--;
            }

            if (GamePad.Down(PAD_L1)) {
                bgm_no -= 10;
            }

            if (GamePad.Down(PAD_R1)) {
                bgm_no += 10;
            }

            if (GamePad.Down(PAD_TRIANGLE)) {
                bgm_seq++;
            }

            if (GamePad.Down(PAD_SQUARE)) {
                bgm_seq--;
            }

            bgm_seq = std::max(bgm_seq, 0);

            bgm_seq = std::min(bgm_seq, 3);

            if (GamePad.Down(PAD_CROSS)) {
                SndBgmStop();
                SndBgmInit();
                SndBgmLoad(bgm_no);
                SndBgmPlay(bgm_seq);
            }

            if (GamePad.Down(PAD_START)) {
                SndBgmStop();
                SndBgmInit();
            }

            break;
        case 1:
            if (GamePad.Down(PAD_RIGHT)) {
                SndBgmDisable(1);
            }

            if (GamePad.Down(PAD_LEFT)) {
                SndBgmDisable(0);
            }

            break;
        case 2:
            if (GamePad.Down(PAD_RIGHT)) {
                se_no++;
            }

            if (GamePad.Down(PAD_LEFT)) {
                se_no--;
            }

            if (GamePad.Down(PAD_L1)) {
                se_no -= 10;
            }

            if (GamePad.Down(PAD_R1)) {
                se_no += 10;
            }

            if (GamePad.Down(PAD_CROSS)) {
                SndSePlay(se_no, -1, 0);
            }

            if (GamePad.Down(PAD_START)) {
                SndSeStop(se_no, 0);
            }

            break;
        case 3:
            if (GamePad.Down(PAD_RIGHT)) {
                set_no++;
            }

            if (GamePad.Down(PAD_LEFT)) {
                set_no--;
            }

            if (GamePad.Down(PAD_L1)) {
                set_no -= 10;
            }

            if (GamePad.Down(PAD_R1)) {
                set_no += 10;
            }

            if (GamePad.Down(PAD_CROSS)) {
                SndSoundLoad(set_no);
            }

            break;
        default:
            break;
    }

    if (GamePad.Down(PAD_DOWN)) {
        select++;
    }

    if (GamePad.Down(PAD_UP)) {
        select--;
    }

    select = std::max(select, 0);

    if (select >= 4) {
        select = 3;
    }
}

namespace {
void RaiseFloorReached(CDngStatusData *status, int dungeon, int floor) {
    if (floor > status->floor_reached[dungeon]) {
        status->floor_reached[dungeon] = floor;
    }
}
} // namespace

void DM_Flag() {
    static int select = 0;
    static int game_no = 0;
    static int map_no = 0;
    static int comp_no = 0;
    static int dun_map = MapNo;
    static int chara = 0;

    // Maps inside a dungeon count as that dungeon.
    if (dun_map == 11) {
        dun_map = 1;
    }

    if (dun_map == 13) {
        dun_map = 1;
    }

    if (dun_map == 33) {
        dun_map = 1;
    }

    if (dun_map == 19) {
        dun_map = 2;
    }

    if (dun_map == 42) {
        dun_map = 3;
    }

    if (dun_map == 23) {
        dun_map = 4;
    }

    if (dun_map == 38) {
        dun_map = 5;
    }

    if (dun_map == 40) {
        dun_map = 5;
    }

    if (dun_map > 5) {
        dun_map = 0;
    }

    constexpr std::array<std::string_view, 2> on_off = {"OFF", "ON"};
    constexpr std::array<std::string_view, 2> cursor = {"  ", "->"};

    AddStr(DebugFont, "L2<-FALG   ->R\n");
    CDngStatusData *status = SaveData->GetDngStatus();
    AddStr(DebugFont, std::format("{}GAMEFLAG {:3d}    = {}\n", cursor[select == 0], game_no, on_off[SaveData->GetGameFlag(game_no)]));
    AddStr(DebugFont, std::format("{}MAPFLAG {:3d}     = {}\n", cursor[select == 1], map_no, on_off[!SaveData->GetMapFlag(MapNo, map_no)]));
    AddStr(DebugFont, std::format("{}COMPFLAG {:3d}    = {}\n", cursor[select == 2], comp_no, on_off[EditPartsInfo.GetCompEvent(comp_no)]));
    AddStr(DebugFont, std::format("{}GAME INT FALG 0 = {}\n", cursor[select == 3], SaveData->GetGameIntFlag(0)));
    AddStr(DebugFont, std::format("{}QUEST DUNGEON   = {}\n", cursor[select == 4], SaveData->QuestDungeon(dun_map, 0)));
    int floor = status->floor_reached[dun_map];
    AddStr(DebugFont, std::format("{}DUNGEON FLOOR   = {}\n", cursor[select == 5], floor));
    AddStr(DebugFont, std::format("{}PARTY NUM       = {}\n", cursor[select == 6], status->party_size));
    SV_GRD_NPC *npc = SaveData->GetGrdNPCData(MapNo, chara);
    int         talk = 0;

    if (npc != nullptr) {
        talk = npc->talk_message;
    }

    AddStr(DebugFont, std::format("{}TALKFLAG {:3d}    = {}\n", cursor[select == 7], chara, talk));

    int *value = nullptr;

    switch (select) {
        case 0:
            value = &game_no;

            if (GamePad.Down(PAD_CROSS)) {
                SaveData->SetGameFlag(game_no, 1);
            }

            if (GamePad.Down(PAD_START)) {
                SaveData->SetGameFlag(game_no, 0);
            }

            break;
        case 1:
            value = &map_no;

            if (GamePad.Down(PAD_CROSS)) {
                SaveData->SetMapFlag(MapNo, map_no, 0);
            }

            if (GamePad.Down(PAD_START)) {
                SaveData->SetMapFlag(MapNo, map_no, 1);
            }

            break;
        case 2:
            value = &comp_no;

            if (GamePad.Down(PAD_CROSS)) {
                EditPartsInfo.SetCompEvent(comp_no, 1);
            }

            if (GamePad.Down(PAD_START)) {
                EditPartsInfo.SetCompEvent(comp_no, 0);
            }

            if (GamePad.Down(PAD_TRIANGLE)) {
                for (int i = 0; i < 24; i++) {
                    EditPartsInfo.SetCompEvent(i, 1);
                }

                SV_GEORAMA_DATA *georama = SaveData->GetGrdData(MapNo);

                if (georama != nullptr) {
                    georama->request_event_flag = 1;
                }
            }

            if (GamePad.Down(PAD_SQUARE)) {
                for (int i = 0; i < 24; i++) {
                    EditPartsInfo.SetCompEvent(i, 0);
                }

                SV_GEORAMA_DATA *georama = SaveData->GetGrdData(MapNo);

                if (georama != nullptr) {
                    georama->request_event_flag = 0;
                }
            }

            break;
        case 3: {
            int flag = SaveData->GetGameIntFlag(0);

            if (GamePad.Down(PAD_RIGHT)) {
                flag++;
            }

            if (GamePad.Down(PAD_LEFT)) {
                flag--;
            }

            if (GamePad.Down(PAD_R1)) {
                flag += 10;
            }

            if (GamePad.Down(PAD_L1)) {
                flag -= 10;
            }

            flag = std::max(flag, 0);

            SaveData->SetGameIntFlag(0, flag);
            break;
        }
        case 4:
            if (GamePad.Down(PAD_RIGHT)) {
                SaveData->QuestDungeon(dun_map, 1);
            }

            if (GamePad.Down(PAD_LEFT)) {
                SaveData->QuestDungeon(dun_map, -1);
            }

            if (GamePad.Down(PAD_R1)) {
                SaveData->QuestDungeon(dun_map, 10);
            }

            if (GamePad.Down(PAD_L1)) {
                SaveData->QuestDungeon(dun_map, -10);
            }

            break;
        case 5: {
            floor = status->floor_reached[dun_map];

            if (GamePad.Down(PAD_RIGHT)) {
                floor++;
            }

            if (GamePad.Down(PAD_LEFT)) {
                floor--;
            }

            if (GamePad.Down(PAD_R1)) {
                floor += 10;
            }

            if (GamePad.Down(PAD_L1)) {
                floor -= 10;
            }

            RaiseFloorReached(status, dun_map, floor);
            break;
        }
        case 6:
            if (GamePad.Down(PAD_RIGHT)) {
                status->party_size++;
            }

            if (GamePad.Down(PAD_LEFT)) {
                status->party_size--;
            }

            if (GamePad.Down(PAD_R1)) {
                status->party_size += 10;
            }

            if (GamePad.Down(PAD_L1)) {
                status->party_size -= 10;
            }

            break;
        case 7:
            value = &chara;

            if (GamePad.Down(PAD_CROSS) && npc != nullptr) {
                npc->talk_message++;
            }

            if (GamePad.Down(PAD_START) && npc != nullptr) {
                npc->talk_message--;
            }

            if (GamePad.Down(PAD_TRIANGLE)) {
                for (int i = 0; i < 20; i++) {
                    SV_GRD_NPC *other = SaveData->GetGrdNPCData(MapNo, i);

                    if (other != nullptr) {
                        other->talk_message = 1;
                    }
                }
            }

            if (GamePad.Down(PAD_SQUARE)) {
                for (int i = 0; i < 20; i++) {
                    SV_GRD_NPC *other = SaveData->GetGrdNPCData(MapNo, i);

                    if (other != nullptr) {
                        other->talk_message = 0;
                    }
                }
            }

            break;
        default:
            break;
    }

    if (value != nullptr) {
        if (GamePad.Down(PAD_RIGHT)) {
            (*value)++;
        }

        if (GamePad.Down(PAD_LEFT)) {
            (*value)--;
        }

        if (GamePad.Down(PAD_R1)) {
            *value += 10;
        }

        if (GamePad.Down(PAD_L1)) {
            *value -= 10;
        }

        *value = std::max(*value, 0);
    }

    if (GamePad.Down(PAD_DOWN)) {
        select++;
    }

    if (GamePad.Down(PAD_UP)) {
        select--;
    }

    if (select < 0) {
        select = 7;
    }

    if (select >= 8) {
        select = 0;
    }
}

/* The box drawn as twelve edges of the frame's own corner list, taken to the screen in one go and
   drawn only if every corner survived: a box with one corner behind the eye would otherwise be
   drawn through a projected point that means nothing. */
namespace {
void DrawBound(CFrame *frame) {
    constexpr std::array<std::array<int, 2>, 12> edges = {
        {
         {0, 1},
         {1, 5},
         {5, 4},
         {4, 0},
         {2, 3},
         {3, 7},
         {7, 6},
         {6, 2},
         {0, 2},
         {4, 6},
         {5, 7},
         {1, 3},
         }
    };

    auto          screen = std::make_unique<int[][4]>(8);
    auto          world = std::make_unique<sceVu0FVECTOR[]>(8);
    sceVu0FMATRIX matrix;

    if (frame == nullptr) {
        return;
    }

    frame->GetLWMatrix(matrix);
    int visible = 1;
    ApplyMatrixN(world.get(), matrix, frame->corner, 8);

    for (int i = 0; i < 8; i++) {
        visible &= MGRotTransPers(screen[i], world[i], 0);
    }

    if (visible == 0) {
        return;
    }

    for (const auto &[from, to] : edges) {
        DrawLine(screen[from], screen[to], 128, 64, 64, 80);
    }
}

// One line in the colour given, blended through the current ALPHA register, depth-tested ALWAYS but
// writing depth as mgZBuffer says, with TEST and ZBUF restored to mglib's shadows afterwards. The
// ends are MGRotTransPers's GS window coordinates.
void DrawLine(const int *from, const int *to, u_char r, u_char g, u_char b, u_char a) {
    sceGsTest test = mgPixelTest;
    test.bits.ate = 0;
    test.bits.aref = 0;
    test.bits.atst = SCE_GS_ALWAYS;
    test.bits.zte = 1;
    test.bits.ztst = SCE_GS_ALWAYS;

    gfx::DrawState state = MGPortDrawState();
    MGPortApplyTest(state, test);
    MGPortApplyZbuf(state, mgZBuffer);
    state.blend = true;
    state.fog = false;

    std::array<gfx::Vertex2D, 2> line = {
        draw2d::Vertex(MGPortLogicalX(from[0] & 0xFFFF), MGPortLogicalY(from[1] & 0xFFFF),
                       MGPortDepth(static_cast<unsigned>(from[2]) & 0xFFFFFF), 0.0f, 0.0f, r, g, b, a),
        draw2d::Vertex(MGPortLogicalX(to[0] & 0xFFFF), MGPortLogicalY(to[1] & 0xFFFF),
                       MGPortDepth(static_cast<unsigned>(to[2]) & 0xFFFFFF), 0.0f, 0.0f, r, g, b, a),
    };
    draw2d::DrawUntextured(gfx::Primitive::Lines, line, state);

    MGPortCurrent().test = mgPixelTest;
    MGPortCurrent().zbuf = mgZBuffer;
}
} // namespace

// Runs in the town's step, ahead of the tick's drawing, so the scene to keep behind the menu is
// the frame before.
void EdSaveFrameImageTask() {
    if (frame_image_flag != 0) {
        MGPortMovePreviousFrameImage(reinterpret_cast<sceGsTex0 *>(&frame_image_tex.tex0));
        frame_image_tex.Initialize();
        frame_image_flag = 0;
    }
}
