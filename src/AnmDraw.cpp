// AnmManager::draw_vm and the render modes it dispatches to.
#include <stddef.h>

#include "AnmManager.h"
#include "Supervisor.h"
#include "ZunAsm.h"

static_assert(offsetof(AnmManager, last_texture_factor) == 0x184fbac, "AnmManager layout");
static_assert(offsetof(AnmManager, fog_unit_quad) == 0x184fbc8, "AnmManager layout");
static_assert(sizeof(RenderVertexXyzTex) == 0x14, "RenderVertexXyzTex layout");
static_assert(sizeof(RenderVertexXyzDiffuseTex) == 0x18, "RenderVertexXyzDiffuseTex layout");

// Corner offsets of a sprite in units of its size, by anchoring (center,
// left/top, right/bottom), for the corners in g_sprite_temp_buffer order.
struct AnmAnchorCorners
{
    f32 corner[4];
};

// At 0x4a3048, right after g_sound_effect_table.
// GLOBAL: TH16 0x4a3048
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

// The 0.5f render_sprite_2d's pixel snapping subtracts after rounding; a
// constant of its own in the original, apart from the compiler's 0.5f.
// GLOBAL: TH16 0x4941d8
static const f32 g_pixel_center_offset = 0.5f;

// The quad in g_sprite_temp_buffer, moved by the camera offset, rounded to
// pixel centers if flags bit 0 is set, culled against the viewport and
// colored by the VM's color mode unless flags bit 1 is set.
// TODO: 73%; the original keeps this on the stack and the parent-colored channels in dword stack slots.
// FUNCTION: TH16 0x465280
i32 AnmManager::render_sprite_2d(AnmVm *vm, i32 flags)
{
    g_sprite_temp_buffer[0].pos.x += camera_2d_offset.x;
    g_sprite_temp_buffer[0].pos.y += camera_2d_offset.y;
    g_sprite_temp_buffer[1].pos.x += camera_2d_offset.x;
    g_sprite_temp_buffer[1].pos.y += camera_2d_offset.y;
    g_sprite_temp_buffer[2].pos.x += camera_2d_offset.x;
    g_sprite_temp_buffer[2].pos.y += camera_2d_offset.y;
    g_sprite_temp_buffer[3].pos.x += camera_2d_offset.x;
    g_sprite_temp_buffer[3].pos.y += camera_2d_offset.y;
    if (flags & ANM_SPRITE_SNAP_TO_PIXELS)
    {
        ZUN_ASM_SNAP_QUAD_TO_PIXEL_CENTERS(g_sprite_temp_buffer, g_pixel_center_offset);
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
    if (last_texture_id != texture)
    {
        last_texture_id = texture;
        flush_sprites();
        g_Supervisor.d3d_device->SetTexture(
            0, loaded_anms[last_texture_id >> 8]->d3d[(u8)last_texture_id].texture);
    }
    if (last_vertex_setup != ANM_VERTEX_SETUP_DIFFUSE)
    {
        flush_sprites();
        last_vertex_setup = ANM_VERTEX_SETUP_DIFFUSE;
    }
    if (!(flags & ANM_SPRITE_KEEP_COLORS))
    {
        switch ((vm->flags_lo >> ANM_VM_COLOR_MODE_SHIFT) & 3)
        {
        case 0:
        case 1: {
            ZunColor color;
            color.d3d = (vm->flags_lo & ANM_VM_COLOR_MODE_MASK) ? vm->color_2.d3d : vm->color_1.d3d;
            u8 r, g, b, a;
            if ((vm->flags_hi & ANM_VM_COLORIZE_CHILDREN) && vm->parent_vm != NULL)
            {
                ZunColor parent = vm->parent_vm->mixed_inherited_color;
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
            if (global_tint_enabled != 0)
            {
                color.r = color_mul(r, global_tint.r);
                color.g = color_mul(g, global_tint.g);
                color.b = color_mul(b, global_tint.b);
                color.a = color_mul(a, global_tint.a);
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
            if (global_tint_enabled != 0)
            {
                color_1.r = color_mul(color_1.r, global_tint.r);
                color_1.g = color_mul(color_1.g, global_tint.g);
                color_1.b = color_mul(color_1.b, global_tint.b);
                color_1.a = color_mul(color_1.a, global_tint.a);
                color_2.r = color_mul(color_2.r, global_tint.r);
                color_2.g = color_mul(color_2.g, global_tint.g);
                color_2.b = color_mul(color_2.b, global_tint.b);
                color_2.a = color_mul(color_2.a, global_tint.a);
            }
            g_sprite_temp_buffer[3].diffuse = color_2.d3d;
            if ((vm->flags_lo & ANM_VM_COLOR_MODE_MASK) == ANM_VM_COLOR_MODE_2)
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
// The resolution scaling is an if/else-if chain: as a switch the two cases
// were laid out the other way round.
// The VM is read through a local copy of the pointer: LTCG orders the
// operands of the multiplies differently for loads through a parameter
// (the same as in AnmVm::transform_coords).
// TODO: 91%; the d corner's x size multiply, every other corner's y screen and scale multiply (a, c) and the position adds still take their operands in the other order.
// FUNCTION: TH16 0x465c40
void __stdcall AnmVm::write_sprite_corners__without_rot(AnmVm *vm_param, Float3 *a, Float3 *b, Float3 *c, Float3 *d)
{
    AnmVm *vm = vm_param;
    AnmAnchorCorners *anchor = &g_anchor_corners_x[(vm->flags_lo >> ANM_VM_ANCHOR_X_SHIFT) & 3];
    a->x = anchor->corner[0];
    b->x = anchor->corner[1];
    c->x = anchor->corner[2];
    d->x = anchor->corner[3];
    anchor = &g_anchor_corners_y[(vm->flags_lo >> ANM_VM_ANCHOR_Y_SHIFT) & 3];
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
    if ((vm->flags_hi & ANM_VM_RESOLUTION_MODE_MASK) == ANM_VM_RESOLUTION_SCALED)
    {
        a->x *= g_screen_coord_scale;
        b->x *= g_screen_coord_scale;
        c->x *= g_screen_coord_scale;
        d->x *= g_screen_coord_scale;
        a->y *= g_screen_coord_scale;
        b->y *= g_screen_coord_scale;
        c->y *= g_screen_coord_scale;
        d->y *= g_screen_coord_scale;
    }
    else if ((vm->flags_hi & ANM_VM_RESOLUTION_MODE_MASK) == ANM_VM_RESOLUTION_HALF_SCALED)
    {
        a->x *= g_screen_coord_scale * 0.5f;
        b->x *= g_screen_coord_scale * 0.5f;
        c->x *= g_screen_coord_scale * 0.5f;
        d->x *= g_screen_coord_scale * 0.5f;
        a->y *= g_screen_coord_scale * 0.5f;
        b->y *= g_screen_coord_scale * 0.5f;
        c->y *= g_screen_coord_scale * 0.5f;
        d->y *= g_screen_coord_scale * 0.5f;
    }
    f32 scale_x = vm->scale_2.x * vm->scale.x;
    f32 scale_y = vm->scale_2.y * vm->scale.y;
    if (vm->parent_vm != NULL && !(vm->flags_hi & ANM_VM_NO_PARENT_POS))
    {
        scale_x *= vm->parent_vm->scale_2.x * vm->parent_vm->scale.x;
        scale_y *= vm->parent_vm->scale_2.y * vm->parent_vm->scale.y;
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
// VM's total z rotation. The corner offsets are plain arrays filled element
// by element: as AnmAnchorCorners struct copies, LTCG keeps them 16-byte
// aligned and the function realigns its frame (and esp, -16), which the
// original does not; as arrays they keep the original's /GS cookie and frame.
// TODO: 71%; the original copies each table row with one movups (ours: four scalar copies), and the x and y offsets (and scale_x/scale_y) trade registers and stack slots.
// FUNCTION: TH16 0x4660b0
void __stdcall AnmVm::write_sprite_corners__with_z_rot(AnmVm *vm, Float3 *a, Float3 *b, Float3 *c, Float3 *d)
{
    f32 angle = vm->get_total_rotation()->z;
    f32 sine;
    f32 cosine;
    ZUN_ASM_SINCOS(angle, sine, cosine);
    AnmAnchorCorners *xt = &g_anchor_corners_x[(vm->flags_lo >> ANM_VM_ANCHOR_X_SHIFT) & 3];
    AnmAnchorCorners *yt = &g_anchor_corners_y[(vm->flags_lo >> ANM_VM_ANCHOR_Y_SHIFT) & 3];
    f32 xs[4];
    f32 ys[4];
    for (i32 j = 0; j < 4; j++)
    {
        xs[j] = xt->corner[j];
        ys[j] = yt->corner[j];
    }
    i32 i;
    for (i = 0; i < 4; i++)
    {
        xs[i] = xs[i] * vm->sprite_size.x - vm->anchor_offset.x;
        ys[i] = ys[i] * vm->sprite_size.y - vm->anchor_offset.y;
    }
    if ((vm->flags_hi & ANM_VM_RESOLUTION_MODE_MASK) == ANM_VM_RESOLUTION_SCALED)
    {
        for (i = 0; i < 4; i++)
        {
            xs[i] *= g_screen_coord_scale;
            ys[i] *= g_screen_coord_scale;
        }
    }
    else if ((vm->flags_hi & ANM_VM_RESOLUTION_MODE_MASK) == ANM_VM_RESOLUTION_HALF_SCALED)
    {
        for (i = 0; i < 4; i++)
        {
            xs[i] *= g_screen_coord_scale * 0.5f;
            ys[i] *= g_screen_coord_scale * 0.5f;
        }
    }
    Float3 pos;
    vm->get_own_transformed_pos(&pos);
    f32 scale_x = vm->scale_2.x * vm->scale.x;
    f32 scale_y = vm->scale_2.y * vm->scale.y;
    if (vm->parent_vm != NULL && !(vm->flags_hi & ANM_VM_NO_PARENT_POS))
    {
        scale_x *= vm->parent_vm->scale_2.x * vm->parent_vm->scale.x;
        scale_y *= vm->parent_vm->scale_2.y * vm->parent_vm->scale.y;
    }
    for (i = 0; i < 4; i++)
    {
        ys[i] *= scale_y;
        xs[i] *= scale_x;
    }
    a->x = xs[0] * cosine - ys[0] * sine + pos.x;
    a->y = ys[0] * cosine + xs[0] * sine + pos.y;
    b->x = xs[1] * cosine - ys[1] * sine + pos.x;
    b->y = ys[1] * cosine + xs[1] * sine + pos.y;
    c->x = xs[2] * cosine - ys[2] * sine + pos.x;
    c->y = ys[2] * cosine + xs[2] * sine + pos.y;
    d->x = xs[3] * cosine - ys[3] * sine + pos.x;
    d->y = ys[3] * cosine + xs[3] * sine + pos.y;
    a->z = b->z = c->z = d->z = vm->entity_pos.z + vm->pos.z + vm->pos_2.z;
}

// Corners of a billboard: the VM's world position projected to the screen,
// sized by how far the camera's right vector projects from it.
// TODO: 83%; register allocation of the corner offsets differs.
// FUNCTION: TH16 0x466390
i32 __stdcall AnmManager::write_billboard_corners(AnmVm *vm)
{
    f32 angle = vm->get_total_rotation()->z;
    f32 sine;
    f32 cosine;
    ZUN_ASM_SINCOS(angle, sine, cosine);
    D3DXVECTOR3 origin(0.0f, 0.0f, 0.0f);
    D3DXMATRIX world;
    D3DXMatrixIdentity(&world);
    world._41 = vm->entity_pos.x + vm->pos.x + vm->pos_2.x;
    world._42 = vm->entity_pos.y + vm->pos.y + vm->pos_2.y;
    world._43 = vm->entity_pos.z + vm->pos.z + vm->pos_2.z;
    D3DXVECTOR3 screen;
    Camera *camera = g_Supervisor.current_camera;
    D3DXVec3Project(&screen, &origin, &camera->viewport, (D3DXMATRIX *)&camera->projection_matrix, (D3DXMATRIX *)&camera->view_matrix, &world);
    if (screen.z < 0.0f || screen.z > 1.0f)
    {
        return -1;
    }
    D3DXVECTOR3 screen_2;
    camera = g_Supervisor.current_camera;
    D3DXVec3Project(&screen_2, &camera->right, &camera->viewport, (D3DXMATRIX *)&camera->projection_matrix, (D3DXMATRIX *)&camera->view_matrix,
                    &world);
    f32 x = screen.x;
    f32 y = screen.y;
    D3DXVECTOR3 diff;
    diff.y = screen_2.y - y;
    diff.x = screen_2.x - x;
    diff.z = screen_2.z - screen.z;
    f32 scale = D3DXVec3Length(&diff) * 0.5f;
    f32 width = vm->sprite_size.x * scale * vm->scale.x * vm->scale_2.x;
    f32 height = vm->sprite_size.y * scale * vm->scale.y * vm->scale_2.y;
    g_sprite_temp_buffer[0].pos.z = g_sprite_temp_buffer[1].pos.z = g_sprite_temp_buffer[2].pos.z =
        g_sprite_temp_buffer[3].pos.z = screen.z;
    ZUN_ASM_SINCOS(angle, sine, cosine);
    f32 x0, x1, x2, x3;
    f32 y0, y1, y2, y3;
    switch ((vm->flags_lo >> ANM_VM_ANCHOR_X_SHIFT) & 3)
    {
    case ANM_ANCHOR_CENTER:
        x0 = x2 = width * -0.5f;
        x1 = x3 = width * 0.5f;
        break;
    case ANM_ANCHOR_START:
        x0 = x2 = 0.0f;
        x1 = x3 = width;
        break;
    case ANM_ANCHOR_END:
        x0 = x2 = -width;
        x1 = x3 = 0.0f;
        break;
    }
    switch ((vm->flags_lo >> ANM_VM_ANCHOR_Y_SHIFT) & 3)
    {
    case ANM_ANCHOR_CENTER:
        y0 = y1 = height * -0.5f;
        y2 = y3 = height * 0.5f;
        break;
    case ANM_ANCHOR_START:
        y0 = y1 = 0.0f;
        y2 = y3 = height;
        break;
    case ANM_ANCHOR_END:
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

// The VM is read through a local copy of the pointer, as in
// write_sprite_corners__without_rot. The color mode switch lists all four
// values (a jump table like the original's; a default case compares
// instead). Nothing follows it: a return after the switch changes the
// layout, hence the C4715 pragma.
// TODO: 85%; the original keeps the scaled color channels in dword stack slots and sums each position component in another order.
#pragma warning(push)
#pragma warning(disable : 4715)
// FUNCTION: TH16 0x466820
i32 AnmManager::draw_billboard_fog(AnmVm *vm_param)
{
    AnmVm *vm = vm_param;
    if (write_billboard_corners(vm) != 0)
    {
        return -1;
    }
    Camera *camera = g_Supervisor.current_camera;
    f32 fog_begin = camera->sky.begin_distance;
    f32 fog_range = fog_begin - camera->sky.end_distance;
    D3DXVECTOR3 diff = vm->entity_pos + vm->pos + vm->pos_2 - camera->position;
    if ((vm->flags_hi & ANM_VM_ORIGIN_MODE_MASK) && vm->parent_vm == NULL)
    {
        diff.x += g_resolution_x * 0.5f;
        diff.y += (g_resolution_y - 448.0f) * 0.5f;
    }
    f32 distance = D3DXVec3Length(&diff);
    switch ((vm->flags_lo >> ANM_VM_COLOR_MODE_SHIFT) & 3)
    {
    case 0:
    case 1: {
        ZunColor color;
        color.d3d = !(vm->flags_lo & ANM_VM_COLOR_MODE_MASK) ? vm->color_1.d3d : vm->color_2.d3d;
        if (global_tint_enabled != 0)
        {
            color.r = color_mul(color.r, global_tint.r);
            color.g = color_mul(color.g, global_tint.g);
            color.b = color_mul(color.b, global_tint.b);
            color.a = color_mul(color.a, global_tint.a);
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
            return render_sprite_2d(vm, ANM_SPRITE_KEEP_COLORS);
        }
        g_sprite_temp_buffer[0].diffuse = color.d3d;
        g_sprite_temp_buffer[1].diffuse = color.d3d;
        g_sprite_temp_buffer[2].diffuse = color.d3d;
        g_sprite_temp_buffer[3].diffuse = color.d3d;
        return render_sprite_2d(vm, ANM_SPRITE_KEEP_COLORS);
    }
    case 2:
    case 3: {
        ZunColor color_1 = vm->color_1;
        ZunColor color_2 = vm->color_2;
        if (global_tint_enabled != 0)
        {
            color_1.r = color_mul(color_1.r, global_tint.r);
            color_1.g = color_mul(color_1.g, global_tint.g);
            color_1.b = color_mul(color_1.b, global_tint.b);
            color_1.a = color_mul(color_1.a, global_tint.a);
            color_2.r = color_mul(color_2.r, global_tint.r);
            color_2.g = color_mul(color_2.g, global_tint.g);
            color_2.b = color_mul(color_2.b, global_tint.b);
            color_2.a = color_mul(color_2.a, global_tint.a);
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
        if ((vm->flags_lo & ANM_VM_COLOR_MODE_MASK) == ANM_VM_COLOR_MODE_2)
        {
            g_sprite_temp_buffer[1].diffuse = g_sprite_temp_buffer[3].diffuse;
            g_sprite_temp_buffer[2].diffuse = g_sprite_temp_buffer[0].diffuse;
        }
        else
        {
            g_sprite_temp_buffer[1].diffuse = g_sprite_temp_buffer[0].diffuse;
            g_sprite_temp_buffer[2].diffuse = g_sprite_temp_buffer[3].diffuse;
        }
        return render_sprite_2d(vm, ANM_SPRITE_KEEP_COLORS);
    }
    }
}
#pragma warning(pop)

// The differences assigned x, z, y give the original's registers for them.
// TODO: the original subtracts y after x and sums x + y (ours y first, y + x); the loop end compares with g_sprite_temp_buffer's end, which our data layout follows with another global.
// FUNCTION: TH16 0x467200
i32 AnmManager::draw_sprite_fog(AnmVm *vm)
{
    build_world_matrix(vm);
    f32 fog_range = g_Supervisor.current_camera->sky.begin_distance - g_Supervisor.current_camera->sky.end_distance;
    ZunColor color;
    color.d3d = (vm->flags_lo & ANM_VM_COLOR_MODE_MASK) ? vm->color_2.d3d : vm->color_1.d3d;
    D3DXVECTOR4 transformed[4];
    for (i32 i = 0; i < 4; i++)
    {
        D3DXVec3Transform(&transformed[i], &fog_unit_quad[i].pos, (D3DXMATRIX *)&current_world_matrix);
        D3DXVECTOR3 diff;
        diff.x = transformed[i].x - g_Supervisor.current_camera->position.x;
        diff.z = transformed[i].z - g_Supervisor.current_camera->position.z;
        diff.y = transformed[i].y - g_Supervisor.current_camera->position.y;
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
    i32 result = render_sprite_2d(vm, ANM_SPRITE_KEEP_COLORS);
    g_sprite_temp_buffer[0].pos.w = g_sprite_temp_buffer[1].pos.w = g_sprite_temp_buffer[2].pos.w =
        g_sprite_temp_buffer[3].pos.w = 1.0f;
    return result;
}

// Selects the VM's texture, flushing the batch first if it changes.
static inline AnmLoadedSprite *set_texture_of_vm(AnmManager *mgr, AnmVm *vm)
{
    AnmLoadedSprite *sprite = &g_AnmManager->loaded_anms[vm->anm_loaded_index]->sprites[vm->sprite_id];
    if (mgr->last_texture_id != sprite->image_file_num_in_all)
    {
        mgr->last_texture_id = sprite->image_file_num_in_all;
        mgr->flush_sprites();
        g_Supervisor.d3d_device->SetTexture(
            0, mgr->loaded_anms[mgr->last_texture_id >> 8]->d3d[(u8)mgr->last_texture_id].texture);
    }
    return sprite;
}

// Sets the texture matrix for the VM's UV scrolling and scale, unless the
// last one set is still right.
static inline void set_texture_transform_of_vm(AnmManager *mgr, AnmVm *vm, AnmLoadedSprite *sprite)
{
    if (mgr->last_texture_matrix_sprite != (i32)sprite || vm->uv_scroll_pos.x != 0.0f || vm->uv_scroll_pos.x != 0.0f ||
        vm->uv_scale.x != 1.0f || vm->uv_scale.y != 1.0f)
    {
        mgr->last_texture_matrix_sprite = (i32)sprite;
        D3DXMATRIX texture_matrix = vm->texture_matrix;
        texture_matrix._31 = vm->uv_quad_of_sprite[0].x + vm->uv_scroll_pos.x;
        texture_matrix._32 = vm->uv_quad_of_sprite[0].y + vm->uv_scroll_pos.y;
        texture_matrix._11 = vm->uv_scale.x * texture_matrix._11;
        texture_matrix._22 = vm->uv_scale.y * texture_matrix._22;
        g_Supervisor.d3d_device->SetTransform(D3DTS_TEXTURE0, &texture_matrix);
    }
}

static inline void set_color_op_modulate()
{
    if (g_AnmManager->last_color_op != ANM_COLOR_OP_MODULATE)
    {
        g_Supervisor.d3d_device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
        g_Supervisor.d3d_device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
        g_AnmManager->last_color_op = ANM_COLOR_OP_MODULATE;
    }
}

// HARNESS_CALLED: kept alive by draw_vm alone, it realigns to 8 (ebx form)
// instead of 16.
// TODO: 41%; the original does not realign at all (its frame is laid out for draw_vm's known 8-byte alignment), and the rotation order cases and the texture matrix copy are laid out differently.
// FUNCTION: TH16 0x467410
HARNESS_CALLED i32 AnmManager::draw_3d(AnmVm *vm)
{
    if (!(vm->flags_lo & ANM_VM_VISIBLE))
    {
        return -1;
    }
    if (!(vm->flags_lo & ANM_VM_SHOWN))
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
    if (vm->flags_lo & ANM_VM_Z_WRITE_DISABLE)
    {
        g_Supervisor.disable_zwrite();
    }
    else
    {
        g_Supervisor.enable_zwrite();
    }
    if (!(vm->flags_lo & ANM_VM_KEEP_WORLD_MATRIX))
    {
        vm->world_matrix = vm->sprite_matrix;
        vm->world_matrix._11 *= vm->scale_2.x * vm->scale.x;
        vm->world_matrix._22 *= vm->scale_2.y * vm->scale.y;
        vm->flags_lo &= ~ANM_VM_SCALE_CHANGED;
        switch (vm->flags_hi & ANM_VM_RESOLUTION_MODE_MASK)
        {
        case ANM_VM_RESOLUTION_SCALED:
            vm->world_matrix._11 *= g_screen_coord_scale;
            vm->world_matrix._22 *= g_screen_coord_scale;
            break;
        case ANM_VM_RESOLUTION_HALF_SCALED:
            vm->world_matrix._11 *= g_screen_coord_scale * 0.5f;
            vm->world_matrix._22 *= g_screen_coord_scale * 0.5f;
            break;
        }
        Float3 rotation = *vm->get_total_rotation();
        D3DXMATRIX rotation_matrix;
        switch ((vm->flags_hi >> ANM_VM_ROTATION_MODE_SHIFT) & 7)
        {
        case 0:
            if (rotation.x != 0.0f)
            {
                D3DXMatrixRotationX(&rotation_matrix, rotation.x);
                D3DXMatrixMultiply(&vm->world_matrix, &vm->world_matrix, &rotation_matrix);
            }
            if (rotation.y != 0.0f)
            {
                D3DXMatrixRotationY(&rotation_matrix, rotation.y);
                D3DXMatrixMultiply(&vm->world_matrix, &vm->world_matrix, &rotation_matrix);
            }
            if (rotation.z != 0.0f)
            {
                D3DXMatrixRotationZ(&rotation_matrix, rotation.z);
                D3DXMatrixMultiply(&vm->world_matrix, &vm->world_matrix, &rotation_matrix);
            }
            break;
        case 1:
            if (rotation.x != 0.0f)
            {
                D3DXMatrixRotationX(&rotation_matrix, rotation.x);
                D3DXMatrixMultiply(&vm->world_matrix, &vm->world_matrix, &rotation_matrix);
            }
            if (rotation.z != 0.0f)
            {
                D3DXMatrixRotationZ(&rotation_matrix, rotation.z);
                D3DXMatrixMultiply(&vm->world_matrix, &vm->world_matrix, &rotation_matrix);
            }
            if (rotation.y != 0.0f)
            {
                D3DXMatrixRotationY(&rotation_matrix, rotation.y);
                D3DXMatrixMultiply(&vm->world_matrix, &vm->world_matrix, &rotation_matrix);
            }
            break;
        case 2:
            if (rotation.y != 0.0f)
            {
                D3DXMatrixRotationY(&rotation_matrix, rotation.y);
                D3DXMatrixMultiply(&vm->world_matrix, &vm->world_matrix, &rotation_matrix);
            }
            if (rotation.x != 0.0f)
            {
                D3DXMatrixRotationX(&rotation_matrix, rotation.x);
                D3DXMatrixMultiply(&vm->world_matrix, &vm->world_matrix, &rotation_matrix);
            }
            if (rotation.z != 0.0f)
            {
                D3DXMatrixRotationZ(&rotation_matrix, rotation.z);
                D3DXMatrixMultiply(&vm->world_matrix, &vm->world_matrix, &rotation_matrix);
            }
            break;
        case 3:
            if (rotation.y != 0.0f)
            {
                D3DXMatrixRotationY(&rotation_matrix, rotation.y);
                D3DXMatrixMultiply(&vm->world_matrix, &vm->world_matrix, &rotation_matrix);
            }
            if (rotation.z != 0.0f)
            {
                D3DXMatrixRotationZ(&rotation_matrix, rotation.z);
                D3DXMatrixMultiply(&vm->world_matrix, &vm->world_matrix, &rotation_matrix);
            }
            if (rotation.x != 0.0f)
            {
                D3DXMatrixRotationX(&rotation_matrix, rotation.x);
                D3DXMatrixMultiply(&vm->world_matrix, &vm->world_matrix, &rotation_matrix);
            }
            break;
        case 4:
            if (rotation.z != 0.0f)
            {
                D3DXMatrixRotationZ(&rotation_matrix, rotation.z);
                D3DXMatrixMultiply(&vm->world_matrix, &vm->world_matrix, &rotation_matrix);
            }
            if (rotation.x != 0.0f)
            {
                D3DXMatrixRotationX(&rotation_matrix, rotation.x);
                D3DXMatrixMultiply(&vm->world_matrix, &vm->world_matrix, &rotation_matrix);
            }
            if (rotation.y != 0.0f)
            {
                D3DXMatrixRotationY(&rotation_matrix, rotation.y);
                D3DXMatrixMultiply(&vm->world_matrix, &vm->world_matrix, &rotation_matrix);
            }
            break;
        case 5:
            if (rotation.z != 0.0f)
            {
                D3DXMatrixRotationZ(&rotation_matrix, rotation.z);
                D3DXMatrixMultiply(&vm->world_matrix, &vm->world_matrix, &rotation_matrix);
            }
            if (rotation.y != 0.0f)
            {
                D3DXMatrixRotationY(&rotation_matrix, rotation.y);
                D3DXMatrixMultiply(&vm->world_matrix, &vm->world_matrix, &rotation_matrix);
            }
            if (rotation.x != 0.0f)
            {
                D3DXMatrixRotationX(&rotation_matrix, rotation.x);
                D3DXMatrixMultiply(&vm->world_matrix, &vm->world_matrix, &rotation_matrix);
            }
            break;
        }
        vm->flags_lo &= ~ANM_VM_ROTATION_CHANGED;
    }
    D3DXMATRIX world = vm->world_matrix;
    world._41 = vm->entity_pos.x + vm->pos.x + vm->pos_2.x - vm->anchor_offset.x * vm->scale.x * vm->scale_2.x;
    world._42 = vm->entity_pos.y + vm->pos.y + vm->pos_2.y - vm->anchor_offset.y * vm->scale.y * vm->scale_2.y;
    vm->transform_coords((Float3 *)&world._41);
    setup_render_state_for_vm(vm);
    ZunColor color;
    color.d3d = (vm->flags_lo & ANM_VM_COLOR_MODE_MASK) ? vm->color_2.d3d : vm->color_1.d3d;
    if (global_tint_enabled != 0)
    {
        color.r = color_mul(color.r, global_tint.r);
        color.g = color_mul(color.g, global_tint.g);
        color.b = color_mul(color.b, global_tint.b);
        color.a = color_mul(color.a, global_tint.a);
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
    if (last_vertex_setup != ANM_VERTEX_SETUP_3D_QUAD)
    {
        g_Supervisor.d3d_device->SetStreamSource(0, vertex_buffer, 0, sizeof(RenderVertexXyzTex));
        g_Supervisor.d3d_device->SetFVF(D3DFVF_XYZ | D3DFVF_TEX1);
        g_Supervisor.d3d_device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_TFACTOR);
        g_Supervisor.d3d_device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_TFACTOR);
        last_vertex_setup = ANM_VERTEX_SETUP_3D_QUAD;
    }
    set_color_op_modulate();
    // The vertex buffer holds one quad per anchoring (setup_vertex_buffer).
    g_Supervisor.d3d_device->DrawPrimitive(
        D3DPT_TRIANGLESTRIP,
        (((vm->flags_lo >> ANM_VM_ANCHOR_Y_SHIFT) & 3) * 3 + ((vm->flags_lo >> ANM_VM_ANCHOR_X_SHIFT) & 3)) * 4, 2);
    return 0;
}

// The identity is written through D3DXMatrixIdentity's return value: as a
// separate statement followed by `world_matrix = sprite_matrix`, our build
// dropped the identity stores entirely (sprite_matrix kept its old contents).
// TODO: 31%; ours realigns the frame to 16 for the rotation matrix (the original does not realign) and keeps vm in esi (original ebx).
// FUNCTION: TH16 0x467d00
i32 AnmManager::draw_3d_vertex_strip(AnmVm *vm, RenderVertexXyzDiffuseTex *vertices, i32 vertex_count)
{
    if (!(vm->flags_lo & ANM_VM_VISIBLE))
    {
        return -1;
    }
    if (!(vm->flags_lo & ANM_VM_SHOWN))
    {
        return -1;
    }
    if (unrendered_sprite_count != 0)
    {
        flush_sprites();
    }
    if (!(vm->flags_lo & ANM_VM_Z_WRITE_DISABLE))
    {
        g_Supervisor.enable_zwrite();
    }
    else
    {
        g_Supervisor.disable_zwrite();
    }
    vm->world_matrix = *D3DXMatrixIdentity(&vm->sprite_matrix);
    vm->world_matrix._11 *= vm->scale_2.x * vm->scale.x;
    vm->world_matrix._22 *= vm->scale_2.y * vm->scale.y;
    vm->flags_lo &= ~ANM_VM_SCALE_CHANGED;
    D3DXMATRIX rotation_matrix;
    if (vm->rotation.x != 0.0f)
    {
        D3DXMatrixRotationX(&rotation_matrix, vm->rotation.x);
        D3DXMatrixMultiply(&vm->world_matrix, &vm->world_matrix, &rotation_matrix);
    }
    if (vm->rotation.y != 0.0f)
    {
        D3DXMatrixRotationY(&rotation_matrix, vm->rotation.y);
        D3DXMatrixMultiply(&vm->world_matrix, &vm->world_matrix, &rotation_matrix);
    }
    if (vm->rotation.z != 0.0f)
    {
        D3DXMatrixRotationZ(&rotation_matrix, vm->rotation.z);
        D3DXMatrixMultiply(&vm->world_matrix, &vm->world_matrix, &rotation_matrix);
    }
    vm->flags_lo &= ~ANM_VM_ROTATION_CHANGED;
    D3DXMATRIX world = vm->world_matrix;
    world._41 = vm->entity_pos.x + vm->pos.x + vm->pos_2.x;
    if ((vm->flags_hi & ANM_VM_ORIGIN_MODE_MASK) && vm->parent_vm == NULL)
    {
        world._41 += g_game_2d_origin_x;
    }
    world._42 = vm->entity_pos.y + vm->pos.y + vm->pos_2.y;
    world._43 = vm->entity_pos.z + vm->pos.z + vm->pos_2.z;
    g_Supervisor.d3d_device->SetTransform(D3DTS_WORLD, &world);
    setup_render_state_for_vm(vm);
    AnmLoadedSprite *sprite = set_texture_of_vm(this, vm);
    set_texture_transform_of_vm(this, vm, sprite);
    if (last_vertex_setup != ANM_VERTEX_SETUP_3D_STRIP)
    {
        g_Supervisor.d3d_device->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1);
        g_Supervisor.d3d_device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
        g_Supervisor.d3d_device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
        last_vertex_setup = ANM_VERTEX_SETUP_3D_STRIP;
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

// A VM's horizontal and vertical anchoring (AnmAnchor). draw_vm reads them
// at each call: as locals before the shape switch they were kept in ebx.
#define ANM_VM_ANCHOR_X(vm) (((vm)->flags_lo >> ANM_VM_ANCHOR_X_SHIFT) & 3)
#define ANM_VM_ANCHOR_Y(vm) (((vm)->flags_lo >> ANM_VM_ANCHOR_Y_SHIFT) & 3)

// Draws a VM by its render mode. The last rect case ends with a break
// rather than a return, which lets the rotated rect case jump into its
// call like the original.
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
    if (!(vm->flags_lo & ANM_VM_SHOWN))
    {
        return -1;
    }
    if (vm->flags_hi & (ANM_VM_DELETE_PENDING | ANM_VM_IN_DELETE_LIST))
    {
        return -1;
    }
    g_Supervisor.disable_zwrite();
    switch ((vm->flags_lo >> ANM_VM_RENDER_MODE_SHIFT) & 0x1f)
    {
    case ANM_RENDER_SPRITE:
        if (is_transparent(vm))
        {
            return -1;
        }
        AnmVm::write_sprite_corners__without_rot(
            vm, (Float3 *)&g_sprite_temp_buffer[0].pos, (Float3 *)&g_sprite_temp_buffer[1].pos,
            (Float3 *)&g_sprite_temp_buffer[2].pos, (Float3 *)&g_sprite_temp_buffer[3].pos);
        return render_sprite_2d(vm, ANM_SPRITE_SNAP_TO_PIXELS);
    case ANM_RENDER_SPRITE_ROTATED:
    case ANM_RENDER_SPRITE_ROTATED_3:
        if (is_transparent(vm))
        {
            return -1;
        }
        AnmVm::write_sprite_corners__with_z_rot(
            vm, (Float3 *)&g_sprite_temp_buffer[0].pos, (Float3 *)&g_sprite_temp_buffer[1].pos,
            (Float3 *)&g_sprite_temp_buffer[2].pos, (Float3 *)&g_sprite_temp_buffer[3].pos);
        return render_sprite_2d(vm, 0);
    case ANM_RENDER_BILLBOARD:
        if (is_transparent(vm))
        {
            return -1;
        }
        if (write_billboard_corners(vm) != 0)
        {
            return -1;
        }
        return render_sprite_2d(vm, 0);
    case ANM_RENDER_MODE_5:
        if (is_transparent(vm))
        {
            return -1;
        }
        return draw_mode_5(vm);
    case ANM_RENDER_BILLBOARD_FOG:
        if (is_transparent(vm))
        {
            return -1;
        }
        return draw_billboard_fog(vm);
    case ANM_RENDER_SPRITE_FOG:
        if (is_transparent(vm))
        {
            return -1;
        }
        return draw_sprite_fog(vm);
    case ANM_RENDER_3D:
        if (is_transparent(vm))
        {
            return -1;
        }
        return draw_3d(vm);
    case ANM_RENDER_3D_FOG:
        if (is_transparent(vm))
        {
            return -1;
        }
        g_Supervisor.enable_d3d_fog();
        draw_3d(vm);
        g_Supervisor.disable_d3d_fog();
        return 0;
    case ANM_RENDER_TEX_CIRCLE:
    case ANM_RENDER_MODE_12:
    case ANM_RENDER_TEX_ARC_EVEN:
    case ANM_RENDER_TEX_ARC:
        return draw_vertex_strip(vm, (RenderVertex144 *)vm->extra_data, vm->int_vars[0] * 2);
    case ANM_RENDER_TRIANGLE_FAN:
        return draw_vertex_fan(vm, (RenderVertex144 *)vm->extra_data, vm->int_vars[0] * 2);
    case ANM_RENDER_TEX_CYLINDER_3D:
    case ANM_RENDER_TEX_RING_3D:
        return draw_3d_vertex_strip(vm, (RenderVertexXyzDiffuseTex *)vm->extra_data, vm->int_vars[0] * 2);
    case ANM_RENDER_SPRITE_UNSNAPPED:
        if (is_transparent(vm))
        {
            return -1;
        }
        AnmVm::write_sprite_corners__without_rot(
            vm, (Float3 *)&g_sprite_temp_buffer[0].pos, (Float3 *)&g_sprite_temp_buffer[1].pos,
            (Float3 *)&g_sprite_temp_buffer[2].pos, (Float3 *)&g_sprite_temp_buffer[3].pos);
        return render_sprite_2d(vm, 0);
    case ANM_RENDER_RECT:
    case ANM_RENDER_RECT_GRAD:
    case ANM_RENDER_RECT_ROT:
    case ANM_RENDER_RECT_ROT_GRAD:
    case ANM_RENDER_LINE:
    case ANM_RENDER_RECT_BORDER: {
        f32 angle = vm->rotation.z;
        f32 width;
        f32 height;
        width = vm->sprite_size.x * vm->scale.x;
        height = vm->sprite_size.y * vm->scale.y;
        Float3 pos;
        vm->get_own_transformed_pos(&pos);
        if (vm->parent_vm != NULL && !(vm->flags_hi & ANM_VM_NO_PARENT_POS))
        {
            width = vm->parent_vm->scale.x * width;
            height = vm->parent_vm->scale.y * height;
            angle = vm->parent_vm->rotation.z + angle;
        }
        setup_render_state_for_vm(vm);
        if ((vm->flags_hi & ANM_VM_RESOLUTION_MODE_MASK) == ANM_VM_RESOLUTION_SCALED)
        {
            width = g_screen_coord_scale * width;
            height = g_screen_coord_scale * height;
        }
        else if ((vm->flags_hi & ANM_VM_RESOLUTION_MODE_MASK) == ANM_VM_RESOLUTION_HALF_SCALED)
        {
            width = g_screen_coord_scale * 0.5f * width;
            height = g_screen_coord_scale * 0.5f * height;
        }
        switch ((vm->flags_lo >> ANM_VM_RENDER_MODE_SHIFT) & 0x1f)
        {
        case ANM_RENDER_LINE:
            draw_line(pos.x, pos.y, width, angle, vm->color_1.d3d,
                      (vm->flags_lo & ANM_VM_COLOR_MODE_MASK) ? vm->color_2.d3d : vm->color_1.d3d, ANM_VM_ANCHOR_X(vm),
                      0);
            return 0;
        case ANM_RENDER_RECT:
            draw_rect(pos.x, pos.y, width, height, angle, vm->color_1.d3d, vm->color_1.d3d, ANM_VM_ANCHOR_X(vm),
                      ANM_VM_ANCHOR_Y(vm));
            return 0;
        case ANM_RENDER_RECT_BORDER:
            draw_rect_outline(pos.x, pos.y, width, height, angle, vm->color_1.d3d,
                              (vm->flags_lo & ANM_VM_COLOR_MODE_MASK) ? vm->color_2.d3d : vm->color_1.d3d,
                              ANM_VM_ANCHOR_X(vm), ANM_VM_ANCHOR_Y(vm));
            return 0;
        case ANM_RENDER_RECT_GRAD:
            draw_rect(pos.x, pos.y, width, height, angle, vm->color_1.d3d, vm->color_2.d3d, ANM_VM_ANCHOR_X(vm),
                      ANM_VM_ANCHOR_Y(vm));
            return 0;
        case ANM_RENDER_RECT_ROT:
            draw_rect_bordered(pos.x, pos.y, width, height, angle, vm->color_1.d3d, vm->color_1.d3d,
                               ANM_VM_ANCHOR_X(vm), ANM_VM_ANCHOR_Y(vm));
            return 0;
        case ANM_RENDER_RECT_ROT_GRAD:
            draw_rect_bordered(pos.x, pos.y, width, height, angle, vm->color_1.d3d, vm->color_2.d3d,
                               ANM_VM_ANCHOR_X(vm), ANM_VM_ANCHOR_Y(vm));
            break;
        }
        break;
    }
    case ANM_RENDER_POLY:
    case ANM_RENDER_POLY_BORDER:
    case ANM_RENDER_RING: {
        f32 angle = vm->rotation.z;
        f32 width;
        f32 height;
        width = vm->sprite_size.x * vm->scale.x;
        height = vm->sprite_size.y * vm->scale.y;
        Float3 pos;
        vm->get_own_transformed_pos(&pos);
        if (vm->parent_vm != NULL && !(vm->flags_hi & ANM_VM_NO_PARENT_POS))
        {
            width *= vm->parent_vm->scale.x;
            angle = vm->parent_vm->rotation.z + angle;
            height *= vm->parent_vm->scale.y;
        }
        if ((vm->flags_hi & ANM_VM_RESOLUTION_MODE_MASK) == ANM_VM_RESOLUTION_SCALED)
        {
            width = g_screen_coord_scale * width;
            height = g_screen_coord_scale * height;
        }
        else if ((vm->flags_hi & ANM_VM_RESOLUTION_MODE_MASK) == ANM_VM_RESOLUTION_HALF_SCALED)
        {
            width = g_screen_coord_scale * 0.5f * width;
            height = g_screen_coord_scale * 0.5f * height;
        }
        setup_render_state_for_vm(vm);
        switch ((vm->flags_lo >> ANM_VM_RENDER_MODE_SHIFT) & 0x1f)
        {
        case ANM_RENDER_RING:
            draw_ring(pos.x, pos.y, width, height, angle, vm->int_vars[0], vm->color_1.d3d);
            return 0;
        case ANM_RENDER_POLY_BORDER:
            draw_circle_outline(pos.x, pos.y, width, angle, vm->int_vars[0], vm->color_1.d3d);
            return 0;
        case ANM_RENDER_POLY:
            draw_circle(pos.x, pos.y, width, angle, vm->int_vars[0], vm->color_1.d3d, vm->color_2.d3d);
            return 0;
        }
        break;
    }
    }
    return 0;
}
