#include <stddef.h>

#include "AnmManager.h"
#include "Supervisor.h"

static_assert(offsetof(AnmManager, render_cache_184fbb0) == 0x184fbb0, "AnmManager layout");
static_assert(offsetof(AnmManager, last_blend_mode) == 0x184fbb4, "AnmManager layout");
static_assert(offsetof(AnmManager, last_color_op) == 0x184fbbb, "AnmManager layout");
static_assert(offsetof(AnmManager, render_cache_184fbc0) == 0x184fbc0, "AnmManager layout");
static_assert(offsetof(AnmManager, unrendered_sprite_count) == 0x184fc18, "AnmManager layout");
static_assert(offsetof(AnmManager, sprite_write_cursor) == 0x1bcfc1c, "AnmManager layout");
static_assert(offsetof(AnmManager, primitive_write_cursor) == 0x1c6fc28, "AnmManager layout");
static_assert(sizeof(RenderVertex144) == 0x1c, "RenderVertex144 layout");

// GLOBAL: TH16 0x4df830
RenderVertex144 g_sprite_temp_buffer[4];

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
void AnmManager::draw_vm__mode_5(AnmVm *vm)
{
    render_sub_466f00(vm);
    render_sprite_2d(vm, 0);
    g_sprite_temp_buffer[3].pos.w = 1.0f;
    g_sprite_temp_buffer[2].pos.w = 1.0f;
    g_sprite_temp_buffer[1].pos.w = 1.0f;
    g_sprite_temp_buffer[0].pos.w = 1.0f;
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
