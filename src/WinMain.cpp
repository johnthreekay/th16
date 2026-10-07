// WinMain and the window and Direct3D device setup it does before handing
// over to Supervisor::initialize.
#include <stddef.h>
#include <string.h>

#include <windows.h>
#include <winnls32.h>

#include "AnmManager.h"
#include "CriticalSections.h"
#include "FileSystem.h"
#include "GameErrorContext.h"
#include "GameWindow.h"
#include "Input.h"
#include "InputManager.h"
#include "SoundManager.h"
#include "Supervisor.h"
#include "TextHelper.h"
#include "UpdateFunc.h"

static_assert(offsetof(Supervisor, window_rect) == 0x10, "Supervisor layout");
static_assert(offsetof(Supervisor, unk_720) == 0x720, "Supervisor layout");
static_assert(offsetof(Supervisor, caps) == 0x73c, "Supervisor layout");

HARNESS_CALLED i32 check_startup_shortcut();
void read_resolution_dialog();
INT_PTR CALLBACK resolution_dialog_proc(HWND dialog, UINT message, WPARAM wparam, LPARAM lparam);
LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
extern HANDLE g_app_mutex;
extern D3DThreadInf g_D3DThreadInf;

// Something WinMain allocates first and frees last; nothing else is known
// to use it.
struct WinMainUnk
{
    i32 value;

    WinMainUnk()
    {
        value = 0;
    }
};

// GLOBAL: TH16 0x4a6d90
WinMainUnk *g_unk_4a6d90;

// The back buffer sizes tried, smallest first; the device gets the first
// one at least as large as the window.
struct ScreenSize
{
    i32 width;
    i32 height;
};

// GLOBAL: TH16 0x493b20
const ScreenSize g_screen_sizes[15] = {
    {640, 480},   {960, 720},   {1024, 768},  {1152, 864},  {1280, 960},
    {1280, 1024}, {1600, 1024}, {1600, 1200}, {1920, 1080}, {1920, 1200},
    {2048, 1152}, {2048, 1536}, {2560, 1440}, {2560, 1600}, {3840, 2160},
};

// Registers the window class and opens the game window: a popup covering
// the screen, or a captioned window for the windowed sizes. 1 on failure.
// FUNCTION: TH16 0x45b330
HARNESS_CALLED i32 create_game_window(HINSTANCE instance)
{
    WNDCLASSA window_class = {};
    window_class.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    window_class.hCursor = LoadCursorA(NULL, IDC_ARROW);
    window_class.hInstance = instance;
    window_class.lpfnWndProc = window_proc;
    g_GameWindow.is_app_active = 1;
    g_GameWindow.show_cursor = 0;
    window_class.lpszClassName = "BASE";
    RegisterClassA(&window_class);
    u32 flags = g_window_flags;
    flags ^= (g_Supervisor.config.window_size << WINDOW_SIZE_SHIFT ^ flags) & WINDOW_SIZE_MASK;
    g_Supervisor.present_params.Windowed = (flags & WINDOW_SIZE_MASK) >= WINDOW_SIZE_WINDOWED_640 << WINDOW_SIZE_SHIFT;
    if (g_Supervisor.config.frame_skip == 0 && g_Supervisor.config.frame_pacing == 2)
    {
        flags |= WINDOW_SLEEP_PACING;
    }
    else
    {
        flags &= ~WINDOW_SLEEP_PACING;
    }
    g_window_flags = flags;
    g_frame_pacing.pacing[0].max_sleep_ms = 15;
    g_frame_pacing.pacing[0].sleep_ms = 15;
    g_frame_pacing.pacing[0].late_frames = 0;
    g_frame_pacing.pacing[1].max_sleep_ms = 12;
    g_frame_pacing.pacing[1].sleep_ms = 12;
    g_frame_pacing.pacing[1].late_frames = 0;
    g_frame_pacing.pacing[2].max_sleep_ms = 12;
    g_frame_pacing.pacing[2].sleep_ms = 12;
    g_frame_pacing.pacing[2].late_frames = 0;
    g_frame_pacing.pacing[3].max_sleep_ms = 8;
    g_frame_pacing.pacing[3].sleep_ms = 8;
    g_frame_pacing.pacing[3].late_frames = 0;
    g_frame_pacing.mode = 0;
    g_GameWindow.set_resolution_from_config();
    if (!g_Supervisor.present_params.Windowed)
    {
        g_GameWindow.window =
            CreateWindowExA(0, "BASE", "\x93\x8c\x95\xfb\x93V\x8b\xf3\xe0\xf6\x81@\x81` Hidden Star in Four Seasons. ver 1.00a",
                            WS_POPUP | WS_VISIBLE, 0, 0, g_resolution_x, g_resolution_y, NULL, NULL, instance, NULL);
    }
    else
    {
        i32 width = g_resolution_x + GetSystemMetrics(SM_CXDLGFRAME) * 2;
        i32 height = GetSystemMetrics(SM_CYDLGFRAME) * 2 + GetSystemMetrics(SM_CYCAPTION) + g_resolution_y;
        g_GameWindow.window =
            CreateWindowExA(0, "BASE", "\x93\x8c\x95\xfb\x93V\x8b\xf3\xe0\xf6\x81@\x81` Hidden Star in Four Seasons. ver 1.00a",
                            0x100b0000, g_Supervisor.config.window_x, g_Supervisor.config.window_y,
                            width, height, NULL, NULL, instance, NULL);
    }
    GetWindowRect(g_GameWindow.window, &g_Supervisor.window_rect);
    g_Supervisor.main_window = g_GameWindow.window;
    if (g_GameWindow.window == NULL)
    {
        return 1;
    }
    SendMessageA(g_GameWindow.window, WM_SYSCOMMAND, SC_MINIMIZE, 0);
    Sleep(16);
    SendMessageA(g_GameWindow.window, WM_SYSCOMMAND, SC_RESTORE, 0);
    return 0;
}

