// Startup code around the Supervisor: input devices, the executable
// checksum and the resolution dialog.
#include <stddef.h>
#include <stdlib.h>

#include "FileSystem.h"
#include "GameErrorContext.h"
#include "GameWindow.h"
#include "Supervisor.h"

BOOL CALLBACK enum_game_controllers(LPCDIDEVICEINSTANCEA instance, LPVOID context);
BOOL CALLBACK enum_controller_axes(LPCDIDEVICEOBJECTINSTANCEA object, LPVOID context);

static_assert(offsetof(Supervisor, joystick_caps) == 0x2c, "Supervisor layout");
static_assert(offsetof(Supervisor, exe_checksum) == 0xa14, "Supervisor layout");

// The controller whose axes enum_controller_axes sets up.
// GLOBAL: TH16 0x4bef3c
i32 g_joystick_index;

// TODO: the two final stores (exe_size, exe_checksum) are scheduled the other way round.
// FUNCTION: TH16 0x45bef0
i32 Supervisor::compute_exe_checksum()
{
    char path[0x105];
    i32 size;
    if (GetModuleFileNameA(NULL, path, sizeof(path)))
    {
        i32 sum = 0;
        u32 *p = (u32 *)file_read_all(path, &size, 1);
        u32 *data = p;
        if (p == NULL)
        {
            return -1;
        }
#pragma loop(no_vector)
        for (i32 i = 0; i < size / 4 - 1; i++, p++)
        {
            sum += *p;
        }
        free(data);
        g_Supervisor.exe_size = size;
        g_Supervisor.exe_checksum = sum;
        return sum;
    }
    return -1;
}

// Reads the options of the resolution dialog: the 60 fps check box and the
// window size (three sizes, windowed or full screen).
// FUNCTION: TH16 0x45c2a0
void read_resolution_dialog()
{
    if (IsDlgButtonChecked(g_GameWindow.dialog, 0xca) == BST_CHECKED)
    {
        g_Supervisor.config.flags_2c |= 0x100;
    }
    else
    {
        g_Supervisor.config.flags_2c &= ~0x100;
    }
    if (IsDlgButtonChecked(g_GameWindow.dialog, 0xcd) == BST_CHECKED)
    {
        g_Supervisor.config.window_size = IsDlgButtonChecked(g_GameWindow.dialog, 0xcb) == BST_CHECKED ? 0 : 3;
    }
    else if (IsDlgButtonChecked(g_GameWindow.dialog, 0xce) == BST_CHECKED)
    {
        g_Supervisor.config.window_size =
            (IsDlgButtonChecked(g_GameWindow.dialog, 0xcb) == BST_CHECKED ? 0 : 3) + 1;
    }
    else if (IsDlgButtonChecked(g_GameWindow.dialog, 0xcf) == BST_CHECKED)
    {
        g_Supervisor.config.window_size =
            (IsDlgButtonChecked(g_GameWindow.dialog, 0xcb) == BST_CHECKED ? 0 : 3) + 2;
    }
}

