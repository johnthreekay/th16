// What d3d9_gl.cpp shares with d3dx9_tex.cpp: pixel format conversion and
// access to texture and back buffer pixels in a format-neutral way.
//
// Pixels travel between the two as 32-bit D3DCOLOR values (0xAARRGGBB).
#pragma once

#include <stdint.h>

#include <vector>

#include <d3d9.h>

// Bytes per pixel of a format the renderer stores, 0 if it does not
// support the format.
uint32_t port_d3d_format_bpp(D3DFORMAT format);

// Converts count pixels of format to D3DCOLOR (missing alpha reads as 0xff,
// missing color as 0, as D3D samples them) and back (dropping what the
// format cannot hold).
void port_d3d_decode_pixels(D3DFORMAT format, const uint8_t *src, uint32_t *dst, uint32_t count);
void port_d3d_encode_pixels(D3DFORMAT format, const uint32_t *src, uint8_t *dst, uint32_t count);

// Creates a texture with the device's CreateTexture semantics (one level).
HRESULT port_d3d_create_texture(IDirect3DDevice9 *device, UINT width, UINT height, DWORD usage, D3DFORMAT format,
                                D3DPOOL pool, IDirect3DTexture9 **texture);

// Reads the pixels of rect (whole surface if NULL) as D3DCOLOR, rows top
// to bottom. Render targets and the back buffer are read back from GL,
// which only works on the device's thread.
HRESULT port_d3d_read_surface(IDirect3DSurface9 *surface, const RECT *rect, std::vector<uint32_t> &pixels,
                              uint32_t *width, uint32_t *height);

// Writes width x height D3DCOLOR pixels into rect (whole surface if NULL)
// of the surface; rect must be width x height.
HRESULT port_d3d_write_surface(IDirect3DSurface9 *surface, const RECT *rect, const uint32_t *pixels, uint32_t width,
                               uint32_t height);
