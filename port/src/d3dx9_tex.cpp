// The D3DX texture helpers. AnmLoader.cpp creates ANM textures from the raw
// pixel data in .anm files (D3DXCreateTexture + D3DXLoadSurfaceFromMemory)
// or from image files in memory (D3DXCreateTextureFromFileInMemoryEx,
// D3DXLoadSurfaceFromFileInMemory); TextHelper.cpp uploads rendered text
// with D3DXLoadSurfaceFromMemory; screen captures copy the back buffer with
// D3DXLoadSurfaceFromSurface. Stubs for now; the renderer implements them
// on top of its IDirect3DTexture9 (with an image decoder for the file
// variants).
#include <d3dx9tex.h>

#include "port_stub.h"

extern "C" {

HRESULT D3DXCreateTexture(IDirect3DDevice9 *pDevice, UINT Width, UINT Height, UINT MipLevels, DWORD Usage,
                          D3DFORMAT Format, D3DPOOL Pool, IDirect3DTexture9 **ppTexture)
{
    PORT_UNIMPLEMENTED();
    *ppTexture = NULL;
    return D3DERR_NOTAVAILABLE;
}

HRESULT D3DXCreateTextureFromFileInMemoryEx(IDirect3DDevice9 *pDevice, LPCVOID pSrcData, UINT SrcDataSize,
                                            UINT Width, UINT Height, UINT MipLevels, DWORD Usage, D3DFORMAT Format,
                                            D3DPOOL Pool, DWORD Filter, DWORD MipFilter, D3DCOLOR ColorKey,
                                            D3DXIMAGE_INFO *pSrcInfo, PALETTEENTRY *pPalette,
                                            IDirect3DTexture9 **ppTexture)
{
    PORT_UNIMPLEMENTED();
    *ppTexture = NULL;
    return D3DERR_NOTAVAILABLE;
}

HRESULT D3DXLoadSurfaceFromSurface(IDirect3DSurface9 *pDestSurface, const PALETTEENTRY *pDestPalette,
                                   const RECT *pDestRect, IDirect3DSurface9 *pSrcSurface,
                                   const PALETTEENTRY *pSrcPalette, const RECT *pSrcRect, DWORD Filter,
                                   D3DCOLOR ColorKey)
{
    PORT_UNIMPLEMENTED();
    return D3DERR_NOTAVAILABLE;
}

HRESULT D3DXLoadSurfaceFromMemory(IDirect3DSurface9 *pDestSurface, const PALETTEENTRY *pDestPalette,
                                  const RECT *pDestRect, LPCVOID pSrcMemory, D3DFORMAT SrcFormat, UINT SrcPitch,
                                  const PALETTEENTRY *pSrcPalette, const RECT *pSrcRect, DWORD Filter,
                                  D3DCOLOR ColorKey)
{
    PORT_UNIMPLEMENTED();
    return D3DERR_NOTAVAILABLE;
}

HRESULT D3DXLoadSurfaceFromFileInMemory(IDirect3DSurface9 *pDestSurface, const PALETTEENTRY *pDestPalette,
                                        const RECT *pDestRect, LPCVOID pSrcData, UINT SrcDataSize,
                                        const RECT *pSrcRect, DWORD Filter, D3DCOLOR ColorKey,
                                        D3DXIMAGE_INFO *pSrcInfo)
{
    PORT_UNIMPLEMENTED();
    return D3DERR_NOTAVAILABLE;
}

HRESULT D3DXGetImageInfoFromFileInMemory(LPCVOID pSrcData, UINT SrcDataSize, D3DXIMAGE_INFO *pSrcInfo)
{
    PORT_UNIMPLEMENTED();
    return D3DERR_NOTAVAILABLE;
}

} // extern "C"
