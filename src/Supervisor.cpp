#include <stdio.h>
#include <direct.h>
#include <process.h>
#include <stdlib.h>
#include <string.h>

#include "Supervisor.h"

#include <mmsystem.h>

#include "Arcfile.h"
#include "FileSystem.h"
#include "Fog.h"
#include "GameWindow.h"
#include "GameErrorContext.h"
#include "Input.h"
#include "EffectManager.h"
#include "Ending.h"
#include "GameThread.h"
#include "Globals.h"
#include "HelpManual.h"
#include "LoadingThread.h"
#include "ReplayManager.h"
#include "MainMenu.h"

#include "CriticalSections.h"
#include "FpsCounter.h"
#include "Rng.h"
#include "Scorefile.h"
#include "StageData.h"
#include "SoundManager.h"
#include "TextHelper.h"
#include "UpdateFunc.h"

static_assert(offsetof(Supervisor, d3d_device) == 0x8, "Supervisor::d3d_device");
static_assert(offsetof(Supervisor, full_window_viewport) == 0xdc, "Supervisor::full_window_viewport");
static_assert(offsetof(Supervisor, present_params) == 0xf4, "Supervisor::present_params");
static_assert(offsetof(Supervisor, arcade_surface_0) == 0x1ac, "Supervisor::arcade_surface_0");
static_assert(offsetof(Supervisor, new_game_started) == 0x700, "Supervisor::new_game_started");
static_assert(offsetof(Supervisor, no_vsync) == 0x71c, "Supervisor::no_vsync");
static_assert(offsetof(Supervisor, text_anm) == 0x728, "Supervisor::text_anm");
static_assert(offsetof(Supervisor, flags) == 0x730, "Supervisor::flags");
static_assert(offsetof(Supervisor, start_time) == 0x734, "Supervisor::start_time");
static_assert(offsetof(Supervisor, screenshot) == 0x870, "Supervisor::screenshot");
static_assert(offsetof(Supervisor, thread) == 0x998, "Supervisor::thread");
static_assert(offsetof(Supervisor, loading_thread) == 0xa24, "Supervisor::loading_thread");
static_assert(offsetof(Supervisor, main_window) == 0x58, "Supervisor::main_window");
static_assert(offsetof(Supervisor, display_mode) == 0x19c, "Supervisor::display_mode");
static_assert(offsetof(Supervisor, unk_714) == 0x714, "Supervisor::unk_714");
static_assert(offsetof(Supervisor, exe_size) == 0xa18, "Supervisor::exe_size");
static_assert(offsetof(Supervisor, ver_file_size) == 0xa1c, "Supervisor::ver_file_size");
static_assert(offsetof(Supervisor, frame_time) == 0xa34, "Supervisor::frame_time");
static_assert(offsetof(Supervisor, background_color) == 0xa3c, "Supervisor::background_color");
static_assert(offsetof(Config, color_mode) == 0x20, "Config::color_mode");
static_assert(offsetof(Config, frame_pacing) == 0x29, "Config::frame_pacing");
static_assert(offsetof(Config, flags) == 0x2c, "Config::flags");
static_assert(offsetof(GameWindow, save_dir) == 0x2d, "GameWindow::save_dir");
static_assert(offsetof(SoundManager, bgm_dat_name) == 0x5560, "SoundManager::bgm_dat_name");

Config::Config()
{
    memset(this, 0, sizeof(Config));
    flags |= CONFIG_SHOW_STARTUP_DIALOG;
    color_mode = 0;
    bgm_mode = 1;
    version = CONFIG_VERSION;
    deadzone_x = deadzone_y = 600;
    se_enabled = 1;
    window_size = WINDOW_SIZE_WINDOWED_1280;
    frame_skip = 0;
    memcpy(pad_mapping, g_pad_mapping, sizeof(pad_mapping));
    unk_25 = 2;
    bgm_volume = 100;
    unk_28 = 0;
    frame_pacing = 2;
    se_volume = 80;
    window_x = CW_USEDEFAULT;
    window_y = CW_USEDEFAULT;
}

// FUNCTION: TH16 0x40d510
Camera::Camera()
{
}

Supervisor::Supervisor()
{
    memset(this, 0, sizeof(Supervisor));
    // SUPERVISOR_NO_STARTUP_TITLE and the unread 0x200 and 0x4000.
    flags |= 0x4240;
}

// GLOBAL: TH16 0x4c10d0
Supervisor g_Supervisor;
// TODO: the original keeps the Config constructor's memset and stores before the Camera constructor calls; ours drops them as overwritten by Supervisor's memset.
// SYNTHETIC: TH16 0x401030
// ??__Eg_Supervisor@@YAXXZ
// SYNTHETIC: TH16 0x48ac40
// ??__Fg_Supervisor@@YAXXZ


// The default joypad buttons (th16.cfg starts with a copy): shot on button
// 0, bomb 1, focus 2, pause 5, season release 3; -1 for the unused slots.
// GLOBAL: TH16 0x4a52e4
i16 g_pad_mapping[10] = {0, 1, 2, 5, -1, -1, -1, -1, -1, 3};

