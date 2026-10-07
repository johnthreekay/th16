// The fog mesh that stages and enemies can carry.
#include <stdlib.h>
#include <string.h>

#include "AnmManager.h"
#include "Fog.h"
#include "Supervisor.h"

static_assert(sizeof(Fog) == 0x1c, "Fog size");
static_assert(sizeof(FogVertex) == 0x1c, "FogVertex size");

#define FOG_STRIP_COUNT 17

// TODO: the original pops each malloc's argument right after the call
// (ours merges them), keeps this in ebx and the loop index in memory.
// FUNCTION: TH16 0x418c70
Fog::Fog(i32 unused_0, i32 points_per_strip, i32 unused_2)
{
    if (g_Supervisor.arcade_surface_0 == NULL)
    {
        memset(this, 0, sizeof(Fog));
        return;
    }
    vm_count = FOG_STRIP_COUNT;
    unk_4 = points_per_strip;
    vm_ids = (AnmId *)malloc(sizeof(AnmId) * FOG_STRIP_COUNT - 1);
    vms = (AnmVm **)malloc(sizeof(AnmVm *) * FOG_STRIP_COUNT - 1);
    i32 num_points = FOG_STRIP_COUNT * points_per_strip;
    buffer_14 = malloc(num_points * sizeof(FogVertex));
    buffer_18 = malloc(num_points * sizeof(D3DXVECTOR3));
    AnmId id;
    id = g_Supervisor.text_anm->create_effect(0x3b, 0x22, NULL);
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
    for (i32 i = 0; i < vm_count - 1; i++)
    {
        vm_ids[i] = g_Supervisor.create_fog_vm(points_per_strip, 0x3b);
        vms[i] = get_vm_or_clear(vm_ids[i]);
        vms[i]->flags_lo &= ~ANM_VM_BLEND_MODE_MASK;
        vms[i]->flags_hi &= ~ANM_VM_ORIGIN_MODE_MASK;
    }
}

// TODO: the original reloads pos.z inside the inner loop (ours keeps it in
// ebx) and tests uv.x only after storing uv.y.
// FUNCTION: TH16 0x418df0
HARNESS_CALLED void Fog::set_rect(f32 x, f32 y, f32 width, f32 height)
{
    if (vm_count == 0)
    {
        return;
    }
    f32 step_x = width / (f32)(vm_count - 1);
    f32 step_y = height / (f32)(unk_4 - 1);
    D3DXVECTOR3 *point = (D3DXVECTOR3 *)buffer_18;
    FogVertex *vertex = (FogVertex *)buffer_14;
    D3DXVECTOR3 pos;
    pos.x = (f32)g_game_2d_origin_x + x;
    pos.y = (f32)g_game_2d_origin_y + y;
    pos.z = 0.0f;
    for (i32 i = 0; i < vm_count; i++)
    {
        for (i32 j = 0; j < unk_4; j++)
        {
            vertex->pos = *point = pos;
            vertex->uv.x = point->x / (f32)g_resolution_x;
            vertex->uv.y = point->y / (f32)g_resolution_y;
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

// FUNCTION: TH16 0x418f40
void Fog::update_vms()
{
    if (vm_count == 0)
    {
        return;
    }
    FogVertex *src = (FogVertex *)buffer_14;
    for (i32 i = 0; i < vm_count - 1; i++)
    {
        FogVertex *dst = (FogVertex *)vms[i]->extra_data;
        for (i32 j = 0; j < unk_4; j++)
        {
            *dst++ = *src;
            *dst++ = src[unk_4];
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
