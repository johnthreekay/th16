#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include "AnmManager.h"
#include "Rng.h"
#include "Supervisor.h"
#include "UpdateFunc.h"

static_assert(offsetof(AnmManager, render_cache_184fbb0) == 0x184fbb0, "AnmManager layout");
static_assert(offsetof(AnmManager, last_blend_mode) == 0x184fbb4, "AnmManager layout");
static_assert(offsetof(AnmManager, last_color_op) == 0x184fbbb, "AnmManager layout");
static_assert(offsetof(AnmManager, render_cache_184fbc0) == 0x184fbc0, "AnmManager layout");
static_assert(offsetof(AnmManager, unrendered_sprite_count) == 0x184fc18, "AnmManager layout");
static_assert(offsetof(AnmManager, sprite_write_cursor) == 0x1bcfc1c, "AnmManager layout");
static_assert(offsetof(AnmManager, primitive_write_cursor) == 0x1c6fc28, "AnmManager layout");
static_assert(sizeof(RenderVertex144) == 0x1c, "RenderVertex144 layout");

// The quads of the 2D drawing code, in TH06's order: transformed without
// color, transformed with color (the sprite batch's) and untransformed.
// Their implicit constructors stay out of line, called by the dynamic
// initializers at 0x4011f0, 0x401220 and 0x401250.
// SYNTHETIC: TH16 0x46a370
// RenderVertexXyzrhwTex::RenderVertexXyzrhwTex
// SYNTHETIC: TH16 0x46a380
// RenderVertex144::RenderVertex144
// SYNTHETIC: TH16 0x46a390
// RenderVertexXyzDiffuseTex::RenderVertexXyzDiffuseTex

// GLOBAL: TH16 0x4df4a8
RenderVertexXyzrhwTex g_quad_vertices_4df4a8[4];

// GLOBAL: TH16 0x4df830
RenderVertex144 g_sprite_temp_buffer[4];

// GLOBAL: TH16 0x4df8a0
RenderVertexXyzDiffuseTex g_quad_vertices_4df8a0[4];

