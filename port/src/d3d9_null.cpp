// A null Direct3D 9 for running the game without a renderer (CMake option
// TH16_NULL_RENDERER, which builds this file instead of the other d3d9_*
// files and d3dx9_tex.cpp): every call succeeds, textures and surfaces are
// plain memory (so the game can lock them and the text renderer's output
// lands in them), nothing is drawn, and Present waits for the next 1/60 s
// when the game asks for vsync. Meant for headless tests of everything but
// the picture (startup, files, threads, input, game logic, replays).
//
// TH16_NULL_DUMP_TEXT=<dir> writes each text image the game uploads with
// D3DXLoadSurfaceFromMemory (TextHelper's output) to <dir> as a PAM file
// (RGBA), to check the text renderer without a screen.
#include <stdlib.h>
#include <string.h>

#include <chrono>
#include <string>
#include <thread>
#include <vector>

#include <d3d9.h>
#include <d3dx9tex.h>

#include "port_platform.h"
#include "port_stub.h"

namespace
{

UINT bytes_per_pixel(D3DFORMAT format)
{
    switch (format)
    {
    case D3DFMT_R5G6B5:
    case D3DFMT_X1R5G5B5:
    case D3DFMT_A1R5G5B5:
    case D3DFMT_A4R4G4B4:
    case D3DFMT_D16:
        return 2;
    default:
        return 4;
    }
}

struct NullSurface : public PortComObject<IDirect3DSurface9>
{
    D3DSURFACE_DESC desc;
    UINT pitch;
    std::vector<uint8_t> pixels;

    NullSurface(UINT width, UINT height, D3DFORMAT format, DWORD usage, D3DPOOL pool)
    {
        memset(&desc, 0, sizeof(desc));
        desc.Format = format;
        desc.Type = D3DRTYPE_SURFACE;
        desc.Usage = usage;
        desc.Pool = pool;
        desc.Width = width;
        desc.Height = height;
        pitch = (width * bytes_per_pixel(format) + 3) & ~3u;
        pixels.assign((size_t)pitch * height, 0);
    }
    HRESULT GetDevice(IDirect3DDevice9 **ppDevice) override
    {
        *ppDevice = NULL;
        return D3DERR_INVALIDCALL;
    }
    DWORD SetPriority(DWORD PriorityNew) override
    {
        return 0;
    }
    DWORD GetPriority() override
    {
        return 0;
    }
    void PreLoad() override
    {
    }
    D3DRESOURCETYPE GetType() override
    {
        return D3DRTYPE_SURFACE;
    }
    HRESULT GetDesc(D3DSURFACE_DESC *pDesc) override
    {
        *pDesc = desc;
        return D3D_OK;
    }
    HRESULT LockRect(D3DLOCKED_RECT *pLockedRect, const RECT *pRect, DWORD Flags) override
    {
        size_t offset = pRect != NULL ? (size_t)pRect->top * pitch + pRect->left * bytes_per_pixel(desc.Format) : 0;
        pLockedRect->Pitch = (INT)pitch;
        pLockedRect->pBits = pixels.data() + offset;
        return D3D_OK;
    }
    HRESULT UnlockRect() override
    {
        return D3D_OK;
    }
};

struct NullTexture : public PortComObject<IDirect3DTexture9>
{
    NullSurface *level;

