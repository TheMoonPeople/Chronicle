#include "eastking.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <string>

#include "clsmes.hpp"
#include "dataread.hpp"
#include "gamepad.hpp"
#include "menu_draw.hpp"
#include "menu_inventory.hpp"
#include "menu_manual.hpp"
#include "savedata.hpp"
#include "snd.hpp"
#include "texture.hpp"

namespace dc::game {

namespace {

/**
 * Stages of the East King event's opening, dialogue, and closing sequence.
 */
enum class EastKingMode : s16 {
    Loading,
    Closing,
    Dialogue,
    MessageFadeOut,
    MessageFadeIn,
};

/**
 * Holds the East King event's loaded resources and transition state.
 */
struct EAST_KING_EVENT_STATE {
    s32          texture_block; /**< Texture block used by the event picture. */
    char         unk_04[6];
    s16          event_no;        /**< Selects the event picture, messages and unlock flag. */
    s16          resources_ready; /**< Whether the picture and message resources are installed. */
    char         unk_0E[2];
    u_long128   *load_buffer; /**< Aligned buffer containing the event archive. */
    EastKingMode mode;        /**< Current loading, message, or closing state. */
    char         unk_16[2];
    s32          transition_frame; /**< Frames elapsed in the current transition. */
};

/**
 * Holds the current East King message and its fade state.
 */
struct EAST_KING_MESSAGE_STATE {
    s16 message_no;   /**< Message currently selected for the event. */
    s16 unk_02;       /**< Cleared when the event starts; nothing reads it. */
    s16 draw_message; /**< Whether the selected message may be laid out. */
    s16 unk_06;
    s16 unk_08;
    s16 alpha; /**< Opacity of the current message window. */
};

static_assert(sizeof(EAST_KING_MESSAGE_STATE) == 0xC);

/** Fully opaque alpha for the message window and the screen fade. */
constexpr int kOpaqueAlpha = 0x80;

/** Frames the opening and closing fades last before the next stage may begin. */
constexpr int kTransitionFrames = 0x40;

/** First game flag of the per-event unlock flags. */
constexpr int kEventSeenFlag = 0xE6;

/** Character width of the message font for each menu language. */
constexpr std::array<s8, 8> kFontWidths = {16, 14, 14, 15, 15, 15, 15};

/** Background music number saved before the event. */
s16 PrevEastKingSndNo;

/** Buffer used to reload the background music that preceded the event. */
u_int *EastKingSndReadBuf;

/** State of the active East King event. */
EAST_KING_EVENT_STATE EastKing;

/** Message selection and opacity state of the active East King event. */
EAST_KING_MESSAGE_STATE EastKingMsg;

/** Number of additional messages in each East King event. */
constexpr std::array<s8, 12> EastKingMsgMax = {2, 2, 3, 3, 2, 3, 2, 2, 2, 2, 6, 1};

/** First message of the current event. */
int FirstEventMessage() {
    return EastKing.event_no * 100 + 100;
}

/**
 * Restores the East King message window to its initial empty state.
 */
void ResetEastKingMessage() {
    EastKingMsgCls.text_columns = MES_WIN_COLUMNS;
    EastKingMsgCls.text_rows = 10;
    EastKingMsgCls.text_len = 0;
    EastKingMsgCls.text_width = 0;
    EastKingMsgCls.text_height = 0;
    EastKingMsgCls.fade = 0.0f;
    EastKingMsgCls.fade_in = true;
    EastKingMsgCls.text_rate = EastKingMsgCls.text_rate_set;
    EastKingMsgCls.waiting = false;
    EastKingMsgCls.text_at = 0.0f;
    EastKingMsgCls.text_no = 0;
    EastKingMsgCls.text_from = 0;
    EastKingMsgCls.page_from = 0;
    EastKingMsgCls.InitMesWinTbl();
    EastKingMsgCls.clut_now = EastKingMsgCls.clut_default;
    EastKingMsgCls.wait = 0;
    EastKingMsgCls.blink = 0;
    EastKingMsgCls.auto_page_wait = 0;
    EastKingMsgCls.mes_made = -1;
    EastKingMsgCls.edge_alpha = kOpaqueAlpha;
    std::ranges::fill(EastKingMsgCls.mes_no, -1);
    std::ranges::fill(EastKingMsgCls.values, 0);
    EastKingMsgCls.value = 0;
    EastKingMsgCls.value_signed = false;
    EastKingMsgCls.value_show = true;
    EastKingMsgCls.value_narrow = false;
    EastKingMsgCls.space_width = -1;
    EastKingMsgCls.space_area = -1;
    EastKingMsgCls.cursor_row = -1;
    EastKingMsgCls.cursor_y = 0;
    EastKingMsgCls.cursor_lit = false;

    for (auto &line : EastKingMsgCls.line_pos) {
        line.x = -1;
        line.y = -1;
    }
}

/**
 * Draws the current East King event message.
 */
void EastKingMsgDraw() {
    if (EastKing.resources_ready != 0 && EastKingMsgCls.mes_made != EastKingMsg.message_no && EastKingMsg.draw_message != 0) {
        EastKingMsgCls.MakeMesWin(EastKingMsg.message_no);
    }

    MenuTextureReload(EastKingMsgCls.tex_block);

    switch (EastKing.mode) {
        case EastKingMode::Loading:
            if (EastKing.resources_ready == 0) {
                break;
            }
            [[fallthrough]];
        case EastKingMode::Dialogue:
        case EastKingMode::MessageFadeIn:
            EastKingMsg.alpha += 2;
            break;
        case EastKingMode::Closing:
        case EastKingMode::MessageFadeOut:
            EastKingMsg.alpha -= 2;
            break;
    }

    EastKingMsg.alpha = std::clamp<s16>(EastKingMsg.alpha, 0, kOpaqueAlpha);

    if (EastKing.resources_ready != 0) {
        int half_width = EastKingMsgCls.char_width >> 1;
        GetMenuCommonPutXY(&EastKingMsgCls, 0x14C - half_width);
        EastKingMsgCls.text_y = 0x156;
        EastKingMsgCls.edge_alpha = EastKingMsg.alpha;
        EastKingMsgCls.Step();
        EastKingMsgCls.DrawMesWin();
    }
}

} // namespace

ClsMes *ManualMsg;

s16 PrevEastKingSndVol;

ClsMes EastKingMsgCls;

void EastKingTextureEnter() {
    if (EastKing.resources_ready == 0 && ReadBGSync() == 0 && SndBgmSyncBG() == 0) {
        LOADTEXTURE_INFO2 textures[] = {
            {.name = "#frame_image#640#" SCREEN_HEIGHT_STR "#4", .block_no = EastKing.texture_block, .mipmap = 0},
            {.name = nullptr,                                    .block_no = EastKing.texture_block, .mipmap = 0},
            {.name = nullptr,                                    .block_no = 0,                      .mipmap = 0},
        };
        BG_READ_INFO *archive = GetReadBGFile(0);
        auto         *pack = reinterpret_cast<u_int *>(archive->buffer);
        textures[1].name = reinterpret_cast<char *>(GetPackFile(pack, "st.img", nullptr));
        TexManager.DeleteTextureBlock(EastKing.texture_block);
        TexManager.CleanUpTextureList();
        TexManager.LoadTextureBlockEX(-1, textures);

        u_long128 *buffer_end = EastKing.load_buffer + (archive->size >> 4) + 1;
        auto      *messages = reinterpret_cast<s16 *>(GetPackFile(pack, "eastking.bin", nullptr));
        ResetEastKingMessage();
        EastKingMsgCls.SetMesFukidashi(MES_FUKIDASHI_LARGE_TABLE_EDGE);
        EastKingMsgCls.char_width = kFontWidths[GetMenuLangFlag()];
        EastKingMsgCls.char_height = 0x1A;
        EastKingMsgCls.columns = 0x1E;
        EastKingMsgCls.text_rate = 0.0f;
        EastKingMsgCls.text_rate_set = 0.0f;
        EastKingMsgCls.centre_rows = true;
        EastKingMsgCls.tex_block = 0x1A;
        EastKingMsgCls.tex_buff = buffer_end;
        EastKingMsgCls.SetBuff(messages);
        EastKingMsgCls.mes_made = -1;
        EastKingMsgCls.unk_02C = 0x10;
        EastKingMsgCls.init_030 = 0x10;
        EastKingMsgCls.page_arrow = true;
        EastKing.resources_ready = true;
        EastKingMsg.draw_message = true;
        SndBgmPlay(0);
    }
}

void GetPrevEastKingSndVol() {
    PrevEastKingSndVol = SndGetBgmVol();
}

int InitEastKingEvent(int event_no, int *texture_block, u_long128 *load_buffer) {
    std::array<char, 76> lang_path{};
    int                  size = 0;

    GetPathReadDifferntLang(lang_path.data());
    std::string path = std::format("{}eastk/st{}.pak", lang_path.data(), event_no + 1);
    EastKing.load_buffer = MenuCalcBufAlignment(load_buffer);
    StartReadBG();
    LoadFileBG(path.data(), EastKing.load_buffer, &size);
    ReadBG();
    EastKingSndReadBuf = reinterpret_cast<u_int *>(MenuCalcBufAlignment(EastKing.load_buffer + size / 16 + 0x10));
    PrevEastKingSndNo = SndGetBgmNo();
    SndBgmLoadBG(0x16, EastKingSndReadBuf, &size);
    EastKing.resources_ready = false;
    EastKing.texture_block = *texture_block;
    EastKing.event_no = event_no;
    EastKing.mode = EastKingMode::Loading;
    EastKing.transition_frame = 0;
    EastKingMsg.message_no = FirstEventMessage();
    EastKingMsg.unk_02 = 0;
    EastKingMsg.alpha = 0;
    EastKingMsg.draw_message = false;

    if (SaveData->GetGameFlag(EastKing.event_no + kEventSeenFlag) == 0) {
        SaveData->SetGameFlag(EastKing.event_no + kEventSeenFlag, 1);
    }

    return 1;
}

int EastKingEventKey() {
    int finished = 0;
    int size = 0;

    ReadBG();

    switch (EastKing.mode) {
        case EastKingMode::Loading:
            EastKing.transition_frame++;
            EastKingTextureEnter();

            if (EastKing.transition_frame > kTransitionFrames && EastKingMsg.alpha >= kOpaqueAlpha && EastKing.resources_ready != 0) {
                EastKing.mode = EastKingMode::Dialogue;
                EastKing.transition_frame = 0;
                EastKingMsg.draw_message = true;
            }

            break;
        case EastKingMode::Closing:
            if (EastKing.transition_frame == 0x32) {
                SndBgmStop();
            }

            if (EastKing.transition_frame == 0x3A) {
                StartReadBG();
                SndBgmLoadBG(PrevEastKingSndNo, EastKingSndReadBuf, &size);
            }

            EastKing.transition_frame++;

            if (EastKing.transition_frame > kTransitionFrames && SndBgmSyncBG() == 0) {
                finished = 1;
                SndBgmFadeIn(0x3C, PrevEastKingSndVol, 0);
                SndBgmPlay(0);
            }

            break;
        case EastKingMode::Dialogue:
            if (GamePad.Down(PAD_CROSS) != 0 && EastKingMsgCls.State() == CLSMES_SHOWN) {
                if (EastKingMsg.message_no + 1 > FirstEventMessage() + EastKingMsgMax[EastKing.event_no]) {
                    EastKing.mode = EastKingMode::Closing;
                    EastKing.transition_frame = 0;
                    SndBgmFadeOut(0x5A, 0);
                } else {
                    EastKing.mode = EastKingMode::MessageFadeOut;
                    EastKing.transition_frame = 0;
                    EastKingMsg.draw_message = false;
                }
            } else if (GamePad.Down(PAD_CIRCLE) != 0) {
                EastKing.transition_frame = 0;
                EastKing.mode = EastKingMode::Closing;
            }

            break;
        case EastKingMode::MessageFadeOut:
            EastKing.transition_frame++;

            if (EastKingMsg.alpha <= 0) {
                EastKing.mode = EastKingMode::MessageFadeIn;
                EastKingMsg.alpha = 0;
                EastKingMsg.message_no++;
                EastKing.transition_frame = 0;
                EastKingMsg.draw_message = true;
            }

            break;
        case EastKingMode::MessageFadeIn:
            EastKing.transition_frame++;

            if (EastKingMsg.alpha >= kOpaqueAlpha) {
                EastKing.mode = EastKingMode::Dialogue;
                EastKing.transition_frame = 0;
            }

            break;
    }

    SndStep();
    return finished;
}

void EastKingEventDraw() {
    AllFadeForMenu(kOpaqueAlpha);

    if (EastKing.resources_ready != 0) {
        MenuTextureReload(EastKing.texture_block);
        CTexture *picture = TexManager.GetTexture("st", EastKing.texture_block);

        if (EastKing.event_no == 2 && EastKingMsg.message_no == 0x12F) {
            picture = TexManager.GetTexture("st1", EastKing.texture_block);
        }

        DrawFullSizePicture(picture, 0, 0, kOpaqueAlpha);
        EastKingMsgDraw();
    }

    int fade = 0;

    switch (EastKing.mode) {
        case EastKingMode::Loading:
            fade = kOpaqueAlpha - EastKing.transition_frame * 2;
            break;
        case EastKingMode::Closing:
            fade = EastKing.transition_frame * 2;
            break;
        case EastKingMode::MessageFadeOut:
            if (EastKing.event_no == 2 && EastKingMsg.message_no == 0x12E) {
                fade = EastKing.transition_frame * 3;
            }

            break;
        case EastKingMode::MessageFadeIn:
            if (EastKing.event_no == 2 && EastKingMsg.message_no == 0x12F) {
                fade = kOpaqueAlpha - EastKing.transition_frame * 3;
            }

            break;
        case EastKingMode::Dialogue:
            break;
    }

    AllFadeForMenu(std::clamp(fade, 0, kOpaqueAlpha));
}

} // namespace dc::game