// Creates the device (or resets it) with the smallest back buffer that
// holds the window. In full screen a mode that is not 60 Hz is only taken
// on the second pass. 0 on success.
// TODO: 91%; register allocation of the retry loop differs.
// FUNCTION: TH16 0x45b530
HARNESS_CALLED i32 create_d3d_device(i32 reset)
{
    D3DPRESENT_PARAMETERS present_params = g_Supervisor.present_params;
    i32 second_pass = 0;
    u32 i;
retry:
    for (i = 0; i < 15; i++)
    {
        if (g_screen_sizes[i].width < g_resolution_x || g_screen_sizes[i].height < g_resolution_y)
        {
            continue;
        }
        present_params.BackBufferWidth = g_screen_sizes[i].width;
        present_params.BackBufferHeight = g_screen_sizes[i].height;
        if (!reset)
        {
            g_Supervisor.flags &= ~SUPERVISOR_HW_VERTEX_PROCESSING;
            if (!(g_Supervisor.config.flags & CONFIG_REFERENCE_RASTERIZER))
            {
                if (g_Supervisor.d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, g_GameWindow.window,
                                                   D3DCREATE_HARDWARE_VERTEXPROCESSING, &present_params,
                                                   &g_Supervisor.d3d_device) == D3D_OK)
                {
                    g_Supervisor.flags |= SUPERVISOR_HW_VERTEX_PROCESSING;
                    goto created;
                }
                if (g_Supervisor.d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, g_GameWindow.window,
                                                   D3DCREATE_SOFTWARE_VERTEXPROCESSING, &present_params,
                                                   &g_Supervisor.d3d_device) == D3D_OK)
                {
                    goto created;
                }
            }
            if (g_Supervisor.d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_REF, g_GameWindow.window,
                                               D3DCREATE_SOFTWARE_VERTEXPROCESSING, &present_params,
                                               &g_Supervisor.d3d_device) != D3D_OK)
            {
                if (g_Supervisor.d3d_device != NULL)
                {
                    g_Supervisor.d3d_device->Release();
                    g_Supervisor.d3d_device = NULL;
                }
                continue;
            }
        }
        else if (g_Supervisor.d3d_device->Reset(&present_params) != D3D_OK)
        {
            continue;
        }
    created:
        HDC dc = GetDC(g_GameWindow.window);
        i32 refresh_rate = GetDeviceCaps(dc, VREFRESH);
        ReleaseDC(g_GameWindow.window, dc);
        if (refresh_rate != 60)
        {
            if (!g_Supervisor.present_params.Windowed && !second_pass)
            {
                if (!reset && g_Supervisor.d3d_device != NULL)
                {
                    g_Supervisor.d3d_device->Release();
                    g_Supervisor.d3d_device = NULL;
                }
                continue;
            }
            g_window_flags &= ~WINDOW_SLEEP_PACING;
        }
        g_Supervisor.present_params = present_params;
        break;
    }
    if (i < 15)
    {
        if (second_pass == 1)
        {
            // リフレッシュレートが60Hzではありません
            g_GameErrorContext.log("\x83\x8a\x83t\x83\x8c\x83"
                                   "b\x83V\x83\x85\x83\x8c\x81[\x83g\x82\xaa"
                                   "60Hz\x82\xc5\x82\xcd\x82\xa0\x82\xe8\x82\xdc\x82\xb9\x82\xf1\r\n");
        }
        // 解像度 %d %dで起動します
        g_GameErrorContext.log("\x89\xf0\x91\x9c\x93x %d %d\x82\xc5\x8bN\x93\xae\x82\xb5\x82\xdc\x82\xb7\r\n",
                               present_params.BackBufferWidth, present_params.BackBufferHeight);
        return 0;
    }
    if (!second_pass)
    {
        second_pass = 1;
        goto retry;
    }
    if (!reset)
    {
        // Direct3D の初期化に失敗、この解像度には対応してません
        g_GameErrorContext.fatal("Direct3D \x82\xcc\x8f\x89\x8a\xfa\x89\xbb\x82\xc9\x8e\xb8\x94s\x81"
                                 "A\x82\xb1\x82\xcc\x89\xf0\x91\x9c\x93x\x82\xc9\x82\xcd\x91\xce\x89\x9e\x82\xb5\x82"
                                 "\xc4\x82\xdc\x82\xb9\x82\xf1\r\n");
        if (g_Supervisor.d3d != NULL)
        {
            g_Supervisor.d3d->Release();
            g_Supervisor.d3d = NULL;
        }
    }
    return -1;
}

