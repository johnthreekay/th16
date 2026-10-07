// AnmManager::draw_vm and the render modes it dispatches to.
#include <stddef.h>

#include "AnmManager.h"
#include "Supervisor.h"

static_assert(offsetof(AnmManager, last_texture_factor) == 0x184fbac, "AnmManager layout");
static_assert(offsetof(AnmManager, quad_184fbc8) == 0x184fbc8, "AnmManager layout");
static_assert(sizeof(RenderVertexXyzTex) == 0x14, "RenderVertexXyzTex layout");
static_assert(sizeof(RenderVertexXyzDiffuseTex) == 0x18, "RenderVertexXyzDiffuseTex layout");

// Corner offsets of a sprite in units of its size, by anchoring (center,
// left/top, right/bottom), for the corners in g_sprite_temp_buffer order.
struct AnmAnchorCorners
{
    f32 corner[4];
};

// At 0x4a3048, right after g_sound_effect_table; not annotated, since the
// sound code's end-of-table pointer would compare as this symbol.
AnmAnchorCorners g_anchor_corners_x[3] = {
    {-0.5f, 0.5f, -0.5f, 0.5f},
    {0.0f, 1.0f, 0.0f, 1.0f},
    {-1.0f, 0.0f, -1.0f, 0.0f},
};

// GLOBAL: TH16 0x4a3078
AnmAnchorCorners g_anchor_corners_y[3] = {
    {-0.5f, -0.5f, 0.5f, 0.5f},
    {0.0f, 0.0f, 1.0f, 1.0f},
    {-1.0f, -1.0f, 0.0f, 0.0f},
};

// A color channel scaled by another, where 0x80 is 1.0, saturating.
static inline u32 color_mul(u32 a, u32 b)
{
    u32 v = a * b >> 7;
    if (v >= 0x100)
    {
        v = 0xff;
    }
    return v;
}

static inline f32 max_f(f32 a, f32 b)
{
    return a > b ? a : b;
}

static inline f32 min_f(f32 a, f32 b)
{
    return a < b ? a : b;
}

static inline ZunColor *diffuse_of(i32 i)
{
    return (ZunColor *)&g_sprite_temp_buffer[i].diffuse;
}

