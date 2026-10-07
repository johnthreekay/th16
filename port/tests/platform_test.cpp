// Tests of the platform layer without the game or its data: the file
// namespace (paths, case, the save folder over the game folder), kernel
// objects (threads, events, waits, thread messages), input (keyboard events
// and an SDL virtual joystick through DirectInput and winmm) and the GDI text
// renderer. Run from any directory; it works in ./platform_test_dirs.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <string>

#include <SDL.h>

#include <dinput.h>
#include <direct.h>
#include <mmsystem.h>
#include <process.h>
#include <windows.h>

#include "port_input.h"
#include "port_platform.h"
#include "port_vfs.h"

// The DirectInput ids the game defines (SupervisorSetup.cpp).
extern "C" const GUID GUID_XAxis = {0xa36d02e0, 0xc9f3, 0x11cf, {0xbf, 0xc7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}};
extern "C" const GUID GUID_YAxis = {0xa36d02e1, 0xc9f3, 0x11cf, {0xbf, 0xc7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}};
extern "C" const GUID GUID_ZAxis = {0xa36d02e2, 0xc9f3, 0x11cf, {0xbf, 0xc7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}};
extern "C" const GUID GUID_RxAxis = {0xa36d02f4, 0xc9f3, 0x11cf, {0xbf, 0xc7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}};
extern "C" const GUID GUID_RyAxis = {0xa36d02f5, 0xc9f3, 0x11cf, {0xbf, 0xc7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}};
extern "C" const GUID GUID_RzAxis = {0xa36d02e3, 0xc9f3, 0x11cf, {0xbf, 0xc7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}};
extern "C" const GUID GUID_Slider = {0xa36d02e4, 0xc9f3, 0x11cf, {0xbf, 0xc7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}};
extern "C" const GUID GUID_Key = {0x55728220, 0xd33c, 0x11cf, {0xbf, 0xc7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}};
extern "C" const GUID GUID_POV = {0xa36d02f2, 0xc9f3, 0x11cf, {0xbf, 0xc7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}};
extern "C" const GUID GUID_SysKeyboard = {0x6f1d2b61, 0xd5a0, 0x11cf, {0xbf, 0xc7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}};
extern "C" const GUID IID_IDirectInput8A = {0xbf798030, 0x483a, 0x4da2, {0xaa, 0x99, 0x5d, 0x64, 0xed, 0x36, 0x97, 0x00}};

static int g_failures;
static int g_checks;

#define CHECK(cond)                                                          \
    do                                                                       \
    {                                                                        \
        g_checks++;                                                          \
        if (!(cond))                                                         \
        {                                                                    \
            g_failures++;                                                    \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        }                                                                    \
    } while (0)

static void write_host_file(const std::string &path, const char *text)
{
    FILE *f = fopen(path.c_str(), "wb");
    if (f == NULL)
    {
        fprintf(stderr, "cannot write %s\n", path.c_str());
        exit(1);
    }
    fputs(text, f);
    fclose(f);
}

static std::string read_host_file(const std::string &path)
{
    FILE *f = fopen(path.c_str(), "rb");
    if (f == NULL)
    {
        return "<missing>";
    }
    char buffer[256];
    size_t n = fread(buffer, 1, sizeof(buffer), f);
    fclose(f);
    return std::string(buffer, n);
}

static std::string read_game_file(const char *path)
{
    HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE)
    {
        return "<missing>";
    }
    char buffer[256];
    DWORD read = 0;
    ReadFile(h, buffer, sizeof(buffer), &read, NULL);
    CloseHandle(h);
    return std::string(buffer, read);
}

