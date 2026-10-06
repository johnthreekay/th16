#include <string.h>

#include "Supervisor.h"

#include <mmsystem.h>

#include "Input.h"

#include "CriticalSections.h"
#include "Scorefile.h"
#include "SoundManager.h"
#include "UpdateFunc.h"

// GLOBAL: TH16 0x4c10d0
Supervisor g_Supervisor;

// GLOBAL: TH16 0x4a52e4
i16 g_pad_mapping[10] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1};

// FUNCTION: TH16 0x4018e0
u32 Supervisor::read_joypad(u32 input)
{
    if (!(g_Supervisor.flags & SUPERVISOR_USE_DIRECTINPUT_PAD))
    {
        JOYINFOEX info;
        memset(&info, 0, sizeof(info));
        info.dwSize = sizeof(JOYINFOEX);
        info.dwFlags = JOY_RETURNALL;
        if (joyGetPosEx(JOYSTICKID1, &info) != JOYERR_NOERROR)
        {
            return input;
        }
        if (g_pad_mapping[0] >= 0)
        {
            input |= (info.dwButtons & (1 << g_pad_mapping[0])) ? 1 : 0;
        }
        if (g_pad_mapping[1] >= 0)
        {
            input |= (info.dwButtons & (1 << g_pad_mapping[1])) ? 2 : 0;
        }
        if (g_pad_mapping[3] >= 0)
        {
            input |= (info.dwButtons & (1 << g_pad_mapping[3])) ? 0x100 : 0;
        }
        if (g_pad_mapping[2] >= 0)
        {
            input |= (info.dwButtons & (1 << g_pad_mapping[2])) ? 8 : 0;
        }
        if (g_pad_mapping[9] >= 0)
        {
            input |= (info.dwButtons & (1 << g_pad_mapping[9])) ? 0x800 : 0;
        }
        u32 center_x = (g_joypad_caps.wXmin + g_joypad_caps.wXmax) / 2;
        u32 margin_x = (g_joypad_caps.wXmax - g_joypad_caps.wXmin) / 4;
        u32 center_y = (g_joypad_caps.wYmin + g_joypad_caps.wYmax) / 2;
        u32 margin_y = (g_joypad_caps.wYmax - g_joypad_caps.wYmin) / 4;
        input |= (info.dwXpos > center_x + margin_x) ? 0x80 : 0;
        input |= (info.dwXpos < center_x - margin_x) ? 0x40 : 0;
        input |= (info.dwYpos > center_y + margin_y) ? 0x20 : 0;
        input |= (info.dwYpos < center_y - margin_y) ? 0x10 : 0;
        return input;
    }

    if (FAILED(g_Supervisor.joystick->Poll()))
    {
        HRESULT hr = g_Supervisor.joystick->Acquire();
        for (i32 i = 0; hr == DIERR_INPUTLOST && i < 400; i++)
        {
            hr = g_Supervisor.joystick->Acquire();
        }
        return input;
    }

    DIJOYSTATE2 js;
    memset(&js, 0, sizeof(js));
    if (FAILED(g_Supervisor.joystick->GetDeviceState(sizeof(DIJOYSTATE2), &js)))
    {
        return input;
    }
    if (g_pad_mapping[0] >= 0)
    {
        input |= (js.rgbButtons[g_pad_mapping[0]] & 0x80) ? 1 : 0;
    }
    if (g_pad_mapping[1] >= 0)
    {
        input |= (js.rgbButtons[g_pad_mapping[1]] & 0x80) ? 2 : 0;
    }
    if (g_pad_mapping[3] >= 0)
    {
        input |= (js.rgbButtons[g_pad_mapping[3]] & 0x80) ? 0x100 : 0;
    }
    if (g_pad_mapping[2] >= 0)
    {
        input |= (js.rgbButtons[g_pad_mapping[2]] & 0x80) ? 8 : 0;
    }
    if (g_pad_mapping[9] >= 0)
    {
        input |= (js.rgbButtons[g_pad_mapping[9]] & 0x80) ? 0x800 : 0;
    }
    return input | ((js.lX < -g_Supervisor.config.deadzone_x) ? 0x40 : 0) |
           ((js.lY < -g_Supervisor.config.deadzone_y) ? 0x10 : 0) |
           ((js.lX > g_Supervisor.config.deadzone_x) ? 0x80 : 0) |
           ((js.lY > g_Supervisor.config.deadzone_y) ? 0x20 : 0);
}