    NullTexture(UINT width, UINT height, D3DFORMAT format, DWORD usage, D3DPOOL pool)
        : level(new NullSurface(width, height, format, usage, pool))
    {
    }
    ~NullTexture()
    {
        level->Release();
    }
    HRESULT GetDevice(IDirect3DDevice9 **ppDevice) override
    {
        *ppDevice = NULL;
        return D3DERR_INVALIDCALL;
    }
    DWORD SetPriority(DWORD PriorityNew) override
    {
        return 0;
    }
    DWORD GetPriority() override
    {
        return 0;
    }
    void PreLoad() override
    {
    }
    D3DRESOURCETYPE GetType() override
    {
        return D3DRTYPE_TEXTURE;
    }
    DWORD SetLOD(DWORD LODNew) override
    {
        return 0;
    }
    DWORD GetLOD() override
    {
        return 0;
    }
    DWORD GetLevelCount() override
    {
        return 1;
    }
    HRESULT GetLevelDesc(UINT Level, D3DSURFACE_DESC *pDesc) override
    {
        return level->GetDesc(pDesc);
    }
    HRESULT GetSurfaceLevel(UINT Level, IDirect3DSurface9 **ppSurfaceLevel) override
    {
        level->AddRef();
        *ppSurfaceLevel = level;
        return D3D_OK;
    }
    HRESULT LockRect(UINT Level, D3DLOCKED_RECT *pLockedRect, const RECT *pRect, DWORD Flags) override
    {
        return level->LockRect(pLockedRect, pRect, Flags);
    }
    HRESULT UnlockRect(UINT Level) override
    {
        return D3D_OK;
    }
    HRESULT AddDirtyRect(const RECT *pDirtyRect) override
    {
        return D3D_OK;
    }
};

struct NullVertexBuffer : public PortComObject<IDirect3DVertexBuffer9>
{
    D3DVERTEXBUFFER_DESC desc;
    std::vector<uint8_t> data;

    NullVertexBuffer(UINT length, DWORD usage, DWORD fvf, D3DPOOL pool) : data(length)
    {
        memset(&desc, 0, sizeof(desc));
        desc.Format = D3DFMT_VERTEXDATA;
        desc.Type = D3DRTYPE_VERTEXBUFFER;
        desc.Usage = usage;
        desc.Pool = pool;
        desc.Size = length;
        desc.FVF = fvf;
    }
    HRESULT GetDevice(IDirect3DDevice9 **ppDevice) override
    {
        *ppDevice = NULL;
        return D3DERR_INVALIDCALL;
    }
    DWORD SetPriority(DWORD PriorityNew) override
    {
        return 0;
    }
    DWORD GetPriority() override
    {
        return 0;
    }
    void PreLoad() override
    {
    }
    D3DRESOURCETYPE GetType() override
    {
        return D3DRTYPE_VERTEXBUFFER;
    }
    HRESULT Lock(UINT OffsetToLock, UINT SizeToLock, void **ppbData, DWORD Flags) override
    {
        *ppbData = data.data() + OffsetToLock;
        return D3D_OK;
    }
    HRESULT Unlock() override
    {
        return D3D_OK;
    }
    HRESULT GetDesc(D3DVERTEXBUFFER_DESC *pDesc) override
    {
        *pDesc = desc;
        return D3D_OK;
    }
};

void fill_caps(D3DCAPS9 *caps)
{
    memset(caps, 0, sizeof(*caps));
    caps->DeviceType = D3DDEVTYPE_HAL;
    caps->PresentationIntervals = D3DPRESENT_INTERVAL_ONE | D3DPRESENT_INTERVAL_IMMEDIATE;
    caps->TextureOpCaps = 0xffffffff;
    caps->MaxTextureWidth = 8192;
    caps->MaxTextureHeight = 8192;
    caps->MaxTextureBlendStages = 8;
    caps->MaxSimultaneousTextures = 8;
    caps->MaxPrimitiveCount = 0xffffff;
    caps->MaxVertexIndex = 0xffffff;
    caps->MaxStreams = 1;
    caps->MaxStreamStride = 256;
}

struct NullDevice : public PortComObject<IDirect3DDevice9>
{
    D3DPRESENT_PARAMETERS params;
    NullSurface *back_buffer = NULL;
    IDirect3DSurface9 *render_target = NULL;
    std::chrono::steady_clock::time_point next_present = std::chrono::steady_clock::now();

