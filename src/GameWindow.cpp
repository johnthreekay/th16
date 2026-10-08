#include <direct.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "AnmManager.h"
#include "CriticalSections.h"
#include "FileSystem.h"
#include "FpsCounter.h"
#include "GameWindow.h"
#include "Spellcard.h"
#include "Supervisor.h"
#include "UpdateFunc.h"

#include "Input.h"

static_assert(offsetof(GameWindow, is_app_active) == 0x10, "GameWindow layout");
static_assert(offsetof(GameWindow, performance_frequency) == 0x1c, "GameWindow layout");
static_assert(offsetof(GameWindow, save_dir) == 0x2d, "GameWindow layout");
static_assert(offsetof(GameWindow, screen_save_active) == 0x2030, "GameWindow layout");
static_assert(offsetof(GameWindow, flags) == 0x203c, "GameWindow layout");
static_assert(offsetof(GameWindow, resolution_x) == 0x204c, "GameWindow layout");
static_assert(offsetof(GameWindow, frame_start_time) == 0x2078, "GameWindow layout");
static_assert(offsetof(GameWindow, sleep_ms) == 0x20ac, "GameWindow layout");
static_assert(offsetof(GameWindow, pacing) == 0x20b4, "GameWindow layout");

// GLOBAL: TH16 0x4d7ce0
DECOMP_ALIGN16 GameWindow g_GameWindow;

// Debug logging, compiled out of the release build. The window setup logs
// the save directories through it.
// FUNCTION: TH16 0x4595b0
void window_debug_log(const char *fmt, ...)
{
}

// get_runtime's body. Its double math has to sit in an inline helper: that
// makes it a call graph node of its own, so LTCG's double stack alignment
// pass sees get_runtime as a function whose callee wants an aligned stack.
// get_runtime then realigns through ebx and its callers get padded frames
// (CSound::Unpause, PauseMenu::leave_paused) as in the original; with the
// math written in get_runtime itself it realigns plainly (and esp, -8).
static __forceinline double runtime_seconds()
{
    ENTER_CS(CS_SUPERVISOR_GAMEMODE);
    if (g_GameWindow.performance_frequency.QuadPart != 0)
    {
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        double t = (double)(now.QuadPart - g_GameWindow.initial_performance_counter.QuadPart) /
                   (double)g_GameWindow.performance_frequency.QuadPart;
        if (g_GameWindow.runtime_base > t)
        {
            g_GameWindow.runtime_base = t;
        }
        LEAVE_CS(CS_SUPERVISOR_GAMEMODE);
        return t - g_GameWindow.runtime_base;
    }
    double t = timeGetTime();
    if (g_GameWindow.runtime_base > t)
    {
        g_GameWindow.runtime_base = t;
    }
    double result = (t - g_GameWindow.runtime_base * 1000.0) / 1000.0;
    LEAVE_CS(CS_SUPERVISOR_GAMEMODE);
    return result;
}

// TODO: the timeGetTime path spills its result to [ebp-8] (the slot of the
// counter path's t); the original uses [ebp-0x10], the slot of now.
// Seconds since startup, from the performance counter if there is one.
// FUNCTION: TH16 0x45b130
double LTCG_VECTORCALL get_runtime()
{
    return runtime_seconds();
}

void window_debug_log(const char *fmt, ...);

