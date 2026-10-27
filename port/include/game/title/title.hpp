#pragma once

#include "common.h"

namespace dc::game {

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
 * Loads the attract movie's next scene pack and sets that scene up, as CScript::load_no and
 * init_no ask.
 */
void DataLoad();

/**
 * Draws title cinematic scene A.
 */
void DrawProcA();

/**
 * Draws title cinematic scene B.
 */
void DrawProcB();

/**
 * Draws title cinematic scene C.
 */
void DrawProcC();

/**
 * Draws title cinematic scene D.
 */
void DrawProcD();

/**
 * Draws title cinematic scene E.
 */
void DrawProcE();

/**
 * Draws title cinematic scene F.
 */
void DrawProcF();

/**
 * Draws title cinematic scene G.
 */
void DrawProcG();

/**
 * Draws title cinematic scene H.
 */
void DrawProcH();

/**
 * Draws title cinematic scene I.
 */
void DrawProcI();

/**
 * Draws the title scene that ends the cinematic.
 */
void DrawProcTitle();

/**
 * Sets up the title screen; a mode of 1 starts with the cursor on Load.
 */
void TitleInit(int mode);

/**
 * Runs one frame of the title screen and returns what the game loop moves on to, or 0 to stay.
 */
int TitleLoop();

/**
 * Draws the active title cinematic or title-screen scene.
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
 * The step the title screen is on; the second word is never used.
 */
class CProcess {
public:
    int no; /**< Step the title screen is on. @see TitleStep. */
    int unk_04;

    CProcess() : no(0) {}
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

// Port additions.

/**
 * Constructs the title overlay's objects the port defines with host classes, as the overlay's
 * static constructors did each time retail loaded TITLE.BIN.
 */
void TitleOverlayConstruct();

} // namespace dc::game
