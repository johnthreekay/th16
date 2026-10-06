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
    u8 unk_20[0x68 - 0x20];
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
    u8 unk_238[0x730 - 0x238];
    u32 flags;
    u8 unk_734[0x998 - 0x734];
    ThreadInf thread;
    u8 unk_9b4[0xa40 - 0x9b4];

    u32 read_joypad(u32 input);
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
