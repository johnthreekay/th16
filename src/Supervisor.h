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

// A screenshot being saved: Supervisor::take_screenshot copies the back
// buffer and starts write_screenshot on a thread to save it as a BMP.
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
    // The game's module handle, stored by WinMain. ExpHP's layout puts d3d,
    // d3d_device and dinput 4 bytes later, but the code says otherwise:
    // GetBackBuffer/SetRenderState go through +0x8 and DirectInput8Create
    // writes +0xc.
    HINSTANCE instance;
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
    // The game window, stored when it is created. Passed to the BGM
    // streaming thread, which ignores it.
    void *main_window;
    u8 unk_5c[0xdc - 0x5c];
    // The full-window viewport, set by screen effects before they draw.
    D3DVIEWPORT9 full_window_viewport;
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
    // The VMs that copy the arcade picture between the "@R" surfaces and
    // onto the back buffer, each named after the on_draw callback (by its
    // priority) that draws it. Created in on_registration; setup_special_anms
    // starts text.anm scripts 0x3b-0x46 on them, by resolution.
    AnmVm *arcade_blit_vm_0f;
    AnmVm *arcade_blit_vm_1a;
    AnmVm *arcade_blit_vm_2c;
    AnmVm *arcade_blit_vm_39;
    u8 unk_1cc[4];
    Config config;
    Camera cameras[4];
    Camera *current_camera;
    i32 current_camera_index;
    // SupervisorGameMode values. Code asks for a switch by writing
    // gamemode_to_switch_to; switch_gamemodes performs it on the next tick.
    i32 gamemode_current;
    i32 gamemode_to_switch_to;
    i32 gamemode_prev;
    // Cleared by initialize; nothing else uses it.
    i32 unk_6fc;
    // 1 when the game thread starts a new game (from the menu, a restart or
    // a retry), 0 when it moves on to the next stage. Copied into each
    // stage's replay snapshot (RpyGamestate::new_game_started) and read by ECL
    // variable -9927 (with replay_mode == 0).
    i32 new_game_started;
    // 1 after GAMEMODE_RESTART_19, 0 after the other restarts; never read.
    i32 unk_704;
    u8 unk_708[0x714 - 0x708];
    // Set to 2 after the device is reset (3 by WinMain); never read.
    i32 unk_714;
    u8 unk_718[0x71c - 0x718];
    // Do not wait for vsync: presentation is immediate and full screen
    // takes the default refresh rate. Set for CONFIG_NO_VSYNC and when the
    // game was started by another program (GameWindow::started_by_launcher).
    i32 no_vsync;
    // Set to 1 by init_d3d; never read.
    i32 unk_720;
    // Cleared by init_d3d; never read.
    i32 unk_724;
    // text.anm: dialogue text and furigana lines.
    struct AnmLoaded *text_anm;
    u8 unk_72c[0x730 - 0x72c];
    // SupervisorFlags.
    u32 flags;
    // timeGetTime() when on_registration ran; also the RNG seed.
    u32 start_time;
    u8 unk_738[0x73c - 0x738];
    // What the device can do, checked once after it is created.
    D3DCAPS9 caps;
    u8 unk_86c[0x870 - 0x86c];
    Screenshot screenshot;
    ThreadInf thread;
    // Nonzero stops on_tick before it switches game modes (2: it also ends
    // the frame loop). Never set.
    i32 unk_9b4;
    // 1 while the g_stage_load_anm_ids effects run; end_stage_load_anms
    // sets it to 0 and abort_stage_load_anms to 2. Nothing sets it to 1.
    i32 stage_load_anm_state;
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

    // 0x4018e0. ORs the pad's buttons and stick into input (game button
    // bits, through g_pad_mapping), by DirectInput or joyGetPosEx.
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
    // 0x43dc30. Releases the "@R" surfaces and the back buffer before a
    // device reset. Reaches the object through g_Supervisor; LTCG dropped
    // this.
    HARNESS_CALLED void release_surfaces();
    // 0x43c630 and 0x43c6a0. Called when the game thread's stage loading
    // finishes or fails: removes the g_stage_load_anm_ids effects
    // (interrupt 1 or 2) if they run.
    void end_stage_load_anms();
    void abort_stage_load_anms();
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

    // 0x43ce10. Performs a requested game mode switch: creates and deletes
    // the title menu, the game thread and the ending. Returns
    // UPDATE_FUNC_CONTINUE, or UPDATE_FUNC_EXIT_SUCCESS/EXIT_ERROR to quit.
    int switch_gamemodes();
    // 0x43b660. Frees everything on exit.
    int teardown_everything();
    // 0x43d970. Points the "@R" surfaces at text.anm's render target
    // textures and starts the blit VMs, picked by window width.
    void setup_special_anms();
    // Members that do not use this; LTCG dropped it.
    // 0x401d50. Reads keyboard and pad into g_hardware_input and returns
    // the buttons held.
    static u32 read_keyboard_input();
    // 0x45ba80. Sets every render state the game relies on (after a
    // device reset, too).
    static void reset_render_state();
    // 0x43ba40. Registers on_tick and the on_draw callbacks; WinMain calls
    // it once the device exists.
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
    // 0x43c370. Queues name + ".wav" (a thbgm.fmt track) to be loaded into
    // the given preload slot (BGM_LOAD).
    HARNESS_CALLED i32 play_bgm_wav(i32 slot, const char *name);
    // 0x43c3f0. Plays the given preload slot (BGM_PLAY) and unlocks the
    // music room's track.
    HARNESS_CALLED i32 play_bgm(i32 slot, i32 track);
    // 0x43c440
    HARNESS_CALLED i32 stop_bgm();
    // 0x43c470. Fades the BGM out; the time is scaled by a slowed game
    // speed.
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

    // 0x43b3d0. Polls input, runs the sound thread's queue and switches
    // game modes. Priority 1, before every other tick.
    static int __fastcall on_tick(void *arg);
    // 0x43b520. Opens th16.dat, sets up the cameras, timers, RNG seeds,
    // fonts and the arcade blit VMs, and starts loading the sound files.
    static int __fastcall on_registration(void *arg);
    // The frame's render target steps, by draw priority (the surface steps
    // do nothing without the "@R" surfaces): 0x01 clears the frame and
    // targets arcade_surface_0; 0x0e, 0x19 and 0x2b target surface 1, 0 and 1
    // again (0x2b with the arcade region scaled up); 0x0f, 0x1a and 0x2c
    // draw the blit VMs, copying the arcade picture across; 0x38 targets
    // the back buffer and 0x39 draws the final picture onto it; 0x55
    // clears cameras 1 and 3's shake_offset.
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

