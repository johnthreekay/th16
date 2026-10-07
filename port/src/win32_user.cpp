// user32: the window, its message loop, the resolution dialog, the cursor
// and the keyboard state.
//
// Stubs for now; the platform layer maps the window onto an SDL window and
// SDL events onto the messages GameWindow.cpp's window_proc handles
// (WM_ACTIVATEAPP, WM_SYSKEYDOWN, WM_CLOSE, ...). The resolution dialog
// (WinMain: CreateDialogParamA with resource 0xcb and resolution_dialog_proc,
// shown when Shift is held at startup or config flag 0x100 is set) has no
// resource here; an implementation can skip it or answer
// IsDlgButtonChecked from the configuration.
#include <windows.h>

#include "port_stub.h"

extern "C" {

ATOM RegisterClassA(const WNDCLASSA *lpWndClass)
{
    PORT_UNIMPLEMENTED();
    return 0;
}

BOOL UnregisterClassA(LPCSTR lpClassName, HINSTANCE hInstance)
{
    PORT_UNIMPLEMENTED();
    return FALSE;
}

HWND CreateWindowExA(DWORD dwExStyle, LPCSTR lpClassName, LPCSTR lpWindowName, DWORD dwStyle, int X, int Y,
                     int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam)
{
    PORT_UNIMPLEMENTED();
    return NULL;
}

BOOL DestroyWindow(HWND hWnd)
{
    PORT_UNIMPLEMENTED();
    return FALSE;
}

BOOL ShowWindow(HWND hWnd, int nCmdShow)
{
    PORT_UNIMPLEMENTED();
    return FALSE;
}

BOOL UpdateWindow(HWND hWnd)
{
    PORT_UNIMPLEMENTED();
    return FALSE;
}

BOOL MoveWindow(HWND hWnd, int X, int Y, int nWidth, int nHeight, BOOL bRepaint)
{
    PORT_UNIMPLEMENTED();
    return FALSE;
}

BOOL SetWindowPos(HWND hWnd, HWND hWndInsertAfter, int X, int Y, int cx, int cy, UINT uFlags)
{
    PORT_UNIMPLEMENTED();
    return FALSE;
}

LONG SetWindowLongA(HWND hWnd, int nIndex, LONG dwNewLong)
{
    PORT_UNIMPLEMENTED();
    return 0;
}

LONG GetWindowLongA(HWND hWnd, int nIndex)
{
    PORT_UNIMPLEMENTED();
    return 0;
}

BOOL GetWindowRect(HWND hWnd, LPRECT lpRect)
{
    PORT_UNIMPLEMENTED();
    memset(lpRect, 0, sizeof(*lpRect));
    return FALSE;
}

BOOL GetClientRect(HWND hWnd, LPRECT lpRect)
{
    PORT_UNIMPLEMENTED();
    memset(lpRect, 0, sizeof(*lpRect));
    return FALSE;
}

BOOL AdjustWindowRect(LPRECT lpRect, DWORD dwStyle, BOOL bMenu)
{
    PORT_UNIMPLEMENTED();
    return FALSE;
}

BOOL SetForegroundWindow(HWND hWnd)
{
    PORT_UNIMPLEMENTED();
    return FALSE;
}

HWND GetForegroundWindow(void)
{
    PORT_UNIMPLEMENTED();
    return NULL;
}

HWND SetFocus(HWND hWnd)
{
    PORT_UNIMPLEMENTED();
    return NULL;
}

BOOL SetWindowTextA(HWND hWnd, LPCSTR lpString)
{
    PORT_UNIMPLEMENTED();
    return FALSE;
}

LRESULT DefWindowProcA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return 0;
}

LRESULT SendMessageA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    PORT_UNIMPLEMENTED();
    return 0;
}

BOOL PostMessageA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    PORT_UNIMPLEMENTED();
    return FALSE;
}

BOOL PostThreadMessageA(DWORD idThread, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    PORT_UNIMPLEMENTED();
    return FALSE;
}

void PostQuitMessage(int nExitCode)
{
    PORT_UNIMPLEMENTED();
}

BOOL PeekMessageA(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax, UINT wRemoveMsg)
{
    PORT_UNIMPLEMENTED();
    return FALSE;
}

BOOL GetMessageA(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax)
{
    PORT_UNIMPLEMENTED();
    return FALSE;
}

BOOL TranslateMessage(const MSG *lpMsg)
{
    PORT_UNIMPLEMENTED();
    return FALSE;
}

