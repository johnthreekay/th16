#pragma once

#include "types.h"

// Auto-repeat timing: a button held for INPUT_REPEAT_DELAY frames repeats,
// then again every INPUT_REPEAT_INTERVAL frames. Holding it for
// INPUT_HELD_LONG_FRAMES sets its held_long bit.
enum InputRepeatTiming
{
    INPUT_REPEAT_DELAY = 26,
    INPUT_REPEAT_INTERVAL = 8,
    INPUT_HELD_LONG_FRAMES = 8,
};

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
    // Never used.
    u8 unk_214[0x22c - 0x214];
    // Buttons held for at least 8 frames.
    u32 held_long;

    // Updates the hold counters, repeat, held_long and the edges from cur
    // and prev.
    void detect_holds_and_repeats();
};