// GLOBAL: TH16 0x4d9d2c
i32 g_resolution_x;
// GLOBAL: TH16 0x4d9d30
i32 g_resolution_y;
// GLOBAL: TH16 0x4d9d34
f32 g_screen_coord_scale;
// GLOBAL: TH16 0x4d9d38
i32 g_early_arcade_offset_x;
// GLOBAL: TH16 0x4d9d3c
i32 g_early_arcade_offset_y;
// GLOBAL: TH16 0x4d9d40
i32 g_arcade_height;
// GLOBAL: TH16 0x4d9d44
i32 g_arcade_width;
// GLOBAL: TH16 0x4d9d50
i32 g_game_2d_origin_x;
// GLOBAL: TH16 0x4d9d54
i32 g_game_2d_origin_y;
// GLOBAL: TH16 0x4c0f4c
AnmId g_anm_ids_4c0f4c[3];
// GLOBAL: TH16 0x4a6ef0
i32 g_unk_4a6ef0;
// GLOBAL: TH16 0x4a6ee8
void (*g_draw_hook_4a6ee8)();

// FUNCTION: TH16 0x43c4b0
HRESULT Supervisor::enable_d3d_fog()
{
    if (fog_enabled != 1)
    {
        g_AnmManager->flush_sprites();
        fog_enabled = 1;
        return d3d_device->SetRenderState(D3DRS_FOGENABLE, TRUE);
    }
    return 0;
}

// FUNCTION: TH16 0x43c4f0
HRESULT Supervisor::disable_d3d_fog()
{
    if (fog_enabled != 0)
    {
        g_AnmManager->flush_sprites();
        fog_enabled = 0;
        return d3d_device->SetRenderState(D3DRS_FOGENABLE, FALSE);
    }
    return 0;
}

// FUNCTION: TH16 0x43c530
HRESULT Supervisor::enable_zwrite()
{
    if (zwrite_enabled != 1)
    {
        g_AnmManager->flush_sprites();
        zwrite_enabled = 1;
        return d3d_device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
    }
    return 0;
}

// FUNCTION: TH16 0x43c570
HRESULT Supervisor::disable_zwrite()
{
    if (zwrite_enabled != 0)
    {
        g_AnmManager->flush_sprites();
        zwrite_enabled = 0;
        return d3d_device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    }
    return 0;
}

// FUNCTION: TH16 0x43c630
void Supervisor::sub_43c630()
{
    if (unk_9b8 == 1)
    {
        AnmManager::interrupt_tree(g_anm_ids_4c0f4c[0], 1);
        AnmManager::interrupt_tree(g_anm_ids_4c0f4c[1], 1);
        AnmManager::interrupt_tree(g_anm_ids_4c0f4c[2], 1);
        g_anm_ids_4c0f4c[0].id = 0;
        g_anm_ids_4c0f4c[1].id = 0;
        g_anm_ids_4c0f4c[2].id = 0;
        unk_9b8 = 0;
    }
    g_unk_4a6ef0 = 0;
}

// FUNCTION: TH16 0x43c6a0
void Supervisor::sub_43c6a0()
{
    if (unk_9b8 == 1)
    {
        AnmManager::interrupt_tree(g_anm_ids_4c0f4c[0], 2);
        AnmManager::interrupt_tree(g_anm_ids_4c0f4c[1], 2);
        AnmManager::interrupt_tree(g_anm_ids_4c0f4c[2], 2);
        g_anm_ids_4c0f4c[0].id = 0;
        g_anm_ids_4c0f4c[1].id = 0;
        g_anm_ids_4c0f4c[2].id = 0;
        unk_9b8 = 2;
    }
    g_unk_4a6ef0 = 0;
}

// FUNCTION: TH16 0x43c710
HARNESS_CALLED void Supervisor::swap_transform_matrices(Camera *camera)
{
    if (g_AnmManager != NULL)
    {
        g_AnmManager->flush_sprites();
    }
    g_Supervisor.d3d_device->SetTransform(D3DTS_VIEW, &camera->view_matrix);
    g_Supervisor.d3d_device->SetTransform(D3DTS_PROJECTION, &camera->projection_matrix);
    if (g_AnmManager != NULL)
    {
        g_AnmManager->camera_unk_fc = camera->unk_fc;
    }
}