// Adds the buttons and directions held on the first game controller (winmm
// or DirectInput) to input. The Acquire retries are written like
// get_controller_state's: that keeps the joyGetPosEx failure's return as
// the shared copy the later returns jump to, as in the original.
// TODO: 74%; the original loads some mapping words through ax and a 16-bit
// stack temporary (ours: cx), and keeps input in a different stack slot.
// FUNCTION: TH16 0x4018e0
HARNESS_CALLED u32 Supervisor::read_joypad(u32 input)
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
        if (g_pad_mapping[PAD_SHOT] >= 0)
        {
            input |= (info.dwButtons & (1 << g_pad_mapping[PAD_SHOT])) ? INPUT_SHOT : 0;
        }
        if (g_pad_mapping[PAD_BOMB] >= 0)
        {
            input |= (info.dwButtons & (1 << g_pad_mapping[PAD_BOMB])) ? INPUT_BOMB : 0;
        }
        if (g_pad_mapping[PAD_PAUSE] >= 0)
        {
            input |= (info.dwButtons & (1 << g_pad_mapping[PAD_PAUSE])) ? INPUT_MENU : 0;
        }
        if (g_pad_mapping[PAD_FOCUS] >= 0)
        {
            input |= (info.dwButtons & (1 << g_pad_mapping[PAD_FOCUS])) ? INPUT_FOCUS : 0;
        }
        if (g_pad_mapping[PAD_RELEASE] >= 0)
        {
            input |= (info.dwButtons & (1 << g_pad_mapping[PAD_RELEASE])) ? INPUT_RELEASE : 0;
        }
        u32 center_x = (g_joypad_caps.wXmin + g_joypad_caps.wXmax) / 2;
        u32 margin_x = (g_joypad_caps.wXmax - g_joypad_caps.wXmin) / 4;
        u32 center_y = (g_joypad_caps.wYmin + g_joypad_caps.wYmax) / 2;
        u32 margin_y = (g_joypad_caps.wYmax - g_joypad_caps.wYmin) / 4;
        input |= (info.dwXpos > center_x + margin_x) ? INPUT_RIGHT : 0;
        input |= (info.dwXpos < center_x - margin_x) ? INPUT_LEFT : 0;
        input |= (info.dwYpos > center_y + margin_y) ? INPUT_DOWN : 0;
        input |= (info.dwYpos < center_y - margin_y) ? INPUT_UP : 0;
        return input;
    }

    if (FAILED(g_Supervisor.joystick->Poll()))
    {
        i32 retries = 0;
        HRESULT hr = g_Supervisor.joystick->Acquire();
        while (hr == DIERR_INPUTLOST)
        {
            hr = g_Supervisor.joystick->Acquire();
            retries++;
            if (retries >= 400)
            {
                return input;
            }
        }
        return input;
    }

    DIJOYSTATE2 js;
    memset(&js, 0, sizeof(js));
    if (FAILED(g_Supervisor.joystick->GetDeviceState(sizeof(DIJOYSTATE2), &js)))
    {
        return input;
    }
    if (g_pad_mapping[PAD_SHOT] >= 0)
    {
        input |= (js.rgbButtons[g_pad_mapping[PAD_SHOT]] & 0x80) ? INPUT_SHOT : 0;
    }
    if (g_pad_mapping[PAD_BOMB] >= 0)
    {
        input |= (js.rgbButtons[g_pad_mapping[PAD_BOMB]] & 0x80) ? INPUT_BOMB : 0;
    }
    if (g_pad_mapping[PAD_PAUSE] >= 0)
    {
        input |= (js.rgbButtons[g_pad_mapping[PAD_PAUSE]] & 0x80) ? INPUT_MENU : 0;
    }
    if (g_pad_mapping[PAD_FOCUS] >= 0)
    {
        input |= (js.rgbButtons[g_pad_mapping[PAD_FOCUS]] & 0x80) ? INPUT_FOCUS : 0;
    }
    if (g_pad_mapping[PAD_RELEASE] >= 0)
    {
        input |= (js.rgbButtons[g_pad_mapping[PAD_RELEASE]] & 0x80) ? INPUT_RELEASE : 0;
    }
    return input | ((js.lX < -g_Supervisor.config.deadzone_x) ? INPUT_LEFT : 0) |
           ((js.lY < -g_Supervisor.config.deadzone_y) ? INPUT_UP : 0) |
           ((js.lX > g_Supervisor.config.deadzone_x) ? INPUT_RIGHT : 0) |
           ((js.lY > g_Supervisor.config.deadzone_y) ? INPUT_DOWN : 0);
}

// GLOBAL: TH16 0x4c0f4c
AnmId g_stage_load_anm_ids[3];
// GLOBAL: TH16 0x4a6ef0
i32 g_unk_4a6ef0;
// GLOBAL: TH16 0x4a6ee8
void (*g_draw_hook_1a)();
// GLOBAL: TH16 0x4a6eec
void (*g_draw_hook_0f)();

// The fog and z-write switches: each flushes the sprite batch and changes
// the render state only if it differs from the cached one.
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
void Supervisor::end_stage_load_anms()
{
    if (stage_load_anm_state == 1)
    {
        AnmManager::interrupt_tree(g_stage_load_anm_ids[0], 1);
        AnmManager::interrupt_tree(g_stage_load_anm_ids[1], 1);
        AnmManager::interrupt_tree(g_stage_load_anm_ids[2], 1);
        g_stage_load_anm_ids[0].id = 0;
        g_stage_load_anm_ids[1].id = 0;
        g_stage_load_anm_ids[2].id = 0;
        stage_load_anm_state = 0;
    }
    g_unk_4a6ef0 = 0;
}

// FUNCTION: TH16 0x43c6a0
void Supervisor::abort_stage_load_anms()
{
    if (stage_load_anm_state == 1)
    {
        AnmManager::interrupt_tree(g_stage_load_anm_ids[0], 2);
        AnmManager::interrupt_tree(g_stage_load_anm_ids[1], 2);
        AnmManager::interrupt_tree(g_stage_load_anm_ids[2], 2);
        g_stage_load_anm_ids[0].id = 0;
        g_stage_load_anm_ids[1].id = 0;
        g_stage_load_anm_ids[2].id = 0;
        stage_load_anm_state = 2;
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
        g_AnmManager->camera_2d_offset = camera->shake_offset;
    }
}


// The tanf from the CRT headers stays out of line (0x43dc90).
DECOMP_NOINLINE float __CRTDECL tanf(float);

// TODO: the original keeps x and y in registers across the tanf call and
// stores them to eye and at afterwards as one packed pair (unpcklps, movq);
// ours stores eye.x and eye.y before the call.
// FUNCTION: TH16 0x43c780
void __stdcall camera_update_2d(Camera *camera)
{
    D3DXVECTOR3 eye;
    eye.x = camera->viewport.X + camera->viewport.Width * 0.5f;
    eye.y = camera->viewport.Height * 0.5f + camera->viewport.Y;
    f32 half_height = camera->viewport.Height / 2;
    eye.z = half_height / tanf(camera->field_of_view * 0.5f);
    D3DXVECTOR3 up(0.0f, -1.0f, 0.0f);
    D3DXVECTOR3 at(eye.x, eye.y, 0.0f);
    D3DXMatrixLookAtLH((D3DXMATRIX *)&camera->view_matrix, &eye, &at, &up);
    D3DXMatrixPerspectiveFovLH((D3DXMATRIX *)&camera->projection_matrix, camera->field_of_view,
                               (f32)camera->viewport.Width / (f32)camera->viewport.Height, 1.0f, 10000.0f);
}

