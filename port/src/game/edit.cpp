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
#include "npcharacter.hpp"
#include "objanime.hpp"
#include "rect.hpp"
#include "render/draw2d.hpp"
#include "render/draw3d.hpp"
#include "render/gs_state.hpp"
#include "savedata.hpp"
#include "shop.hpp"
#include "snd.hpp"
#include "sound.hpp"
#include "sysmes.hpp"
#include "texture.hpp"

namespace dc::game {

using namespace dc::render;

int EdDebugEventEnable = 1;
int EdDebugCameraFlag;
int EdDebugParamDrawOff;
int EdDebugCharaDrawOff;
int EdDebugMoveFlag;
int EdDebugRunEventNo;

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

    int volume = static_cast<int>(level * static_cast<float>(SndGetDefaultBgmVol()));

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

namespace {

/**
 * Stores one positional sound effect and every place it is heard from this frame.
 */
struct SOUND_SRC {
    int                   se;     /**< Sound effect the slot plays; negative when the slot is free. */
    int                   num;    /**< Number of places entered this frame. */
    std::array<float, 16> volume; /**< Volume the sound arrives at from each place. */
    std::array<float, 16> pan;    /**< Pan the sound arrives at from each place. */
};

// A source is one sound effect and every place it is heard from at once, so a sound heard from
// two sides occupies one slot and plays at a single volume and pan.
std::array<SOUND_SRC, 4> sound_src;
std::array<int, 4>       now_play_se;
std::array<int, 4>       now_play_se_flag;

void init_sound_src() {
    for (SOUND_SRC &source : sound_src) {
        source.se = -1;
        source.num = 0;
    }
}

/* The slot a sound effect already occupies, or a free one, or nothing - which caps the world at
   four sounds at once. */
SOUND_SRC *get_sound_src(int se) {
    for (SOUND_SRC &source : sound_src) {
        if (se == source.se) {
            return &source;
        }
    }

    for (SOUND_SRC &source : sound_src) {
        if (source.se < 0) {
            return &source;
        }
    }

    return nullptr;
}

/* Claims a channel for the effect: 1 when it was already playing, 0 when the caller must start
   it, and -1 when every channel is taken. The flag keeps this frame's sounds alive past the sweep
   at the end of the frame. */
int check_se_play(int se) {
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

} // namespace

void EdInitSoundSrc() {
    int i;

    for (i = 0; i < 4; i++) {
        now_play_se[i] = -1;
        now_play_se_flag[i] = 0;
    }

    init_sound_src();
}

/* A sound entered from several places plays at the sum of their volumes and at their pans averaged
   by volume, so the nearer place decides where it sits. Whatever was playing and was not entered
   this frame is stopped. */
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

namespace {

/* How far a point is from a segment, and where on the segment it is nearest to. When the
   projection falls outside the segment, the nearer end is the answer. */
float GetDistLine(float *point, float *from, float *to, float *near_point) {
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

} // namespace

/* The sources are rebuilt from scratch every frame. The part's own frame is moved first because
   the effects hang off it, and a river is the one source the part itself carries rather than one
   of its effects. */
void EdSetSoundSrcVol(float time, CMapParts **parts, int count, float *camera_pos, float *camera_dir) {
    int               i;
    int               j;
    CFrame           *frame;
    EDIT_EFFECT_INFO *effect;
    int               se;
    SOUND_SRC        *source;
    SOUND_SRC        *river_source;
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
    float             river_volume;
    float             river_pan;

    SndSetCamera(camera_pos, camera_dir);
    init_sound_src();

    for (i = 0; i < count; i++) {
        if (parts[i] == nullptr) {
            continue;
        }

        frame = parts[i]->frame[0];

        if (frame == nullptr) {
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

            if (effect == nullptr) {
                continue;
            }

            if (effect->kind != EDIT_EFFECT_SOUND && effect->kind != EDIT_EFFECT_FIRE_LARGE && effect->kind != EDIT_EFFECT_FIRE_SMALL) {
                continue;
            }

            if (effect->frame == nullptr) {
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
            se = static_cast<int>(effect->sound_no);

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

            if (source == nullptr) {
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

        river_source = get_sound_src(52);

        if (river_source == nullptr) {
            continue;
        }

        SndGetVolPan(&river_volume, &river_pan, position, 100.0f, 300.0f);
        river_source->se = 52;

        if (river_source->num < 16) {
            river_source->volume[river_source->num] = river_volume;
            river_source->pan[river_source->num] = river_pan;
            river_source->num++;
        }
    }

    EdPlaySoundSrc();
}

void EdDoorOpenSe(int door_no, float *position) {
    static constexpr std::array<int, 8> se_open = {0x6C, 0x7E, 0x6E, 0x80, 0x74, 0x82, 0x70, 0x72};

    if (door_no < 0 || door_no >= 8) {
        return;
    }

    SndSePlay(se_open[door_no], -1, 0);
}

void EdDoorCloseSe(int door_no, float *position) {
    static constexpr std::array<int, 8> se_close = {0x6D, 0x7F, 0x6F, 0x81, 0x75, 0x83, 0x71, 0x73};

    if (door_no < 0 || door_no >= 8) {
        return;
    }

    SndSePlay(se_close[door_no], -1, 0);
}

int EdGetDoorMotion(int door_no, int state) {
    static constexpr std::array<std::array<int, 2>, 8> motion = {
        {
         {3, 4},
         {10, 10},
         {3, 4},
         {10, 10},
         {3, 4},
         {10, 10},
         {10, 10},
         {10, 10},
         }
    };

    if (door_no < 0 || door_no >= 8) {
        return 0;
    }

    return motion[door_no][state != 0];
}

namespace {

// One depth-of-field description at a time, taken from the map's own data or replaced by a
// default set.
DEPTH_OF_FIELD_INFO dof;

} // namespace

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
    int drawn_level = dof.level;

    if (drawn_level < 0) {
        return;
    }

    if (level < drawn_level) {
        drawn_level = level;
    }

    DepthOfField(dof.distance, drawn_level, dof.alpha, dof.blur);
}

namespace {

// Lightning is a model node shown for a few frames plus a flash added to the parallel lights. The
// node is named in the ground's own model, so a map without it has no lightning.
int           thunder_count;
int           start_thunder;
int           next_thunder_cnt;
sceVu0FMATRIX thd_light;
sceVu0FMATRIX thd_color;

} // namespace

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

        if (frame != nullptr) {
            frame = frame->SearchFrame("inazuma");
        }

        if (frame == nullptr) {
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

namespace {

bool IsVisible(const CNPCharacter *npc) {
    return npc->initialized != 0 && npc->draw_enabled != 0;
}

} // namespace

void EdDrawCharacter(CCharacter *player, int player_draw_mask, int npc_count, CNPCharacter *npcs, int *npc_draw_masks, int draw_shadows, ED_EVENT_INFO *event) {
    int i;

    if (EdDebugCharaDrawOff != 0) {
        player_draw_mask = 0;
    }

    if (event != nullptr) {
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

        if (npc_draw_masks != nullptr) {
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
        for (i = 0; i < npc_count && npc_draw_masks != nullptr; i++) {
            CNPCharacter *npc = &npcs[i];

            if (npc->CheckDraw() != 0 && event->npc_draw_before[i] != 0 && (npc_draw_masks[i] & 1) != 0 && IsVisible(npc)) {
                npc_draw_masks[i] = 0;
                TexManager.ReloadTexture(Vif1Packet, npc->texture_block);
                npc->TextureAnime(npc->texture_block);
                CCharacter *chara = npc;
                chara->Draw();
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
        MGBeginDrawShadow(*reinterpret_cast<sceGsTex0 *>(&TexManager.GetTexture("shadow_buff", -1)->tex0));

        if (player_draw_mask & 2) {
            player->DrawShadow();
        }

        for (i = 0; i < npc_count && npc_draw_masks != nullptr; i++) {
            if ((npc_draw_masks[i] & 2) != 0 && IsVisible(&npcs[i])) {
                CCharacter *chara = &npcs[i];
                chara->DrawShadow();
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

    for (i = 0; i < npc_count && npc_draw_masks != nullptr; i++) {
        if (npcs[i].CheckDraw() != 0 && (npc_draw_masks[i] & 1) != 0 && IsVisible(&npcs[i])) {
            TexManager.ReloadTexture(Vif1Packet, npcs[i].texture_block);
            npcs[i].TextureAnime(npcs[i].texture_block);
            CCharacter *chara = &npcs[i];
            chara->Draw();
        }
    }
}

/* The effect is built in the first unclaimed entry of the description table and registered in the
   part's slot table, which is what the sound and the drawing walk later. */
void EnterPartsEffect(CMapParts *parts, EPARTS_FUNC_DATA *func_data, EDIT_EFFECT_INFO *effects, int count) {
    int index = 0;

    // The last entry is never handed out, so a full table refuses rather than overruns.
    for (int i = 1; i <= count; i++, index++) {
        if (i == count) {
            return;
        }

        if (effects[index].kind <= EDIT_EFFECT_NONE) {
            break;
        }
    }

    EDIT_EFFECT_INFO *effect = &effects[index];

    if (InitEditEffect(parts->frame[0], func_data, effect) == 0) {
        return;
    }

    for (int slot = 0; slot < 24; slot++) {
        if (parts->effect_on[slot] == 0) {
            parts->effect_on[slot] = 1;
            parts->effect[slot] = effect;
            return;
        }
    }
}

namespace {

// Entering a menu takes four frames: the counter is reset by EdInitMenu and only the fourth
// EdInitModeFinish reaches the menu.
int                 init_menu_cnt;
int                 menu_mode;
int                 use_item;
int                 shop_no;
int                 name_reg_chara;
std::array<int, 32> use_item_list;

// The menus number weapons differently from the rest of the game, so every item number crossing
// into or out of a menu is converted.
int ConvertItemNo(int item) {
    return TransWepNo(item);
}

int InvertItemNo(int item) {
    return TransWepNoNewToOld(item);
}

} // namespace

void EdUseItemInit() {
    use_item_list[0] = ITEM_ATTACH_PEARL;
    use_item_list[1] = -1;
    use_item = -1;
}

void EdSetUseItem(int *items) {
    for (int i = 0; i < 32; i++) {
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

// The menus draw over a still of the last game frame, so the frame is kept before the menu is built.
int EdInitModeFinish(CCamera *camera, CTexture *texture) {
    init_menu_cnt++;

    if (init_menu_cnt > 3) {
        init_menu_cnt = 4;
    }

    if (init_menu_cnt < 4) {
        return 0;
    }

    if (texture != nullptr) {
        EdSaveFrameImage(*texture);
    }

    GamePad.MenuModeOn(120);

    std::array<int, 6> texture_block = {16, 17, 18, 25, 32, 33};

    switch (menu_mode) {
        case ED_MENU_EDIT:
            EditMenuInit(texture_block.data(), EditMenuStatus.mode == EDIT_MENU_MODE_PLACE);
            return ED_MODE_GEORAMA_MENU;
        case ED_MENU_BATTLE:
            BattleMenuInit(texture_block.data(), BATTLE_MENU_MODE_TOWN);
            break;
        case ED_MENU_SHOP:
        case ED_MENU_UNK_4:
            ShopNoInput(texture_block.data(), shop_no, 1);
            break;
        case ED_MENU_USE_ITEM:
            InitEventItemSelect(16, use_item_list.data(), &SaveData->GetDngStatus()->item_pack, 180, 210, 1, 0);
            break;
        case ED_MENU_CHANGE_ESA:
            InitEventItemSelect(16, use_item_list.data(), &SaveData->GetDngStatus()->item_pack, 180, 210, 1, 1);
            break;
        case ED_MENU_NAME_REGISTRY:
            InitNameRegist(name_reg_chara, 16, nullptr);
            break;
        case ED_MENU_WORLD_MAP:
            InitMenuMove(MENU_MOVE_WORLD_MAP_DIRECT, 16, reinterpret_cast<u_long128 *>(read_buffer));
            break;
        case ED_MENU_FISHING_EXCHANGE:
            InitFishingExchange(nullptr, texture_block.data(), 0);
            break;
        case ED_MENU_FISH_RANKING:
            InitFishRecordView(nullptr, texture_block.data(), 0);
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
    int finished = 0;

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
    if (count < 0) {
        count = 1;
    }

    CDngStatusData *status = SaveData->GetDngStatus();
    int             item_no = ConvertItemNo(item);

    if (item_no < 0) {
        return;
    }

    for (int i = 0; i < count; i++) {
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
    ITEM_PACK *pack = &SaveData->GetDngStatus()->item_pack;
    int        new_count = pack->num + amount;

    if (new_count < 0) {
        new_count = 0;
    }

    if (new_count > 100) {
        new_count = 100;
    }

    for (int i = pack->num; i < new_count; i++) {
        pack->item[i] = -1;
    }

    pack->num = static_cast<char>(new_count);
    return new_count;
}

int EdCheckItemOver() {
    return SaveData->GetDngStatus()->overflow_flag;
}

void EdClearItemOverFlag() {
    SaveData->GetDngStatus()->overflow_flag = false;
}

int EdCheckItem(int item) {
    CDngStatusData *status = SaveData->GetDngStatus();
    int             item_no = ConvertItemNo(item);

    if (item_no >= 0) {
        return status->SearchItemIndexNo(item_no);
    }

    return -1;
}

int EdCheckGetItem(int item) {
    CDngStatusData *status = SaveData->GetDngStatus();
    int             item_no = ConvertItemNo(item);

    if (item_no >= 0) {
        return status->CheckItemGet(item_no);
    }

    return 0;
}

void EdGetItemFile(int item_no, char *model_path, char *texture_path) {
    BtGetItemNamePath(model_path, texture_path, item_no);
}

void EdDrawItem() {
    if (EdEventInfo.item_frame[0] != nullptr) {
        TexManager.ReloadTexture(GetVif1Packet(), 0x28);
        MGDraw(EdEventInfo.item_frame[0]);
    }
}

namespace {

// The editor's message window is driven alongside the town's own system messages and drawn after
// them. A help message is built through EdSetSystemMes and its number and count are then moved
// into the help pair, which leaves the system slot free for the next message.
int SystemMesNo = -1;
int HelpMesNo = -1;
int SystemMesCount;
int SystemMesPosition;
int SystemMesW;
int SystemMesH;
int SystemMesX;
int SystemMesY;
int SystemMesWait;
int SystemMesInputKey;
int HelpMesCount;
int build_help_delay;
int move_help_delay;

} // namespace

void EdSetSystemMes(int mes_no, int count, int position, int input_key, int *args, int value) {
    SystemMesInputKey = input_key;
    SystemMesPosition = position;

    if (args != nullptr) {
        for (int i = 0; i < 4; i++) {
            EditSystemMes.mes_no[i] = -1;

            if (args[i] >= 0) {
                EditSystemMes.mes_no[i] = args[i];
            }
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
    if (mes_no == 120) {
        int indent = 16 - (EditSystemMes.text_columns - 30);

        if (indent < 0) {
            indent = 0;
        }

        int y = SystemMesY;
        int x = SystemMesX + ((indent * EditSystemMes.char_width) >> 1);
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

/* The window's width is not known before the text is laid out, so these messages place it by hand:
   this one centres a forty-character line and the rest give a left edge outright. */
void EdEditMainHelpMes(int building) {
    SystemMesW = 0;
    SystemMesH = 0;
    SystemMesX = 320 - ((EditSystemMes.char_width * 40) >> 1);
    SystemMesY = 380;
    EdSetHelpMes(100 + (building != 0), 2, -1, nullptr, -1);
}

void EdWalkToEditMes(int wait) {
    SystemMesW = 0;
    SystemMesH = 0;
    SystemMesX = 40;
    SystemMesY = 380;
    SystemMesWait = wait;
    EdSetHelpMes(110, 120, -1, nullptr, -1);
}

/* The build and move prompts take turns: each counts down its own delay before showing itself and
   then arms the other's, so the two alternate rather than both being up. */
void EdEditBuildHelpMes(int part_no) {
    if (part_no < 0) {
        return;
    }

    if (build_help_delay > 0) {
        build_help_delay--;
        return;
    }

    build_help_delay = 0;
    move_help_delay = 1;

    std::array<int, 4> args = {-1, -1, -1, -1};

    args[0] = GetAtraMsgNo(MapNo, part_no);
    SystemMesW = 600;
    SystemMesH = 50;
    SystemMesX = 30;
    SystemMesY = 380;
    EdSetHelpMes(120, 2, -1, args.data(), -1);
}

void EdEditMoveHelpMes() {
    if (move_help_delay > 0) {
        move_help_delay--;
        return;
    }

    move_help_delay = 0;
    build_help_delay = 1;
    SystemMesW = 0;
    SystemMesH = 0;
    SystemMesX = 230;
    SystemMesY = 412;
    EdSetHelpMes(130, 2, -1, nullptr, -1);
}

// Message 201 names the bait and 200 says there is none.
void EdFishingWalkHelpMes(int bait) {
    SystemMesW = 0;
    SystemMesH = 0;

    std::array<int, 4> args = {-1, -1, -1, -1};

    args[0] = bait + 100;
    int mes_no = 200;

    if (bait >= 0) {
        mes_no++;
    }

    EdSetHelpMes(mes_no, 2, 9, args.data(), -1);
}

void EdFishingAngleHelpMEs(int bait) {
}

void EdFishingLostEsaMes() {
    SystemMesW = 0;
    SystemMesH = 0;
    EdSetHelpMes(210, 180, 8, nullptr, -1);
}

void EdItemGetMes(int item, int kind, int attachment, int duration) {
    ItemGetMes(item, attachment, duration, 1);
}

namespace {

// One treasure box at a time: its lid (the model node `top`) turns over a count that runs down from
// 40, and the ambient light fades over the last twenty frames.
int           ibox_open_flag;
int           ibox_open_close_flag;
int           ibox_open_cnt;
CFrameVu1    *ibox_frame;
CFrameVu1    *ibox_base_frame;
sceVu0FVECTOR ibox_pos;
sceVu0FVECTOR ibox_rot;

} // namespace

void EdInitOpenItemBox(CFrameVu1 *frame, CFrameVu1 *base) {
    ibox_open_close_flag = 0;
    ibox_open_flag = 0;
    ibox_open_cnt = 0;
    ibox_frame = frame;
    ibox_base_frame = base;
}

void EdSetOpenItemBox(float *position, float *rotation) {
    if (ibox_frame != nullptr) {
        ibox_open_flag = 1;
        ibox_open_close_flag = 0;
        ibox_open_cnt = 40;
        sceVu0CopyVector(ibox_pos, position);
        sceVu0CopyVector(ibox_rot, rotation);
    }
}

void EdStepOpenItemBox() {
    if (ibox_open_flag == 0 && ibox_open_close_flag == 0) {
        return;
    }

    ibox_open_cnt--;

    if (ibox_open_cnt < 0) {
        ibox_open_flag = 0;
        ibox_open_close_flag = 0;
        return;
    }

    if (ibox_frame == nullptr) {
        return;
    }

    ibox_frame->SetPosition(ibox_pos);
    ibox_frame->SetRotation(ibox_rot[0], ibox_rot[1], ibox_rot[2]);
    CFrame *lid = ibox_frame->SearchFrame("top");
    lid->SetRotType(2);

    if (ibox_open_flag != 0) {
        // The lid swings over the first half of the count and stands open through the second.
        if (ibox_open_cnt <= 20) {
            return;
        }

        float open_angle = -(HALF_PI - HALF_PI * static_cast<float>(ibox_open_cnt - 20) / 20.0f);
        lid->SetRotation(open_angle, 0.0f, 0.0f);
    } else {
        float close_angle = 0.0f;

        if (ibox_open_cnt >= 6) {
            close_angle = HALF_PI - HALF_PI * static_cast<float>(ibox_open_cnt - 5) / 5.0f;
        } else {
            close_angle = HALF_PI * static_cast<float>(ibox_open_cnt) / 5.0f;
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

    if (ibox_frame != nullptr) {
        ibox_frame->SetPosition(ibox_pos);
        ibox_frame->SetRotation(ibox_rot[0], ibox_rot[1], ibox_rot[2]);
    }

    MGGetAmbient(ambient);
    sceVu0CopyVector(saved_ambient, ambient);

    if (ibox_open_cnt < 20) {
        ambient[3] = 128.0f * static_cast<float>(ibox_open_cnt) / 20.0f;
    }

    if (ibox_open_flag != 0) {
        MGSetAmbient(ambient);
    }

    MGDraw(ibox_frame);
    MGSetAmbient(saved_ambient);
}

int      frame_image_flag;
CTexture frame_image_tex;

void EdSaveFrameImage(CTexture texture) {
    frame_image_flag = 1;
    frame_image_tex = texture;
}

// Runs in the town's step, ahead of the tick's drawing, so the scene to keep behind the menu is
// the frame before.
void EdSaveFrameImageTask() {
    if (frame_image_flag != 0) {
        MGPortMovePreviousFrameImage(reinterpret_cast<sceGsTex0 *>(&frame_image_tex.tex0));
        frame_image_tex.Initialize();
        frame_image_flag = 0;
    }
}

void EdSaveFrameImageInit() {
    frame_image_tex.Initialize();
    frame_image_flag = 0;
}

int EdMenuLoop(ClsMes *message) {
    if (message == nullptr) {
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

            for (s32 &mes_no : message->mes_no) {
                mes_no = -1;
            }

            for (s32 &value : message->values) {
                value = 0;
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

            for (MES_LINE_POS &line : message->line_pos) {
                line.x = -1;
                line.y = -1;
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

} // namespace dc::game