// Picks the presentation parameters from the display mode and the options,
// creates the device and checks what it supports. 1 on failure (ExpHP:
// sub_45b7d0_lots_of_d3d_init).
// TODO: 68%; the presentation parameters are filled in another order.
// FUNCTION: TH16 0x45b7d0
HARNESS_CALLED i32 init_d3d()
{
    D3DPRESENT_PARAMETERS present_params;
    memset(&present_params, 0, sizeof(present_params));
    D3DDISPLAYMODE display_mode;
    g_Supervisor.d3d->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &display_mode);
    if (display_mode.Format == D3DFMT_X8R8G8B8)
    {
        display_mode.Format = D3DFMT_A8R8G8B8;
    }
    g_Supervisor.display_mode = display_mode;
    if (g_Supervisor.present_params.Windowed && display_mode.RefreshRate != 60)
    {
        // リフレッシュレートが60Hzではありません
        g_GameErrorContext.log("\x83\x8a\x83t\x83\x8c\x83"
                               "b\x83V\x83\x85\x83\x8c\x81[\x83g\x82\xaa"
                               "60Hz\x82\xc5\x82\xcd\x82\xa0\x82\xe8\x82\xdc\x82\xb9\x82\xf1\r\n");
        g_window_flags &= ~WINDOW_SLEEP_PACING;
    }
    if (g_GameWindow.started_by_launcher)
    {
        g_Supervisor.no_vsync = 1;
    }
    D3DFORMAT format;
    if (!g_Supervisor.present_params.Windowed)
    {
        if (g_Supervisor.config.color_mode == 0xff)
        {
            present_params.BackBufferFormat = format = D3DFMT_A8R8G8B8;
            g_Supervisor.config.color_mode = 0;
            // 初回起動、画面を 32Bits で初期化しました
            g_GameErrorContext.log("\x8f\x89\x89\xf1\x8bN\x93\xae\x81"
                                   "A\x89\xe6\x96\xca\x82\xf0 32Bits \x82\xc5\x8f\x89\x8a\xfa\x89\xbb\x82\xb5\x82\xdc"
                                   "\x82\xb5\x82\xbd\r\n");
        }
        else
        {
            present_params.BackBufferFormat = format =
                (D3DFORMAT)((g_Supervisor.config.color_mode != 0) * 2 + D3DFMT_A8R8G8B8);
        }
        if (g_Supervisor.no_vsync == 0)
        {
            present_params.FullScreen_RefreshRateInHz = 60;
            if (g_window_flags & WINDOW_SLEEP_PACING)
            {
                present_params.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
                present_params.SwapEffect = D3DSWAPEFFECT_DISCARD;
            }
            else
            {
                present_params.SwapEffect = D3DSWAPEFFECT_DISCARD;
                present_params.PresentationInterval =
                    g_Supervisor.config.frame_pacing == 3 ? D3DPRESENT_INTERVAL_IMMEDIATE : D3DPRESENT_INTERVAL_ONE;
            }
        }
        else
        {
            present_params.FullScreen_RefreshRateInHz = 0;
            present_params.SwapEffect = D3DSWAPEFFECT_DISCARD;
            present_params.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
        }
    }
    else
    {
        present_params.BackBufferFormat = format = display_mode.Format;
        present_params.SwapEffect = D3DSWAPEFFECT_DISCARD;
        if (g_window_flags & WINDOW_SLEEP_PACING)
        {
            present_params.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
        }
        else if (g_Supervisor.config.frame_pacing == 3 || display_mode.RefreshRate != 60)
        {
            present_params.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
        }
        else
        {
            present_params.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
        }
        present_params.Windowed = TRUE;
    }
    g_Supervisor.flags |= SUPERVISOR_D3D_INITIALIZED;
    present_params.EnableAutoDepthStencil = TRUE;
    present_params.AutoDepthStencilFormat = D3DFMT_D16;
    present_params.Flags = D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;
    g_Supervisor.unk_720 = 1;
    g_Supervisor.present_params = present_params;
    if (create_d3d_device(0) != 0)
    {
        return 1;
    }
    g_Supervisor.d3d_device->GetDeviceCaps(&g_Supervisor.caps);
    if (!(g_Supervisor.caps.TextureOpCaps & D3DTEXOPCAPS_ADD))
    {
        // D3DTEXOPCAPS_ADD をサポートしていません、色加算エミュレートモードで動作します
        g_GameErrorContext.log("D3DTEXOPCAPS_ADD \x82\xf0\x83T\x83|\x81[\x83g\x82\xb5\x82\xc4\x82\xa2\x82\xdc\x82\xb9"
                               "\x82\xf1\x81"
                               "A\x90"
                               "F\x89\xc1\x8eZ\x83G\x83~\x83\x85\x83\x8c\x81[\x83g\x83\x82\x81[\x83h\x82\xc5\x93\xae"
                               "\x8d\xec\x82\xb5\x82\xdc\x82\xb7\r\n");
    }
    if (g_Supervisor.caps.MaxTextureWidth <= 256)
    {
        // 512 以上のテクスチャをサポートしていません。殆どの絵がボケて表示されます。
        g_GameErrorContext.log("512 \x88\xc8\x8f\xe3\x82\xcc\x83"
                               "e\x83N\x83X\x83`\x83\x83\x82\xf0\x83T\x83|\x81[\x83g\x82\xb5\x82\xc4\x82\xa2\x82\xdc"
                               "\x82\xb9\x82\xf1\x81"
                               "B\x96w\x82\xc7\x82\xcc\x8aG\x82\xaa\x83{\x83P\x82\xc4\x95\\\x8e\xa6\x82\xb3\x82\xea"
                               "\x82\xdc\x82\xb7\x81"
                               "B\r\n");
    }
    if (g_Supervisor.d3d->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, format, 0, D3DRTYPE_TEXTURE,
                                            D3DFMT_A8R8G8B8) == D3D_OK)
    {
        g_Supervisor.flags |= SUPERVISOR_ARGB_TEXTURES;
    }
    else
    {
        g_Supervisor.flags &= ~SUPERVISOR_ARGB_TEXTURES;
        g_Supervisor.config.flags |= CONFIG_REDUCED_COLOR;
        // D3DFMT_A8R8G8B8 をサポートしていません、減色モードで動作します
        g_GameErrorContext.log("D3DFMT_A8R8G8B8 \x82\xf0\x83T\x83|\x81[\x83g\x82\xb5\x82\xc4\x82\xa2\x82\xdc\x82\xb9"
                               "\x82\xf1\x81"
                               "A\x8c\xb8\x90"
                               "F\x83\x82\x81[\x83h\x82\xc5\x93\xae\x8d\xec\x82\xb5\x82\xdc\x82\xb7\r\n");
    }
    g_GameWindow.exit_requested = 0;
    g_Supervisor.reset_render_state();
    g_Supervisor.unk_724 = 0;
    return 0;
}