// TODO: the original keeps &camera->up in ebx across the LookAt call; ours
// recomputes it (whole-program effect: this once matched while
// write_screenshot did not exist, but leaving it out no longer helps).
// FUNCTION: TH16 0x43c940
void __stdcall camera_apply_3d(Camera *camera)
{
    if (g_AnmManager != NULL)
    {
        g_AnmManager->flush_sprites();
    }
    D3DXVECTOR3 eye = camera->rocking_vector_1 + camera->position;
    D3DXVECTOR3 facing = camera->facing_normalized;
    D3DXVECTOR3 at = facing + eye;
    D3DXMatrixLookAtLH((D3DXMATRIX *)&camera->view_matrix, &eye, &at, &camera->up);
    D3DXMatrixPerspectiveFovLH((D3DXMATRIX *)&camera->projection_matrix, camera->field_of_view,
                               (f32)camera->viewport.Width / (f32)camera->viewport.Height, 30.0f, 8000.0f);
    g_Supervisor.d3d_device->SetTransform(D3DTS_VIEW, &camera->view_matrix);
    g_Supervisor.d3d_device->SetTransform(D3DTS_PROJECTION, &camera->projection_matrix);
    D3DXVec3Cross(&camera->right, &facing, &camera->up);
    D3DXVec3Normalize(&camera->right, &camera->right);
    if (g_AnmManager != NULL)
    {
        g_AnmManager->camera_2d_offset = camera->shake_offset;
    }
}

// FUNCTION: TH16 0x43cb10
void Supervisor::setup_cameras()
{
    cameras[2].position = D3DXVECTOR3(0.0f, 0.0f, 0.0f);
    cameras[2].facing = D3DXVECTOR3(0.0f, 0.0f, 0.0f);
    cameras[2].up = D3DXVECTOR3(0.0f, 1.0f, 0.0f);
    cameras[2].field_of_view = ZUN_PI / 6.0f;
    cameras[2].viewport.X = 0;
    cameras[2].viewport.Y = 0;
    cameras[2].viewport.Width = g_resolution_x;
    cameras[2].viewport.Height = g_resolution_y;
    if ((g_window_flags & WINDOW_SIZE_MASK) == WINDOW_SIZE_FULLSCREEN_1280 << WINDOW_SIZE_SHIFT)
    {
        cameras[2].viewport.Height = 960;
    }
    cameras[2].rocking_vector_1 = D3DXVECTOR3(0.0f, 0.0f, 0.0f);
    cameras[2].rocking_vector_2 = D3DXVECTOR3(0.0f, 0.0f, 0.0f);
    cameras[2].viewport.MinZ = 0.0f;
    cameras[2].viewport.MaxZ = 1.0f;
    cameras[2].camera_index = 2;
    cameras[2].window_resolution[0] = g_resolution_x;
    cameras[2].window_resolution[1] = g_resolution_y;
    camera_update_2d(&cameras[2]);

    cameras[0] = cameras[2];
    cameras[0].camera_index = 0;
    cameras[0].viewport.X = (i32)(g_screen_coord_scale * 32.0f);
    cameras[0].viewport.Y = (i32)(g_screen_coord_scale * 16.0f);
    cameras[0].viewport.Width = (i32)(g_screen_coord_scale * 384.0f);
    cameras[0].viewport.Height = (i32)(g_screen_coord_scale * 448.0f);
    camera_update_2d(&cameras[0]);

    cameras[1] = cameras[0];
    cameras[1].camera_index = 1;
    cameras[1].viewport.X = (i32)(g_screen_coord_scale * 128.0f);
    cameras[1].viewport.Y = (i32)(g_screen_coord_scale * 16.0f);
    cameras[1].viewport.Width = (i32)(g_screen_coord_scale * 384.0f);
    cameras[1].viewport.Height = (i32)(g_screen_coord_scale * 448.0f);
    camera_update_2d(&cameras[1]);

    cameras[3] = cameras[0];
    cameras[3].camera_index = 3;
    cameras[3].viewport.X = (i32)((g_resolution_x - 408.0f) * 0.5f);
    cameras[3].viewport.Y = (i32)((g_resolution_y - 472.0f) * 0.5f);
    cameras[3].viewport.Width = 408;
    cameras[3].viewport.Height = 472;
    camera_update_2d(&cameras[3]);

    g_arcade_hud_origin_x = g_resolution_x / 2;
    g_arcade_hud_origin_y = (i32)(g_screen_coord_scale * 16.0f);
}

// GLOBAL: TH16 0x4a6f1c
i32 g_title_return_point;