// The quad in g_sprite_temp_buffer, moved by the camera offset, rounded to
// pixel centers if flags bit 0 is set, culled against the viewport and
// colored by the VM's color mode unless flags bit 1 is set.
// FUNCTION: TH16 0x465280
i32 AnmManager::render_sprite_2d(AnmVm *vm, i32 flags)
{
    static const f32 half = 0.5f;

    g_sprite_temp_buffer[0].pos.x += camera_unk_fc.x;
    g_sprite_temp_buffer[0].pos.y += camera_unk_fc.y;
    g_sprite_temp_buffer[1].pos.x += camera_unk_fc.x;
    g_sprite_temp_buffer[1].pos.y += camera_unk_fc.y;
    g_sprite_temp_buffer[2].pos.x += camera_unk_fc.x;
    g_sprite_temp_buffer[2].pos.y += camera_unk_fc.y;
    g_sprite_temp_buffer[3].pos.x += camera_unk_fc.x;
    g_sprite_temp_buffer[3].pos.y += camera_unk_fc.y;
    if (flags & 1)
    {
        __asm {
            fld g_sprite_temp_buffer[0 * TYPE g_sprite_temp_buffer].pos.x
            frndint
            fsub half
            fld g_sprite_temp_buffer[1 * TYPE g_sprite_temp_buffer].pos.x
            frndint
            fsub half
            fld g_sprite_temp_buffer[0 * TYPE g_sprite_temp_buffer].pos.y
            frndint
            fsub half
            fld g_sprite_temp_buffer[2 * TYPE g_sprite_temp_buffer].pos.y
            frndint
            fsub half
            fst g_sprite_temp_buffer[2 * TYPE g_sprite_temp_buffer].pos.y
            fstp g_sprite_temp_buffer[3 * TYPE g_sprite_temp_buffer].pos.y
            fst g_sprite_temp_buffer[0 * TYPE g_sprite_temp_buffer].pos.y
            fstp g_sprite_temp_buffer[1 * TYPE g_sprite_temp_buffer].pos.y
            fst g_sprite_temp_buffer[1 * TYPE g_sprite_temp_buffer].pos.x
            fstp g_sprite_temp_buffer[3 * TYPE g_sprite_temp_buffer].pos.x
            fst g_sprite_temp_buffer[0 * TYPE g_sprite_temp_buffer].pos.x
            fstp g_sprite_temp_buffer[2 * TYPE g_sprite_temp_buffer].pos.x
        }
    }
    vm->last_rendered_quad_in_surface_space[0] = *(Float3 *)&g_sprite_temp_buffer[0].pos;
    vm->last_rendered_quad_in_surface_space[1] = *(Float3 *)&g_sprite_temp_buffer[1].pos;
    vm->last_rendered_quad_in_surface_space[2] = *(Float3 *)&g_sprite_temp_buffer[2].pos;
    vm->last_rendered_quad_in_surface_space[3] = *(Float3 *)&g_sprite_temp_buffer[3].pos;
    g_sprite_temp_buffer[0].uv.x = vm->uv_scroll_pos.x + vm->uv_quad_of_sprite[0].x;
    g_sprite_temp_buffer[0].uv.y = vm->uv_quad_of_sprite[0].y + vm->uv_scroll_pos.y;
    g_sprite_temp_buffer[1].uv.x = (vm->uv_quad_of_sprite[1].x - vm->uv_quad_of_sprite[0].x) * vm->uv_scale.x +
                                   (vm->uv_scroll_pos.x + vm->uv_quad_of_sprite[0].x);
    g_sprite_temp_buffer[1].uv.y = vm->uv_quad_of_sprite[1].y + vm->uv_scroll_pos.y;
    g_sprite_temp_buffer[2].uv.x = vm->uv_quad_of_sprite[2].x + vm->uv_scroll_pos.x;
    g_sprite_temp_buffer[2].uv.y = (vm->uv_quad_of_sprite[2].y - vm->uv_quad_of_sprite[0].y) * vm->uv_scale.y +
                                   (vm->uv_scroll_pos.y + vm->uv_quad_of_sprite[0].y);
    g_sprite_temp_buffer[3].uv.x = (vm->uv_quad_of_sprite[3].x - vm->uv_quad_of_sprite[2].x) * vm->uv_scale.x +
                                   (vm->uv_scroll_pos.x + vm->uv_quad_of_sprite[2].x);
    g_sprite_temp_buffer[3].uv.y = (vm->uv_quad_of_sprite[3].y - vm->uv_quad_of_sprite[1].y) * vm->uv_scale.y +
                                   (vm->uv_scroll_pos.y + vm->uv_quad_of_sprite[1].y);
    D3DVIEWPORT9 *viewport = &g_Supervisor.current_camera->viewport;
    if (max_f(g_sprite_temp_buffer[3].pos.x,
              max_f(g_sprite_temp_buffer[2].pos.x, max_f(g_sprite_temp_buffer[0].pos.x, g_sprite_temp_buffer[1].pos.x))) <
        (f32)viewport->X)
    {
        return 0;
    }
    if (max_f(g_sprite_temp_buffer[3].pos.y,
              max_f(g_sprite_temp_buffer[2].pos.y, max_f(g_sprite_temp_buffer[0].pos.y, g_sprite_temp_buffer[1].pos.y))) <
        (f32)viewport->Y)
    {
        return 0;
    }
    if (min_f(g_sprite_temp_buffer[3].pos.x,
              min_f(g_sprite_temp_buffer[2].pos.x, min_f(g_sprite_temp_buffer[0].pos.x, g_sprite_temp_buffer[1].pos.x))) >
        (f32)(viewport->X + viewport->Width))
    {
        return 0;
    }
    if (min_f(g_sprite_temp_buffer[3].pos.y,
              min_f(g_sprite_temp_buffer[2].pos.y, min_f(g_sprite_temp_buffer[0].pos.y, g_sprite_temp_buffer[1].pos.y))) >
        (f32)(viewport->Y + viewport->Height))
    {
        return 0;
    }
    i32 texture = g_AnmManager->loaded_anms[vm->anm_loaded_index]->sprites[vm->sprite_id].image_file_num_in_all;
    if (render_cache_184fbb0 != texture)
    {
        render_cache_184fbb0 = texture;
        flush_sprites();
        g_Supervisor.d3d_device->SetTexture(
            0, loaded_anms[render_cache_184fbb0 >> 8]->d3d[(u8)render_cache_184fbb0].texture);
    }
    if (render_cache_184fbb6 != 1)
    {
        flush_sprites();
        render_cache_184fbb6 = 1;
    }
    if (!(flags & 2))
    {
        switch ((vm->flags_lo >> 17) & 3)
        {
        case 0:
        case 1: {
            ZunColor color;
            color.d3d = (vm->flags_lo & ANM_VM_COLOR_MODE_MASK) ? vm->color_2.d3d : vm->color_1.d3d;
            u8 r, g, b, a;
            if ((vm->flags_hi & ANM_VM_FLAG_HI_2000000) && vm->unk_5b0 != NULL)
            {
                ZunColor parent = vm->unk_5b0->mixed_inherited_color;
                color.r = r = color_mul(color.r, parent.r);
                color.g = g = color_mul(color.g, parent.g);
                color.b = b = color_mul(color.b, parent.b);
                color.a = a = color_mul(color.a, parent.a);
            }
            else
            {
                r = color.r;
                g = color.g;
                b = color.b;
                a = color.a;
            }
            vm->mixed_inherited_color = color;
            if (unk_1c7fd8c != 0)
            {
                color.r = color_mul(r, unk_1c7fd88.r);
                color.g = color_mul(g, unk_1c7fd88.g);
                color.b = color_mul(b, unk_1c7fd88.b);
                color.a = color_mul(a, unk_1c7fd88.a);
            }
            g_sprite_temp_buffer[2].diffuse = color.d3d;
            g_sprite_temp_buffer[3].diffuse = color.d3d;
            g_sprite_temp_buffer[1].diffuse = color.d3d;
            g_sprite_temp_buffer[0].diffuse = color.d3d;
            break;
        }
        case 2:
        case 3: {
            ZunColor color_1 = vm->color_1;
            ZunColor color_2 = vm->color_2;
            if (unk_1c7fd8c != 0)
            {
                color_1.r = color_mul(color_1.r, unk_1c7fd88.r);
                color_1.g = color_mul(color_1.g, unk_1c7fd88.g);
                color_1.b = color_mul(color_1.b, unk_1c7fd88.b);
                color_1.a = color_mul(color_1.a, unk_1c7fd88.a);
                color_2.r = color_mul(color_2.r, unk_1c7fd88.r);
                color_2.g = color_mul(color_2.g, unk_1c7fd88.g);
                color_2.b = color_mul(color_2.b, unk_1c7fd88.b);
                color_2.a = color_mul(color_2.a, unk_1c7fd88.a);
            }
            g_sprite_temp_buffer[3].diffuse = color_2.d3d;
            if ((vm->flags_lo & ANM_VM_COLOR_MODE_MASK) == 2 << 17)
            {
                g_sprite_temp_buffer[1].diffuse = color_2.d3d;
                g_sprite_temp_buffer[2].diffuse = color_1.d3d;
            }
            else
            {
                g_sprite_temp_buffer[2].diffuse = color_2.d3d;
                g_sprite_temp_buffer[1].diffuse = color_1.d3d;
            }
            g_sprite_temp_buffer[0].diffuse = color_1.d3d;
            break;
        }
        }
    }
    setup_render_state_for_vm(vm);
    write_sprite(g_sprite_temp_buffer);
    return 0;
}