static void test_files(const std::string &root)
{
    std::string game = root + "/game";
    std::string save = root + "/save";
    mkdir(root.c_str(), 0755);
    mkdir(game.c_str(), 0755);
    mkdir((game + "/Replay").c_str(), 0755);
    write_host_file(game + "/TH16.DAT", "archive");
    write_host_file(game + "/th16.cfg", "game cfg");
    write_host_file(game + "/Replay/th16_ud0001.rpy", "replay 1");
    CHECK(port_vfs_init(game, save));

    char path[MAX_PATH];
    CHECK(GetModuleFileNameA(NULL, path, sizeof(path)) == strlen(PORT_VFS_EXE_PATH));
    CHECK(strcmp(path, "C:\\th16\\th16.exe") == 0);
    char appdata[MAX_PATH];
    CHECK(GetEnvironmentVariableA("APPDATA", appdata, sizeof(appdata)) == strlen(PORT_VFS_APPDATA));

    // Case-insensitive names, either separator, absolute or relative.
    CHECK(read_game_file("C:\\th16\\th16.dat") == "archive");
    CHECK(read_game_file("th16.dat") == "archive");
    CHECK(read_game_file("C:/TH16/Th16.Dat") == "archive");
    CHECK(read_game_file("C:\\AppData\\ShanghaiAlice\\th16\\th16.dat") == "archive");
    CHECK(read_game_file("D:\\th16.dat") == "<missing>");
    CHECK(read_game_file("missing.dat") == "<missing>");
    CHECK(GetLastError() == ERROR_FILE_NOT_FOUND);
    CHECK(read_game_file("nodir\\missing.dat") == "<missing>");
    CHECK(GetLastError() == ERROR_PATH_NOT_FOUND);

    // The save folder: _chdir/_mkdir as the game does them.
    std::string save_dir = std::string(PORT_VFS_APPDATA) + "\\ShanghaiAlice";
    _mkdir(save_dir.c_str());
    save_dir += "\\th16";
    _mkdir(save_dir.c_str());
    save_dir += "\\";
    CHECK(_chdir(save_dir.c_str()) == 0);
    CHECK(_mkdir("snapshot") == 0);
    CHECK(_mkdir("snapshot") == -1);
    struct stat st;
    CHECK(stat((save + "/snapshot").c_str(), &st) == 0);
    // The game folder has it, so it exists: it is made in the save folder
    // when something is written into it.
    CHECK(_mkdir("replay") == -1);
    CHECK(stat((save + "/Replay").c_str(), &st) != 0);
    CHECK(_chdir("nowhere") == -1);

    // Reading falls back to the game folder; writing goes to the save
    // folder only.
    CHECK(read_game_file("th16.cfg") == "game cfg");
    HANDLE h = CreateFileA("th16.cfg", GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    CHECK(GetLastError() == ERROR_ALREADY_EXISTS);
    DWORD written = 0;
    CHECK(WriteFile(h, "saved cfg", 9, &written, NULL) && written == 9);
    CHECK(CloseHandle(h));
    CHECK(read_game_file("th16.cfg") == "saved cfg");
    CHECK(read_host_file(game + "/th16.cfg") == "game cfg");
    CHECK(read_host_file(save + "/th16.cfg") == "saved cfg");

    // Appending to a game folder file copies it over first (Pbg::File 'a').
    h = CreateFileA("C:\\th16\\replay\\TH16_UD0001.RPY", GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS,
                    FILE_ATTRIBUTE_NORMAL, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    CHECK(SetFilePointer(h, 0, NULL, FILE_END) == 8);
    CHECK(WriteFile(h, "+", 1, &written, NULL));
    CloseHandle(h);
    CHECK(read_host_file(save + "/Replay/th16_ud0001.rpy") == "replay 1+");
    CHECK(read_host_file(game + "/Replay/th16_ud0001.rpy") == "replay 1");

    // Seeks: absolute positions far past the end, FILE_CURRENT with 0.
    h = CreateFileA("th16.dat", GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    CHECK(SetFilePointer(h, 388 * 1024 * 1024, NULL, FILE_BEGIN) == 388u * 1024 * 1024);
    CHECK(SetFilePointer(h, 0, NULL, FILE_CURRENT) == 388u * 1024 * 1024);
    CHECK(SetFilePointer(h, 2, NULL, FILE_BEGIN) == 2);
    char buffer[8];
    DWORD read = 0;
    CHECK(ReadFile(h, buffer, 8, &read, NULL) && read == 5 && memcmp(buffer, "chive", 5) == 0);
    CHECK(GetFileSize(h, NULL) == 7);
    CHECK(CloseHandle(h));
    CHECK(!CloseHandle(h));
    CHECK(!CloseHandle(INVALID_HANDLE_VALUE));
    CHECK(!CloseHandle(NULL));

    // Directory listings join both folders.
    write_host_file(save + "/Replay/th16_ud0002.rpy", "replay 2");
    CHECK(_chdir("replay") == 0);
    WIN32_FIND_DATAA find;
    HANDLE search = FindFirstFileA("th16_ud????.rpy", &find);
    CHECK(search != INVALID_HANDLE_VALUE);
    CHECK(strcmp(find.cFileName, "th16_ud0001.rpy") == 0);
    CHECK(FindNextFileA(search, &find) && strcmp(find.cFileName, "th16_ud0002.rpy") == 0 &&
          find.nFileSizeLow == 8);
    CHECK(!FindNextFileA(search, &find) && GetLastError() == ERROR_NO_MORE_FILES);
    CHECK(FindClose(search));
    CHECK(FindFirstFileA("*.bmp", &find) == INVALID_HANDLE_VALUE);
    CHECK(DeleteFileA("th16_ud0002.rpy"));
    CHECK(!DeleteFileA("th16_ud0002.rpy"));
    char cwd[MAX_PATH];
    CHECK(_getcwd(cwd, sizeof(cwd)) != NULL && strcmp(cwd, "C:\\AppData\\ShanghaiAlice\\th16\\replay") == 0);
    CHECK(_chdir("C:\\th16") == 0);
}

static DWORD WINAPI thread_returns_7(LPVOID arg)
{
    Sleep(20);
    *(int *)arg = 1;
    return 7;
}

static HANDLE g_event;
static DWORD g_bgm_result[3];

// Like SoundManager::bgm_thread_proc: wakes on its event or a message.
static DWORD WINAPI bgm_like_thread(LPVOID arg)
{
    MSG msg;
    for (int i = 0; i < 3; i++)
    {
        g_bgm_result[i] = MsgWaitForMultipleObjects(1, &g_event, FALSE, INFINITE, QS_ALLEVENTS);
        if (g_bgm_result[i] == WAIT_OBJECT_0 + 1)
        {
            while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE))
            {
                if (msg.message == WM_QUIT)
                {
                    return 0;
                }
            }
        }
    }
    return 1;
}

static unsigned __stdcall beginthreadex_thread(void *arg)
{
    return 42;
}

static void test_kernel()
{
    int flag = 0;
    DWORD id = 0;
    HANDLE thread = CreateThread(NULL, 0, thread_returns_7, &flag, 0, &id);
    CHECK(thread != NULL && id != 0 && id != GetCurrentThreadId());
    DWORD code = 0;
    CHECK(GetExitCodeThread(thread, &code) && code == STILL_ACTIVE);
    CHECK(WaitForSingleObject(thread, 0) == WAIT_TIMEOUT);
    CHECK(WaitForSingleObject(thread, 2000) == WAIT_OBJECT_0);
    CHECK(flag == 1);
    CHECK(GetExitCodeThread(thread, &code) && code == 7);
    CHECK(CloseHandle(thread));

    unsigned tid = 0;
    HANDLE thread2 = (HANDLE)_beginthreadex(NULL, 0, beginthreadex_thread, NULL, 0, &tid);
    CHECK(WaitForSingleObject(thread2, INFINITE) == WAIT_OBJECT_0);
    CHECK(GetExitCodeThread(thread2, &code) && code == 42);
    CloseHandle(thread2);

    // Auto-reset and manual-reset events.
    HANDLE automatic = CreateEventA(NULL, FALSE, FALSE, NULL);
    CHECK(WaitForSingleObject(automatic, 10) == WAIT_TIMEOUT);
    CHECK(SetEvent(automatic));
    CHECK(WaitForSingleObject(automatic, 0) == WAIT_OBJECT_0);
    CHECK(WaitForSingleObject(automatic, 0) == WAIT_TIMEOUT);
    HANDLE manual = CreateEventA(NULL, TRUE, TRUE, NULL);
    CHECK(WaitForSingleObject(manual, 0) == WAIT_OBJECT_0);
    CHECK(WaitForSingleObject(manual, 0) == WAIT_OBJECT_0);
    CHECK(ResetEvent(manual));
    CHECK(WaitForSingleObject(manual, 0) == WAIT_TIMEOUT);
    HANDLE both[2] = {automatic, manual};
    SetEvent(manual);
    CHECK(WaitForMultipleObjects(2, both, FALSE, 0) == WAIT_OBJECT_0 + 1);
    CHECK(WaitForMultipleObjects(2, both, TRUE, 0) == WAIT_TIMEOUT);
    CloseHandle(automatic);
    CloseHandle(manual);

    // The BGM thread's pattern: event, then a posted WM_QUIT.
    g_event = CreateEventA(NULL, FALSE, FALSE, NULL);
    DWORD bgm_id;
    HANDLE bgm = CreateThread(NULL, 0, bgm_like_thread, NULL, 0, &bgm_id);
    Sleep(20);
    SetEvent(g_event);
    Sleep(20);
    CHECK(PostThreadMessageA(bgm_id, WM_QUIT, 0, 0));
    CHECK(WaitForSingleObject(bgm, 2000) == WAIT_OBJECT_0);
    CHECK(g_bgm_result[0] == WAIT_OBJECT_0);
    CHECK(g_bgm_result[1] == WAIT_OBJECT_0 + 1);
    CHECK(GetExitCodeThread(bgm, &code) && code == 0);
    CHECK(!PostThreadMessageA(bgm_id, WM_QUIT, 0, 0));
    CloseHandle(bgm);
    CloseHandle(g_event);

    // Messages to the calling thread.
    MSG msg;
    PostQuitMessage(3);
    CHECK(PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE) && msg.message == WM_QUIT && msg.wParam == 3);
    CHECK(!PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE));
}

