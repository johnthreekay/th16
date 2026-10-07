// Startup code around the Supervisor: input devices, the executable
// checksum and the resolution dialog.
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <shlobj.h>

#include "FileSystem.h"
#include "GameErrorContext.h"
#include "GameWindow.h"
#include "Supervisor.h"

BOOL CALLBACK enum_game_controllers(LPCDIDEVICEINSTANCEA instance, LPVOID context);
BOOL CALLBACK enum_controller_axes(LPCDIDEVICEOBJECTINSTANCEA object, LPVOID context);
HARNESS_CALLED BOOL __stdcall resolve_shortcut(const char *link_path, char *out, i32 unused);
void read_resolution_dialog();

static_assert(offsetof(Supervisor, joystick_caps) == 0x2c, "Supervisor layout");
static_assert(offsetof(Supervisor, exe_checksum) == 0xa14, "Supervisor layout");

// The controller whose axes enum_controller_axes sets up.
// GLOBAL: TH16 0x4bef3c
i32 g_joystick_index;

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

// dinput.h's ids, defined here rather than taken from dxguid.lib, whose one
// object also defines IID_IDirectSoundNotify (DSUtil.cpp defines that one
// to annotate it). Any reference pulls that object in, and the data formats
// c_dfDIKeyboard and c_dfDIJoystick2 (dinput8.lib) refer to the axis, POV
// and key ids, so all of them are here.
// GLOBAL: TH16 0x48b53c
extern "C" const DIDATAFORMAT c_dfDIKeyboard;
// GLOBAL: TH16 0x48b744
extern "C" const DIDATAFORMAT c_dfDIJoystick2;
// GLOBAL: TH16 0x48b75c
extern "C" const GUID IID_IDirectInput8A = {0xbf798030, 0x483a, 0x4da2, {0xaa, 0x99, 0x5d, 0x64, 0xed, 0x36, 0x97, 0x00}};
// GLOBAL: TH16 0x48b76c
extern "C" const GUID GUID_XAxis = {0xa36d02e0, 0xc9f3, 0x11cf, {0xbf, 0xc7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}};
// GLOBAL: TH16 0x48b77c
extern "C" const GUID GUID_YAxis = {0xa36d02e1, 0xc9f3, 0x11cf, {0xbf, 0xc7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}};
// GLOBAL: TH16 0x48b78c
extern "C" const GUID GUID_ZAxis = {0xa36d02e2, 0xc9f3, 0x11cf, {0xbf, 0xc7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}};
// GLOBAL: TH16 0x48b79c
extern "C" const GUID GUID_RxAxis = {0xa36d02f4, 0xc9f3, 0x11cf, {0xbf, 0xc7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}};
// GLOBAL: TH16 0x48b7ac
extern "C" const GUID GUID_RyAxis = {0xa36d02f5, 0xc9f3, 0x11cf, {0xbf, 0xc7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}};
// GLOBAL: TH16 0x48b7bc
extern "C" const GUID GUID_RzAxis = {0xa36d02e3, 0xc9f3, 0x11cf, {0xbf, 0xc7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}};
// GLOBAL: TH16 0x48b7cc
extern "C" const GUID GUID_Slider = {0xa36d02e4, 0xc9f3, 0x11cf, {0xbf, 0xc7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}};
// GLOBAL: TH16 0x48b7dc
extern "C" const GUID GUID_Key = {0x55728220, 0xd33c, 0x11cf, {0xbf, 0xc7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}};
// GLOBAL: TH16 0x48b7ec
extern "C" const GUID GUID_POV = {0xa36d02f2, 0xc9f3, 0x11cf, {0xbf, 0xc7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}};
// GLOBAL: TH16 0x48b7fc
extern "C" const GUID GUID_SysKeyboard = {0x6f1d2b61, 0xd5a0, 0x11cf, {0xbf, 0xc7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}};

