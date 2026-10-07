// user32 over SDL: the game window, its messages, the resolution dialog,
// the cursor, the keyboard state and message boxes.
//
// The game window is an SDL window (port_sdl_window); SDL events become the
// messages GameWindow.cpp's window_proc handles: WM_ACTIVATEAPP (focus),
// WM_CLOSE (closing the window, or SDL_QUIT on Ctrl+C), WM_SIZE with
// SIZE_MAXIMIZED, WM_SYSKEYDOWN VK_RETURN (Alt+Enter), WM_KEYDOWN/UP,
// WM_LBUTTONDOWN and WM_SETCURSOR. The main thread's message queue holds
// them (port_kernel.h); PeekMessageA and GetMessageA pump SDL's events into
// it when it is empty.
//
// Window sizes: the system metrics for frames and captions are 0, so the
// size the game asks for is the client size. A WS_POPUP window is full
// screen, as SDL_WINDOW_FULLSCREEN_DESKTOP (the renderer scales the back
// buffer into the drawable, see port_window_drawable_size); the game
// switches between the two with SetWindowLongA(GWL_STYLE) and SetWindowPos.
//
// Without SDL video (no display), the window exists without an SDL window
// and no events arrive.
#include <stdlib.h>
#include <string.h>

#include <map>
#include <mutex>
#include <set>
#include <string>

#include <SDL.h>

#include <windows.h>

#include "port_input.h"
#include "port_kernel.h"
#include "port_platform.h"
#include "port_stub.h"
#ifdef TH16_THCRAP
#include "thcrap/thcrap.h"
#endif