static void key_event(SDL_Scancode scancode, SDL_Keycode keycode, bool down)
{
    SDL_Event event;
    memset(&event, 0, sizeof(event));
    event.type = down ? SDL_KEYDOWN : SDL_KEYUP;
    event.key.keysym.scancode = scancode;
    event.key.keysym.sym = keycode;
    port_input_handle_event(event);
}

static void test_input()
{
    BYTE keys[256];
    key_event(SDL_SCANCODE_Z, SDLK_z, true);
    key_event(SDL_SCANCODE_LSHIFT, SDLK_LSHIFT, true);
    key_event(SDL_SCANCODE_UP, SDLK_UP, true);
    port_input_get_dik_state(keys);
    CHECK(keys[DIK_Z] == 0x80 && keys[DIK_LSHIFT] == 0x80 && keys[DIK_UP] == 0x80 && keys[DIK_X] == 0);
    CHECK(GetKeyboardState(keys));
    CHECK(keys['Z'] == 0x80 && keys[VK_SHIFT] == 0x80 && keys[VK_UP] == 0x80 && keys['X'] == 0);
    key_event(SDL_SCANCODE_Z, SDLK_z, false);
    port_input_get_dik_state(keys);
    CHECK(keys[DIK_Z] == 0 && keys[DIK_LSHIFT] == 0x80);
    port_input_release_all_keys();

    IDirectInput8A *dinput = NULL;
    CHECK(DirectInput8Create(NULL, DIRECTINPUT_VERSION, IID_IDirectInput8A, (void **)&dinput, NULL) == DI_OK);
    IDirectInputDevice8A *keyboard = NULL;
    CHECK(dinput->CreateDevice(GUID_SysKeyboard, &keyboard, NULL) == DI_OK);
    CHECK(keyboard->SetDataFormat(&c_dfDIKeyboard) == DI_OK);
    CHECK(keyboard->GetDeviceState(256, keys) == DIERR_NOTACQUIRED);
    CHECK(keyboard->Acquire() == DI_OK);
    key_event(SDL_SCANCODE_ESCAPE, SDLK_ESCAPE, true);
    CHECK(keyboard->GetDeviceState(256, keys) == DI_OK && keys[DIK_ESCAPE] == 0x80);
    key_event(SDL_SCANCODE_ESCAPE, SDLK_ESCAPE, false);
    keyboard->Release();

    // A virtual game controller (SDL maps it, so it has the XInput layout:
    // virtual button 2 is X, DirectInput button 2).
    if (SDL_InitSubSystem(SDL_INIT_JOYSTICK) != 0 ||
        SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER, 2, 4, 1) < 0)
    {
        fprintf(stderr, "no virtual joystick (%s); skipping the pad tests\n", SDL_GetError());
        dinput->Release();
        return;
    }
    struct Context
    {
        IDirectInput8A *dinput;
        IDirectInputDevice8A *pad;
    } context = {dinput, NULL};
    dinput->EnumDevices(
        DI8DEVCLASS_GAMECTRL,
        [](LPCDIDEVICEINSTANCEA instance, LPVOID ref) -> BOOL {
            Context *c = (Context *)ref;
            c->dinput->CreateDevice(instance->guidInstance, &c->pad, NULL);
            return DIENUM_STOP;
        },
        &context, DIEDFL_ATTACHEDONLY);
    CHECK(context.pad != NULL);
    if (context.pad == NULL)
    {
        dinput->Release();
        return;
    }
    IDirectInputDevice8A *pad = context.pad;
    CHECK(pad->SetDataFormat(&c_dfDIJoystick2) == DI_OK);
    DIDEVCAPS caps;
    caps.dwSize = sizeof(caps);
    CHECK(pad->GetCapabilities(&caps) == DI_OK && caps.dwButtons == 10 && caps.dwAxes == 5 && caps.dwPOVs == 1);
    // The game's enum_controller_axes: every axis to -1000..1000.
    pad->EnumObjects(
        [](LPCDIDEVICEOBJECTINSTANCEA object, LPVOID ref) -> BOOL {
            if (object->dwType & DIDFT_AXIS)
            {
                DIPROPRANGE range;
                range.diph.dwSize = sizeof(range);
                range.diph.dwHeaderSize = sizeof(range.diph);
                range.diph.dwObj = object->dwType;
                range.diph.dwHow = DIPH_BYID;
                range.lMin = -1000;
                range.lMax = 1000;
                ((IDirectInputDevice8A *)ref)->SetProperty(DIPROP_RANGE, &range.diph);
            }
            return DIENUM_CONTINUE;
        },
        pad, DIDFT_ALL);
    SDL_Joystick *virtual_pad = SDL_JoystickOpen(SDL_NumJoysticks() - 1);
    SDL_JoystickSetVirtualAxis(virtual_pad, 0, 32767);
    SDL_JoystickSetVirtualAxis(virtual_pad, 1, -32768);
    SDL_JoystickSetVirtualButton(virtual_pad, 2, 1);
    SDL_JoystickUpdate();
    CHECK(pad->Poll() == DIERR_NOTACQUIRED);
    CHECK(pad->Acquire() == DI_OK);
    CHECK(pad->Poll() == DI_OK);
    DIJOYSTATE2 js;
    CHECK(pad->GetDeviceState(sizeof(js), &js) == DI_OK);
    CHECK(js.lX == 1000 && js.lY == -1000 && js.rgdwPOV[0] == 0xffffffffu);
    CHECK(js.rgbButtons[2] == 0x80 && js.rgbButtons[0] == 0);
    SDL_JoystickSetVirtualAxis(virtual_pad, 0, 0);
    SDL_JoystickSetVirtualButton(virtual_pad, 2, 0);
    SDL_JoystickUpdate();
    CHECK(pad->GetDeviceState(sizeof(js), &js) == DI_OK && js.lX == 0 && js.rgbButtons[2] == 0);
    SDL_JoystickSetVirtualAxis(virtual_pad, 0, 32767);
    SDL_JoystickSetVirtualButton(virtual_pad, 2, 1);
    SDL_JoystickUpdate();

    JOYINFOEX info;
    memset(&info, 0, sizeof(info));
    info.dwSize = sizeof(info);
    info.dwFlags = JOY_RETURNALL;
    CHECK(joyGetPosEx(JOYSTICKID1, &info) == JOYERR_NOERROR);
    CHECK(info.dwXpos == 65535 && info.dwYpos == 0 && info.dwButtons == 4);
    JOYCAPSA joycaps;
    CHECK(joyGetDevCapsA(JOYSTICKID1, &joycaps, sizeof(joycaps)) == JOYERR_NOERROR && joycaps.wXmax == 65535);
    pad->Release();
    dinput->Release();
    SDL_JoystickClose(virtual_pad);
}