// Corners for render modes 0, 2 and 3: the anchored sprite rectangle,
// scaled, at the VM's transformed position.
// FUNCTION: TH16 0x465c40
void __stdcall AnmVm::write_sprite_corners__without_rot(AnmVm *vm, Float3 *a, Float3 *b, Float3 *c, Float3 *d)
{
    AnmAnchorCorners *anchor = &g_anchor_corners_x[(vm->flags_lo >> 21) & 3];
    a->x = anchor->corner[0];
    b->x = anchor->corner[1];
    c->x = anchor->corner[2];
    d->x = anchor->corner[3];
    anchor = &g_anchor_corners_y[(vm->flags_lo >> 23) & 3];
    a->y = anchor->corner[0];
    b->y = anchor->corner[1];
    c->y = anchor->corner[2];
    d->y = anchor->corner[3];
    a->x *= vm->sprite_size.x;
    b->x *= vm->sprite_size.x;
    c->x *= vm->sprite_size.x;
    d->x *= vm->sprite_size.x;
    a->y *= vm->sprite_size.y;
    b->y *= vm->sprite_size.y;
    c->y *= vm->sprite_size.y;
    d->y *= vm->sprite_size.y;
    a->x -= vm->anchor_offset.x;
    b->x -= vm->anchor_offset.x;
    c->x -= vm->anchor_offset.x;
    d->x -= vm->anchor_offset.x;
    a->y -= vm->anchor_offset.y;
    b->y -= vm->anchor_offset.y;
    c->y -= vm->anchor_offset.y;
    d->y -= vm->anchor_offset.y;
    switch (vm->flags_hi & ANM_VM_COORD_MODE_MASK)
    {
    case 1 << 20:
        a->x *= g_screen_coord_scale;
        b->x *= g_screen_coord_scale;
        c->x *= g_screen_coord_scale;
        d->x *= g_screen_coord_scale;
        a->y *= g_screen_coord_scale;
        b->y *= g_screen_coord_scale;
        c->y *= g_screen_coord_scale;
        d->y *= g_screen_coord_scale;
        break;
    case 2 << 20:
        a->x *= g_screen_coord_scale * 0.5f;
        b->x *= g_screen_coord_scale * 0.5f;
        c->x *= g_screen_coord_scale * 0.5f;
        d->x *= g_screen_coord_scale * 0.5f;
        a->y *= g_screen_coord_scale * 0.5f;
        b->y *= g_screen_coord_scale * 0.5f;
        c->y *= g_screen_coord_scale * 0.5f;
        d->y *= g_screen_coord_scale * 0.5f;
        break;
    }
    f32 scale_x = vm->scale_2.x * vm->scale.x;
    f32 scale_y = vm->scale_2.y * vm->scale.y;
    if (vm->unk_5b0 != NULL && !(vm->flags_hi & ANM_VM_NO_PARENT_POS))
    {
        scale_x *= vm->unk_5b0->scale_2.x * vm->unk_5b0->scale.x;
        scale_y *= vm->unk_5b0->scale_2.y * vm->unk_5b0->scale.y;
    }
    a->x *= scale_x;
    b->x *= scale_x;
    c->x *= scale_x;
    d->x *= scale_x;
    a->y *= scale_y;
    b->y *= scale_y;
    c->y *= scale_y;
    d->y *= scale_y;
    Float3 pos;
    vm->get_own_transformed_pos(&pos);
    *a += pos;
    *b += pos;
    *c += pos;
    *d += pos;
    a->z = b->z = c->z = d->z = vm->entity_pos.z + vm->pos.z + vm->pos_2.z;
}

// Corners for render modes 1 and 3: the same rectangle rotated by the
// VM's total z rotation.
// FUNCTION: TH16 0x4660b0
void __stdcall AnmVm::write_sprite_corners__with_z_rot(AnmVm *vm, Float3 *a, Float3 *b, Float3 *c, Float3 *d)
{
    f32 angle = vm->get_total_rotation()->z;
    f32 sine;
    f32 cosine;
    __asm {
        fld angle
        fsincos
        fstp cosine
        fstp sine
    }
    AnmAnchorCorners xs = g_anchor_corners_x[(vm->flags_lo >> 21) & 3];
    AnmAnchorCorners ys = g_anchor_corners_y[(vm->flags_lo >> 23) & 3];
    i32 i;
    for (i = 0; i < 4; i++)
    {
        xs.corner[i] = xs.corner[i] * vm->sprite_size.x - vm->anchor_offset.x;
        ys.corner[i] = ys.corner[i] * vm->sprite_size.y - vm->anchor_offset.y;
    }
    switch (vm->flags_hi & ANM_VM_COORD_MODE_MASK)
    {
    case 1 << 20:
        for (i = 0; i < 4; i++)
        {
            xs.corner[i] *= g_screen_coord_scale;
            ys.corner[i] *= g_screen_coord_scale;
        }
        break;
    case 2 << 20:
        for (i = 0; i < 4; i++)
        {
            xs.corner[i] *= g_screen_coord_scale * 0.5f;
            ys.corner[i] *= g_screen_coord_scale * 0.5f;
        }
        break;
    }
    Float3 pos;
    vm->get_own_transformed_pos(&pos);
    f32 scale_x = vm->scale_2.x * vm->scale.x;
    f32 scale_y = vm->scale_2.y * vm->scale.y;
    if (vm->unk_5b0 != NULL && !(vm->flags_hi & ANM_VM_NO_PARENT_POS))
    {
        scale_x *= vm->unk_5b0->scale_2.x * vm->unk_5b0->scale.x;
        scale_y *= vm->unk_5b0->scale_2.y * vm->unk_5b0->scale.y;
    }
    for (i = 0; i < 4; i++)
    {
        xs.corner[i] *= scale_x;
        ys.corner[i] *= scale_y;
    }
    a->x = xs.corner[0] * cosine - ys.corner[0] * sine + pos.x;
    a->y = ys.corner[0] * cosine + xs.corner[0] * sine + pos.y;
    b->x = xs.corner[1] * cosine - ys.corner[1] * sine + pos.x;
    b->y = ys.corner[1] * cosine + xs.corner[1] * sine + pos.y;
    c->x = xs.corner[2] * cosine - ys.corner[2] * sine + pos.x;
    c->y = ys.corner[2] * cosine + xs.corner[2] * sine + pos.y;
    d->x = xs.corner[3] * cosine - ys.corner[3] * sine + pos.x;
    d->y = ys.corner[3] * cosine + xs.corner[3] * sine + pos.y;
    a->z = b->z = c->z = d->z = vm->entity_pos.z + vm->pos.z + vm->pos_2.z;
}