// FUNCTION: TH16 0x464f10
void AnmManager::setup_render_state_for_vm(AnmVm *vm)
{
    if (last_blend_mode != ((vm->flags_lo >> ANM_VM_BLEND_MODE_SHIFT) & 0xf))
    {
        flush_sprites();
        last_blend_mode = (vm->flags_lo >> ANM_VM_BLEND_MODE_SHIFT) & 0xf;
        g_Supervisor.d3d_device->SetRenderState(D3DRS_ALPHATESTENABLE, TRUE);
        switch (last_blend_mode)
        {
        case 0:
            g_Supervisor.d3d_device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
            g_Supervisor.d3d_device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
            g_Supervisor.d3d_device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
            break;
        case 1:
            g_Supervisor.d3d_device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
            g_Supervisor.d3d_device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
            g_Supervisor.d3d_device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
            break;
        case 2:
            g_Supervisor.d3d_device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
            g_Supervisor.d3d_device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
            g_Supervisor.d3d_device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_REVSUBTRACT);
            break;
        case 3:
            g_Supervisor.d3d_device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
            g_Supervisor.d3d_device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
            g_Supervisor.d3d_device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ZERO);
            g_Supervisor.d3d_device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
            break;
        case 4:
            g_Supervisor.d3d_device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_INVDESTCOLOR);
            g_Supervisor.d3d_device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCCOLOR);
            g_Supervisor.d3d_device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
            break;
        case 6:
            g_Supervisor.d3d_device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_INVSRCCOLOR);
            g_Supervisor.d3d_device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
            g_Supervisor.d3d_device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
            break;
        case 5:
            g_Supervisor.d3d_device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_DESTCOLOR);
            g_Supervisor.d3d_device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ZERO);
            g_Supervisor.d3d_device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
            break;
        case 7:
            g_Supervisor.d3d_device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_DESTALPHA);
            g_Supervisor.d3d_device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVDESTALPHA);
            g_Supervisor.d3d_device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
            break;
        case 8:
            g_Supervisor.d3d_device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
            g_Supervisor.d3d_device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
            g_Supervisor.d3d_device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_MIN);
            break;
        case 9:
            g_Supervisor.d3d_device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
            g_Supervisor.d3d_device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
            g_Supervisor.d3d_device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_MAX);
            break;
        }
    }
    if (last_filter_point != ((vm->flags_hi >> ANM_VM_FILTER_POINT_SHIFT) & 1))
    {
        flush_sprites();
        last_filter_point = (vm->flags_hi >> ANM_VM_FILTER_POINT_SHIFT) & 1;
        if (!last_filter_point)
        {
            g_Supervisor.d3d_device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
            g_Supervisor.d3d_device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
        }
        else
        {
            g_Supervisor.d3d_device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
            g_Supervisor.d3d_device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
        }
    }
    if (last_address_u != (vm->flags_hi & ANM_VM_ADDRESS_U_MASK))
    {
        flush_sprites();
        last_address_u = vm->flags_hi & ANM_VM_ADDRESS_U_MASK;
        switch (last_address_u)
        {
        case 0:
            g_Supervisor.d3d_device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
            break;
        case 1:
            g_Supervisor.d3d_device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
            break;
        case 2:
            g_Supervisor.d3d_device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_MIRROR);
            break;
        }
    }
    if (last_address_v != (vm->flags_lo >> ANM_VM_ADDRESS_V_SHIFT))
    {
        flush_sprites();
        last_address_v = (vm->flags_lo >> ANM_VM_ADDRESS_V_SHIFT) & 3;
        switch (last_address_v)
        {
        case 0:
            g_Supervisor.d3d_device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
            break;
        case 1:
            g_Supervisor.d3d_device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
            break;
        case 2:
            g_Supervisor.d3d_device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_MIRROR);
            break;
        }
    }
    unk_c8++;
}

// FUNCTION: TH16 0x465a30
HARNESS_CALLED void AnmManager::reset_vertex_buffers()
{
    unrendered_sprite_count = 0;
    sprite_write_cursor = sprite_vertex_data;
    sprite_render_cursor = sprite_vertex_data;
    unrendered_primitive_count = 0;
    primitive_write_cursor = primitive_vertex_data;
    primitive_render_cursor = primitive_vertex_data;
}

// TODO: the device and its vtable swap registers (ecx/edx) for DrawPrimitiveUP.
// FUNCTION: TH16 0x465a80
void AnmManager::flush_sprites()
{
    if (unrendered_sprite_count == 0)
    {
        return;
    }
    if (g_AnmManager->last_color_op != 1)
    {
        g_Supervisor.d3d_device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
        g_Supervisor.d3d_device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
        g_AnmManager->last_color_op = 1;
    }
    g_Supervisor.d3d_device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
    g_Supervisor.d3d_device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    g_Supervisor.d3d_device->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1);
    g_Supervisor.d3d_device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, unrendered_sprite_count * 2, sprite_render_cursor,
                                             sizeof(RenderVertex144));
    unk_cc++;
    sprite_render_cursor = sprite_write_cursor;
    unrendered_sprite_count = 0;
}

// FUNCTION: TH16 0x465b50
i32 AnmManager::write_sprite(RenderVertex144 *vertices)
{
    if (sprite_write_cursor + 6 >= sprite_vertex_data + 0x20000)
    {
        return 1;
    }
    sprite_write_cursor[0] = vertices[0];
    sprite_write_cursor[1] = vertices[1];
    sprite_write_cursor[2] = vertices[2];
    sprite_write_cursor[3] = vertices[1];
    sprite_write_cursor[4] = vertices[2];
    sprite_write_cursor[5] = vertices[3];
    sprite_write_cursor += 6;
    unrendered_sprite_count++;
    return 0;
}