// Sends BM_SETCHECK to one of the resolution dialog's radio buttons.
static inline void check_dialog_button(i32 id, BOOL checked)
{
    SendMessageA(GetDlgItem(g_GameWindow.dialog, id), BM_SETCHECK, checked, 0);
}

static inline void release_com(IUnknown **object)
{
    if (*object != NULL)
    {
        (*object)->Release();
        *object = NULL;
    }
}

// The two sound helpers below were separate (inlined) functions in the
// original too. LTCG builds its call graph before inlining, and WinMain
// itself would change their callees: it realigns its frame, so taking
// thread_init's address there gives thread_init known alignment (a padded
// frame, and SoundManager::initialize realigns), and its stores to
// g_SoundManager's fields stop LTCG from folding release's this.

// Clears g_SoundManager and starts the thread that initializes DirectSound
// and loads the sound effects.
static inline void start_sound(HWND window)
{
    memset(&g_SoundManager, 0, sizeof(SoundManager));
    g_SoundManager.window = window;
    g_SoundManager.init_thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)SoundManager::thread_init,
                                              &g_SoundManager, 0, &g_SoundManager.init_thread_id);
}

// Tells both sound threads to quit and waits for them.
static inline void stop_sound_threads()
{
    g_SoundManager.thread_state = SOUND_THREAD_QUIT;
    SoundManager::stop_threads();
}