// FUNCTION: TH16 0x43ce10
int Supervisor::switch_gamemodes()
{
    if (gamemode_current == gamemode_to_switch_to)
    {
        return UPDATE_FUNC_CONTINUE;
    }
    ENTER_CS(CS_SUPERVISOR_GAMEMODE);
    gamemode_prev = gamemode_current;
    background_color = 0xff000000;
    switch (gamemode_to_switch_to)
    {
    case GAMEMODE_STARTUP:
        gamemode_to_switch_to = GAMEMODE_LOADING;
        loading_thread = LoadingThread::create();
        if (loading_thread != NULL)
        {
            break;
        }
        gamemode_to_switch_to = GAMEMODE_QUIT;
        // Falls through.
    case GAMEMODE_QUIT:
        destroy_game_objects();
        g_CriticalSections.leave(CS_SUPERVISOR_GAMEMODE);
        return UPDATE_FUNC_EXIT_SUCCESS;
    case GAMEMODE_TITLE:
        switch (gamemode_current)
        {
        case GAMEMODE_LOADING:
        case GAMEMODE_IDLE:
            TitleInf::create();
            break;
        case GAMEMODE_GAME:
            GameThread::destroy();
            TitleInf::create();
            break;
        case GAMEMODE_ENDING:
            Ending::destroy();
            TitleInf::create();
            break;
        }
        break;
    case GAMEMODE_TITLE_SCORE_ENTRY:
        // Only from a game, the ending or GAMEMODE_IDLE; from anything else
        // the mode just becomes GAMEMODE_TITLE_SCORE_ENTRY.
        switch (gamemode_current)
        {
        case GAMEMODE_IDLE:
            break;
        case GAMEMODE_GAME:
            GameThread::destroy();
            break;
        case GAMEMODE_ENDING:
            Ending::destroy();
            break;
        default:
            goto done;
        }
        gamemode_to_switch_to = GAMEMODE_TITLE;
        g_title_return_point = TITLE_RETURN_SCORE_ENTRY;
        TitleInf::create();
        break;
    case GAMEMODE_GAME:
        if (gamemode_current == GAMEMODE_TITLE)
        {
            TitleInf::destroy();
        }
        new_game_started = 1;
        GameThread::create(0); // playing
        break;
    case GAMEMODE_START_REPLAY:
        if (gamemode_current == GAMEMODE_TITLE)
        {
            TitleInf::destroy();
        }
        gamemode_to_switch_to = GAMEMODE_GAME;
        new_game_started = 1;
        GameThread::create(1); // a replay
        break;
    case GAMEMODE_NEXT_STAGE:
    {
        i32 replay_mode = g_GameThread->replay_mode;
        new_game_started = 0;
        if (gamemode_current == GAMEMODE_GAME)
        {
            GameThread::destroy();
        }
        gamemode_to_switch_to = GAMEMODE_GAME;
        GameThread::create(replay_mode);
        break;
    }
    case GAMEMODE_RESTART:
        GameThread::destroy();
        new_game_started = 1;
        unk_704 = 0;
        gamemode_to_switch_to = GAMEMODE_GAME;
        g_Globals.stage_num = g_Globals.weird_stage_num;
        g_stage_data = &g_stage_table[g_Globals.stage_num];
        GameThread::create(0); // playing
        break;
    case GAMEMODE_RESTART_REPLAY:
        GameThread::destroy();
        new_game_started = 1;
        unk_704 = 0;
        gamemode_to_switch_to = GAMEMODE_GAME;
        g_Globals.stage_num = g_Globals.weird_stage_num;
        g_stage_data = &g_stage_table[g_Globals.stage_num];
        GameThread::create(1); // a replay
        break;
    case GAMEMODE_RESTART_19:
        GameThread::destroy();
        new_game_started = 1;
        unk_704 = 1;
        gamemode_to_switch_to = GAMEMODE_GAME;
        g_Globals.stage_num = g_Globals.weird_stage_num;
        g_stage_data = &g_stage_table[g_Globals.stage_num];
        GameThread::create(0); // playing
        break;
    case GAMEMODE_RETRY_STAGE:
        GameThread::destroy();
        new_game_started = 1;
        gamemode_to_switch_to = GAMEMODE_GAME;
        GameThread::create(0); // playing
        break;
    case GAMEMODE_ENDING:
        if (gamemode_current == GAMEMODE_GAME)
        {
            GameThread::destroy();
        }
        Ending::create();
        break;
    case GAMEMODE_QUIT_ERROR:
        destroy_game_objects();
        g_CriticalSections.leave(CS_SUPERVISOR_GAMEMODE);
        return UPDATE_FUNC_EXIT_ERROR;
    }
done:
    gamemode_current = gamemode_to_switch_to;
    LEAVE_CS(CS_SUPERVISOR_GAMEMODE);
    return UPDATE_FUNC_CONTINUE;
}

// Clears the frame and resets the render state the sprite code caches.
// FUNCTION: TH16 0x43d140
int __fastcall Supervisor::on_draw_01(void *arg)
{
    Supervisor *s = (Supervisor *)arg;
    if (g_Supervisor.arcade_surface_0 != NULL)
    {
        g_Supervisor.d3d_device->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0xffffffff, 1.0f, 0);
        g_Supervisor.d3d_device->SetRenderTarget(0, g_Supervisor.arcade_surface_0);
        D3DVIEWPORT9 &vp = g_Supervisor.cameras[3].viewport;
        D3DRECT rect;
        rect.x1 = vp.X;
        rect.y1 = vp.Y;
        rect.x2 = vp.X + vp.Width;
        rect.y2 = vp.Y + vp.Height;
        g_Supervisor.d3d_device->Clear(1, &rect, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, g_Supervisor.background_color,
                                       1.0f, 0);
        g_arcade_width = 384;
        g_game_2d_origin_x = g_resolution_x / 2;
        g_arcade_height = 448;
        g_game_2d_origin_y = (i32)(g_resolution_y - 448.0f) / 2;
    }
    else
    {
        g_Supervisor.d3d_device->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, g_Supervisor.background_color,
                                       1.0f, 0);
    }
    AnmManager *anm = g_AnmManager;
    anm->last_texture_matrix_sprite = 0;
    anm->last_texture_id = -1;
    anm->last_blend_mode = ANM_BLEND_FORCE_RESET;
    anm->render_cache_184fbb5 = 0xff;
    anm->render_cache_184fbb7 = 0xff;
    anm->render_cache_184fbb8 = 0xff;
    anm->global_tint_enabled = 0;
    anm->global_tint.d3d = 0x80808080;
    anm->last_filter_point = 0xff;
    anm->last_color_op = ANM_COLOR_OP_NONE;
    anm->camera_2d_offset.y = 0.0f;
    anm->camera_2d_offset.x = 0.0f;
    anm->last_vertex_setup = ANM_VERTEX_SETUP_NONE;
    s->current_camera = &s->cameras[2];
    s->swap_transform_matrices(&s->cameras[2]);
    s->d3d_device->SetViewport(&s->current_camera->viewport);
    s->current_camera_index = 2;
    return 1;
}

