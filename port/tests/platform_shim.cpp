// Test-only stand-ins for the platform layer, so the renderer can be tested
// before the real window, file and thread code (port/src/win32_*.cpp)
// exists. Built only with -DTH16_TEST_SHIM=ON, which links this file first
// with --allow-multiple-definition: its functions win over the stubs of the
// same name. The real platform layer makes all of this obsolete.
//
// - Files: paths are the game's (backslashes); relative ones resolve against
//   the current directory, and a read-only open that finds nothing falls
//   back to the same base name in the game folder (TH16_GAME_DIR), which is
//   never written. GetModuleFileNameA puts the "exe" in the current
//   directory and APPDATA is ./appdata, so everything the game writes stays
//   in the directory the test runs in.
// - Threads, events: std::thread and condition variables.
// - Window: an SDL window handed to the renderer (port_gl_attach_window).
//   No messages; GetKeyboardState plays the script in TH16_SHIM_KEYS
//   ("frame:KEY:frames,...", frames counted in GetKeyboardState calls,
//   KEY one of Z X C P UP DOWN LEFT RIGHT ESC SHIFT CTRL ENTER).
// - Sound: without DirectSound nothing consumes the BGM command queue, and
//   GameThread waits for it to drain before a stage starts; a shim thread
//   drops the commands.
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <SDL.h>

#include <windows.h>
#include <mmsystem.h>
#include <process.h>

#include "../src/d3d9_gl.h"
#include "SoundManager.h"

namespace
{

std::string game_dir()
{
    const char *dir = getenv("TH16_GAME_DIR");
    if (dir != NULL && dir[0] != '\0')
    {
        return dir;
    }
    const char *home = getenv("HOME");
    return std::string(home != NULL ? home : "") +
           "/Touhou Project/(TH16) Touhou Tenkuushou ~ Hidden Star in Four Seasons";
}

std::string host_path(const char *path)
{
    std::string out(path);
    for (char &c : out)
    {
        if (c == '\\')
        {
            c = '/';
        }
    }
    return out;
}

std::string win_path(const std::string &path)
{
    std::string out(path);
    for (char &c : out)
    {
        if (c == '/')
        {
            c = '\\';
        }
    }
    return out;
}

struct ShimObject
{
    virtual ~ShimObject()
    {
    }
};

struct ShimFile : ShimObject
{
    int fd = -1;
    ~ShimFile() override
    {
        if (fd >= 0)
        {
            close(fd);
        }
    }
};

struct ShimWaitable : ShimObject
{
    std::mutex mutex;
    std::condition_variable cv;
    bool signaled = false;
    bool manual = true;
};

struct ShimThread : ShimWaitable
{
    DWORD exit_code = STILL_ACTIVE;
};

std::mutex g_handles_mutex;
std::map<void *, std::shared_ptr<ShimObject>> g_handles;

HANDLE add_handle(std::shared_ptr<ShimObject> object)
{
    std::lock_guard<std::mutex> guard(g_handles_mutex);
    void *key = object.get();
    g_handles[key] = std::move(object);
    return key;
}

std::shared_ptr<ShimObject> find_handle(HANDLE handle)
{
    std::lock_guard<std::mutex> guard(g_handles_mutex);
    auto it = g_handles.find(handle);
    return it != g_handles.end() ? it->second : nullptr;
}

ShimFile *as_file(const std::shared_ptr<ShimObject> &object)
{
    return dynamic_cast<ShimFile *>(object.get());
}

template <typename F> HANDLE start_thread(F body)
{
    auto thread = std::make_shared<ShimThread>();
    thread->manual = true;
    HANDLE handle = add_handle(thread);
    std::thread([thread, body]() {
        DWORD code = body();
        std::lock_guard<std::mutex> guard(thread->mutex);
        thread->exit_code = code;
        thread->signaled = true;
        thread->cv.notify_all();
    }).detach();
    return handle;
}

bool wait_one(ShimWaitable *w, DWORD ms)
{
    std::unique_lock<std::mutex> lock(w->mutex);
    if (ms == INFINITE)
    {
        w->cv.wait(lock, [w] { return w->signaled; });
    }
    else if (!w->cv.wait_for(lock, std::chrono::milliseconds(ms), [w] { return w->signaled; }))
    {
        return false;
    }
    if (!w->manual)
    {
        w->signaled = false;
    }
    return true;
}

// Drops queued BGM commands while there is no sound device.
void start_bgm_queue_drain()
{
    std::thread([]() {
        for (;;)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            if (g_SoundManager.manager == NULL)
            {
                for (i32 i = 0; i < 0x1f; i++)
                {
                    g_SoundManager.bgm_commands[i].command = 0;
                }
            }
        }
    }).detach();
}