// FUNCTION: TH16 0x43d400
int __fastcall Supervisor::on_draw_0e(void *arg)
{
    if (g_Supervisor.arcade_surface_0 != NULL)
    {
        g_AnmManager->flush_sprites();
        g_Supervisor.d3d_device->SetRenderTarget(0, g_Supervisor.arcade_surface_1);
        D3DVIEWPORT9 &vp = g_Supervisor.cameras[3].viewport;
        D3DRECT rect;
        rect.x1 = vp.X;
        rect.y1 = vp.Y;
        rect.x2 = vp.X + vp.Width;
        rect.y2 = vp.Y + vp.Height;
        g_Supervisor.d3d_device->Clear(1, &rect, D3DCLEAR_ZBUFFER, g_Supervisor.background_color, 1.0f, 0);
    }
    return 1;
}

// FUNCTION: TH16 0x43d4a0
int __fastcall Supervisor::on_draw_1a(void *arg)
{
    if (g_Supervisor.arcade_surface_0 != NULL)
    {
        if (g_draw_hook_4a6ee8 == NULL)
        {
            g_Supervisor.d3d_device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
            g_Supervisor.d3d_device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
            g_Supervisor.disable_zwrite();
            g_Supervisor.current_camera = &g_Supervisor.cameras[3];
            g_Supervisor.swap_transform_matrices(&g_Supervisor.cameras[3]);
            g_Supervisor.d3d_device->SetViewport(&g_Supervisor.current_camera->viewport);
            g_Supervisor.current_camera_index = 3;
            g_AnmManager->draw_vm(g_Supervisor.vm_1c0);
            g_Supervisor.vm_1c0->color_1.d3d = 0xffffffff;
            g_AnmManager->flush_sprites();
            g_Supervisor.d3d_device->SetRenderState(D3DRS_ALPHATESTENABLE, TRUE);
        }
        else
        {
            g_draw_hook_4a6ee8();
        }
    }
    return 1;
}

// FUNCTION: TH16 0x43d5a0
int __fastcall Supervisor::on_draw_19(void *arg)
{
    if (g_Supervisor.arcade_surface_0 != NULL)
    {
        g_AnmManager->flush_sprites();
        g_Supervisor.d3d_device->SetRenderTarget(0, g_Supervisor.arcade_surface_0);
        D3DVIEWPORT9 &vp = g_Supervisor.cameras[3].viewport;
        D3DRECT rect;
        rect.x1 = vp.X;
        rect.y1 = vp.Y;
        rect.x2 = vp.X + vp.Width;
        rect.y2 = vp.Y + vp.Height;
        g_Supervisor.d3d_device->Clear(1, &rect, D3DCLEAR_ZBUFFER, g_Supervisor.background_color, 1.0f, 0);
    }
    return 1;
}

// FUNCTION: TH16 0x43d640
int __fastcall Supervisor::on_draw_2c(void *arg)
{
    if (g_Supervisor.arcade_surface_0 != NULL)
    {
        g_Supervisor.d3d_device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
        g_Supervisor.d3d_device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
        g_Supervisor.disable_zwrite();
        g_AnmManager->draw_vm(g_Supervisor.vm_1c4);
        g_AnmManager->flush_sprites();
        g_Supervisor.vm_1c4->color_1.d3d = 0xffffffff;
        g_Supervisor.d3d_device->SetRenderState(D3DRS_ALPHATESTENABLE, TRUE);
    }
    return 1;
}

// FUNCTION: TH16 0x43d6f0
int __fastcall Supervisor::on_draw_2b(void *arg)
{
    if (g_Supervisor.arcade_surface_0 != NULL)
    {
        g_AnmManager->flush_sprites();
        g_Supervisor.d3d_device->SetRenderTarget(0, g_Supervisor.arcade_surface_1);
        D3DVIEWPORT9 &vp = g_Supervisor.cameras[1].viewport;
        D3DRECT rect;
        rect.x1 = vp.X;
        rect.y1 = vp.Y;
        rect.x2 = vp.X + vp.Width;
        rect.y2 = vp.Y + vp.Height;
        g_Supervisor.d3d_device->Clear(1, &rect, D3DCLEAR_ZBUFFER, g_Supervisor.background_color, 1.0f, 0);
        g_arcade_width = g_screen_coord_scale * 384.0f;
        g_arcade_height = g_screen_coord_scale * 448.0f;
    }
    return 1;
}

