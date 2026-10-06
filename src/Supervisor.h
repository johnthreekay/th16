#pragma once

#include <windows.h>

#include <d3d9.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>

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
    u8 unk_20[0x23 - 0x20];
    // Window size option; 0, 1, 2 pick ascii.anm, ascii_960.anm,
    // ascii_1280.anm.
    u8 window_size;
    u8 unk_24[0x68 - 0x24];
};

// A view of the scene with its matrices and viewport. Layout from
// ExpHP's th-re-data (zCamera).
struct Camera
{
    f32 position[3];
    f32 facing[3];
    f32 up[3];
    f32 facing_normalized[3];
    f32 unk_30[3];
    f32 rocking_vector_1[3];
    f32 rocking_vector_2[3];
    f32 field_of_view;
    i32 window_resolution[2];
    D3DMATRIX view_matrix;
    D3DMATRIX projection_matrix;
    D3DVIEWPORT9 viewport;
    i32 camera_index;
    f32 unk_fc[2];
    f32 unk_104[3];
    u8 sky[0x12c - 0x110];
};

// Owns the Direct3D/DirectInput objects and global game state. ZUN's name
// for it in older games was MotherInf (per ExpHP).
struct Supervisor
{
    u8 unk_0[8];
    IDirect3D9 *d3d;
    IDirect3DDevice9 *d3d_device;
    IDirectInput8A *dinput;
    u8 unk_14[0x10];
    IDirectInputDevice8A *joystick;
    u8 unk_28[0x1d0 - 0x28];
    Config config;
    Camera cameras[4];
    // One of cameras; set together with current_camera_index.
    Camera *current_camera;
    i32 current_camera_index;
    u8 unk_6f0[0x730 - 0x6f0];
    u32 flags;
    u8 unk_734[0x998 - 0x734];
    ThreadInf thread;
    u8 unk_9b4[0xa40 - 0x9b4];

    u32 read_joypad(u32 input);
    // Callers never pass this; LTCG dropped it.
    static void __stdcall swap_transform_matrices(Camera *camera);
};

enum SupervisorFlags
{
    // Read the pad through DirectInput rather than joyGetPosEx.
    SUPERVISOR_USE_DIRECTINPUT_PAD = 1 << 11,
};

extern Supervisor g_Supervisor;

// Scale from 640x480 coordinates to the window's (1, 1.5 or 2).
extern f32 g_screen_coord_scale;

// Pad button numbers for each game button, -1 if unassigned. Index meaning
// (from read_joypad): 0 shot, 1 bomb, 2 -> 0x8, 3 -> 0x100, 9 -> 0x800.
extern i16 g_pad_mapping[10];
