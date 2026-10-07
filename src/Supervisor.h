#pragma once

#include <string.h>
#include <windows.h>

#include <d3d9.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>

#include "AnmManager.h"
#include "Camera.h"
#include "Thread.h"
#include "types.h"

extern i16 g_pad_mapping[10];

// th16.cfg's version field; a file with another version is reset to the
// defaults.
constexpr u32 CONFIG_VERSION = 0x160002;

// Config::flags. Most are startup options from custom.exe; the game logs
// each one it finds when it loads th16.cfg.
enum ConfigFlags
{
    // Set by the game when the device cannot use A8R8G8B8 textures
    // ("running in reduced color mode").
    CONFIG_REDUCED_COLOR = 1 << 0,
    // Create the device with the reference rasterizer.
    CONFIG_REFERENCE_RASTERIZER = 1 << 1,
    // Never turn fog on.
    CONFIG_NO_FOG = 1 << 2,
    // Read pad and keyboard without DirectInput.
    CONFIG_NO_DIRECTINPUT = 1 << 3,
    // Read the BGM tracks into memory instead of streaming them from
    // thbgm.dat.
    CONFIG_BGM_IN_MEMORY = 1 << 4,
    // Do not wait for vsync (Supervisor::no_vsync).
    CONFIG_NO_VSYNC = 1 << 5,
    // Do not detect the text rendering environment.
    CONFIG_NO_TEXT_ENV_DETECTION = 1 << 6,
    // Show the resolution dialog at startup (its check box; holding Shift
    // shows it too).
    CONFIG_SHOW_STARTUP_DIALOG = 1 << 8,
    // Holding shot for 10 frames or more also holds focus.
    CONFIG_SHOT_HOLD_FOCUS = 1 << 9,
};

// Config::window_size and the window size bits of g_window_flags: three
// resolutions, full screen or windowed.
enum WindowSize
{
    WINDOW_SIZE_FULLSCREEN_640 = 0,
    WINDOW_SIZE_FULLSCREEN_960 = 1,
    WINDOW_SIZE_FULLSCREEN_1280 = 2,
    WINDOW_SIZE_WINDOWED_640 = 3,
    WINDOW_SIZE_WINDOWED_960 = 4,
    WINDOW_SIZE_WINDOWED_1280 = 5,
};

// The game's settings. Everything from version on is th16.cfg (ConfigData,
// GameThread.h, is the same 0x64 bytes as a struct of its own, used to read
// and write the file and in replays). Layout from ExpHP's th-re-data and
// the code.
struct Config
{
    // Not part of the file: the "now loading" effect (an EffectManager UI
    // effect) the title menu starts before a game, which the game thread
    // removes once the stage is up.
    u32 loading_effect_id;
    // CONFIG_VERSION.
    u32 version;
    // The pad button for each game button (g_pad_mapping).
    i16 pad_mapping[10];
    // Analog stick dead zones (DirectInput axis units).
    i16 deadzone_x;
    i16 deadzone_y;
    // 0: 32-bit color, 1: 16-bit; 0xff: not chosen yet (the device setup
    // then picks 32-bit color).
    u8 color_mode;
    // 0: no BGM, 1: WAV (thbgm.dat). Values up to 2 are accepted.
    u8 bgm_mode;
    // Nonzero: sound effects play.
    u8 se_enabled;
    // A WindowSize. AsciiManager picks ascii.anm, ascii_960.anm or
    // ascii_1280.anm by window_size % 3.
    u8 window_size;
    // Frames skipped per drawn frame (FpsCounter counts them as drawn).
    u8 frame_skip;
    // Accepted from 0 to 2 (default 2); the game never reads it.
    u8 unk_25;
    // Percentages from the options menu.
    i8 bgm_volume;
    i8 se_volume;
    // Reset to 0 with the volumes by the options menu's default command;
    // nothing else uses it.
    u8 unk_28;
    // How frames are paced to 60 Hz: 1 sleeps before presenting, 2 (the
    // default) paces by sleeping when nothing is skipped
    // (WINDOW_SLEEP_PACING), 3 presents without waiting for vsync.
    u8 frame_pacing;
    u8 unk_2a[0x2c - 0x2a];
    // ConfigFlags.
    u32 flags;
    // Where the window goes in the windowed sizes (CW_USEDEFAULT until the
    // game saves its position on exit).
    u32 window_x;
    u32 window_y;
    u8 unk_38[0x68 - 0x38];

    // The defaults ConfigData::set_defaults also writes, for the whole
    // struct.
    Config();
};

