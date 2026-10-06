#pragma once

#include <windows.h>

#include <mmsystem.h>

#include "types.h"

extern JOYCAPSA g_joypad_caps;

// Returns 0 if joystick 0 or 1 answers, after reading joystick 0's
// capabilities; 1 (and a log line) if neither does.
i32 get_joypad_capabilities();
void clear_all_keydown_states();

// The buttons held this frame, straight from the devices (replays do not
// overwrite it).
extern u32 g_hardware_input;
