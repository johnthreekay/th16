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