WNDPROC g_window_proc = NULL;
SDL_Window *g_window = NULL;
int g_cursor_count = 0;

} // namespace

extern "C" {

// --- Files -----------------------------------------------------------------

HANDLE CreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
                   LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition,
                   DWORD dwFlagsAndAttributes, HANDLE hTemplateFile)
{
    std::string path = host_path(lpFileName);
    bool write = (dwDesiredAccess & GENERIC_WRITE) != 0;
    int flags = write ? ((dwDesiredAccess & GENERIC_READ) ? O_RDWR : O_WRONLY) : O_RDONLY;
    switch (dwCreationDisposition)
    {
    case CREATE_NEW:
        flags |= O_CREAT | O_EXCL;
        break;
    case CREATE_ALWAYS:
        flags |= O_CREAT | O_TRUNC;
        break;
    case OPEN_ALWAYS:
        flags |= O_CREAT;
        break;
    case TRUNCATE_EXISTING:
        flags |= O_TRUNC;
        break;
    default:
        break;
    }
    int fd = open(path.c_str(), flags | O_CLOEXEC, 0644);
    if (fd < 0 && !write)
    {
        size_t slash = path.find_last_of('/');
        std::string fallback = game_dir() + "/" + (slash == std::string::npos ? path : path.substr(slash + 1));
        fd = open(fallback.c_str(), O_RDONLY | O_CLOEXEC);
    }
    if (fd < 0)
    {
        SetLastError(ERROR_FILE_NOT_FOUND);
        return INVALID_HANDLE_VALUE;
    }
    auto file = std::make_shared<ShimFile>();
    file->fd = fd;
    return add_handle(file);
}

BOOL ReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, LPDWORD lpNumberOfBytesRead,
              LPOVERLAPPED lpOverlapped)
{
    auto object = find_handle(hFile);
    ShimFile *file = as_file(object);
    if (file == NULL)
    {
        return FALSE;
    }
    size_t done = 0;
    while (done < nNumberOfBytesToRead)
    {
        ssize_t n = read(file->fd, (char *)lpBuffer + done, nNumberOfBytesToRead - done);
        if (n < 0 && errno == EINTR)
        {
            continue;
        }
        if (n <= 0)
        {
            break;
        }
        done += n;
    }
    if (lpNumberOfBytesRead != NULL)
    {
        *lpNumberOfBytesRead = (DWORD)done;
    }
    return TRUE;
}

BOOL WriteFile(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite, LPDWORD lpNumberOfBytesWritten,
               LPOVERLAPPED lpOverlapped)
{
    auto object = find_handle(hFile);
    ShimFile *file = as_file(object);
    if (file == NULL)
    {
        return FALSE;
    }
    ssize_t n = write(file->fd, lpBuffer, nNumberOfBytesToWrite);
    if (lpNumberOfBytesWritten != NULL)
    {
        *lpNumberOfBytesWritten = n > 0 ? (DWORD)n : 0;
    }
    return n >= 0;
}

