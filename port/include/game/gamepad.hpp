#pragma once

#include "common.h"

namespace dc::game {

/**
 * Steps of a controller's setup, as PAD_STATUS::phase holds them.
 */
// clang-format off
enum PadSetupPhase {
    PAD_PHASE_QUERY          = 0,  /**< Reading the controller type. */
    PAD_PHASE_ANALOG_CHECK   = 40, /**< Checking for analog mode. */
    PAD_PHASE_ANALOG_SET     = 41, /**< Locking analog mode. */
    PAD_PHASE_ANALOG_WAIT    = 42, /**< Waiting for analog mode. */
    PAD_PHASE_ACTUATOR_CHECK = 70, /**< Checking for actuators. */
    PAD_PHASE_ACTUATOR_WAIT  = 71, /**< Waiting for the actuators. */
    PAD_PHASE_READY          = 99, /**< Reading buttons. */
};

// clang-format on

/**
 * Controller types scePadInfoMode reports.
 */
// clang-format off
enum PadTerminalId {
    PAD_TERMINAL_NEGCON          = 2,     /**< NeGcon. */
    PAD_TERMINAL_KONAMI_GUN      = 3,     /**< Konami gun. */
    PAD_TERMINAL_DIGITAL         = 4,     /**< Digital controller. */
    PAD_TERMINAL_ANALOG_JOYSTICK = 5,     /**< Analog joystick. */
    PAD_TERMINAL_NAMCO_GUN       = 6,     /**< Namco gun. */
    PAD_TERMINAL_DUALSHOCK       = 7,     /**< DualShock. */
    PAD_TERMINAL_EX_TSURICON     = 0x100, /**< Fishing controller. */
    PAD_TERMINAL_EX_JOGCON       = 0x300, /**< Jog controller. */
};

// clang-format on

/**
 * Bits of a controller's button word, one per button.
 */
enum PadButton {
    PAD_L2 = 0x0001,       /**< L2 shoulder button. */
    PAD_R2 = 0x0002,       /**< R2 shoulder button. */
    PAD_L1 = 0x0004,       /**< L1 shoulder button. */
    PAD_R1 = 0x0008,       /**< R1 shoulder button. */
    PAD_TRIANGLE = 0x0010, /**< Triangle button. */
    PAD_CIRCLE = 0x0020,   /**< Circle button. */
    PAD_CROSS = 0x0040,    /**< Cross button. */
    PAD_SQUARE = 0x0080,   /**< Square button. */
    PAD_SELECT = 0x0100,   /**< Select button. */
    PAD_L3 = 0x0200,       /**< Left stick press. */
    PAD_R3 = 0x0400,       /**< Right stick press. */
    PAD_START = 0x0800,    /**< Start button. */
    PAD_UP = 0x1000,       /**< Up on the directional pad. */
    PAD_RIGHT = 0x2000,    /**< Right on the directional pad. */
    PAD_DOWN = 0x4000,     /**< Down on the directional pad. */
    PAD_LEFT = 0x8000,     /**< Left on the directional pad. */
    PAD_DPAD = 0xF000,     /**< Any direction on the directional pad. */
};

/**
 * Defines the state of one controller.
 */
struct PAD_STATUS {
    int           button;             /**< Specifies the pressed buttons. */
    int           left_y;             /**< Specifies the left stick vertical position. */
    int           left_x;             /**< Specifies the left stick horizontal position. */
    int           right_y;            /**< Specifies the right stick vertical position. */
    int           right_x;            /**< Specifies the right stick horizontal position. */
    int           phase;              /**< Specifies the controller setup phase. @see PadSetupPhase. */
    int           state;              /**< Specifies the controller connection state. */
    int           extended_id;        /**< Specifies the extended terminal ID the controller reports. */
    int           pad_mode;           /**< Specifies the terminal mode of the latest read. */
    int           previous_pad_mode;  /**< Specifies the terminal mode of the previous successful read. */
    unsigned char vibration[6];       /**< Specifies the actuator strengths. */
    unsigned char actuator[6];        /**< Specifies the actuator alignment. */
    int           vibration_timer[2]; /**< Specifies the remaining actuator times. */
    int           unk_3C[3];
};

STATIC_ASSERT(sizeof(PAD_STATUS) == 0x48);

/**
 * Defines the input and actuator views of one controller port.
 */
union PAD_DATA {
    /**
     * Defines the input view of one controller port.
     */
    struct {
        int        unk_00;
        PAD_STATUS status; /**< Specifies the current controller status. */
    } input;               /**< Provides the controller input view. */

