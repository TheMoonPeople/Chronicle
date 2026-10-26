#pragma once

#define scePadStateDiscon 0
#define scePadStateFindPad 1
#define scePadStateFindCTP1 2
#define scePadStateExecCmd 5
#define scePadStateStable 6
#define scePadStateError 7

#define InfoModeCurID 1
#define InfoModeCurExID 2

extern "C" {

/**
 * Initializes the controller library.
 */
int scePadInit(int mode);

/**
 * Opens one controller port and slot.
 */
int scePadPortOpen(int port, int slot, void *buffer);

/**
 * Reads the current data from one controller.
 */
int scePadRead(int port, int slot, unsigned char *data);

/**
 * Gets the connection state of one controller.
 */
int scePadGetState(int port, int slot);

/**
 * Gets mode information from one controller.
 */
int scePadInfoMode(int port, int slot, int info, int index);

/**
 * Selects the main mode of one controller.
 */
int scePadSetMainMode(int port, int slot, int mode, int lock);

/**
 * Gets actuator information from one controller.
 */
int scePadInfoAct(int port, int slot, int actuator, int command);

/**
 * Sets the actuator alignment of one controller.
 */
int scePadSetActAlign(int port, int slot, unsigned char *alignment);

/**
 * Sends actuator values directly to one controller.
 */
int scePadSetActDirect(int port, int slot, unsigned char *values);

}
