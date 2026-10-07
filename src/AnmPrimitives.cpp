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

// A width x height rectangle at (x, y) rotated by angle, anchored by
// anchor_x and anchor_y (0 center, 1 left/top, 2 right/bottom), colored
// color_1 on the left and color_2 on the right.
// TODO: the float register allocation of the corner coordinates differs (the original reuses height's stack slot).
// FUNCTION: TH16 0x468c70
HARNESS_CALLED i32 AnmManager::draw_rect(f32 x, f32 y, f32 width, f32 height, f32 angle, D3DCOLOR color_1,
                                         D3DCOLOR color_2, i32 anchor_x, i32 anchor_y)
{
    RenderVertex044 *vertices = primitive_write_cursor;
    if (vertices + 4 >= primitive_vertex_data + 0x8000)
    {
        return 0;
    }
    flush_sprites();
    f32 c;
    f32 s;
    f32 a = angle;
    __asm {
        fld a
        fsincos
        fstp c
        fstp s
    }
    f32 x0, x1, x2, x3;
    f32 y0, y1, y2, y3;
    switch (anchor_x)
    {
    case ANM_ANCHOR_CENTER:
        x1 = x3 = width * 0.5f;
        x0 = x2 = width * -0.5f;
        break;
    case ANM_ANCHOR_START:
        x1 = x3 = width;
        x0 = x2 = 0.0f;
        break;
    case ANM_ANCHOR_END:
        x1 = x3 = 0.0f;
        x0 = x2 = -width;
        break;
    }
    switch (anchor_y)
    {
    case ANM_ANCHOR_CENTER:
        y2 = y3 = height * 0.5f;
        y0 = y1 = height * -0.5f;
        break;
    case ANM_ANCHOR_START:
        y2 = y3 = height;
        y0 = y1 = 0.0f;
        break;
    case ANM_ANCHOR_END:
        y2 = y3 = 0.0f;
        y0 = y1 = -height;
        break;
    }
    vertices[0].pos.x = x0 * c - y0 * s + x;
    vertices[0].pos.y = x0 * s + y0 * c + y;
    vertices[1].pos.x = x1 * c - y1 * s + x;
    vertices[1].pos.y = x1 * s + y1 * c + y;
    vertices[2].pos.x = x2 * c - y2 * s + x;
    vertices[2].pos.y = x2 * s + y2 * c + y;
    vertices[3].pos.x = x3 * c - y3 * s + x;
    vertices[3].pos.y = x3 * s + y3 * c + y;
    vertices[0].diffuse = vertices[2].diffuse = color_1;
    vertices[0].pos.z = vertices[1].pos.z = vertices[2].pos.z = vertices[3].pos.z = 0.0f;
    vertices[0].pos.w = vertices[1].pos.w = vertices[2].pos.w = vertices[3].pos.w = 1.0f;
    vertices[1].diffuse = vertices[3].diffuse = color_2;
    if (g_Supervisor.zwrite_enabled != 0)
    {
        g_AnmManager->flush_sprites();
        g_Supervisor.zwrite_enabled = 0;
        g_Supervisor.d3d_device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    }
    if (g_AnmManager->last_color_op != ANM_COLOR_OP_DIFFUSE)
    {
        g_Supervisor.d3d_device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2);
        g_Supervisor.d3d_device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG2);
        g_AnmManager->last_color_op = ANM_COLOR_OP_DIFFUSE;
    }
    if (last_vertex_setup != ANM_VERTEX_SETUP_DIFFUSE)
    {
        g_Supervisor.d3d_device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
        g_Supervisor.d3d_device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
        last_vertex_setup = ANM_VERTEX_SETUP_DIFFUSE;
    }
    g_Supervisor.d3d_device->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
    g_Supervisor.d3d_device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, primitive_write_cursor, sizeof(RenderVertex044));
    primitive_write_cursor += 4;
    stat_draw_calls++;
    return 0;
}

