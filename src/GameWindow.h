#pragma once

#include <windows.h>

#include "decomp.h"
#include "types.h"

// Frame pacing statistics, one set per pacing mode (GameWindow::pacing_mode).
struct FramePacing
{
    // Upper bound for sleep_ms.
    i32 max_sleep_ms;
    // How long update_window sleeps after presenting.
    i32 sleep_ms;
    // Frames in a row that came in late.
    i32 late_frames;
};

// The main window (TH06's GameWindow) and the frame loop state. Window
// methods reach the fields through this; other code addresses them as
// globals. Several of those globals are defined on their own (g_unk_4d9d20,
// g_resolution_x and the rest from 0x4d9d2c, g_unk_4d9d90): they are fields
// of this struct.
#pragma pack(push, 4)
struct GameWindow
{
    HWND window;
    // The resolution dialog while it is open.
    HWND dialog;
    i32 unk_8;
    HINSTANCE instance;
    // Nonzero while the window has focus; input reads nothing otherwise.
    i32 is_app_active;
    i32 unk_14;
    // Counts drawn frames up to the frame skip setting.
    i8 frame_skip_counter;
    u8 unk_19[3];
    // Zero if there is no performance counter (timeGetTime is used then).
    LARGE_INTEGER performance_frequency;
    LARGE_INTEGER initial_performance_counter;
    u8 unk_2c;
    // %APPDATA%\ShanghaiAlice\th16 and the directory of the executable.
    char save_dir[0x1000];
    char exe_dir[0x1000];
    u8 unk_202d[3];
    // SystemParametersInfo values restored on exit.
    i32 screen_save_active;
    i32 low_power_active;
    i32 power_off_active;
    // 0x2: device lost; 0x3c: window size; 0x40: frame pacing by sleeping.
    u32 flags;
    i32 unk_2040;
    u8 unk_2044[0x204c - 0x2044];
    i32 resolution_x;
    i32 resolution_y;
    f32 screen_coord_scale;
    i32 early_arcade_offset_x;
    i32 early_arcade_offset_y;
    i32 arcade_height;
    i32 arcade_width;
    i32 arcade_hud_origin_x;
    i32 arcade_hud_origin_y;
    i32 game_2d_origin_x;
    i32 game_2d_origin_y;
    // Times in seconds since startup (ExpHP: TOTAL_APPLICATION_RUNTIME).
    double frame_start_time;
    double last_frame_time;
    double next_frame_time;
    // The smallest time get_runtime has seen; it counts from there.
    double runtime_base;
    double present_time;
    double sleep_start_time;
    u8 unk_20a8[4];
    // Milliseconds to sleep this frame.
    i32 sleep_ms;
    i32 pacing_mode;
    FramePacing pacing[4];

    // The frame loop variants; 0 to keep running, 1 or 2 to quit. With
    // flags & 0x40 the loop paces itself by sleeping; otherwise vsync does,
    // with or without frame skipping.
    HARNESS_CALLED i32 do_frame_sleeping();
    HARNESS_CALLED i32 do_frame();
    HARNESS_CALLED i32 do_frame_frameskip();
    // Present the frame (recovering a lost device), pacing it as the
    // variant needs.
    HARNESS_CALLED void update_window_sleeping();
    HARNESS_CALLED void update_window();
    HARNESS_CALLED void present();
    // Saves a screenshot when the key is pressed, after serving the ANM
    // screenshot requests.
    HARNESS_CALLED void take_screenshot();
    // Sets the resolution globals from the window size option.
    void set_resolution_from_config();
};

#pragma pack(pop)

extern GameWindow g_GameWindow;

// Seconds since startup, from the performance counter if there is one.
double LTCG_VECTORCALL get_runtime();
