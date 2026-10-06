#pragma once

#include <windows.h>

#include <mmsystem.h>

#include "decomp.h"
#include "types.h"

extern JOYCAPSA g_joypad_caps;

// Returns 0 if joystick 0 or 1 answers, after reading joystick 0's
// capabilities; 1 (and a log line) if neither does.
i32 get_joypad_capabilities();
void clear_all_keydown_states();

// The buttons held this frame, straight from the devices (replays do not
// overwrite it).
extern u32 g_hardware_input;

// Button state block around ExpHP's INPUT (0x4a52c8).
struct InputState
{
    // Frames each button has been held.
    i32 hold_time[0x21];
    u32 input;
    u32 input_prev;

    HARNESS_CALLED i32 get_hold_time(int button);
};

extern InputState g_InputState;

// Game buttons in g_hardware_input and the replay input words.
enum InputButton
{
    INPUT_SHOT = 1 << 0,
    INPUT_BOMB = 1 << 1,
    INPUT_FOCUS = 1 << 3,
    INPUT_UP = 1 << 4,
    INPUT_DOWN = 1 << 5,
    INPUT_LEFT = 1 << 6,
    INPUT_RIGHT = 1 << 7,
    INPUT_MENU = 1 << 8,
    INPUT_SKIP = 1 << 9,
    // The season release (C, pad mapping 9).
    INPUT_RELEASE = 1 << 11,
    INPUT_Q = 1 << 16,
    INPUT_S = 1 << 17,
    // Home or P.
    INPUT_SCREENSHOT = 1 << 18,
    INPUT_ENTER = 1 << 19,
    INPUT_D = 1 << 20,
    INPUT_R = 1 << 21,
    INPUT_F10 = 1 << 23,
};

// Joypad buttons as DirectInput reports them (0x80 = held), filled by
// get_controller_state.
extern u8 g_controller_data[0x80];

// 0x401c30. Reads the pad's buttons into g_controller_data and returns it.
u8 *get_controller_state();

// 0x402130. Fills keys with the keyboard state. Returns 0 while the window
// is inactive, 1 after reading through DirectInput, 2 through
// GetKeyboardState.
HARNESS_CALLED i32 get_keyboard_state(u8 *keys);