namespace
{

enum HwndKind
{
    HWND_KIND_WINDOW,
    HWND_KIND_DIALOG,
    HWND_KIND_DIALOG_ITEM,
};

struct PortHwnd
{
    HwndKind kind;
};

struct PortWindow : PortHwnd
{
    WNDPROC proc;
    SDL_Window *sdl;
    DWORD style;
    DWORD ex_style;
    // The windowed size and position (client area; no frame).
    int x;
    int y;
    int width;
    int height;
};

struct PortDialogItem : PortHwnd
{
    struct PortDialog *dialog = NULL;
    int id = 0;
    UINT check = BST_UNCHECKED;
};

struct PortDialog : PortHwnd
{
    DLGPROC proc;
    std::map<int, PortDialogItem> items;
};

// The window procedure of each registered class.
std::map<std::string, WNDPROC> g_classes;
// Every live HWND (main thread only, like the window functions).
std::set<PortHwnd *> g_hwnds;
PortWindow *g_game_window;

int g_cursor_count;
bool g_cursor_set = true;
bool g_quit_posted;

PortHwnd *from_hwnd(HWND hwnd)
{
    PortHwnd *object = (PortHwnd *)hwnd;
    return object != NULL && g_hwnds.count(object) ? object : NULL;
}

PortWindow *window_of(HWND hwnd)
{
    PortHwnd *object = from_hwnd(hwnd);
    return object != NULL && object->kind == HWND_KIND_WINDOW ? (PortWindow *)object : NULL;
}

PortDialog *dialog_of(HWND hwnd)
{
    PortHwnd *object = from_hwnd(hwnd);
    return object != NULL && object->kind == HWND_KIND_DIALOG ? (PortDialog *)object : NULL;
}

PortDialogItem *item_of(HWND hwnd)
{
    PortHwnd *object = from_hwnd(hwnd);
    return object != NULL && object->kind == HWND_KIND_DIALOG_ITEM ? (PortDialogItem *)object : NULL;
}

bool headless()
{
    const char *driver = SDL_WasInit(SDL_INIT_VIDEO) ? SDL_GetCurrentVideoDriver() : NULL;
    return driver == NULL || strcmp(driver, "offscreen") == 0 || strcmp(driver, "dummy") == 0;
}

void post(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    MSG msg = {};
    msg.hwnd = hwnd;
    msg.message = message;
    msg.wParam = wparam;
    msg.lParam = lparam;
    msg.time = GetTickCount();
    port_post_thread_message(port_main_thread_id(), msg);
}

void update_cursor()
{
    if (SDL_WasInit(SDL_INIT_VIDEO))
    {
        SDL_ShowCursor(g_cursor_count >= 0 && g_cursor_set ? SDL_ENABLE : SDL_DISABLE);
    }
}

// Full screen for WS_POPUP windows (SDL_WINDOW_FULLSCREEN_DESKTOP), the
// windowed size and position otherwise.
void apply_window_mode(PortWindow *window)
{
    if (window->sdl == NULL)
    {
        return;
    }
    if (window->style & WS_POPUP)
    {
        port_log("window: full screen");
        if (SDL_SetWindowFullscreen(window->sdl, SDL_WINDOW_FULLSCREEN_DESKTOP) != 0)
        {
            port_log("SDL_SetWindowFullscreen failed: %s", SDL_GetError());
        }
        return;
    }
    port_log("window: %dx%d windowed", window->width, window->height);
    SDL_SetWindowFullscreen(window->sdl, 0);
    if (SDL_GetWindowFlags(window->sdl) & SDL_WINDOW_MAXIMIZED)
    {
        SDL_RestoreWindow(window->sdl);
    }
    if (window->width > 0 && window->height > 0)
    {
        SDL_SetWindowSize(window->sdl, window->width, window->height);
    }
    SDL_SetWindowPosition(window->sdl, window->x, window->y);
}

// Whether (x, y) puts a width x height window mostly on some display.
bool position_on_screen(int x, int y, int width, int height)
{
    int count = SDL_GetNumVideoDisplays();
    for (int i = 0; i < count; i++)
    {
        SDL_Rect bounds;
        if (SDL_GetDisplayUsableBounds(i, &bounds) == 0 && x >= bounds.x && y >= bounds.y &&
            x + width / 2 <= bounds.x + bounds.w && y + height / 2 <= bounds.y + bounds.h)
        {
            return true;
        }
    }
    return false;
}

// SDL events to window messages.
void translate_event(const SDL_Event &event)
{
    static const bool debug = getenv("TH16_DEBUG_EVENTS") != NULL;
    if (debug)
    {
        if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP)
        {
            port_log("event: key %s scancode %d", event.type == SDL_KEYDOWN ? "down" : "up",
                     (int)event.key.keysym.scancode);
        }
        else if (event.type == SDL_WINDOWEVENT)
        {
            port_log("event: window %d", (int)event.window.event);
        }
        else
        {
            port_log("event: 0x%x", (unsigned)event.type);
        }
    }
    port_input_handle_event(event);
    if (g_game_window == NULL)
    {
        return;
    }
    HWND hwnd = (HWND)g_game_window;
    switch (event.type)
    {
    case SDL_QUIT:
        if (!g_quit_posted)
        {
            port_log("quit requested");
            post(hwnd, WM_CLOSE, 0, 0);
            g_quit_posted = true;
        }
        break;
    case SDL_WINDOWEVENT:
        if (g_game_window->sdl == NULL || event.window.windowID != SDL_GetWindowID(g_game_window->sdl))
        {
            break;
        }
        switch (event.window.event)
        {
        case SDL_WINDOWEVENT_CLOSE:
            post(hwnd, WM_CLOSE, 0, 0);
            break;
        case SDL_WINDOWEVENT_FOCUS_GAINED:
            post(hwnd, WM_ACTIVATEAPP, TRUE, 0);
            post(hwnd, WM_SETCURSOR, (WPARAM)hwnd, 1);
            break;
        case SDL_WINDOWEVENT_FOCUS_LOST:
            port_input_release_all_keys();
            post(hwnd, WM_ACTIVATEAPP, FALSE, 0);
            break;
        case SDL_WINDOWEVENT_ENTER:
            post(hwnd, WM_SETCURSOR, (WPARAM)hwnd, 1);
            break;
        case SDL_WINDOWEVENT_MAXIMIZED:
            post(hwnd, WM_SIZE, SIZE_MAXIMIZED, 0);
            break;
        case SDL_WINDOWEVENT_MINIMIZED:
            post(hwnd, WM_SIZE, SIZE_MINIMIZED, 0);
            break;
        case SDL_WINDOWEVENT_RESTORED:
            post(hwnd, WM_SIZE, SIZE_RESTORED, 0);
            break;
        case SDL_WINDOWEVENT_MOVED:
            if (!(g_game_window->style & WS_POPUP) &&
                !(SDL_GetWindowFlags(g_game_window->sdl) & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_MAXIMIZED)))
            {
                g_game_window->x = event.window.data1;
                g_game_window->y = event.window.data2;
            }
            break;
        case SDL_WINDOWEVENT_EXPOSED:
            post(hwnd, WM_PAINT, 0, 0);
            break;
        }
        break;
    case SDL_KEYDOWN:
    case SDL_KEYUP: {
        int vk = port_input_vk_from_sdl(event.key.keysym.scancode, event.key.keysym.sym);
        if (vk == 0)
        {
            break;
        }
        bool alt = (event.key.keysym.mod & KMOD_ALT) != 0;
        // Alt combinations and F10 are system keys, as on Windows.
        bool system = alt || vk == VK_F10 || vk == VK_MENU || vk == 0xa4 || vk == 0xa5;
        UINT message = event.type == SDL_KEYDOWN ? (system ? WM_SYSKEYDOWN : WM_KEYDOWN)
                                                 : (system ? WM_SYSKEYUP : WM_KEYUP);
        LPARAM lparam = 1 | ((LPARAM)(event.key.keysym.scancode & 0xff) << 16) | (alt ? (1 << 29) : 0) |
                        (event.key.repeat ? (1 << 30) : 0) | (event.type == SDL_KEYUP ? (3u << 30) : 0);
        // The modifier keys arrive as the generic codes.
        if (vk == 0xa0 || vk == 0xa1)
        {
            vk = VK_SHIFT;
        }
        else if (vk == 0xa2 || vk == 0xa3)
        {
            vk = VK_CONTROL;
        }
        else if (vk == 0xa4 || vk == 0xa5)
        {
            vk = VK_MENU;
        }
        post(hwnd, message, vk, lparam);
        break;
    }
    case SDL_MOUSEBUTTONDOWN:
        if (event.button.button == SDL_BUTTON_LEFT)
        {
            post(hwnd, WM_LBUTTONDOWN, 1, MAKELONG(event.button.x, event.button.y));
        }
        break;
    case SDL_MOUSEBUTTONUP:
        if (event.button.button == SDL_BUTTON_LEFT)
        {
            post(hwnd, WM_LBUTTONUP, 0, MAKELONG(event.button.x, event.button.y));
        }
        break;
    }
}