// FUNCTION: TH16 0x466390
i32 __stdcall AnmManager::write_sprite_corners__mode_4(AnmVm *vm)
{
    f32 angle = vm->get_total_rotation()->z;
    f32 sine;
    f32 cosine;
    __asm {
        fld angle
        fsincos
        fstp cosine
        fstp sine
    }
    D3DXVECTOR3 world_pos(vm->entity_pos.x + vm->pos.x + vm->pos_2.x, vm->entity_pos.y + vm->pos.y + vm->pos_2.y,
                          vm->entity_pos.z + vm->pos.z + vm->pos_2.z);
    D3DXMATRIX world;
    D3DXMatrixIdentity(&world);
    world._41 = world_pos.x;
    world._42 = world_pos.y;
    world._43 = world_pos.z;
    D3DXVECTOR3 origin(0.0f, 0.0f, 0.0f);
    D3DXVECTOR3 screen;
    Camera *camera = g_Supervisor.current_camera;
    D3DXVec3Project(&screen, &origin, &camera->viewport, (D3DXMATRIX *)&camera->projection_matrix, (D3DXMATRIX *)&camera->view_matrix, &world);
    if (screen.z < 0.0f || screen.z > 1.0f)
    {
        return -1;
    }
    D3DXVECTOR3 screen_2;
    camera = g_Supervisor.current_camera;
    D3DXVec3Project(&screen_2, &camera->unk_30, &camera->viewport, (D3DXMATRIX *)&camera->projection_matrix, (D3DXMATRIX *)&camera->view_matrix,
                    &world);
    f32 x = screen.x;
    f32 y = screen.y;
    D3DXVECTOR3 diff = screen_2 - screen;
    f32 scale = D3DXVec3Length(&diff) * 0.5f;
    f32 width = vm->sprite_size.x * scale * vm->scale.x * vm->scale_2.x;
    f32 height = vm->sprite_size.y * scale * vm->scale.y * vm->scale_2.y;
    g_sprite_temp_buffer[0].pos.z = g_sprite_temp_buffer[1].pos.z = g_sprite_temp_buffer[2].pos.z =
        g_sprite_temp_buffer[3].pos.z = screen.z;
    __asm {
        fld angle
        fsincos
        fstp cosine
        fstp sine
    }
    f32 x0, x1, x2, x3;
    f32 y0, y1, y2, y3;
    switch ((vm->flags_lo >> 21) & 3)
    {
    case 0:
        x0 = x2 = width * -0.5f;
        x1 = x3 = width * 0.5f;
        break;
    case 1:
        x0 = x2 = 0.0f;
        x1 = x3 = width;
        break;
    case 2:
        x0 = x2 = -width;
        x1 = x3 = 0.0f;
        break;
    }
    switch ((vm->flags_lo >> 23) & 3)
    {
    case 0:
        y0 = y1 = height * -0.5f;
        y2 = y3 = height * 0.5f;
        break;
    case 1:
        y0 = y1 = 0.0f;
        y2 = y3 = height;
        break;
    case 2:
        y0 = y1 = -height;
        y2 = y3 = 0.0f;
        break;
    }
    g_sprite_temp_buffer[0].pos.x = x0 * cosine - y0 * sine + x;
    g_sprite_temp_buffer[0].pos.y = y0 * cosine + x0 * sine + y;
    g_sprite_temp_buffer[1].pos.x = x1 * cosine - y1 * sine + x;
    g_sprite_temp_buffer[1].pos.y = y1 * cosine + x1 * sine + y;
    g_sprite_temp_buffer[2].pos.x = x2 * cosine - y2 * sine + x;
    g_sprite_temp_buffer[2].pos.y = y2 * cosine + x2 * sine + y;
    g_sprite_temp_buffer[3].pos.x = x3 * cosine - y3 * sine + x;
    g_sprite_temp_buffer[3].pos.y = y3 * cosine + x3 * sine + y;
    return 0;
}

// FUNCTION: TH16 0x466820
i32 AnmManager::draw_vm__mode_6(AnmVm *vm)
{
    if (write_sprite_corners__mode_4(vm) != 0)
    {
        return -1;
    }
    Camera *camera = g_Supervisor.current_camera;
    f32 fog_begin = camera->sky.begin_distance;
    f32 fog_range = fog_begin - camera->sky.end_distance;
    D3DXVECTOR3 diff;
    diff.x = vm->entity_pos.x + vm->pos.x + vm->pos_2.x - camera->position.x;
    diff.y = vm->entity_pos.y + vm->pos.y + vm->pos_2.y - camera->position.y;
    diff.z = vm->entity_pos.z + vm->pos.z + vm->pos_2.z - camera->position.z;
    if ((vm->flags_hi & ANM_VM_LAYER_KIND_MASK) && vm->unk_5b0 == NULL)
    {
        diff.x += g_resolution_x * 0.5f;
        diff.y += (g_resolution_y - 448.0f) * 0.5f;
    }
    f32 distance = D3DXVec3Length(&diff);
    switch ((vm->flags_lo >> 17) & 3)
    {
    case 0:
    case 1: {
        ZunColor color;
        color.d3d = (vm->flags_lo & ANM_VM_COLOR_MODE_MASK) ? vm->color_2.d3d : vm->color_1.d3d;
        if (unk_1c7fd8c != 0)
        {
            color.r = color_mul(color.r, unk_1c7fd88.r);
            color.g = color_mul(color.g, unk_1c7fd88.g);
            color.b = color_mul(color.b, unk_1c7fd88.b);
            color.a = color_mul(color.a, unk_1c7fd88.a);
        }
        if (distance > fog_begin)
        {
            f32 t = (fog_begin - distance) / fog_range;
            if (t >= 1.0f)
            {
                return -1;
            }
            camera = g_Supervisor.current_camera;
            diffuse_of(0)->b = color.b - (i32)((color.b - (i32)camera->sky.color_components[0]) * t);
            diffuse_of(0)->g = color.g - (i32)((color.g - (i32)camera->sky.color_components[1]) * t);
            diffuse_of(0)->r = color.r - (i32)((color.r - (i32)camera->sky.color_components[2]) * t);
            diffuse_of(0)->a = (i32)((1.0f - t * t * t) * color.a);
            g_sprite_temp_buffer[1].diffuse = g_sprite_temp_buffer[0].diffuse;
            g_sprite_temp_buffer[2].diffuse = g_sprite_temp_buffer[0].diffuse;
            g_sprite_temp_buffer[3].diffuse = g_sprite_temp_buffer[0].diffuse;
            return render_sprite_2d(vm, 2);
        }
        g_sprite_temp_buffer[0].diffuse = color.d3d;
        g_sprite_temp_buffer[1].diffuse = color.d3d;
        g_sprite_temp_buffer[2].diffuse = color.d3d;
        g_sprite_temp_buffer[3].diffuse = color.d3d;
        return render_sprite_2d(vm, 2);
    }
    default: {
        ZunColor color_1 = vm->color_1;
        ZunColor color_2 = vm->color_2;
        if (unk_1c7fd8c != 0)
        {
            color_1.r = color_mul(color_1.r, unk_1c7fd88.r);
            color_1.g = color_mul(color_1.g, unk_1c7fd88.g);
            color_1.b = color_mul(color_1.b, unk_1c7fd88.b);
            color_1.a = color_mul(color_1.a, unk_1c7fd88.a);
            color_2.r = color_mul(color_2.r, unk_1c7fd88.r);
            color_2.g = color_mul(color_2.g, unk_1c7fd88.g);
            color_2.b = color_mul(color_2.b, unk_1c7fd88.b);
            color_2.a = color_mul(color_2.a, unk_1c7fd88.a);
        }
        if (distance > fog_begin)
        {
            f32 t = (fog_begin - distance) / fog_range;
            if (t >= 1.0f)
            {
                return -1;
            }
            camera = g_Supervisor.current_camera;
            diffuse_of(0)->b = color_1.b - (i32)((color_1.b - (i32)camera->sky.color_components[0]) * t);
            diffuse_of(0)->g = color_1.g - (i32)((color_1.g - (i32)camera->sky.color_components[1]) * t);
            diffuse_of(0)->r = color_1.r - (i32)((color_1.r - (i32)camera->sky.color_components[2]) * t);
            diffuse_of(0)->a = (i32)(color_1.a * (1.0f - t));
            diffuse_of(3)->b = color_2.b - (i32)((color_2.b - (i32)camera->sky.color_components[0]) * t);
            diffuse_of(3)->g = color_2.g - (i32)((color_2.g - (i32)camera->sky.color_components[1]) * t);
            diffuse_of(3)->r = color_2.r - (i32)((color_2.r - (i32)camera->sky.color_components[2]) * t);
            diffuse_of(3)->a = (i32)(color_2.a * (1.0f - t));
        }
        else
        {
            g_sprite_temp_buffer[0].diffuse = color_1.d3d;
            g_sprite_temp_buffer[3].diffuse = color_2.d3d;
        }
        if ((vm->flags_lo & ANM_VM_COLOR_MODE_MASK) == 2 << 17)
        {
            g_sprite_temp_buffer[1].diffuse = g_sprite_temp_buffer[3].diffuse;
            g_sprite_temp_buffer[2].diffuse = g_sprite_temp_buffer[0].diffuse;
        }
        else
        {
            g_sprite_temp_buffer[1].diffuse = g_sprite_temp_buffer[0].diffuse;
            g_sprite_temp_buffer[2].diffuse = g_sprite_temp_buffer[3].diffuse;
        }
        return render_sprite_2d(vm, 2);
    }
    }
}