// The outline of draw_rect's rectangle, as a line strip.
// TODO: the float register allocation of the corner coordinates differs (the original reuses height's stack slot).
// FUNCTION: TH16 0x468fc0
HARNESS_CALLED i32 AnmManager::draw_rect_outline(f32 x, f32 y, f32 width, f32 height, f32 angle, D3DCOLOR color_1,
                                         D3DCOLOR color_2, i32 anchor_x, i32 anchor_y)
{
    RenderVertex044 *vertices = primitive_write_cursor;
    if (vertices + 5 >= primitive_vertex_data + 0x8000)
    {
        return 0;
    }
    flush_sprites();
    f32 c;
    f32 s;
    f32 a = angle;
    __asm {
        fld a
        fsincos
        fstp c
        fstp s
    }
    f32 x0, x1, x2, x3;
    f32 y0, y1, y2, y3;
    switch (anchor_x)
    {
    case ANM_ANCHOR_CENTER:
        x1 = x3 = width * 0.5f;
        x0 = x2 = width * -0.5f;
        break;
    case ANM_ANCHOR_START:
        x1 = x3 = width;
        x0 = x2 = 0.0f;
        break;
    case ANM_ANCHOR_END:
        x1 = x3 = 0.0f;
        x0 = x2 = -width;
        break;
    }
    switch (anchor_y)
    {
    case ANM_ANCHOR_CENTER:
        y2 = y3 = height * 0.5f;
        y0 = y1 = height * -0.5f;
        break;
    case ANM_ANCHOR_START:
        y2 = y3 = height;
        y0 = y1 = 0.0f;
        break;
    case ANM_ANCHOR_END:
        y2 = y3 = 0.0f;
        y0 = y1 = -height;
        break;
    }
    vertices[0].pos.x = x0 * c - y0 * s + x;
    vertices[0].pos.y = x0 * s + y0 * c + y;
    vertices[1].pos.x = x1 * c - y1 * s + x;
    vertices[1].pos.y = x1 * s + y1 * c + y;
    vertices[3].pos.x = x2 * c - y2 * s + x;
    vertices[3].pos.y = x2 * s + y2 * c + y;
    vertices[2].pos.x = x3 * c - y3 * s + x;
    vertices[2].pos.y = x3 * s + y3 * c + y;
    *(D3DXVECTOR2 *)&vertices[4].pos = *(D3DXVECTOR2 *)&vertices[0].pos;
    vertices[0].diffuse = vertices[3].diffuse = vertices[4].diffuse = color_1;
    vertices[0].pos.z = vertices[1].pos.z = vertices[2].pos.z = vertices[3].pos.z = vertices[4].pos.z = 0.0f;
    vertices[0].pos.w = vertices[1].pos.w = vertices[2].pos.w = vertices[3].pos.w = vertices[4].pos.w = 1.0f;
    vertices[1].diffuse = vertices[2].diffuse = color_2;
    if (g_Supervisor.zwrite_enabled != 0)
    {
        g_AnmManager->flush_sprites();
        g_Supervisor.zwrite_enabled = 0;
        g_Supervisor.d3d_device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    }
    if (g_AnmManager->last_color_op != ANM_COLOR_OP_DIFFUSE)
    {
        g_Supervisor.d3d_device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2);
        g_Supervisor.d3d_device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG2);
        g_AnmManager->last_color_op = ANM_COLOR_OP_DIFFUSE;
    }
    if (last_vertex_setup != ANM_VERTEX_SETUP_DIFFUSE)
    {
        g_Supervisor.d3d_device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
        g_Supervisor.d3d_device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
        last_vertex_setup = ANM_VERTEX_SETUP_DIFFUSE;
    }
    g_Supervisor.d3d_device->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
    g_Supervisor.d3d_device->DrawPrimitiveUP(D3DPT_LINESTRIP, 4, primitive_write_cursor, sizeof(RenderVertex044));
    primitive_write_cursor += 5;
    stat_draw_calls++;
    return 0;
}