DWORD SetFilePointer(HANDLE hFile, LONG lDistanceToMove, PLONG lpDistanceToMoveHigh, DWORD dwMoveMethod)
{
    auto object = find_handle(hFile);
    ShimFile *file = as_file(object);
    if (file == NULL)
    {
        return INVALID_SET_FILE_POINTER;
    }
    off_t offset = lDistanceToMove;
    if (lpDistanceToMoveHigh != NULL)
    {
        offset = (off_t)(((uint64_t)(uint32_t)*lpDistanceToMoveHigh << 32) | (uint32_t)lDistanceToMove);
    }
    int whence = dwMoveMethod == FILE_CURRENT ? SEEK_CUR : dwMoveMethod == FILE_END ? SEEK_END : SEEK_SET;
    off_t pos = lseek(file->fd, offset, whence);
    if (pos < 0)
    {
        return INVALID_SET_FILE_POINTER;
    }
    if (lpDistanceToMoveHigh != NULL)
    {
        *lpDistanceToMoveHigh = (LONG)((uint64_t)pos >> 32);
    }
    return (DWORD)pos;
}

DWORD GetFileSize(HANDLE hFile, LPDWORD lpFileSizeHigh)
{
    auto object = find_handle(hFile);
    ShimFile *file = as_file(object);
    struct stat st;
    if (file == NULL || fstat(file->fd, &st) != 0)
    {
        return INVALID_SET_FILE_POINTER;
    }
    if (lpFileSizeHigh != NULL)
    {
        *lpFileSizeHigh = (DWORD)((uint64_t)st.st_size >> 32);
    }
    return (DWORD)st.st_size;
}

BOOL DeleteFileA(LPCSTR lpFileName)
{
    return unlink(host_path(lpFileName).c_str()) == 0;
}

BOOL CloseHandle(HANDLE hObject)
{
    std::lock_guard<std::mutex> guard(g_handles_mutex);
    g_handles.erase(hObject);
    return TRUE;
}

DWORD GetModuleFileNameA(HMODULE hModule, LPSTR lpFilename, DWORD nSize)
{
    char cwd[2048];
    if (getcwd(cwd, sizeof(cwd)) == NULL)
    {
        return 0;
    }
    std::string path = win_path(std::string(cwd) + "/th16.exe");
    snprintf(lpFilename, nSize, "%s", path.c_str());
    return (DWORD)strlen(lpFilename);
}

DWORD GetEnvironmentVariableA(LPCSTR lpName, LPSTR lpBuffer, DWORD nSize)
{
    if (strcmp(lpName, "APPDATA") != 0)
    {
        return 0;
    }
    char cwd[2048];
    if (getcwd(cwd, sizeof(cwd)) == NULL)
    {
        return 0;
    }
    std::string dir = std::string(cwd) + "/appdata";
    mkdir(dir.c_str(), 0755);
    std::string path = win_path(dir);
    snprintf(lpBuffer, nSize, "%s", path.c_str());
    return (DWORD)strlen(lpBuffer);
}

// --- Threads and events ----------------------------------------------------

HANDLE CreateThread(LPSECURITY_ATTRIBUTES lpThreadAttributes, SIZE_T dwStackSize,
                    LPTHREAD_START_ROUTINE lpStartAddress, LPVOID lpParameter, DWORD dwCreationFlags,
                    LPDWORD lpThreadId)
{
    if (lpThreadId != NULL)
    {
        *lpThreadId = 1;
    }
    return start_thread([=]() { return (DWORD)lpStartAddress(lpParameter); });
}

uintptr_t _beginthreadex(void *security, unsigned stack_size, unsigned(__stdcall *start_address)(void *),
                         void *arglist, unsigned initflag, unsigned *thrdaddr)
{
    if (thrdaddr != NULL)
    {
        *thrdaddr = 1;
    }
    return (uintptr_t)start_thread([=]() { return (DWORD)start_address(arglist); });
}

uintptr_t _beginthread(void(__cdecl *start_address)(void *), unsigned stack_size, void *arglist)
{
    std::thread([=]() { start_address(arglist); }).detach();
    return 1;
}