// Moves SDL's pending events into the main thread's queue.
void pump_events()
{
    if (GetCurrentThreadId() != port_main_thread_id() || !SDL_WasInit(SDL_INIT_EVENTS))
    {
        return;
    }
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        translate_event(event);
    }
}

LRESULT call_window(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    if (PortWindow *window = window_of(hwnd))
    {
        return window->proc(hwnd, message, wparam, lparam);
    }
    if (PortDialog *dialog = dialog_of(hwnd))
    {
        return dialog->proc(hwnd, message, wparam, lparam);
    }
    if (PortDialogItem *item = item_of(hwnd))
    {
        switch (message)
        {
        case BM_GETCHECK:
            return item->check;
        case BM_SETCHECK:
            item->check = (UINT)wparam;
            return 0;
        }
    }
    return 0;
}

SDL_Window *create_sdl_window(const char *title, int x, int y, int width, int height, bool full_screen)
{
    if (!SDL_WasInit(SDL_INIT_VIDEO))
    {
        return NULL;
    }
    Uint32 renderer_flags = port_gl_window_flags();
    Uint32 flags = SDL_WINDOW_SHOWN | (full_screen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
    if (x == CW_USEDEFAULT || !position_on_screen(x, y, width, height))
    {
        x = SDL_WINDOWPOS_CENTERED;
        y = SDL_WINDOWPOS_CENTERED;
    }
    port_gl_prepare_window();
    SDL_Window *sdl = SDL_CreateWindow(title, x, y, width, height, flags | renderer_flags);
    if (sdl != NULL)
    {
        port_gl_attach_window(sdl);
        return sdl;
    }
    if (renderer_flags != 0)
    {
        port_log("SDL_CreateWindow with the renderer's flags 0x%x failed (%s); trying without", renderer_flags,
                 SDL_GetError());
        sdl = SDL_CreateWindow(title, x, y, width, height, flags);
    }
    if (sdl == NULL)
    {
        port_log("SDL_CreateWindow failed: %s; running without a window", SDL_GetError());
    }
    return sdl;
}

} // namespace

// Defaults for builds without the OpenGL renderer (see port_platform.h);
// its definitions replace these at link time.
__attribute__((weak)) uint32_t port_gl_window_flags()
{
    return 0;
}

__attribute__((weak)) void port_gl_prepare_window()
{
}

__attribute__((weak)) void port_gl_attach_window(SDL_Window *window)
{
}

__attribute__((weak)) void port_gl_detach_window(SDL_Window *window)
{
}

SDL_Window *port_sdl_window(HWND hwnd)
{
    PortWindow *window = hwnd == NULL ? g_game_window : window_of(hwnd);
    return window != NULL ? window->sdl : NULL;
}

