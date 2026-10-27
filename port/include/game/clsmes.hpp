#pragma once

#include "common.h"

#include <libpkt.h>

namespace dc::game {

/**
 * What a message window is doing, as ClsMes::State reports it.
 */
// clang-format off
enum ClsMesState {
    CLSMES_CLOSED    = 0, /**< Closed. */
    CLSMES_FADE_IN   = 1, /**< Fading in. */
    CLSMES_REVEALING = 2, /**< Revealing text. */
    CLSMES_SHOWN     = 3, /**< All text shown. */
    CLSMES_FADE_OUT  = 4, /**< Fading out. */
    CLSMES_PAGE_WAIT = 5, /**< Waiting on a page. */
};

// clang-format on

/**
 * Control codes in a message's text.
 */
// clang-format off
enum MesCode {
    MES_CODE_NEWLINE = -0x100, /**< Starts a new line. */
    MES_CODE_END     = -0xFF,  /**< Ends the message. */
    MES_CODE_SPACE   = -0xFE,  /**< A space. */
    MES_CODE_PAGE    = -0xFD,  /**< Waits for input before the next page. */
};

// clang-format on

/**
 * Edge a message window draws around its text, as ClsMes::style holds it.
 */
// clang-format off
enum MesEdgeStyle {
    MES_EDGE_NONE   = 0, /**< No edge. */
    MES_EDGE_WHITE  = 1, /**< One-pixel white edge. */
    MES_EDGE_BLACK  = 2, /**< One-pixel black edge. */
    MES_EDGE_TABLE  = 3, /**< Edge from FuchiTbl_E. */
    MES_EDGE_DOUBLE = 4, /**< Grey and black double edge. */
};

// clang-format on

/**
 * Presets ClsMes::Preset applies.
 */
// clang-format off
enum MesPreset {
    MES_PRESET_FUKIDASHI = 0, /**< Speech bubble. */
    MES_PRESET_SYSTEM    = 1, /**< White text with a black edge. */
    MES_PRESET_AUTO_PAGE = 2, /**< No edge, turning pages by itself. */
    MES_PRESET_NONE      = 3, /**< Changes nothing. */
    MES_PRESET_NAME      = 4, /**< Gold text with a table edge. */
};

// clang-format on

/**
 * Window styles ClsMes::SetMesFukidashi applies.
 */
// clang-format off
enum MesFukidashiStyle {
    MES_FUKIDASHI_BUBBLE            = 0, /**< Speech bubble. */
    MES_FUKIDASHI_NO_BUBBLE         = 1, /**< Bubble removed. */
    MES_FUKIDASHI_SMALL_DOUBLE_EDGE = 2, /**< Small font with a double edge. */
    MES_FUKIDASHI_SMALL_FRAMED      = 3, /**< Small font in a frame with a black edge. */
    MES_FUKIDASHI_LARGE_TABLE_EDGE  = 4, /**< Large font with a table edge. */
    MES_FUKIDASHI_LARGE_BLACK_EDGE  = 5, /**< Large font with a black edge. */
};

// clang-format on

/**
 * Screen positions a message window can take, as ClsMes::auto_pos holds them.
 */
// clang-format off
enum MesAutoPos {
    MES_POS_NONE         = -1, /**< No forced position; a stay window keeps its own placement. */
    MES_POS_AUTO         = 0,  /**< Best position. */
    MES_POS_TOP_LEFT     = 1,  /**< Top left. */
    MES_POS_TOP          = 2,  /**< Top centre. */
    MES_POS_TOP_RIGHT    = 3,  /**< Top right. */
    MES_POS_LEFT         = 4,  /**< Middle left. */
    MES_POS_CENTRE       = 5,  /**< Centre. */
    MES_POS_RIGHT        = 6,  /**< Middle right. */
    MES_POS_BOTTOM_LEFT  = 7,  /**< Bottom left. */
    MES_POS_BOTTOM       = 8,  /**< Bottom centre. */
    MES_POS_BOTTOM_RIGHT = 9,  /**< Bottom right. */
};

// clang-format on

class CCharacter;
class CRect_i_;
class CTexture;

/**
 * One pass of the outline a message window's text draws behind itself.
 */
struct MES_FUCHI {
    s32 x;     /**< How far right of the text this pass draws. */
    s32 y;     /**< How far below the text this pass draws. */
    s32 r;     /**< Red the pass draws with. */
    s32 g;     /**< Green the pass draws with. */
    s32 b;     /**< Blue the pass draws with. */
    s32 alpha; /**< Alpha the pass draws with; zero or less ends the table. */
};

STATIC_ASSERT(sizeof(MES_FUCHI) == 0x18);

/**
 * One laid-out line of a message window.
 */
struct MES_WIN_LINE {
    s16 code;  /**< The laid-out character, or the control code it stands for. */
    s16 x;     /**< Distance of the character from the left of the window. */
    s16 y;     /**< Distance of the character from the top of the window. */
    u8  clut;  /**< Palette entry the character draws with. */
    u8  space; /**< Extra width the character takes, for a justified line. */
};

STATIC_ASSERT(sizeof(MES_WIN_LINE) == 0x8);

/**
 * Characters one message window can lay out.
 */
inline constexpr int MES_WIN_LINE_MAX = 720;

/**
 * Characters one line of a message window holds by default.
 */
inline constexpr int MES_WIN_COLUMNS = 72;

/**
 * Where one line of a message window draws, for the windows that place their
 * lines themselves rather than laying them out in a block.
 */
struct MES_LINE_POS {
    s32 x; /**< Distance of the line from the left of the screen; -1 where it is not set. */
    s32 y; /**< Distance of the line from the top of the screen; -1 where it is not set. */
};

/**
 * A message window: lays a message out, reveals it, and draws it in a frame or speech bubble.
 */
class ClsMes {
public:
    s32          text_x;       /**< Distance of the text from the left of the screen. */
    s32          text_y;       /**< Distance of the text from the top of the screen. */
    s32          char_width;   /**< How wide one character of the font draws. */
    s32          char_height;  /**< How tall one character of the font draws. */
    s32          columns;      /**< Characters one line of the window holds. */
    s32          rows;         /**< Lines the window holds. */
    s32          text_columns; /**< Characters the widest line of the text takes. */
    s32          text_rows;    /**< Lines the text takes. */
    s32          text_len;     /**< Characters the window has laid out to draw. */
    s32          text_width;   /**< How wide the laid-out text is, in pixels. */
    s32          text_height;  /**< How tall the laid-out text is, in pixels. */
    s32          unk_02C;
    s32          init_030;                   /**< Set to 16 by the constructor and never read. */
    s32          fukidashi;                  /**< Whether the window draws as a speech bubble, not a frame. */
    s32          fukidashi_shape;            /**< Which shape the bubble takes, as the text asks for. */
    s32          grow_x;                     /**< Point the window grows out of and shrinks back into, from the left. */
    s32          grow_y;                     /**< Point the window grows out of and shrinks back into, from the top. */
    s32          win_x;                      /**< Distance of the window from the left of the screen. */
    s32          win_y;                      /**< Distance of the window from the top of the screen. */
    s32          win_width;                  /**< How wide the window draws. */
    s32          win_height;                 /**< How tall the window draws. */
    s32          auto_pos;                   /**< Position the window is forced to take. @see MesAutoPos. */
    s32          tail_on;                    /**< Whether the window draws a tail pointing at its speaker. */
    s32          tail_to_x;                  /**< Where the tail points, from the left of the screen. */
    s32          tail_to_y;                  /**< Where the tail points, from the top of the screen. */
    s32          tail_x;                     /**< Where the tail leaves the window, from the left. */
    s32          tail_y;                     /**< Where the tail leaves the window, from the top. */
    s32          tail_half_width;            /**< Half the width of the tail where it meets the window. */
    s32          tail_length;                /**< How far the tail reaches towards what it points at. */
    s32          tail_left_x;                /**< Left corner of the tail's base, worked out by Step. */
    s32          tail_left_y;                /**< Left corner of the tail's base, worked out by Step. */
    s32          tail_right_x;               /**< Right corner of the tail's base, worked out by Step. */
    s32          tail_right_y;               /**< Right corner of the tail's base, worked out by Step. */
    s32          tail_tip_x;                 /**< Point of the tail, worked out by Step. */
    s32          tail_tip_y;                 /**< Point of the tail, worked out by Step. */
    float        fade_speed;                 /**< How much of the fade one step covers. */
    float        fade;                       /**< How far the window has faded in, from zero to one. */
    s32          fade_in;                    /**< Whether the window is fading in rather than out. */
    s32          stay_frame;                 /**< Whether the window draws a plain frame behind its text. */
    s32          stay_width;                 /**< Width that frame takes; zero or less follows the text. */
    s32          stay_height;                /**< Height that frame takes; zero or less follows the text. */
    float        text_rate;                  /**< Characters the text reveals each step, now. */
    float        text_rate_set;              /**< Characters the text reveals each step, as configured. */
    s32          waiting;                    /**< Whether the text has stopped on a wait code. */
    s32          auto_page;                  /**< Whether the window turns its own pages. */
    float        text_at;                    /**< How far through the laid-out characters the text has revealed. */
    s32          text_no;                    /**< The laid-out character the text has revealed up to. */
    s32          text_from;                  /**< The laid-out character the text starts drawing at. */
    s32          page_from;                  /**< The laid-out character the next page begins at. */
    MES_WIN_LINE win_line[MES_WIN_LINE_MAX]; /**< Every line the window lays out. */
    s32          win_line_num;               /**< Lines the window has laid out. */
    s32          clut_default;               /**< Palette entry the text draws with unless a code changes it. */
    s32          clut_now;                   /**< Palette entry the text draws with now. */
    s32          wait;                       /**< Steps left before the text reveals its next character. */
    s32          blink;                      /**< Steps the window has drawn for, which drives what flashes. */
    s32          auto_page_wait;             /**< Steps the window holds a full page before turning it by itself. */
    s32          mes_made;                   /**< The message the window has laid out; negative where it has none. */
    s32          style;                      /**< Which of the window's frames and colours to draw. @see MesEdgeStyle. */
    s32          page_arrow;                 /**< Whether the window draws the mark that says more text follows. */
    s32          end_mark;                   /**< Whether the window always draws that mark, wherever the text is. */
    s32          centre_rows;                /**< Whether the text sits in the middle of the window's rows. */
    s32          page_mark_style;            /**< Whether the window's style asks for the more-text mark; set with the style and never read. */
    s32          edge_alpha;                 /**< How solid the outline behind the text draws. */
    s32          narrow_gaiji_set;           /**< Forces the narrow gaiji page on (2) or off (1); 0 by width. */
    s32          narrow_gaiji;               /**< Whether the editor's external characters draw narrowed. */
    s32          mes_no[10];                 /**< The message each of the window's slots names. */
    s32          values[8];                  /**< The number each of the window's value slots prints. */
    s32          value;                      /**< The number the window prints for a bare value code. */
    s32          value_signed;               /**< Whether a positive value prints with a leading plus. */
    s32          value_show;                 /**< Whether a value prints even when it is zero. */
    s32          value_narrow;               /**< Whether a value's digits draw at three quarters width. */
    s32          space_width;                /**< Width one space takes on a justified line; -1 where it is not. */
    s32          space_area;                 /**< Width a justified line spreads across; -1 where it is not. */
    s32          cursor_row;                 /**< Line the choice cursor stands on; negative where there is none. */
    s32          cursor_y;                   /**< Where the cursor has eased to, from the top of the text. */
    s32          cursor_lit;                 /**< Whether the line the cursor stands on is the one drawn wide. */
    s32          tex_block;                  /**< Texture block the window's font is loaded into. */
    MES_LINE_POS line_pos[10];               /**< Where each line draws, where the window says so. */
    short       *buff;                       /**< The message file the window reads its text out of. */
    short       *buff_system;                /**< The same, for the system messages. */
    char        *text;                       /**< Where the text of that file starts. */
    char        *text_system;                /**< The same, for the system messages. */
    void        *tex_buff;                   /**< Buffer the window builds its text texture in, cleared to start over; null where there is none. */
    u32         *clut;                       /**< The palette the window's font draws out of. */