// Supervisor::flags. The constructor sets 0x4240 (0x40, 0x200, 0x4000);
// 0x200 and 0x4000 are not read anywhere.
enum SupervisorFlags
{
    // The device was created with hardware vertex processing.
    SUPERVISOR_HW_VERTEX_PROCESSING = 1 << 0,
    // Set once init_d3d has picked the presentation parameters.
    SUPERVISOR_D3D_INITIALIZED = 1 << 1,
    // The device can use A8R8G8B8 textures.
    SUPERVISOR_ARGB_TEXTURES = 1 << 2,
    // The device was reset during this frame (the game then pauses).
    SUPERVISOR_DEVICE_WAS_RESET = 1 << 4,
    // STARTUPINFO had no title (not started from a shortcut); not read.
    SUPERVISOR_NO_STARTUP_TITLE = 1 << 6,
    // The window was closed: on_tick quits once no loader thread runs, and
    // loaders give up.
    SUPERVISOR_QUIT_REQUESTED = 1 << 7,
    // Never set. Loaders give up when it is set too, and on_tick only
    // quits when it is clear.
    SUPERVISOR_FLAG_100 = 1 << 8,
    // Read the keyboard through DirectInput rather than GetKeyboardState.
    SUPERVISOR_USE_DIRECTINPUT_KEYBOARD = 1 << 10,
    // Read the pad through DirectInput rather than joyGetPosEx.
    SUPERVISOR_USE_DIRECTINPUT_PAD = 1 << 11,
    // Leaving the title, a game or the ending goes to GAMEMODE_IDLE
    // instead of quitting or returning to the title. Cleared when the
    // loading screen is done and never set: a leftover.
    SUPERVISOR_IDLE_ON_EXIT = 1 << 13,
};

// The text.anm scripts of the arcade blit VMs at 640x480; each one's 960
// and 1280 versions follow it (script + 1, script + 2).
enum ArcadeBlitScript
{
    TEXT_SCRIPT_BLIT_0F = 0x3b,
    TEXT_SCRIPT_BLIT_2C = 0x3e,
    TEXT_SCRIPT_BLIT_1A = 0x41,
    TEXT_SCRIPT_BLIT_39 = 0x44,
};

