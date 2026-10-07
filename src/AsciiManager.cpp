#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "AnmManager.h"
#include "AsciiManager.h"
#include "GameErrorContext.h"
#include "Supervisor.h"

// The original keeps this UCRT header function out of line (0x405540).
// Every caller is vsprintf, so LTCG folds the count (-1) away entirely and
// leaves the NULL locale's slot filled with junk (push ecx).
DECOMP_NOINLINE int __CRTDECL _vsnprintf_l(char *buffer, size_t count, const char *format, _locale_t locale, va_list args);

// GLOBAL: TH16 0x4a6d98
AsciiInf *g_AsciiManager;

// FUNCTION: TH16 0x407c10
AsciiInf::AsciiInf()
{
    memset(this, 0, sizeof(AsciiInf));
    g_AsciiManager = this;
    flags |= 2;
    color.d3d = 0xffffffff;
    scale.x = 1.0f;
    scale.y = 1.0f;
    unk_1921c = 0;
    character_spacing_for_font_0 = 9;
    align_h = ASCII_ALIGN_START;
    align_v = ASCII_ALIGN_START;
}

// FUNCTION: TH16 0x409030
u32 AsciiInf::get_size()
{
    return sizeof(AsciiInf);
}

// FUNCTION: TH16 0x407cb0
i32 AsciiInf::initialize()
{
    const char *anm_names[3] = {"ascii.anm", "ascii_960.anm", "ascii_1280.anm"};

    ascii_anm = AnmManager::preload_anm(ANM_SLOT_ASCII, anm_names[g_Supervisor.config.window_size % 3]);
    if (ascii_anm == NULL)
    {
        // "The data is corrupt."
        g_GameErrorContext.log("\x83" "f\x81[\x83^\x82\xaa\x89\xf3\x82\xea\x82\xc4\x82\xa2\x82\xdc\x82\xb7\r\n");
        return -1;
    }

    UpdateFunc *f = g_UpdateFuncRegistry->create_func(on_tick_callback);
    f->flags &= ~UPDATE_FUNC_ACTIVE;
    f->arg = this;
    g_UpdateFuncRegistry->register_on_tick(f, 4);
    on_tick_func = f;

    f = g_UpdateFuncRegistry->create_func(on_draw_1_callback);
    f->flags &= ~UPDATE_FUNC_ACTIVE;
    f->arg = this;
    g_UpdateFuncRegistry->register_on_draw(f, 0x51);
    on_draw_func_1 = f;

    f = g_UpdateFuncRegistry->create_func(on_draw_2_callback);
    f->flags &= ~UPDATE_FUNC_ACTIVE;
    f->arg = this;
    g_UpdateFuncRegistry->register_on_draw(f, 0x35);
    on_draw_func_2 = f;

    f = g_UpdateFuncRegistry->create_func(on_draw_3_callback);
    f->flags &= ~UPDATE_FUNC_ACTIVE;
    f->arg = this;
    g_UpdateFuncRegistry->register_on_draw(f, 0x42);
    on_draw_func_3 = f;

    ascii_anm->init_vm_with_sprite(&glyph_vm, 0);
    ascii_anm->init_vm_with_sprite(&vm_2, 0x62);
    return 0;
}

// FUNCTION: TH16 0x407e00
AsciiInf::~AsciiInf()
{
    g_UpdateFuncRegistry->unregister_locked(on_tick_func);
    g_UpdateFuncRegistry->unregister_locked(on_draw_func_1);
    g_UpdateFuncRegistry->unregister_locked(on_draw_func_2);
    g_UpdateFuncRegistry->unregister_locked(on_draw_func_3);
    g_AnmManager->unload_anm(ANM_SLOT_ASCII);
    g_AnmManager->unload_anm(ANM_SLOT_TEXT);
    g_AsciiManager = NULL;
}

// Drops the strings whose time ran out, keeping the others in order.
// TODO: same loop shape, but i and the remaining_time pointer trade eax and
// edx, and esi is pushed before the loop guard (for/while, index copies and a
// separate decrement do not change it).
// FUNCTION: TH16 0x408fb0
void AsciiInf::tick()
{
    AsciiStr *dst = strings;
    i32 kept = 0;
    AsciiStr *src;
    i32 i;
    for (i = 0, src = strings; i < num_strings; i++, src++)
    {
        if (--strings[i].remaining_time >= 0)
        {
            if (kept != i)
            {
                *dst = *src;
            }
            kept++;
            dst++;
        }
    }
    num_strings = kept;
}