    /**
     * Gives back how wide one external character of the font draws, in pixels.
     */
    s16 GetGaijiW(int code);

    /**
     * Gives back how many characters one character's registered name has, or
     * -1 where there is no such name.
     */
    int GetNameLen(int chara);

    /**
     * Adds up how wide a character's registered name draws, in pixels.
     */
    int GetNameWidth(int chara);

    /**
     * Reads where two characters stand on screen, so that AutoSet can place
     * the window clear of both of them.
     */
    void AutoSetSub(CCharacter *first, CCharacter *second, int *out_pos);

    /**
     * Puts the window at whichever of nine screen positions lies furthest
     * from the two points @p pos names, and works out where its tail leaves
     * it.
     */
    void AutoSet(int *pos);

    /**
     * Pulls the window back inside the screen's margins, and moves its text
     * along with it.
     */
    void AbsFukidashiIn();

    /**
     * Starts a window with nothing to say and every setting at its default.
     */
    ClsMes();

    /**
     * Gives the window the message file it reads its text out of.
     */
    void SetBuff(short *buff);

    /**
     * The same, for the file the system messages come out of.
     */
    void SetBuff_system(short *buff);

    /**
     * Sets the frame, colours and text speed that one of the window's styles
     * asks for.
     */
    void Preset(int style);