// Draws the arcade region (arcade_blit_vm_0f and layer 0x22) onto the screen.
// FUNCTION: TH16 0x43d2f0
int __fastcall Supervisor::on_draw_0f(void *arg)
{
    if (g_Supervisor.arcade_surface_0 != NULL)
    {
        if (g_draw_hook_0f == NULL)
        {
            g_Supervisor.d3d_device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
            g_Supervisor.d3d_device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
            g_Supervisor.disable_zwrite();
            g_Supervisor.current_camera = &g_Supervisor.cameras[3];
            g_Supervisor.swap_transform_matrices(&g_Supervisor.cameras[3]);
            g_Supervisor.d3d_device->SetViewport(&g_Supervisor.current_camera->viewport);
            g_Supervisor.current_camera_index = 3;
            g_AnmManager->draw_vm(g_Supervisor.arcade_blit_vm_0f);
            g_Supervisor.arcade_blit_vm_0f->color_1.d3d = 0xffffffff;
            g_AnmManager->render_layer(0x22);
            g_AnmManager->flush_sprites();
            g_Supervisor.d3d_device->SetRenderState(D3DRS_ALPHATESTENABLE, TRUE);
            return 1;
        }
        g_draw_hook_0f();
    }
    return 1;
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
        if (g_draw_hook_1a == NULL)
        {
            g_Supervisor.d3d_device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
            g_Supervisor.d3d_device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
            g_Supervisor.disable_zwrite();
            g_Supervisor.current_camera = &g_Supervisor.cameras[3];
            g_Supervisor.swap_transform_matrices(&g_Supervisor.cameras[3]);
            g_Supervisor.d3d_device->SetViewport(&g_Supervisor.current_camera->viewport);
            g_Supervisor.current_camera_index = 3;
            g_AnmManager->draw_vm(g_Supervisor.arcade_blit_vm_1a);
            g_Supervisor.arcade_blit_vm_1a->color_1.d3d = 0xffffffff;
            g_AnmManager->flush_sprites();
            g_Supervisor.d3d_device->SetRenderState(D3DRS_ALPHATESTENABLE, TRUE);
        }
        else
        {
            g_draw_hook_1a();
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
        g_AnmManager->draw_vm(g_Supervisor.arcade_blit_vm_2c);
        g_AnmManager->flush_sprites();
        g_Supervisor.arcade_blit_vm_2c->color_1.d3d = 0xffffffff;
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
        g_AnmManager->draw_vm(g_Supervisor.arcade_blit_vm_39);
        g_Supervisor.arcade_blit_vm_39->color_1.d3d = 0xffffffff;
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
    g_Supervisor.cameras[3].shake_offset.x = 0.0f;
    g_Supervisor.cameras[3].shake_offset.y = 0.0f;
    g_Supervisor.cameras[1].shake_offset.x = 0.0f;
    g_Supervisor.cameras[1].shake_offset.y = 0.0f;
    return 1;
}

// TODO: the original schedules the int_vars store between the flags and/or (ours stores it after the flags; split &=/|= or a bitfield are worse), and moves eax before pop edi after the loop.
// FUNCTION: TH16 0x43d8b0
HARNESS_CALLED AnmId Supervisor::create_fog_vm(i32 count, i32 script)
{
    AnmId id = g_Supervisor.text_anm->create_effect(script, 0x22, NULL);
    AnmVm *vm = get_vm_or_clear(id);
    vm->alloc_extra_data(count * 2 * sizeof(RenderVertex144));
    if (count > 2)
    {
        RenderVertex144 *vertices = (RenderVertex144 *)vm->extra_data;
        vm->flags_lo = (vm->flags_lo & ~(0x1f << ANM_VM_RENDER_MODE_SHIFT)) | (12 << ANM_VM_RENDER_MODE_SHIFT);
        vm->int_vars[0] = count;
        for (i32 i = 0; i < count * 2; i++)
        {
            vertices[i].pos.z = 0.0f;
            vertices[i].pos.w = 1.0f;
            vertices[i].diffuse = 0xffffffff;
        }
    }
    else
    {
        vm->flags_lo &= ~(0x1f << ANM_VM_RENDER_MODE_SHIFT);
    }
    return id;
}

// Points the arcade surfaces at text.anm's render target textures and
// starts the VMs that draw them, picked by window width.
// TODO: effective match only since AnmVm::run became real code: the last
// flag `and` is scheduled after the pops in ours, before them in the original.
// FUNCTION: TH16 0x43d970
void Supervisor::setup_special_anms()
{
    if (arcade_surface_0 == NULL)
    {
        if (back_buffer == NULL)
        {
            d3d_device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &back_buffer);
        }
        text_anm->d3d[2].texture->GetSurfaceLevel(0, &arcade_surface_0);
        text_anm->d3d[3].texture->GetSurfaceLevel(0, &arcade_surface_1);
        AnmVm *vm = arcade_blit_vm_0f;
        if (!(vm->flags_lo & ANM_VM_VISIBLE))
        {
            if (g_resolution_x == 640)
            {
                text_anm->copy_vm(vm, TEXT_SCRIPT_BLIT_0F);
                vm->parent_vm = NULL;
                vm->root_vm = NULL;
                vm->run();
                vm = arcade_blit_vm_1a;
                text_anm->copy_vm(vm, TEXT_SCRIPT_BLIT_1A);
                vm->parent_vm = NULL;
                vm->root_vm = NULL;
                vm->run();
                vm = arcade_blit_vm_2c;
                text_anm->copy_vm(vm, TEXT_SCRIPT_BLIT_2C);
                vm->parent_vm = NULL;
                vm->root_vm = NULL;
                vm->run();
                vm = arcade_blit_vm_39;
                text_anm->copy_vm(vm, TEXT_SCRIPT_BLIT_39);
                vm->parent_vm = NULL;
                vm->root_vm = NULL;
                vm->run();
            }
            else if (g_resolution_x == 960)
            {
                text_anm->copy_vm(vm, TEXT_SCRIPT_BLIT_0F + 1);
                vm->parent_vm = NULL;
                vm->root_vm = NULL;
                vm->run();
                vm = arcade_blit_vm_1a;
                text_anm->copy_vm(vm, TEXT_SCRIPT_BLIT_1A + 1);
                vm->parent_vm = NULL;
                vm->root_vm = NULL;
                vm->run();
                vm = arcade_blit_vm_2c;
                text_anm->copy_vm(vm, TEXT_SCRIPT_BLIT_2C + 1);
                vm->parent_vm = NULL;
                vm->root_vm = NULL;
                vm->run();
                vm = arcade_blit_vm_39;
                text_anm->copy_vm(vm, TEXT_SCRIPT_BLIT_39 + 1);
                vm->parent_vm = NULL;
                vm->root_vm = NULL;
                vm->run();
            }
            else if (g_resolution_x == 1280)
            {
                text_anm->copy_vm(vm, TEXT_SCRIPT_BLIT_0F + 2);
                vm->parent_vm = NULL;
                vm->root_vm = NULL;
                vm->run();
                vm = arcade_blit_vm_1a;
                text_anm->copy_vm(vm, TEXT_SCRIPT_BLIT_1A + 2);
                vm->parent_vm = NULL;
                vm->root_vm = NULL;
                vm->run();
                vm = arcade_blit_vm_2c;
                text_anm->copy_vm(vm, TEXT_SCRIPT_BLIT_2C + 2);
                vm->parent_vm = NULL;
                vm->root_vm = NULL;
                vm->run();
                vm = arcade_blit_vm_39;
                text_anm->copy_vm(vm, TEXT_SCRIPT_BLIT_39 + 2);
                vm->parent_vm = NULL;
                vm->root_vm = NULL;
                vm->run();
            }
        }
        if (g_screen_coord_scale == 1.5f)
        {
            arcade_blit_vm_2c->flags_hi &= ~ANM_VM_FILTER_POINT;
        }
    }
    else
    {
        cameras[3] = cameras[0];
    }
}

// FUNCTION: TH16 0x43dc30
HARNESS_CALLED void Supervisor::release_surfaces()
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

