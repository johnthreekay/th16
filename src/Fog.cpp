// The distortion mesh ("fog") that stages and enemies can carry.
#include <stdlib.h>
#include <string.h>

#include "AnmManager.h"
#include "Fog.h"
#include "Supervisor.h"

static_assert(sizeof(Fog) == 0x1c, "Fog size");
static_assert(sizeof(FogVertex) == 0x1c, "FogVertex size");

// Columns of the grid; the strips run between neighbouring columns.
#define FOG_STRIP_COUNT 17
// text.anm script of the main VM and the strip VMs.
#define FOG_TEXT_ANM_SCRIPT 0x3b

// Allocates the grid and creates the VMs (the main VM on layer 0x22,
// drawing through anm_effect_4_on_draw). Without the arcade surface to
// sample there is no mesh. The id and VM pointer arrays are allocated one
// byte short of 17 entries, room for the 16 strips.
// TODO: the original pops each malloc's argument right after the call
// (ours merges them), keeps this in ebx and the loop index in memory.
// FUNCTION: TH16 0x418c70
HARNESS_CALLED Fog::Fog(i32 unused_0, i32 points_per_strip, i32 unused_2)
{
    if (g_Supervisor.arcade_surface_0 == NULL)
    {
        memset(this, 0, sizeof(Fog));
        return;
    }
    strip_count = FOG_STRIP_COUNT;
    strip_points = points_per_strip;
    vm_ids = (AnmId *)malloc(sizeof(AnmId) * FOG_STRIP_COUNT - 1);
    vms = (AnmVm **)malloc(sizeof(AnmVm *) * FOG_STRIP_COUNT - 1);
    i32 num_points = FOG_STRIP_COUNT * points_per_strip;
    vertices = malloc(num_points * sizeof(FogVertex));
    points = malloc(num_points * sizeof(D3DXVECTOR3));
    AnmId id;
    id = g_Supervisor.text_anm->create_effect(FOG_TEXT_ANM_SCRIPT, 0x22, NULL);
    AnmVm *vm = g_AnmManager->get_vm_with_id(id);
    if (vm == NULL)
    {
        id.id = 0;
    }
    vm->alloc_extra_data(0x70);
    vm->flags_lo &= ~(0x1f << ANM_VM_RENDER_MODE_SHIFT);
    main_vm = id;
    vm = get_vm_or_clear(main_vm);
    vm->index_of_on_draw = ANM_ON_DRAW_FOG;
    vm->associated_game_entity = this;
    for (i32 i = 0; i < strip_count - 1; i++)
    {
        vm_ids[i] = g_Supervisor.create_fog_vm(points_per_strip, FOG_TEXT_ANM_SCRIPT);
        vms[i] = get_vm_or_clear(vm_ids[i]);
        vms[i]->flags_lo &= ~ANM_VM_BLEND_MODE_MASK;
        vms[i]->flags_hi &= ~ANM_VM_ORIGIN_MODE_MASK;
    }
}

// Spreads the grid evenly over the rectangle (in game area coordinates),
// with texture coordinates that sample the screen at each point.
// TODO: before the loops the original converts strip_points - 1 after the
// game area origin's y and divides after adding y; ours the other way round.
// FUNCTION: TH16 0x418df0
HARNESS_CALLED void Fog::set_rect(f32 x, f32 y, f32 width, f32 height)
{
    if (strip_count == 0)
    {
        return;
    }
    f32 step_x = width / (f32)(strip_count - 1);
    f32 step_y = height / (f32)(strip_points - 1);
    D3DXVECTOR3 *point = (D3DXVECTOR3 *)points;
    FogVertex *vertex = (FogVertex *)vertices;
    D3DXVECTOR3 pos;
    pos.x = (f32)g_game_2d_origin_x + x;
    pos.y = (f32)g_game_2d_origin_y + y;
    pos.z = 0.0f;
    for (i32 i = 0; i < strip_count; i++)
    {
        for (i32 j = 0; j < strip_points; j++)
        {
            vertex->pos = *point = pos;
            vertex->uv.x = point->x / (f32)g_resolution_x;
            // Through D3DXVECTOR2's operator FLOAT*: the store may alias uv.x,
            // so its test comes after it, as in the original.
            vertex->uv[1] = point->y / (f32)g_resolution_y;
            if (vertex->uv.x < 0.0f)
            {
                vertex->uv.x = 0.0f;
            }
            if (vertex->uv.y < 0.0f)
            {
                vertex->uv.y = 0.0f;
            }
            vertex->rhw = 1.0f;
            vertex->diffuse = 0xffffffff;
            pos.y += step_y;
            point++;
            vertex++;
        }
        pos.y = (f32)g_game_2d_origin_y + y;
        pos.x += step_x;
    }
    update_vms();
}

// Each strip VM draws a triangle strip zigzagging between its two columns.
// FUNCTION: TH16 0x418f40
void Fog::update_vms()
{
    if (strip_count == 0)
    {
        return;
    }
    FogVertex *src = (FogVertex *)vertices;
    for (i32 i = 0; i < strip_count - 1; i++)
    {
        FogVertex *dst = (FogVertex *)vms[i]->extra_data;
        for (i32 j = 0; j < strip_points; j++)
        {
            *dst++ = *src;
            *dst++ = src[strip_points];
            src++;
        }
    }
}

// FUNCTION: TH16 0x418c60
int __fastcall anm_effect_4_on_draw(AnmVm *vm)
{
    ((Fog *)vm->associated_game_entity)->update_vms();
    return 0;
}