void port_window_drawable_size(int *width, int *height)
{
    *width = 0;
    *height = 0;
    SDL_Window *sdl = port_sdl_window();
    if (sdl == NULL)
    {
        return;
    }
    if (SDL_GetWindowFlags(sdl) & SDL_WINDOW_OPENGL)
    {
        SDL_GL_GetDrawableSize(sdl, width, height);
    }
    else
    {
        SDL_GetWindowSize(sdl, width, height);
    }
}

static int display_index()
{
    SDL_Window *sdl = port_sdl_window();
    int index = sdl != NULL ? SDL_GetWindowDisplayIndex(sdl) : 0;
    return index < 0 ? 0 : index;
}

int port_display_refresh_rate(void)
{
    SDL_DisplayMode mode;
    if (SDL_WasInit(SDL_INIT_VIDEO) && SDL_GetCurrentDisplayMode(display_index(), &mode) == 0 &&
        mode.refresh_rate > 0)
    {
        return mode.refresh_rate;
    }
    return 60;
}

void port_display_size(int *width, int *height)
{
    SDL_DisplayMode mode;
    if (SDL_WasInit(SDL_INIT_VIDEO) && SDL_GetDesktopDisplayMode(display_index(), &mode) == 0)
    {
        *width = mode.w;
        *height = mode.h;
        return;
    }
    *width = 640;
    *height = 480;
}

