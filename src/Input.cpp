#include <stddef.h>
#include <string.h>

#include "GameErrorContext.h"
#include "Input.h"

#include "GameWindow.h"
#include "InputManager.h"
#include "Supervisor.h"

// GLOBAL: TH16 0x4df508
JOYCAPSA g_joypad_caps;

// GLOBAL: TH16 0x4a50b0
InputGlobals g_input;

// FUNCTION: TH16 0x401850
i32 get_joypad_capabilities()
{
    JOYINFOEX info;

    info.dwSize = sizeof(JOYINFOEX);
    info.dwFlags = JOY_RETURNALL;
    if (joyGetPosEx(JOYSTICKID1, &info) != JOYERR_NOERROR && joyGetPosEx(JOYSTICKID2, &info) != JOYERR_NOERROR)
    {
        // 使えるパッドが存在しないようです、残念
        g_GameErrorContext.log("\x8eg\x82\xa6\x82\xe9\x83p\x83" "b\x83h\x82\xaa\x91\xb6\x8d\xdd\x82\xb5\x82\xc8\x82\xa2"
                               "\x82\xe6\x82\xa4\x82\xc5\x82\xb7\x81" "A\x8e" "c\x94O\r\n");
        return 1;
    }
    joyGetDevCapsA(JOYSTICKID1, &g_joypad_caps, sizeof(g_joypad_caps));
    return 0;
}

// FUNCTION: TH16 0x4021a0
void clear_all_keydown_states()
{
    BYTE keys[256];

    GetKeyboardState(keys);
    for (i32 i = 0; i < 256; i++)
    {
        keys[i] &= 0x7f;
    }
    SetKeyboardState(keys);
}

// GLOBAL: TH16 0x4dfb50
u8 g_controller_data[0x80];

// get_controller_state's body: the buttons of the first controller, from
// winmm or DirectInput. As an inline helper it is a call graph node of its
// own, which LTCG's stack alignment pass treats as a callee that wants an
// aligned frame: get_controller_state then realigns through ebx with
// ebp-relative locals like the original; written in place it realigns with
// and esp, -8 and esp-relative locals.
static __forceinline u8 *controller_state()
{
    JOYINFOEX info;
    DIJOYSTATE2 js;

    memset(g_controller_data, 0, sizeof(g_controller_data));
    if (!(g_Supervisor.flags & SUPERVISOR_USE_DIRECTINPUT_PAD))
    {
        u32 bits;
        u32 i;

        memset(&info, 0, sizeof(info));
        info.dwSize = sizeof(JOYINFOEX);
        info.dwFlags = JOY_RETURNALL;
        if (joyGetPosEx(JOYSTICKID1, &info) != JOYERR_NOERROR)
        {
            return g_controller_data;
        }
        for (bits = info.dwButtons, i = 0; i < 32; i++, bits >>= 1)
        {
            if (bits & 1)
            {
                g_controller_data[i] = 0x80;
            }
        }
        return g_controller_data;
    }
    else
    {
        HRESULT hr = g_Supervisor.joystick->Poll();
        if (FAILED(hr))
        {
            i32 retries = 0;
            hr = g_Supervisor.joystick->Acquire();
            while (hr == DIERR_INPUTLOST)
            {
                hr = g_Supervisor.joystick->Acquire();
                retries++;
                if (retries >= 400)
                {
                    return g_controller_data;
                }
            }
            return g_controller_data;
        }
        // Checks the Poll result again rather than this one (as in TH06).
        g_Supervisor.joystick->GetDeviceState(sizeof(DIJOYSTATE2), &js);
        if (FAILED(hr))
        {
            return g_controller_data;
        }
        memcpy(g_controller_data, js.rgbButtons, sizeof(js.rgbButtons));
        return g_controller_data;
    }
}

// FUNCTION: TH16 0x401c30
u8 *get_controller_state()
{
    return controller_state();
}

#define KEY_PRESSED(button, key) (keys[key] & 0x80 ? (button) : 0)

