#pragma once

#include "common.h"

/**
 * Steps of the title screen, as CProcess::no holds them.
 */
// clang-format off
enum TitleStep {
    TITLE_STEP_FADE_IN           = 0,  /**< Fading in. */
    TITLE_STEP_RETURN_FADE_IN    = 1,  /**< Fading in after the save or option screen. */
    TITLE_STEP_WAIT              = 2,  /**< Waiting. */
    TITLE_STEP_LOGO              = 3,  /**< Logo animation. */
    TITLE_STEP_MENU              = 4,  /**< Choosing from the menu. */
    TITLE_STEP_MENU_BLINK        = 5,  /**< Blinking the chosen row. */
    TITLE_STEP_MENU_FADE_OUT     = 6,  /**< Fading out after a choice. */
    TITLE_STEP_ATTRACT_FADE_OUT  = 7,  /**< Fading out to the attract movie. */
    TITLE_STEP_OPENING_BOOK_INIT = 8,  /**< Starting the opening book. */
    TITLE_STEP_OPENING_BOOK      = 9,  /**< Opening book. */
    TITLE_STEP_SAVE_INIT         = 10, /**< Starting the load screen. */
    TITLE_STEP_SAVE              = 11, /**< Load screen. */
    TITLE_STEP_OPTION_INIT       = 12, /**< Starting the option screen. */
    TITLE_STEP_OPTION            = 13, /**< Option screen. */
    TITLE_STEP_EXIT              = 14, /**< Leaving the title screen. */
};

// clang-format on

class CCamera;
class CCameraFollow;
class CCharacter;
class CFrame;
class CSprite;
class CLogo;
class CCursol;
class CFrameVu1;
class CScFader;

/**
 *          Draws title cinematic scene A.
 *
 * @mangled DrawProcA__Fv
 * @address 0x1DCBD90
 * @size 0x7D4
 * @unknownret
 */
void DrawProcA();

/**
 * @mangled DrawProcB__Fv
 * @address 0x1DCCD20
 * @size 0x4B0
 * @unknownret
 */
void DrawProcB();

/**
 * @mangled DrawProcC__Fv
 * @address 0x1DCDAE0
 * @size 0x37C
 * @unknownret
 */
void DrawProcC();

/**
 * @mangled DrawProcD__Fv
 * @address 0x1DCE480
 * @size 0x218
 * @unknownret
 */
void DrawProcD();

/**
 * @mangled DrawProcE__Fv
 * @address 0x1DCEBE0
 * @size 0x230
 * @unknownret
 */
void DrawProcE();

/**
 * @mangled DrawProcF__Fv
 * @address 0x1DCF4A0
 * @size 0x27C
 * @unknownret
 */
void DrawProcF();

/**
 * @mangled DrawProcG__Fv
 * @address 0x1DCFCF0
 * @size 0x200
 * @unknownret
 */
void DrawProcG();

/**
 * @mangled DrawProcH__Fv
 * @address 0x1DD08E0
 * @size 0x3BC
 * @unknownret
 */
void DrawProcH();

/**
 * @mangled DrawProcI__Fv
 * @address 0x1DD14C0
 * @size 0x29C
 * @unknownret
 */
void DrawProcI();

/**
 * @mangled DrawProcTitle__Fv
 * @address 0x1DD1800
 * @size 0x2A4
 * @unknownret
 */
void DrawProcTitle();

/**
 * @mangled TitleInit__Fi
 * @address 0x1DD1AB0
 * @size 0x76C
 * @unknownret
 */
void TitleInit(int mode);

/**
 * @mangled TitleLoop__Fv
 * @address 0x1DD2220
 * @size 0x9E4
 * @unknownret
 */
int TitleLoop();

/**
 *          Draws the active title cinematic or title-screen scene.
 *
 * @mangled TitleDraw__Fv
 * @address 0x1DD2C10
 * @size 0x11EC
 * @unknownret
 */
void TitleDraw();

/**
 * Camera the title is drawn through.
 */
extern CCamera Camera;

/**
 * Following camera of the title.
 */
extern CCameraFollow FCamera;

/**
 * Cloud model of the title.
 */
extern CCharacter Cloud__2;

/**
 * Logo model of the title.
 */
extern CCharacter Logo;

/**
 * Spark models of the title.
 */
extern CCharacter Spark[];

/**
 * Frame of the third title object.
 */
extern CFrame *ObjectFrame3;

/**
 * Screen fader of the title.
 */
extern CScFader CFade;

/**
 * The step the title screen is on. The symbol is eight bytes; only the first word is ever
 * initialised and the second is never read or written anywhere in the overlay.
 */
class CProcess {
public:
    int no; /**< Step the title screen is on. @see TitleStep. */
    int unk_04;

    CProcess() { no = 0; }
};

/**
 * Step the title screen is on.
 */
extern CProcess CProcess;

/**
 * Sprite drawer of the title.
 */
extern CSprite CSprite;

/**
 * Logo of the title.
 */
extern CLogo CLogo;

/**
 * Cursor of the title menu.
 */
extern CCursol CCursol;