// TODO: the first two failures do not share their tail, and the flags test comes before the pushes.
// FUNCTION: TH16 0x45c360
i32 Supervisor::dx_direct_input_initialize()
{
    if (config.flags_2c & 8)
    {
        return -1;
    }
    if (DirectInput8Create(g_GameWindow.instance, DIRECTINPUT_VERSION, IID_IDirectInput8A, (void **)&dinput,
                           NULL) < 0)
    {
        dinput = NULL;
        g_GameErrorContext.log("DirectInput \x82\xaa\x8eg\x97p\x82\xc5\x82\xab\x82\xdc\x82\xb9\x82\xf1\r\n");
        return -1;
    }
    if (dinput->CreateDevice(GUID_SysKeyboard, &keyboard, NULL) < 0)
    {
        if (dinput != NULL)
        {
            dinput->Release();
            dinput = NULL;
        }
        g_GameErrorContext.log("DirectInput \x82\xaa\x8eg\x97p\x82\xc5\x82\xab\x82\xdc\x82\xb9\x82\xf1\r\n");
        return -1;
    }
    if (keyboard->SetDataFormat(&c_dfDIKeyboard) < 0)
    {
        if (keyboard != NULL)
        {
            keyboard->Release();
            keyboard = NULL;
        }
        if (dinput != NULL)
        {
            dinput->Release();
            dinput = NULL;
        }
        g_GameErrorContext.log("DirectInput SetDataFormat \x82\xaa\x8eg\x97p\x82\xc5\x82\xab\x82\xdc\x82\xb9\x82\xf1\r\n");
        return -1;
    }
    if (keyboard->SetCooperativeLevel(g_GameWindow.window, DISCL_NONEXCLUSIVE | DISCL_FOREGROUND | DISCL_NOWINKEY) < 0)
    {
        if (keyboard != NULL)
        {
            keyboard->Release();
            keyboard = NULL;
        }
        release_dinput();
        g_GameErrorContext.log("DirectInput SetCooperativeLevel \x82\xaa\x8eg\x97p\x82\xc5\x82\xab\x82\xdc\x82\xb9\x82\xf1\r\n");
        return -1;
    }
    keyboard->Acquire();
    g_GameErrorContext.log("DirectInput \x82\xcd\x90\xb3\x8f\xed\x82\xc9\x8f\x89\x8a\xfa\x89\xbb\x82\xb3\x82\xea\x82\xdc\x82\xb5\x82\xbd\r\n");
    dinput->EnumDevices(DI8DEVCLASS_GAMECTRL, enum_game_controllers, NULL, DIEDFL_ATTACHEDONLY);
    if (joystick != NULL)
    {
        joystick->SetDataFormat(&c_dfDIJoystick2);
        joystick->SetCooperativeLevel(g_GameWindow.window, DISCL_NONEXCLUSIVE | DISCL_BACKGROUND);
        g_Supervisor.joystick_caps.dwSize = sizeof(DIDEVCAPS);
        joystick->GetCapabilities(&g_Supervisor.joystick_caps);
        g_joystick_index = 0;
        joystick->EnumObjects(enum_controller_axes, NULL, DIDFT_ALL);
        g_GameErrorContext.log("\x97L\x8c\xf8\x82\xc8\x83p\x83" "b\x83h\x82\xf0\x94\xad\x8c\xa9\x82\xb5\x82\xdc\x82\xb5\x82\xbd\r\n");
    }
    return 0;
}

// Opens the first game controller DirectInput reports.
// FUNCTION: TH16 0x45c500
BOOL CALLBACK enum_game_controllers(LPCDIDEVICEINSTANCEA instance, LPVOID context)
{
    for (i32 i = 0; i < 1; i++)
    {
        if ((&g_Supervisor.joystick)[i] == NULL)
        {
            g_Supervisor.dinput->CreateDevice(instance->guidInstance, &(&g_Supervisor.joystick)[i], NULL);
            return DIENUM_CONTINUE;
        }
    }
    return DIENUM_STOP;
}

// Gives every axis of the controller the range -1000 to 1000.
// TODO: the original frame is 4 bytes larger (sub esp, 0x20).
// FUNCTION: TH16 0x45c550
BOOL CALLBACK enum_controller_axes(LPCDIDEVICEOBJECTINSTANCEA object, LPVOID context)
{
    if (object->dwType & DIDFT_AXIS)
    {
        DIPROPRANGE range;
        range.diph.dwSize = sizeof(DIPROPRANGE);
        range.diph.dwHeaderSize = sizeof(DIPROPHEADER);
        range.diph.dwObj = object->dwType;
        range.diph.dwHow = DIPH_BYID;
        range.lMin = -1000;
        range.lMax = 1000;
        if ((&g_Supervisor.joystick)[g_joystick_index]->SetProperty(DIPROP_RANGE, &range.diph) < 0)
        {
            return DIENUM_STOP;
        }
    }
    return DIENUM_CONTINUE;
}

// FUNCTION: TH16 0x45c5e0
void Supervisor::init_input()
{
    g_Supervisor.flags &= ~(SUPERVISOR_USE_DIRECTINPUT_KEYBOARD | SUPERVISOR_USE_DIRECTINPUT_PAD);
    g_Supervisor.dx_direct_input_initialize();
    g_Supervisor.flags =
        (g_Supervisor.flags & ~SUPERVISOR_USE_DIRECTINPUT_KEYBOARD) | ((g_Supervisor.keyboard != NULL) << 10);
    g_Supervisor.flags =
        (g_Supervisor.flags & ~SUPERVISOR_USE_DIRECTINPUT_PAD) | ((g_Supervisor.joystick != NULL) << 11);
}