    explicit NullDevice(const D3DPRESENT_PARAMETERS &present)
    {
        Reset((D3DPRESENT_PARAMETERS *)&present);
    }
    ~NullDevice()
    {
        if (render_target != NULL)
        {
            render_target->Release();
        }
        if (back_buffer != NULL)
        {
            back_buffer->Release();
        }
    }
    HRESULT TestCooperativeLevel() override
    {
        return D3D_OK;
    }
    UINT GetAvailableTextureMem() override
    {
        return 512u << 20;
    }
    HRESULT EvictManagedResources() override
    {
        return D3D_OK;
    }
    HRESULT GetDirect3D(IDirect3D9 **ppD3D9) override
    {
        *ppD3D9 = NULL;
        return D3DERR_INVALIDCALL;
    }
    HRESULT GetDeviceCaps(D3DCAPS9 *pCaps) override
    {
        fill_caps(pCaps);
        return D3D_OK;
    }
    HRESULT GetDisplayMode(UINT iSwapChain, D3DDISPLAYMODE *pMode) override
    {
        port_display_size((int *)&pMode->Width, (int *)&pMode->Height);
        pMode->RefreshRate = port_display_refresh_rate();
        pMode->Format = D3DFMT_X8R8G8B8;
        return D3D_OK;
    }
    HRESULT Reset(D3DPRESENT_PARAMETERS *pPresentationParameters) override
    {
        params = *pPresentationParameters;
        if (render_target != NULL)
        {
            render_target->Release();
        }
        if (back_buffer != NULL)
        {
            back_buffer->Release();
        }
        UINT width = params.BackBufferWidth != 0 ? params.BackBufferWidth : 640;
        UINT height = params.BackBufferHeight != 0 ? params.BackBufferHeight : 480;
        D3DFORMAT format = params.BackBufferFormat != D3DFMT_UNKNOWN ? params.BackBufferFormat : D3DFMT_X8R8G8B8;
        back_buffer = new NullSurface(width, height, format, D3DUSAGE_RENDERTARGET, D3DPOOL_DEFAULT);
        back_buffer->AddRef();
        render_target = back_buffer;
        return D3D_OK;
    }
    HRESULT Present(const RECT *pSourceRect, const RECT *pDestRect, HWND hDestWindowOverride,
                    const void *pDirtyRegion) override
    {
        if (params.PresentationInterval != D3DPRESENT_INTERVAL_IMMEDIATE)
        {
            auto now = std::chrono::steady_clock::now();
            if (next_present > now)
            {
                std::this_thread::sleep_until(next_present);
            }
            else
            {
                next_present = now;
            }
            next_present += std::chrono::microseconds(16667);
        }
        return D3D_OK;
    }
    HRESULT GetBackBuffer(UINT iSwapChain, UINT iBackBuffer, D3DBACKBUFFER_TYPE Type,
                          IDirect3DSurface9 **ppBackBuffer) override
    {
        back_buffer->AddRef();
        *ppBackBuffer = back_buffer;
        return D3D_OK;
    }
    HRESULT GetRasterStatus(UINT iSwapChain, D3DRASTER_STATUS *pRasterStatus) override
    {
        pRasterStatus->InVBlank = TRUE;
        pRasterStatus->ScanLine = 0;
        return D3D_OK;
    }
    HRESULT CreateTexture(UINT Width, UINT Height, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool,
                          IDirect3DTexture9 **ppTexture, HANDLE *pSharedHandle) override
    {
        *ppTexture = new NullTexture(Width, Height, Format, Usage, Pool);
        return D3D_OK;
    }
    HRESULT CreateVertexBuffer(UINT Length, DWORD Usage, DWORD FVF, D3DPOOL Pool,
                               IDirect3DVertexBuffer9 **ppVertexBuffer, HANDLE *pSharedHandle) override
    {
        *ppVertexBuffer = new NullVertexBuffer(Length, Usage, FVF, Pool);
        return D3D_OK;
    }
    HRESULT CreateOffscreenPlainSurface(UINT Width, UINT Height, D3DFORMAT Format, D3DPOOL Pool,
                                        IDirect3DSurface9 **ppSurface, HANDLE *pSharedHandle) override
    {
        *ppSurface = new NullSurface(Width, Height, Format, 0, Pool);
        return D3D_OK;
    }
    HRESULT UpdateSurface(IDirect3DSurface9 *pSourceSurface, const RECT *pSourceRect,
                          IDirect3DSurface9 *pDestinationSurface, const POINT *pDestPoint) override
    {
        return D3D_OK;
    }
    HRESULT StretchRect(IDirect3DSurface9 *pSourceSurface, const RECT *pSourceRect, IDirect3DSurface9 *pDestSurface,
                        const RECT *pDestRect, D3DTEXTUREFILTERTYPE Filter) override
    {
        return D3D_OK;
    }
    HRESULT SetRenderTarget(DWORD RenderTargetIndex, IDirect3DSurface9 *pRenderTarget) override
    {
        if (pRenderTarget == NULL)
        {
            return D3DERR_INVALIDCALL;
        }
        pRenderTarget->AddRef();
        render_target->Release();
        render_target = pRenderTarget;
        return D3D_OK;
    }
    HRESULT GetRenderTarget(DWORD RenderTargetIndex, IDirect3DSurface9 **ppRenderTarget) override
    {
        render_target->AddRef();
        *ppRenderTarget = render_target;
        return D3D_OK;
    }
    HRESULT BeginScene() override
    {
        return D3D_OK;
    }
    HRESULT EndScene() override
    {
        return D3D_OK;
    }
    HRESULT Clear(DWORD Count, const D3DRECT *pRects, DWORD Flags, D3DCOLOR Color, float Z, DWORD Stencil) override
    {
        return D3D_OK;
    }
    HRESULT SetTransform(D3DTRANSFORMSTATETYPE State, const D3DMATRIX *pMatrix) override
    {
        return D3D_OK;
    }
    HRESULT GetTransform(D3DTRANSFORMSTATETYPE State, D3DMATRIX *pMatrix) override
    {
        memset(pMatrix, 0, sizeof(*pMatrix));
        return D3D_OK;
    }
    HRESULT SetViewport(const D3DVIEWPORT9 *pViewport) override
    {
        return D3D_OK;
    }
    HRESULT GetViewport(D3DVIEWPORT9 *pViewport) override
    {
        memset(pViewport, 0, sizeof(*pViewport));
        return D3D_OK;
    }
    HRESULT SetRenderState(D3DRENDERSTATETYPE State, DWORD Value) override
    {
        return D3D_OK;
    }
    HRESULT GetRenderState(D3DRENDERSTATETYPE State, DWORD *pValue) override
    {
        *pValue = 0;
        return D3D_OK;
    }
    HRESULT GetTexture(DWORD Stage, IDirect3DBaseTexture9 **ppTexture) override
    {
        *ppTexture = NULL;
        return D3D_OK;
    }
    HRESULT SetTexture(DWORD Stage, IDirect3DBaseTexture9 *pTexture) override
    {
        return D3D_OK;
    }
    HRESULT GetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD *pValue) override
    {
        *pValue = 0;
        return D3D_OK;
    }
    HRESULT SetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD Value) override
    {
        return D3D_OK;
    }
    HRESULT GetSamplerState(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD *pValue) override
    {
        *pValue = 0;
        return D3D_OK;
    }
    HRESULT SetSamplerState(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD Value) override
    {
        return D3D_OK;
    }
    HRESULT DrawPrimitive(D3DPRIMITIVETYPE PrimitiveType, UINT StartVertex, UINT PrimitiveCount) override
    {
        return D3D_OK;
    }
    HRESULT DrawPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType, UINT PrimitiveCount, const void *pVertexStreamZeroData,
                            UINT VertexStreamZeroStride) override
    {
        return D3D_OK;
    }
    HRESULT SetFVF(DWORD FVF) override
    {
        return D3D_OK;
    }
    HRESULT GetFVF(DWORD *pFVF) override
    {
        *pFVF = 0;
        return D3D_OK;
    }
    HRESULT SetVertexShader(IDirect3DVertexShader9 *pShader) override
    {
        return D3D_OK;
    }
    HRESULT SetStreamSource(UINT StreamNumber, IDirect3DVertexBuffer9 *pStreamData, UINT OffsetInBytes,
                            UINT Stride) override
    {
        return D3D_OK;
    }
    HRESULT SetPixelShader(IDirect3DPixelShader9 *pShader) override
    {
        return D3D_OK;
    }
};