// FUNCTION: TH16 0x467200
i32 AnmManager::draw_vm__mode_7(AnmVm *vm)
{
    render_sub_466f00(vm);
    f32 fog_range = g_Supervisor.current_camera->sky.begin_distance - g_Supervisor.current_camera->sky.end_distance;
    ZunColor color;
    color.d3d = (vm->flags_lo & ANM_VM_COLOR_MODE_MASK) ? vm->color_2.d3d : vm->color_1.d3d;
    D3DXVECTOR4 transformed[4];
    for (i32 i = 0; i < 4; i++)
    {
        D3DXVec3Transform(&transformed[i], &quad_184fbc8[i].pos, (D3DXMATRIX *)&matrix_184f56c);
        D3DXVECTOR3 diff(transformed[i].x - g_Supervisor.current_camera->position.x,
                         transformed[i].y - g_Supervisor.current_camera->position.y,
                         transformed[i].z - g_Supervisor.current_camera->position.z);
        f32 distance = D3DXVec3Length(&diff);
        ZunColor *diffuse = diffuse_of(i);
        if (distance > g_Supervisor.current_camera->sky.begin_distance)
        {
            f32 t = (g_Supervisor.current_camera->sky.begin_distance - distance) / fog_range;
            if (t >= 1.0f)
            {
                diffuse->d3d = *(D3DCOLOR *)g_Supervisor.current_camera->sky.color;
                diffuse->a = color.a;
            }
            else
            {
                diffuse->b = color.b - (i32)((color.b - g_Supervisor.current_camera->sky.color_components[0]) * t);
                diffuse->g = color.g - (i32)((color.g - g_Supervisor.current_camera->sky.color_components[1]) * t);
                diffuse->r = color.r - (i32)((color.r - g_Supervisor.current_camera->sky.color_components[2]) * t);
                diffuse->a = color.a;
            }
        }
        else
        {
            diffuse->d3d = color.d3d;
        }
    }
    i32 result = render_sprite_2d(vm, 2);
    g_sprite_temp_buffer[0].pos.w = g_sprite_temp_buffer[1].pos.w = g_sprite_temp_buffer[2].pos.w =
        g_sprite_temp_buffer[3].pos.w = 1.0f;
    return result;
}

// Selects the VM's texture, flushing the batch first if it changes.
static inline AnmLoadedSprite *set_texture_of_vm(AnmManager *mgr, AnmVm *vm)
{
    AnmLoadedSprite *sprite = &g_AnmManager->loaded_anms[vm->anm_loaded_index]->sprites[vm->sprite_id];
    if (mgr->render_cache_184fbb0 != sprite->image_file_num_in_all)
    {
        mgr->render_cache_184fbb0 = sprite->image_file_num_in_all;
        mgr->flush_sprites();
        g_Supervisor.d3d_device->SetTexture(
            0, mgr->loaded_anms[mgr->render_cache_184fbb0 >> 8]->d3d[(u8)mgr->render_cache_184fbb0].texture);
    }
    return sprite;
}