// draw_rect with a one pixel wider border at half the alpha behind it.
// FUNCTION: TH16 0x469570
HARNESS_CALLED i32 AnmManager::draw_rect_bordered(f32 x, f32 y, f32 width, f32 height, f32 angle, D3DCOLOR color_1,
                                                  D3DCOLOR color_2, i32 anchor_x, i32 anchor_y)
{
    draw_rect(x, y, width + 1.0f, height + 1.0f, angle, ((color_1 >> 1) & 0x7f000000) | (color_1 & 0xffffff),
              ((color_2 >> 1) & 0x7f000000) | (color_2 & 0xffffff), anchor_x, anchor_y);
    draw_rect(x, y, width, height, angle, color_1, color_2, anchor_x, anchor_y);
    return 0;
}

// A line of the given length through (x, y) at angle, anchored at its
// center (0), start (1) or end (2), colored from color_1 to color_2.
// TODO: ours sinks the second vertex's y computation below the color and z/w stores.
// FUNCTION: TH16 0x469330
HARNESS_CALLED i32 AnmManager::draw_line(f32 x, f32 y, f32 length, f32 angle, D3DCOLOR color_1, D3DCOLOR color_2,
                                         i32 anchor, i32 unused)
{
    RenderVertex044 *vertices = primitive_write_cursor;
    if (vertices + 2 >= primitive_vertex_data + 0x8000)
    {
        return 0;
    }
    flush_sprites();
    f32 c;
    f32 s;
    f32 a = angle;
    __asm {
        fld a
        fsincos
        fstp c
        fstp s
    }
    f32 start;
    f32 end;
    f32 offset = 0.0f;
    switch (anchor)
    {
    case ANM_ANCHOR_CENTER:
        start = length * -0.5f;
        end = length * 0.5f;
        break;
    case ANM_ANCHOR_START:
        start = 0.0f;
        end = length;
        break;
    case ANM_ANCHOR_END:
        start = -length;
        end = 0.0f;
        break;
    }
    vertices[0].pos.x = start * c - offset * s + x;
    vertices[0].pos.y = start * s + offset * c + y;
    vertices[1].pos.x = end * c - offset * s + x;
    vertices[1].pos.y = end * s + offset * c + y;
    vertices[0].diffuse = color_1;
    vertices[0].pos.z = vertices[1].pos.z = 0.0f;
    vertices[0].pos.w = vertices[1].pos.w = vertices[2].pos.w = vertices[3].pos.w = vertices[4].pos.w = 1.0f;
    vertices[1].diffuse = color_2;
    if (g_Supervisor.zwrite_enabled != 0)
    {
        g_AnmManager->flush_sprites();
        g_Supervisor.zwrite_enabled = 0;
        g_Supervisor.d3d_device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    }
    if (g_AnmManager->last_color_op != ANM_COLOR_OP_DIFFUSE)
    {
        g_Supervisor.d3d_device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2);
        g_Supervisor.d3d_device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG2);
        g_AnmManager->last_color_op = ANM_COLOR_OP_DIFFUSE;
    }
    if (last_vertex_setup != ANM_VERTEX_SETUP_DIFFUSE)
    {
        g_Supervisor.d3d_device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
        g_Supervisor.d3d_device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
        last_vertex_setup = ANM_VERTEX_SETUP_DIFFUSE;
    }
    g_Supervisor.d3d_device->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
    g_Supervisor.d3d_device->DrawPrimitiveUP(D3DPT_LINESTRIP, 1, primitive_write_cursor, sizeof(RenderVertex044));
    primitive_write_cursor += 2;
    stat_draw_calls++;
    return 0;
}