// FUNCTION: TH16 0x45a700
void GameWindow::make_dirs_and_disable_screensaver()
{
    SystemParametersInfoA(SPI_GETSCREENSAVEACTIVE, 0, &g_GameWindow.screen_save_active, 0);
    SystemParametersInfoA(SPI_GETLOWPOWERACTIVE, 0, &g_GameWindow.low_power_active, 0);
    SystemParametersInfoA(SPI_GETPOWEROFFACTIVE, 0, &g_GameWindow.power_off_active, 0);
    SystemParametersInfoA(SPI_SETSCREENSAVEACTIVE, 0, NULL, SPIF_SENDCHANGE);
    SystemParametersInfoA(SPI_SETLOWPOWERACTIVE, 0, NULL, SPIF_SENDCHANGE);
    SystemParametersInfoA(SPI_SETPOWEROFFACTIVE, 0, NULL, SPIF_SENDCHANGE);
    QueryPerformanceFrequency(&g_GameWindow.performance_frequency);
    QueryPerformanceCounter(&g_GameWindow.initial_performance_counter);
    GetEnvironmentVariableA("APPDATA", save_dir, sizeof(save_dir));
    if (save_dir[0] != '\0')
    {
        strcat(save_dir, "\\ShanghaiAlice");
        _mkdir(g_GameWindow.save_dir);
        strcat(save_dir, "\\th16");
        _mkdir(g_GameWindow.save_dir);
        strcat(save_dir, "\\");
        window_debug_log("%d\n", save_dir);
    }
    if (GetModuleFileNameA(NULL, exe_dir, sizeof(exe_dir)))
    {
        char *slash = strrchr(exe_dir, '\\');
        if (slash != NULL)
        {
            *slash = '\0';
        }
        window_debug_log("%d\n", exe_dir);
    }
    _chdir(save_dir);
    _mkdir("replay");
    _chdir(exe_dir);
    HMODULE dwmapi = LoadLibraryA("dwmapi.dll");
    if (dwmapi != NULL)
    {
        HRESULT(WINAPI * enable_composition)(UINT) =
            (HRESULT(WINAPI *)(UINT))GetProcAddress(dwmapi, "DwmEnableComposition");
        if (enable_composition != NULL)
        {
            enable_composition(0);
        }
    }
}

// FUNCTION: TH16 0x45a8a0
HARNESS_CALLED i32 GameWindow::do_frame_sleeping()
{
    if (g_window_flags & WINDOW_SLEEP_PACING)
    {
        while (frame_start_time > next_frame_time)
        {
            next_frame_time += 1.0 / 60.0;
        }
    }
    g_AnmManager->flush_sprites();
    g_Supervisor.current_camera = &g_Supervisor.cameras[2];
    g_Supervisor.swap_transform_matrices(&g_Supervisor.cameras[2]);
    g_Supervisor.d3d_device->SetViewport(&g_Supervisor.current_camera->viewport);
    g_Supervisor.current_camera_index = 2;
    i32 result = g_UpdateFuncRegistry->run_all_on_tick();
    if (result == 0)
    {
        g_Supervisor.thread.join_if_running();
        return 1;
    }
    if (result == -1)
    {
        g_Supervisor.thread.join_if_running();
        return 2;
    }
    frame_skip_counter++;
    if (g_Supervisor.config.frame_skip < frame_skip_counter)
    {
        g_Supervisor.d3d_device->BeginScene();
        g_AnmManager->reset_vertex_buffers();
        g_Supervisor.fog_enabled = 0xff;
        g_Supervisor.disable_d3d_fog();
        g_UpdateFuncRegistry->run_all_on_draw();
        g_AnmManager->flush_sprites();
        g_Supervisor.d3d_device->SetTexture(0, NULL);
        g_Supervisor.d3d_device->EndScene();
        frame_skip_counter = 0;
        update_window_sleeping();
    }
    g_Supervisor.frame_time = get_runtime() - frame_start_time;
    return 0;
}