// A screenshot being saved: Supervisor's 0x43bbd0 copies the back buffer
// and starts write_screenshot (0x43be40) on a thread to save it as a BMP.
struct Screenshot
{
    // Nonzero while the writer thread runs.
    uintptr_t thread;
    BITMAPFILEHEADER file_header;
    BITMAPINFO *info;
    // The bottom-up 24-bit rows to write.
    u8 *bmp_data;
    // A copy of the locked back buffer, and its pitch.
    u8 *pixels;
    i32 pitch;
    char path[MAX_PATH];
};

// Owns the Direct3D/DirectInput objects and global game state. ZUN's name
// for it in older games was MotherInf (per ExpHP). Packed to 4 bytes:
// frame_time is a double at an offset that is not a multiple of 8.
#pragma pack(push, 4)
struct Supervisor
{
    // ExpHP's layout puts d3d, d3d_device and dinput 4 bytes later, but
    // the code says otherwise: GetBackBuffer/SetRenderState go through +0x8
    // and DirectInput8Create writes +0xc.
    u8 unk_0[4];
    IDirect3D9 *d3d;
    IDirect3DDevice9 *d3d_device;
    IDirectInput8A *dinput;
    // The window's screen rectangle, kept to restore its place when it
    // goes back from full screen.
    RECT window_rect;
    IDirectInputDevice8A *keyboard;
    IDirectInputDevice8A *joystick;
    u8 unk_28[0x2c - 0x28];
    DIDEVCAPS joystick_caps;
    // Passed to the BGM streaming thread, which ignores it.
    void *unk_58;
    u8 unk_5c[0xdc - 0x5c];
    // The full-window viewport, set by screen effects before they draw.
    D3DVIEWPORT9 viewport_dc;
    // What the device was created with (BackBufferFormat picks the
    // texture formats).
    D3DPRESENT_PARAMETERS present_params;
    u8 unk_12c[0x19c - 0x12c];
    // The adapter's display mode at startup.
    D3DDISPLAYMODE display_mode;
    // Render targets for the arcade region while it is drawn at the
    // default resolution (the "@R" surfaces), and the back buffer.
    IDirect3DSurface9 *arcade_surface_0;
    IDirect3DSurface9 *arcade_surface_1;
    IDirect3DSurface9 *back_buffer;
    u8 unk_1b8[4];
    // Created in on_registration; drawn by some of the on_draw callbacks.
    AnmVm *vm_1bc;
    AnmVm *vm_1c0;
    AnmVm *vm_1c4;
    AnmVm *vm_1c8;
    u8 unk_1cc[4];
    Config config;
    Camera cameras[4];
    Camera *current_camera;
    i32 current_camera_index;
    i32 gamemode_current;
    i32 gamemode_to_switch_to;
    i32 gamemode_prev;
    i32 unk_6fc;
    // Copied into each stage's replay snapshot (RpyGamestate::flag_290) and
    // read by ECL variable -9927. Set by switch_gamemodes before it starts
    // a game (0 for a replay restart).
    i32 unk_700;
    i32 unk_704;
    u8 unk_708[0x714 - 0x708];
    // Set to 2 after the device is reset (3 by WinMain).
    i32 unk_714;
    u8 unk_718[0x71c - 0x718];
    // Set by load_game_config for config flag 0x20.
    i32 unk_71c;
    // Set to 1 by the Direct3D setup; cleared once it is done.
    i32 unk_720;
    i32 unk_724;
    // text.anm: dialogue text and furigana lines.
    struct AnmLoaded *text_anm;
    u8 unk_72c[0x730 - 0x72c];
    u32 flags;
    // timeGetTime() when on_registration ran; also the RNG seed.
    u32 start_time;
    u8 unk_738[0x73c - 0x738];
    // What the device can do, checked once after it is created.
    D3DCAPS9 caps;
    u8 unk_86c[0x870 - 0x86c];
    Screenshot screenshot;
    ThreadInf thread;
    i32 unk_9b4;
    i32 unk_9b8;
    u8 unk_9bc[0xa0c - 0x9bc];
    i32 fog_enabled;
    i32 zwrite_enabled;
    // Sum of the executable's dwords and its size, from compute_exe_checksum.
    i32 exe_checksum;
    i32 exe_size;
    // th16_<version>.ver, read in on_registration.
    i32 ver_file_size;
    void *ver_file_data;
    struct LoadingThread *loading_thread;
    u8 unk_a28[0xa34 - 0xa28];
    // Seconds the last frame's update and draw took.
    double frame_time;
    D3DCOLOR background_color;

    Supervisor();

    u32 read_joypad(u32 input);