extern "C" {

ATOM RegisterClassA(const WNDCLASSA *lpWndClass)
{
    static ATOM next_atom = 0xc000;
    g_classes[lpWndClass->lpszClassName] = lpWndClass->lpfnWndProc;
    return next_atom++;
}

BOOL UnregisterClassA(LPCSTR lpClassName, HINSTANCE hInstance)
{
    return g_classes.erase(lpClassName) != 0;
}

HWND CreateWindowExA(DWORD dwExStyle, LPCSTR lpClassName, LPCSTR lpWindowName, DWORD dwStyle, int X, int Y,
                     int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam)
{
    auto window_class = g_classes.find(lpClassName);
    if (window_class == g_classes.end())
    {
        SetLastError(1407); // ERROR_CANNOT_FIND_WND_CLASS
        return NULL;
    }
    if (g_game_window != NULL)
    {
        port_log("CreateWindowExA: only one window is supported");
        return NULL;
    }
    PortWindow *window = new PortWindow();
    window->kind = HWND_KIND_WINDOW;
    window->proc = window_class->second;
    window->style = dwStyle;
    window->ex_style = dwExStyle;
    window->x = X;
    window->y = Y;
    window->width = nWidth;
    window->height = nHeight;
    std::string title = port_sjis_to_utf8(lpWindowName != NULL ? lpWindowName : "");
#ifdef TH16_THCRAP
    port_thcrap_window_title(lpWindowName, &title);
#endif
    bool full_screen = (dwStyle & WS_POPUP) != 0;
    window->sdl = create_sdl_window(title.c_str(), X, Y, nWidth, nHeight, full_screen);
    if (window->sdl != NULL)
    {
        SDL_GetWindowPosition(window->sdl, &window->x, &window->y);
        // No text input (and no IME) while playing.
        SDL_StopTextInput();
        port_log("window: %dx%d %s (%s)", nWidth, nHeight, full_screen ? "full screen" : "windowed",
                 SDL_GetCurrentVideoDriver());
    }
    g_hwnds.insert(window);
    g_game_window = window;
    HWND hwnd = (HWND)window;
    window->proc(hwnd, WM_CREATE, 0, 0);
    // The game starts active (create_game_window sets is_app_active).
    return hwnd;
}

BOOL DestroyWindow(HWND hWnd)
{
    if (PortWindow *window = window_of(hWnd))
    {
        window->proc(hWnd, WM_DESTROY, 0, 0);
        if (window->sdl != NULL)
        {
            port_gl_detach_window(window->sdl);
            SDL_DestroyWindow(window->sdl);
        }
        g_hwnds.erase(window);
        if (g_game_window == window)
        {
            g_game_window = NULL;
        }
        delete window;
        return TRUE;
    }
    if (PortDialog *dialog = dialog_of(hWnd))
    {
        for (auto &item : dialog->items)
        {
            g_hwnds.erase(&item.second);
        }
        g_hwnds.erase(dialog);
        delete dialog;
        return TRUE;
    }
    SetLastError(1400); // ERROR_INVALID_WINDOW_HANDLE
    return FALSE;
}

BOOL ShowWindow(HWND hWnd, int nCmdShow)
{
    PortWindow *window = window_of(hWnd);
    if (window == NULL || window->sdl == NULL)
    {
        return FALSE;
    }
    bool was_visible = (SDL_GetWindowFlags(window->sdl) & SDL_WINDOW_SHOWN) != 0;
    switch (nCmdShow)
    {
    case SW_HIDE:
        SDL_HideWindow(window->sdl);
        break;
    case SW_MINIMIZE:
    case SW_SHOWMINIMIZED:
        SDL_MinimizeWindow(window->sdl);
        break;
    case SW_SHOWMAXIMIZED:
        SDL_MaximizeWindow(window->sdl);
        break;
    default:
        SDL_ShowWindow(window->sdl);
        break;
    }
    return was_visible;
}

BOOL UpdateWindow(HWND hWnd)
{
    return window_of(hWnd) != NULL;
}

// The game moves its window to 0, 0, 0 x 0 before destroying it; sizes of 0
// are ignored.
BOOL MoveWindow(HWND hWnd, int X, int Y, int nWidth, int nHeight, BOOL bRepaint)
{
    PortWindow *window = window_of(hWnd);
    if (window == NULL)
    {
        return FALSE;
    }
    if (nWidth <= 0 || nHeight <= 0)
    {
        return TRUE;
    }
    window->x = X;
    window->y = Y;
    window->width = nWidth;
    window->height = nHeight;
    apply_window_mode(window);
    return TRUE;
}

BOOL SetWindowPos(HWND hWnd, HWND hWndInsertAfter, int X, int Y, int cx, int cy, UINT uFlags)
{
    PortWindow *window = window_of(hWnd);
    if (window == NULL)
    {
        return FALSE;
    }
    if (!(uFlags & SWP_NOMOVE) && !(window->style & WS_POPUP))
    {
        window->x = X;
        window->y = Y;
    }
    if (!(uFlags & SWP_NOSIZE) && !(window->style & WS_POPUP) && cx > 0 && cy > 0)
    {
        window->width = cx;
        window->height = cy;
    }
    if (!(uFlags & (SWP_NOMOVE | SWP_NOSIZE)) || (uFlags & SWP_FRAMECHANGED))
    {
        apply_window_mode(window);
    }
    if (window->sdl != NULL && (uFlags & SWP_SHOWWINDOW))
    {
        SDL_ShowWindow(window->sdl);
    }
    if (window->sdl != NULL && (uFlags & SWP_HIDEWINDOW))
    {
        SDL_HideWindow(window->sdl);
    }
    return TRUE;
}

// GWL_STYLE takes effect with the next SetWindowPos (SWP_FRAMECHANGED), as
// on Windows.
LONG SetWindowLongA(HWND hWnd, int nIndex, LONG dwNewLong)
{
    PortWindow *window = window_of(hWnd);
    if (window == NULL)
    {
        return 0;
    }
    LONG old = 0;
    if (nIndex == GWL_STYLE)
    {
        old = (LONG)window->style;
        window->style = (DWORD)dwNewLong;
    }
    else if (nIndex == GWL_EXSTYLE)
    {
        old = (LONG)window->ex_style;
        window->ex_style = (DWORD)dwNewLong;
    }
    return old;
}

LONG GetWindowLongA(HWND hWnd, int nIndex)
{
    PortWindow *window = window_of(hWnd);
    if (window == NULL)
    {
        return 0;
    }
    return nIndex == GWL_STYLE ? (LONG)window->style : nIndex == GWL_EXSTYLE ? (LONG)window->ex_style : 0;
}

BOOL GetWindowRect(HWND hWnd, LPRECT lpRect)
{
    PortWindow *window = window_of(hWnd);
    if (window == NULL)
    {
        memset(lpRect, 0, sizeof(*lpRect));
        SetLastError(1400);
        return FALSE;
    }
    if (window->style & WS_POPUP)
    {
        int width, height;
        port_display_size(&width, &height);
        lpRect->left = 0;
        lpRect->top = 0;
        lpRect->right = width;
        lpRect->bottom = height;
        return TRUE;
    }
    lpRect->left = window->x;
    lpRect->top = window->y;
    lpRect->right = window->x + window->width;
    lpRect->bottom = window->y + window->height;
    return TRUE;
}

BOOL GetClientRect(HWND hWnd, LPRECT lpRect)
{
    PortWindow *window = window_of(hWnd);
    memset(lpRect, 0, sizeof(*lpRect));
    if (window == NULL)
    {
        return FALSE;
    }
    int width = window->width;
    int height = window->height;
    if (window->sdl != NULL)
    {
        SDL_GetWindowSize(window->sdl, &width, &height);
    }
    lpRect->right = width;
    lpRect->bottom = height;
    return TRUE;
}

// Windows have no frame here.
BOOL AdjustWindowRect(LPRECT lpRect, DWORD dwStyle, BOOL bMenu)
{
    return TRUE;
}

BOOL SetForegroundWindow(HWND hWnd)
{
    PortWindow *window = window_of(hWnd);
    if (window == NULL)
    {
        return FALSE;
    }
    if (window->sdl != NULL)
    {
        SDL_RaiseWindow(window->sdl);
    }
    return TRUE;
}

HWND GetForegroundWindow(void)
{
    if (g_game_window != NULL && g_game_window->sdl != NULL &&
        (SDL_GetWindowFlags(g_game_window->sdl) & SDL_WINDOW_INPUT_FOCUS))
    {
        return (HWND)g_game_window;
    }
    return NULL;
}

HWND SetFocus(HWND hWnd)
{
    HWND previous = GetForegroundWindow();
    SetForegroundWindow(hWnd);
    return previous;
}

BOOL SetWindowTextA(HWND hWnd, LPCSTR lpString)
{
    PortWindow *window = window_of(hWnd);
    if (window == NULL)
    {
        return FALSE;
    }
    if (window->sdl != NULL)
    {
        SDL_SetWindowTitle(window->sdl, port_sjis_to_utf8(lpString).c_str());
    }
    return TRUE;
}

// What Windows does for the messages the game passes on: closing goes
// through WM_CLOSE; minimising and restoring from WM_SYSCOMMAND are left out
// (the game minimises and restores its new window once to bring it to the
// front, which SDL_RaiseWindow does without the flicker).
LRESULT DefWindowProcA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    switch (Msg)
    {
    case WM_SYSCOMMAND:
        switch (wParam & 0xfff0)
        {
        case SC_CLOSE:
            post(hWnd, WM_CLOSE, 0, 0);
            return 0;
        case SC_RESTORE:
            SetForegroundWindow(hWnd);
            return 0;
        }
        return 0;
    case WM_CLOSE:
        DestroyWindow(hWnd);
        return 0;
    case WM_SETCURSOR:
        return 0;
    }
    return 0;
}