// FUNCTION: TH16 0x401d50
u32 Supervisor::read_keyboard_input()
{
    u8 keys[256];
    u32 input = 0;

    if (g_GameWindow.is_app_active)
    {
        if (!(g_Supervisor.flags & SUPERVISOR_USE_DIRECTINPUT_KEYBOARD))
        {
            GetKeyboardState(keys);
            input |= KEY_PRESSED(INPUT_UP, VK_UP);
            input |= KEY_PRESSED(INPUT_DOWN, VK_DOWN);
            input |= KEY_PRESSED(INPUT_LEFT, VK_LEFT);
            input |= KEY_PRESSED(INPUT_RIGHT, VK_RIGHT);
            input |= KEY_PRESSED(INPUT_UP, VK_NUMPAD8);
            input |= KEY_PRESSED(INPUT_DOWN, VK_NUMPAD2);
            input |= KEY_PRESSED(INPUT_LEFT, VK_NUMPAD4);
            input |= KEY_PRESSED(INPUT_RIGHT, VK_NUMPAD6);
            input |= KEY_PRESSED(INPUT_UP | INPUT_LEFT, VK_NUMPAD7);
            input |= KEY_PRESSED(INPUT_UP | INPUT_RIGHT, VK_NUMPAD9);
            input |= KEY_PRESSED(INPUT_DOWN | INPUT_LEFT, VK_NUMPAD1);
            input |= KEY_PRESSED(INPUT_DOWN | INPUT_RIGHT, VK_NUMPAD3);
            input |= KEY_PRESSED(INPUT_SCREENSHOT, VK_HOME);
            input |= KEY_PRESSED(INPUT_SCREENSHOT, 'P');
            input |= KEY_PRESSED(INPUT_SHOT, 'Z');
            input |= KEY_PRESSED(INPUT_BOMB, 'X');
            input |= KEY_PRESSED(INPUT_FOCUS, VK_SHIFT);
            input |= KEY_PRESSED(INPUT_MENU, VK_ESCAPE);
            input |= KEY_PRESSED(INPUT_SKIP, VK_CONTROL);
            input |= KEY_PRESSED(INPUT_SKIP, 'C');
            input |= KEY_PRESSED(INPUT_RELEASE, 'C');
            input |= KEY_PRESSED(INPUT_Q, 'Q');
            input |= KEY_PRESSED(INPUT_S, 'S');
            input |= KEY_PRESSED(INPUT_ENTER, VK_RETURN);
            input |= KEY_PRESSED(INPUT_D, 'D');
            input |= KEY_PRESSED(INPUT_R, 'R');
            input |= KEY_PRESSED(INPUT_F10, VK_F10);
        }
        else
        {
            HRESULT hr = g_Supervisor.keyboard->GetDeviceState(sizeof(keys), keys);
            input = 0;
            if (hr == DIERR_INPUTLOST)
            {
                g_Supervisor.keyboard->Acquire();
            }
            else if (hr != DI_OK)
            {
                g_Supervisor.keyboard->Acquire();
            }
            else
            {
                input |= KEY_PRESSED(INPUT_UP, DIK_UP);
                input |= KEY_PRESSED(INPUT_DOWN, DIK_DOWN);
                input |= KEY_PRESSED(INPUT_LEFT, DIK_LEFT);
                input |= KEY_PRESSED(INPUT_RIGHT, DIK_RIGHT);
                input |= KEY_PRESSED(INPUT_UP, DIK_NUMPAD8);
                input |= KEY_PRESSED(INPUT_DOWN, DIK_NUMPAD2);
                input |= KEY_PRESSED(INPUT_LEFT, DIK_NUMPAD4);
                input |= KEY_PRESSED(INPUT_RIGHT, DIK_NUMPAD6);
                input |= KEY_PRESSED(INPUT_UP | INPUT_LEFT, DIK_NUMPAD7);
                input |= KEY_PRESSED(INPUT_UP | INPUT_RIGHT, DIK_NUMPAD9);
                input |= KEY_PRESSED(INPUT_DOWN | INPUT_LEFT, DIK_NUMPAD1);
                input |= KEY_PRESSED(INPUT_DOWN | INPUT_RIGHT, DIK_NUMPAD3);
                input |= KEY_PRESSED(INPUT_SCREENSHOT, DIK_HOME);
                input |= KEY_PRESSED(INPUT_SCREENSHOT, DIK_P);
                input |= KEY_PRESSED(INPUT_SHOT, DIK_Z);
                input |= KEY_PRESSED(INPUT_BOMB, DIK_X);
                input |= KEY_PRESSED(INPUT_FOCUS, DIK_LSHIFT);
                input |= KEY_PRESSED(INPUT_MENU, DIK_ESCAPE);
                input |= KEY_PRESSED(INPUT_SKIP, DIK_LCONTROL);
                input |= KEY_PRESSED(INPUT_SKIP, DIK_C);
                input |= KEY_PRESSED(INPUT_RELEASE, DIK_C);
                input |= KEY_PRESSED(INPUT_Q, DIK_Q);
                input |= KEY_PRESSED(INPUT_S, DIK_S);
                input |= KEY_PRESSED(INPUT_ENTER, DIK_RETURN);
                input |= KEY_PRESSED(INPUT_D, DIK_D);
                input |= KEY_PRESSED(INPUT_R, DIK_R);
                input |= KEY_PRESSED(INPUT_F10, DIK_F10);
            }
        }
    }
    input = g_Supervisor.read_joypad(input);
    InputManager *hw = (InputManager *)&g_hardware_input;
    hw->prev = hw->cur;
    hw->cur = input;
    hw->detect_holds_and_repeats();
    return input;
}