// Render mode 9: like draw_vm__mode_11 for visible VMs only, with the
// texture set first and color ops reset to modulate.
// TODO: the original keeps this in ebx and vm in esi (edi only around SetTexture); ours spills this.
// FUNCTION: TH16 0x4681f0
i32 AnmManager::draw_vm__mode_9(AnmVm *vm, RenderVertex144 *vertices, i32 vertex_count)
{
    if (!(vm->flags_lo & ANM_VM_VISIBLE))
    {
        return -1;
    }
    if (!(vm->flags_lo & ANM_VM_FLAG_LO_2))
    {
        return -1;
    }
    if (vm->color_1.a == 0)
    {
        return -1;
    }
    if (unrendered_sprite_count != 0)
    {
        flush_sprites();
    }
    i32 texture = g_AnmManager->loaded_anms[vm->anm_loaded_index]->sprites[vm->sprite_id].image_file_num_in_all;
    if (render_cache_184fbb0 != texture)
    {
        render_cache_184fbb0 = texture;
        g_Supervisor.d3d_device->SetTexture(0, loaded_anms[texture >> 8]->d3d[texture & 0xff].texture);
    }
    if (render_cache_184fbb6 != 3)
    {
        g_Supervisor.d3d_device->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1);
        g_Supervisor.d3d_device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
        g_Supervisor.d3d_device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
        render_cache_184fbb6 = 3;
    }
    setup_render_state_for_vm(vm);
    if (g_AnmManager->last_color_op != 1)
    {
        g_Supervisor.d3d_device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
        g_Supervisor.d3d_device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
        g_AnmManager->last_color_op = 1;
    }
    g_Supervisor.d3d_device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, vertex_count - 2, vertices,
                                             sizeof(RenderVertex144));
    return 0;
}

// TODO: the original keeps this in edi with a stack copy; ours uses ebx.
// FUNCTION: TH16 0x468350
i32 AnmManager::draw_vm__mode_11(AnmVm *vm, RenderVertex144 *vertices, i32 vertex_count)
{
    if (unrendered_sprite_count != 0)
    {
        flush_sprites();
    }
    if (render_cache_184fbb6 != 3)
    {
        g_Supervisor.d3d_device->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1);
        render_cache_184fbb6 = 3;
    }
    setup_render_state_for_vm(vm);
    i32 texture = g_AnmManager->loaded_anms[vm->anm_loaded_index]->sprites[vm->sprite_id].image_file_num_in_all;
    if (render_cache_184fbb0 != texture)
    {
        render_cache_184fbb0 = texture;
        g_Supervisor.d3d_device->SetTexture(0, loaded_anms[texture >> 8]->d3d[texture & 0xff].texture);
    }
    g_Supervisor.disable_zwrite();
    if (render_cache_184fbb6 != 1)
    {
        g_Supervisor.d3d_device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
        g_Supervisor.d3d_device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
        render_cache_184fbb6 = 1;
    }
    g_Supervisor.d3d_device->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, vertex_count - 2, vertices, sizeof(RenderVertex144));
    return 0;
}

// Draws count points, each center + offsets[i] in colors[i], as a line
// strip (despite the name) from the primitive buffer.
// TODO: ours never uses ebx (the original keeps count * 20 and center in it) and spills the loop counter.
// FUNCTION: TH16 0x469890
HARNESS_CALLED void AnmManager::draw_triangle_fan(i32 count, Float3 *center, Float2 *offsets, ZunColor *colors)
{
    AnmManager *mgr = g_AnmManager;
    RenderVertex044 *vertices = mgr->primitive_write_cursor;
    if (vertices + 1 + count >= mgr->primitive_vertex_data + 0x8000)
    {
        return;
    }
    mgr->flush_sprites();
    for (i32 i = 0; i < count; i++)
    {
        vertices->pos.x = center->x + offsets->x;
        vertices->pos.y = offsets->y + center->y;
        vertices->pos.z = 0.0f;
        vertices->pos.w = 1.0f;
        vertices->diffuse = colors->d3d;
        vertices++;
        offsets++;
        colors++;
    }
    if (g_AnmManager->last_color_op != 0)
    {
        g_Supervisor.d3d_device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2);
        g_Supervisor.d3d_device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG2);
        g_AnmManager->last_color_op = 0;
    }
    if (mgr->render_cache_184fbb6 != 1)
    {
        g_Supervisor.d3d_device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
        g_Supervisor.d3d_device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
        mgr->render_cache_184fbb6 = 1;
    }
    g_Supervisor.d3d_device->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
    g_Supervisor.d3d_device->DrawPrimitiveUP(D3DPT_LINESTRIP, count - 1, mgr->primitive_write_cursor,
                                             sizeof(RenderVertex044));
    mgr->primitive_write_cursor += count;
    mgr->unk_cc++;
}