// FUNCTION: TH16 0x43dcc0
void Supervisor::release_dinput()
{
    if (dinput != NULL)
    {
        dinput->Release();
        dinput = NULL;
    }
}

// GLOBAL: TH16 0x4a5788
f32 g_game_speed = 1.0f;

// FUNCTION: TH16 0x43b3d0
int __fastcall Supervisor::on_tick(void *arg)
{
    Supervisor *s = (Supervisor *)arg;

    if ((s->flags & (SUPERVISOR_QUIT_REQUESTED | SUPERVISOR_FLAG_100)) == SUPERVISOR_QUIT_REQUESTED &&
        !s->thread.should_run)
    {
        g_Supervisor.gamemode_to_switch_to = GAMEMODE_QUIT;
    }
    SoundManager::update_sound_thread();
    SoundManager::tick_bgm_fade();
    read_keyboard_input();
    if (g_AnmManager->service_pending_loads())
    {
        return UPDATE_FUNC_EXIT_SUCCESS;
    }
    if (g_device_reset_frames != 0)
    {
        g_device_reset_frames--;
    }
    if (s->unk_9b4 != 0)
    {
        return s->unk_9b4 == 2 ? UPDATE_FUNC_EXIT_SUCCESS : UPDATE_FUNC_CONTINUE;
    }
    int result = s->switch_gamemodes();
    if (result == UPDATE_FUNC_CONTINUE)
    {
        g_game_2d_origin_x = g_resolution_x / 2;
        g_game_2d_origin_y = (g_resolution_y - 448) / 2;
        return 1;
    }
    return result;
}

// FUNCTION: TH16 0x43b480
i32 Supervisor::open_data_files()
{
    char path[128];
    i32 size;

    if (g_Arcfile.open("th16.dat"))
    {
        sprintf(path, "th16_%.4x%c.ver", 0x100, 'a');
        g_Supervisor.ver_file_data = file_read_all(path, &size, 0);
        g_Supervisor.ver_file_size = size;
        if (g_Supervisor.ver_file_data == NULL)
        {
            g_GameErrorContext.fatal("error : \x83" "f\x81[\x83^\x82\xcc\x83o\x81[\x83W\x83\x87\x83\x93\x82\xaa\x88\xe1\x82\xa2\x82\xdc\x82\xb7\r\n");
            return -1;
        }
        return 0;
    }
    g_GameErrorContext.fatal("error : \x83" "f\x81[\x83^\x83t\x83@\x83" "C\x83\x8b\x82\xaa\x91\xb6\x8d\xdd\x82\xb5\x82\xdc\x82\xb9\x82\xf1\r\n");
    return -1;
}

// FUNCTION: TH16 0x43b520
int __fastcall Supervisor::on_registration(void *arg)
{
    open_data_files();
    g_game_speed = 1.0f;
    g_Supervisor.background_color = 0xff000000;
    g_Supervisor.setup_cameras();
    g_Supervisor.start_time = timeGetTime();
    g_replay_safe_rng.seed = g_Supervisor.start_time;
    g_replay_unsafe_rng.seed = g_Supervisor.start_time;
    {
        DWORD thread_id;
        g_SoundManager.load_thread = CreateThread(
            NULL, 0, (LPTHREAD_START_ROUTINE)SoundManager::thread_load_sound_files, &g_SoundManager, 0, &thread_id);
    }
    FpsCounter *fps = new FpsCounter;
    g_FpsCounter = fps;
    UpdateFunc *f = g_UpdateFuncRegistry->create_func((UpdateFuncCallback)FpsCounter::on_draw_callback);
    f->flags |= UPDATE_FUNC_ACTIVE;
    f->arg = fps;
    g_UpdateFuncRegistry->register_on_draw(f, 0x4b);
    fps->on_draw = f;
    AnmManager::setup_vertex_buffer();
    create_fonts();
    g_Supervisor.arcade_blit_vm_0f = new AnmVm;
    g_Supervisor.arcade_blit_vm_1a = new AnmVm;
    g_Supervisor.arcade_blit_vm_2c = new AnmVm;
    g_Supervisor.arcade_blit_vm_39 = new AnmVm;
    g_draw_hook_0f = NULL;
    g_draw_hook_1a = NULL;
    return 0;
}

// FUNCTION: TH16 0x43b660
int Supervisor::teardown_everything()
{
    while (SoundManager::update_sound_thread() != 0)
    {
    }
    g_SoundManager.thread_state = SOUND_THREAD_QUIT;
    g_Supervisor.thread.join_if_running();
    if (g_Supervisor.ver_file_data != NULL)
    {
        free(g_Supervisor.ver_file_data);
        g_Supervisor.ver_file_data = NULL;
    }
    destroy_game_objects();
    if (g_FpsCounter != NULL)
    {
        delete g_FpsCounter;
    }
    AnmManager *anm = g_AnmManager;
    if (anm->vertex_buffer != NULL)
    {
        anm->vertex_buffer->Release();
        anm->vertex_buffer = NULL;
    }
    g_SoundManager.modify_bgm(BGM_RELEASE, 0, "dummy");
    g_TextHelper.release_buffer();
    DeleteObject(g_text_font_default);
    DeleteObject(g_text_font_0);
    DeleteObject(g_text_font_2);
    DeleteObject(g_text_font_4);
    DeleteObject(g_text_font_6);
    DeleteObject(g_text_font_1);
    DeleteObject(g_text_font_3);
    DeleteObject(g_text_font_5);
    DeleteObject(g_text_font_7);
    if (keyboard != NULL)
    {
        keyboard->Unacquire();
        if (keyboard != NULL)
        {
            keyboard->Release();
            keyboard = NULL;
        }
    }
    if (joystick != NULL)
    {
        joystick->Unacquire();
        if (joystick != NULL)
        {
            joystick->Release();
            joystick = NULL;
        }
    }
    if (dinput != NULL)
    {
        dinput->Release();
        dinput = NULL;
    }
    g_Arcfile.close();
    delete g_Supervisor.arcade_blit_vm_0f;
    g_Supervisor.arcade_blit_vm_0f = NULL;
    delete g_Supervisor.arcade_blit_vm_1a;
    g_Supervisor.arcade_blit_vm_1a = NULL;
    delete g_Supervisor.arcade_blit_vm_2c;
    g_Supervisor.arcade_blit_vm_2c = NULL;
    delete g_Supervisor.arcade_blit_vm_39;
    g_Supervisor.arcade_blit_vm_39 = NULL;
    return 0;
}