LRESULT SendMessageA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return call_window(hWnd, Msg, wParam, lParam);
}

BOOL PostMessageA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (hWnd != NULL && from_hwnd(hWnd) == NULL)
    {
        SetLastError(1400);
        return FALSE;
    }
    if (hWnd == NULL)
    {
        MSG msg = {};
        msg.message = Msg;
        msg.wParam = wParam;
        msg.lParam = lParam;
        return port_post_thread_message(GetCurrentThreadId(), msg);
    }
    post(hWnd, Msg, wParam, lParam);
    return TRUE;
}

BOOL PostThreadMessageA(DWORD idThread, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    MSG msg = {};
    msg.message = Msg;
    msg.wParam = wParam;
    msg.lParam = lParam;
    msg.time = GetTickCount();
    if (!port_post_thread_message(idThread, msg))
    {
        SetLastError(1444); // ERROR_INVALID_THREAD_ID
        return FALSE;
    }
    return TRUE;
}

void PostQuitMessage(int nExitCode)
{
    MSG msg = {};
    msg.message = WM_QUIT;
    msg.wParam = (WPARAM)nExitCode;
    port_post_thread_message(GetCurrentThreadId(), msg);
}

// Filters (window and message range) are not used by the game and are
// ignored.
BOOL PeekMessageA(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax, UINT wRemoveMsg)
{
    if (port_peek_thread_message(lpMsg, (wRemoveMsg & PM_REMOVE) != 0))
    {
        return TRUE;
    }
    pump_events();
    return port_peek_thread_message(lpMsg, (wRemoveMsg & PM_REMOVE) != 0);
}

BOOL GetMessageA(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax)
{
    for (;;)
    {
        if (PeekMessageA(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax, PM_REMOVE))
        {
            return lpMsg->message != WM_QUIT;
        }
        // SDL events do not wake the kernel's waits; look again soon.
        bool main_thread = GetCurrentThreadId() == port_main_thread_id();
        port_wait(0, NULL, FALSE, main_thread ? 5 : INFINITE, true);
    }
}