LRESULT DispatchMessageA(const MSG *lpMsg)
{
    PORT_UNIMPLEMENTED();
    return 0;
}

DWORD MsgWaitForMultipleObjects(DWORD nCount, const HANDLE *pHandles, BOOL fWaitAll, DWORD dwMilliseconds,
                                DWORD dwWakeMask)
{
    PORT_UNIMPLEMENTED();
    return WAIT_FAILED;
}

UINT_PTR SetTimer(HWND hWnd, UINT_PTR nIDEvent, UINT uElapse, TIMERPROC lpTimerFunc)
{
    PORT_UNIMPLEMENTED();
    return 0;
}

BOOL KillTimer(HWND hWnd, UINT_PTR uIDEvent)
{
    PORT_UNIMPLEMENTED();
    return FALSE;
}

// The game shows its fatal errors this way (Shift-JIS text).
int MessageBoxA(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption, UINT uType)
{
    PORT_UNIMPLEMENTED();
    fprintf(stderr, "[th16-port] message box: %s: %s\n", lpCaption != NULL ? lpCaption : "",
            lpText != NULL ? lpText : "");
    return IDOK;
}

HWND CreateDialogParamA(HINSTANCE hInstance, LPCSTR lpTemplateName, HWND hWndParent, DLGPROC lpDialogFunc,
                        LPARAM dwInitParam)
{
    PORT_UNIMPLEMENTED();
    return NULL;
}

INT_PTR DialogBoxParamA(HINSTANCE hInstance, LPCSTR lpTemplateName, HWND hWndParent, DLGPROC lpDialogFunc,
                        LPARAM dwInitParam)
{
    PORT_UNIMPLEMENTED();
    return -1;
}

BOOL EndDialog(HWND hDlg, INT_PTR nResult)
{
    PORT_UNIMPLEMENTED();
    return FALSE;
}

BOOL IsDialogMessageA(HWND hDlg, LPMSG lpMsg)
{
    PORT_UNIMPLEMENTED();
    return FALSE;
}

HWND GetDlgItem(HWND hDlg, int nIDDlgItem)
{
    PORT_UNIMPLEMENTED();
    return NULL;
}

UINT IsDlgButtonChecked(HWND hDlg, int nIDButton)
{
    PORT_UNIMPLEMENTED();
    return BST_UNCHECKED;
}

BOOL CheckDlgButton(HWND hDlg, int nIDButton, UINT uCheck)
{
    PORT_UNIMPLEMENTED();
    return FALSE;
}

LRESULT SendDlgItemMessageA(HWND hDlg, int nIDDlgItem, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    PORT_UNIMPLEMENTED();
    return 0;
}

int GetSystemMetrics(int nIndex)
{
    PORT_UNIMPLEMENTED();
    return 0;
}

// Screen saver and power settings, saved and restored around the game.
BOOL SystemParametersInfoA(UINT uiAction, UINT uiParam, PVOID pvParam, UINT fWinIni)
{
    PORT_UNIMPLEMENTED();
    return FALSE;
}

int ShowCursor(BOOL bShow)
{
    PORT_UNIMPLEMENTED();
    return bShow ? 0 : -1;
}

HCURSOR SetCursor(HCURSOR hCursor)
{
    PORT_UNIMPLEMENTED();
    return NULL;
}

HCURSOR LoadCursorA(HINSTANCE hInstance, LPCSTR lpCursorName)
{
    PORT_UNIMPLEMENTED();
    return NULL;
}

HICON LoadIconA(HINSTANCE hInstance, LPCSTR lpIconName)
{
    PORT_UNIMPLEMENTED();
    return NULL;
}

// The game's keyboard fallback when DirectInput is off: 256 virtual-key
// states, bit 7 set while held.
BOOL GetKeyboardState(PBYTE lpKeyState)
{
    PORT_UNIMPLEMENTED();
    memset(lpKeyState, 0, 256);
    return TRUE;
}

BOOL SetKeyboardState(LPBYTE lpKeyState)
{
    PORT_UNIMPLEMENTED();
    return TRUE;
}

short GetAsyncKeyState(int vKey)
{
    PORT_UNIMPLEMENTED();
    return 0;
}

short GetKeyState(int nVirtKey)
{
    PORT_UNIMPLEMENTED();
    return 0;
}

HDC GetDC(HWND hWnd)
{
    PORT_UNIMPLEMENTED();
    return NULL;
}

int ReleaseDC(HWND hWnd, HDC hDC)
{
    PORT_UNIMPLEMENTED();
    return 0;
}

} // extern "C"