// WinMain has C linkage, so it is annotated by its linker symbol.
// TODO: 80%; the critical section loops count differently and some blocks are laid out in another order.
// SYNTHETIC: TH16 0x459830 SYMBOL
// _WinMain@16
int WINAPI WinMain(HINSTANCE instance, HINSTANCE prev_instance, LPSTR command_line, int show)
{
    MSG msg;
    BYTE keys[256];
    char path[0x1000];
    HINSTANCE instance_copy = instance;
    i32 result = 0;
    g_GameWindow.instance = instance;
    timeBeginPeriod(1);
    for (i32 i = 0; i < CS_COUNT; i++)
    {
        InitializeCriticalSection(&g_CriticalSections.cs[i]);
    }
    g_CriticalSections.enabled = true;
    g_unk_4a6d90 = new WinMainUnk;
    g_unk_4a6d90->value = 0;
    // 東方動作記録 ---------------------------------------------
    g_GameErrorContext.log("\x93\x8c\x95\xfb\x93\xae\x8d\xec\x8bL\x98^ --------------------------------------------- \r\n");
    g_app_mutex = CreateMutexA(NULL, TRUE, "Touhou 12 App");
    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        // 二つは起動できません
        g_GameErrorContext.fatal("\x93\xf1\x82\xc2\x82\xcd\x8bN\x93\xae\x82\xc5\x82\xab\x82\xdc\x82\xb9\x82\xf1\r\n");
        goto shutdown;
    }
    if (check_startup_shortcut() == -1)
    {
        goto shutdown;
    }
    g_Supervisor.instance = instance_copy;
    g_GameWindow.make_dirs_and_disable_screensaver();
    if (g_Supervisor.load_game_config("th16.cfg") != 0)
    {
        goto shutdown;
    }
    get_joypad_capabilities();
    clear_all_keydown_states();
    if ((g_Supervisor.config.flags & CONFIG_SHOW_STARTUP_DIALOG) || (GetKeyboardState(keys), keys[VK_SHIFT] & 0x80))
    {
        g_GameWindow.dialog =
            CreateDialogParamA(instance_copy, MAKEINTRESOURCEA(0xcb), NULL, resolution_dialog_proc, 0);
        ShowWindow(g_GameWindow.dialog, SW_SHOW);
        for (;;)
        {
            InputManager *hw = (InputManager *)&g_hardware_input;
            u32 input = g_Supervisor.read_joypad(0);
            hw->prev = hw->cur;
            hw->cur = input;
            hw->detect_holds_and_repeats();
            if (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE))
            {
                if (IsDialogMessageA(g_GameWindow.dialog, &msg))
                {
                    continue;
                }
                TranslateMessage(&msg);
                DispatchMessageA(&msg);
            }
            if (g_hardware_input_pressed & 0x80001)
            {
                read_resolution_dialog();
                g_window_flags &= ~(WINDOW_DIALOG_CANCELLED | WINDOW_DIALOG_OPEN);
                DestroyWindow(g_GameWindow.dialog);
                g_GameWindow.dialog = NULL;
                break;
            }
            if (g_hardware_input_pressed & 0x20)
            {
                if (IsDlgButtonChecked(g_GameWindow.dialog, 0xcd) == BST_CHECKED)
                {
                    check_dialog_button(0xcd, FALSE);
                    check_dialog_button(0xce, TRUE);
                }
                else if (IsDlgButtonChecked(g_GameWindow.dialog, 0xce) == BST_CHECKED)
                {
                    check_dialog_button(0xce, FALSE);
                    check_dialog_button(0xcf, TRUE);
                }
                else if (IsDlgButtonChecked(g_GameWindow.dialog, 0xcf) == BST_CHECKED)
                {
                    check_dialog_button(0xcf, FALSE);
                    check_dialog_button(0xcd, TRUE);
                }
            }
            if (g_hardware_input_pressed & 0x10)
            {
                if (IsDlgButtonChecked(g_GameWindow.dialog, 0xcd) == BST_CHECKED)
                {
                    check_dialog_button(0xcd, FALSE);
                    check_dialog_button(0xcf, TRUE);
                }
                else if (IsDlgButtonChecked(g_GameWindow.dialog, 0xce) == BST_CHECKED)
                {
                    check_dialog_button(0xce, FALSE);
                    check_dialog_button(0xcd, TRUE);
                }
                else if (IsDlgButtonChecked(g_GameWindow.dialog, 0xcf) == BST_CHECKED)
                {
                    check_dialog_button(0xcf, FALSE);
                    check_dialog_button(0xce, TRUE);
                }
            }
            if (g_GameWindow.dialog == NULL)
            {
                break;
            }
            Sleep(6);
        }
    }
    if (g_window_flags & (WINDOW_DIALOG_CANCELLED | WINDOW_DIALOG_OPEN))
    {
        goto shutdown;
    }
    g_window_flags ^= (g_Supervisor.config.window_size << WINDOW_SIZE_SHIFT ^ g_window_flags) & WINDOW_SIZE_MASK;
    g_Supervisor.compute_exe_checksum();
