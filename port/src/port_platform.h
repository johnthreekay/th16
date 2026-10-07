// What the platform layer (window, messages, input, files, threads, text:
// win32_*.cpp, input_sdl.cpp, dinput_sdl.cpp, gdi_text.cpp, port_vfs.cpp)
// offers the rest of the port: the renderer (d3d9_*.cpp) and the sound code
// (dsound_*.cpp). Everything else they need from the operating system goes
// through the Win32 calls the game itself uses (CreateFileA, CreateEventA,
// SetEvent, ...), which work in the port as documented by Microsoft.
//
// SDL is initialised by main() (video, events, game controllers) before
// WinMain runs, and shut down after it returns. A subsystem the platform
// layer does not start (audio) is started with SDL_InitSubSystem by the code
// that needs it and stopped with SDL_QuitSubSystem.
#pragma once

#include <stdint.h>

#include <string>

#include <windows.h>

struct SDL_Window;

// ---------------------------------------------------------------------------
// The game window (win32_user.cpp)

// The SDL window behind a window handle from CreateWindowExA; NULL for the
// game window when it does not exist (yet), and for other handles. With
// hwnd == NULL: the game window. NULL also when the port runs without a
// display (SDL video failed to start; the game then runs windowless).
SDL_Window *port_sdl_window(HWND hwnd = NULL);

// The renderer's side of the window (d3d9_gl.h in the OpenGL renderer;
// same declarations): CreateWindowExA calls port_gl_prepare_window (SDL_GL_*
// attributes) and creates the SDL window with port_gl_window_flags(), then
// hands it over with port_gl_attach_window; DestroyWindow calls
// port_gl_detach_window first. If creation fails with the renderer's flags,
// the window is made without them and not attached. win32_user.cpp has weak
// defaults (no flags, nothing to do) for builds without that renderer (the
// stubs, the null renderer).
uint32_t port_gl_window_flags();
void port_gl_prepare_window();
void port_gl_attach_window(SDL_Window *window);
void port_gl_detach_window(SDL_Window *window);

// The size of the window's drawable area in pixels (what the back buffer is
// scaled into when presenting; it differs from the back buffer size in
// full screen, which is SDL_WINDOW_FULLSCREEN_DESKTOP, and on high-DPI
// displays). 0 x 0 without a window.
void port_window_drawable_size(int *width, int *height);

// The refresh rate of the display the game window is on (or of the main
// display before it exists); 60 when SDL does not know. GetDeviceCaps
// (VREFRESH) reports this, and IDirect3D9::GetAdapterDisplayMode should
// too.
int port_display_refresh_rate(void);

// The desktop size of that display (GetSystemMetrics(SM_CXSCREEN) and
// SM_CYSCREEN; also right for GetAdapterDisplayMode). 640 x 480 without one.
void port_display_size(int *width, int *height);

// ---------------------------------------------------------------------------
// Files (port_vfs.cpp). The game sees a Windows file system: it runs from
// "C:\th16\th16.exe" and saves under "C:\AppData\ShanghaiAlice\th16\".
// Both directories are the same place on the host: the save folder layered
// over the game folder. Reads look in the save folder first, then in the
// game folder; writes, new directories and deletions only ever touch the
// save folder.

// Host path of the game folder (th16.dat, thbgm.dat), without a trailing
// slash.
const char *port_game_dir(void);
// Host path of the save folder (th16.cfg, scoreth16.dat, replay/,
// snapshot/, log.txt), without a trailing slash.
const char *port_save_dir(void);
// The host file an existing file name the game uses refers to (relative
// names are taken from the current directory set with _chdir), or "" when
// there is none.
std::string port_host_path(const char *windows_path);

// ---------------------------------------------------------------------------
// Text (port_text.cpp)

// Shift-JIS (code page 932, the game's encoding for every string) to UTF-8.
// Invalid bytes become U+FFFD. length < 0: up to the terminator.
std::string port_sjis_to_utf8(const char *text, int length = -1);
// UTF-8 to Shift-JIS; false when some character has no Shift-JIS form.
bool port_utf8_to_sjis(const char *text, std::string *out);

// The GDI text renderer (gdi_text.cpp), for thcrap's text layout
// (port/src/thcrap/text.cpp), which TextOutA goes through when a patch
// stack is loaded:
extern "C" {
// TextOutA without the layout: one run of text in the DC's font.
BOOL port_gdi_text_out_raw(HDC hdc, int x, int y, const char *text, int length);
// The width in pixels of a run in the DC's font.
int port_gdi_text_width(HDC hdc, const char *text, int length);
// The width of the DIB section selected into the DC (0 without one).
int port_gdi_bitmap_width(HDC hdc);
// The font selected into the DC, and a font's LOGFONT (GetObject).
HFONT port_gdi_current_font(HDC hdc);
bool port_gdi_font_logfont(HFONT font, LOGFONTA *lf);
// Take strings as UTF-8 where they are valid UTF-8 (else Shift-JIS), as
// thcrap's win32_utf8 does; real fonts then also keep their own
// (proportional) advances. Off by default.
void port_gdi_set_utf8(bool on);
// Makes a font file's faces available by family name (patch.js "fonts").
bool port_gdi_add_font_file(const char *path);
}

// ---------------------------------------------------------------------------
// Logging

// Writes "[th16-port] <message>" to stderr (the message in UTF-8).
void port_log(const char *format, ...) __attribute__((format(printf, 1, 2)));