struct NullDirect3D9 : public PortComObject<IDirect3D9>
{
    UINT GetAdapterCount() override
    {
        return 1;
    }
    HRESULT GetAdapterDisplayMode(UINT Adapter, D3DDISPLAYMODE *pMode) override
    {
        int width, height;
        port_display_size(&width, &height);
        pMode->Width = width;
        pMode->Height = height;
        pMode->RefreshRate = port_display_refresh_rate();
        pMode->Format = D3DFMT_X8R8G8B8;
        return D3D_OK;
    }
    HRESULT CheckDeviceType(UINT Adapter, D3DDEVTYPE DevType, D3DFORMAT AdapterFormat, D3DFORMAT BackBufferFormat,
                            BOOL bWindowed) override
    {
        return D3D_OK;
    }
    HRESULT CheckDeviceFormat(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT AdapterFormat, DWORD Usage,
                              D3DRESOURCETYPE RType, D3DFORMAT CheckFormat) override
    {
        return D3D_OK;
    }
    HRESULT GetDeviceCaps(UINT Adapter, D3DDEVTYPE DeviceType, D3DCAPS9 *pCaps) override
    {
        fill_caps(pCaps);
        return D3D_OK;
    }
    HRESULT CreateDevice(UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags,
                         D3DPRESENT_PARAMETERS *pPresentationParameters,
                         IDirect3DDevice9 **ppReturnedDeviceInterface) override
    {
        *ppReturnedDeviceInterface = new NullDevice(*pPresentationParameters);
        return D3D_OK;
    }
};

