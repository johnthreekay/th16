// The rest of the Windows API the game touches: the winmm joystick
// functions (Input.cpp's fallback when DirectInput has no controller; they
// read the same controller as DirectInput, see port_input.h), COM and the
// shell (resolving the .lnk the game was started from, and the special
// folders) and the IME switch.
#include <string.h>

#include <mmsystem.h>
#include <shlobj.h>
#include <windows.h>
#include <winnls32.h>

#include "port_input.h"
#include "port_platform.h"
#include "port_stub.h"
#include "port_vfs.h"

extern "C" {

// Axes are 0..65535 (joyGetDevCapsA's range), buttons a bit mask.
MMRESULT joyGetPosEx(UINT uJoyID, LPJOYINFOEX pji)
{
    PortPadState state;
    if (uJoyID != JOYSTICKID1 || pji == NULL || pji->dwSize < sizeof(JOYINFOEX))
    {
        return JOYERR_PARMS;
    }
    if (!port_input_read_pad(&state))
    {
        return JOYERR_UNPLUGGED;
    }
    DWORD *positions[6] = {&pji->dwXpos, &pji->dwYpos, &pji->dwZpos, &pji->dwRpos, &pji->dwUpos, &pji->dwVpos};
    // winmm's order: X, Y, Z, R (rudder: Rz on DirectInput), U, V.
    static const int axis_of[6] = {0, 1, 2, 5, 3, 4};
    for (int i = 0; i < 6; i++)
    {
        *positions[i] = (DWORD)(state.axes[axis_of[i]] + 32768);
    }
    pji->dwButtons = 0;
    pji->dwButtonNumber = 0;
    for (int i = 0; i < state.button_count; i++)
    {
        if (state.buttons[i])
        {
            pji->dwButtons |= 1u << i;
            pji->dwButtonNumber++;
        }
    }
    pji->dwPOV = state.pov == 0xffffffffu ? JOY_POVCENTERED : state.pov;
    return JOYERR_NOERROR;
}

MMRESULT joyGetDevCapsA(UINT_PTR uJoyID, LPJOYCAPSA pjc, UINT cbjc)
{
    PortPadState state;
    if (uJoyID != JOYSTICKID1 || pjc == NULL || cbjc < sizeof(JOYCAPSA))
    {
        return JOYERR_PARMS;
    }
    if (!port_input_read_pad(&state))
    {
        return JOYERR_UNPLUGGED;
    }
    memset(pjc, 0, sizeof(*pjc));
    strncpy(pjc->szPname, port_input_pad_name(), MAXPNAMELEN - 1);
    pjc->wXmax = pjc->wYmax = pjc->wZmax = pjc->wRmax = pjc->wUmax = pjc->wVmax = 65535;
    pjc->wNumButtons = state.button_count;
    pjc->wMaxButtons = 32;
    pjc->wPeriodMin = 10;
    pjc->wPeriodMax = 1000;
    pjc->wMaxAxes = 6;
    for (int i = 0; i < 6; i++)
    {
        pjc->wNumAxes += state.has_axis[i];
    }
    pjc->wCaps = state.has_pov ? 0x10 | 0x20 : 0; // JOYCAPS_HASPOV | JOYCAPS_POV4DIR
    return JOYERR_NOERROR;
}

// COM is only used for IShellLinkA; without it the game treats the start
// as not coming from a shortcut.
HRESULT CoInitialize(void *pvReserved)
{
    return S_OK;
}

void CoUninitialize(void)
{
}

HRESULT CoCreateInstance(REFCLSID rclsid, LPUNKNOWN pUnkOuter, uint32_t dwClsContext, REFIID riid, void **ppv)
{
    if (ppv != NULL)
    {
        *ppv = NULL;
    }
    return E_NOINTERFACE;
}

// Application data folders are the APPDATA of GetEnvironmentVariableA (see
// port_vfs.h); there are no others.
HRESULT SHGetFolderPathA(HWND hwnd, int csidl, HANDLE hToken, DWORD dwFlags, LPSTR pszPath)
{
    switch (csidl & 0xff)
    {
    case CSIDL_APPDATA:
    case CSIDL_LOCAL_APPDATA:
    case CSIDL_PERSONAL:
        strcpy(pszPath, PORT_VFS_APPDATA);
        return S_OK;
    }
    pszPath[0] = '\0';
    return E_FAIL;
}

BOOL SHGetSpecialFolderPathA(HWND hwnd, LPSTR pszPath, int csidl, BOOL fCreate)
{
    return SUCCEEDED(SHGetFolderPathA(hwnd, csidl, NULL, 0, pszPath));
}

// Japanese input method on/off; text input is off in the game window.
BOOL WINNLSEnableIME(HWND hwnd, BOOL bFlag)
{
    return TRUE;
}

} // extern "C"