    /**
     * Sets the font size, frame and colours that one of the speech-bubble
     * styles asks for.
     */
    void SetMesFukidashi(int style);

    /**
     * Carries the window's fade on by one step, and works out where its tail
     * stands now.
     */
    void Step();

    /**
     * Gives back what the window is doing: closed, fading in or out, revealing
     * its text, stopped on a page, or done.
     */
    int State();

    /**
     * Forgets which characters of the text the window has already drawn.
     */
    void MyTextureMake_InitAll();

    /**
     * Carries the window on to the page that follows the one it stopped at.
     */
    void GoNextPage();

    /**
     * Steps the window's text on by one laid-out character and gives back
     * what the caller has to do about it.
     */
    int MyTextureMake_sub();

    /**
     * Reveals as much of the text as this step's rate allows.
     */
    void MyTextureMake();

    /**
     * Lays out the number that a bare value code prints, and moves the pen
     * past it.
     */
    void MakeMesWinTbl_value(int *x, int *y);

    /**
     * The same, for the number one of the window's value slots holds.
     */
    void MakeMesWinTbl_value(int which, int *x, int *y);

    /**
     * Lays out the system message that a code names, and gives back whether
     * there was one to lay out.
     */
    int MakeMesWinTbl_system(int code, int *x, int *y);