    /**
     * Defines the actuator view of one controller port.
     */
    struct {
        unsigned char unk_00[0x2C];
        unsigned char vibration[6]; /**< Specifies the actuator strengths. */
        unsigned char unk_32[6];
        int           vibration_timer[5]; /**< Specifies the remaining actuator times. */
    } actuator;                           /**< Provides the controller actuator view. */
};

STATIC_ASSERT(sizeof(PAD_DATA) == 0x4C);

/**
 * Defines automatic button repeat state.
 */
struct PAD_REPEAT {
    int enabled;           /**< Specifies buttons with automatic repeat. */
    int active;            /**< Specifies buttons in the repeat phase. */
    int counter[32];       /**< Specifies each button repeat counter. */
    int initial_delay[32]; /**< Specifies each button initial delay. */
    int repeat_delay[32];  /**< Specifies each button repeat delay. */
};

STATIC_ASSERT(sizeof(PAD_REPEAT) == 0x188);

/**
 * Manages both game controllers.
 */
class CGamePad {
    PAD_DATA   pad[2];          /**< Specifies the current controller data. */
    PAD_DATA   previous_pad[2]; /**< Specifies the previous controller data. */
    int        unk_130[5];
    PAD_REPEAT repeat[2];         /**< Specifies automatic repeat state for both controllers. */
    int        axis_threshold[2]; /**< Specifies analog-stick menu thresholds. */
    int        key_lock;          /**< Specifies the first controller input lock. */
    int        key_lock2;         /**< Specifies the second controller input lock. */
    int        vibration_enabled; /**< Specifies whether vibration can run. */

public:
    /**
     * Initializes the controller library and both controller ports.
     */
    void Init();

    /**
     * Reads both controllers and updates derived button states.
     */
    void UpDate();

    /**
     * Tests whether the first controller presses any requested button.
     */
    int On(int mask);

    /**
     * Tests whether the second controller presses any requested button.
     */
    int On2(int mask);

    /**
     * Tests whether the first controller newly presses any requested button.
     */
    int Down(int mask);

    /**
     * Tests whether the second controller newly presses any requested button.
     */
    int Down2(int mask);

    /**
     * Gets the buttons pressed on the first controller.
     */
    int GetPadOn();

    /**
     * Gets the buttons newly pressed on the first controller.
     */
    int GetPadDown();

    /**
     * Gets the buttons newly released on the first controller.
     */
    int GetPadUp();

    /**
     * Gets the calibrated first-controller left stick horizontal position.
     */
    int GetLX();

    /**
     * Gets the calibrated first-controller left stick vertical position.
     */
    int GetLY();

    /**
     * Gets the calibrated first-controller right stick horizontal position.
     */
    int GetRX();

    /**
     * Gets the calibrated first-controller right stick vertical position.
     */
    int GetRY();

    /**
     * Gets the calibrated second-controller left stick horizontal position.
     */
    int GetLX2();

    /**
     * Gets the calibrated second-controller left stick vertical position.
     */
    int GetLY2();

    /**
     * Gets the calibrated second-controller right stick horizontal position.
     */
    int GetRX2();

    /**
     * Gets the calibrated second-controller right stick vertical position.
     */
    int GetRY2();

    /**
     * Combines the button and stick states of both controllers.
     */
    int AllOn();

    /**
     * Gets the normalized first-controller right stick horizontal position.
     */
    float GetRXf();

    /**
     * Gets the normalized first-controller right stick vertical position.
     */
    float GetRYf();

    /**
     * Gets the normalized first-controller left stick horizontal position.
     */
    float GetLXf();

    /**
     * Gets the normalized first-controller left stick vertical position.
     */
    float GetLYf();

    /**
     * Gets the normalized second-controller left stick horizontal position.
     */
    float GetLXf2();

    /**
     * Gets the normalized second-controller left stick vertical position.
     */
    float GetLYf2();

    /**
     * Sets the first controller input lock mask.
     */
    void KeyLock(int mask);

    /**
     * Sets the second controller input lock mask.
     */
    void KeyLock2(int mask);

    /**
     * Stops both vibration actuators immediately.
     */
    void StopVibration();

    /**
     * Enables or disables controller vibration.
     */
    void VibrationEnable(int enable);

    /**
     * Updates vibration timers and sends actuator values.
     */
    void Step();

    /**
     * Disables automatic repeat for every button.
     */
    void AutoRepeatOff();

    /**
     * Disables analog-stick menu direction conversion.
     */
    void MenuModeOff();

    /**
     * Enables analog-stick menu direction conversion at a threshold.
     */
    void MenuModeOn(int threshold);

    /**
     * Configures automatic repeat for selected buttons.
     */
    void SetAutoRepeat(int mask, int initial_delay, int repeat_delay);

    /**
     * Cancels automatic repeat for selected buttons.
     */
    void CancelAutoRepeat(int mask);

    /**
     * Starts one vibration actuator for a limited time.
     */
    void SetVibration(int motor, int intensity, int duration);
};

STATIC_ASSERT(sizeof(CGamePad) == 0x468);

/**
 * Manages the game controllers.
 */
extern CGamePad GamePad;

/**
 * Reads buttons and analog values from one controller.
 */
int pad_button_read(PAD_STATUS *status, int port, int slot);

/**
 * Advances setup and reads one controller when it is ready.
 */
int read_pad(PAD_STATUS *status, int port, int slot);

/**
 * Applies the controller analog-axis dead zone and scale.
 */
int AxisCalibration(int axis);

// Port additions.

/**
 * Sets the buttons CGamePad::Down ignores, until cleared by passing 0.
 */
void PadHoldDown(int mask);

} // namespace dc::game
