// Untextured shapes drawn from the primitive vertex buffer (AnmManager's
// render modes 16 and up).
#include "AnmManager.h"
#include "Supervisor.h"

// This file's copy of ZunMath.h's sincosmul.
// FUNCTION: TH16 0x469e00
static void __fastcall primitive_sincosmul(Float3 *dst, f32 angle, f32 radius)
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

// The outline of a circle of count segments around (x, y), as a line
// strip starting at angle.
// TODO: ours never uses ebx (the original keeps the vertex count bytes and color in it) and spills this.
// FUNCTION: TH16 0x469a00
HARNESS_CALLED i32 AnmManager::draw_circle_outline(f32 x, f32 y, f32 radius, f32 angle, i32 count, D3DCOLOR color)
{
    if (primitive_write_cursor + 1 + count >= primitive_vertex_data + 0x8000)
    {
        return 0;
    }
    flush_sprites();
    RenderVertex044 *vertices = primitive_write_cursor;
    f32 step = ZUN_2PI / count;
    for (i32 i = count + 1; i > 0; i--)
    {
        primitive_sincosmul((Float3 *)&vertices->pos, angle, radius);
        vertices->pos.x = x + vertices->pos.x;
        vertices->pos.z = 0.0f;
        vertices->pos.w = 1.0f;
        vertices->diffuse = color;
        vertices->pos.y = y + vertices->pos.y;
        vertices++;
        angle = wrap_angle(angle + step);
    }
    if (g_AnmManager->last_color_op != 0)
    {
        g_Supervisor.d3d_device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2);
        g_Supervisor.d3d_device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG2);
        g_AnmManager->last_color_op = 0;
    }
    if (render_cache_184fbb6 != 1)
    {
        g_Supervisor.d3d_device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
        g_Supervisor.d3d_device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
        render_cache_184fbb6 = 1;
    }
    g_Supervisor.d3d_device->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
    g_Supervisor.d3d_device->DrawPrimitiveUP(D3DPT_LINESTRIP, count, primitive_write_cursor,
                                             sizeof(RenderVertex044));
    primitive_write_cursor += count + 1;
    unk_cc++;
    return 0;
}

// A ring of count segments around (x, y), width wide, as a triangle strip
// starting at angle.
// TODO: ours never uses ebx (the original keeps the color in it) and spills this.
// FUNCTION: TH16 0x469bd0
HARNESS_CALLED i32 AnmManager::draw_ring(f32 x, f32 y, f32 radius, f32 width, f32 angle, i32 count, D3DCOLOR color)
{
    if (primitive_write_cursor + 2 + count * 2 >= primitive_vertex_data + 0x8000)
    {
        return 0;
    }
    flush_sprites();
    f32 half_width = width * 0.5f;
    RenderVertex044 *vertices = primitive_write_cursor;
    f32 inner = radius - half_width;
    f32 outer = half_width + radius;
    f32 step = ZUN_2PI / count;
    for (i32 i = count + 1; i > 0; i--)
    {
        primitive_sincosmul((Float3 *)&vertices[0].pos, angle, inner);
        vertices[0].pos.x = vertices[0].pos.x + x;
        vertices[0].pos.z = 0.0f;
        vertices[0].pos.w = 1.0f;
        vertices[0].diffuse = color;
        vertices[0].pos.y = vertices[0].pos.y + y;
        primitive_sincosmul((Float3 *)&vertices[1].pos, angle, outer);
        vertices[1].pos.x = vertices[1].pos.x + x;
        vertices[1].pos.z = 0.0f;
        vertices[1].pos.w = 1.0f;
        vertices[1].diffuse = color;
        vertices[1].pos.y = vertices[1].pos.y + y;
        vertices += 2;
        angle = wrap_angle(angle + step);
    }
    if (g_AnmManager->last_color_op != 0)
    {
        g_Supervisor.d3d_device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2);
        g_Supervisor.d3d_device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG2);
        g_AnmManager->last_color_op = 0;
    }
    if (render_cache_184fbb6 != 1)
    {
        g_Supervisor.d3d_device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
        g_Supervisor.d3d_device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
        render_cache_184fbb6 = 1;
    }
    g_Supervisor.d3d_device->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
    g_Supervisor.d3d_device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, count * 2, primitive_write_cursor,
                                             sizeof(RenderVertex044));
    primitive_write_cursor += count * 2 + 2;
    unk_cc++;
    return 0;
}
