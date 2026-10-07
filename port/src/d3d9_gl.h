// The renderer's interface to the window code (port/src/win32_user*.cpp).
//
// The Direct3D 9 implementation in d3d9_gl.cpp draws with OpenGL 3.3 (core
// profile) through SDL2. It does not own the game window: the window code
// creates it (with the flags and attributes below) and hands it over with
// port_gl_attach_window before the game calls CreateDevice. If nothing is
// attached when the device is created, the renderer opens a plain window
// of its own (port_gl_window returns it), so the game still runs while the
// window code is a stub.
//
// The renderer only ever touches GL from the thread that created the device
// (the game's main thread). Window events, input and the message loop are
// the window code's business; the renderer never pumps SDL events.
#pragma once

#include <stdint.h>

struct SDL_Window;

// SDL_CreateWindow flags the renderer needs (SDL_WINDOW_OPENGL).
uint32_t port_gl_window_flags();

// Sets the SDL_GL_* attributes (core profile 3.3, double buffering).
// Call after SDL_Init(SDL_INIT_VIDEO) and before SDL_CreateWindow.
void port_gl_prepare_window();

// Hands the game window to the renderer. Call before CreateDevice; calling
// it again with another window moves the renderer to that window.
void port_gl_attach_window(SDL_Window *window);

// Call before destroying an attached window. The device keeps its GL
// context, but stops presenting until a window is attached again.
void port_gl_detach_window(SDL_Window *window);

// The window the renderer presents to: the attached one, or its own.
SDL_Window *port_gl_window();

// Maps a position in window pixels (SDL mouse coordinates) to back buffer
// pixels, taking the letterboxing of Present into account. Returns false
// when there is no device or the point is outside the picture.
bool port_gl_window_to_back_buffer(int x, int y, int *out_x, int *out_y);