// uuid.lib is not linked; these are the shell GUIDs the original takes
// from it.
// GLOBAL: TH16 0x48b81c
extern const GUID g_IID_IShellLinkA = {0x000214ee, 0, 0, {0xc0, 0, 0, 0, 0, 0, 0, 0x46}};
// GLOBAL: TH16 0x48b82c
extern const GUID g_IID_IPersistFile = {0x0000010b, 0, 0, {0xc0, 0, 0, 0, 0, 0, 0, 0x46}};
// GLOBAL: TH16 0x48b83c
extern const GUID g_CLSID_ShellLink = {0x00021401, 0, 0, {0xc0, 0, 0, 0, 0, 0, 0, 0x46}};
// cguid.h's null GUID, which SoundManager passes when it creates the BGM
// stream; defined here so that it can be annotated.
// GLOBAL: TH16 0x48b84c
extern "C" const GUID GUID_NULL = {0, 0, 0, {0, 0, 0, 0, 0, 0, 0, 0}};

// Created by WinMain so that only one instance runs.
// GLOBAL: TH16 0x4dfb4c
HANDLE g_app_mutex;

// Decides whether the game was started from a shortcut (or from somewhere
// other than its own executable), which the frame pacing and the window
// setup look at. 0 if the single instance mutex exists, -1 otherwise.
// FUNCTION: TH16 0x45bd70
HARNESS_CALLED i32 check_startup_shortcut()
{
    STARTUPINFOA startup = {sizeof(STARTUPINFOA)};
    char exe_path[0x105];
    char title[0x105];
    GetModuleFileNameA(NULL, exe_path, sizeof(exe_path));
    GetConsoleTitleA(title, sizeof(title));
    GetStartupInfoA(&startup);
    if (startup.lpTitle != NULL)
    {
        char *ext = strrchr(startup.lpTitle, '.');
        if (file_exists(startup.lpTitle) && ext != NULL)
        {
            if (_stricmp(ext, ".lnk") == 0)
            {
                do
                {
                    resolve_shortcut(startup.lpTitle, title, 0);
                } while (_stricmp(strrchr(title, '.'), ".lnk") == 0);
            }
            else
            {
                strcpy(title, startup.lpTitle);
            }
            if (strcmp(exe_path, title) != 0)
            {
                g_GameWindow.unk_2c = 1;
            }
        }
        g_Supervisor.flags &= ~0x40;
    }
    else
    {
        g_Supervisor.flags |= 0x40;
    }
    return g_app_mutex != NULL ? 0 : -1;
}

// Resolves a .lnk file to the path it points at.
// FUNCTION: TH16 0x45bff0
HARNESS_CALLED BOOL __stdcall resolve_shortcut(const char *link_path, char *out, i32 unused)
{
    IShellLinkA *link;
    IPersistFile *file;
    WIN32_FIND_DATAA find_data;
    if (out == NULL)
    {
        return FALSE;
    }
    BOOL ok = FALSE;
    CoInitialize(NULL);
    if (SUCCEEDED(CoCreateInstance(g_CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, g_IID_IShellLinkA, (void **)&link)))
    {
        if (SUCCEEDED(link->QueryInterface(g_IID_IPersistFile, (void **)&file)))
        {
            WCHAR *wide_path = new WCHAR[MAX_PATH];
            MultiByteToWideChar(CP_ACP, 0, link_path, -1, wide_path, MAX_PATH);
            if (SUCCEEDED(file->Load(wide_path, STGM_READ)))
            {
                if (SUCCEEDED(link->GetPath(out, MAX_PATH, &find_data, 0)))
                {
                    ok = TRUE;
                }
            }
            delete wide_path;
            file->Release();
        }
        link->Release();
    }
    CoUninitialize();
    return ok;
}