BOOL GetExitCodeThread(HANDLE hThread, LPDWORD lpExitCode)
{
    auto object = find_handle(hThread);
    ShimThread *thread = dynamic_cast<ShimThread *>(object.get());
    if (thread == NULL || lpExitCode == NULL)
    {
        return FALSE;
    }
    std::lock_guard<std::mutex> guard(thread->mutex);
    *lpExitCode = thread->exit_code;
    return TRUE;
}

BOOL SetThreadPriority(HANDLE hThread, int nPriority)
{
    return TRUE;
}

DWORD WaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds)
{
    auto object = find_handle(hHandle);
    ShimWaitable *w = dynamic_cast<ShimWaitable *>(object.get());
    if (w == NULL)
    {
        return WAIT_OBJECT_0;
    }
    return wait_one(w, dwMilliseconds) ? WAIT_OBJECT_0 : WAIT_TIMEOUT;
}

DWORD WaitForMultipleObjects(DWORD nCount, const HANDLE *lpHandles, BOOL bWaitAll, DWORD dwMilliseconds)
{
    auto start = std::chrono::steady_clock::now();
    for (;;)
    {
        bool all = true;
        for (DWORD i = 0; i < nCount; i++)
        {
            auto object = find_handle(lpHandles[i]);
            ShimWaitable *w = dynamic_cast<ShimWaitable *>(object.get());
            if (w == NULL || wait_one(w, 0))
            {
                if (!bWaitAll)
                {
                    return WAIT_OBJECT_0 + i;
                }
            }
            else
            {
                all = false;
            }
        }
        if (bWaitAll && all)
        {
            return WAIT_OBJECT_0;
        }
        if (dwMilliseconds != INFINITE && std::chrono::steady_clock::now() - start >=
                                              std::chrono::milliseconds(dwMilliseconds))
        {
            return WAIT_TIMEOUT;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

DWORD MsgWaitForMultipleObjects(DWORD nCount, const HANDLE *pHandles, BOOL fWaitAll, DWORD dwMilliseconds,
                                DWORD dwWakeMask)
{
    return WaitForMultipleObjects(nCount, pHandles, fWaitAll, dwMilliseconds);
}

HANDLE CreateEventA(LPSECURITY_ATTRIBUTES lpEventAttributes, BOOL bManualReset, BOOL bInitialState, LPCSTR lpName)
{
    auto event = std::make_shared<ShimWaitable>();
    event->manual = bManualReset != 0;
    event->signaled = bInitialState != 0;
    return add_handle(event);
}

BOOL SetEvent(HANDLE hEvent)
{
    auto object = find_handle(hEvent);
    ShimWaitable *w = dynamic_cast<ShimWaitable *>(object.get());
    if (w == NULL)
    {
        return FALSE;
    }
    std::lock_guard<std::mutex> guard(w->mutex);
    w->signaled = true;
    w->cv.notify_all();
    return TRUE;
}

BOOL ResetEvent(HANDLE hEvent)
{
    auto object = find_handle(hEvent);
    ShimWaitable *w = dynamic_cast<ShimWaitable *>(object.get());
    if (w == NULL)
    {
        return FALSE;
    }
    std::lock_guard<std::mutex> guard(w->mutex);
    w->signaled = false;
    return TRUE;
}

// --- Window ----------------------------------------------------------------

ATOM RegisterClassA(const WNDCLASSA *lpWndClass)
{
    g_window_proc = lpWndClass->lpfnWndProc;
    return 1;
}

HWND CreateWindowExA(DWORD dwExStyle, LPCSTR lpClassName, LPCSTR lpWindowName, DWORD dwStyle, int X, int Y,
                     int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam)
{
    if (!SDL_WasInit(SDL_INIT_VIDEO) && SDL_InitSubSystem(SDL_INIT_VIDEO) != 0)
    {
        fprintf(stderr, "[shim] SDL video init failed: %s\n", SDL_GetError());
        return NULL;
    }
    port_gl_prepare_window();
    g_window = SDL_CreateWindow("th16 (test shim)", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, nWidth, nHeight,
                                port_gl_window_flags());
    if (g_window == NULL)
    {
        fprintf(stderr, "[shim] SDL_CreateWindow failed: %s\n", SDL_GetError());
        return NULL;
    }
    port_gl_attach_window(g_window);
    start_bgm_queue_drain();
    return (HWND)g_window;
}

BOOL DestroyWindow(HWND hWnd)
{
    if (hWnd != NULL && (SDL_Window *)hWnd == g_window)
    {
        port_gl_detach_window(g_window);
        SDL_DestroyWindow(g_window);
        g_window = NULL;
    }
    return TRUE;
}

BOOL ShowWindow(HWND hWnd, int nCmdShow)
{
    return TRUE;
}

BOOL GetWindowRect(HWND hWnd, LPRECT lpRect)
{
    int w = 640, h = 480;
    if (g_window != NULL)
    {
        SDL_GetWindowSize(g_window, &w, &h);
    }
    lpRect->left = 0;
    lpRect->top = 0;
    lpRect->right = w;
    lpRect->bottom = h;
    return TRUE;
}

BOOL GetClientRect(HWND hWnd, LPRECT lpRect)
{
    return GetWindowRect(hWnd, lpRect);
}

LRESULT DefWindowProcA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return 0;
}

LRESULT SendMessageA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return 0;
}

BOOL PeekMessageA(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax, UINT wRemoveMsg)
{
    if (g_window != NULL)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
        }
    }
    return FALSE;
}