    HRESULT enable_d3d_fog();
    // Called by the frame loop and draw_vm; LTCG inlined it into the layer
    // draw callbacks, which use disable_d3d_fog_inline.
    DECOMP_NOINLINE HRESULT disable_d3d_fog();
    HRESULT disable_d3d_fog_inline();
    HRESULT enable_d3d_fog_inline();
    HRESULT enable_zwrite_inline();
    HRESULT disable_zwrite_inline();
    HRESULT enable_zwrite();
    HRESULT disable_zwrite();
    void swap_transform_matrices(Camera *camera);
    // Reaches the object through g_Supervisor; LTCG dropped this.
    HARNESS_CALLED void release_surfaces();
    void sub_43c630();
    void sub_43c6a0();
    // 0x43bbd0. Copies the back buffer and starts write_screenshot to save
    // it to path. Works on g_Supervisor; returns 1 for an unsupported
    // back buffer format. Its one caller is GameWindow::take_screenshot;
    // LTCG dropped this.
    HARNESS_CALLED int take_screenshot(const char *path);
    // 0x43be40. The screenshot thread: converts and saves g_Supervisor's
    // screenshot.
    static void __cdecl write_screenshot(void *arg);
    // 0x43c050. Loads th16.cfg (the only path passed, which LTCG folds),
    // falling back to the defaults, and writes it back.
    HARNESS_CALLED int load_game_config(const char *path);
    // 0x43cb10. Sets up the four cameras for the window size.
    void setup_cameras();

    int switch_gamemodes();
    // 0x43b660. Frees everything on exit.
    int teardown_everything();
    void setup_special_anms();
    // Members that do not use this; LTCG dropped it.
    // 0x401d50. Reads keyboard and pad into g_hardware_input and returns
    // the buttons held.
    static u32 read_keyboard_input();
    // 0x45ba80. Sets every render state the game relies on (after a
    // device reset, too).
    static void reset_render_state();
    int initialize();
    // Creates the DirectInput keyboard and the first game controller.
    i32 dx_direct_input_initialize();
    // Sets up DirectInput and picks the input paths it made available.
    static void init_input();
    // Checksums th16.exe into exe_checksum; -1 if it cannot be read.
    static i32 compute_exe_checksum();
    // Runs a loader function on `thread`. Every caller passes NULL for arg,
    // which LTCG folds; the loaders themselves are plain void functions.
    HARNESS_CALLED i32 start_thread(ThreadStart start, void *arg);
    HARNESS_CALLED i32 play_bgm_wav(i32 arg, const char *name);
    HARNESS_CALLED i32 play_bgm(i32 arg, i32 track);
    HARNESS_CALLED i32 stop_bgm();
    HARNESS_CALLED i32 fade_out_bgm(f32 seconds);
    // 0x43b480. Opens th16.dat and reads the version file from it, for
    // on_registration.
    static i32 open_data_files();
    // 0x43b950. Deletes the game, menu, loading, ending, replay, effect and
    // manual objects.
    static void destroy_game_objects();
    // 0x43d8b0. A text.anm effect VM (script 0x3b at the one call site,
    // which LTCG folds) with vertices for count * 2 points as its extra
    // data; render mode 12 when count > 2. Fog's initialize uses it.
    HARNESS_CALLED AnmId create_fog_vm(i32 count, i32 script);

    static int __fastcall on_tick(void *arg);
    static int __fastcall on_registration(void *arg);
    static int __fastcall on_draw_01(void *arg);
    static int __fastcall on_draw_0e(void *arg);
    static int __fastcall on_draw_0f(void *arg);
    static int __fastcall on_draw_19(void *arg);
    static int __fastcall on_draw_1a(void *arg);
    static int __fastcall on_draw_2b(void *arg);
    static int __fastcall on_draw_2c(void *arg);
    static int __fastcall on_draw_38(void *arg);
    static int __fastcall on_draw_39(void *arg);
    static int __fastcall on_draw_55(void *arg);

    // 0x43dcc0. Called once, when DirectInput setup fails.
    DECOMP_NOINLINE void release_dinput();
};

#pragma pack(pop)

enum SupervisorFlags
{
    // Read the pad through DirectInput rather than joyGetPosEx.
    SUPERVISOR_USE_DIRECTINPUT_PAD = 1 << 11,
    // Read the keyboard through DirectInput rather than GetKeyboardState.
    SUPERVISOR_USE_DIRECTINPUT_KEYBOARD = 1 << 10,
    // Picks the game mode after the ending (2 if set, else 16).
    SUPERVISOR_FLAG_2000 = 1 << 13,
};

extern Supervisor g_Supervisor;

