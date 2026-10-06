#pragma once

#include <windows.h>

#include <d3d9.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>

#include "AnmManager.h"
#include "Camera.h"
#include "Thread.h"
#include "types.h"

// The game's settings, as stored in th16.cfg. Layout from ExpHP's
// th-re-data; most fields are still unknown.
struct Config
{
    u32 unk_0;
    u32 unk_4;
    // Copied from g_pad_mapping by a static initializer.
    i16 pad_mapping_copy[10];
    // Analog stick dead zones (DirectInput axis units).
    i16 deadzone_x;
    i16 deadzone_y;
    u8 unk_20[0x2c - 0x20];
    // 0x8 skips DirectInput setup.
    u32 flags_2c;
    u8 unk_30[0x68 - 0x30];
};

// Owns the Direct3D/DirectInput objects and global game state. ZUN's name
// for it in older games was MotherInf (per ExpHP).
struct Supervisor
{
    // ExpHP's layout puts d3d, d3d_device and dinput 4 bytes later, but
    // the code says otherwise: GetBackBuffer/SetRenderState go through +0x8
    // and DirectInput8Create writes +0xc.
    u8 unk_0[4];
    IDirect3D9 *d3d;
    IDirect3DDevice9 *d3d_device;
    IDirectInput8A *dinput;
    u8 unk_10[0x10];
    IDirectInputDevice8A *keyboard;
    IDirectInputDevice8A *joystick;
    u8 unk_28[0x1ac - 0x28];
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
    u8 unk_700[0x730 - 0x700];
    u32 flags;
    u8 unk_734[0x998 - 0x734];
    ThreadInf thread;
    i32 unk_9b4;
    i32 unk_9b8;
    u8 unk_9bc[0xa0c - 0x9bc];
    i32 fog_enabled;
    i32 zwrite_enabled;
    u8 unk_a14[0xa1c - 0xa14];
    // th16_<version>.ver, read in on_registration.
    i32 ver_file_size;
    void *ver_file_data;
    u8 unk_a24[0xa3c - 0xa24];
    D3DCOLOR background_color;

    u32 read_joypad(u32 input);

    HRESULT enable_d3d_fog();
    HRESULT disable_d3d_fog();
    HRESULT enable_zwrite();
    HRESULT disable_zwrite();
    void swap_transform_matrices(Camera *camera);
    void release_surfaces();
    void sub_43c630();
    void sub_43c6a0();

    int switch_gamemodes();
    void setup_special_anms();
    // Members that do not use this; LTCG dropped it.
    static void read_keyboard_input();
    int initialize();
    // Runs a loader function on `thread`. Every caller passes NULL for arg,
    // which LTCG folds; the loaders themselves are plain void functions.
    HARNESS_CALLED i32 start_thread(ThreadStart start, void *arg);
    i32 play_bgm_wav(i32 arg, const char *name);
    i32 play_bgm(i32 arg, i32 track);
    i32 stop_bgm();
    HARNESS_CALLED i32 fade_out_bgm(f32 seconds);

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
};

enum SupervisorFlags
{
    // Read the pad through DirectInput rather than joyGetPosEx.
    SUPERVISOR_USE_DIRECTINPUT_PAD = 1 << 11,
};

extern Supervisor g_Supervisor;

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
// Where game coordinate (0, 0) is on the arcade surface.
extern i32 g_game_2d_origin_x;
extern i32 g_game_2d_origin_y;

// The ANM ids Supervisor::sub_43c630/sub_43c6a0 interrupt.
extern AnmId g_anm_ids_4c0f4c[3];
extern i32 g_unk_4a6ef0;
// When set, Supervisor::on_draw_1a calls it instead of drawing.
extern void (*g_draw_hook_4a6ee8)();
// Set once the loading screen is done.
extern i32 g_unk_4d9d90;
// Counted down once per frame by Supervisor::on_tick.
extern i32 g_unk_4d9d20;
// The game speed multiplier. ECL changes it (slowing down final boss
// deaths), and much code changes it temporarily so that different objects
// see time pass differently.
extern f32 g_game_speed;