// Sets the texture matrix for the VM's UV scrolling and scale, unless the
// last one set is still right.
static inline void set_texture_transform_of_vm(AnmManager *mgr, AnmVm *vm, AnmLoadedSprite *sprite)
{
    if (mgr->render_cache_184fbc0 != (i32)sprite || vm->uv_scroll_pos.x != 0.0f || vm->uv_scroll_pos.x != 0.0f ||
        vm->uv_scale.x != 1.0f || vm->uv_scale.y != 1.0f)
    {
        mgr->render_cache_184fbc0 = (i32)sprite;
        D3DXMATRIX texture_matrix = vm->matrix_450;
        texture_matrix._31 = vm->uv_quad_of_sprite[0].x + vm->uv_scroll_pos.x;
        texture_matrix._32 = vm->uv_quad_of_sprite[0].y + vm->uv_scroll_pos.y;
        texture_matrix._11 = vm->uv_scale.x * texture_matrix._11;
        texture_matrix._22 = vm->uv_scale.y * texture_matrix._22;
        g_Supervisor.d3d_device->SetTransform(D3DTS_TEXTURE0, &texture_matrix);
    }
}

static inline void set_color_op_modulate()
{
    if (g_AnmManager->last_color_op != 1)
    {
        g_Supervisor.d3d_device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
        g_Supervisor.d3d_device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
        g_AnmManager->last_color_op = 1;
    }
}

// FUNCTION: TH16 0x467410
i32 AnmManager::draw_vm__mode_8(AnmVm *vm)
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
    if (vm->flags_lo & 0x2000)
    {
        g_Supervisor.disable_zwrite();
    }
    else
    {
        g_Supervisor.enable_zwrite();
    }
    if (!(vm->flags_lo & 0x10000))
    {
        vm->matrix_410 = vm->matrix_3d0;
        vm->matrix_410._11 *= vm->scale_2.x * vm->scale.x;
        vm->matrix_410._22 *= vm->scale_2.y * vm->scale.y;
        vm->flags_lo &= ~ANM_VM_SCALE_CHANGED;
        switch (vm->flags_hi & ANM_VM_COORD_MODE_MASK)
        {
        case 1 << 20:
            vm->matrix_410._11 *= g_screen_coord_scale;
            vm->matrix_410._22 *= g_screen_coord_scale;
            break;
        case 2 << 20:
            vm->matrix_410._11 *= g_screen_coord_scale * 0.5f;
            vm->matrix_410._22 *= g_screen_coord_scale * 0.5f;
            break;
        }
        Float3 rotation = *vm->get_total_rotation();
        D3DXMATRIX rotation_matrix;
        switch ((vm->flags_hi >> 2) & 7)
        {
        case 0:
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
            break;
        case 1:
            if (rotation.x != 0.0f)
            {
                D3DXMatrixRotationX(&rotation_matrix, rotation.x);
                D3DXMatrixMultiply(&vm->matrix_410, &vm->matrix_410, &rotation_matrix);
            }
            if (rotation.z != 0.0f)
            {
                D3DXMatrixRotationZ(&rotation_matrix, rotation.z);
                D3DXMatrixMultiply(&vm->matrix_410, &vm->matrix_410, &rotation_matrix);
            }
            if (rotation.y != 0.0f)
            {
                D3DXMatrixRotationY(&rotation_matrix, rotation.y);
                D3DXMatrixMultiply(&vm->matrix_410, &vm->matrix_410, &rotation_matrix);
            }
            break;
        case 2:
            if (rotation.y != 0.0f)
            {
                D3DXMatrixRotationY(&rotation_matrix, rotation.y);
                D3DXMatrixMultiply(&vm->matrix_410, &vm->matrix_410, &rotation_matrix);
            }
            if (rotation.x != 0.0f)
            {
                D3DXMatrixRotationX(&rotation_matrix, rotation.x);
                D3DXMatrixMultiply(&vm->matrix_410, &vm->matrix_410, &rotation_matrix);
            }
            if (rotation.z != 0.0f)
            {
                D3DXMatrixRotationZ(&rotation_matrix, rotation.z);
                D3DXMatrixMultiply(&vm->matrix_410, &vm->matrix_410, &rotation_matrix);
            }
            break;
        case 3:
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
            if (rotation.x != 0.0f)
            {
                D3DXMatrixRotationX(&rotation_matrix, rotation.x);
                D3DXMatrixMultiply(&vm->matrix_410, &vm->matrix_410, &rotation_matrix);
            }
            break;
        case 4:
            if (rotation.z != 0.0f)
            {
                D3DXMatrixRotationZ(&rotation_matrix, rotation.z);
                D3DXMatrixMultiply(&vm->matrix_410, &vm->matrix_410, &rotation_matrix);
            }
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
            break;
        case 5:
            if (rotation.z != 0.0f)
            {
                D3DXMatrixRotationZ(&rotation_matrix, rotation.z);
                D3DXMatrixMultiply(&vm->matrix_410, &vm->matrix_410, &rotation_matrix);
            }
            if (rotation.y != 0.0f)
            {
                D3DXMatrixRotationY(&rotation_matrix, rotation.y);
                D3DXMatrixMultiply(&vm->matrix_410, &vm->matrix_410, &rotation_matrix);
            }
            if (rotation.x != 0.0f)
            {
                D3DXMatrixRotationX(&rotation_matrix, rotation.x);
                D3DXMatrixMultiply(&vm->matrix_410, &vm->matrix_410, &rotation_matrix);
            }
            break;
        }
        vm->flags_lo &= ~ANM_VM_ROTATION_CHANGED;
    }
    D3DXMATRIX world = vm->matrix_410;
    world._41 = vm->entity_pos.x + vm->pos.x + vm->pos_2.x - vm->anchor_offset.x * vm->scale.x * vm->scale_2.x;
    world._42 = vm->entity_pos.y + vm->pos.y + vm->pos_2.y - vm->anchor_offset.y * vm->scale.y * vm->scale_2.y;
    vm->transform_coords((Float3 *)&world._41);
    setup_render_state_for_vm(vm);
    ZunColor color;
    color.d3d = (vm->flags_lo & ANM_VM_COLOR_MODE_MASK) ? vm->color_2.d3d : vm->color_1.d3d;
    if (unk_1c7fd8c != 0)
    {
        color.r = color_mul(color.r, unk_1c7fd88.r);
        color.g = color_mul(color.g, unk_1c7fd88.g);
        color.b = color_mul(color.b, unk_1c7fd88.b);
        color.a = color_mul(color.a, unk_1c7fd88.a);
    }
    if (last_texture_factor != color.d3d)
    {
        flush_sprites();
        last_texture_factor = color.d3d;
        g_Supervisor.d3d_device->SetRenderState(D3DRS_TEXTUREFACTOR, color.d3d);
    }
    world._43 = vm->entity_pos.z + vm->pos.z + vm->pos_2.z;
    g_Supervisor.d3d_device->SetTransform(D3DTS_WORLD, &world);
    AnmLoadedSprite *sprite = set_texture_of_vm(this, vm);
    set_texture_transform_of_vm(this, vm, sprite);
    if (render_cache_184fbb6 != 2)
    {
        g_Supervisor.d3d_device->SetStreamSource(0, vertex_buffer, 0, sizeof(RenderVertexXyzTex));
        g_Supervisor.d3d_device->SetFVF(D3DFVF_XYZ | D3DFVF_TEX1);
        g_Supervisor.d3d_device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_TFACTOR);
        g_Supervisor.d3d_device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_TFACTOR);
        render_cache_184fbb6 = 2;
    }
    set_color_op_modulate();
    g_Supervisor.d3d_device->DrawPrimitive(
        D3DPT_TRIANGLESTRIP, (((vm->flags_lo >> 23) & 3) * 3 + ((vm->flags_lo >> 21) & 3)) * 4, 2);
    return 0;
}