// FUNCTION: TH16 0x408070
int __fastcall AsciiInf::on_tick_callback(void *arg)
{
    AsciiInf *ascii = (AsciiInf *)arg;
    ascii->tick();
    ascii->num_ticks_alive++;
    return UPDATE_FUNC_CONTINUE;
}

// FUNCTION: TH16 0x408140
void AsciiInf::create_string(Float3 *pos, const char *text)
{
    if (num_strings >= 0x140)
    {
        return;
    }
    AsciiStr *str = &strings[num_strings];
    num_strings++;
    strcpy(str->text, text);
    str->pos = *pos;
    str->pos *= g_screen_coord_scale;
    str->render_group = group;
    str->color = color.d3d;
    str->scale = scale;
    str->unk_11c = unk_19218;
    str->font_id = font_id;
    str->draw_shadows = draw_shadows;
    str->remaining_time = duration;
    str->align_h = align_h;
    str->align_v = align_v;
}

// TODO: ours passes fmt to _vsprintf_l in edx (the original pushes it) and
// has a 4 bytes smaller frame.
// FUNCTION: TH16 0x408260
void AsciiInf::create_stringf(Float3 *pos, const char *fmt, ...)
{
    char buf[0x104];
    va_list args;
    va_start(args, fmt);
    _vsprintf_l(buf, fmt, NULL, args);
    create_string(pos, buf);
    va_end(args);
}

// FUNCTION: TH16 0x4084f0
void AsciiInf::create_debug_stringf(Float3 *pos, const char *fmt, ...)
{
    char buf[0x100];
    va_list args;
    va_start(args, fmt);
    _vsprintf_l(buf, fmt, NULL, args);
    create_string(pos, buf);
    strings[num_strings - 1].font_id = ASCII_FONT_DEBUG;
    va_end(args);
}

// FUNCTION: TH16 0x4082b0
void AsciiInf::create_number(Float3 *pos, u32 value)
{
    char buf[0x104];
    AsciiInf *ascii = g_AsciiManager;
    if (value < 1000)
    {
        sprintf(buf, "%d", value);
    }
    else if (value < 1000000)
    {
        sprintf(buf, "%d,%.3d", value / 1000, value % 1000);
    }
    else if (value < 1000000000)
    {
        sprintf(buf, "%d,%.3d,%.3d", value / 1000000 % 1000, value / 1000 % 1000, value % 1000);
    }
    else
    {
        sprintf(buf, "%d,%.3d,%.3d,%.3d", value / 1000000000 % 1000, value / 1000000 % 1000, value / 1000 % 1000,
                value % 1000);
    }
    ascii->create_string(pos, buf);
}

// TODO: the original frame has 4 more bytes above the buffer.
// FUNCTION: TH16 0x4083b0
void AsciiInf::create_number_with_digit(Float3 *pos, u32 value, u32 digit)
{
    char buf[0x104];
    AsciiInf *ascii = g_AsciiManager;
    if (value < 100)
    {
        sprintf(buf, "%d", value * 10 + digit);
    }
    else if (value < 100000)
    {
        sprintf(buf, "%d,%.2d%d", value / 100, value % 100, digit);
    }
    else if (value < 100000000)
    {
        sprintf(buf, "%d,%.3d,%.2d%d", value / 100000 % 1000, value / 100 % 1000, value % 100, digit);
    }
    else
    {
        sprintf(buf, "%d,%.3d,%.3d,%.2d%d", value / 100000000 % 1000, value / 100000 % 1000, value / 100 % 1000,
                value % 100, digit);
    }
    ascii->create_string(pos, buf);
}

// FUNCTION: TH16 0x408090
int __fastcall AsciiInf::on_draw_1_callback(void *arg)
{
    return ((AsciiInf *)arg)->draw_group(0);
}

// FUNCTION: TH16 0x4080a0
int __fastcall AsciiInf::on_draw_2_callback(void *arg)
{
    return ((AsciiInf *)arg)->draw_group_1();
}

// FUNCTION: TH16 0x4080b0
int __fastcall AsciiInf::on_draw_3_callback(void *arg)
{
    AsciiInf *ascii = (AsciiInf *)arg;
    g_Supervisor.current_camera = &g_Supervisor.cameras[0];
    g_Supervisor.swap_transform_matrices(&g_Supervisor.cameras[0]);
    g_Supervisor.d3d_device->SetViewport(&g_Supervisor.current_camera->viewport);
    g_Supervisor.current_camera_index = 0;
    int result = ascii->draw_group(2);
    g_Supervisor.current_camera = &g_Supervisor.cameras[2];
    g_Supervisor.swap_transform_matrices(&g_Supervisor.cameras[2]);
    g_Supervisor.d3d_device->SetViewport(&g_Supervisor.current_camera->viewport);
    g_Supervisor.current_camera_index = 2;
    return result;
}

