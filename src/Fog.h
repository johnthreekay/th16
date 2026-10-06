#pragma once

#include "AnmVm.h"
#include "types.h"

// A point of the fog mesh as drawn (the layout of RenderVertex144).
struct FogVertex
{
    D3DXVECTOR3 pos;
    f32 rhw;
    D3DCOLOR diffuse;
    D3DXVECTOR2 uv;
};

// The fog effect an enemy can carry: a ring of ANM VMs (ExpHP: zFog). Its
// vertex buffers come from malloc.
struct Fog
{
    i32 vm_count;
    i32 unk_4;
    AnmId main_vm;
    AnmId *vm_ids;
    AnmVm **vms;
    void *buffer_14;
    void *buffer_18;

    // 0x418c70 (ExpHP: Fog::initialize). Every caller passes the same
    // first and third arguments, which LTCG folded away.
    Fog(i32 unused_0, i32 points_per_strip, i32 unused_2);
    // 0x409550
    ~Fog();
    // 0x418df0. Lays the mesh out over a rectangle of the game area. LTCG
    // passes x, y and width in xmm1-3.
    HARNESS_CALLED void set_rect(f32 x, f32 y, f32 width, f32 height);
    // 0x418f40. Copies the mesh into the strip VMs' vertex data.
    void update_vms();
    // 0x43d8b0. Creates one strip VM with room for two columns of points.
    // Callers do not pass this; LTCG dropped it.
    HARNESS_CALLED AnmId create_strip_vm(i32 points_per_strip, i32 unused);
};

// 0x418c60. on_draw of the fog's main VM (effect kind 4).
int __fastcall anm_effect_4_on_draw(AnmVm *vm);
