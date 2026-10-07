// Keyboard and game controller state from SDL (input_sdl.cpp), for the
// three ways the game reads input: DirectInput (dinput_sdl.cpp: DIK_* scan
// codes and DIJOYSTATE2), GetKeyboardState (VK_* virtual keys) and the
// winmm joystick functions (win32_misc.cpp). Main thread only, like the
// game's input code.
#pragma once

#include <stdint.h>

#include <windows.h>

union SDL_Event;

// Keeps the key and controller state up to date; the message pump
// (win32_user.cpp) passes every SDL event here.
void port_input_handle_event(const SDL_Event &event);

// 256 DirectInput key states (index DIK_*), 0x80 while held.
void port_input_get_dik_state(BYTE *keys);
// 256 virtual-key states (index VK_*), 0x80 while held.
void port_input_get_vk_state(BYTE *keys);
// The virtual-key code for an SDL scancode and keycode (0 if none).
int port_input_vk_from_sdl(int scancode, int keycode);
// Forgets every held key (the window lost focus).
void port_input_release_all_keys(void);

// The first game controller (or joystick SDL knows no mapping for).
// Controllers mapped by SDL report the XInput layout DirectInput gives an
// Xbox pad: buttons A, B, X, Y, LB, RB, Back, Start, left stick, right stick;
// axes X/Y left stick, Z triggers (left minus right), Rx/Ry right stick;
// the d-pad as POV 0. The d-pad also moves the X/Y axes to their ends, since
// the game only reads those.
struct PortPadState
{
    // X, Y, Z, Rx, Ry, Rz, slider 0, slider 1: -32768..32767.
    int32_t axes[8];
    // Which of axes[] the device has.
    uint8_t has_axis[8];
    // Hundredths of degrees clockwise from up, or 0xffffffff if centred.
    DWORD pov;
    bool has_pov;
    uint8_t buttons[32];
    int button_count;
};

// Opens the controller if needed and reads it; false if there is none.
bool port_input_read_pad(PortPadState *state);
// The controller's name ("" if none).
const char *port_input_pad_name(void);
