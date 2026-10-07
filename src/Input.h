#pragma once

#include <windows.h>

#include <mmsystem.h>

#include "InputManager.h"
#include "decomp.h"
#include "types.h"

extern JOYCAPSA g_joypad_caps;

// Returns 0 if joystick 0 or 1 answers, after reading joystick 0's
// capabilities; 1 (and a log line) if neither does.
i32 get_joypad_capabilities();
void clear_all_keydown_states();

// The game's button state, from the hardware or a replay (ExpHP's INPUT,
// 0x4a52c8): the InputManager logic again, for game code.
struct InputState
{
    // Frames each button has been held.
    i32 hold_time[0x21];
    // InputButton bits held this frame and last frame.
    u32 input;
    u32 input_prev;
    // Buttons auto-repeating this frame (like InputManager::repeat).
    u32 input_repeat;
    // Buttons pressed and released this frame.
    u32 input_rising;
    u32 input_falling;
    u32 unk_98;
    // Buttons held for at least INPUT_HELD_LONG_FRAMES frames.
    u32 input_held_long;

    // Frames the button (an InputButton bit number) has been held, 0 if it
    // is up.
    HARNESS_CALLED i32 get_hold_time(int button);
    // 0x418650. Updates the hold times and edges from input and
    // input_prev. Works on g_InputState.
    static void update();
};

// The input globals from ExpHP's HARDWARE_INPUT (0x4a50b0) on are one
// object: the devices' InputManager, whose hold counters have room for 64
// buttons, and the game's InputState, which overlaps the end of it and
// keeps its counters in the upper 32 (InputState::update addresses them
// from the start of the object).
union InputGlobals
{
    InputManager hardware;
    struct
    {
        u8 unk_0[0x94];
        // Per button: frames until InputState::update repeats it in
        // input_repeat (hardware.hold_frames[0x20 + i]).
        i32 repeat_time[0x20];
    };
    struct
    {
        u8 unk_0_[0x194];
        InputState state;
    };
    // The hold counters by device: [0] the hardware's, [1] the game's.
    struct
    {
        u8 unk_0__[0x14];
        u32 hold[2][0x20];
        u32 hold_total[2][0x20];
    };
};

extern InputGlobals g_input;

// The parts other code names on their own.
// The buttons held this frame, straight from the devices (replays do not
// overwrite it).
#define g_hardware_input (g_input.hardware.cur)
// Hardware buttons auto-repeating and newly pressed this frame.
#define g_hardware_input_repeat (g_input.hardware.repeat)
#define g_hardware_input_pressed (g_input.hardware.rising_edge)
// Frames the shot button has been held (hardware.hold_frames_total[0],
// 0x4a51c4).
#define g_hardware_shot_hold_frames (g_input.hardware.hold_frames_total[0])
#define g_input_repeat_time (g_input.repeat_time)
#define g_InputState (g_input.state)

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
