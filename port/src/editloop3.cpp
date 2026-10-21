#include "common.h"

#include <libvu0.h>

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

#include "battlemenu.hpp"
#include "boxvu0.hpp"
#include "camera.hpp"
#include "camerafollow.hpp"
#include "character.hpp"
#include "clsmes.hpp"
#include "collision.hpp"
#include "dataalloc.hpp"
#include "dataread.hpp"
#include "dialog_npc.hpp"
#include "edit.hpp"
#include "editground.hpp"
#include "editloop.hpp"
#include "editloop3.hpp"
#include "editpartsinfo.hpp"
#include "frame.hpp"
#include "gamepad.hpp"
#include "gameutil.hpp"
#include "main.hpp"
#include "mapparts.hpp"
#include "mathutil.hpp"
#include "mglib.hpp"
#include "npcharacter.hpp"
#include "platform/config.hpp"
#include "quest_host.hpp"
#include "runscript.hpp"
#include "savedata.hpp"

void PortEdSetVillagerNextPos(CNPCharacter *villager, VILLAGER_INFO *info, CEditGround *ground);
int  PortEdRunEvent(int program, CDataAlloc2<1> *arena);

namespace {

RS_STACKDATA *world_map_result = nullptr;

float g_villager_ground_height[10] = {};
bool  g_villager_ground_height_valid[10] = {};
int   g_villager_ground_height_map = -1;

float SmoothVillagerGroundHeight(int villager, int map_no, float height) {
    if (g_villager_ground_height_map != map_no) {
        for (int i = 0; i < 10; i++) {
            g_villager_ground_height_valid[i] = false;
        }
        g_villager_ground_height_map = map_no;
    }

    if (!g_villager_ground_height_valid[villager]) {
        g_villager_ground_height[villager] = height;
        g_villager_ground_height_valid[villager] = true;
    } else {
        // Adjacent terrain triangles can produce slightly different ray hits from tick to tick.
        // Ease toward the sampled surface so those changes do not move a walking villager abruptly.
        g_villager_ground_height[villager] += (height - g_villager_ground_height[villager]) * 0.25f;
    }

    return g_villager_ground_height[villager];
}

// The villagers retail itself keeps (the two nearest of KeepNearVillagers). Only these play their
// footsteps: every footstep of a ground keys the same effect, so one from a villager retail would
// not have drawn cuts off the player's, even where it is too far away to be heard.
bool g_villager_retail_near[10] = {};

// Retail's GetNearVill and the choice its callers make from it. Of the villagers flagged to draw,
// the two nearest the player that stand in front of the camera stay flagged where they are within
// 150 of the player, and so does every one within video.detail_distance of the player; the rest
// fade out.
void KeepNearVillagers(CCamera *camera, CCharacter *player) {
    sceVu0FVECTOR player_position;
    sceVu0FVECTOR camera_direction;
    sceVu0FVECTOR camera_offset;
    sceVu0FVECTOR camera_position;
    sceVu0FVECTOR villager_position;
    int           nearest[2] = {-1, -1};
    float         nearest_distance[2] = {150.0f, 150.0f};
    float         detail = ConfigDetailDistance();

    player->GetPosition(player_position);
    camera->GetDir(camera_direction);
    camera->GetPos(camera_position);

    for (int i = 0; i < 10; i++) {
        if (EdVillager[i].near_camera == 0) {
            continue;
        }

        EdVillager[i].GetPosition(villager_position);
        float distance = DistVector(player_position, villager_position);
        EdVillager[i].near_camera = distance < detail;
        sceVu0SubVector(camera_offset, villager_position, camera_position);

        if (!(sceVu0InnerProduct(camera_offset, camera_direction) > 0.0f)) {
            continue;
        }

        if (distance < nearest_distance[0]) {
            nearest[1] = nearest[0];
            nearest_distance[1] = nearest_distance[0];
            nearest[0] = i;
            nearest_distance[0] = distance;
        } else if (distance < nearest_distance[1]) {
            nearest[1] = i;
            nearest_distance[1] = distance;
        }
    }

    for (int i = 0; i < 10; i++) {
        g_villager_retail_near[i] = false;
    }

    for (int i = 0; i < 2; i++) {
        if (nearest[i] >= 0) {
            EdVillager[nearest[i]].near_camera = true;
            g_villager_retail_near[nearest[i]] = true;
        }
    }
}

} // namespace

