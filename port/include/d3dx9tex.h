// Stand-in for the DirectX SDK's d3dx9tex.h: the texture creation and
// surface loading helpers the game uses (port/src/d3dx9_tex.cpp).
#pragma once

#include <d3d9.h>

#define D3DX_DEFAULT ((UINT)-1)
#define D3DX_DEFAULT_NONPOW2 ((UINT)-2)
#define D3DX_FROM_FILE ((UINT)-3)
#define D3DFMT_FROM_FILE ((D3DFORMAT)-3)

#define D3DX_FILTER_NONE (1 << 0)
#define D3DX_FILTER_POINT (2 << 0)
#define D3DX_FILTER_LINEAR (3 << 0)
#define D3DX_FILTER_TRIANGLE (4 << 0)
#define D3DX_FILTER_BOX (5 << 0)
#define D3DX_FILTER_MIRROR_U (1 << 16)
#define D3DX_FILTER_MIRROR_V (2 << 16)
#define D3DX_FILTER_MIRROR_W (4 << 16)
#define D3DX_FILTER_MIRROR (7 << 16)
#define D3DX_FILTER_DITHER (1 << 19)
#define D3DX_FILTER_DITHER_DIFFUSION (2 << 19)
#define D3DX_FILTER_SRGB_IN (1 << 21)
#define D3DX_FILTER_SRGB_OUT (2 << 21)
#define D3DX_FILTER_SRGB (3 << 21)

typedef enum _D3DXIMAGE_FILEFORMAT {
    D3DXIFF_BMP = 0,
    D3DXIFF_JPG = 1,
    D3DXIFF_TGA = 2,
    D3DXIFF_PNG = 3,
    D3DXIFF_DDS = 4,
    D3DXIFF_PPM = 5,
    D3DXIFF_DIB = 6,
    D3DXIFF_HDR = 7,
    D3DXIFF_PFM = 8,
    D3DXIFF_FORCE_DWORD = 0x7fffffff
} D3DXIMAGE_FILEFORMAT;

typedef struct _D3DXIMAGE_INFO
{
    UINT Width;
    UINT Height;
    UINT Depth;
    UINT MipLevels;
    D3DFORMAT Format;
    D3DRESOURCETYPE ResourceType;
    D3DXIMAGE_FILEFORMAT ImageFileFormat;
} D3DXIMAGE_INFO;

extern "C" {
HRESULT D3DXCreateTexture(IDirect3DDevice9 *pDevice, UINT Width, UINT Height, UINT MipLevels, DWORD Usage,
                          D3DFORMAT Format, D3DPOOL Pool, IDirect3DTexture9 **ppTexture);
HRESULT D3DXCreateTextureFromFileInMemoryEx(IDirect3DDevice9 *pDevice, LPCVOID pSrcData, UINT SrcDataSize,
                                            UINT Width, UINT Height, UINT MipLevels, DWORD Usage, D3DFORMAT Format,
                                            D3DPOOL Pool, DWORD Filter, DWORD MipFilter, D3DCOLOR ColorKey,
                                            D3DXIMAGE_INFO *pSrcInfo, PALETTEENTRY *pPalette,
                                            IDirect3DTexture9 **ppTexture);
HRESULT D3DXLoadSurfaceFromSurface(IDirect3DSurface9 *pDestSurface, const PALETTEENTRY *pDestPalette,
                                   const RECT *pDestRect, IDirect3DSurface9 *pSrcSurface,
                                   const PALETTEENTRY *pSrcPalette, const RECT *pSrcRect, DWORD Filter,
                                   D3DCOLOR ColorKey);
HRESULT D3DXLoadSurfaceFromMemory(IDirect3DSurface9 *pDestSurface, const PALETTEENTRY *pDestPalette,
                                  const RECT *pDestRect, LPCVOID pSrcMemory, D3DFORMAT SrcFormat, UINT SrcPitch,
                                  const PALETTEENTRY *pSrcPalette, const RECT *pSrcRect, DWORD Filter,
                                  D3DCOLOR ColorKey);
HRESULT D3DXLoadSurfaceFromFileInMemory(IDirect3DSurface9 *pDestSurface, const PALETTEENTRY *pDestPalette,
                                        const RECT *pDestRect, LPCVOID pSrcData, UINT SrcDataSize,
                                        const RECT *pSrcRect, DWORD Filter, D3DCOLOR ColorKey,
                                        D3DXIMAGE_INFO *pSrcInfo);
HRESULT D3DXGetImageInfoFromFileInMemory(LPCVOID pSrcData, UINT SrcDataSize, D3DXIMAGE_INFO *pSrcInfo);
}
