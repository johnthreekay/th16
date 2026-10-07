// The D3DX texture helpers, over the renderer's textures (d3d9_gl.cpp).
//
// AnmLoader.cpp creates ANM textures from the raw pixel data in .anm files
// (D3DXCreateTexture + D3DXLoadSurfaceFromMemory) or from image files in
// memory (D3DXCreateTextureFromFileInMemoryEx, D3DXLoadSurfaceFromFileInMemory),
// TextHelper.cpp uploads rendered text with D3DXLoadSurfaceFromMemory, and
// the screen copies (pause menu, ANM screen captures) copy from the back
// buffer or a render target with D3DXLoadSurfaceFromSurface.
//
// Everything goes through 32-bit D3DCOLOR pixels: decode the source,
// resample it to the destination rectangle following the filter, apply the
// color key, encode into the destination's format.
#include <stdio.h>
#include <string.h>

#include <vector>

#include <d3dx9tex.h>

#include "d3d9_gl_internal.h"
#include "port_stub.h"

namespace
{

uint32_t round_up_pow2(uint32_t v)
{
    uint32_t p = 1;
    while (p < v)
    {
        p <<= 1;
    }
    return p;
}

// Resamples src (sw x sh) to dw x dh. D3DX_FILTER_NONE copies without
// scaling (the part outside the source is transparent black);
// D3DX_FILTER_POINT takes the nearest pixel; the other filters average the
// covered area when shrinking and interpolate when growing.
void resample(const std::vector<uint32_t> &src, uint32_t sw, uint32_t sh, std::vector<uint32_t> &dst, uint32_t dw,
              uint32_t dh, DWORD filter)
{
    dst.assign((size_t)dw * dh, 0);
    DWORD kind = filter == D3DX_DEFAULT ? D3DX_FILTER_TRIANGLE : filter & 0xff;
    if (sw == dw && sh == dh)
    {
        dst = src;
        return;
    }
    if (kind == D3DX_FILTER_NONE || sw == 0 || sh == 0)
    {
        uint32_t w = sw < dw ? sw : dw;
        uint32_t h = sh < dh ? sh : dh;
        for (uint32_t y = 0; y < h; y++)
        {
            memcpy(&dst[(size_t)y * dw], &src[(size_t)y * sw], w * 4);
        }
        return;
    }
    if (kind == D3DX_FILTER_POINT)
    {
        for (uint32_t y = 0; y < dh; y++)
        {
            uint32_t sy = (uint32_t)(((uint64_t)y * 2 + 1) * sh / (2 * (uint64_t)dh));
            for (uint32_t x = 0; x < dw; x++)
            {
                uint32_t sx = (uint32_t)(((uint64_t)x * 2 + 1) * sw / (2 * (uint64_t)dw));
                dst[(size_t)y * dw + x] = src[(size_t)sy * sw + sx];
            }
        }
        return;
    }
    if (sw >= dw && sh >= dh)
    {
        // Area average: each destination pixel covers sw/dw x sh/dh source
        // pixels, with partial coverage at the edges.
        double fx = (double)sw / dw, fy = (double)sh / dh;
        for (uint32_t y = 0; y < dh; y++)
        {
            double y0 = y * fy, y1 = (y + 1) * fy;
            for (uint32_t x = 0; x < dw; x++)
            {
                double x0 = x * fx, x1 = (x + 1) * fx;
                double acc[4] = {0, 0, 0, 0}, total = 0;
                for (uint32_t sy = (uint32_t)y0; sy < sh && sy < y1; sy++)
                {
                    double wy = (sy + 1 < y1 ? sy + 1 : y1) - (sy > y0 ? sy : y0);
                    for (uint32_t sx = (uint32_t)x0; sx < sw && sx < x1; sx++)
                    {
                        double wx = (sx + 1 < x1 ? sx + 1 : x1) - (sx > x0 ? sx : x0);
                        double wgt = wx * wy;
                        uint32_t c = src[(size_t)sy * sw + sx];
                        acc[0] += (c >> 24) * wgt;
                        acc[1] += ((c >> 16) & 0xff) * wgt;
                        acc[2] += ((c >> 8) & 0xff) * wgt;
                        acc[3] += (c & 0xff) * wgt;
                        total += wgt;
                    }
                }
                uint32_t out = 0;
                if (total > 0)
                {
                    for (int i = 0; i < 4; i++)
                    {
                        uint32_t v = (uint32_t)(acc[i] / total + 0.5);
                        out = (out << 8) | (v > 255 ? 255 : v);
                    }
                }
                dst[(size_t)y * dw + x] = out;
            }
        }
        return;
    }
    // Bilinear, sampling at pixel centers, clamped at the edges.
    for (uint32_t y = 0; y < dh; y++)
    {
        double syf = (y + 0.5) * sh / dh - 0.5;
        syf = syf < 0 ? 0 : syf;
        uint32_t sy0 = (uint32_t)syf;
        uint32_t sy1 = sy0 + 1 < sh ? sy0 + 1 : sy0;
        double ty = syf - sy0;
        for (uint32_t x = 0; x < dw; x++)
        {
            double sxf = (x + 0.5) * sw / dw - 0.5;
            sxf = sxf < 0 ? 0 : sxf;
            uint32_t sx0 = (uint32_t)sxf;
            uint32_t sx1 = sx0 + 1 < sw ? sx0 + 1 : sx0;
            double tx = sxf - sx0;
            uint32_t c00 = src[(size_t)sy0 * sw + sx0], c10 = src[(size_t)sy0 * sw + sx1];
            uint32_t c01 = src[(size_t)sy1 * sw + sx0], c11 = src[(size_t)sy1 * sw + sx1];
            uint32_t out = 0;
            for (int shift = 24; shift >= 0; shift -= 8)
            {
                double top = ((c00 >> shift) & 0xff) * (1 - tx) + ((c10 >> shift) & 0xff) * tx;
                double bottom = ((c01 >> shift) & 0xff) * (1 - tx) + ((c11 >> shift) & 0xff) * tx;
                uint32_t v = (uint32_t)(top * (1 - ty) + bottom * ty + 0.5);
                out |= (v > 255 ? 255 : v) << shift;
            }
            dst[(size_t)y * dw + x] = out;
        }
    }
}

// Pixels equal to the color key (compared as A8R8G8B8) become transparent
// black.
void apply_color_key(std::vector<uint32_t> &pixels, D3DCOLOR key)
{
    if (key == 0)
    {
        return;
    }
    for (uint32_t &p : pixels)
    {
        if (p == key)
        {
            p = 0;
        }
    }
}

// Decodes an image file in memory. No container format turned up in
// th16.dat (see NOTES.md); report what arrives so a decoder can be added.
bool decode_image(const void *data, UINT size, std::vector<uint32_t> &pixels, D3DXIMAGE_INFO *info)
{
    const uint8_t *p = (const uint8_t *)data;
    if (size >= 4)
    {
        fprintf(stderr, "[th16-port] D3DX: unsupported image file (%u bytes, starts %02x %02x %02x %02x)\n", size,
                p[0], p[1], p[2], p[3]);
    }
    return false;
}

HRESULT load_surface(IDirect3DSurface9 *dst_surface, const RECT *dst_rect, const std::vector<uint32_t> &src,
                     uint32_t sw, uint32_t sh, DWORD filter, D3DCOLOR color_key)
{
    D3DSURFACE_DESC desc;
    if (dst_surface == NULL || dst_surface->GetDesc(&desc) != D3D_OK)
    {
        return D3DERR_INVALIDCALL;
    }
    RECT d = dst_rect != NULL ? *dst_rect : RECT{0, 0, (LONG)desc.Width, (LONG)desc.Height};
    if (d.right <= d.left || d.bottom <= d.top)
    {
        return D3DERR_INVALIDCALL;
    }
    uint32_t dw = d.right - d.left;
    uint32_t dh = d.bottom - d.top;
    std::vector<uint32_t> scaled;
    resample(src, sw, sh, scaled, dw, dh, filter);
    apply_color_key(scaled, color_key);
    return port_d3d_write_surface(dst_surface, &d, scaled.data(), dw, dh);
}

} // namespace