// Retail's EdMoveVillager, with a larger detail distance and the fixed ground model's collision
// added for walking villagers. The editable grid's flat polygons alone can place them above the
// sculpted Muska Racka terrain. A villager the detail distance keeps past retail's choice walks
// silently.
PORT_OVERRIDE void EdMoveVillager(VILLAGER_INFO *villagers) {
    CEditGround *ground = EdExchangeInfo.ground;
    CCharacter  *player = EdExchangeInfo.player;
    CCamera     *camera = EdExchangeInfo.camera;
    int          i;

    for (i = 0; i < 10; i++) {
        sceVu0FVECTOR  position;
        sceVu0FVECTOR  rotation;
        CNPCharacter  *npc = &EdVillager[i];
        VILLAGER_INFO *info = &villagers[i];
        npc->near_camera = false;

        if (info->placed == 0) {
            continue;
        }

        if (info->character_no >= 0 && npc->draw_enabled == 0) {
            continue;
        }

        if (info->character_no >= 0 && info->initial_motion == 0) {
            EDITPARTS_INFO *parts = EditPartsInfo.GetPartsInfo(info->character_no);

            if (parts->obtained == 0 || parts->elements[info->model_no].enabled == 0) {
                continue;
            }
        }

        npc->near_camera = true;
        sceVu0CopyVector(position, info->position);
        sceVu0CopyVector(rotation, info->rotation);

        if (info->initial_motion == 0) {
            npc->CCharacter::SetPosition(position);
            npc->CCharacter::SetRotation(rotation[0], rotation[1], rotation[2]);
        } else {
            sceVu0FVECTOR player_position;
            sceVu0FVECTOR villager_position;
            player->GetPosition(player_position);
            sceVu0CopyVector(villager_position, npc->pos);
            float distance = DistVector(player_position, villager_position);
            npc->sequence_enabled = 1;

            if (distance < 20.0f) {
                npc->sequence_enabled = 0;
                npc->motion_no = 0;
                npc->motion_flags = 0;
                npc->motion_speed = -1.0f;
            }

            PortEdSetVillagerNextPos(npc, info, ground);
        }
    }

    KeepNearVillagers(camera, player);

    for (i = 0; i < 10; i++) {
        EdVillager[i].Step();
        EdVillager[i].ShadowStep();
        EdVillager[i].ClothStep(0);

        if (EdVillager[i].event_status != 0) {
            sceVu0FVECTOR position;
            sceVu0FVECTOR hit;
            sceVu0CopyVector(position, EdVillager[i].pos);
            float altitude = ground->GetAlt(position[0], position[1], position[2]);
            bool  use_terrain = ground->map_no == TOWN_MUSKA_LACKA && villagers[i].initial_motion != 0;
            EdVillager[i].SetPosition(position[0], altitude, position[2]);
            EdVillager[i].FootSoundEnable(0);

            if (EdVillager[i].CheckDraw()) {
                EdVillager[i].FootSoundEnable(g_villager_retail_near[i]);
                WorkBuffer__2->used = 0;
                CCPoly *polys = reinterpret_cast<CCPoly *>(WorkBuffer__2->Alloc(2000));
                int     count = 0;
                if (!use_terrain) {
                    count = ground->PickUpEditAreaPoly(polys, position[0], position[1], position[2]);
                }
                CMapParts *parts = ground->GetParts(position[0], position[1], position[2]);

                if (parts != nullptr) {
                    CBoxVu0 box;
                    box.max[0] = position[0] + 20.0f;
                    box.min[0] = position[0] - 20.0f;
                    box.max[2] = position[2] + 20.0f;
                    box.min[2] = position[2] - 20.0f;
                    box.max[1] = 1000.0f;
                    box.min[1] = -1000.0f;
                    CFrame *frame = parts->GetCollisionFrame();

                    if (frame != nullptr) {
                        count += frame->PickUpNearPoly(polys + count, box);
                    }
                }

                if (use_terrain) {
                    CBoxVu0 box;
                    box.max[0] = position[0] + 1.0f;
                    box.min[0] = position[0] - 1.0f;
                    box.max[2] = position[2] + 1.0f;
                    box.min[2] = position[2] - 1.0f;
                    box.max[1] = 1000.0f;
                    box.min[1] = -1000.0f;
                    CFrame *terrain = ground->fixed_parts[0].GetCollisionFrame();
                    if (terrain != nullptr) {
                        count += terrain->PickUpNearPoly(polys + count, box);
                    }
                }

                if (count == 0) {
                    count = ground->PickUpEditAreaPoly(polys, position[0], position[1], position[2]);
                }

                if (count > 0) {
                    CCPoly poly;
                    position[1] += 30.0f;

                    if (GetFootPoly(position, 1000.0f, &poly, hit, polys, count, 0)) {
                        altitude = hit[1];
                        EdVillager[i].SetFootSoundID(poly.attr.foot_sound);
                    }
                }

                position[1] = altitude;
                if (use_terrain) {
                    position[1] = SmoothVillagerGroundHeight(i, ground->map_no, altitude);
                }
                EdVillager[i].SetPosition(position);
            }
        }
    }
}

