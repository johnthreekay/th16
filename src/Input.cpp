#include <string.h>

#include "GameErrorContext.h"
#include "Input.h"

#include "GameWindow.h"
#include "InputManager.h"
#include "Supervisor.h"

// GLOBAL: TH16 0x4df508
JOYCAPSA g_joypad_caps;

// GLOBAL: TH16 0x4a50b0
u32 g_hardware_input;

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

// TODO: the original aligns its frame through ebx (push ebx; mov ebx, esp;
// ... ebp-relative locals); ours aligns with esp-relative locals.
// FUNCTION: TH16 0x401c30
u8 *get_controller_state()
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

#define KEY_PRESSED(button, key) (keys[key] & 0x80 ? (button) : 0)

// TODO: the DirectInput failure branch (Acquire) is placed after the key
// reads instead of before them; everything else matches.
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
            if (hr == DIERR_INPUTLOST || hr != DI_OK)
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