create_d3d:
    g_Supervisor.d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (g_Supervisor.d3d == NULL)
    {
        // Direct3D オブジェクトは何故か作成出来なかった
        g_GameErrorContext.fatal("Direct3D \x83I\x83u\x83W\x83"
                                 "F\x83N\x83g\x82\xcd\x89\xbd\x8c\xcc\x82\xa9\x8d\xec\x90\xac\x8fo\x97\x88\x82\xc8\x82"
                                 "\xa9\x82\xc1\x82\xbd\r\n");
        goto shutdown;
    }
    if (create_game_window(instance_copy) != 0)
    {
        goto shutdown;
    }
    g_Supervisor.init_input();
    g_D3DThreadInf.join_if_running();
    start_sound(g_GameWindow.window);
    if (init_d3d() != 0)
    {
        goto shutdown;
    }
    g_UpdateFuncRegistry = new UpdateFuncRegistry;
    g_AnmManager = new AnmManager;
    if (!g_Supervisor.present_params.Windowed)
    {
        WINNLSEnableIME(NULL, FALSE);
        while (ShowCursor(FALSE) >= 0)
        {
        }
        SetCursor(NULL);
    }
    g_GameWindow.runtime_base = 0.0;
    g_GameWindow.frame_start_time = g_GameWindow.last_frame_time = g_GameWindow.next_frame_time = get_runtime();
    g_GameWindow.present_time = g_GameWindow.sleep_start_time = get_runtime();
    SetForegroundWindow(g_GameWindow.window);
    result = g_Supervisor.initialize();
    if (result != 0)
    {
        if (result != -1)
        {
            result = 2;
        }
        goto teardown;
    }
    g_window_flags |= WINDOW_RUNNING;
    result = 0;
    g_GameWindow.frame_skip_counter = -4;
    while (g_GameWindow.exit_requested == 0)
    {
        if (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
            continue;
        }
        HRESULT hr = g_Supervisor.d3d_device->TestCooperativeLevel();
        if (hr == D3D_OK)
        {
            if (!(g_window_flags & WINDOW_CHANGE_MODE))
            {
                if (g_window_flags & WINDOW_SLEEP_PACING)
                {
                    result = g_GameWindow.do_frame_sleeping();
                }
                else if (g_Supervisor.present_params.PresentationInterval == D3DPRESENT_INTERVAL_ONE &&
                         g_Supervisor.config.frame_skip == 0)
                {
                    result = g_GameWindow.do_frame();
                }
                else
                {
                    result = g_GameWindow.do_frame_frameskip();
                }
                if (result != 0)
                {
                    goto teardown;
                }
                g_Supervisor.flags &= ~SUPERVISOR_DEVICE_WAS_RESET;
                continue;
            }
        }
        else if (hr != D3DERR_DEVICENOTRESET)
        {
            continue;
        }
        g_GameWindow.device_reset_frames = 10;
        if (g_window_flags & WINDOW_CHANGE_MODE)
        {
            D3DFORMAT format;
            if ((g_window_flags & WINDOW_SIZE_MASK) <= WINDOW_SIZE_FULLSCREEN_1280 << WINDOW_SIZE_SHIFT)
            {
                GetWindowRect(g_GameWindow.window, &g_Supervisor.window_rect);
                g_Supervisor.present_params.FullScreen_RefreshRateInHz = 60;
                g_Supervisor.present_params.Windowed = FALSE;
                format = (D3DFORMAT)((g_Supervisor.config.color_mode != 0) * 2 + D3DFMT_A8R8G8B8);
            }
            else
            {
                format = g_Supervisor.display_mode.Format;
                g_Supervisor.present_params.FullScreen_RefreshRateInHz = 0;
                g_Supervisor.present_params.Windowed = TRUE;
            }
            g_Supervisor.present_params.BackBufferFormat = format;
        }
        g_Supervisor.release_surfaces();
        g_AnmManager->release_textures();
        if (create_d3d_device(1) != 0)
        {
            goto teardown;
        }
        g_Supervisor.reset_render_state();
        g_AnmManager->create_d3d_textures_for_loaded_anms();
        g_Supervisor.flags |= SUPERVISOR_DEVICE_WAS_RESET;
        g_Supervisor.unk_714 = 3;
        if (g_window_flags & WINDOW_CHANGE_MODE)
        {
            g_GameWindow.set_resolution_from_config();
            if ((g_window_flags & WINDOW_SIZE_MASK) >= WINDOW_SIZE_WINDOWED_640 << WINDOW_SIZE_SHIFT)
            {
                i32 width = g_resolution_x + GetSystemMetrics(SM_CXDLGFRAME) * 2;
                i32 height = GetSystemMetrics(SM_CYDLGFRAME) * 2 + GetSystemMetrics(SM_CYCAPTION) + g_resolution_y;
                SetWindowLongA(g_GameWindow.window, GWL_STYLE, 0x10cb0000);
                SetWindowPos(g_GameWindow.window, NULL, g_Supervisor.window_rect.left, g_Supervisor.window_rect.top,
                             width, height, SWP_SHOWWINDOW | SWP_FRAMECHANGED);
                ShowWindow(g_GameWindow.window, SW_SHOWNORMAL);
                WINNLSEnableIME(NULL, TRUE);
                while (ShowCursor(TRUE) < 0)
                {
                }
            }
            else
            {
                SetWindowLongA(g_GameWindow.window, GWL_STYLE, WS_POPUP | WS_VISIBLE);
                SetWindowPos(g_GameWindow.window, NULL, 0, 0, g_resolution_x, g_resolution_y, SWP_FRAMECHANGED);
                WINNLSEnableIME(NULL, FALSE);
                while (ShowCursor(FALSE) >= 0)
                {
                }
                SetCursor(NULL);
                g_GameWindow.show_cursor = 0;
            }
        }
        g_Supervisor.setup_special_anms();
        g_window_flags &= ~WINDOW_CHANGE_MODE;
    }