// FUNCTION: TH16 0x409040
void AnmVm::set_sprite_uvs(i32 sprite)
{
    AnmLoaded *anm = g_AnmManager->loaded_anms[anm_loaded_index];
    AnmLoadedSprite *s = &anm->sprites[sprite];
    sprite_id = sprite;
    uv_quad_of_sprite[0].x = uv_quad_of_sprite[2].x = s->uv_start.x;
    uv_quad_of_sprite[1].x = uv_quad_of_sprite[3].x = s->uv_end.x;
    uv_quad_of_sprite[0].y = uv_quad_of_sprite[1].y = s->uv_start.y;
    uv_quad_of_sprite[2].y = uv_quad_of_sprite[3].y = s->uv_end.y;
}

__forceinline void AnmVm::set_sprite_uvs_inline(AnmManager *anm, i32 sprite)
{
    AnmLoadedSprite *s = &anm->loaded_anms[anm_loaded_index]->sprites[sprite];
    sprite_id = sprite;
    uv_quad_of_sprite[0].x = uv_quad_of_sprite[2].x = s->uv_start.x;
    uv_quad_of_sprite[1].x = uv_quad_of_sprite[3].x = s->uv_end.x;
    uv_quad_of_sprite[0].y = uv_quad_of_sprite[1].y = s->uv_start.y;
    uv_quad_of_sprite[2].y = uv_quad_of_sprite[3].y = s->uv_end.y;
}