// FUNCTION: TH16 0x45c110
INT_PTR CALLBACK resolution_dialog_proc(HWND dialog, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message)
    {
    case WM_INITDIALOG:
        if (g_Supervisor.config.flags_2c & 0x100)
        {
            SendMessageA(GetDlgItem(dialog, 0xca), BM_SETCHECK, BST_CHECKED, 0);
        }
        switch (g_Supervisor.config.window_size)
        {
        case 0:
            SendMessageA(GetDlgItem(dialog, 0xcb), BM_SETCHECK, BST_CHECKED, 0);
        case 3:
            SendMessageA(GetDlgItem(dialog, 0xcd), BM_SETCHECK, BST_CHECKED, 0);
            break;
        case 1:
            SendMessageA(GetDlgItem(dialog, 0xcb), BM_SETCHECK, BST_CHECKED, 0);
        case 4:
            SendMessageA(GetDlgItem(dialog, 0xce), BM_SETCHECK, BST_CHECKED, 0);
            break;
        case 2:
            SendMessageA(GetDlgItem(dialog, 0xcb), BM_SETCHECK, BST_CHECKED, 0);
        case 5:
            SendMessageA(GetDlgItem(dialog, 0xcf), BM_SETCHECK, BST_CHECKED, 0);
            break;
        }
        g_unk_4d9d1c = (g_unk_4d9d1c & ~0x80) | 0x100;
        return FALSE;
    case WM_COMMAND:
        if (LOWORD(wparam) != 0xd0)
        {
            return FALSE;
        }
        read_resolution_dialog();
        g_unk_4d9d1c &= ~0x180;
        DestroyWindow(g_GameWindow.dialog);
        g_GameWindow.dialog = NULL;
        // Falls through.
    case WM_CLOSE:
        if ((g_unk_4d9d1c & 0x180) == 0x100)
        {
            g_unk_4d9d1c &= ~0x100;
            g_unk_4d9d1c |= 0x80;
        }
        DestroyWindow(g_GameWindow.dialog);
        g_GameWindow.dialog = NULL;
        return TRUE;
    }
    return FALSE;
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

// FUNCTION: TH16 0x45ba80
void Supervisor::reset_render_state()
{
    f32 f;
    g_Supervisor.d3d_device->SetRenderState(D3DRS_ZENABLE, TRUE);
    g_Supervisor.d3d_device->SetRenderState(D3DRS_LIGHTING, FALSE);
    g_Supervisor.d3d_device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    g_Supervisor.d3d_device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    g_Supervisor.d3d_device->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, FALSE);
    g_Supervisor.d3d_device->SetRenderState(D3DRS_SHADEMODE, D3DSHADE_GOURAUD);
    g_Supervisor.d3d_device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    g_Supervisor.d3d_device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    g_Supervisor.d3d_device->SetRenderState(D3DRS_ZFUNC, D3DCMP_ALWAYS);
    g_Supervisor.d3d_device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
    g_Supervisor.d3d_device->SetRenderState(D3DRS_ALPHATESTENABLE, TRUE);
    g_Supervisor.d3d_device->SetRenderState(D3DRS_ALPHAREF, 1);
    g_Supervisor.d3d_device->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL);
    g_Supervisor.d3d_device->SetRenderState(D3DRS_FOGENABLE, TRUE);
    f = 1.0f;
    g_Supervisor.d3d_device->SetRenderState(D3DRS_FOGDENSITY, *(DWORD *)&f);
    g_Supervisor.d3d_device->SetRenderState(D3DRS_FOGTABLEMODE, D3DFOG_NONE);
    g_Supervisor.d3d_device->SetRenderState(D3DRS_FOGVERTEXMODE, D3DFOG_LINEAR);
    g_Supervisor.d3d_device->SetRenderState(D3DRS_FOGCOLOR, 0xffa0a0a0);
    f = 1000.0f;
    g_Supervisor.d3d_device->SetRenderState(D3DRS_FOGSTART, *(DWORD *)&f);
    f = 5000.0f;
    g_Supervisor.d3d_device->SetRenderState(D3DRS_FOGEND, *(DWORD *)&f);
    g_Supervisor.d3d_device->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS, FALSE);
    g_Supervisor.d3d_device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
    g_Supervisor.d3d_device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    g_Supervisor.d3d_device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_TFACTOR);
    g_Supervisor.d3d_device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    g_Supervisor.d3d_device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    g_Supervisor.d3d_device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_TFACTOR);
    g_Supervisor.d3d_device->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2);
    g_Supervisor.d3d_device->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);
    g_Supervisor.d3d_device->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
    g_Supervisor.d3d_device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
    g_Supervisor.d3d_device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    g_Supervisor.d3d_device->SetSamplerState(0, D3DSAMP_ADDRESSW, D3DTADDRESS_CLAMP);
    g_Supervisor.d3d_device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
    g_Supervisor.d3d_device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
    if (g_AnmManager != NULL)
    {
        g_AnmManager->last_blend_mode = ANM_BLEND_FORCE_RESET;
        g_AnmManager->render_cache_184fbb5 = 0xff;
        g_AnmManager->last_vertex_setup = ANM_VERTEX_SETUP_NONE;
        g_AnmManager->last_texture_id = -1;
        g_AnmManager->render_cache_184fbb8 = 0xff;
    }
}