// FUNCTION: TH16 0x467d00
i32 AnmManager::draw_vm__mode_24(AnmVm *vm, RenderVertexXyzDiffuseTex *vertices, i32 vertex_count)
{
    if (!(vm->flags_lo & ANM_VM_VISIBLE))
    {
        return -1;
    }
    if (!(vm->flags_lo & ANM_VM_FLAG_LO_2))
    {
        return -1;
    }
    if (unrendered_sprite_count != 0)
    {
        flush_sprites();
    }
    if (!(vm->flags_lo & 0x2000))
    {
        g_Supervisor.enable_zwrite();
    }
    else
    {
        g_Supervisor.disable_zwrite();
    }
    D3DXMatrixIdentity(&vm->matrix_3d0);
    vm->matrix_410 = vm->matrix_3d0;
    vm->matrix_410._11 *= vm->scale_2.x * vm->scale.x;
    vm->matrix_410._22 *= vm->scale_2.y * vm->scale.y;
    vm->flags_lo &= ~ANM_VM_SCALE_CHANGED;
    D3DXMATRIX rotation_matrix;
    if (vm->rotation.x != 0.0f)
    {
        D3DXMatrixRotationX(&rotation_matrix, vm->rotation.x);
        D3DXMatrixMultiply(&vm->matrix_410, &vm->matrix_410, &rotation_matrix);
    }
    if (vm->rotation.y != 0.0f)
    {
        D3DXMatrixRotationY(&rotation_matrix, vm->rotation.y);
        D3DXMatrixMultiply(&vm->matrix_410, &vm->matrix_410, &rotation_matrix);
    }
    if (vm->rotation.z != 0.0f)
    {
        D3DXMatrixRotationZ(&rotation_matrix, vm->rotation.z);
        D3DXMatrixMultiply(&vm->matrix_410, &vm->matrix_410, &rotation_matrix);
    }
    vm->flags_lo &= ~ANM_VM_ROTATION_CHANGED;
    D3DXMATRIX world = vm->matrix_410;
    world._41 = vm->entity_pos.x + vm->pos.x + vm->pos_2.x;
    if ((vm->flags_hi & ANM_VM_LAYER_KIND_MASK) && vm->unk_5b0 == NULL)
    {
        world._41 += g_game_2d_origin_x;
    }
    world._42 = vm->entity_pos.y + vm->pos.y + vm->pos_2.y;
    world._43 = vm->entity_pos.z + vm->pos.z + vm->pos_2.z;
    g_Supervisor.d3d_device->SetTransform(D3DTS_WORLD, &world);
    setup_render_state_for_vm(vm);
    AnmLoadedSprite *sprite = set_texture_of_vm(this, vm);
    set_texture_transform_of_vm(this, vm, sprite);
    if (render_cache_184fbb6 != 5)
    {
        g_Supervisor.d3d_device->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1);
        g_Supervisor.d3d_device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
        g_Supervisor.d3d_device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
        render_cache_184fbb6 = 5;
    }
    set_color_op_modulate();
    g_Supervisor.d3d_device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, vertex_count - 2, vertices,
                                             sizeof(RenderVertexXyzDiffuseTex));
    return 0;
}

// Whether a VM drawn as a sprite would be invisible: both colors have zero
// alpha.
static __forceinline BOOL is_transparent(AnmVm *vm)
{
    return vm->color_1.a == 0 && vm->color_2.a == 0;
}

