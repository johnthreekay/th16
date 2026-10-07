// Direct3D 9: the skeleton of the renderer the platform layer will provide
// (OpenGL). Every interface the game calls has a stub class here with every
// method, logging and failing; Direct3DCreate9 hands out the IDirect3D9 one,
// whose CreateDevice fails, so the game takes its device-creation error
// path.
//
// What the game needs from a real implementation (see NOTES.md): pre-
// transformed (D3DFVF_XYZRHW) and world-space (D3DFVF_XYZ) quads, strips and
// fans through DrawPrimitiveUP, fixed-function texture stage states
// (MODULATE, SELECTARG*, ADD with D3DTA_TFACTOR), the blend modes in
// AnmManager's render state code, linear fog, render-to-texture with
// GetSurfaceLevel/SetRenderTarget, and lockable textures for the text
// renderer and screenshots.
#include <d3d9.h>

#include "port_stub.h"

struct StubDirect3DTexture9 : public PortComObject<IDirect3DTexture9>
{
    HRESULT GetDevice(IDirect3DDevice9 **ppDevice) override
    {
        PORT_UNIMPLEMENTED();
        return D3DERR_INVALIDCALL;
    }
    DWORD SetPriority(DWORD PriorityNew) override
    {
        PORT_UNIMPLEMENTED();
        return 0;
    }
    DWORD GetPriority() override
    {
        PORT_UNIMPLEMENTED();
        return 0;
    }
    void PreLoad() override
    {
        PORT_UNIMPLEMENTED();
    }
    D3DRESOURCETYPE GetType() override
    {
        return D3DRTYPE_TEXTURE;
    }
    DWORD SetLOD(DWORD LODNew) override
    {
        PORT_UNIMPLEMENTED();
        return 0;
    }
    DWORD GetLOD() override
    {
        PORT_UNIMPLEMENTED();
        return 0;
    }
    DWORD GetLevelCount() override
    {
        PORT_UNIMPLEMENTED();
        return 1;
    }
    HRESULT GetLevelDesc(UINT Level, D3DSURFACE_DESC *pDesc) override
    {
        PORT_UNIMPLEMENTED();
        return D3DERR_INVALIDCALL;
    }
    HRESULT GetSurfaceLevel(UINT Level, IDirect3DSurface9 **ppSurfaceLevel) override
    {
        PORT_UNIMPLEMENTED();
        *ppSurfaceLevel = NULL;
        return D3DERR_INVALIDCALL;
    }
    HRESULT LockRect(UINT Level, D3DLOCKED_RECT *pLockedRect, const RECT *pRect, DWORD Flags) override
    {
        PORT_UNIMPLEMENTED();
        return D3DERR_INVALIDCALL;
    }
    HRESULT UnlockRect(UINT Level) override
    {
        PORT_UNIMPLEMENTED();
        return D3DERR_INVALIDCALL;
    }
    HRESULT AddDirtyRect(const RECT *pDirtyRect) override
    {
        PORT_UNIMPLEMENTED();
        return D3D_OK;
    }
};

struct StubDirect3DSurface9 : public PortComObject<IDirect3DSurface9>
{
    HRESULT GetDevice(IDirect3DDevice9 **ppDevice) override
    {
        PORT_UNIMPLEMENTED();
        return D3DERR_INVALIDCALL;
    }
    DWORD SetPriority(DWORD PriorityNew) override
    {
        PORT_UNIMPLEMENTED();
        return 0;
    }
    DWORD GetPriority() override
    {
        PORT_UNIMPLEMENTED();
        return 0;
    }
    void PreLoad() override
    {
        PORT_UNIMPLEMENTED();
    }
    D3DRESOURCETYPE GetType() override
    {
        return D3DRTYPE_SURFACE;
    }
    HRESULT GetDesc(D3DSURFACE_DESC *pDesc) override
    {
        PORT_UNIMPLEMENTED();
        return D3DERR_INVALIDCALL;
    }
    HRESULT LockRect(D3DLOCKED_RECT *pLockedRect, const RECT *pRect, DWORD Flags) override
    {
        PORT_UNIMPLEMENTED();
        return D3DERR_INVALIDCALL;
    }
    HRESULT UnlockRect() override
    {
        PORT_UNIMPLEMENTED();
        return D3DERR_INVALIDCALL;
    }
};