int GetSystemMetrics(int nIndex)
{
    switch (nIndex)
    {
    case SM_CXSCREEN:
        return 1920;
    case SM_CYSCREEN:
        return 1080;
    case SM_CYCAPTION:
        return 20;
    default:
        return 3;
    }
}

int ShowCursor(BOOL bShow)
{
    return bShow ? ++g_cursor_count : --g_cursor_count;
}

HDC GetDC(HWND hWnd)
{
    return (HDC)1;
}

int ReleaseDC(HWND hWnd, HDC hDC)
{
    return 1;
}

int GetDeviceCaps(HDC hdc, int index)
{
    switch (index)
    {
    case VREFRESH:
        return 60;
    case 12: // BITSPIXEL
        return 32;
    case 88: // LOGPIXELSX
    case 90: // LOGPIXELSY
        return 96;
    default:
        return 0;
    }
}

// Scripted keys: "frame:KEY:frames,..." in GetKeyboardState calls.
BOOL GetKeyboardState(PBYTE lpKeyState)
{
    static std::vector<std::pair<std::pair<int, int>, int>> script; // (start, length), vk
    static bool parsed = false;
    static int frame = 0;
    if (!parsed)
    {
        parsed = true;
        const char *env = getenv("TH16_SHIM_KEYS");
        std::string s = env != NULL ? env : "";
        size_t pos = 0;
        while (pos < s.size())
        {
            size_t end = s.find(',', pos);
            std::string item = s.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
            pos = end == std::string::npos ? s.size() : end + 1;
            int start = 0, length = 4;
            char key[32] = {0};
            if (sscanf(item.c_str(), "%d:%31[A-Z]:%d", &start, key, &length) < 2)
            {
                continue;
            }
            static const struct
            {
                const char *name;
                int vk;
            } keys[] = {{"Z", 'Z'},        {"X", 'X'},          {"C", 'C'},          {"UP", 0x26},
                        {"DOWN", 0x28},    {"LEFT", 0x25},      {"RIGHT", 0x27},     {"ESC", 0x1b},
                        {"SHIFT", 0x10},   {"ENTER", 0x0d},     {"CTRL", 0x11},      {"P", 'P'}};
            for (auto &k : keys)
            {
                if (strcmp(k.name, key) == 0)
                {
                    script.push_back(std::make_pair(std::make_pair(start, length), k.vk));
                }
            }
        }
    }
    memset(lpKeyState, 0, 256);
    for (auto &entry : script)
    {
        if (frame >= entry.first.first && frame < entry.first.first + entry.first.second)
        {
            lpKeyState[entry.second] = 0x80;
        }
    }
    frame++;
    return TRUE;
}

} // extern "C"
