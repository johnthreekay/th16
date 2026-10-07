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

// GameWindow::flags (g_window_flags).
enum WindowFlags
{
    // Set once Supervisor::initialize has succeeded; the window procedure
    // only switches modes on maximize after that.
    WINDOW_RUNNING = 1 << 0,
    // A switch between full screen and windowed is pending (Alt+Enter,
    // maximize): the frame loop resets the device in the new mode.
    WINDOW_CHANGE_MODE = 1 << 1,
    // The current WindowSize, in bits 2 to 5.
    WINDOW_SIZE_SHIFT = 2,
    WINDOW_SIZE_MASK = 0xf << WINDOW_SIZE_SHIFT,
    // Pace frames by sleeping (frame_pacing 2 without frame skip, at 60 Hz).
    WINDOW_SLEEP_PACING = 1 << 6,
    // The resolution dialog was closed without OK: the game quits.
    WINDOW_DIALOG_CANCELLED = 1 << 7,
    // The resolution dialog is open.
    WINDOW_DIALOG_OPEN = 1 << 8,
};

// The resolution dialog shown at startup (dialog resource IDD_RESOLUTION)
// and its controls.
enum ResolutionDialogIds
{
    IDD_RESOLUTION = 0xcb,
    // Check box: show this dialog every time (CONFIG_SHOW_STARTUP_DIALOG).
    IDC_SHOW_AT_STARTUP = 0xca,
    // Check box: full screen rather than a window.
    IDC_FULL_SCREEN = 0xcb,
    // Radio buttons for the three sizes.
    IDC_SIZE_640 = 0xcd,
    IDC_SIZE_960 = 0xce,
    IDC_SIZE_1280 = 0xcf,
    IDC_OK = 0xd0,
};

// The main window (TH06's GameWindow) and the frame loop state. Window
// methods reach the fields through this; other code addresses them as
// globals. Several of those globals are defined on their own
// (g_window_flags for flags, g_device_reset_frames, g_resolution_x and the rest from
// 0x4d9d2c, g_frame_pacing for pacing_mode and pacing): they are fields of
// this struct, and code that addresses them as globals uses those
// definitions.
#pragma pack(push, 4)
struct GameWindow
{
    HWND window;
    // The resolution dialog while it is open.
    HWND dialog;
    // The frame loop runs while this is 0. Cleared by init_d3d and never
    // set.
    i32 exit_requested;
    HINSTANCE instance;
    // Nonzero while the window has focus; input reads nothing otherwise.
    i32 is_app_active;
    // Show the mouse cursor over the full screen window (set while the game
    // is in the background).
    i32 show_cursor;
    // Counts drawn frames up to the frame skip setting.
    i8 frame_skip_counter;
    u8 unk_19[3];
    // Zero if there is no performance counter (timeGetTime is used then).
    LARGE_INTEGER performance_frequency;
    LARGE_INTEGER initial_performance_counter;
    // Set when the game was started through a shortcut (or by a program)
    // whose target is not this executable; turns vsync waiting off
    // (Supervisor::no_vsync).
    u8 started_by_launcher;
    // %APPDATA%\ShanghaiAlice\th16 and the directory of the executable:
    // where th16.cfg, replays and screenshots go, and where the game runs
    // from (Supervisor::load_game_config switches between them).
    char save_dir[0x1000];
    char exe_dir[0x1000];
    u8 unk_202d[3];
    // SystemParametersInfo values restored on exit.
    i32 screen_save_active;
    i32 low_power_active;
    i32 power_off_active;
    // WindowFlags.
    u32 flags;
    // Set to 10 when the device is reset; counted down by
    // Supervisor::on_tick; never read.
    i32 device_reset_frames;
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
    // WINDOW_SLEEP_PACING the loop paces itself by sleeping; otherwise vsync
    // does, with or without frame skipping.
    HARNESS_CALLED i32 do_frame_sleeping();
    HARNESS_CALLED i32 do_frame();
    HARNESS_CALLED i32 do_frame_frameskip();
    // Present the frame (recovering a lost device), pacing it as the
    // variant needs.
    HARNESS_CALLED void update_window_sleeping();
    HARNESS_CALLED void update_window();
    HARNESS_CALLED void present();
    // Turns off the screen saver and power saving, starts the timers and
    // creates the save directories.
    void make_dirs_and_disable_screensaver();
    // Saves a screenshot when the key is pressed, after serving the ANM
    // screenshot requests.
    HARNESS_CALLED void take_screenshot();
    // Sets the resolution globals from the window size option.
    void set_resolution_from_config();
};

#pragma pack(pop)

extern DECOMP_ALIGN16 GameWindow g_GameWindow;

// GameWindow's fields from flags on (0x4d9d1c to 0x4d9dc4 in the original)
// are what the rest of the game addresses as globals: code outside the
// window methods names them directly. They are the same memory, so these
// names are aliases of the fields, not variables of their own.
#define g_window_flags (g_GameWindow.flags)
#define g_device_reset_frames (g_GameWindow.device_reset_frames)
#define g_resolution_x (g_GameWindow.resolution_x)
#define g_resolution_y (g_GameWindow.resolution_y)
#define g_screen_coord_scale (g_GameWindow.screen_coord_scale)
#define g_early_arcade_offset_x (g_GameWindow.early_arcade_offset_x)
#define g_early_arcade_offset_y (g_GameWindow.early_arcade_offset_y)
#define g_arcade_height (g_GameWindow.arcade_height)
#define g_arcade_width (g_GameWindow.arcade_width)
#define g_arcade_hud_origin_x (g_GameWindow.arcade_hud_origin_x)
#define g_arcade_hud_origin_y (g_GameWindow.arcade_hud_origin_y)
#define g_game_2d_origin_x (g_GameWindow.game_2d_origin_x)
#define g_game_2d_origin_y (g_GameWindow.game_2d_origin_y)

// pacing_mode and the pacing table (0x4d9d90), as code outside the window
// methods addresses them. 16-byte aligned like g_GameWindow (0x20b0 is a
// multiple of 16), so the compiler may use aligned SSE stores.
struct DECOMP_ALIGN16 FramePacingTable
{
    i32 mode;
    struct
    {
        i32 max_sleep_ms;
        i32 sleep_ms;
        i32 late_frames;
    } pacing[4];
};
#define g_frame_pacing (*(FramePacingTable *)&g_GameWindow.pacing_mode)

// Seconds since startup, from the performance counter if there is one.
double LTCG_VECTORCALL get_runtime();