// The extra data of the VMs drawn by anm_on_draw_fan: a fan of 33 vertices
// (the center, 31 points around it and the first point again), with each
// point's radius and its growth, and the texture scroll speed.
struct AnmFanData
{
    RenderVertex144 vertices[33];
    u8 unk_39c[4];
    f32 radius[33];
    f32 radius_speed[32];
    f32 uv_speed;
    f32 unk_4a8;
    u8 unk_4ac[4];
};

// This file's copy of ZunMath.h's sincosmul.
// FUNCTION: TH16 0x46a350
static void __fastcall fan_sincosmul(Float3 *dst, f32 angle, f32 radius)
{
    __asm {
        mov eax, dst
        fld angle
        fsincos
        fmul radius
        fstp [eax]
        fmul radius
        fstp [eax+4]
    }
}

// Sets up render mode 10 (ANM instruction 302): a fan of random radii
// around the VM, moved by on_tick 4 and drawn by on_draw 6.
// TODO: the original keeps the angle in xmm4 and the radius speed in memory, storing the speed after the random call.
// FUNCTION: TH16 0x469e20
int __fastcall anm_effect_4_init(AnmVm *vm)
{
    if (vm->ins_508_extra_data != NULL)
    {
        free(vm->ins_508_extra_data);
        vm->ins_508_extra_data = NULL;
        vm->ins_508_extra_data_size = 0;
    }
    vm->alloc_extra_data(sizeof(AnmFanData));
    vm->index_of_on_tick = 4;
    vm->index_of_on_draw = 6;
    AnmFanData *data = (AnmFanData *)vm->ins_508_extra_data;
    data->uv_speed = g_replay_safe_rng.randf_neg_1_to_1() * (1.0f / 120.0f);
    data->unk_4a8 = g_replay_safe_rng.randf_neg_1_to_1() * (1.0f / 120.0f);
    f32 angle = -ZUN_PI;
    *(Float3 *)&data->vertices[0].pos = vm->entity_pos + vm->pos;
    data->vertices[0].pos.w = 1.0f;
    data->vertices[0].uv.x = 0.5f;
    data->vertices[0].uv.y = 0.5f;
    f32 speed = g_replay_safe_rng.randf_neg_1_to_1() * (1.0f / 15.0f);
    RenderVertex144 *vertex = &data->vertices[1];
    f32 *radius = data->radius;
    for (i32 i = 31; i != 0; i--)
    {
        if (angle >= ZUN_PI)
        {
            angle -= ZUN_2PI;
        }
        vertex->pos.w = 1.0f;
        Float3 uv;
        fan_sincosmul(&uv, angle, 0.5f);
        vertex->pos.z = 0.0f;
        vertex->uv.x = uv.x + 0.5f;
        vertex->uv.y = uv.y + 0.5f;
        f32 r = g_replay_safe_rng.randf_neg_1_to_1() * 8.0f + 80.0f;
        radius[33] = speed;
        *radius = r;
        speed += g_replay_safe_rng.randf_neg_1_to_1() * (1.0f / 30.0f);
        if (speed < -(1.0f / 15.0f))
        {
            speed = -(1.0f / 15.0f);
        }
        else if (speed > 1.0f / 15.0f)
        {
            speed = 1.0f / 15.0f;
        }
        fan_sincosmul((Float3 *)&vertex->pos, angle, *radius);
        vertex->pos.x = vertex->pos.x + (vm->entity_pos.x + vm->pos.x);
        vertex->pos.y = vertex->pos.y + (vm->pos.y + vm->entity_pos.y);
        vertex->pos.z = vertex->pos.z + (vm->entity_pos.z + vm->pos.z);
        angle += ZUN_2PI / 31.0f;
        vertex++;
        radius++;
    }
    return 0;
}