// FUNCTION: TH16 0x402130
HARNESS_CALLED i32 get_keyboard_state(u8 *keys)
{
    memset(keys, 0, 256);
    if (g_GameWindow.is_app_active)
    {
        if (!(g_Supervisor.flags & SUPERVISOR_USE_DIRECTINPUT_KEYBOARD))
        {
            GetKeyboardState(keys);
            return 2;
        }
        HRESULT hr = g_Supervisor.keyboard->GetDeviceState(256, keys);
        if (hr == DIERR_INPUTLOST || hr != DI_OK)
        {
            g_Supervisor.keyboard->Acquire();
        }
        return 1;
    }
    return 0;
}

// Word indices into g_input of hold[1][0] and hold_total[1][0], the game's
// hold counters.
enum
{
    INPUT_GAME_HOLD_WORD = 0x25,
    INPUT_GAME_HOLD_TOTAL_WORD = 0x65,
};

// The devices' detect_holds_and_repeats for the game's input, on the upper
// half of the hold counters (hold[1] and hold_total[1]). It only matches
// indexing them as words from the start of the object with an unsigned
// index.
// FUNCTION: TH16 0x418650
void InputState::update()
{
    u32 *words = (u32 *)&g_input;
    u32 mask = 1;
    u32 bits = g_InputState.input;
    g_InputState.input_repeat = 0;
    g_InputState.input_held_long = 0;
    for (u32 i = 0; i < 32; i++, bits >>= 1, mask <<= 1)
    {
        if (bits & 1)
        {
            words[INPUT_GAME_HOLD_WORD + i]++;
            words[INPUT_GAME_HOLD_TOTAL_WORD + i]++;
            if (words[INPUT_GAME_HOLD_WORD + i] >= INPUT_HELD_LONG_FRAMES)
            {
                g_InputState.input_held_long |= mask;
            }
            if (words[INPUT_GAME_HOLD_WORD + i] >= INPUT_REPEAT_DELAY)
            {
                g_InputState.input_repeat |= mask;
                words[INPUT_GAME_HOLD_WORD + i] -= INPUT_REPEAT_INTERVAL;
            }
        }
        else
        {
            words[INPUT_GAME_HOLD_WORD + i] = 0;
            words[INPUT_GAME_HOLD_TOTAL_WORD + i] = 0;
        }
    }
    g_InputState.input_rising = (g_InputState.input ^ g_InputState.input_prev) & g_InputState.input;
    g_InputState.input_falling = (g_InputState.input ^ g_InputState.input_prev) & ~g_InputState.input;
}

static_assert(offsetof(InputGlobals, repeat_time) == 0x94, "InputGlobals::repeat_time");
static_assert(offsetof(InputGlobals, state) == 0x194, "InputGlobals::state");
static_assert(offsetof(InputManager, held_long) == 0x22c, "InputManager::held_long");
static_assert(offsetof(InputState, input) == 0x84, "InputState::input");
static_assert(sizeof(InputGlobals) == 0x234, "InputGlobals");
static_assert(offsetof(InputGlobals, hold) + 0x80 == INPUT_GAME_HOLD_WORD * 4, "InputGlobals::hold");
static_assert(offsetof(InputGlobals, hold_total) + 0x80 == INPUT_GAME_HOLD_TOTAL_WORD * 4, "InputGlobals::hold_total");
