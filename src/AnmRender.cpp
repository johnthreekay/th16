#include "AnmManager.h"
#include "Supervisor.h"

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