// Scrolls the fan's texture coordinates, keeping them from going negative.
static inline void fan_scroll_u(AnmFanData *data, RenderVertex144 *vertex)
{
    vertex->uv.x += data->uv_speed;
    if (vertex->uv.x < 0.0f)
    {
        for (i32 i = 0; i < 33; i++)
        {
            data->vertices[i].uv.x += 1.0f;
        }
    }
}

static inline void fan_scroll_v(AnmFanData *data, RenderVertex144 *vertex)
{
    vertex->uv.y += data->uv_speed;
    if (vertex->uv.y < 0.0f)
    {
        for (i32 i = 0; i < 33; i++)
        {
            data->vertices[i].uv.y += 1.0f;
        }
    }
}

// The on_tick callback of the fan VMs: grows the points, scrolls the
// texture and places the fan at the VM.
// TODO: the original hoists the -pi, 0 and 1 constants into xmm4-6 at entry and walks the radii with ebx; scheduling differs.
// FUNCTION: TH16 0x46a0b0
i32 __fastcall anm_on_tick_fan(AnmVm *vm)
{
    AnmFanData *data = (AnmFanData *)vm->ins_508_extra_data;
    *(Float3 *)&data->vertices[0].pos = vm->entity_pos + vm->pos;
    data->vertices[0].uv.x += data->uv_speed;
    if (data->vertices[0].uv.x < 0.0f)
    {
        for (i32 i = 0; i < 33; i++)
        {
            data->vertices[i].uv.x += 1.0f;
        }
    }
    data->vertices[0].uv.y += data->uv_speed;
    if (data->vertices[0].uv.y < 0.0f)
    {
        for (i32 i = 0; i < 33; i++)
        {
            data->vertices[i].uv.y += 1.0f;
        }
    }
    data->vertices[0].diffuse = vm->color_1.d3d;
    f32 angle = -ZUN_PI;
    for (i32 i = 0; i < 31; i++)
    {
        RenderVertex144 *vertex = &data->vertices[i + 1];
        fan_scroll_u(data, vertex);
        fan_scroll_v(data, vertex);
        vertex->diffuse = vm->color_1.d3d;
        ((ZunColor *)&vertex->diffuse)->a = 0;
        data->radius[i] = data->radius_speed[i] + data->radius[i];
        fan_sincosmul((Float3 *)&vertex->pos, angle, data->radius[i]);
        angle += ZUN_2PI / 31.0f;
        vertex->pos.x = vertex->pos.x + (vm->pos.x + vm->entity_pos.x);
        vertex->pos.y = (vm->pos.y + vm->entity_pos.y) + vertex->pos.y;
        vertex->pos.z = (vm->entity_pos.z + vm->pos.z) + vertex->pos.z;
    }
    data->vertices[32] = data->vertices[1];
    return 0;
}

// The on_draw callback of VMs that carry their own vertices: a fan of 33
// vertices in the extra data of instruction 508 (ExpHP: AnmVm::on_draw__6).
// FUNCTION: TH16 0x46a330
i32 __fastcall anm_on_draw_fan(AnmVm *vm)
{
    g_AnmManager->draw_vm__mode_11(vm, (RenderVertex144 *)vm->ins_508_extra_data, 0x21);
    return 0;
}