// Same-format copies between the null surfaces; the rest is ignored.
HRESULT copy_rect(IDirect3DSurface9 *dst_surface, const RECT *dst_rect, const uint8_t *src, D3DFORMAT src_format,
                  UINT src_pitch, const RECT *src_rect)
{
    NullSurface *dst = (NullSurface *)dst_surface;
    if (dst == NULL)
    {
        return D3DERR_INVALIDCALL;
    }
    if (src_format != dst->desc.Format)
    {
        return D3D_OK;
    }
    UINT bpp = bytes_per_pixel(src_format);
    RECT full_dst = {0, 0, (LONG)dst->desc.Width, (LONG)dst->desc.Height};
    const RECT &d = dst_rect != NULL ? *dst_rect : full_dst;
    LONG sx = src_rect != NULL ? src_rect->left : 0;
    LONG sy = src_rect != NULL ? src_rect->top : 0;
    LONG width = d.right - d.left;
    LONG height = d.bottom - d.top;
    if (src_rect != NULL)
    {
        width = width < src_rect->right - src_rect->left ? width : src_rect->right - src_rect->left;
        height = height < src_rect->bottom - src_rect->top ? height : src_rect->bottom - src_rect->top;
    }
    for (LONG y = 0; y < height; y++)
    {
        if (d.top + y < 0 || d.top + y >= (LONG)dst->desc.Height || d.left < 0 ||
            d.left + width > (LONG)dst->desc.Width)
        {
            continue;
        }
        memcpy(dst->pixels.data() + (size_t)(d.top + y) * dst->pitch + d.left * bpp,
               src + (size_t)(sy + y) * src_pitch + sx * bpp, (size_t)width * bpp);
    }
    return D3D_OK;
}