struct StubDirect3DVertexBuffer9 : public PortComObject<IDirect3DVertexBuffer9>
{
    HRESULT GetDevice(IDirect3DDevice9 **ppDevice) override
    {
        PORT_UNIMPLEMENTED();
        return D3DERR_INVALIDCALL;
    }
    DWORD SetPriority(DWORD PriorityNew) override
    {
        PORT_UNIMPLEMENTED();
        return 0;
    }
    DWORD GetPriority() override
    {
        PORT_UNIMPLEMENTED();
        return 0;
    }
    void PreLoad() override
    {
        PORT_UNIMPLEMENTED();
    }
    D3DRESOURCETYPE GetType() override
    {
        return D3DRTYPE_VERTEXBUFFER;
    }
    HRESULT Lock(UINT OffsetToLock, UINT SizeToLock, void **ppbData, DWORD Flags) override
    {
        PORT_UNIMPLEMENTED();
        return D3DERR_INVALIDCALL;
    }
    HRESULT Unlock() override
    {
        PORT_UNIMPLEMENTED();
        return D3DERR_INVALIDCALL;
    }
    HRESULT GetDesc(D3DVERTEXBUFFER_DESC *pDesc) override
    {
        PORT_UNIMPLEMENTED();
        return D3DERR_INVALIDCALL;
    }
};