// FUNCTION: TH16 0x43d7c0
int __fastcall Supervisor::on_draw_39(void *arg)
{
    if (g_Supervisor.arcade_surface_0 != NULL)
    {
        g_Supervisor.d3d_device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
        g_AnmManager->draw_vm(g_Supervisor.vm_1c8);
        g_Supervisor.vm_1c8->color_1.d3d = 0xffffffff;
        g_AnmManager->flush_sprites();
        g_Supervisor.d3d_device->SetRenderState(D3DRS_ALPHATESTENABLE, TRUE);
    }
    return 1;
}

// FUNCTION: TH16 0x43d820
int __fastcall Supervisor::on_draw_38(void *arg)
{
    g_AnmManager->flush_sprites();
    g_Supervisor.d3d_device->SetRenderTarget(0, g_Supervisor.back_buffer);
    g_Supervisor.d3d_device->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0xff000000, 1.0f, 0);
    return 1;
}

// FUNCTION: TH16 0x43d870
int __fastcall Supervisor::on_draw_55(void *arg)
{
    g_AnmManager->flush_sprites();
    g_Supervisor.cameras[3].unk_fc.x = 0.0f;
    g_Supervisor.cameras[3].unk_fc.y = 0.0f;
    g_Supervisor.cameras[1].unk_fc.x = 0.0f;
    g_Supervisor.cameras[1].unk_fc.y = 0.0f;
    return 1;
}

// FUNCTION: TH16 0x43dc30
void Supervisor::release_surfaces()
{
    if (g_Supervisor.arcade_surface_0 != NULL)
    {
        g_Supervisor.arcade_surface_0->Release();
        g_Supervisor.arcade_surface_0 = NULL;
    }
    if (g_Supervisor.arcade_surface_1 != NULL)
    {
        g_Supervisor.arcade_surface_1->Release();
        g_Supervisor.arcade_surface_1 = NULL;
    }
    if (g_Supervisor.back_buffer != NULL)
    {
        g_Supervisor.back_buffer->Release();
        g_Supervisor.back_buffer = NULL;
    }
    g_Supervisor.arcade_surface_0 = NULL;
}

// GLOBAL: TH16 0x4d9d20
i32 g_unk_4d9d20;
// GLOBAL: TH16 0x4d9d90
i32 g_unk_4d9d90;
// GLOBAL: TH16 0x4a5788
f32 g_game_speed;

// FUNCTION: TH16 0x43b3d0
int __fastcall Supervisor::on_tick(void *arg)
{
    Supervisor *s = (Supervisor *)arg;

    if ((s->flags & 0x180) == 0x80 && !s->thread.should_run)
    {
        g_Supervisor.gamemode_to_switch_to = 3;
    }
    SoundManager::update_sound_thread();
    SoundManager::tick_bgm_fade();
    read_keyboard_input();
    if (AnmManager::sub_46d690())
    {
        return UPDATE_FUNC_EXIT_SUCCESS;
    }
    if (g_unk_4d9d20 != 0)
    {
        g_unk_4d9d20--;
    }
    if (s->unk_9b4 != 0)
    {
        return s->unk_9b4 == 2 ? UPDATE_FUNC_EXIT_SUCCESS : UPDATE_FUNC_CONTINUE;
    }
    int result = s->switch_gamemodes();
    if (result == 1)
    {
        g_game_2d_origin_x = g_resolution_x / 2;
        g_game_2d_origin_y = (g_resolution_y - 448) / 2;
        return 1;
    }
    return result;
}