BOOL TranslateMessage(const MSG *lpMsg)
{
    return FALSE;
}

LRESULT DispatchMessageA(const MSG *lpMsg)
{
    if (lpMsg->message == WM_TIMER && lpMsg->lParam != 0)
    {
        ((TIMERPROC)lpMsg->lParam)(lpMsg->hwnd, WM_TIMER, lpMsg->wParam, GetTickCount());
        return 0;
    }
    return call_window(lpMsg->hwnd, lpMsg->message, lpMsg->wParam, lpMsg->lParam);
}

DWORD MsgWaitForMultipleObjects(DWORD nCount, const HANDLE *pHandles, BOOL fWaitAll, DWORD dwMilliseconds,
                                DWORD dwWakeMask)
{
    return port_wait(nCount, pHandles, fWaitAll, dwMilliseconds, dwWakeMask != 0);
}

// The sound code asks for a WM_TIMER every 250 ms, which the window
// procedure ignores; no messages are generated.
UINT_PTR SetTimer(HWND hWnd, UINT_PTR nIDEvent, UINT uElapse, TIMERPROC lpTimerFunc)
{
    return nIDEvent != 0 ? nIDEvent : 1;
}

BOOL KillTimer(HWND hWnd, UINT_PTR uIDEvent)
{
    return TRUE;
}

// The game shows its error log this way when it quits after a fatal error
// (Shift-JIS text). The text goes to stderr; a message box opens too unless
// there is no display (or TH16_NO_DIALOGS is set).
int MessageBoxA(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption, UINT uType)
{
    std::string caption = port_sjis_to_utf8(lpCaption != NULL ? lpCaption : "");
    std::string text = port_sjis_to_utf8(lpText != NULL ? lpText : "");
    std::string clean;
    for (char c : text)
    {
        if (c != '\r')
        {
            clean.push_back(c);
        }
    }
    port_log("message box \"%s\":\n%s", caption.c_str(), clean.c_str());
    if (!headless() && getenv("TH16_NO_DIALOGS") == NULL)
    {
        Uint32 flags = (uType & 0xf0) == MB_ICONHAND           ? SDL_MESSAGEBOX_ERROR
                       : (uType & 0xf0) == MB_ICONEXCLAMATION ? SDL_MESSAGEBOX_WARNING
                                                              : SDL_MESSAGEBOX_INFORMATION;
        SDL_ShowSimpleMessageBox(flags, caption.c_str(), clean.c_str(), port_sdl_window());
    }
    return (uType & 0xf) == MB_YESNO ? IDYES : IDOK;
}

// ---------------------------------------------------------------------------
// Dialogs. There are no dialog resources: a dialog gets WM_INITDIALOG (so
// its controls are set from the configuration, as the game does) and is
// then answered at once. The only dialog is the resolution dialog (0xcb),
// which WinMain shows when the configuration asks for it or Shift is held;
// it is answered with its start button (0xd0), which keeps the
// configuration's choices.

HWND CreateDialogParamA(HINSTANCE hInstance, LPCSTR lpTemplateName, HWND hWndParent, DLGPROC lpDialogFunc,
                        LPARAM dwInitParam)
{
    PortDialog *dialog = new PortDialog();
    dialog->kind = HWND_KIND_DIALOG;
    dialog->proc = lpDialogFunc;
    g_hwnds.insert(dialog);
    HWND hwnd = (HWND)dialog;
    lpDialogFunc(hwnd, WM_INITDIALOG, 0, dwInitParam);
    if (lpTemplateName == MAKEINTRESOURCEA(0xcb))
    {
        post(hwnd, WM_COMMAND, 0xd0, 0);
    }
    else
    {
        post(hwnd, WM_CLOSE, 0, 0);
    }
    port_log("dialog %u: no dialog resources; keeping the saved configuration",
             (unsigned)(uintptr_t)lpTemplateName);
    return hwnd;
}

INT_PTR DialogBoxParamA(HINSTANCE hInstance, LPCSTR lpTemplateName, HWND hWndParent, DLGPROC lpDialogFunc,
                        LPARAM dwInitParam)
{
    port_log("DialogBoxParamA: no dialog resources");
    return -1;
}

BOOL EndDialog(HWND hDlg, INT_PTR nResult)
{
    return DestroyWindow(hDlg);
}

BOOL IsDialogMessageA(HWND hDlg, LPMSG lpMsg)
{
    if (hDlg == NULL || lpMsg->hwnd != hDlg || dialog_of(hDlg) == NULL)
    {
        return FALSE;
    }
    call_window(hDlg, lpMsg->message, lpMsg->wParam, lpMsg->lParam);
    return TRUE;
}