// Rebuilds the VM's world matrix (scale, then rotation) when needed and
// puts it, moved to the VM's position, in matrix_184f56c.
// TODO: ours aligns the frame to 16 for the spilled translation row (movaps); the original keeps an ebp frame with movups.
// FUNCTION: TH16 0x466f00
void AnmManager::render_sub_466f00(AnmVm *vm)
{
    D3DXMATRIX m;
    if (!(vm->flags_lo & 0x10000))
    {
        vm->matrix_410 = vm->matrix_3d0;
        vm->matrix_410._11 *= vm->scale_2.x * vm->scale.x;
        vm->matrix_410._22 *= vm->scale_2.y * vm->scale.y;
        vm->flags_lo &= ~ANM_VM_SCALE_CHANGED;
        Float3 rotation = *vm->get_total_rotation();
        D3DXMATRIX rotation_matrix;
        if (rotation.x != 0.0f)
        {
            D3DXMatrixRotationX(&rotation_matrix, rotation.x);
            D3DXMatrixMultiply(&vm->matrix_410, &vm->matrix_410, &rotation_matrix);
        }
        if (rotation.y != 0.0f)
        {
            D3DXMatrixRotationY(&rotation_matrix, rotation.y);
            D3DXMatrixMultiply(&vm->matrix_410, &vm->matrix_410, &rotation_matrix);
        }
        if (rotation.z != 0.0f)
        {
            D3DXMatrixRotationZ(&rotation_matrix, rotation.z);
            D3DXMatrixMultiply(&vm->matrix_410, &vm->matrix_410, &rotation_matrix);
        }
        vm->flags_lo &= ~ANM_VM_ROTATION_CHANGED;
    }
    m = vm->matrix_410;
    m._41 = vm->entity_pos.x + vm->pos.x + vm->pos_2.x + m._41;
    if ((vm->flags_hi & ANM_VM_LAYER_KIND_MASK) && vm->unk_5b0 == NULL)
    {
        m._41 += g_resolution_x * 0.5f;
        m._42 += (g_resolution_y - 448.0f) * 0.5f;
    }
    m._42 = vm->entity_pos.y + vm->pos.y + vm->pos_2.y + m._42;
    m._43 = vm->entity_pos.z + vm->pos.z + vm->pos_2.z;
    if (vm->unk_5b0 != NULL && !(vm->flags_hi & ANM_VM_NO_PARENT_POS))
    {
        AnmVm *parent = vm->unk_5b0;
        m._41 = parent->entity_pos.x + parent->pos.x + parent->pos_2.x + m._41;
        m._42 = parent->entity_pos.y + parent->pos.y + parent->pos_2.y + m._42;
        m._43 = parent->entity_pos.z + parent->pos.z + parent->pos_2.z + m._43;
    }
    matrix_184f56c = m;
}

// FUNCTION: TH16 0x4671b0
i32 AnmManager::draw_vm__mode_5(AnmVm *vm)
{
    render_sub_466f00(vm);
    i32 result = render_sprite_2d(vm, 0);
    g_sprite_temp_buffer[3].pos.w = 1.0f;
    g_sprite_temp_buffer[2].pos.w = 1.0f;
    g_sprite_temp_buffer[1].pos.w = 1.0f;
    g_sprite_temp_buffer[0].pos.w = 1.0f;
    return result;
}

// TODO: the original reserves a dead 4-byte local (push ecx/pop ecx).
// FUNCTION: TH16 0x468c00
void AnmVm::write_sprite_corners(Float3 *corners)
{
    switch ((flags_lo >> ANM_VM_RENDER_MODE_SHIFT) & 0x1f)
    {
    case 1:
        write_sprite_corners__with_z_rot(this, &corners[0], &corners[1], &corners[2], &corners[3]);
        break;
    case 0:
    case 2:
    case 3:
        write_sprite_corners__without_rot(this, &corners[0], &corners[1], &corners[2], &corners[3]);
        break;
    }
}