// Supervisor's game modes (gamemode_current, gamemode_to_switch_to). The
// switches themselves are in Supervisor::switch_gamemodes.
enum SupervisorGameMode
{
    // gamemode_current until the first switch.
    GAMEMODE_NONE = -2,
    // Tested for (with GAMEMODE_QUIT) by GameThread::update_play_time but
    // never set.
    GAMEMODE_INVALID = -1,
    // Starts the loading thread (GAMEMODE_LOADING), or quits if it fails.
    GAMEMODE_STARTUP = 0,
    // The loading screen; LoadingThread switches to GAMEMODE_TITLE.
    GAMEMODE_LOADING = 1,
    // No scene at all (see SUPERVISOR_IDLE_ON_EXIT).
    GAMEMODE_IDLE = 2,
    // Deletes everything and ends the frame loop.
    GAMEMODE_QUIT = 3,
    // The title screen and its menus.
    GAMEMODE_TITLE = 4,
    // A game in progress (also playing a replay): the game thread runs.
    GAMEMODE_GAME = 7,
    // Tested for by Gui but never set.
    GAMEMODE_UNUSED_8 = 8,
    // Restarts the game from the stage it began at (the pause menu's
    // restart, and the game over menu in some cases).
    GAMEMODE_RESTART = 10,
    // The same for a replay.
    GAMEMODE_RESTART_REPLAY = 11,
    // Moves on to the next stage, keeping the game's state.
    GAMEMODE_NEXT_STAGE = 12,
    // Starts a replay from the title (the replay menu or the demo).
    GAMEMODE_START_REPLAY = 13,
    // Continues after a game over in the Extra stage by restarting the
    // stage.
    GAMEMODE_RETRY_STAGE = 14,
    GAMEMODE_ENDING = 15,
    // Back to the title for the high score name entry (after a game over
    // or the ending).
    GAMEMODE_TITLE_SCORE_ENTRY = 16,
    // Deletes everything and ends the frame loop with an error.
    GAMEMODE_QUIT_ERROR = 17,
    // Like GAMEMODE_RESTART, but sets unk_704. Never set.
    GAMEMODE_RESTART_19 = 19,
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

// The game buttons a pad button can be assigned to: indices into
// g_pad_mapping.
enum PadButton
{
    PAD_SHOT = 0,
    PAD_BOMB = 1,
    PAD_FOCUS = 2,
    PAD_PAUSE = 3,
    // The season release.
    PAD_RELEASE = 9,
};

// The pad button number for each PadButton, -1 if unassigned.
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
// GameWindow::flags (WindowFlags), which code outside the window methods
// addresses as a global.
extern u32 g_window_flags;
// Where game coordinate (0, 0) is on the arcade surface.
extern i32 g_game_2d_origin_x;
extern i32 g_game_2d_origin_y;
// Where the arcade region sits on the window-sized "@R" surfaces it is
// drawn to before upscaling: half the scaled window size minus the unscaled
// arcade size (ExpHP: EARLY_RENDERING_ARCADE_OFFSET_X/Y).
extern i32 g_early_arcade_offset_x;
extern i32 g_early_arcade_offset_y;

// The effects Supervisor::end_stage_load_anms/abort_stage_load_anms
// remove. Nothing in TH16 creates them.
extern AnmId g_stage_load_anm_ids[3];
// Cleared with them; nothing else uses it.
extern i32 g_unk_4a6ef0;
// When set, Supervisor::on_draw_1a calls it instead of drawing.
extern void (*g_draw_hook_1a)();
// When set, Supervisor::on_draw_0f calls it instead of drawing.
extern void (*g_draw_hook_0f)();

// Where the title menu starts the next time it is created
// (g_title_return_point).
enum TitleReturnPoint
{
    // The first time after startup.
    TITLE_RETURN_FIRST = 0,
    // The title screen.
    TITLE_RETURN_MAIN = 1,
    // The replay menu, after a replay.
    TITLE_RETURN_REPLAY_MENU = 2,
    // The high score name entry (GAMEMODE_TITLE_SCORE_ENTRY).
    TITLE_RETURN_SCORE_ENTRY = 3,
    // The difficulty select, after a practice game.
    TITLE_RETURN_PRACTICE = 4,
    // The spell practice stage select, after a spell practice.
    TITLE_RETURN_SPELL_PRACTICE = 5,
};
// A TitleReturnPoint.
extern i32 g_title_return_point;
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
// FramePacingTable::mode: which set of sleep statistics the frame loop
// uses, by what is running.
enum FramePacingMode
{
    // Loading screens, scene changes and the pause menu (up to 15 ms of
    // sleep per frame).
    FRAME_PACING_IDLE = 0,
    // The title menus and the end-of-game menus (12 ms).
    FRAME_PACING_MENU = 1,
    // A stage in progress (12 ms). Mode 3 (8 ms) is never used.
    FRAME_PACING_GAME = 2,
};
// GameWindow::device_reset_frames: set to 10 when the device is reset,
// counted down once per frame by Supervisor::on_tick, never read.
extern i32 g_device_reset_frames;
// The game speed multiplier. ECL changes it (slowing down final boss
// deaths), and much code changes it temporarily so that different objects
// see time pass differently.
extern f32 g_game_speed;