struct StubDirect3DDevice9 : public PortComObject<IDirect3DDevice9>
{
    HRESULT TestCooperativeLevel() override
    {
        PORT_UNIMPLEMENTED();
        return D3D_OK;
    }
    UINT GetAvailableTextureMem() override
    {
        PORT_UNIMPLEMENTED();
        return 0;
    }
    HRESULT EvictManagedResources() override
    {
        PORT_UNIMPLEMENTED();
        return D3D_OK;
    }
    HRESULT GetDirect3D(IDirect3D9 **ppD3D9) override
    {
        PORT_UNIMPLEMENTED();
        return D3DERR_INVALIDCALL;
    }
    HRESULT GetDeviceCaps(D3DCAPS9 *pCaps) override
    {
        PORT_UNIMPLEMENTED();
        return D3DERR_INVALIDCALL;
    }
    HRESULT GetDisplayMode(UINT iSwapChain, D3DDISPLAYMODE *pMode) override
    {
        PORT_UNIMPLEMENTED();
        return D3DERR_INVALIDCALL;
    }
    HRESULT Reset(D3DPRESENT_PARAMETERS *pPresentationParameters) override
    {
        PORT_UNIMPLEMENTED();
        return D3DERR_INVALIDCALL;
    }
    HRESULT Present(const RECT *pSourceRect, const RECT *pDestRect, HWND hDestWindowOverride,
                    const void *pDirtyRegion) override
    {
        PORT_UNIMPLEMENTED();
        return D3D_OK;
    }
    HRESULT GetBackBuffer(UINT iSwapChain, UINT iBackBuffer, D3DBACKBUFFER_TYPE Type,
                          IDirect3DSurface9 **ppBackBuffer) override
    {
        PORT_UNIMPLEMENTED();
        *ppBackBuffer = NULL;
        return D3DERR_INVALIDCALL;
    }
    HRESULT GetRasterStatus(UINT iSwapChain, D3DRASTER_STATUS *pRasterStatus) override
    {
        PORT_UNIMPLEMENTED();
        return D3DERR_INVALIDCALL;
    }
    HRESULT CreateTexture(UINT Width, UINT Height, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool,
                          IDirect3DTexture9 **ppTexture, HANDLE *pSharedHandle) override
    {
        PORT_UNIMPLEMENTED();
        *ppTexture = NULL;
        return D3DERR_OUTOFVIDEOMEMORY;
    }
    HRESULT CreateVertexBuffer(UINT Length, DWORD Usage, DWORD FVF, D3DPOOL Pool,
                               IDirect3DVertexBuffer9 **ppVertexBuffer, HANDLE *pSharedHandle) override
    {
        PORT_UNIMPLEMENTED();
        *ppVertexBuffer = NULL;
        return D3DERR_OUTOFVIDEOMEMORY;
    }
    HRESULT CreateOffscreenPlainSurface(UINT Width, UINT Height, D3DFORMAT Format, D3DPOOL Pool,
                                        IDirect3DSurface9 **ppSurface, HANDLE *pSharedHandle) override
    {
        PORT_UNIMPLEMENTED();
        *ppSurface = NULL;
        return D3DERR_OUTOFVIDEOMEMORY;
    }
    HRESULT UpdateSurface(IDirect3DSurface9 *pSourceSurface, const RECT *pSourceRect,
                          IDirect3DSurface9 *pDestinationSurface, const POINT *pDestPoint) override
    {
        PORT_UNIMPLEMENTED();
        return D3DERR_INVALIDCALL;
    }
    HRESULT StretchRect(IDirect3DSurface9 *pSourceSurface, const RECT *pSourceRect, IDirect3DSurface9 *pDestSurface,
                        const RECT *pDestRect, D3DTEXTUREFILTERTYPE Filter) override
    {
        PORT_UNIMPLEMENTED();
        return D3DERR_INVALIDCALL;
    }
    HRESULT SetRenderTarget(DWORD RenderTargetIndex, IDirect3DSurface9 *pRenderTarget) override
    {
        PORT_UNIMPLEMENTED();
        return D3D_OK;
    }
    HRESULT GetRenderTarget(DWORD RenderTargetIndex, IDirect3DSurface9 **ppRenderTarget) override
    {
        PORT_UNIMPLEMENTED();
        *ppRenderTarget = NULL;
        return D3DERR_INVALIDCALL;
    }
    HRESULT BeginScene() override
    {
        PORT_UNIMPLEMENTED();
        return D3D_OK;
    }
    HRESULT EndScene() override
    {
        PORT_UNIMPLEMENTED();
        return D3D_OK;
    }
    HRESULT Clear(DWORD Count, const D3DRECT *pRects, DWORD Flags, D3DCOLOR Color, float Z, DWORD Stencil) override
    {
        PORT_UNIMPLEMENTED();
        return D3D_OK;
    }
    HRESULT SetTransform(D3DTRANSFORMSTATETYPE State, const D3DMATRIX *pMatrix) override
    {
        PORT_UNIMPLEMENTED();
        return D3D_OK;
    }
    HRESULT GetTransform(D3DTRANSFORMSTATETYPE State, D3DMATRIX *pMatrix) override
    {
        PORT_UNIMPLEMENTED();
        return D3DERR_INVALIDCALL;
    }
    HRESULT SetViewport(const D3DVIEWPORT9 *pViewport) override
    {
        PORT_UNIMPLEMENTED();
        return D3D_OK;
    }
    HRESULT GetViewport(D3DVIEWPORT9 *pViewport) override
    {
        PORT_UNIMPLEMENTED();
        return D3DERR_INVALIDCALL;
    }
    HRESULT SetRenderState(D3DRENDERSTATETYPE State, DWORD Value) override
    {
        PORT_UNIMPLEMENTED();
        return D3D_OK;
    }
    HRESULT GetRenderState(D3DRENDERSTATETYPE State, DWORD *pValue) override
    {
        PORT_UNIMPLEMENTED();
        return D3DERR_INVALIDCALL;
    }
    HRESULT GetTexture(DWORD Stage, IDirect3DBaseTexture9 **ppTexture) override
    {
        PORT_UNIMPLEMENTED();
        *ppTexture = NULL;
        return D3DERR_INVALIDCALL;
    }
    HRESULT SetTexture(DWORD Stage, IDirect3DBaseTexture9 *pTexture) override
    {
        PORT_UNIMPLEMENTED();
        return D3D_OK;
    }
    HRESULT GetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD *pValue) override
    {
        PORT_UNIMPLEMENTED();
        return D3DERR_INVALIDCALL;
    }
    HRESULT SetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD Value) override
    {
        PORT_UNIMPLEMENTED();
        return D3D_OK;
    }
    HRESULT GetSamplerState(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD *pValue) override
    {
        PORT_UNIMPLEMENTED();
        return D3DERR_INVALIDCALL;
    }
    HRESULT SetSamplerState(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD Value) override
    {
        PORT_UNIMPLEMENTED();
        return D3D_OK;
    }
    HRESULT DrawPrimitive(D3DPRIMITIVETYPE PrimitiveType, UINT StartVertex, UINT PrimitiveCount) override
    {
        PORT_UNIMPLEMENTED();
        return D3D_OK;
    }
    HRESULT DrawPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType, UINT PrimitiveCount, const void *pVertexStreamZeroData,
                            UINT VertexStreamZeroStride) override
    {
        PORT_UNIMPLEMENTED();
        return D3D_OK;
    }
    HRESULT SetFVF(DWORD FVF) override
    {
        PORT_UNIMPLEMENTED();
        return D3D_OK;
    }
    HRESULT GetFVF(DWORD *pFVF) override
    {
        PORT_UNIMPLEMENTED();
        return D3DERR_INVALIDCALL;
    }
    HRESULT SetVertexShader(IDirect3DVertexShader9 *pShader) override
    {
        PORT_UNIMPLEMENTED();
        return D3D_OK;
    }
    HRESULT SetStreamSource(UINT StreamNumber, IDirect3DVertexBuffer9 *pStreamData, UINT OffsetInBytes,
                            UINT Stride) override
    {
        PORT_UNIMPLEMENTED();
        return D3D_OK;
    }
    HRESULT SetPixelShader(IDirect3DPixelShader9 *pShader) override
    {
        PORT_UNIMPLEMENTED();
        return D3D_OK;
    }
};