static void test_text()
{
    std::string utf8 = port_sjis_to_utf8("\x93\x8c\x95\xfb Project");
    CHECK(utf8 == "\xe6\x9d\xb1\xe6\x96\xb9 Project");
    std::string sjis;
    CHECK(port_utf8_to_sjis(utf8.c_str(), &sjis) && sjis == "\x93\x8c\x95\xfb Project");

    // TextHelper's setup: a top-down A4R4G4B4 DIB section in a memory DC.
    struct
    {
        BITMAPINFOHEADER header;
        DWORD masks[4];
    } info;
    memset(&info, 0, sizeof(info));
    info.header.biSize = 108;
    info.header.biWidth = 256;
    info.header.biHeight = -64;
    info.header.biPlanes = 1;
    info.header.biBitCount = 16;
    info.header.biCompression = BI_BITFIELDS;
    info.masks[0] = 0x0f00;
    info.masks[1] = 0x00f0;
    info.masks[2] = 0x000f;
    info.masks[3] = 0xf000;
    void *bits = NULL;
    HBITMAP bitmap = CreateDIBSection(NULL, (BITMAPINFO *)&info, 0, &bits, NULL, 0);
    CHECK(bitmap != NULL && bits != NULL);
    if (bitmap == NULL)
    {
        return;
    }
    uint16_t *pixels = (uint16_t *)bits;
    for (int i = 0; i < 256 * 64; i++)
    {
        pixels[i] = 0xf000;
    }
    HDC dc = CreateCompatibleDC(NULL);
    HGDIOBJ old_bitmap = SelectObject(dc, bitmap);
    HFONT font = CreateFontA(24, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, SHIFTJIS_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, PROOF_QUALITY, FF_ROMAN | FIXED_PITCH,
                             "\x82l\x82r \x83S\x83V\x83" "b\x83N");
    CHECK(font != NULL);
    HGDIOBJ old_font = SelectObject(dc, font);
    SIZE size;
    CHECK(GetTextExtentPoint32A(dc, "A\x93\x8c", 3, &size) && size.cx == 36 && size.cy == 24);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(255, 255, 255));
    CHECK(TextOutA(dc, 2, 2, "\x93\x8c\x95\xfb", 4));
    // Drawn pixels lose their alpha bits; white at full coverage.
    int touched = 0;
    int white = 0;
    for (int i = 0; i < 256 * 64; i++)
    {
        if ((pixels[i] & 0xf000) == 0)
        {
            touched++;
            white += pixels[i] == 0x0fff;
        }
    }
    CHECK(touched > 100 && white > 20);
    // Nothing outside the two characters' cells (x 2..50, y 2..26).
    bool outside = false;
    for (int y = 0; y < 64; y++)
    {
        for (int x = 0; x < 256; x++)
        {
            if ((pixels[y * 256 + x] & 0xf000) == 0 && (x < 2 || x >= 50 || y < 2 || y >= 28))
            {
                outside = true;
            }
        }
    }
    CHECK(!outside);
    SelectObject(dc, old_font);
    SelectObject(dc, old_bitmap);
    CHECK(DeleteObject(font));
    CHECK(DeleteObject(bitmap));
    CHECK(DeleteDC(dc));
}

int main(int argc, char **argv)
{
    SDL_Init(SDL_INIT_EVENTS);
    char cwd[4096];
    std::string root = std::string(getcwd(cwd, sizeof(cwd))) + "/platform_test_dirs";
    if (system(("rm -rf '" + root + "'").c_str()) != 0)
    {
        return 1;
    }
    test_files(root);
    test_kernel();
    test_input();
    test_text();
    SDL_Quit();
    printf("%d checks, %d failed\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
