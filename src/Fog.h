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

// A distortion mesh over part of the game area (ExpHP: zFog): a grid of
// strip_count columns of strip_points points, drawn as strip_count - 1
// triangle strip VMs that sample the screen behind them. Enemies carry one
// (EnemyFog) and the stage's STD_DISTORTION makes one. Its buffers come
// from malloc.
struct Fog
{
    i32 strip_count;
    i32 strip_points;
    // The VM whose on_draw copies the mesh into the strip VMs.
    AnmId main_vm;
    // The strip VMs.
    AnmId *vm_ids;
    AnmVm **vms;
    // The grid's points as drawn (FogVertex), and their undistorted
    // positions (D3DXVECTOR3).
    void *vertices;
    void *points;

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
};

// 0x418c60. on_draw of the fog's main VM (effect kind 4).
int __fastcall anm_effect_4_on_draw(AnmVm *vm);
