#pragma once

#include "common.h"

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
     *
     * @mangled Init__8CGamePadFv
     * @address 0x12A610
     * @size 0x214
     */
    void Init();

    /**
     * Reads both controllers and updates derived button states.
     *
     * @mangled UpDate__8CGamePadFv
     * @address 0x12AD70
     * @size 0x3CC
     */
    void UpDate();

    /**
     * Tests whether the first controller presses any requested button.
     *
     * @mangled On__8CGamePadFi
     * @address 0x12B810
     * @size 0x2C
     */
    int On(int mask);

    /**
     * Tests whether the second controller presses any requested button.
     *
     * @mangled On2__8CGamePadFi
     * @address 0x12B840
     * @size 0x2C
     */
    int On2(int mask);

    /**
     * Tests whether the first controller newly presses any requested button.
     *
     * @mangled Down__8CGamePadFi
     * @address 0x12B870
     * @size 0x38
     */
    int Down(int mask);

    /**
     * Tests whether the second controller newly presses any requested button.
     *
     * @mangled Down2__8CGamePadFi
     * @address 0x12B8B0
     * @size 0x38
     */
    int Down2(int mask);

    /**
     * Gets the buttons pressed on the first controller.
     *
     * @mangled GetPadOn__8CGamePadFv
     * @address 0x12B600
     * @size 0x28
     */
    int GetPadOn();

    /**
     * Gets the buttons newly pressed on the first controller.
     *
     * @mangled GetPadDown__8CGamePadFv
     * @address 0x12B630
     * @size 0x30
     */
    int GetPadDown();

    /**
     * Gets the buttons newly released on the first controller.
     *
     * @mangled GetPadUp__8CGamePadFv
     * @address 0x12B660
     * @size 0x30
     */
    int GetPadUp();

    /**
     * Gets the calibrated first-controller left stick horizontal position.
     *
     * @mangled GetLX__8CGamePadFv
     * @address 0x12B310
     * @size 0x24
     */
    int GetLX();

    /**
     * Gets the calibrated first-controller left stick vertical position.
     *
     * @mangled GetLY__8CGamePadFv
     * @address 0x12B340
     * @size 0x24
     */
    int GetLY();

    /**
     * Gets the calibrated first-controller right stick horizontal position.
     *
     * @mangled GetRX__8CGamePadFv
     * @address 0x12B2B0
     * @size 0x24
     */
    int GetRX();

    /**
     * Gets the calibrated first-controller right stick vertical position.
     *
     * @mangled GetRY__8CGamePadFv
     * @address 0x12B2E0
     * @size 0x24
     */
    int GetRY();

    /**
     * Gets the calibrated second-controller left stick horizontal position.
     *
     * @mangled GetLX2__8CGamePadFv
     * @address 0x12B3D0
     * @size 0x24
     */
    int GetLX2();

    /**
     * Gets the calibrated second-controller left stick vertical position.
     *
     * @mangled GetLY2__8CGamePadFv
     * @address 0x12B400
     * @size 0x24
     */
    int GetLY2();

    /**
     * Gets the calibrated second-controller right stick horizontal position.
     *
     * @mangled GetRX2__8CGamePadFv
     * @address 0x12B370
     * @size 0x24
     */
    int GetRX2();

    /**
     * Gets the calibrated second-controller right stick vertical position.
     *
     * @mangled GetRY2__8CGamePadFv
     * @address 0x12B3A0
     * @size 0x24
     */
    int GetRY2();

    /**
     * Combines the button and stick states of both controllers.
     *
     * @mangled AllOn__8CGamePadFv
     * @address 0x12B430
     * @size 0xB4
     */
    int AllOn();

    /**
     * Gets the normalized first-controller right stick horizontal position.
     *
     * @mangled GetRXf__8CGamePadFv
     * @address 0x12B690
     * @size 0x3C
     */
    float GetRXf();

    /**
     * Gets the normalized first-controller right stick vertical position.
     *
     * @mangled GetRYf__8CGamePadFv
     * @address 0x12B6D0
     * @size 0x3C
     */
    float GetRYf();

    /**
     * Gets the normalized first-controller left stick horizontal position.
     *
     * @mangled GetLXf__8CGamePadFv
     * @address 0x12B710
     * @size 0x3C
     */
    float GetLXf();

    /**
     * Gets the normalized first-controller left stick vertical position.
     *
     * @mangled GetLYf__8CGamePadFv
     * @address 0x12B750
     * @size 0x3C
     */
    float GetLYf();

    /**
     * Gets the normalized second-controller left stick horizontal position.
     *
     * @mangled GetLXf2__8CGamePadFv
     * @address 0x12B790
     * @size 0x3C
     */
    float GetLXf2();

    /**
     * Gets the normalized second-controller left stick vertical position.
     *
     * @mangled GetLYf2__8CGamePadFv
     * @address 0x12B7D0
     * @size 0x3C
     */
    float GetLYf2();

    /**
     * Sets the first controller input lock mask.
     *
     * @mangled KeyLock__8CGamePadFi
     * @address 0x12B5E0
     * @size 0xC
     */
    void KeyLock(int mask);

    /**
     * Sets the second controller input lock mask.
     *
     * @mangled KeyLock2__8CGamePadFi
     * @address 0x12B5F0
     * @size 0xC
     */
    void KeyLock2(int mask);

    /**
     * Stops both vibration actuators immediately.
     *
     * @mangled StopVibration__8CGamePadFv
     * @address 0x12B9C0
     * @size 0x5C
     */
    void StopVibration();

    /**
     * Enables or disables controller vibration.
     *
     * @mangled VibrationEnable__8CGamePadFi
     * @address 0x12B9B0
     * @size 0xC
     */
    void VibrationEnable(int enable);

    /**
     * Updates vibration timers and sends actuator values.
     *
     * @mangled Step__8CGamePadFv
     * @address 0x12B140
     * @size 0xEC
     */
    void Step();

    /**
     * Disables automatic repeat for every button.
     *
     * @mangled AutoRepeatOff__8CGamePadFv
     * @address 0x12B8F0
     * @size 0x24
     */
    void AutoRepeatOff();

    /**
     * Disables analog-stick menu direction conversion.
     *
     * @mangled MenuModeOff__8CGamePadFv
     * @address 0x12B930
     * @size 0xC
     */
    void MenuModeOff();

    /**
     * Enables analog-stick menu direction conversion at a threshold.
     *
     * @mangled MenuModeOn__8CGamePadFi
     * @address 0x12B920
     * @size 0xC
     */
    void MenuModeOn(int threshold);

    /**
     * Configures automatic repeat for selected buttons.
     *
     * @mangled SetAutoRepeat__8CGamePadFiii
     * @address 0x12B560
     * @size 0x78
     */
    void SetAutoRepeat(int mask, int initial_delay, int repeat_delay);

    /**
     * Cancels automatic repeat for selected buttons.
     *
     * @mangled CancelAutoRepeat__8CGamePadFi
     * @address 0x12B4F0
     * @size 0x6C
     */
    void CancelAutoRepeat(int mask);

    /**
     * Starts one vibration actuator for a limited time.
     *
     * @mangled SetVibration__8CGamePadFiii
     * @address 0x12B940
     * @size 0x64
     */
    void SetVibration(int motor, int intensity, int duration);
};

STATIC_ASSERT(sizeof(CGamePad) == 0x468);

/** Manages the game controllers. */
extern CGamePad GamePad;

/**
 * Reads buttons and analog values from one controller.
 *
 * @mangled pad_button_read__FP10PAD_STATUSii
 * @address 0x12A830
 * @size 0xC8
 */
int pad_button_read(PAD_STATUS *status, int port, int slot);

/**
 * Advances setup and reads one controller when it is ready.
 *
 * @mangled read_pad__FP10PAD_STATUSii
 * @address 0x12A900
 * @size 0x468
 */
int read_pad(PAD_STATUS *status, int port, int slot);

/**
 * Applies the controller analog-axis dead zone and scale.
 *
 * @mangled AxisCalibration__Fi
 * @address 0x12B230
 * @size 0x80
 */
int AxisCalibration(int axis);