// Retail's EdMoveVillagerSubMap, with KeepNearVillagers in place of GetNearVill and the choice of
// the two nearest.
PORT_OVERRIDE void EdMoveVillagerSubMap(VILLAGER_INFO *villagers) {
    CCharacter *player = EdExchangeInfo.player;
    CCamera    *camera = EdExchangeInfo.camera;
    int         i;

    for (i = 0; i < 10; i++) {
        if (EdVillager[i].villager_id >= 0) {
            EdVillager[i].near_camera = false;
            EdVillager[i].SetPosition(villagers[i].position);
            EdVillager[i].SetRotation(villagers[i].rotation);

            if (EdVillager[i].frame != nullptr) {
                EdVillager[i].near_camera = true;
            }
        }
    }

    KeepNearVillagers(camera, player);

    for (i = 0; i < 10; i++) {
        if (EdVillager[i].villager_id < 0) {
            EdVillager[i].near_camera = true;
        }

        EdVillager[i].Step();
        EdVillager[i].ShadowStep();
        EdVillager[i].ClothStep(0);
    }
}

// Retail's EdInitToEPInfo lays the cell map and names out 0x78 bytes in, past its own header; the
// host header is 0xA0 bytes, so 0x78 lands on its element names and function table. They start past
// the host's header instead.
PORT_OVERRIDE int EdInitToEPInfo(INIT_PARTSINFO *init, EPARTS_INFO_HEADER *header) {
    constexpr size_t cell_capacity = sizeof(init->cell) / sizeof(init->cell[0][0]);
    if (init == nullptr || header == nullptr || init->width < 0 || init->height < 0 || init->width > cell_capacity ||
        init->height > cell_capacity || init->width * init->height > cell_capacity) {
        return 0;
    }

    header->header_size = sizeof(EPARTS_INFO_HEADER);
    header->width = init->width;
    header->height = init->height;
    header->kind = init->kind;
    header->cell = nullptr;

    for (int i = 0; i < 6; i++) {
        header->element_id[i] = init->element_id[i];
        header->element_name[i] = nullptr;
    }

    u8 *write = reinterpret_cast<u8 *>(header) + header->header_size;
    header->cell = write;

    for (int i = 0; i < header->width * header->height; i++) {
        *write++ = init->cell[i][0];
    }

    for (int i = 0; i < 6; i++) {
        header->element_name[i] = reinterpret_cast<char *>(write);
        const char *terminator = static_cast<const char *>(
            std::memchr(init->element_name[i], '\0', sizeof(init->element_name[i])));
        const size_t name_length = terminator != nullptr ? static_cast<size_t>(terminator - init->element_name[i])
                                                         : sizeof(init->element_name[i]);
        std::memcpy(write, init->element_name[i], name_length);
        write += name_length;
        *write++ = '\0';
        *write++ = '\0';
    }

    header->data_size = write - reinterpret_cast<u8 *>(header);
    return header->data_size;
}

// Talking to a villager, retail's EdTalkMode with the dialog framework in front of it
// (docs/DIALOG_FRAMEWORK.md). The functions below are every one that reads retail's file statics
// for the conversation, so the port has its own copies of those.