// FUNCTION: TH16 0x43ba40
int Supervisor::initialize()
{
    UpdateFunc *f;
    int result;

    g_Supervisor.gamemode_current = -2;
    g_Supervisor.gamemode_to_switch_to = 0;
    g_Supervisor.unk_6fc = 0;

    f = g_UpdateFuncRegistry->create_func(on_tick);
    f->flags |= UPDATE_FUNC_ACTIVE;
    f->arg = &g_Supervisor;
    f->on_registration = on_registration;
    result = g_UpdateFuncRegistry->register_on_tick(f, 1);
    if (result != 0)
    {
        return result;
    }

    f = g_UpdateFuncRegistry->create_func(on_draw_01);
    f->flags |= UPDATE_FUNC_ACTIVE;
    f->arg = &g_Supervisor;
    g_UpdateFuncRegistry->register_on_draw(f, 1);

    f = g_UpdateFuncRegistry->create_func(on_draw_0e);
    f->flags |= UPDATE_FUNC_ACTIVE;
    f->arg = &g_Supervisor;
    g_UpdateFuncRegistry->register_on_draw(f, 0xe);

    f = g_UpdateFuncRegistry->create_func(on_draw_0f);
    f->flags |= UPDATE_FUNC_ACTIVE;
    f->arg = &g_Supervisor;
    g_UpdateFuncRegistry->register_on_draw(f, 0xf);

    f = g_UpdateFuncRegistry->create_func(on_draw_19);
    f->flags |= UPDATE_FUNC_ACTIVE;
    f->arg = &g_Supervisor;
    g_UpdateFuncRegistry->register_on_draw(f, 0x19);

    f = g_UpdateFuncRegistry->create_func(on_draw_1a);
    f->flags |= UPDATE_FUNC_ACTIVE;
    f->arg = &g_Supervisor;
    g_UpdateFuncRegistry->register_on_draw(f, 0x1a);

    f = g_UpdateFuncRegistry->create_func(on_draw_2b);
    f->flags |= UPDATE_FUNC_ACTIVE;
    f->arg = &g_Supervisor;
    g_UpdateFuncRegistry->register_on_draw(f, 0x2b);

    f = g_UpdateFuncRegistry->create_func(on_draw_2c);
    f->flags |= UPDATE_FUNC_ACTIVE;
    f->arg = &g_Supervisor;
    g_UpdateFuncRegistry->register_on_draw(f, 0x2c);

    f = g_UpdateFuncRegistry->create_func(on_draw_38);
    f->flags |= UPDATE_FUNC_ACTIVE;
    f->arg = &g_Supervisor;
    g_UpdateFuncRegistry->register_on_draw(f, 0x38);

    f = g_UpdateFuncRegistry->create_func(on_draw_39);
    f->flags |= UPDATE_FUNC_ACTIVE;
    f->arg = &g_Supervisor;
    g_UpdateFuncRegistry->register_on_draw(f, 0x39);

    f = g_UpdateFuncRegistry->create_func(on_draw_55);
    f->flags |= UPDATE_FUNC_ACTIVE;
    f->arg = &g_Supervisor;
    g_UpdateFuncRegistry->register_on_draw(f, 0x55);

    g_Supervisor.d3d_device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &g_Supervisor.back_buffer);
    return 0;
}

// TODO: the original stores ".wav" as an immediate (no literal in .rdata);
// ours copies it from a literal.
// FUNCTION: TH16 0x43c370
i32 Supervisor::play_bgm_wav(i32 arg, const char *name)
{
    char path[256];

    strcpy(path, name);
    strcat(path, ".wav");
    g_SoundManager.modify_bgm(BGM_PLAY_WAV, arg, path);
    return 1;
}

// FUNCTION: TH16 0x43c3f0
i32 Supervisor::play_bgm(i32 arg, i32 track)
{
    if (g_Supervisor.config.flags_2c & 0x10)
    {
        g_SoundManager.modify_bgm(BGM_STOP_4, 0, "dummy");
    }
    g_SoundManager.modify_bgm(BGM_PLAY, arg, "dummy");
    g_Scorefile->bgm_unlocked[track] = 1;
    return 0;
}

// FUNCTION: TH16 0x43c440
i32 Supervisor::stop_bgm()
{
    if (g_Supervisor.config.flags_2c & 0x10)
    {
        g_SoundManager.modify_bgm(BGM_STOP_4, 0, "dummy");
    }
    else
    {
        g_SoundManager.modify_bgm(BGM_STOP, 0, "dummy");
    }
    return 0;
}

// FUNCTION: TH16 0x43c470
HARNESS_CALLED i32 Supervisor::fade_out_bgm(f32 seconds)
{
    if (g_game_speed != 0.0f && !(g_game_speed > 1.0f))
    {
        seconds /= g_game_speed;
    }
    g_SoundManager.modify_bgm(BGM_FADE_OUT, seconds, "");
    return 0;
}

// FUNCTION: TH16 0x43c5b0
HARNESS_CALLED i32 Supervisor::start_thread(ThreadStart start, void *arg)
{
    ENTER_CS(CS_SUPERVISOR_THREAD);
    g_Supervisor.thread.restart(start, arg);
    LEAVE_CS(CS_SUPERVISOR_THREAD);
    return 0;
}

// FUNCTION: TH16 0x43dce0
void debug_log_43dce0(const char *fmt, ...)
{
}
