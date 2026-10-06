#include <string.h>

#include "Supervisor.h"

#include <mmsystem.h>

#include "Input.h"

// GLOBAL: TH16 0x4c10d0
Supervisor g_Supervisor;

// GLOBAL: TH16 0x4a52e4
i16 g_pad_mapping[10] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1};

// FUNCTION: TH16 0x4018e0
u32 Supervisor::read_joypad(u32 input)
{
    if (!(g_Supervisor.flags & SUPERVISOR_USE_DIRECTINPUT_PAD))
    {
        JOYINFOEX info;
        memset(&info, 0, sizeof(info));
        info.dwSize = sizeof(JOYINFOEX);
        info.dwFlags = JOY_RETURNALL;
        if (joyGetPosEx(JOYSTICKID1, &info) != JOYERR_NOERROR)
        {
            return input;
        }
        if (g_pad_mapping[0] >= 0)
        {
            input |= (info.dwButtons & (1 << g_pad_mapping[0])) ? 1 : 0;
        }
        if (g_pad_mapping[1] >= 0)
        {
            input |= (info.dwButtons & (1 << g_pad_mapping[1])) ? 2 : 0;
        }
        if (g_pad_mapping[3] >= 0)
        {
            input |= (info.dwButtons & (1 << g_pad_mapping[3])) ? 0x100 : 0;
        }
        if (g_pad_mapping[2] >= 0)
        {
            input |= (info.dwButtons & (1 << g_pad_mapping[2])) ? 8 : 0;
        }
        if (g_pad_mapping[9] >= 0)
        {
            input |= (info.dwButtons & (1 << g_pad_mapping[9])) ? 0x800 : 0;
        }
        u32 center_x = (g_joypad_caps.wXmin + g_joypad_caps.wXmax) / 2;
        u32 margin_x = (g_joypad_caps.wXmax - g_joypad_caps.wXmin) / 4;
        u32 center_y = (g_joypad_caps.wYmin + g_joypad_caps.wYmax) / 2;
        u32 margin_y = (g_joypad_caps.wYmax - g_joypad_caps.wYmin) / 4;
        input |= (info.dwXpos > center_x + margin_x) ? 0x80 : 0;
        input |= (info.dwXpos < center_x - margin_x) ? 0x40 : 0;
        input |= (info.dwYpos > center_y + margin_y) ? 0x20 : 0;
        input |= (info.dwYpos < center_y - margin_y) ? 0x10 : 0;
        return input;
    }

    if (FAILED(g_Supervisor.joystick->Poll()))
    {
        HRESULT hr = g_Supervisor.joystick->Acquire();
        for (i32 i = 0; hr == DIERR_INPUTLOST && i < 400; i++)
        {
            hr = g_Supervisor.joystick->Acquire();
        }
        return input;
    }

    DIJOYSTATE2 js;
    memset(&js, 0, sizeof(js));
    if (FAILED(g_Supervisor.joystick->GetDeviceState(sizeof(DIJOYSTATE2), &js)))
    {
        return input;
    }
    if (g_pad_mapping[0] >= 0)
    {
        input |= (js.rgbButtons[g_pad_mapping[0]] & 0x80) ? 1 : 0;
    }
    if (g_pad_mapping[1] >= 0)
    {
        input |= (js.rgbButtons[g_pad_mapping[1]] & 0x80) ? 2 : 0;
    }
    if (g_pad_mapping[3] >= 0)
    {
        input |= (js.rgbButtons[g_pad_mapping[3]] & 0x80) ? 0x100 : 0;
    }
    if (g_pad_mapping[2] >= 0)
    {
        input |= (js.rgbButtons[g_pad_mapping[2]] & 0x80) ? 8 : 0;
    }
    if (g_pad_mapping[9] >= 0)
    {
        input |= (js.rgbButtons[g_pad_mapping[9]] & 0x80) ? 0x800 : 0;
    }
    return input | ((js.lX < -g_Supervisor.config.deadzone_x) ? 0x40 : 0) |
           ((js.lY < -g_Supervisor.config.deadzone_y) ? 0x10 : 0) |
           ((js.lX > g_Supervisor.config.deadzone_x) ? 0x80 : 0) |
           ((js.lY > g_Supervisor.config.deadzone_y) ? 0x20 : 0);
}