    /**
     * Gives back how many characters the longest line of one system message
     * has, or -1 where there is no such message.
     */
    int GetMesLen_system(int mes_no);

    /**
     * Gives back how wide the widest line of one system message draws.
     */
    int GetMesWidth_system(int mes_no);

    /**
     * Gives back where one message of the window's file starts.
     */
    short *GetTextLineDataTop(int line);

    /**
     * The same, for the system messages.
     */
    short *GetTextLineDataTop_system(int line);

    /**
     * Empties the window's line table so that a message can be laid out into
     * it.
     */
    void InitMesWinTbl();

    /**
     * Enters one laid-out character into the window's line table, or applies
     * the control code it stands for; gives back whether a line was entered.
     */
    int SetMesWinTbl(int code, int mode, short x, short y);

    /**
     * Works out how wide each space of one justified line has to draw.
     */
    int CalcSpaceW(int width, int glyph_width, short *text);

    /**
     * Lays one message out into the window's line table, and gives back
     * whether it ran as far as a page break.
     */
    int MakeMesWinTbl(int mes_no);

    /**
     * Works out the characters, lines, width and height that a message needs,
     * and writes the largest of each into the four slots of @p out.
     */
    void NeedMesWinWH(int mes_no, int *out);

    /**
     * Opens the window on one message: lays the text out, sizes the frame
     * around it and places it on screen.
     */
    int MakeMesWin(int mes_no);

    /**
     * Lays one message out and works out the frame it needs, unless the
     * window already holds that message.
     */
    void MakeMesTexture(int mes_no);

    /**
     * Draws the outline that stands behind one character of the text.
     */
    void Myset2DSprite_Fuchi(sceVif1Packet *packet, CTexture *texture, int x, int y, int width, int height, int u, int v, int u_width, int v_height);

    /**
     * Writes the points that the speech bubble's outline is drawn from into
     * the packet.
     */
    void MakeFukidashi_sub(sceVif1Packet *packet, int prim);

    /**
     * Draws the speech bubble into the texture that the text is laid over.
     */
    void MakeFukidashi(sceVif1Packet *packet);

    /**
     * Draws the window's frame once, offset by @p dx and @p dy; @p frame asks
     * for the nine-slice frame rather than one plain sprite.
     */
    void DrawMesWin_sub(CTexture *texture, int dx, int dy, int frame);

    /**
     * Draws one external character of the text at the colour its palette
     * entry asks for.
     */
    void DrawGaijiFont(CTexture *texture, int index, const CRect_i_ &texel, const CRect_i_ &screen, int wide, int dark);

    /**
     * Draws the window: its frame, the text revealed so far, and the marks
     * that go alongside them.
     */
    void DrawMesWin();
};

STATIC_ASSERT(sizeof(ClsMes) == 0x1858);

/** The message window the menus share. */
extern ClsMes CommonMenuMes1;

// Port additions.

/**
 * Marks mes as the dialog framework's choice window (dialog_npc.cpp), or none with nullptr. Its rows
 * above first_row are text, not choices: with a cursor_row the window dims every row the cursor is not
 * on, and these keep their colour. A coloured choice the cursor is not on fades rather than brightens,
 * and the hand follows the mouse pointer while one is in use (MenuPointerHand). Retail's windows are
 * never marked, and draw as retail draws them.
 */
void ClsMesSetChoiceRows(const ClsMes *mes, int first_row);

/**
 * Says whether the choice window's choices run on past it, above the first in view or below the last:
 * it then shows the page arrow below, and the page arrow turned over above the first choice, blinking
 * as the page arrow does.
 */
void ClsMesSetChoiceScroll(bool more_above, bool more_below);

/**
 * Lays messages over a window's own: the window looks a message up in file (a message file, as
 * SetBuff takes one) first, then in its own, as a port shop's lines over its shopkeeper's
 * (custom_shop.cpp). One window at a time; a null mes or file ends it.
 */
void ClsMesSetMessageOverlay(const ClsMes *mes, short *file);

} // namespace dc::game