// TODO: ours realigns its frame (ebx form); matches when the DirectInput enum callbacks are left out (whole-program effect).
// FUNCTION: TH16 0x45a9f0
HARNESS_CALLED void GameWindow::update_window_sleeping()
{
    D3DRASTER_STATUS raster;
    double now = get_runtime();
    double deadline = present_time + 1.0 / 60.0;
    frame_start_time = now;
    sleep_ms = (deadline - now) * 1000.0 - 2.5;
    if (pacing[pacing_mode].sleep_ms >= sleep_ms)
    {
        pacing[pacing_mode].sleep_ms = sleep_ms >= 0 ? sleep_ms : 0;
        pacing[pacing_mode].late_frames = 0;
    }
    else
    {
        pacing[pacing_mode].late_frames++;
        if (pacing[pacing_mode].late_frames >= 15)
        {
            if (pacing[pacing_mode].sleep_ms < pacing[pacing_mode].max_sleep_ms)
            {
                pacing[pacing_mode].sleep_ms++;
            }
            pacing[pacing_mode].late_frames = 0;
        }
    }
    if (sleep_ms < 0)
    {
        sleep_ms = 0;
    }
    raster.ScanLine = 0;
    now = get_runtime();
    if (sleep_start_time > now)
    {
        sleep_start_time = now;
    }
    if (now - sleep_start_time < 0.019)
    {
        if (!g_Supervisor.present_params.Windowed)
        {
            while (get_runtime() - sleep_start_time < 0.013)
            {
                Sleep(1);
            }
            raster.InVBlank = FALSE;
            while (g_Supervisor.d3d_device->GetRasterStatus(0, &raster) == D3D_OK)
            {
                if (raster.InVBlank)
                {
                    break;
                }
            }
        }
    }
    else if (pacing[pacing_mode].sleep_ms > 0)
    {
        pacing[pacing_mode].sleep_ms--;
    }
    sleep_start_time = get_runtime();
    take_screenshot();
    if (g_Supervisor.d3d_device->Present(NULL, NULL, NULL, NULL) < 0)
    {
        g_Supervisor.release_surfaces();
        g_AnmManager->release_textures();
        g_Supervisor.d3d_device->Reset(&g_Supervisor.present_params);
        g_AnmManager->create_d3d_textures_for_loaded_anms();
        g_Supervisor.setup_special_anms();
        Supervisor::reset_render_state();
        g_Supervisor.unk_714 = 2;
    }
    if (g_FpsCounter != NULL)
    {
        g_FpsCounter->update();
    }
    if (g_Spellcard != NULL)
    {
        Spellcard::measure_real_time();
    }
    get_runtime();
    if (pacing[pacing_mode].sleep_ms > 0)
    {
        Sleep(pacing[pacing_mode].sleep_ms);
    }
    present_time = get_runtime();
}

// FUNCTION: TH16 0x45ac50
HARNESS_CALLED i32 GameWindow::do_frame_frameskip()
{
    double now = get_runtime();
    frame_start_time = now;
    if (last_frame_time > now)
    {
        next_frame_time = now;
    }
    double ahead = (next_frame_time - now) * 1000.0;
    last_frame_time = now;
    if (ahead >= 1.5)
    {
        Sleep(1);
    }
    if (!(frame_start_time > next_frame_time))
    {
        return 0;
    }
    while (frame_start_time > next_frame_time)
    {
        next_frame_time += 1.0 / 60.0;
    }
    g_AnmManager->flush_sprites();
    g_Supervisor.current_camera = &g_Supervisor.cameras[2];
    g_Supervisor.swap_transform_matrices(&g_Supervisor.cameras[2]);
    g_Supervisor.d3d_device->SetViewport(&g_Supervisor.current_camera->viewport);
    g_Supervisor.current_camera_index = 2;
    i32 result = g_UpdateFuncRegistry->run_all_on_tick();
    if (result == 0)
    {
        g_Supervisor.thread.join_if_running();
        return 1;
    }
    if (result == -1)
    {
        g_Supervisor.thread.join_if_running();
        return 2;
    }
    frame_skip_counter++;
    if (g_Supervisor.config.frame_skip < frame_skip_counter)
    {
        g_Supervisor.d3d_device->BeginScene();
        g_AnmManager->reset_vertex_buffers();
        g_Supervisor.fog_enabled = 0xff;
        g_Supervisor.disable_d3d_fog();
        g_UpdateFuncRegistry->run_all_on_draw();
        g_AnmManager->flush_sprites();
        g_Supervisor.d3d_device->SetTexture(0, NULL);
        g_Supervisor.d3d_device->EndScene();
        frame_skip_counter = 0;
        present();
    }
    g_Supervisor.frame_time = get_runtime() - frame_start_time;
    return 0;
}

// TODO: ours lacks the push ecx padding; matches when the DirectInput enum callbacks are left out (whole-program effect).
// FUNCTION: TH16 0x45adf0
HARNESS_CALLED void GameWindow::present()
{
    take_screenshot();
    if (g_Supervisor.d3d_device->Present(NULL, NULL, NULL, NULL) < 0)
    {
        g_Supervisor.release_surfaces();
        g_AnmManager->release_textures();
        g_Supervisor.d3d_device->Reset(&g_Supervisor.present_params);
        g_AnmManager->create_d3d_textures_for_loaded_anms();
        g_Supervisor.setup_special_anms();
        Supervisor::reset_render_state();
        g_Supervisor.unk_714 = 2;
    }
    if (g_FpsCounter != NULL)
    {
        g_FpsCounter->update();
    }
    if (g_Spellcard != NULL)
    {
        Spellcard::measure_real_time();
    }
}

