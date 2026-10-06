#pragma once

#include <windows.h>

#include "types.h"

// The main window (TH06's GameWindow). Partial: ExpHP names only the
// handle (WINDOW) and two performance counter fields further on.
struct GameWindow
{
    HWND window;
    u8 unk_4[0x10 - 0x4];
    // Nonzero while the window has focus; input reads nothing otherwise.
    i32 is_app_active;
};

extern GameWindow g_GameWindow;