// FUNCTION: TH16 0x43b950
void Supervisor::destroy_game_objects()
{
    GameThread::destroy();
    delete g_MainMenu;
    delete g_LoadingThread;
    delete g_Ending;
    delete g_ReplayManager;
    delete g_EffectManager;
    g_EffectManager = NULL;
    delete g_HelpManual;
}

extern HANDLE g_file;

void supervisor_debug_log(const char *fmt, ...);

// FUNCTION: TH16 0x43bbd0
HARNESS_CALLED int Supervisor::take_screenshot(const char *path)
{
    Screenshot *shot = &g_Supervisor.screenshot;
    while (shot->thread != 0)
    {
        Sleep(10);
    }
    IDirect3DSurface9 *surface = NULL;
    supervisor_debug_log("SnapShot! %s\n", path);
    g_Supervisor.d3d_device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &surface);
    memset(&shot->file_header, 0, sizeof(BITMAPFILEHEADER));
    shot->file_header.bfType = 0x4d42;
    shot->file_header.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    shot->file_header.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    strcpy(shot->path, path);
    switch (g_Supervisor.present_params.BackBufferFormat)
    {
    case D3DFMT_R5G6B5:
        // "16bit is not supported"
        g_GameErrorContext.log("16bit \x82\xcd\x8e\xe6\x82\xe8\x8d\x9e\x82\xdf\x82\xc8\x82\xa2\r\n");
        break;
    case D3DFMT_A8R8G8B8:
    case D3DFMT_X8R8G8B8:
    {
        shot->info = (BITMAPINFO *)malloc(sizeof(BITMAPINFO));
        if (shot->info == NULL)
        {
            // "snapShotScreen : could not allocate"
            g_GameErrorContext.log("snapShotScreen : \x8am\x95\xdb\x82\xb5\x82\xad\x82\xe8\r\n");
            break;
        }
        memset(shot->info, 0, sizeof(BITMAPINFO));
        i32 row = g_resolution_x * 3;
        i32 stride = row + 1 + (row % 4 != 0) * (4 - row % 4);
        shot->bmp_data = (u8 *)malloc(g_resolution_y * stride);
        if (shot->bmp_data == NULL)
        {
            g_GameErrorContext.log("snapShotScreen : \x8am\x95\xdb\x82\xb5\x82\xad\x82\xe8\r\n");
            break;
        }
        shot->file_header.bfSize += g_resolution_y * stride;
        shot->info->bmiHeader.biBitCount = 24;
        shot->info->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        shot->info->bmiHeader.biWidth = g_resolution_x;
        shot->info->bmiHeader.biHeight = g_resolution_y;
        shot->info->bmiHeader.biPlanes = 1;
        shot->info->bmiHeader.biCompression = BI_RGB;
        D3DLOCKED_RECT locked;
        surface->LockRect(&locked, NULL, 0);
        shot->pixels = (u8 *)malloc(g_resolution_y * locked.Pitch);
        memcpy(shot->pixels, locked.pBits, g_resolution_y * locked.Pitch);
        shot->pitch = locked.Pitch;
        surface->UnlockRect();
        shot->thread = _beginthread(write_screenshot, 0, NULL);
        break;
    }
    default:
        g_GameErrorContext.log("error : snapShotScreen\n");
        return 1;
    }
    if (surface != NULL)
    {
        surface->Release();
    }
    return 0;
}

// Writes to the file file_create opened; on a short write, closes it.
static inline void file_write_chunk(const void *data, DWORD size)
{
    if (g_file != INVALID_HANDLE_VALUE)
    {
        DWORD written;
        WriteFile(g_file, data, size, &written, NULL);
        if (size != written)
        {
            CloseHandle(g_file);
            LEAVE_CS(CS_FILE);
        }
    }
}

// FUNCTION: TH16 0x43be40
void __cdecl Supervisor::write_screenshot(void *arg)
{
    Screenshot *shot = &g_Supervisor.screenshot;
    u8 *dst = shot->bmp_data;
    u8 *pixels = shot->pixels;
    for (i32 y = g_resolution_y - 1; y > -1; y--)
    {
        u32 *src = (u32 *)(pixels + shot->pitch * y);
        for (i32 x = 0; x < g_resolution_x; x++)
        {
            *(u32 *)dst = *src++;
            dst += 3;
        }
    }
    file_create(shot->path);
    file_write_chunk(&shot->file_header, sizeof(BITMAPFILEHEADER));
    file_write_chunk(shot->info, sizeof(BITMAPINFOHEADER));
    i32 row = g_resolution_x * 3;
    file_write_chunk(shot->bmp_data, (row % 4 != 0) * (4 - row % 4) + g_resolution_x * g_resolution_y * 3);
    file_close();
    if (shot->info != NULL)
    {
        free(shot->info);
        shot->info = NULL;
    }
    if (shot->bmp_data != NULL)
    {
        free(shot->bmp_data);
        shot->bmp_data = NULL;
    }
    if (shot->pixels != NULL)
    {
        free(shot->pixels);
        shot->pixels = NULL;
    }
    shot->thread = 0;
}