teardown:
    g_Supervisor.config.window_size = (g_window_flags >> WINDOW_SIZE_SHIFT) & 0xf;
    if (g_Supervisor.config.window_size >= 3)
    {
        GetWindowRect(g_GameWindow.window, &g_Supervisor.window_rect);
        g_Supervisor.config.window_x = g_Supervisor.window_rect.left;
        g_Supervisor.config.window_y = g_Supervisor.window_rect.top;
    }
    g_Supervisor.teardown_everything();
    delete g_UpdateFuncRegistry;
    g_UpdateFuncRegistry = NULL;
shutdown:
    stop_sound_threads();
    g_SoundManager.release();
    {
        AnmManager *anm = g_AnmManager;
        if (anm != NULL)
        {
            anm->~AnmManager();
            operator delete(anm, sizeof(AnmManager));
        }
    }
    g_AnmManager = NULL;
    release_com((IUnknown **)&g_Supervisor.arcade_surface_0);
    release_com((IUnknown **)&g_Supervisor.arcade_surface_1);
    release_com((IUnknown **)&g_Supervisor.back_buffer);
    release_com((IUnknown **)&g_Supervisor.d3d_device);
    release_com((IUnknown **)&g_Supervisor.d3d);
    if (g_GameWindow.window != NULL)
    {
        ShowWindow(g_GameWindow.window, SW_HIDE);
        MoveWindow(g_GameWindow.window, 0, 0, 0, 0, FALSE);
        DestroyWindow(g_GameWindow.window);
        g_GameWindow.window = NULL;
    }
    while (ShowCursor(TRUE) < 0)
    {
    }
    if (result == 2)
    {
        // 再起動を要するオプションが変更されたので再起動します
        g_GameErrorContext.log("\x8d\xc4\x8bN\x93\xae\x82\xf0\x97v\x82\xb7\x82\xe9\x83I\x83v\x83V\x83\x87\x83\x93\x82"
                               "\xaa\x95\xcf\x8dX\x82\xb3\x82\xea\x82\xbd\x82\xcc\x82\xc5\x8d\xc4\x8bN\x93\xae\x82\xb5"
                               "\x82\xdc\x82\xb7\r\n");
        g_GameErrorContext.buffer_end = g_GameErrorContext.buffer;
        g_GameErrorContext.buffer[0] = '\0';
        if (!g_Supervisor.present_params.Windowed)
        {
            WINNLSEnableIME(NULL, FALSE);
        }
        for (i32 i = 60; i != 0; i--)
        {
            if (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE))
            {
                TranslateMessage(&msg);
                DispatchMessageA(&msg);
            }
        }
        g_Supervisor.flags &= ~(SUPERVISOR_QUIT_REQUESTED | SUPERVISOR_FLAG_100);
        goto create_d3d;
    }
    strcpy(path, g_GameWindow.save_dir);
    strcat(path, "th16.cfg");
    file_write(path, &g_Supervisor.config.version, 0x64);
    timeEndPeriod(1);
    strcpy(path, g_GameWindow.save_dir);
    strcat(path, "log.txt");
    if (g_GameErrorContext.buffer_end != g_GameErrorContext.buffer)
    {
        g_GameErrorContext.log("---------------------------------------------------------- \r\n");
        if (g_GameErrorContext.show_message_box)
        {
            MessageBoxA(NULL, g_GameErrorContext.buffer, "log", MB_ICONSTOP);
        }
        file_write(path, g_GameErrorContext.buffer, strlen(g_GameErrorContext.buffer));
    }
    SystemParametersInfoA(SPI_SETSCREENSAVEACTIVE, g_GameWindow.screen_save_active, NULL, SPIF_SENDCHANGE);
    SystemParametersInfoA(SPI_SETLOWPOWERACTIVE, g_GameWindow.low_power_active, NULL, SPIF_SENDCHANGE);
    SystemParametersInfoA(SPI_SETPOWEROFFACTIVE, g_GameWindow.power_off_active, NULL, SPIF_SENDCHANGE);
    WINNLSEnableIME(NULL, TRUE);
    if (g_unk_4a6d90 != NULL)
    {
        operator delete(g_unk_4a6d90, sizeof(WinMainUnk));
    }
    g_CriticalSections.enabled = false;
    for (i32 i = 0; i < CS_COUNT; i++)
    {
        DeleteCriticalSection(&g_CriticalSections.cs[i]);
    }
    return 0;
}
