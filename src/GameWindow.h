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
    u8 unk_14[0x2d - 0x14];
    // The directory th16.cfg, replays and screenshots go to, and the one
    // the game runs from (Supervisor::load_game_config switches between
    // them).
    char save_dir[0x1000];
    char exe_dir[0x1000];
};

extern GameWindow g_GameWindow;
