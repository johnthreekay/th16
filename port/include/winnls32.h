// Stand-in for the Windows SDK's winnls32.h: the IME switch WinMain uses.
#pragma once

#include <windows.h>

extern "C" BOOL WINNLSEnableIME(HWND hwnd, BOOL bFlag);