extern "C" {

HRESULT D3DXCreateTexture(IDirect3DDevice9 *pDevice, UINT Width, UINT Height, UINT MipLevels, DWORD Usage,
                          D3DFORMAT Format, D3DPOOL Pool, IDirect3DTexture9 **ppTexture)
{
    if (ppTexture == NULL)
    {
        return D3DERR_INVALIDCALL;
    }
    // One level whatever MipLevels asks: the game draws without mipmaps.
    if (Width == 0 || Width == D3DX_DEFAULT || Width == D3DX_DEFAULT_NONPOW2)
    {
        Width = 256;
    }
    if (Height == 0 || Height == D3DX_DEFAULT || Height == D3DX_DEFAULT_NONPOW2)
    {
        Height = 256;
    }
    if (Format == D3DFMT_UNKNOWN || (!(Usage & D3DUSAGE_RENDERTARGET) && port_d3d_format_bpp(Format) == 0))
    {
        Format = D3DFMT_A8R8G8B8;
    }
    return port_d3d_create_texture(pDevice, Width, Height, Usage, Format, Pool, ppTexture);
}

HRESULT D3DXCreateTextureFromFileInMemoryEx(IDirect3DDevice9 *pDevice, LPCVOID pSrcData, UINT SrcDataSize,
                                            UINT Width, UINT Height, UINT MipLevels, DWORD Usage, D3DFORMAT Format,
                                            D3DPOOL Pool, DWORD Filter, DWORD MipFilter, D3DCOLOR ColorKey,
                                            D3DXIMAGE_INFO *pSrcInfo, PALETTEENTRY *pPalette,
                                            IDirect3DTexture9 **ppTexture)
{
    if (ppTexture == NULL || pSrcData == NULL)
    {
        return D3DERR_INVALIDCALL;
    }
    *ppTexture = NULL;
    std::vector<uint32_t> pixels;
    D3DXIMAGE_INFO info;
    if (!decode_image(pSrcData, SrcDataSize, pixels, &info))
    {
        return D3DERR_INVALIDCALL;
    }
    if (pSrcInfo != NULL)
    {
        *pSrcInfo = info;
    }
    // 0 and D3DX_DEFAULT round the file's size up to a power of two.
    UINT w = Width == 0 || Width == D3DX_DEFAULT ? round_up_pow2(info.Width)
             : Width == D3DX_DEFAULT_NONPOW2 || Width == D3DX_FROM_FILE ? info.Width
                                                                        : Width;
    UINT h = Height == 0 || Height == D3DX_DEFAULT ? round_up_pow2(info.Height)
             : Height == D3DX_DEFAULT_NONPOW2 || Height == D3DX_FROM_FILE ? info.Height
                                                                          : Height;
    D3DFORMAT format = Format == D3DFMT_UNKNOWN || Format == D3DFMT_FROM_FILE ? info.Format : Format;
    HRESULT hr = D3DXCreateTexture(pDevice, w, h, 1, Usage, format, Pool, ppTexture);
    if (hr != D3D_OK)
    {
        return hr;
    }
    IDirect3DSurface9 *surface = NULL;
    (*ppTexture)->GetSurfaceLevel(0, &surface);
    hr = load_surface(surface, NULL, pixels, info.Width, info.Height, Filter, ColorKey);
    surface->Release();
    if (hr != D3D_OK)
    {
        (*ppTexture)->Release();
        *ppTexture = NULL;
    }
    return hr;
}

HRESULT D3DXLoadSurfaceFromSurface(IDirect3DSurface9 *pDestSurface, const PALETTEENTRY *pDestPalette,
                                   const RECT *pDestRect, IDirect3DSurface9 *pSrcSurface,
                                   const PALETTEENTRY *pSrcPalette, const RECT *pSrcRect, DWORD Filter,
                                   D3DCOLOR ColorKey)
{
    std::vector<uint32_t> pixels;
    uint32_t w, h;
    HRESULT hr = port_d3d_read_surface(pSrcSurface, pSrcRect, pixels, &w, &h);
    if (hr != D3D_OK)
    {
        return hr;
    }
    return load_surface(pDestSurface, pDestRect, pixels, w, h, Filter, ColorKey);
}

HRESULT D3DXLoadSurfaceFromMemory(IDirect3DSurface9 *pDestSurface, const PALETTEENTRY *pDestPalette,
                                  const RECT *pDestRect, LPCVOID pSrcMemory, D3DFORMAT SrcFormat, UINT SrcPitch,
                                  const PALETTEENTRY *pSrcPalette, const RECT *pSrcRect, DWORD Filter,
                                  D3DCOLOR ColorKey)
{
    uint32_t bpp = port_d3d_format_bpp(SrcFormat);
    if (pSrcMemory == NULL || pSrcRect == NULL || bpp == 0)
    {
        return D3DERR_INVALIDCALL;
    }
    if (pSrcRect->right <= pSrcRect->left || pSrcRect->bottom <= pSrcRect->top)
    {
        return D3DERR_INVALIDCALL;
    }
    uint32_t w = pSrcRect->right - pSrcRect->left;
    uint32_t h = pSrcRect->bottom - pSrcRect->top;
    std::vector<uint32_t> pixels((size_t)w * h);
    const uint8_t *base = (const uint8_t *)pSrcMemory + (size_t)pSrcRect->top * SrcPitch + (size_t)pSrcRect->left * bpp;
    for (uint32_t y = 0; y < h; y++)
    {
        port_d3d_decode_pixels(SrcFormat, base + (size_t)y * SrcPitch, &pixels[(size_t)y * w], w);
    }
    return load_surface(pDestSurface, pDestRect, pixels, w, h, Filter, ColorKey);
}

HRESULT D3DXLoadSurfaceFromFileInMemory(IDirect3DSurface9 *pDestSurface, const PALETTEENTRY *pDestPalette,
                                        const RECT *pDestRect, LPCVOID pSrcData, UINT SrcDataSize,
                                        const RECT *pSrcRect, DWORD Filter, D3DCOLOR ColorKey,
                                        D3DXIMAGE_INFO *pSrcInfo)
{
    std::vector<uint32_t> pixels;
    D3DXIMAGE_INFO info;
    if (pSrcData == NULL || !decode_image(pSrcData, SrcDataSize, pixels, &info))
    {
        return D3DERR_INVALIDCALL;
    }
    if (pSrcInfo != NULL)
    {
        *pSrcInfo = info;
    }
    uint32_t w = info.Width, h = info.Height;
    if (pSrcRect != NULL)
    {
        RECT r = *pSrcRect;
        if (r.left < 0 || r.top < 0 || r.right > (LONG)w || r.bottom > (LONG)h || r.right <= r.left ||
            r.bottom <= r.top)
        {
            return D3DERR_INVALIDCALL;
        }
        std::vector<uint32_t> part((size_t)(r.right - r.left) * (r.bottom - r.top));
        for (LONG y = r.top; y < r.bottom; y++)
        {
            memcpy(&part[(size_t)(y - r.top) * (r.right - r.left)], &pixels[(size_t)y * w + r.left],
                   (r.right - r.left) * 4);
        }
        pixels.swap(part);
        w = r.right - r.left;
        h = r.bottom - r.top;
    }
    return load_surface(pDestSurface, pDestRect, pixels, w, h, Filter, ColorKey);
}

HRESULT D3DXGetImageInfoFromFileInMemory(LPCVOID pSrcData, UINT SrcDataSize, D3DXIMAGE_INFO *pSrcInfo)
{
    std::vector<uint32_t> pixels;
    D3DXIMAGE_INFO info;
    if (pSrcData == NULL || pSrcInfo == NULL || !decode_image(pSrcData, SrcDataSize, pixels, &info))
    {
        return D3DERR_INVALIDCALL;
    }
    *pSrcInfo = info;
    return D3D_OK;
}

} // extern "C"