// HARNESS_CALLED: LTCG then drops the unused `this`, so WinMain calls it
// without setting ecx, as the original does.
// FUNCTION: TH16 0x43ba40
HARNESS_CALLED int Supervisor::initialize()
{
    UpdateFunc *f;
    int result;

    g_Supervisor.gamemode_current = GAMEMODE_NONE;
    g_Supervisor.gamemode_to_switch_to = GAMEMODE_STARTUP;
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

// FUNCTION: TH16 0x43c050
HARNESS_CALLED int Supervisor::load_game_config(const char *path)
{
    ConfigData *data = (ConfigData *)&g_Supervisor.config.version;
    data->set_defaults_inline();
    _chdir(g_GameWindow.save_dir);
    i32 size;
    ConfigData *file = (ConfigData *)file_read_all(path, &size, 1);
    _chdir(g_GameWindow.exe_dir);
    if (file == NULL)
    {
        // "Config data not found, so it was initialized"
        g_GameErrorContext.log("\x83R\x83\x93\x83t\x83" "B\x83O\x83" "f\x81[\x83^\x82\xaa\x8c\xa9\x82\xc2\x82\xa9\x82\xe7\x82\xc8\x82\xa2\x82\xcc\x82\xc5\x8f\x89\x8a\xfa\x89\xbb\x82\xb5\x82\xdc\x82\xb5\x82\xbd\r\n");
        goto reset;
    }
    else
    {
        *data = *file;
        free(file);
        if (data->color_mode >= 2 || data->bgm_mode >= 3 || data->se_enabled >= 2 || data->window_size >= 6 ||
            data->frame_skip >= 3 || data->unk_21 >= 3 || data->version != CONFIG_VERSION ||
            size != sizeof(ConfigData))
        {
            // "Config data was broken, so it was reinitialized"
            g_GameErrorContext.log("\x83R\x83\x93\x83t\x83" "B\x83O\x83" "f\x81[\x83^\x82\xaa\x88\xd9\x8f\xed\x82\xc5\x82\xb5\x82\xbd\x82\xcc\x82\xc5\x8d\xc4\x8f\x89\x8a\xfa\x89\xbb\x82\xb5\x82\xdc\x82\xb5\x82\xbd\r\n");
        reset:
            data->set_defaults_inline();
        }
        else
        {
            memcpy(g_pad_mapping, data->pad_mapping, sizeof(g_pad_mapping));
        }
    }
    no_vsync = 0;
    if (config.flags & CONFIG_NO_FOG)
    {
        // "Fog is suppressed"
        g_GameErrorContext.log("\x83t\x83H\x83O\x82\xcc\x8eg\x97p\x82\xf0\x97}\x90\xa7\x82\xb5\x82\xdc\x82\xb7\r\n");
    }
    if (present_params.Windowed)
    {
        // "Starting in window mode"
        g_GameErrorContext.log("\x83" "E\x83" "B\x83\x93\x83h\x83" "E\x83\x82\x81[\x83h\x82\xc5\x8bN\x93\xae\x82\xb5\x82\xdc\x82\xb7\r\n");
    }
    if (config.flags & CONFIG_REFERENCE_RASTERIZER)
    {
        // "Forcing the reference rasterizer"
        g_GameErrorContext.log("\x83\x8a\x83t\x83@\x83\x8c\x83\x93\x83X\x83\x89\x83X\x83^\x83\x89\x83" "C\x83U\x82\xf0\x8b\xad\x90\xa7\x82\xb5\x82\xdc\x82\xb7\r\n");
    }
    if (config.flags & CONFIG_NO_DIRECTINPUT)
    {
        // "Not using DirectInput for pad and keyboard input"
        g_GameErrorContext.log("\x83p\x83" "b\x83h\x81" "A\x83L\x81[\x83{\x81[\x83h\x82\xcc\x93\xfc\x97\xcd\x82\xc9 DirectInput \x82\xf0\x8eg\x97p\x82\xb5\x82\xdc\x82\xb9\x82\xf1\r\n");
    }
    if (config.flags & CONFIG_BGM_IN_MEMORY)
    {
        // "Loading the BGM into memory"
        g_GameErrorContext.log("\x82" "a\x82" "f\x82l\x82\xf0\x83\x81\x83\x82\x83\x8a\x82\xc9\x93\xc7\x82\xdd\x8d\x9e\x82\xdd\x82\xdc\x82\xb7\r\n");
    }
    if (config.flags & CONFIG_NO_VSYNC)
    {
        // "Not waiting for vsync"
        g_GameErrorContext.log("\x90\x82\x92\xbc\x93\xaf\x8a\xfa\x82\xf0\x8e\xe6\x82\xe8\x82\xdc\x82\xb9\x82\xf1\r\n");
        g_Supervisor.no_vsync = 1;
    }
    if (config.flags & CONFIG_NO_TEXT_ENV_DETECTION)
    {
        // "Not detecting the text rendering environment"
        g_GameErrorContext.log("\x95\xb6\x8e\x9a\x95`\x89\xe6\x82\xcc\x8a\xc2\x8b\xab\x82\xf0\x8e\xa9\x93\xae\x8c\x9f\x8fo\x82\xb5\x82\xdc\x82\xb9\x82\xf1\r\n");
    }
    _chdir(g_GameWindow.save_dir);
    if (file_write(path, data, sizeof(ConfigData)) != 0)
    {
        // "Cannot write the file %s"
        g_GameErrorContext.fatal("\x83t\x83@\x83" "C\x83\x8b\x82\xaa\x8f\x91\x82\xab\x8fo\x82\xb9\x82\xdc\x82\xb9\x82\xf1 %s\r\n", path);
        // "Is the folder write-protected, or the disk full?"
        g_GameErrorContext.fatal("\x83t\x83H\x83\x8b\x83_\x82\xaa\x8f\x91\x8d\x9e\x82\xdd\x8b\xd6\x8e~\x91\xae\x90\xab\x82\xc9\x82\xc8\x82\xc1\x82\xc4\x82\xa2\x82\xe9\x82\xa9\x81" "A\x83" "f\x83" "B\x83X\x83N\x82\xaa\x82\xa2\x82\xc1\x82\xcf\x82\xa2\x82\xa2\x82\xc1\x82\xcf\x82\xa2\x82\xc9\x82\xc8\x82\xc1\x82\xc4\x82\xdc\x82\xb9\x82\xf1\x82\xa9\x81H\r\n");
        _chdir(g_GameWindow.exe_dir);
        return -1;
    }
    _chdir(g_GameWindow.exe_dir);
    return 0;
}

// FUNCTION: TH16 0x43c370
HARNESS_CALLED i32 Supervisor::play_bgm_wav(i32 slot, const char *name)
{
    char path[256];

    strcpy(path, name);
    append_wav_extension(path);
    g_SoundManager.modify_bgm(BGM_LOAD, slot, path);
    return 1;
}

// FUNCTION: TH16 0x43c3f0
HARNESS_CALLED i32 Supervisor::play_bgm(i32 slot, i32 track)
{
    if (g_Supervisor.config.flags & CONFIG_BGM_IN_MEMORY)
    {
        g_SoundManager.modify_bgm(BGM_RELEASE, 0, "dummy");
    }
    g_SoundManager.modify_bgm(BGM_PLAY, slot, "dummy");
    g_Scorefile->bgm_unlocked[track] = 1;
    return 0;
}

// FUNCTION: TH16 0x43c440
HARNESS_CALLED i32 Supervisor::stop_bgm()
{
    if (g_Supervisor.config.flags & CONFIG_BGM_IN_MEMORY)
    {
        g_SoundManager.modify_bgm(BGM_RELEASE, 0, "dummy");
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

// Debug output, empty in the release build.
// FUNCTION: TH16 0x43dce0
void supervisor_debug_log(const char *fmt, ...)
{
}
