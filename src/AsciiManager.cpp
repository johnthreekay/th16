#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "AnmManager.h"
#include "AsciiManager.h"
#include "GameErrorContext.h"
#include "Supervisor.h"

// The original keeps this UCRT header function out of line (0x405540) and
// calls it with the folded NULL locale for vsprintf.
DECOMP_NOINLINE int __CRTDECL _vsprintf_l(char *buffer, const char *format, _locale_t locale, va_list args);

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
    align_h = 1;
    align_v = 1;
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

    ascii_anm = AnmManager::preload_anm(2, anm_names[g_Supervisor.config.window_size % 3]);
    if (ascii_anm == NULL)
    {
        // データが壊れています
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

    ascii_anm->init_vm_with_sprite(&vm_1, 0);
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
    g_AnmManager->unload_anm(2);
    g_AnmManager->unload_anm(0);
    g_AsciiManager = NULL;
}

// Drops the strings whose time ran out, keeping the others in order.
// TODO: same loop shape, but the induction variables land in other registers.
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

// TODO: ours passes fmt to _vsprintf_l in edx; the original pushes it.
// FUNCTION: TH16 0x4084f0
void AsciiInf::create_debug_stringf(Float3 *pos, const char *fmt, ...)
{
    char buf[0x100];
    va_list args;
    va_start(args, fmt);
    _vsprintf_l(buf, fmt, NULL, args);
    create_string(pos, buf);
    strings[num_strings - 1].font_id = 1;
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

// TODO: register allocation differs (the original keeps this in a stack slot); draw_string is still a stub.
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
        g_AnmManager->camera_unk_fc.x = g_Supervisor.cameras[2].unk_fc.x;
        g_AnmManager->camera_unk_fc.y = g_Supervisor.cameras[2].unk_fc.y;
    }
    g_Supervisor.d3d_device->SetViewport(&g_Supervisor.current_camera->viewport);
    g_Supervisor.current_camera_index = 2;
    return 1;
}

// Group 1 is drawn with layer kind 2 coordinates on the first camera.
// FUNCTION: TH16 0x408ef0
i32 AsciiInf::draw_group_1()
{
    g_Supervisor.current_camera = &g_Supervisor.cameras[0];
    g_Supervisor.swap_transform_matrices(&g_Supervisor.cameras[0]);
    g_Supervisor.d3d_device->SetViewport(&g_Supervisor.current_camera->viewport);
    g_Supervisor.current_camera_index = 0;
    g_AnmManager->flush_sprites();
    vm_1.flags_hi = vm_1.flags_hi & ~ANM_VM_LAYER_KIND_MASK | ANM_VM_LAYER_UI;
    draw_group(1);
    vm_1.flags_hi &= ~ANM_VM_LAYER_KIND_MASK;
    g_AnmManager->flush_sprites();
    g_Supervisor.current_camera = &g_Supervisor.cameras[2];
    g_Supervisor.swap_transform_matrices(&g_Supervisor.cameras[2]);
    g_Supervisor.d3d_device->SetViewport(&g_Supervisor.current_camera->viewport);
    g_Supervisor.current_camera_index = 2;
    return 1;
}