// strcat(path, ".wav") as play_bgm_wav and Gui::start_dialogue have it:
// strlen, then ".wav" stored as one immediate and the terminator from the
// zero the loop ended on. strcat, or strcpy/memcpy of the literal, copy it
// from memory instead.
__forceinline void append_wav_extension(char *path)
{
    char *p = &path[strlen(path)];
    p[0] = '.';
    p[1] = 'w';
    p[2] = 'a';
    p[3] = 'v';
    p[4] = '\0';
}

// enable_d3d_fog, enable_zwrite and disable_zwrite as the stage drawing code
// has them inline.
inline HRESULT Supervisor::enable_d3d_fog_inline()
{
    if (fog_enabled != 1)
    {
        g_AnmManager->flush_sprites();
        fog_enabled = 1;
        return d3d_device->SetRenderState(D3DRS_FOGENABLE, TRUE);
    }
    return 0;
}

inline HRESULT Supervisor::enable_zwrite_inline()
{
    if (zwrite_enabled != 1)
    {
        g_AnmManager->flush_sprites();
        zwrite_enabled = 1;
        return d3d_device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
    }
    return 0;
}

inline HRESULT Supervisor::disable_zwrite_inline()
{
    if (zwrite_enabled != 0)
    {
        g_AnmManager->flush_sprites();
        zwrite_enabled = 0;
        return d3d_device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    }
    return 0;
}

inline HRESULT Supervisor::disable_d3d_fog_inline()
{
    if (fog_enabled != 0)
    {
        g_AnmManager->flush_sprites();
        fog_enabled = 0;
        return d3d_device->SetRenderState(D3DRS_FOGENABLE, FALSE);
    }
    return 0;
}

// Pad button numbers for each game button, -1 if unassigned. Index meaning
// (from read_joypad): 0 shot, 1 bomb, 2 -> 0x8, 3 -> 0x100, 9 -> 0x800.
extern i16 g_pad_mapping[10];

// Screen geometry, rescaled for the chosen window size. Names and notes from
// ExpHP's th-re-data.
extern i32 g_resolution_x;
extern i32 g_resolution_y;
// 1.0, 1.5 or 2.0 depending on the window size.
extern f32 g_screen_coord_scale;
// Size of the arcade region (384x448 unscaled).
extern i32 g_arcade_height;
extern i32 g_arcade_width;
// Half the window width and the scaled top of the arcade region, for HUD
// elements drawn at full resolution (ExpHP: ARCADE_HUD_ORIGIN_X/Y).
extern i32 g_arcade_hud_origin_x;
extern i32 g_arcade_hud_origin_y;
// GameWindow::flags (0x2: device lost; 0x3c: window size; 0x40: frame
// pacing by sleeping), which code outside the window methods addresses as a
// global. setup_cameras makes camera 2 960 pixels high when the bits 0x3c
// are 8.
extern u32 g_unk_4d9d1c;
// Where game coordinate (0, 0) is on the arcade surface.
extern i32 g_game_2d_origin_x;
extern i32 g_game_2d_origin_y;
// Where the arcade region sits on the window-sized "@R" surfaces it is
// drawn to before upscaling: half the scaled window size minus the unscaled
// arcade size (ExpHP: EARLY_RENDERING_ARCADE_OFFSET_X/Y).
extern i32 g_early_arcade_offset_x;
extern i32 g_early_arcade_offset_y;

// The ANM ids Supervisor::sub_43c630/sub_43c6a0 interrupt.
extern AnmId g_anm_ids_4c0f4c[3];
extern i32 g_unk_4a6ef0;
// When set, Supervisor::on_draw_1a calls it instead of drawing.
extern void (*g_draw_hook_4a6ee8)();
// When set, Supervisor::on_draw_0f calls it instead of drawing.
extern void (*g_draw_hook_4a6eec)();
// Set to 3 by switch_gamemodes when it returns to the title screen (mode 16).
extern i32 g_unk_4a6f1c;
// GameWindow's pacing_mode and pacing table (0x4d9d90), as code outside
// the window methods addresses them: as a global of their own, like the
// other GameWindow fields from 0x4d9d1c on. The mode is set once the
// loading screen is done.
struct FramePacingTable
{
    i32 mode;
    struct
    {
        i32 max_sleep_ms;
        i32 sleep_ms;
        i32 late_frames;
    } pacing[4];
};
extern DECOMP_ALIGN16 FramePacingTable g_frame_pacing;
#define g_unk_4d9d90 (g_frame_pacing.mode)
// Counted down once per frame by Supervisor::on_tick.
extern i32 g_unk_4d9d20;
// The game speed multiplier. ECL changes it (slowing down final boss
// deaths), and much code changes it temporarily so that different objects
// see time pass differently.
extern f32 g_game_speed;