// Draws one string a character at a time with glyph_vm: picks the glyph for
// the font, aligns the string and draws an optional shadow first.
// TODO: same structure; the original keeps this in ecx (ours edx) and numbers the xmm registers differently, which shifts most of the function.
// FUNCTION: TH16 0x408650
void AsciiInf::draw_string(AsciiStr *str)
{
    size_t len = strlen(str->text);
    AnmVm *vm = &glyph_vm;
    glyph_vm.flags_hi &= ~ANM_VM_RESOLUTION_MODE_MASK;
    // Anchored at its left and top, visible.
    glyph_vm.flags_lo = (glyph_vm.flags_lo & ~(2 << ANM_VM_ANCHOR_X_SHIFT | 2 << ANM_VM_ANCHOR_Y_SHIFT)) |
                        (1 << ANM_VM_ANCHOR_X_SHIFT | 1 << ANM_VM_ANCHOR_Y_SHIFT | ANM_VM_VISIBLE);
    glyph_vm.pos = str->pos;
    vm->flags_lo |= ANM_VM_SCALE_CHANGED;
    vm->scale.x = str->scale.x;
    vm->scale.y = str->scale.y;
    f32 advance;
    f32 line_height;
    switch (str->font_id)
    {
    case ASCII_FONT_DEBUG:
        glyph_vm.flags_hi &= ~ANM_VM_FILTER_POINT;
        advance = str->scale.x * 6.0f;
        line_height = 9.0f;
        break;
    case ASCII_FONT_SMALL:
        glyph_vm.flags_hi &= ~ANM_VM_FILTER_POINT;
        line_height = 10.0f;
        advance = str->scale.x * 7.0f;
        break;
    case ASCII_FONT_SMALL_POINT:
        glyph_vm.flags_hi |= ANM_VM_FILTER_POINT;
        line_height = 10.0f;
        advance = str->scale.x * 7.0f;
        break;
    case ASCII_FONT_LARGE:
        glyph_vm.flags_hi &= ~ANM_VM_FILTER_POINT;
        line_height = 16.0f;
        advance = str->scale.x * 12.0f;
        break;
    case ASCII_FONT_LARGE_POINT:
        glyph_vm.flags_hi |= ANM_VM_FILTER_POINT;
        line_height = 16.0f;
        advance = str->scale.x * 12.0f;
        break;
    default:
        if (str->scale.x != 1.0f)
        {
            glyph_vm.flags_hi |= ANM_VM_FILTER_POINT;
        }
        else
        {
            glyph_vm.flags_hi &= ~ANM_VM_FILTER_POINT;
        }
        line_height = 14.0f;
        advance = character_spacing_for_font_0 * str->scale.x;
        break;
    }
    const char *p;
    switch (str->align_h)
    {
    case ASCII_ALIGN_CENTER:
        switch (str->font_id)
        {
        case ASCII_FONT_SMALL:
        case ASCII_FONT_SMALL_POINT:
            for (p = str->text; *p != '\0'; p++)
            {
                vm->pos.x += (*p == '.' ? str->scale.x * -4.0f * 0.5f : advance * -0.5f) * g_screen_coord_scale;
            }
            break;
        case ASCII_FONT_LARGE:
        case ASCII_FONT_LARGE_POINT:
            for (p = str->text; *p != '\0'; p++)
            {
                vm->pos.x += (*p == ',' ? str->scale.x * -4.0f * 0.5f : advance * -0.5f) * g_screen_coord_scale;
            }
            break;
        default:
            vm->pos.x += -(i32)len * advance * 0.5f * g_screen_coord_scale;
            break;
        }
        break;
    case ASCII_ALIGN_END:
        switch (str->font_id)
        {
        case ASCII_FONT_SMALL:
        case ASCII_FONT_SMALL_POINT:
            for (p = str->text; *p != '\0'; p++)
            {
                vm->pos.x += *p == '.' ? str->scale.x * -4.0f * g_screen_coord_scale : -(g_screen_coord_scale * advance);
            }
            break;
        case ASCII_FONT_LARGE:
        case ASCII_FONT_LARGE_POINT:
            for (p = str->text; *p != '\0'; p++)
            {
                vm->pos.x += *p == ',' ? str->scale.x * -4.0f * g_screen_coord_scale : -(g_screen_coord_scale * advance);
            }
            break;
        default:
            vm->pos.x += -(i32)len * advance * g_screen_coord_scale;
            break;
        }
        break;
    }
    switch (str->align_v)
    {
    case ASCII_ALIGN_CENTER:
        vm->pos.y += line_height * -0.5f * g_screen_coord_scale;
        break;
    case ASCII_ALIGN_END:
        vm->pos.y += -(g_screen_coord_scale * line_height);
        break;
    }
    for (p = str->text; *p != '\0'; p++)
    {
        char c = *p;
        if (c == '\n')
        {
            vm->pos.y += str->scale.y * line_height * g_screen_coord_scale;
            glyph_vm.pos.x = str->pos.x;
            continue;
        }
        if (c != ' ')
        {
            AnmManager *anm = g_AnmManager;
            switch (str->font_id)
            {
            case ASCII_FONT_DEFAULT:
                vm->set_sprite_uvs_inline(anm, (u8)c - 0x20);
                break;
            case ASCII_FONT_DEBUG:
                vm->set_sprite_uvs_inline(anm, (u8)c + 0x42);
                break;
            case ASCII_FONT_SMALL:
            case ASCII_FONT_SMALL_POINT:
                advance = str->scale.x * 7.0f;
                if (c >= 'a' && c <= 'z')
                {
                    vm->set_sprite_uvs((u8)c + 0x74);
                }
                else if (c >= 'A' && c <= 'Z')
                {
                    vm->set_sprite_uvs((u8)c + 0x94);
                }
                else if (c == '/')
                {
                    vm->set_sprite_uvs(0xce);
                }
                else if (c == ':')
                {
                    vm->set_sprite_uvs(0xcf);
                }
                else if (c == '-')
                {
                    vm->set_sprite_uvs(0xd0);
                }
                else if (c == '*')
                {
                    vm->set_sprite_uvs(0xd1);
                }
                else if (c == '%')
                {
                    vm->set_sprite_uvs(0xd2);
                }
                else if (c == '$')
                {
                    vm->set_sprite_uvs(0x101);
                }
                else if (c == '.')
                {
                    vm->set_sprite_uvs(0xd3);
                    advance = str->scale.x * 4.0f;
                }
                else if (c == '+')
                {
                    vm->set_sprite_uvs(0xd4);
                }
                else
                {
                    vm->set_sprite_uvs((u8)c + 0x94);
                }
                break;
            case ASCII_FONT_LARGE:
            case ASCII_FONT_LARGE_POINT:
                advance = str->scale.x * 12.0f;
                glyph_vm.pos.y = str->pos.y;
                if (c == '/')
                {
                    vm->set_sprite_uvs(0xf9);
                }
                else if (c == '.')
                {
                    vm->set_sprite_uvs(0xfa);
                }
                else if (c == 's')
                {
                    vm->set_sprite_uvs(0xfb);
                }
                else if (c == '*')
                {
                    vm->set_sprite_uvs(0xfc);
                }
                else if (c == ',')
                {
                    vm->set_sprite_uvs(0xfd);
                    advance = str->scale.x * 4.0f;
                    glyph_vm.pos.y = g_screen_coord_scale * 3.0f + str->pos.y;
                }
                else
                {
                    vm->set_sprite_uvs((u8)c + 0xbf);
                }
                break;
            }
            f32 sprite_height = anm->loaded_anms[vm->anm_loaded_index]->sprites[vm->sprite_id].sprite_height;
            f32 sprite_width = anm->loaded_anms[vm->anm_loaded_index]->sprites[vm->sprite_id].sprite_width;
            vm->flags_lo |= ANM_VM_SCALE_CHANGED;
            vm->sprite_size.x = sprite_width;
            vm->sprite_size.y = sprite_height;
            if (str->draw_shadows)
            {
                glyph_vm.color_1.d3d = str->color & 0xff000000;
                glyph_vm.color_1.a = str->color >> 25;
                vm->pos.x += g_screen_coord_scale * 2.0f;
                vm->pos.y += g_screen_coord_scale * 2.0f;
                AnmVm::write_sprite_corners__without_rot(vm, (Float3 *)&g_sprite_temp_buffer[0],
                                                         (Float3 *)&g_sprite_temp_buffer[1],
                                                         (Float3 *)&g_sprite_temp_buffer[2],
                                                         (Float3 *)&g_sprite_temp_buffer[3]);
                anm->render_sprite_2d(vm, ANM_SPRITE_SNAP_TO_PIXELS);
                anm = g_AnmManager;
                vm->pos.x += g_screen_coord_scale * -2.0f;
                vm->pos.y += g_screen_coord_scale * -2.0f;
            }
            glyph_vm.color_1.d3d = str->color;
            AnmVm::write_sprite_corners__without_rot(vm, (Float3 *)&g_sprite_temp_buffer[0],
                                                     (Float3 *)&g_sprite_temp_buffer[1],
                                                     (Float3 *)&g_sprite_temp_buffer[2],
                                                     (Float3 *)&g_sprite_temp_buffer[3]);
            anm->render_sprite_2d(vm, ANM_SPRITE_SNAP_TO_PIXELS);
        }
        vm->pos.x += g_screen_coord_scale * advance;
    }
}