HWND GetDlgItem(HWND hDlg, int nIDDlgItem)
{
    PortDialog *dialog = dialog_of(hDlg);
    if (dialog == NULL)
    {
        return NULL;
    }
    PortDialogItem &item = dialog->items[nIDDlgItem];
    if (item.dialog == NULL)
    {
        item.kind = HWND_KIND_DIALOG_ITEM;
        item.dialog = dialog;
        item.id = nIDDlgItem;
        item.check = BST_UNCHECKED;
        g_hwnds.insert(&item);
    }
    return (HWND)&item;
}

UINT IsDlgButtonChecked(HWND hDlg, int nIDButton)
{
    HWND item = GetDlgItem(hDlg, nIDButton);
    return item != NULL ? item_of(item)->check : BST_UNCHECKED;
}

BOOL CheckDlgButton(HWND hDlg, int nIDButton, UINT uCheck)
{
    HWND item = GetDlgItem(hDlg, nIDButton);
    if (item == NULL)
    {
        return FALSE;
    }
    item_of(item)->check = uCheck;
    return TRUE;
}

LRESULT SendDlgItemMessageA(HWND hDlg, int nIDDlgItem, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return SendMessageA(GetDlgItem(hDlg, nIDDlgItem), Msg, wParam, lParam);
}

// ---------------------------------------------------------------------------
// System settings, cursor, keyboard

int GetSystemMetrics(int nIndex)
{
    int width, height;
    switch (nIndex)
    {
    case SM_CXSCREEN:
        port_display_size(&width, &height);
        return width;
    case SM_CYSCREEN:
        port_display_size(&width, &height);
        return height;
    }
    // Frames and captions: none.
    return 0;
}

// The screen saver and power saving settings, read and switched off at
// startup and restored at exit: the screen saver is held off while the
// game runs.
BOOL SystemParametersInfoA(UINT uiAction, UINT uiParam, PVOID pvParam, UINT fWinIni)
{
    switch (uiAction)
    {
    case SPI_GETSCREENSAVEACTIVE:
    case SPI_GETLOWPOWERACTIVE:
    case SPI_GETPOWEROFFACTIVE:
        if (pvParam != NULL)
        {
            *(BOOL *)pvParam = uiAction == SPI_GETSCREENSAVEACTIVE && SDL_WasInit(SDL_INIT_VIDEO)
                                   ? SDL_IsScreenSaverEnabled()
                                   : TRUE;
        }
        return TRUE;
    case SPI_SETSCREENSAVEACTIVE:
        if (SDL_WasInit(SDL_INIT_VIDEO))
        {
            if (uiParam)
            {
                SDL_EnableScreenSaver();
            }
            else
            {
                SDL_DisableScreenSaver();
            }
        }
        return TRUE;
    }
    return TRUE;
}

int ShowCursor(BOOL bShow)
{
    g_cursor_count += bShow ? 1 : -1;
    update_cursor();
    return g_cursor_count;
}

static HCURSOR arrow_cursor()
{
    static struct HICON__ arrow;
    return &arrow;
}

HCURSOR SetCursor(HCURSOR hCursor)
{
    HCURSOR previous = g_cursor_set ? arrow_cursor() : NULL;
    g_cursor_set = hCursor != NULL;
    update_cursor();
    return previous;
}

HCURSOR LoadCursorA(HINSTANCE hInstance, LPCSTR lpCursorName)
{
    return arrow_cursor();
}

HICON LoadIconA(HINSTANCE hInstance, LPCSTR lpIconName)
{
    return arrow_cursor();
}

// 256 virtual-key states, bit 7 set while held. The game's keyboard input
// when DirectInput is off, and its Shift check at startup.
BOOL GetKeyboardState(PBYTE lpKeyState)
{
    port_input_get_vk_state(lpKeyState);
    return TRUE;
}

// The game clears every key's held bit at startup; the state here comes
// from the keyboard and needs no clearing.
BOOL SetKeyboardState(LPBYTE lpKeyState)
{
    return TRUE;
}

short GetAsyncKeyState(int vKey)
{
    BYTE keys[256];
    port_input_get_vk_state(keys);
    return (keys[vKey & 0xff] & 0x80) ? (short)0x8000 : 0;
}

short GetKeyState(int nVirtKey)
{
    return GetAsyncKeyState(nVirtKey);
}

} // extern "C"