namespace {
/**
 * Steps of a villager conversation, as EdTalkMode's talk_mode holds them.
 */
// clang-format off
enum TalkModeState {
    TALK_MODE_SELECT  = 0, /**< Choice prompt is shown with its cursor. */
    TALK_MODE_MESSAGE = 1, /**< Plain message is shown until it is closed. */
};

/**
 * Who shows the active conversation.
 */
enum TalkDialog {
    TALK_DIALOG_UNASKED = 0, /**< Not yet offered to the dialog framework. */
    TALK_DIALOG_OPEN    = 1, /**< The framework's window is up. */
    TALK_DIALOG_RETAIL  = 2, /**< Retail's message window, as the game has it. */
};

// clang-format on

/** Villager involved in the active conversation. */
static CNPCharacter *talk_villager__2;

/** Metadata identifier of the character involved in the active conversation. */
static int talk_chara_info_id;

/** Camera placement variant selected for the current conversation. */
static int talk_camera;

/** Conversation state currently being processed. */
static int talk_mode;

/** Selected choice in the active conversation prompt. */
static int talk_select;

/** Whether the active conversation message still needs to be created. */
static int TalkMesMake;

/** Message number the active conversation is showing. */
static int TalkMesNo;

/** Whether the dialog framework has the active conversation: it is asked on the conversation's first tick. */
static TalkDialog talk_dialog = TALK_DIALOG_RETAIL;
} // namespace

PORT_OVERRIDE int EdTalkModeInit(CNPCharacter *villager, int character_info_id) {
    if (villager->villager_id < 0) {
        return 0;
    }

    talk_villager__2 = villager;

    if (character_info_id < 0) {
        talk_chara_info_id = villager->villager_id;
    } else {
        talk_chara_info_id = character_info_id;
    }

    // The talk script runs next: what a quest takes over from it is held back from here.
    QuestHostTalkStarting(MapNo, talk_chara_info_id);

    talk_mode = TALK_MODE_SELECT;
    talk_dialog = TALK_DIALOG_UNASKED;
    TalkMesMake = 1;
    TalkMesNo = 0;
    talk_camera = 0;

    if (villager->event_status != 0) {
        talk_camera = rand() % 3;
    }

    talk_select = 0;
    EdEventInfo.talk_npc_id = (villager - EdVillager);
    EdEventInfo.talk_select_prompt = -1;
    return 1;
}

PORT_OVERRIDE CNPCharacter *EdNowTalkChara() {
    return talk_villager__2;
}

PORT_OVERRIDE int EdNowTalkCharaInfoID() {
    return talk_chara_info_id;
}

/**
 * Closes the conversation window and leaves it ready for the next message.
 */
namespace {
void EdCloseTalkMes() {
    EditMes1.text_rate = EditMes1.text_rate_set;
    EditMes1.mes_made = -1;
    EditMes1.fade_in = false;
    EditMes1.text_columns = MES_WIN_COLUMNS;
    EditMes1.text_rows = 0xA;
    EditMes1.text_len = 0;
    EditMes1.text_width = 0;
    EditMes1.text_height = 0;
    EditMes1.fade = 0.0f;
    EditMes1.fade_in = true;
    EditMes1.text_rate = EditMes1.text_rate_set;
    EditMes1.waiting = false;
    EditMes1.text_at = 0.0f;
    EditMes1.text_no = 0;
    EditMes1.text_from = 0;
    EditMes1.page_from = 0;
    EditMes1.InitMesWinTbl();
    EditMes1.clut_now = EditMes1.clut_default;
    EditMes1.wait = 0;
    EditMes1.blink = 0;
    EditMes1.auto_page_wait = 0;
    EditMes1.mes_made = -1;
    EditMes1.edge_alpha = 0x80;

    for (int i = 0; i < 10; i++) {
        EditMes1.mes_no[i] = -1;
    }

    for (int i = 0; i < 8; i++) {
        EditMes1.values[i] = 0;
    }

    EditMes1.value = 0;
    EditMes1.value_signed = false;
    EditMes1.value_show = true;
    EditMes1.value_narrow = false;
    EditMes1.space_width = -1;
    EditMes1.space_area = -1;
    EditMes1.cursor_row = -1;
    EditMes1.cursor_y = 0;
    EditMes1.cursor_lit = false;

    for (int i = 0; i < 10; i++) {
        EditMes1.line_pos[i].x = -1;
        EditMes1.line_pos[i].y = -1;
    }
}
} // namespace