// FUNCTION: TH16 0x408560
i32 AsciiInf::draw_group(i32 group)
{
    AsciiStr *str = strings;
    for (i32 i = 0; i < num_strings; i++, str++)
    {
        if (str->render_group == group)
        {
            draw_string(str);
        }
    }
    g_AnmManager->flush_sprites();
    g_Supervisor.current_camera = &g_Supervisor.cameras[2];
    if (g_AnmManager != NULL)
    {
        g_AnmManager->flush_sprites();
    }
    g_Supervisor.d3d_device->SetTransform(D3DTS_VIEW, &g_Supervisor.cameras[2].view_matrix);
    g_Supervisor.d3d_device->SetTransform(D3DTS_PROJECTION, &g_Supervisor.cameras[2].projection_matrix);
    if (g_AnmManager != NULL)
    {
        g_AnmManager->camera_2d_offset.x = g_Supervisor.cameras[2].unk_fc.x;
        g_AnmManager->camera_2d_offset.y = g_Supervisor.cameras[2].unk_fc.y;
    }
    g_Supervisor.d3d_device->SetViewport(&g_Supervisor.current_camera->viewport);
    g_Supervisor.current_camera_index = 2;
    return 1;
}

// Group 1 is drawn with camera 0, placed from the arcade HUD origin.
// FUNCTION: TH16 0x408ef0
i32 AsciiInf::draw_group_1()
{
    g_Supervisor.current_camera = &g_Supervisor.cameras[0];
    g_Supervisor.swap_transform_matrices(&g_Supervisor.cameras[0]);
    g_Supervisor.d3d_device->SetViewport(&g_Supervisor.current_camera->viewport);
    g_Supervisor.current_camera_index = 0;
    g_AnmManager->flush_sprites();
    glyph_vm.flags_hi = glyph_vm.flags_hi & ~ANM_VM_ORIGIN_MODE_MASK | ANM_VM_ORIGIN_HUD;
    draw_group(1);
    glyph_vm.flags_hi &= ~ANM_VM_ORIGIN_MODE_MASK;
    g_AnmManager->flush_sprites();
    g_Supervisor.current_camera = &g_Supervisor.cameras[2];
    g_Supervisor.swap_transform_matrices(&g_Supervisor.cameras[2]);
    g_Supervisor.d3d_device->SetViewport(&g_Supervisor.current_camera->viewport);
    g_Supervisor.current_camera_index = 2;
    return 1;
}