struct StubDirect3D9 : public PortComObject<IDirect3D9>
{
    UINT GetAdapterCount() override
    {
        PORT_UNIMPLEMENTED();
        return 1;
    }
    HRESULT GetAdapterDisplayMode(UINT Adapter, D3DDISPLAYMODE *pMode) override
    {
        PORT_UNIMPLEMENTED();
        pMode->Width = 640;
        pMode->Height = 480;
        pMode->RefreshRate = 60;
        pMode->Format = D3DFMT_X8R8G8B8;
        return D3D_OK;
    }
    HRESULT CheckDeviceType(UINT Adapter, D3DDEVTYPE DevType, D3DFORMAT AdapterFormat, D3DFORMAT BackBufferFormat,
                            BOOL bWindowed) override
    {
        PORT_UNIMPLEMENTED();
        return D3DERR_NOTAVAILABLE;
    }
    HRESULT CheckDeviceFormat(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT AdapterFormat, DWORD Usage,
                              D3DRESOURCETYPE RType, D3DFORMAT CheckFormat) override
    {
        PORT_UNIMPLEMENTED();
        return D3DERR_NOTAVAILABLE;
    }
    HRESULT GetDeviceCaps(UINT Adapter, D3DDEVTYPE DeviceType, D3DCAPS9 *pCaps) override
    {
        PORT_UNIMPLEMENTED();
        return D3DERR_NOTAVAILABLE;
    }
    HRESULT CreateDevice(UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags,
                         D3DPRESENT_PARAMETERS *pPresentationParameters,
                         IDirect3DDevice9 **ppReturnedDeviceInterface) override
    {
        // A StubDirect3DDevice9 would let the game go on to draw nothing;
        // failing here stops it at the device error instead.
        PORT_UNIMPLEMENTED();
        *ppReturnedDeviceInterface = NULL;
        return D3DERR_NOTAVAILABLE;
    }
};

extern "C" IDirect3D9 *Direct3DCreate9(UINT SDKVersion)
{
    PORT_UNIMPLEMENTED();
    return new StubDirect3D9();
}

// Referenced so the stub classes the device would hand out are compiled and
// kept in sync with the interfaces even while nothing creates them.
IDirect3DDevice9 *port_new_stub_d3d_device()
{
    return new StubDirect3DDevice9();
}

IDirect3DTexture9 *port_new_stub_d3d_texture()
{
    return new StubDirect3DTexture9();
}

IDirect3DSurface9 *port_new_stub_d3d_surface()
{
    return new StubDirect3DSurface9();
}

IDirect3DVertexBuffer9 *port_new_stub_d3d_vertex_buffer()
{
    return new StubDirect3DVertexBuffer9();
}