// FUNCTION: TH16 0x45ae70
HARNESS_CALLED i32 GameWindow::do_frame()
{
    g_AnmManager->flush_sprites();
    g_Supervisor.current_camera = &g_Supervisor.cameras[2];
    g_Supervisor.swap_transform_matrices(&g_Supervisor.cameras[2]);
    g_Supervisor.d3d_device->SetViewport(&g_Supervisor.current_camera->viewport);
    g_Supervisor.current_camera_index = 2;
    i32 result = g_UpdateFuncRegistry->run_all_on_tick();
    if (result == 0)
    {
        g_Supervisor.thread.join_if_running();
        return 1;
    }
    if (result == -1)
    {
        g_Supervisor.thread.join_if_running();
        return 2;
    }
    frame_skip_counter++;
    if (g_Supervisor.config.frame_skip < frame_skip_counter)
    {
        g_Supervisor.d3d_device->BeginScene();
        g_AnmManager->reset_vertex_buffers();
        g_Supervisor.fog_enabled = 0xff;
        g_Supervisor.disable_d3d_fog();
        g_UpdateFuncRegistry->run_all_on_draw();
        g_AnmManager->flush_sprites();
        g_Supervisor.d3d_device->SetTexture(0, NULL);
        g_Supervisor.d3d_device->EndScene();
        frame_skip_counter = 0;
        update_window();
    }
    g_Supervisor.frame_time = get_runtime() - frame_start_time;
    return 0;
}

// FUNCTION: TH16 0x45af80
HARNESS_CALLED void GameWindow::update_window()
{
    double now = get_runtime();
    frame_start_time = now;
    if (g_Supervisor.config.frame_pacing == 1)
    {
        double elapsed = now - present_time;
        if (elapsed < 1.0 / 60.0 && elapsed > 0.0)
        {
            i32 ms = (present_time + 1.0 / 60.0 - now) * 1000.0 - 3.5;
            if (ms > 0)
            {
                Sleep(ms);
            }
        }
    }
    sleep_start_time = get_runtime();
    take_screenshot();
    if (g_Supervisor.d3d_device->Present(NULL, NULL, NULL, NULL) < 0)
    {
        g_Supervisor.release_surfaces();
        g_AnmManager->release_textures();
        g_Supervisor.d3d_device->Reset(&g_Supervisor.present_params);
        g_AnmManager->create_d3d_textures_for_loaded_anms();
        g_Supervisor.setup_special_anms();
        Supervisor::reset_render_state();
        g_Supervisor.unk_714 = 2;
    }
    present_time = get_runtime();
    if (g_FpsCounter != NULL)
    {
        g_FpsCounter->update();
    }
    if (g_Spellcard != NULL)
    {
        Spellcard::measure_real_time();
    }
}

// FUNCTION: TH16 0x45b080
HARNESS_CALLED void GameWindow::take_screenshot()
{
    char path[0x100];
    g_AnmManager->take_screenshots();
    if (!(g_hardware_input_pressed & INPUT_SCREENSHOT))
    {
        return;
    }
    _chdir(g_GameWindow.save_dir);
    _mkdir("snapshot");
    i32 i;
    for (i = 0; i < 10000; i++)
    {
        sprintf(path, "%ssnapshot\\th16_%.3d.bmp", save_dir, i);
        if (!file_exists(path))
        {
            break;
        }
    }
    if (i < 10000)
    {
        g_Supervisor.take_screenshot(path);
    }
    _chdir(g_GameWindow.exe_dir);
}

// FUNCTION: TH16 0x45b280
void GameWindow::set_resolution_from_config()
{
    f32 scale;
    u32 size = (g_window_flags >> WINDOW_SIZE_SHIFT) & 0xf;
    if (size == WINDOW_SIZE_FULLSCREEN_1280 || size == WINDOW_SIZE_WINDOWED_1280)
    {
        scale = 2.0f;
    }
    else if (size == WINDOW_SIZE_FULLSCREEN_960 || size == WINDOW_SIZE_WINDOWED_960)
    {
        scale = 1.5f;
    }
    else
    {
        scale = 1.0f;
    }
    g_screen_coord_scale = scale;
    g_resolution_x = scale * 640.0f;
    g_resolution_y = scale * 480.0f;
    early_arcade_offset_x = (i32)(g_resolution_x - 384.0f) / 2;
    early_arcade_offset_y = (i32)(g_resolution_y - 448.0f) / 2;
}

