#pragma once

#include "types.h"

// Button state with edge, hold and auto-repeat tracking. The global
// instance at 0x4a50b0 (ExpHP's HARDWARE_INPUT) holds the raw keyboard and
// pad state.
struct InputManager
{
    u32 cur;
    u32 prev;
    // Buttons auto-repeating this frame (held 26 frames, then every 8).
    u32 repeat;
    u32 rising_edge;
    u32 falling_edge;
    // Frames each button has been held; the first counter drops back while
    // auto-repeating, the second keeps counting.
    u32 hold_frames[0x40];
    u32 hold_frames_total[0x40];
    u8 unk_214[0x22c - 0x214];
    // Buttons held for at least 8 frames.
    u32 held_long;

    void detect_holds_and_repeats();
};