// AnmManager's UpdateFuncs, as LTCG inlined create_func into its
// constructor.
static __forceinline UpdateFunc *anm_create_func(UpdateFuncCallback function, AnmManager *arg)
{
    UpdateFunc *f = new UpdateFunc;
    f->flags |= UPDATE_FUNC_HEAP_ALLOCATED;
    f->function = function;
    f->on_registration = NULL;
    f->on_cleanup = NULL;
    f->flags |= UPDATE_FUNC_ACTIVE;
    f->arg = arg;
    return f;
}

#define ANM_REGISTER_ON_TICK(callback, priority)                                                                    \
    g_UpdateFuncRegistry->register_on_tick(anm_create_func((UpdateFuncCallback)(callback), this), (priority))
#define ANM_REGISTER_ON_DRAW(callback, priority)                                                                    \
    g_UpdateFuncRegistry->register_on_draw(anm_create_func((UpdateFuncCallback)(callback), this), (priority))

// TODO: 99.9%; one inlined UpdateFunc constructor clears the heap flag two stores later in the original.
// FUNCTION: TH16 0x46a3a0
AnmManager::AnmManager()
{
    last_discriminator = 0;
    memset(this, 0, sizeof(AnmManager));
    g_quad_vertices_4df4a8[0].pos.w = g_quad_vertices_4df4a8[1].pos.w = g_quad_vertices_4df4a8[2].pos.w =
        g_quad_vertices_4df4a8[3].pos.w = 1.0f;
    g_quad_vertices_4df4a8[0].uv.x = 0.0f;
    g_quad_vertices_4df4a8[0].uv.y = 0.0f;
    g_quad_vertices_4df4a8[1].uv.x = 1.0f;
    g_quad_vertices_4df4a8[1].uv.y = 0.0f;
    g_quad_vertices_4df4a8[2].uv.x = 0.0f;
    g_quad_vertices_4df4a8[2].uv.y = 1.0f;
    g_quad_vertices_4df4a8[3].uv.x = 1.0f;
    g_quad_vertices_4df4a8[3].uv.y = 1.0f;
    g_sprite_temp_buffer[0].pos.w = g_sprite_temp_buffer[1].pos.w = g_sprite_temp_buffer[2].pos.w =
        g_sprite_temp_buffer[3].pos.w = 1.0f;
    g_sprite_temp_buffer[0].uv.x = 0.0f;
    g_sprite_temp_buffer[0].uv.y = 0.0f;
    g_sprite_temp_buffer[1].uv.x = 1.0f;
    g_sprite_temp_buffer[1].uv.y = 0.0f;
    g_sprite_temp_buffer[2].uv.x = 0.0f;
    g_sprite_temp_buffer[2].uv.y = 1.0f;
    g_sprite_temp_buffer[3].uv.x = 1.0f;
    g_sprite_temp_buffer[3].uv.y = 1.0f;
    vertex_buffer = NULL;
    render_cache_184fbb0 = -1;
    last_blend_mode = 0;
    render_cache_184fbb5 = 0;
    last_texture_factor = 1;
    render_cache_184fbb6 = 0;
    render_cache_184fbb7 = 0;
    render_cache_184fbb8 = 0xff;
    screen_copies[0].anm_slot = -1;
    screen_copies[1].anm_slot = -1;
    screen_copies[2].anm_slot = -1;
    screen_copies[3].anm_slot = -1;
    freelist_head.init((AnmFastVm *)&freelist_head);
    i32 i;
    for (i = 0; i < 0x1fff; i++)
    {
        AnmFastVm *fast = &fast_array[i];
        fast->vm.wipe();
        fast->vm.fast_id = i;
        fast->is_alive = false;
        fast->fast_id = i;
        fast->freelist_node.init(fast);
        freelist_head.insert_after(&fast->freelist_node);
    }
    next_snapshot_fast_id = 0;
    for (i = 0; i < 0x1fff; i++)
    {
        snapshot_fast_array[i].vm.wipe();
        snapshot_fast_array[i].vm.fast_id = i;
        snapshot_fast_array[i].fast_id = i;
        snapshot_fast_array[i].is_alive = false;
    }
    ANM_REGISTER_ON_TICK(on_tick_21, 0x21);
    ANM_REGISTER_ON_TICK(on_tick_09, 9);
    ANM_REGISTER_ON_DRAW(on_draw_05_layer_00, 5);
    ANM_REGISTER_ON_DRAW(on_draw_07_layer_01, 7);
    ANM_REGISTER_ON_DRAW(on_draw_09_layer_02, 9);
    ANM_REGISTER_ON_DRAW(on_draw_0b_layer_04, 0xb);
    ANM_REGISTER_ON_DRAW(on_draw_0a_layer_03, 0xa);
    ANM_REGISTER_ON_DRAW(on_draw_0d_layer_05, 0xd);
    ANM_REGISTER_ON_DRAW(on_draw_10_layer_06, 0x10);
    ANM_REGISTER_ON_DRAW(on_draw_12_layer_07, 0x12);
    ANM_REGISTER_ON_DRAW(on_draw_14_layer_08, 0x14);
    ANM_REGISTER_ON_DRAW(on_draw_15_layer_09, 0x15);
    ANM_REGISTER_ON_DRAW(on_draw_16_layer_10, 0x16);
    ANM_REGISTER_ON_DRAW(on_draw_18_layer_11, 0x18);
    ANM_REGISTER_ON_DRAW(on_draw_1b_layer_12, 0x1b);
    ANM_REGISTER_ON_DRAW(on_draw_1c_layer_13, 0x1c);
    ANM_REGISTER_ON_DRAW(on_draw_1f_layer_14, 0x1f);
    ANM_REGISTER_ON_DRAW(on_draw_20_layer_15, 0x20);
    ANM_REGISTER_ON_DRAW(on_draw_22_layer_16, 0x22);
    ANM_REGISTER_ON_DRAW(on_draw_24_layer_17, 0x24);
    ANM_REGISTER_ON_DRAW(on_draw_27_layer_18, 0x27);
    ANM_REGISTER_ON_DRAW(on_draw_2a_layer_19, 0x2a);
    ANM_REGISTER_ON_DRAW(on_draw_2d_layer_20, 0x2d);
    ANM_REGISTER_ON_DRAW(on_draw_2e_layer_21, 0x2e);
    ANM_REGISTER_ON_DRAW(on_draw_34_layer_22, 0x34);
    ANM_REGISTER_ON_DRAW(on_draw_36_layer_23, 0x36);
    ANM_REGISTER_ON_DRAW(on_draw_3d_layer_26, 0x3d);
    ANM_REGISTER_ON_DRAW(on_draw_3e_layer_27, 0x3e);
    ANM_REGISTER_ON_DRAW(on_draw_4d_layer_29, 0x4d);
    ANM_REGISTER_ON_DRAW(on_draw_4f_layer_30, 0x4f);
    ANM_REGISTER_ON_DRAW(on_draw_52_layer_31, 0x52);
    ANM_REGISTER_ON_DRAW(on_draw_3b_layer_25, 0x3b);
    ANM_REGISTER_ON_DRAW(on_draw_3a_layer_24, 0x3a);
    ANM_REGISTER_ON_DRAW(on_draw_40_layer_28, 0x40);
    ANM_REGISTER_ON_DRAW(on_draw_37_layer_36, 0x37);
    ANM_REGISTER_ON_DRAW(on_draw_3c_layer_37, 0x3c);
    ANM_REGISTER_ON_DRAW(on_draw_3f_layer_38, 0x3f);
    ANM_REGISTER_ON_DRAW(on_draw_41_layer_39, 0x41);
    ANM_REGISTER_ON_DRAW(on_draw_4e_layer_40, 0x4e);
    ANM_REGISTER_ON_DRAW(on_draw_50_layer_41, 0x50);
    ANM_REGISTER_ON_DRAW(on_draw_53_layer_42, 0x53);
    g_Supervisor.d3d_device->SetVertexShader(NULL);
    next_snapshot_discriminator = 0;
}
