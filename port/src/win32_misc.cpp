// The rest of the Windows API the game touches: the winmm joystick
// functions (Input.cpp's fallback when DirectInput has no pad), COM and the
// shell (resolving the .lnk the game was started from), and the IME switch.
#include <mmsystem.h>
#include <shlobj.h>
#include <windows.h>
#include <winnls32.h>

#include "port_stub.h"

extern "C" {

MMRESULT joyGetPosEx(UINT uJoyID, LPJOYINFOEX pji)
{
    PORT_UNIMPLEMENTED();
    return JOYERR_UNPLUGGED;
}

MMRESULT joyGetDevCapsA(UINT_PTR uJoyID, LPJOYCAPSA pjc, UINT cbjc)
{
    PORT_UNIMPLEMENTED();
    return JOYERR_PARMS;
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

HRESULT SHGetFolderPathA(HWND hwnd, int csidl, HANDLE hToken, DWORD dwFlags, LPSTR pszPath)
{
    PORT_UNIMPLEMENTED();
    return E_FAIL;
}

BOOL SHGetSpecialFolderPathA(HWND hwnd, LPSTR pszPath, int csidl, BOOL fCreate)
{
    PORT_UNIMPLEMENTED();
    return FALSE;
}

// Japanese input method on/off; nothing to do without one.
BOOL WINNLSEnableIME(HWND hwnd, BOOL bFlag)
{
    return TRUE;
}

} // extern "C"