// Sets the window size option bits of g_window_flags (GameWindow::flags).
#define SET_WINDOW_SIZE(n) (g_window_flags = (g_window_flags & ~WINDOW_SIZE_MASK) | ((n) << WINDOW_SIZE_SHIFT))

// The window procedure (TH06: GameWindow_WindowProc). Alt+Enter and
// maximizing switch between the windowed and fullscreen sizes.
// TODO: the original keeps window in edx for the DefWindowProcA calls; ours pushes it from the stack.
// FUNCTION: TH16 0x45a450
LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message)
    {
    case WM_ERASEBKGND:
        return 1;
    case WM_ACTIVATEAPP:
        g_GameWindow.is_app_active = wparam;
        g_GameWindow.show_cursor = wparam == 0;
        break;
    case WM_SIZE:
        if (g_window_flags & WINDOW_RUNNING)
        {
            switch (wparam)
            {
            case SIZE_MAXIMIZED:
                g_window_flags |= WINDOW_CHANGE_MODE;
                switch ((g_window_flags >> WINDOW_SIZE_SHIFT) & 0xf)
                {
                case WINDOW_SIZE_WINDOWED_640:
                    SET_WINDOW_SIZE(WINDOW_SIZE_FULLSCREEN_640);
                    break;
                case WINDOW_SIZE_WINDOWED_960:
                    SET_WINDOW_SIZE(WINDOW_SIZE_FULLSCREEN_960);
                    break;
                case WINDOW_SIZE_WINDOWED_1280:
                    SET_WINDOW_SIZE(WINDOW_SIZE_FULLSCREEN_1280);
                    break;
                }
                break;
            }
        }
        break;
    case WM_CLOSE:
        g_Supervisor.flags = (g_Supervisor.flags & ~SUPERVISOR_FLAG_100) | SUPERVISOR_QUIT_REQUESTED;
        return 1;
    case WM_SETCURSOR:
        if (!g_Supervisor.present_params.Windowed)
        {
            if (g_GameWindow.show_cursor)
            {
                SetCursor(LoadCursorA(NULL, IDC_ARROW));
                while (ShowCursor(TRUE) < 0)
                {
                }
            }
            else
            {
                while (ShowCursor(FALSE) >= 0)
                {
                }
                SetCursor(NULL);
            }
        }
        else
        {
            SetCursor(LoadCursorA(NULL, IDC_ARROW));
            ShowCursor(TRUE);
        }
        return 1;
    case WM_LBUTTONDOWN:
        SetForegroundWindow(window);
        break;
    case WM_SYSCOMMAND:
        switch (wparam & 0xfff0)
        {
        case SC_MOUSEMENU:
        case SC_KEYMENU:
            return 1;
        }
        break;
    case WM_SYSKEYDOWN:
        if (wparam == VK_RETURN)
        {
            g_window_flags |= WINDOW_CHANGE_MODE;
            switch ((g_window_flags >> WINDOW_SIZE_SHIFT) & 0xf)
            {
            case WINDOW_SIZE_WINDOWED_640:
                SET_WINDOW_SIZE(WINDOW_SIZE_FULLSCREEN_640);
                break;
            case WINDOW_SIZE_WINDOWED_960:
                SET_WINDOW_SIZE(WINDOW_SIZE_FULLSCREEN_960);
                break;
            case WINDOW_SIZE_WINDOWED_1280:
                SET_WINDOW_SIZE(WINDOW_SIZE_FULLSCREEN_1280);
                break;
            case WINDOW_SIZE_FULLSCREEN_640:
                SET_WINDOW_SIZE(WINDOW_SIZE_WINDOWED_640);
                break;
            case WINDOW_SIZE_FULLSCREEN_960:
                SET_WINDOW_SIZE(WINDOW_SIZE_WINDOWED_960);
                break;
            case WINDOW_SIZE_FULLSCREEN_1280:
                SET_WINDOW_SIZE(WINDOW_SIZE_WINDOWED_1280);
                break;
            }
        }
        break;
    }
    return DefWindowProcA(window, message, wparam, lparam);
}