void dump_text(const uint8_t *src, D3DFORMAT format, UINT pitch, const RECT *rect)
{
    const char *dir = getenv("TH16_NULL_DUMP_TEXT");
    if (dir == NULL || rect == NULL || (format != D3DFMT_A4R4G4B4 && format != D3DFMT_A8R8G8B8))
    {
        return;
    }
    static int count;
    std::string path = std::string(dir) + "/text_" + std::to_string(count++) + ".pam";
    FILE *file = fopen(path.c_str(), "wb");
    if (file == NULL)
    {
        return;
    }
    int width = rect->right - rect->left;
    int height = rect->bottom - rect->top;
    fprintf(file, "P7\nWIDTH %d\nHEIGHT %d\nDEPTH 4\nMAXVAL 255\nTUPLTYPE RGB_ALPHA\nENDHDR\n", width, height);
    for (int y = rect->top; y < rect->bottom; y++)
    {
        for (int x = rect->left; x < rect->right; x++)
        {
            uint8_t rgba[4];
            if (format == D3DFMT_A4R4G4B4)
            {
                uint16_t p = ((const uint16_t *)(src + (size_t)y * pitch))[x];
                rgba[0] = ((p >> 8) & 0xf) * 17;
                rgba[1] = ((p >> 4) & 0xf) * 17;
                rgba[2] = (p & 0xf) * 17;
                rgba[3] = (p >> 12) * 17;
            }
            else
            {
                uint32_t p = ((const uint32_t *)(src + (size_t)y * pitch))[x];
                rgba[0] = (p >> 16) & 0xff;
                rgba[1] = (p >> 8) & 0xff;
                rgba[2] = p & 0xff;
                rgba[3] = p >> 24;
            }
            fwrite(rgba, 1, 4, file);
        }
    }
    fclose(file);
}

// Width and height from a PNG, BMP or JPEG header.
bool image_size(const uint8_t *data, UINT size, UINT *width, UINT *height)
{
    if (size >= 24 && memcmp(data, "\x89PNG", 4) == 0)
    {
        *width = (data[16] << 24) | (data[17] << 16) | (data[18] << 8) | data[19];
        *height = (data[20] << 24) | (data[21] << 16) | (data[22] << 8) | data[23];
        return true;
    }
    if (size >= 26 && data[0] == 'B' && data[1] == 'M')
    {
        int32_t w, h;
        memcpy(&w, data + 18, 4);
        memcpy(&h, data + 22, 4);
        *width = (UINT)w;
        *height = (UINT)(h < 0 ? -h : h);
        return true;
    }
    for (UINT i = 2; size >= 4 && i + 9 < size && data[0] == 0xff && data[1] == 0xd8;)
    {
        if (data[i] != 0xff)
        {
            break;
        }
        uint8_t marker = data[i + 1];
        UINT length = (data[i + 2] << 8) | data[i + 3];
        if (marker >= 0xc0 && marker <= 0xc3)
        {
            *height = (data[i + 5] << 8) | data[i + 6];
            *width = (data[i + 7] << 8) | data[i + 8];
            return true;
        }
        i += 2 + length;
    }
    return false;
}

} // namespace

extern "C" IDirect3D9 *Direct3DCreate9(UINT SDKVersion)
{
    port_log("null renderer: nothing is drawn");
    return new NullDirect3D9();
}