// A filled circle of count segments around (x, y) as a triangle fan, its
// color fading from the center to the edge, with fog off and the depth
// test always passing.
// TODO: the original adds the angle step right after the sincosmul call; ours after the stores, through xmm0.
// FUNCTION: TH16 0x469640
HARNESS_CALLED i32 AnmManager::draw_circle(f32 x, f32 y, f32 radius, f32 angle, i32 count, D3DCOLOR center_color,
                                           D3DCOLOR edge_color)
{
    RenderVertex044 *vertices = primitive_write_cursor;
    if (vertices + 2 + count >= primitive_vertex_data + 0x8000)
    {
        return 0;
    }
    flush_sprites();
    vertices->diffuse = center_color;
    vertices->pos.x = x;
    vertices->pos.y = y;
    vertices->pos.z = 0.0f;
    vertices->pos.w = 1.0f;
    vertices++;
    f32 step = ZUN_2PI / count;
    for (i32 i = 0; i < count + 1; i++)
    {
        primitive_sincosmul((Float3 *)&vertices->pos, angle, radius);
        vertices->pos.x = vertices->pos.x + x;
        vertices->pos.z = 0.0f;
        vertices->pos.w = 1.0f;
        vertices->diffuse = edge_color;
        vertices->pos.y = vertices->pos.y + y;
        vertices++;
        angle = wrap_angle(angle + step);
    }
    if (g_Supervisor.zwrite_enabled != 0)
    {
        g_AnmManager->flush_sprites();
        g_Supervisor.zwrite_enabled = 0;
        g_Supervisor.d3d_device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    }
    g_AnmManager->flush_sprites();
    g_Supervisor.d3d_device->SetRenderState(D3DRS_ZFUNC, D3DCMP_ALWAYS);
    if (g_AnmManager->last_color_op != ANM_COLOR_OP_DIFFUSE)
    {
        g_Supervisor.d3d_device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2);
        g_Supervisor.d3d_device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG2);
        g_AnmManager->last_color_op = ANM_COLOR_OP_DIFFUSE;
    }
    if (last_vertex_setup != ANM_VERTEX_SETUP_DIFFUSE)
    {
        g_Supervisor.d3d_device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
        g_Supervisor.d3d_device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
        last_vertex_setup = ANM_VERTEX_SETUP_DIFFUSE;
    }
    g_Supervisor.d3d_device->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
    g_Supervisor.d3d_device->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, count, primitive_write_cursor,
                                             sizeof(RenderVertex044));
    primitive_write_cursor += count + 2;
    stat_draw_calls++;
    return 0;
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
    for (i32 i = 0; i < count + 1; i++)
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
    if (g_AnmManager->last_color_op != ANM_COLOR_OP_DIFFUSE)
    {
        g_Supervisor.d3d_device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2);
        g_Supervisor.d3d_device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG2);
        g_AnmManager->last_color_op = ANM_COLOR_OP_DIFFUSE;
    }
    if (last_vertex_setup != ANM_VERTEX_SETUP_DIFFUSE)
    {
        g_Supervisor.d3d_device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
        g_Supervisor.d3d_device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
        last_vertex_setup = ANM_VERTEX_SETUP_DIFFUSE;
    }
    g_Supervisor.d3d_device->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
    g_Supervisor.d3d_device->DrawPrimitiveUP(D3DPT_LINESTRIP, count, primitive_write_cursor,
                                             sizeof(RenderVertex044));
    primitive_write_cursor += count + 1;
    stat_draw_calls++;
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
    for (i32 i = 0; i < count + 1; i++)
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
    if (g_AnmManager->last_color_op != ANM_COLOR_OP_DIFFUSE)
    {
        g_Supervisor.d3d_device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2);
        g_Supervisor.d3d_device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG2);
        g_AnmManager->last_color_op = ANM_COLOR_OP_DIFFUSE;
    }
    if (last_vertex_setup != ANM_VERTEX_SETUP_DIFFUSE)
    {
        g_Supervisor.d3d_device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
        g_Supervisor.d3d_device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
        last_vertex_setup = ANM_VERTEX_SETUP_DIFFUSE;
    }
    g_Supervisor.d3d_device->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
    g_Supervisor.d3d_device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, count * 2, primitive_write_cursor,
                                             sizeof(RenderVertex044));
    primitive_write_cursor += count * 2 + 2;
    stat_draw_calls++;
    return 0;
}