// FUNCTION: TH16 0x468490
HARNESS_CALLED i32 AnmManager::draw_vm(AnmVm *vm)
{
    if (vm->index_of_on_draw != 0)
    {
        g_anm_on_draw_funcs[vm->index_of_on_draw](vm);
    }
    if (!(vm->flags_lo & ANM_VM_VISIBLE))
    {
        return -1;
    }
    if (!(vm->flags_lo & ANM_VM_FLAG_LO_2))
    {
        return -1;
    }
    if (vm->flags_hi & (ANM_VM_DELETE_PENDING | ANM_VM_FLAG_HI_40))
    {
        return -1;
    }
    g_Supervisor.disable_zwrite();
    switch ((vm->flags_lo >> ANM_VM_RENDER_MODE_SHIFT) & 0x1f)
    {
    case 0:
        if (is_transparent(vm))
        {
            return -1;
        }
        AnmVm::write_sprite_corners__without_rot(
            vm, (Float3 *)&g_sprite_temp_buffer[0].pos, (Float3 *)&g_sprite_temp_buffer[1].pos,
            (Float3 *)&g_sprite_temp_buffer[2].pos, (Float3 *)&g_sprite_temp_buffer[3].pos);
        return render_sprite_2d(vm, 1);
    case 1:
    case 3:
        if (is_transparent(vm))
        {
            return -1;
        }
        AnmVm::write_sprite_corners__with_z_rot(
            vm, (Float3 *)&g_sprite_temp_buffer[0].pos, (Float3 *)&g_sprite_temp_buffer[1].pos,
            (Float3 *)&g_sprite_temp_buffer[2].pos, (Float3 *)&g_sprite_temp_buffer[3].pos);
        return render_sprite_2d(vm, 0);
    case 4:
        if (is_transparent(vm))
        {
            return -1;
        }
        if (write_sprite_corners__mode_4(vm) != 0)
        {
            return -1;
        }
        return render_sprite_2d(vm, 0);
    case 5:
        if (is_transparent(vm))
        {
            return -1;
        }
        return draw_vm__mode_5(vm);
    case 6:
        if (is_transparent(vm))
        {
            return -1;
        }
        return draw_vm__mode_6(vm);
    case 7:
        if (is_transparent(vm))
        {
            return -1;
        }
        return draw_vm__mode_7(vm);
    case 8:
        if (is_transparent(vm))
        {
            return -1;
        }
        return draw_vm__mode_8(vm);
    case 15:
        if (is_transparent(vm))
        {
            return -1;
        }
        g_Supervisor.enable_d3d_fog();
        draw_vm__mode_8(vm);
        g_Supervisor.disable_d3d_fog();
        return 0;
    case 9:
    case 12:
    case 13:
    case 14:
        return draw_vm__mode_9(vm, (RenderVertex144 *)vm->ins_508_extra_data, vm->int_vars[0] * 2);
    case 11:
        return draw_vm__mode_11(vm, (RenderVertex144 *)vm->ins_508_extra_data, vm->int_vars[0] * 2);
    case 24:
    case 25:
        return draw_vm__mode_24(vm, (RenderVertexXyzDiffuseTex *)vm->ins_508_extra_data, vm->int_vars[0] * 2);
    case 2:
        if (is_transparent(vm))
        {
            return -1;
        }
        AnmVm::write_sprite_corners__without_rot(
            vm, (Float3 *)&g_sprite_temp_buffer[0].pos, (Float3 *)&g_sprite_temp_buffer[1].pos,
            (Float3 *)&g_sprite_temp_buffer[2].pos, (Float3 *)&g_sprite_temp_buffer[3].pos);
        return render_sprite_2d(vm, 0);
    case 16:
    case 20:
    case 21:
    case 22:
    case 26:
    case 27: {
        f32 angle = vm->rotation.z;
        Float2 size;
        size.x = vm->sprite_size.x * vm->scale.x;
        size.y = vm->sprite_size.y * vm->scale.y;
        Float3 pos;
        vm->get_own_transformed_pos(&pos);
        if (vm->unk_5b0 != NULL && !(vm->flags_hi & ANM_VM_NO_PARENT_POS))
        {
            size.x = vm->unk_5b0->scale.x * size.x;
            size.y = vm->unk_5b0->scale.y * size.y;
            angle = vm->unk_5b0->rotation.z + angle;
        }
        setup_render_state_for_vm(vm);
        if ((vm->flags_hi & ANM_VM_COORD_MODE_MASK) == 1 << 20)
        {
            size.x = g_screen_coord_scale * size.x;
            size.y = g_screen_coord_scale * size.y;
        }
        else if ((vm->flags_hi & ANM_VM_COORD_MODE_MASK) == 2 << 20)
        {
            size.x = g_screen_coord_scale * 0.5f * size.x;
            size.y = g_screen_coord_scale * 0.5f * size.y;
        }
        i32 anchor_x = (vm->flags_lo >> 21) & 3;
        i32 anchor_y = (vm->flags_lo >> 23) & 3;
        switch ((vm->flags_lo >> ANM_VM_RENDER_MODE_SHIFT) & 0x1f)
        {
        case 26:
            draw_line(pos.x, pos.y, size.x, angle, vm->color_1.d3d,
                      (vm->flags_lo & ANM_VM_COLOR_MODE_MASK) ? vm->color_2.d3d : vm->color_1.d3d, anchor_x, 0);
            return 0;
        case 16:
            draw_rect(pos.x, pos.y, size.x, size.y, angle, vm->color_1.d3d, vm->color_1.d3d, anchor_x, anchor_y);
            return 0;
        case 27:
            draw_rect_outline(pos.x, pos.y, size.x, size.y, angle, vm->color_1.d3d,
                              (vm->flags_lo & ANM_VM_COLOR_MODE_MASK) ? vm->color_2.d3d : vm->color_1.d3d,
                              anchor_x, anchor_y);
            return 0;
        case 20:
            draw_rect(pos.x, pos.y, size.x, size.y, angle, vm->color_1.d3d, vm->color_2.d3d, anchor_x, anchor_y);
            return 0;
        case 21:
            draw_rect_bordered(pos.x, pos.y, size.x, size.y, angle, vm->color_1.d3d, vm->color_1.d3d, anchor_x,
                               anchor_y);
            return 0;
        case 22:
            draw_rect_bordered(pos.x, pos.y, size.x, size.y, angle, vm->color_1.d3d, vm->color_2.d3d, anchor_x,
                               anchor_y);
            return 0;
        }
        break;
    }
    case 17:
    case 18:
    case 19: {
        Float2 size;
        size.x = vm->sprite_size.x * vm->scale.x;
        size.y = vm->sprite_size.y * vm->scale.y;
        f32 angle = vm->rotation.z;
        Float3 pos;
        vm->get_own_transformed_pos(&pos);
        if (vm->unk_5b0 != NULL && !(vm->flags_hi & ANM_VM_NO_PARENT_POS))
        {
            angle = vm->unk_5b0->rotation.z + angle;
            size.x *= vm->unk_5b0->scale.x;
            size.y *= vm->unk_5b0->scale.y;
        }
        if ((vm->flags_hi & ANM_VM_COORD_MODE_MASK) == 1 << 20)
        {
            size.x = g_screen_coord_scale * size.x;
            size.y = g_screen_coord_scale * size.y;
        }
        else if ((vm->flags_hi & ANM_VM_COORD_MODE_MASK) == 2 << 20)
        {
            size.x = g_screen_coord_scale * 0.5f * size.x;
            size.y = g_screen_coord_scale * 0.5f * size.y;
        }
        setup_render_state_for_vm(vm);
        switch ((vm->flags_lo >> ANM_VM_RENDER_MODE_SHIFT) & 0x1f)
        {
        case 19:
            draw_ring(pos.x, pos.y, size.x, size.y, angle, vm->int_vars[0], vm->color_1.d3d);
            return 0;
        case 18:
            draw_circle_outline(pos.x, pos.y, size.x, angle, vm->int_vars[0], vm->color_1.d3d);
            return 0;
        case 17:
            draw_circle(pos.x, pos.y, size.x, angle, vm->int_vars[0], vm->color_1.d3d, vm->color_2.d3d);
            return 0;
        }
        break;
    }
    }
    return 0;
}