extern "C" {

HRESULT D3DXCreateTexture(IDirect3DDevice9 *pDevice, UINT Width, UINT Height, UINT MipLevels, DWORD Usage,
                          D3DFORMAT Format, D3DPOOL Pool, IDirect3DTexture9 **ppTexture)
{
    return pDevice->CreateTexture(Width, Height, 1, Usage, Format, Pool, ppTexture, NULL);
}

HRESULT D3DXGetImageInfoFromFileInMemory(LPCVOID pSrcData, UINT SrcDataSize, D3DXIMAGE_INFO *pSrcInfo)
{
    UINT width, height;
    if (!image_size((const uint8_t *)pSrcData, SrcDataSize, &width, &height))
    {
        return D3DERR_INVALIDCALL;
    }
    memset(pSrcInfo, 0, sizeof(*pSrcInfo));
    pSrcInfo->Width = width;
    pSrcInfo->Height = height;
    pSrcInfo->Depth = 1;
    pSrcInfo->MipLevels = 1;
    pSrcInfo->Format = D3DFMT_A8R8G8B8;
    pSrcInfo->ResourceType = D3DRTYPE_TEXTURE;
    pSrcInfo->ImageFileFormat = D3DXIFF_PNG;
    return D3D_OK;
}

HRESULT D3DXCreateTextureFromFileInMemoryEx(IDirect3DDevice9 *pDevice, LPCVOID pSrcData, UINT SrcDataSize,
                                            UINT Width, UINT Height, UINT MipLevels, DWORD Usage, D3DFORMAT Format,
                                            D3DPOOL Pool, DWORD Filter, DWORD MipFilter, D3DCOLOR ColorKey,
                                            D3DXIMAGE_INFO *pSrcInfo, PALETTEENTRY *pPalette,
                                            IDirect3DTexture9 **ppTexture)
{
    D3DXIMAGE_INFO info;
    if (D3DXGetImageInfoFromFileInMemory(pSrcData, SrcDataSize, &info) != D3D_OK)
    {
        info.Width = info.Height = 256;
    }
    if (pSrcInfo != NULL)
    {
        *pSrcInfo = info;
    }
    UINT width = Width == D3DX_DEFAULT || Width == D3DX_DEFAULT_NONPOW2 || Width == 0 ? info.Width : Width;
    UINT height = Height == D3DX_DEFAULT || Height == D3DX_DEFAULT_NONPOW2 || Height == 0 ? info.Height : Height;
    D3DFORMAT format = Format == D3DFMT_UNKNOWN || Format == D3DFMT_FROM_FILE ? D3DFMT_A8R8G8B8 : Format;
    return pDevice->CreateTexture(width, height, 1, Usage, format, Pool, ppTexture, NULL);
}

HRESULT D3DXLoadSurfaceFromSurface(IDirect3DSurface9 *pDestSurface, const PALETTEENTRY *pDestPalette,
                                   const RECT *pDestRect, IDirect3DSurface9 *pSrcSurface,
                                   const PALETTEENTRY *pSrcPalette, const RECT *pSrcRect, DWORD Filter,
                                   D3DCOLOR ColorKey)
{
    NullSurface *src = (NullSurface *)pSrcSurface;
    if (src == NULL)
    {
        return D3DERR_INVALIDCALL;
    }
    return copy_rect(pDestSurface, pDestRect, src->pixels.data(), src->desc.Format, src->pitch, pSrcRect);
}

HRESULT D3DXLoadSurfaceFromMemory(IDirect3DSurface9 *pDestSurface, const PALETTEENTRY *pDestPalette,
                                  const RECT *pDestRect, LPCVOID pSrcMemory, D3DFORMAT SrcFormat, UINT SrcPitch,
                                  const PALETTEENTRY *pSrcPalette, const RECT *pSrcRect, DWORD Filter,
                                  D3DCOLOR ColorKey)
{
    dump_text((const uint8_t *)pSrcMemory, SrcFormat, SrcPitch, pSrcRect);
    return copy_rect(pDestSurface, pDestRect, (const uint8_t *)pSrcMemory, SrcFormat, SrcPitch, pSrcRect);
}

HRESULT D3DXLoadSurfaceFromFileInMemory(IDirect3DSurface9 *pDestSurface, const PALETTEENTRY *pDestPalette,
                                        const RECT *pDestRect, LPCVOID pSrcData, UINT SrcDataSize,
                                        const RECT *pSrcRect, DWORD Filter, D3DCOLOR ColorKey,
                                        D3DXIMAGE_INFO *pSrcInfo)
{
    if (pSrcInfo != NULL)
    {
        D3DXGetImageInfoFromFileInMemory(pSrcData, SrcDataSize, pSrcInfo);
    }
    return D3D_OK;
}

} // extern "C"