/**
 * Runs the talking-to-a-villager mode for one frame.
 *
 * @mangled EdTalkMode__FP10CCharacterP13CCameraFollowiPi
 * @address 0x199090
 * @size 0xEBC
 */
PORT_OVERRIDE int EdTalkMode(CCharacter *player, CCameraFollow *camera, int mode, int *selection) {
    /**
     * Camera offsets from the speakers' midpoint for each of the three talk-camera views.
     */
    static sceVu0FVECTOR vv[3] = {
        {-18.8f, 7.1f, -21.3f, 1.0f},
        {21.0f,  6.5f, -5.7f,  1.0f},
        {22.6f,  6.9f, 16.5f,  1.0f},
    };

    GamePad.MenuModeOn(0x78);
    ReadBG();

    if (camera != nullptr) {
        camera->FollowOff();
    }

    EditMes1.Step();

    CNPCharacter *villager = talk_villager__2;
    villager->near_camera = true;
    sceVu0FVECTOR player_position;
    sceVu0FVECTOR villager_position;
    sceVu0FVECTOR player_rotation;
    sceVu0FVECTOR villager_rotation;
    sceVu0FVECTOR direction;
    player->GetPosition(player_position);
    player->GetRotation(player_rotation);
    villager->GetPosition(villager_position);
    villager->GetRotation(villager_rotation);
    sceVu0SubVector(direction, villager_position, player_position);
    float player_yaw = atan2f(direction[0], direction[2]);
    float villager_yaw = atan2f(-direction[0], -direction[2]);
    player_rotation[1] = AngleInterpolate(player_rotation[1], player_yaw, 0.1f, INTERPOLATE_STEP);
    player->SetRotation(player_rotation);
    villager_rotation[1] = AngleInterpolate(villager_rotation[1], villager_yaw, 0.1f, INTERPOLATE_STEP);

    if (EdInteriorFlag == 0 || EdEventInfo.villagers[talk_chara_info_id].talk_rotation == 0) {
        villager->SetRotation(villager_rotation);
    }

    sceVu0FVECTOR reference;
    sceVu0AddVector(reference, player_position, villager_position);
    sceVu0ScaleVector(reference, reference, 0.5f);
    reference[1] += 15.0f;

    if (camera != nullptr) {
        camera->FollowOff();
        camera->SetRef(reference);
    }

    sceVu0FVECTOR camera_position;
    sceVu0FMATRIX rotation;
    sceVu0UnitMatrix(rotation);
    sceVu0RotMatrixY(rotation, rotation, player_yaw);
    sceVu0ApplyMatrix(camera_position, rotation, vv[talk_camera]);
    sceVu0AddVector(camera_position, reference, camera_position);

    if (camera != nullptr) {
        camera->SetPos(camera_position);
    }

    if (camera != nullptr) {
        sceVu0FMATRIX view;
        sceVu0FVECTOR eye;
        camera->GetCameraMatrix(view);
        camera->GetPos(eye);
        MGSetViewMatrix(view, eye);
    }

    if (EdEventInfo.talk_select_prompt < 0) {
        talk_mode = TALK_MODE_MESSAGE;
        TalkMesNo = EdEventInfo.talk_messages[0];
    } else {
        TalkMesNo = EdEventInfo.talk_select_message;
    }

    int choice_count = 0;

    for (;;) {
        if (EdEventInfo.talk_messages[choice_count] < 0) {
            break;
        }

        choice_count++;
    }

    SaveData->GetGrdNPCData(MapNo, EdNowTalkCharaInfoID());

    // A villager a provider adds choices to opens on the framework's menu, in this window placed as
    // retail places its choice prompt. "Talk" hands back to the conversation as retail has it, from
    // the start, on the next tick; leaving ends it.
    if (talk_dialog == TALK_DIALOG_UNASKED) {
        // The talk script has run: what a quest held back of it can go back to retail.
        QuestHostTalkOpened();
        talk_dialog = DialogNpcOpen(MapNo, talk_chara_info_id, TalkMesNo) ? TALK_DIALOG_OPEN : TALK_DIALOG_RETAIL;
    }

    if (talk_dialog == TALK_DIALOG_OPEN) {
        switch (DialogNpcStep()) {
            case DialogNpcState::Running: {
                // Placed as retail places them: the villager's words over the villager, with the
                // tail and the talking motion of its messages; the player's choices over the player,
                // as its choice prompts are.
                int dialog_position[4];

                if (DialogNpcSpeaking()) {
                    EditMes1.tail_on = true;
                    EditMes1.auto_pos = MES_POS_AUTO;
                    EditMes1.AutoSetSub(villager, player, dialog_position);
                    villager->SetMotion(3, 0);
                } else {
                    if (mode != 0) {
                        EditMes1.tail_on = false;
                        EditMes1.auto_pos = MES_POS_BOTTOM;
                    }

                    EditMes1.AutoSetSub(player, villager, dialog_position);
                    villager->SetMotion(0, 0);
                }

                EditMes1.AutoSet(dialog_position);
                DialogNpcShowWindow();
                return 0;
            }
            case DialogNpcState::PlayOriginal:
                talk_dialog = TALK_DIALOG_RETAIL;
                EdCloseTalkMes();
                return 0;
            case DialogNpcState::Closed: {
                talk_dialog = TALK_DIALOG_RETAIL;
                EdCloseTalkMes();
                villager->SetMotion(0, 0);

                if (camera != nullptr) {
                    camera->FollowOn();
                }

                GamePad.AutoRepeatOff();
                GamePad.MenuModeOff();

                // A quest's choice opened a shop: off to it, as a shopkeeper's prompt goes, with the
                // villager behind the counter where the quest asks. Their model is the name they were
                // loaded with, or failing that their entry in the map's villager table, which
                // EdInitVilager loads them from.
                const char *model = villager->resource_name;

                if (model[0] == '\0' && talk_chara_info_id >= 0 && talk_chara_info_id < 16) {
                    model = EdEventInfo.villagers[talk_chara_info_id].name;
                }

                auto directory = std::make_unique<char[]>(4096);
                GetEditDataDir(directory.get());
                const std::string speaker = std::string(directory.get()) + "chara/" + model + ".chr";
                int               shop_no = 0;

                if (QuestHostTakeShop(model[0] != '\0' ? speaker.c_str() : nullptr, &shop_no)) {
                    EdSetShopNo(shop_no);
                    return 2;
                }

                // A quest showed an item received: its event runs, as a talk's own event does.
                int event_no = 0;

                if (selection != nullptr && QuestHostTakeItemShown(&event_no)) {
                    *selection = event_no;
                    return 4;
                }

                return 1;
            }
        }
    }

    switch (talk_mode) {
        case TALK_MODE_MESSAGE:
            if (GamePad.Down(PAD_CIRCLE | PAD_CROSS) != 0) {
                if (EditMes1.State() == CLSMES_SHOWN) {
                    EdCloseTalkMes();

                    if (camera != nullptr) {
                        camera->FollowOn();
                    }

                    villager->SetMotion(0, 0);
                    GamePad.AutoRepeatOff();
                    GamePad.MenuModeOff();
                    return 1;
                }

                if (EditMes1.State() == CLSMES_PAGE_WAIT) {
                    EditMes1.GoNextPage();
                } else {
                    EditMes1.text_rate = 0.0f;
                }
            }

            EditMes1.cursor_row = -1;
            villager->SetMotion(3, 0);
            break;
        case TALK_MODE_SELECT:
            if (GamePad.Down(PAD_UP) != 0) {
                talk_select--;

                if (talk_select < 0) {
                    talk_select = choice_count - 1;
                }
            }

            if (GamePad.Down(PAD_DOWN) != 0) {
                talk_select++;

                if (talk_select > choice_count - 1) {
                    talk_select = 0;
                }
            }

            if (GamePad.Down(PAD_CROSS) != 0) {
                if (EditMes1.State() == CLSMES_SHOWN) {
                    talk_mode = TALK_MODE_MESSAGE;
                    TalkMesMake = 1;
                    TalkMesNo = EdEventInfo.talk_messages[talk_select];

                    if (EdEventInfo.talk_select_prompt == 0 && talk_select == 3) {
                        SV_GRD_NPC *npc = SaveData->GetGrdNPCData(MapNo, EdNowTalkCharaInfoID());

                        if (npc != nullptr) {
                            npc->flags |= 2;
                        }
                    }

                    if ((talk_select == 0 && EdEventInfo.talk_select_prompt > 0 && EdEventInfo.talk_select_prompt != 4) || (talk_select > 0 && EdEventInfo.talk_select_prompt == 4)) {
                        EdCloseTalkMes();

                        if (camera != nullptr) {
                            camera->FollowOn();
                        }

                        villager->SetMotion(0, 0);
                        GamePad.AutoRepeatOff();
                        GamePad.MenuModeOff();

                        if (EdEventInfo.talk_select_prompt == 4) {
                            EdSetShopNo(EdEventInfo.talk_messages[talk_select]);

                            if (talk_select == 1) {
                                return 2;
                            }

                            if (talk_select == 2) {
                                return 3;
                            }

                            if (talk_select == 3) {
                                return 2;
                            }

                            if (talk_select == 4) {
                                return 2;
                            }
                        }

                        EdSetShopNo(EdEventInfo.talk_messages[0]);

                        if (EdEventInfo.talk_select_prompt == 1) {
                            return 2;
                        }

                        if (EdEventInfo.talk_select_prompt == 2) {
                            return 3;
                        }

                        if (EdEventInfo.talk_select_prompt == 3) {
                            if (selection != nullptr) {
                                *selection = EdEventInfo.talk_messages[0];
                            }

                            return 4;
                        }

                        return 1;
                    }
                } else {
                    EditMes1.text_rate = 0.0f;
                }
            }

            if (GamePad.Down(PAD_CIRCLE) != 0) {
                GamePad.AutoRepeatOff();
                GamePad.MenuModeOff();
                EdCloseTalkMes();

                if (camera != nullptr) {
                    camera->FollowOn();
                }

                villager->SetMotion(0, 0);
                return 1;
            }

            EditMes1.cursor_row = talk_select;
            villager->SetMotion(0, 0);
            break;
    }

    int talk_position[4];

    if (talk_mode == TALK_MODE_SELECT) {
        if (mode != 0) {
            EditMes1.tail_on = false;
            EditMes1.auto_pos = MES_POS_BOTTOM;
        }

        EditMes1.AutoSetSub(player, villager, talk_position);
        EditMes1.AutoSet(talk_position);
    } else {
        EditMes1.tail_on = true;
        EditMes1.auto_pos = MES_POS_AUTO;
        EditMes1.AutoSetSub(villager, player, talk_position);
        EditMes1.AutoSet(talk_position);
    }

    switch (talk_mode) {
        case TALK_MODE_SELECT:
            if (TalkMesMake != 0) {
                EditMes1.text_rate = 0.0f;
                TalkMesMake = EditMes1.MakeMesWin(TalkMesNo);
            }

            break;
        case TALK_MODE_MESSAGE:
            if (TalkMesMake != 0) {
                TalkMesMake = EditMes1.MakeMesWin(TalkMesNo);
            }

            break;
    }

    return 0;
}

// Starts the event interpreter held by this translation unit.
PORT_OVERRIDE int EdRunEvent(int program, CDataAlloc2<1> *arena) {
    return PortEdRunEvent(program, arena);
}

PORT_OVERRIDE int PortEventMenuMode = ED_MENU_NONE;

/**
 * Opens the world map while retaining the script result pointer at native width.
 */
PORT_OVERRIDE int PortWorldMapCommand(RS_STACKDATA *stack, int argument_count) {
    if (stack->type != RS_PTR) {
        return 0;
    }
    world_map_result = stack->p;
    PortEventMenuMode = ED_MENU_WORLD_MAP;
    return 1;
}

/**
 * Publishes the world-map choice when its menu finishes.
 */
int PortEdEventMenuMode() {
    const int done = EdMenuMode();
    if (done && PortEventMenuMode == ED_MENU_WORLD_MAP && world_map_result != nullptr) {
        world_map_result->i = MenuMapJumpMode + 1;
        world_map_result = nullptr;
    }
    return done;
}
