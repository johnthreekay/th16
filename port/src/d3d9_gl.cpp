// Direct3D 9 on OpenGL 3.3 (core profile) through SDL2: IDirect3D9,
// IDirect3DDevice9, IDirect3DTexture9, IDirect3DSurface9 and
// IDirect3DVertexBuffer9 with what the game uses of them.
//
// The fixed-function pipeline is one shader (kShaderVs/kShaderFs) driven by
// uniforms: pre-transformed (XYZRHW) and world-space (XYZ) vertices, the
// world/view/projection and texture transforms, texture stage 0's color and
// alpha operations, the texture factor, alpha test, vertex fog. Blending,
// depth and culling map to GL state.
//
// Conventions:
// - Every render target, the back buffer included, is a GL texture with an
//   FBO. Memory row 0 is the top of the picture, as in D3D, so textures
//   loaded from memory and textures rendered to are sampled the same way.
//   The shader maps D3D's y-down screen to GL's window coordinates so that
//   window row 0 is D3D's row 0; Present flips when it blits the back buffer
//   to the window.
// - D3D9 puts pixel centers on integer coordinates, GL on half-integers: the
//   shader moves every vertex by half a pixel.
// - Depth: D3D's clip-space z is [0, w], GL's [-w, w]; the shader remaps.
// - Textures keep a copy of their pixels in their D3D format (as D3D's
//   managed pool does), which LockRect hands out; the GL texture (RGBA8) is
//   updated lazily on the device thread. This lets the loading thread create
//   and fill textures while the main thread draws: GL is only ever called
//   from the thread that created the device.
// - Render targets have no CPU copy: LockRect reads them back with
//   glReadPixels (device thread only).
//
// Not implemented (the game never uses them): stages above 0, lighting,
// vertex/pixel shaders, index buffers, stencil, point sprites, specular.
#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <SDL.h>
#include <SDL_opengl.h>

#include <d3d9.h>

#include "d3d9_gl.h"
#include "d3d9_gl_funcs.h"
#include "d3d9_gl_internal.h"
#include "port_stub.h"

namespace gl
{
#define PORT_GL_DECLARE_11(name) static decltype(&::gl##name) name;
#define PORT_GL_DECLARE_EXT(name, type) static type name;
PORT_GL_FUNCS_11(PORT_GL_DECLARE_11)
PORT_GL_FUNCS_EXT(PORT_GL_DECLARE_EXT)
#undef PORT_GL_DECLARE_11
#undef PORT_GL_DECLARE_EXT
} // namespace gl

namespace
{

void gl_log(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    fputs("[th16-gl] ", stderr);
    vfprintf(stderr, format, args);
    fputc('\n', stderr);
    va_end(args);
}

bool load_gl_functions()
{
    bool ok = true;
#define PORT_GL_LOAD_11(name)                                                       \
    gl::name = (decltype(gl::name))SDL_GL_GetProcAddress("gl" #name);               \
    if (gl::name == NULL)                                                           \
    {                                                                               \
        gl_log("missing GL function gl" #name);                                     \
        ok = false;                                                                 \
    }
#define PORT_GL_LOAD_EXT(name, type) PORT_GL_LOAD_11(name)
    PORT_GL_FUNCS_11(PORT_GL_LOAD_11)
    PORT_GL_FUNCS_EXT(PORT_GL_LOAD_EXT)
#undef PORT_GL_LOAD_11
#undef PORT_GL_LOAD_EXT
    return ok;
}

bool env_flag(const char *name, bool fallback)
{
    const char *value = getenv(name);
    if (value == NULL || value[0] == '\0')
    {
        return fallback;
    }
    return value[0] != '0';
}

double now_seconds()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

struct PortDevice;
struct PortTexture;

// The window side (see d3d9_gl.h).
SDL_Window *g_attached_window = NULL;
SDL_Window *g_own_window = NULL;
bool g_we_initialized_video = false;

// The one device, and the thread its GL context lives on.
PortDevice *g_device = NULL;
std::thread::id g_gl_thread;

bool on_gl_thread()
{
    return g_device != NULL && std::this_thread::get_id() == g_gl_thread;
}

// GL objects released from other threads, deleted by the device thread.
std::mutex g_dead_mutex;
std::vector<GLuint> g_dead_textures;
std::vector<GLuint> g_dead_framebuffers;

} // namespace

// ---------------------------------------------------------------------------
// Pixel formats

uint32_t port_d3d_format_bpp(D3DFORMAT format)
{
    switch (format)
    {
    case D3DFMT_A8R8G8B8:
    case D3DFMT_X8R8G8B8:
    case D3DFMT_A8B8G8R8:
    case D3DFMT_X8B8G8R8:
        return 4;
    case D3DFMT_R8G8B8:
        return 3;
    case D3DFMT_R5G6B5:
    case D3DFMT_X1R5G5B5:
    case D3DFMT_A1R5G5B5:
    case D3DFMT_A4R4G4B4:
    case D3DFMT_X4R4G4B4:
    case D3DFMT_A8R3G3B2:
    case D3DFMT_A8L8:
        return 2;
    case D3DFMT_A8:
    case D3DFMT_R3G3B2:
    case D3DFMT_L8:
        return 1;
    default:
        return 0;
    }
}

static inline uint32_t expand5(uint32_t v)
{
    return (v << 3) | (v >> 2);
}
static inline uint32_t expand6(uint32_t v)
{
    return (v << 2) | (v >> 4);
}
static inline uint32_t expand3(uint32_t v)
{
    return (v << 5) | (v << 2) | (v >> 1);
}
static inline uint32_t argb(uint32_t a, uint32_t r, uint32_t g, uint32_t b)
{
    return (a << 24) | (r << 16) | (g << 8) | b;
}

void port_d3d_decode_pixels(D3DFORMAT format, const uint8_t *src, uint32_t *dst, uint32_t count)
{
    uint32_t i;
    switch (format)
    {
    case D3DFMT_A8R8G8B8:
        memcpy(dst, src, count * 4);
        break;
    case D3DFMT_X8R8G8B8:
        memcpy(dst, src, count * 4);
        for (i = 0; i < count; i++)
        {
            dst[i] |= 0xff000000u;
        }
        break;
    case D3DFMT_A8B8G8R8:
    case D3DFMT_X8B8G8R8:
        for (i = 0; i < count; i++)
        {
            uint32_t v;
            memcpy(&v, src + i * 4, 4);
            v = (v & 0xff00ff00u) | ((v >> 16) & 0xff) | ((v & 0xff) << 16);
            dst[i] = format == D3DFMT_X8B8G8R8 ? v | 0xff000000u : v;
        }
        break;
    case D3DFMT_R8G8B8:
        for (i = 0; i < count; i++)
        {
            dst[i] = argb(0xff, src[i * 3 + 2], src[i * 3 + 1], src[i * 3]);
        }
        break;
    case D3DFMT_R5G6B5:
        for (i = 0; i < count; i++)
        {
            uint32_t v = src[i * 2] | (src[i * 2 + 1] << 8);
            dst[i] = argb(0xff, expand5(v >> 11), expand6((v >> 5) & 63), expand5(v & 31));
        }
        break;
    case D3DFMT_X1R5G5B5:
    case D3DFMT_A1R5G5B5:
        for (i = 0; i < count; i++)
        {
            uint32_t v = src[i * 2] | (src[i * 2 + 1] << 8);
            uint32_t a = format == D3DFMT_X1R5G5B5 || (v & 0x8000) ? 0xff : 0;
            dst[i] = argb(a, expand5((v >> 10) & 31), expand5((v >> 5) & 31), expand5(v & 31));
        }
        break;
    case D3DFMT_A4R4G4B4:
    case D3DFMT_X4R4G4B4:
        for (i = 0; i < count; i++)
        {
            uint32_t v = src[i * 2] | (src[i * 2 + 1] << 8);
            uint32_t a = format == D3DFMT_X4R4G4B4 ? 0xff : (v >> 12) * 17;
            dst[i] = argb(a, ((v >> 8) & 15) * 17, ((v >> 4) & 15) * 17, (v & 15) * 17);
        }
        break;
    case D3DFMT_A8R3G3B2:
        for (i = 0; i < count; i++)
        {
            uint32_t v = src[i * 2] | (src[i * 2 + 1] << 8);
            dst[i] = argb(v >> 8, expand3((v >> 5) & 7), expand3((v >> 2) & 7), (v & 3) * 85);
        }
        break;
    case D3DFMT_R3G3B2:
        for (i = 0; i < count; i++)
        {
            uint32_t v = src[i];
            dst[i] = argb(0xff, expand3((v >> 5) & 7), expand3((v >> 2) & 7), (v & 3) * 85);
        }
        break;
    case D3DFMT_A8:
        for (i = 0; i < count; i++)
        {
            dst[i] = (uint32_t)src[i] << 24;
        }
        break;
    case D3DFMT_L8:
        for (i = 0; i < count; i++)
        {
            dst[i] = argb(0xff, src[i], src[i], src[i]);
        }
        break;
    case D3DFMT_A8L8:
        for (i = 0; i < count; i++)
        {
            dst[i] = argb(src[i * 2 + 1], src[i * 2], src[i * 2], src[i * 2]);
        }
        break;
    default:
        memset(dst, 0, count * 4);
        break;
    }
}

void port_d3d_encode_pixels(D3DFORMAT format, const uint32_t *src, uint8_t *dst, uint32_t count)
{
    uint32_t i;
    for (i = 0; i < count; i++)
    {
        uint32_t c = src[i];
        uint32_t a = c >> 24, r = (c >> 16) & 0xff, g = (c >> 8) & 0xff, b = c & 0xff;
        uint32_t v;
        switch (format)
        {
        case D3DFMT_A8R8G8B8:
        case D3DFMT_X8R8G8B8:
            memcpy(dst + i * 4, &c, 4);
            break;
        case D3DFMT_A8B8G8R8:
        case D3DFMT_X8B8G8R8:
            v = (c & 0xff00ff00u) | (r) | (b << 16);
            memcpy(dst + i * 4, &v, 4);
            break;
        case D3DFMT_R8G8B8:
            dst[i * 3] = b;
            dst[i * 3 + 1] = g;
            dst[i * 3 + 2] = r;
            break;
        case D3DFMT_R5G6B5:
            v = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
            dst[i * 2] = v & 0xff;
            dst[i * 2 + 1] = v >> 8;
            break;
        case D3DFMT_X1R5G5B5:
        case D3DFMT_A1R5G5B5:
            v = (a >= 0x80 || format == D3DFMT_X1R5G5B5 ? 0x8000 : 0) | ((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3);
            dst[i * 2] = v & 0xff;
            dst[i * 2 + 1] = v >> 8;
            break;
        case D3DFMT_A4R4G4B4:
        case D3DFMT_X4R4G4B4:
            v = ((format == D3DFMT_X4R4G4B4 ? 0xf : a >> 4) << 12) | ((r >> 4) << 8) | ((g >> 4) << 4) | (b >> 4);
            dst[i * 2] = v & 0xff;
            dst[i * 2 + 1] = v >> 8;
            break;
        case D3DFMT_A8R3G3B2:
            v = (a << 8) | ((r >> 5) << 5) | ((g >> 5) << 2) | (b >> 6);
            dst[i * 2] = v & 0xff;
            dst[i * 2 + 1] = v >> 8;
            break;
        case D3DFMT_R3G3B2:
            dst[i] = ((r >> 5) << 5) | ((g >> 5) << 2) | (b >> 6);
            break;
        case D3DFMT_A8:
            dst[i] = a;
            break;
        case D3DFMT_L8:
            dst[i] = (r * 77 + g * 150 + b * 29) >> 8;
            break;
        case D3DFMT_A8L8:
            dst[i * 2] = (r * 77 + g * 150 + b * 29) >> 8;
            dst[i * 2 + 1] = a;
            break;
        default:
            break;
        }
    }
}

namespace
{

// ---------------------------------------------------------------------------
// Resources

void rect_union(RECT &acc, bool &any, const RECT &r)
{
    if (!any)
    {
        acc = r;
        any = true;
        return;
    }
    acc.left = r.left < acc.left ? r.left : acc.left;
    acc.top = r.top < acc.top ? r.top : acc.top;
    acc.right = r.right > acc.right ? r.right : acc.right;
    acc.bottom = r.bottom > acc.bottom ? r.bottom : acc.bottom;
}

struct PortSurface final : public IDirect3DSurface9
{
    PortTexture *owner;

    explicit PortSurface(PortTexture *texture) : owner(texture)
    {
    }

    HRESULT QueryInterface(REFIID riid, void **ppvObject) override;
    uint32_t AddRef() override;
    uint32_t Release() override;
    HRESULT GetDevice(IDirect3DDevice9 **ppDevice) override;
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
    HRESULT GetDesc(D3DSURFACE_DESC *pDesc) override;
    HRESULT LockRect(D3DLOCKED_RECT *pLockedRect, const RECT *pRect, DWORD Flags) override;
    HRESULT UnlockRect() override;
};

// A texture with one level. Also stands for the back buffer (is_back_buffer),
// which the game only sees through its surface.
struct PortTexture final : public IDirect3DTexture9
{
    std::atomic<uint32_t> ref_count{1};
    PortSurface surface{this};
    UINT width;
    UINT height;
    D3DFORMAT format;
    DWORD usage;
    D3DPOOL pool;
    uint32_t bpp;
    uint32_t pitch;
    bool render_target;
    bool is_back_buffer = false;

    // Guards pixels, dirty and the lock: other threads lock textures while
    // the device thread uploads them.
    std::recursive_mutex mutex;
    // Level 0 in `format`. Render targets only use it during a lock.
    std::vector<uint8_t> pixels;
    // The part of pixels newer than the GL texture.
    bool dirty = false;
    RECT dirty_rect = {0, 0, 0, 0};
    bool locked = false;
    DWORD lock_flags = 0;
    RECT lock_rect = {0, 0, 0, 0};

    // Device thread only.
    GLuint gl_texture = 0;
    GLuint fbo = 0;

    PortTexture(UINT w, UINT h, D3DFORMAT fmt, DWORD use, D3DPOOL p)
        : width(w), height(h), format(fmt), usage(use), pool(p), bpp(port_d3d_format_bpp(fmt)),
          render_target((use & D3DUSAGE_RENDERTARGET) != 0)
    {
        pitch = (width * bpp + 3) & ~3u;
        if (!render_target)
        {
            pixels.assign((size_t)pitch * height, 0);
        }
    }

    ~PortTexture() override;

    HRESULT QueryInterface(REFIID riid, void **ppvObject) override
    {
        if (ppvObject != NULL)
        {
            *ppvObject = NULL;
        }
        return E_NOINTERFACE;
    }
    uint32_t AddRef() override
    {
        return ++ref_count;
    }
    uint32_t Release() override
    {
        uint32_t count = --ref_count;
        if (count == 0)
        {
            delete this;
        }
        return count;
    }
    HRESULT GetDevice(IDirect3DDevice9 **ppDevice) override;
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
        if (Level != 0 || pDesc == NULL)
        {
            return D3DERR_INVALIDCALL;
        }
        pDesc->Format = format;
        pDesc->Type = D3DRTYPE_SURFACE;
        pDesc->Usage = usage;
        pDesc->Pool = pool;
        pDesc->MultiSampleType = D3DMULTISAMPLE_NONE;
        pDesc->MultiSampleQuality = 0;
        pDesc->Width = width;
        pDesc->Height = height;
        return D3D_OK;
    }
    HRESULT GetSurfaceLevel(UINT Level, IDirect3DSurface9 **ppSurfaceLevel) override
    {
        if (ppSurfaceLevel == NULL)
        {
            return D3DERR_INVALIDCALL;
        }
        if (Level != 0)
        {
            *ppSurfaceLevel = NULL;
            return D3DERR_INVALIDCALL;
        }
        AddRef();
        *ppSurfaceLevel = &surface;
        return D3D_OK;
    }
    HRESULT LockRect(UINT Level, D3DLOCKED_RECT *pLockedRect, const RECT *pRect, DWORD Flags) override;
    HRESULT UnlockRect(UINT Level) override;
    HRESULT AddDirtyRect(const RECT *pDirtyRect) override
    {
        return D3D_OK;
    }

    // Device thread: creates the GL texture and uploads what changed.
    void sync_gl();
    // Device thread: the FBO drawing into this texture.
    GLuint framebuffer();
    // Device thread: reads the GL texture into pixels (render targets).
    void read_back(const RECT &rect);
    // Device thread: uploads rect of pixels to the GL texture.
    void upload(const RECT &rect);
    // Marks rect of pixels as newer than the GL texture.
    void mark_dirty(const RECT &rect)
    {
        rect_union(dirty_rect, dirty, rect);
    }
    void release_gl();
};

uint32_t PortSurface::AddRef()
{
    return owner->AddRef();
}
uint32_t PortSurface::Release()
{
    return owner->Release();
}
HRESULT PortSurface::QueryInterface(REFIID riid, void **ppvObject)
{
    if (ppvObject != NULL)
    {
        *ppvObject = NULL;
    }
    return E_NOINTERFACE;
}
HRESULT PortSurface::GetDesc(D3DSURFACE_DESC *pDesc)
{
    return owner->GetLevelDesc(0, pDesc);
}
HRESULT PortSurface::LockRect(D3DLOCKED_RECT *pLockedRect, const RECT *pRect, DWORD Flags)
{
    return owner->LockRect(0, pLockedRect, pRect, Flags);
}
HRESULT PortSurface::UnlockRect()
{
    return owner->UnlockRect(0);
}

PortSurface *as_surface(IDirect3DSurface9 *surface)
{
    return static_cast<PortSurface *>(surface);
}

struct PortVertexBuffer final : public PortComObject<IDirect3DVertexBuffer9>
{
    std::vector<uint8_t> data;
    DWORD usage;
    DWORD fvf;
    D3DPOOL pool;

    HRESULT GetDevice(IDirect3DDevice9 **ppDevice) override;
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
        if (ppbData == NULL || OffsetToLock > data.size())
        {
            return D3DERR_INVALIDCALL;
        }
        *ppbData = data.data() + OffsetToLock;
        return D3D_OK;
    }
    HRESULT Unlock() override
    {
        return D3D_OK;
    }
    HRESULT GetDesc(D3DVERTEXBUFFER_DESC *pDesc) override
    {
        if (pDesc == NULL)
        {
            return D3DERR_INVALIDCALL;
        }
        pDesc->Format = D3DFMT_VERTEXDATA;
        pDesc->Type = D3DRTYPE_VERTEXBUFFER;
        pDesc->Usage = usage;
        pDesc->Pool = pool;
        pDesc->Size = (UINT)data.size();
        pDesc->FVF = fvf;
        return D3D_OK;
    }
};

// ---------------------------------------------------------------------------
// The fixed-function shader

const char kShaderVs[] = R"(#version 330 core
in vec4 a_pos;
in vec4 a_diffuse;
in vec4 a_specular;
in vec4 a_tex;

uniform mat4 u_wvp;        // world * view * projection (D3D row-vector order)
uniform mat4 u_wv;         // world * view, for the fog distance
uniform mat4 u_tex_matrix; // D3DTS_TEXTURE0
uniform vec4 u_viewport;   // x, y, width, height in render target pixels
uniform ivec4 u_vflags;    // pre-transformed, texture transform, fog mode, range fog
uniform vec4 u_fog;        // start, end, density

out vec4 v_diffuse;
out vec4 v_specular;
out vec2 v_tex;
out float v_fog;

void main()
{
    v_diffuse = a_diffuse;
    v_specular = a_specular;
    v_fog = 1.0;
    if (u_vflags.x != 0)
    {
        // Screen coordinates of the render target, D3D pixel centers on
        // integers: map them through the viewport so GL lands them half a
        // pixel further, on its own centers. 1 / rhw is w, which keeps the
        // interpolation perspective-correct as in D3D.
        float w = a_pos.w != 0.0 ? 1.0 / a_pos.w : 1.0;
        vec2 ndc = (a_pos.xy + 0.5 - u_viewport.xy) * 2.0 / u_viewport.zw - 1.0;
        gl_Position = vec4(ndc, a_pos.z * 2.0 - 1.0, 1.0) * w;
        v_tex = a_tex.xy;
        return;
    }
    vec4 pos = vec4(a_pos.xyz, 1.0);
    vec4 clip = u_wvp * pos;
    // D3D's y points down the screen in window terms; GL's window row 0 is
    // D3D's row 0 here, so y flips. Half-pixel offset, then z from [0, w]
    // to [-w, w].
    gl_Position = vec4(clip.x + clip.w / u_viewport.z, -clip.y + clip.w / u_viewport.w,
                       2.0 * clip.z - clip.w, clip.w);
    if (u_vflags.y != 0)
    {
        // D3D extends 2D texture coordinates to (u, v, 1, 0).
        v_tex = (u_tex_matrix * vec4(a_tex.xy, 1.0, 0.0)).xy;
    }
    else
    {
        v_tex = a_tex.xy;
    }
    if (u_vflags.z != 0)
    {
        vec4 eye = u_wv * pos;
        float d = u_vflags.w != 0 ? length(eye.xyz) : abs(eye.z);
        if (u_vflags.z == 3)
        {
            v_fog = u_fog.y != u_fog.x ? (u_fog.y - d) / (u_fog.y - u_fog.x) : (d < u_fog.y ? 1.0 : 0.0);
        }
        else if (u_vflags.z == 1)
        {
            v_fog = exp(-d * u_fog.z);
        }
        else
        {
            v_fog = exp(-(d * u_fog.z) * (d * u_fog.z));
        }
        v_fog = clamp(v_fog, 0.0, 1.0);
    }
}
)";

const char kShaderFs[] = R"(#version 330 core
in vec4 v_diffuse;
in vec4 v_specular;
in vec2 v_tex;
in float v_fog;

uniform sampler2D u_texture;
uniform ivec4 u_ops;        // color op, alpha op, fog on, -
uniform ivec4 u_color_args; // arg1, arg2, arg0
uniform ivec4 u_alpha_args;
uniform ivec4 u_alpha_test; // enabled, function, reference
uniform vec4 u_tfactor;
uniform vec4 u_fog_color;

out vec4 o_color;

vec4 g_tex;

vec4 arg(int a, vec4 current)
{
    int sel = a & 15;
    vec4 r;
    if (sel == 0) r = v_diffuse;
    else if (sel == 1) r = current;
    else if (sel == 2) r = g_tex;
    else if (sel == 3) r = u_tfactor;
    else if (sel == 4) r = v_specular;
    else r = vec4(0.0);
    if ((a & 16) != 0) r = vec4(1.0) - r;
    if ((a & 32) != 0) r = vec4(r.a);
    return r;
}

vec4 op(int o, vec4 a1, vec4 a2, vec4 a0, vec4 current)
{
    if (o == 2) return a1;
    if (o == 3) return a2;
    if (o == 4) return a1 * a2;
    if (o == 5) return a1 * a2 * 2.0;
    if (o == 6) return a1 * a2 * 4.0;
    if (o == 7) return a1 + a2;
    if (o == 8) return a1 + a2 - 0.5;
    if (o == 9) return (a1 + a2 - 0.5) * 2.0;
    if (o == 10) return a1 - a2;
    if (o == 11) return a1 + a2 - a1 * a2;
    if (o == 12) return mix(a2, a1, v_diffuse.a);
    if (o == 13) return mix(a2, a1, g_tex.a);
    if (o == 14) return mix(a2, a1, u_tfactor.a);
    if (o == 15) return a1 + a2 * (1.0 - g_tex.a);
    if (o == 16) return mix(a2, a1, current.a);
    if (o == 25) return a0 + a1 * a2;
    if (o == 26) return mix(a2, a1, a0);
    return current;
}

void main()
{
    g_tex = texture(u_texture, v_tex);
    vec4 current = v_diffuse;
    vec4 result = current;
    if (u_ops.x != 1)
    {
        vec4 c = op(u_ops.x, arg(u_color_args.x, current), arg(u_color_args.y, current),
                    arg(u_color_args.z, current), current);
        result.rgb = clamp(c.rgb, 0.0, 1.0);
        if (u_ops.y != 1)
        {
            vec4 a = op(u_ops.y, arg(u_alpha_args.x, current), arg(u_alpha_args.y, current),
                        arg(u_alpha_args.z, current), current);
            result.a = clamp(a.a, 0.0, 1.0);
        }
    }
    if (u_alpha_test.x != 0)
    {
        int a = int(floor(result.a * 255.0 + 0.5));
        int ref = u_alpha_test.z;
        int f = u_alpha_test.y;
        bool pass = f == 8 || (f == 2 && a < ref) || (f == 3 && a == ref) || (f == 4 && a <= ref) ||
                    (f == 5 && a > ref) || (f == 6 && a != ref) || (f == 7 && a >= ref);
        if (!pass) discard;
    }
    if (u_ops.z != 0)
    {
        result.rgb = mix(u_fog_color.rgb, result.rgb, v_fog);
    }
    o_color = result;
}
)";

enum
{
    ATTR_POS = 0,
    ATTR_DIFFUSE = 1,
    ATTR_SPECULAR = 2,
    ATTR_TEX = 3,
};

// Device state groups that need GL work before the next draw.
enum : uint32_t
{
    DIRTY_FRAMEBUFFER = 1u << 0,
    DIRTY_VIEWPORT = 1u << 1,
    DIRTY_BLEND = 1u << 2,
    DIRTY_DEPTH = 1u << 3,
    DIRTY_CULL = 1u << 4,
    DIRTY_SAMPLER = 1u << 5,
    DIRTY_TRANSFORM = 1u << 6,
    DIRTY_COLOR_MASK = 1u << 7,
    DIRTY_ALL = 0xffffffffu,
};

GLenum gl_blend_factor(DWORD blend)
{
    switch (blend)
    {
    case D3DBLEND_ZERO:
        return GL_ZERO;
    case D3DBLEND_ONE:
        return GL_ONE;
    case D3DBLEND_SRCCOLOR:
        return GL_SRC_COLOR;
    case D3DBLEND_INVSRCCOLOR:
        return GL_ONE_MINUS_SRC_COLOR;
    case D3DBLEND_SRCALPHA:
        return GL_SRC_ALPHA;
    case D3DBLEND_INVSRCALPHA:
        return GL_ONE_MINUS_SRC_ALPHA;
    case D3DBLEND_DESTALPHA:
        return GL_DST_ALPHA;
    case D3DBLEND_INVDESTALPHA:
        return GL_ONE_MINUS_DST_ALPHA;
    case D3DBLEND_DESTCOLOR:
        return GL_DST_COLOR;
    case D3DBLEND_INVDESTCOLOR:
        return GL_ONE_MINUS_DST_COLOR;
    case D3DBLEND_SRCALPHASAT:
        return GL_SRC_ALPHA_SATURATE;
    case D3DBLEND_BLENDFACTOR:
        return GL_CONSTANT_COLOR;
    case D3DBLEND_INVBLENDFACTOR:
        return GL_ONE_MINUS_CONSTANT_COLOR;
    default:
        return GL_ONE;
    }
}

// D3DBLEND_BOTHSRCALPHA and D3DBLEND_BOTHINVSRCALPHA set both factors.
void gl_blend_factors(DWORD src, DWORD dst, GLenum *gl_src, GLenum *gl_dst)
{
    if (src == D3DBLEND_BOTHSRCALPHA)
    {
        *gl_src = GL_SRC_ALPHA;
        *gl_dst = GL_ONE_MINUS_SRC_ALPHA;
        return;
    }
    if (src == D3DBLEND_BOTHINVSRCALPHA)
    {
        *gl_src = GL_ONE_MINUS_SRC_ALPHA;
        *gl_dst = GL_SRC_ALPHA;
        return;
    }
    *gl_src = gl_blend_factor(src);
    *gl_dst = gl_blend_factor(dst);
}

GLenum gl_blend_op(DWORD op)
{
    switch (op)
    {
    case D3DBLENDOP_SUBTRACT:
        return GL_FUNC_SUBTRACT;
    case D3DBLENDOP_REVSUBTRACT:
        return GL_FUNC_REVERSE_SUBTRACT;
    case D3DBLENDOP_MIN:
        return GL_MIN;
    case D3DBLENDOP_MAX:
        return GL_MAX;
    default:
        return GL_FUNC_ADD;
    }
}

GLenum gl_compare(DWORD func)
{
    switch (func)
    {
    case D3DCMP_NEVER:
        return GL_NEVER;
    case D3DCMP_LESS:
        return GL_LESS;
    case D3DCMP_EQUAL:
        return GL_EQUAL;
    case D3DCMP_LESSEQUAL:
        return GL_LEQUAL;
    case D3DCMP_GREATER:
        return GL_GREATER;
    case D3DCMP_NOTEQUAL:
        return GL_NOTEQUAL;
    case D3DCMP_GREATEREQUAL:
        return GL_GEQUAL;
    default:
        return GL_ALWAYS;
    }
}

GLint gl_address(DWORD address)
{
    switch (address)
    {
    case D3DTADDRESS_MIRROR:
    case D3DTADDRESS_MIRRORONCE:
        return GL_MIRRORED_REPEAT;
    case D3DTADDRESS_CLAMP:
        return GL_CLAMP_TO_EDGE;
    case D3DTADDRESS_BORDER:
        return GL_CLAMP_TO_BORDER;
    default:
        return GL_REPEAT;
    }
}

void matrix_multiply(D3DMATRIX *out, const D3DMATRIX &a, const D3DMATRIX &b)
{
    D3DMATRIX r;
    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            r.m[i][j] = a.m[i][0] * b.m[0][j] + a.m[i][1] * b.m[1][j] + a.m[i][2] * b.m[2][j] + a.m[i][3] * b.m[3][j];
        }
    }
    *out = r;
}

void matrix_identity(D3DMATRIX *m)
{
    memset(m, 0, sizeof(*m));
    m->_11 = m->_22 = m->_33 = m->_44 = 1.0f;
}

float dword_to_float(DWORD v)
{
    float f;
    memcpy(&f, &v, 4);
    return f;
}

void color_to_vec4(D3DCOLOR c, float out[4])
{
    out[0] = ((c >> 16) & 0xff) / 255.0f;
    out[1] = ((c >> 8) & 0xff) / 255.0f;
    out[2] = (c & 0xff) / 255.0f;
    out[3] = (c >> 24) / 255.0f;
}

// ---------------------------------------------------------------------------
// PNG output for frame dumps (TH16_GL_DUMP_DIR): stored deflate blocks, no
// compression, so it needs no zlib.

uint32_t crc32_update(uint32_t crc, const uint8_t *data, size_t size)
{
    static uint32_t table[256];
    static bool ready = false;
    if (!ready)
    {
        for (uint32_t n = 0; n < 256; n++)
        {
            uint32_t c = n;
            for (int k = 0; k < 8; k++)
            {
                c = c & 1 ? 0xedb88320u ^ (c >> 1) : c >> 1;
            }
            table[n] = c;
        }
        ready = true;
    }
    crc = ~crc;
    for (size_t i = 0; i < size; i++)
    {
        crc = table[(crc ^ data[i]) & 0xff] ^ (crc >> 8);
    }
    return ~crc;
}

void png_chunk(FILE *f, const char *type, const std::vector<uint8_t> &data)
{
    uint8_t header[8];
    uint32_t size = (uint32_t)data.size();
    header[0] = size >> 24;
    header[1] = size >> 16;
    header[2] = size >> 8;
    header[3] = size;
    memcpy(header + 4, type, 4);
    fwrite(header, 1, 8, f);
    if (size != 0)
    {
        fwrite(data.data(), 1, size, f);
    }
    uint32_t crc = crc32_update(0, header + 4, 4);
    crc = crc32_update(crc, data.data(), size);
    uint8_t tail[4] = {(uint8_t)(crc >> 24), (uint8_t)(crc >> 16), (uint8_t)(crc >> 8), (uint8_t)crc};
    fwrite(tail, 1, 4, f);
}

// pixels: width x height D3DCOLOR, top row first.
bool write_png(const char *path, const uint32_t *pixels, uint32_t width, uint32_t height)
{
    FILE *f = fopen(path, "wb");
    if (f == NULL)
    {
        return false;
    }
    static const uint8_t signature[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
    fwrite(signature, 1, 8, f);
    std::vector<uint8_t> ihdr = {(uint8_t)(width >> 24), (uint8_t)(width >> 16), (uint8_t)(width >> 8),
                                 (uint8_t)width, (uint8_t)(height >> 24), (uint8_t)(height >> 16),
                                 (uint8_t)(height >> 8), (uint8_t)height, 8, 2, 0, 0, 0};
    png_chunk(f, "IHDR", ihdr);
    std::vector<uint8_t> raw;
    raw.reserve((size_t)height * (width * 3 + 1));
    for (uint32_t y = 0; y < height; y++)
    {
        raw.push_back(0);
        for (uint32_t x = 0; x < width; x++)
        {
            uint32_t c = pixels[(size_t)y * width + x];
            raw.push_back((c >> 16) & 0xff);
            raw.push_back((c >> 8) & 0xff);
            raw.push_back(c & 0xff);
        }
    }
    std::vector<uint8_t> z = {0x78, 0x01};
    size_t pos = 0;
    uint32_t a = 1, b = 0;
    for (uint8_t byte : raw)
    {
        a = (a + byte) % 65521;
        b = (b + a) % 65521;
    }
    do
    {
        size_t n = raw.size() - pos > 65535 ? 65535 : raw.size() - pos;
        bool last = pos + n == raw.size();
        z.push_back(last ? 1 : 0);
        z.push_back(n & 0xff);
        z.push_back(n >> 8);
        z.push_back(~n & 0xff);
        z.push_back((~n >> 8) & 0xff);
        z.insert(z.end(), raw.begin() + pos, raw.begin() + pos + n);
        pos += n;
    } while (pos < raw.size());
    uint32_t adler = (b << 16) | a;
    z.push_back(adler >> 24);
    z.push_back(adler >> 16);
    z.push_back(adler >> 8);
    z.push_back(adler);
    png_chunk(f, "IDAT", z);
    png_chunk(f, "IEND", std::vector<uint8_t>());
    return fclose(f) == 0;
}

// ---------------------------------------------------------------------------
// The device

struct VertexLayout
{
    uint32_t key; // fvf | stride << 32 does not fit; see layout_key
    GLuint vao;
};

struct PortDevice final : public IDirect3DDevice9
{
    std::atomic<uint32_t> ref_count{1};
    IDirect3D9 *d3d;
    D3DPRESENT_PARAMETERS params;
    SDL_Window *window = NULL;
    SDL_GLContext context = NULL;

    PortTexture *back_buffer = NULL;
    IDirect3DSurface9 *render_target = NULL;
    GLuint depth_buffer = 0;
    uint32_t depth_width = 0;
    uint32_t depth_height = 0;

    DWORD render_states[256];
    DWORD stage_states[8][33];
    DWORD sampler_states[8][14];
    D3DMATRIX world, view, projection, texture_matrix;
    D3DVIEWPORT9 viewport;
    DWORD fvf = 0;
    PortVertexBuffer *stream = NULL;
    UINT stream_offset = 0;
    UINT stream_stride = 0;
    PortTexture *texture = NULL;
    uint32_t dirty = DIRTY_ALL;

    // GL objects.
    GLuint program = 0;
    GLuint vertex_buffer = 0;
    size_t vertex_buffer_size = 0;
    size_t vertex_buffer_used = 0;
    GLuint sampler = 0;
    GLuint white_texture = 0;
    std::vector<std::pair<uint64_t, GLuint>> layouts;
    GLuint bound_vao = 0;
    GLuint bound_texture = ~0u;
    bool vsync = false;
    bool pace = true;
    bool pace_allowed = true;
    double next_frame_time = 0.0;
    // Checks that vsync really throttles (it does not on every driver or
    // when the window is hidden): the start of a 60-frame window.
    double vsync_check_start = 0.0;
    uint32_t vsync_check_frames = 0;
    uint32_t frame_count = 0;

    struct Uniforms
    {
        GLint wvp, wv, tex_matrix, viewport, vflags, fog, texture, ops, color_args, alpha_args, alpha_test, tfactor,
            fog_color;
    } loc;
    // The last values sent, to skip redundant glUniform calls.
    struct UniformCache
    {
        D3DMATRIX wvp, wv, tex_matrix;
        float viewport[4], fog[4], tfactor[4], fog_color[4];
        GLint vflags[4], ops[4], color_args[4], alpha_args[4], alpha_test[4];
    } cache;
    bool cache_valid = false;
    D3DMATRIX wvp, wv;
    int depth_clamp = -1;
    bool gl_ready = false;

    // Frame dumps for testing (see NOTES.md).
    std::string dump_dir;
    uint32_t dump_every = 0;
    uint32_t exit_after = 0;
    // TH16_GL_TRACE_FRAME: logs the calls that build that frame.
    uint32_t trace_frame = 0;
    bool tracing() const
    {
        return trace_frame != 0 && frame_count + 1 == trace_frame;
    }

    PortDevice(IDirect3D9 *parent) : d3d(parent)
    {
        d3d->AddRef();
    }
    ~PortDevice() override;

    bool init(SDL_Window *target, const D3DPRESENT_PARAMETERS &pp);
    void reset_state();
    bool create_back_buffer();
    void ensure_depth(uint32_t w, uint32_t h);
    void apply_present_params();
    void delete_dead_objects();
    void apply_state(bool pretransformed);
    void set_uniforms(bool pretransformed);
    GLuint layout_for(DWORD format, UINT stride);
    HRESULT draw(D3DPRIMITIVETYPE type, UINT count, const void *data, UINT stride);
    void target_size(uint32_t *w, uint32_t *h);
    void dump_frame();
    void pace_frame();

    // IUnknown
    HRESULT QueryInterface(REFIID riid, void **ppvObject) override
    {
        if (ppvObject != NULL)
        {
            *ppvObject = NULL;
        }
        return E_NOINTERFACE;
    }
    uint32_t AddRef() override
    {
        return ++ref_count;
    }
    uint32_t Release() override
    {
        uint32_t count = --ref_count;
        if (count == 0)
        {
            delete this;
        }
        return count;
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
        if (ppD3D9 == NULL)
        {
            return D3DERR_INVALIDCALL;
        }
        d3d->AddRef();
        *ppD3D9 = d3d;
        return D3D_OK;
    }
    HRESULT GetDeviceCaps(D3DCAPS9 *pCaps) override
    {
        return d3d->GetDeviceCaps(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, pCaps);
    }
    HRESULT GetDisplayMode(UINT iSwapChain, D3DDISPLAYMODE *pMode) override
    {
        return d3d->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, pMode);
    }
    HRESULT Reset(D3DPRESENT_PARAMETERS *pPresentationParameters) override;
    HRESULT Present(const RECT *pSourceRect, const RECT *pDestRect, HWND hDestWindowOverride,
                    const void *pDirtyRegion) override;
    HRESULT GetBackBuffer(UINT iSwapChain, UINT iBackBuffer, D3DBACKBUFFER_TYPE Type,
                          IDirect3DSurface9 **ppBackBuffer) override
    {
        if (ppBackBuffer == NULL || iSwapChain != 0 || iBackBuffer != 0)
        {
            return D3DERR_INVALIDCALL;
        }
        back_buffer->AddRef();
        *ppBackBuffer = &back_buffer->surface;
        return D3D_OK;
    }
    HRESULT GetRasterStatus(UINT iSwapChain, D3DRASTER_STATUS *pRasterStatus) override
    {
        // The game polls this in full screen to wait for the vertical
        // blank; Present does the waiting here.
        if (pRasterStatus == NULL)
        {
            return D3DERR_INVALIDCALL;
        }
        pRasterStatus->InVBlank = TRUE;
        pRasterStatus->ScanLine = 0;
        return D3D_OK;
    }
    HRESULT CreateTexture(UINT Width, UINT Height, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool,
                          IDirect3DTexture9 **ppTexture, HANDLE *pSharedHandle) override
    {
        return port_d3d_create_texture(this, Width, Height, Usage, Format, Pool, ppTexture);
    }
    HRESULT CreateVertexBuffer(UINT Length, DWORD Usage, DWORD FVF, D3DPOOL Pool,
                               IDirect3DVertexBuffer9 **ppVertexBuffer, HANDLE *pSharedHandle) override
    {
        if (ppVertexBuffer == NULL || Length == 0)
        {
            return D3DERR_INVALIDCALL;
        }
        PortVertexBuffer *vb = new PortVertexBuffer();
        vb->data.assign(Length, 0);
        vb->usage = Usage;
        vb->fvf = FVF;
        vb->pool = Pool;
        *ppVertexBuffer = vb;
        return D3D_OK;
    }
    HRESULT CreateOffscreenPlainSurface(UINT Width, UINT Height, D3DFORMAT Format, D3DPOOL Pool,
                                        IDirect3DSurface9 **ppSurface, HANDLE *pSharedHandle) override
    {
        // A one-level texture's surface; the texture lives as long as it.
        IDirect3DTexture9 *texture = NULL;
        HRESULT hr = port_d3d_create_texture(this, Width, Height, 0, Format, Pool, &texture);
        if (hr != D3D_OK)
        {
            *ppSurface = NULL;
            return hr;
        }
        texture->GetSurfaceLevel(0, ppSurface);
        texture->Release();
        return D3D_OK;
    }
    HRESULT UpdateSurface(IDirect3DSurface9 *pSourceSurface, const RECT *pSourceRect,
                          IDirect3DSurface9 *pDestinationSurface, const POINT *pDestPoint) override;
    HRESULT StretchRect(IDirect3DSurface9 *pSourceSurface, const RECT *pSourceRect, IDirect3DSurface9 *pDestSurface,
                        const RECT *pDestRect, D3DTEXTUREFILTERTYPE Filter) override;
    HRESULT SetRenderTarget(DWORD RenderTargetIndex, IDirect3DSurface9 *pRenderTarget) override;
    HRESULT GetRenderTarget(DWORD RenderTargetIndex, IDirect3DSurface9 **ppRenderTarget) override
    {
        if (ppRenderTarget == NULL || RenderTargetIndex != 0)
        {
            return D3DERR_INVALIDCALL;
        }
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
    HRESULT Clear(DWORD Count, const D3DRECT *pRects, DWORD Flags, D3DCOLOR Color, float Z, DWORD Stencil) override;
    HRESULT SetTransform(D3DTRANSFORMSTATETYPE State, const D3DMATRIX *pMatrix) override
    {
        if (pMatrix == NULL)
        {
            return D3DERR_INVALIDCALL;
        }
        switch ((int)State)
        {
        case D3DTS_VIEW:
            view = *pMatrix;
            break;
        case D3DTS_PROJECTION:
            projection = *pMatrix;
            break;
        case D3DTS_TEXTURE0:
            texture_matrix = *pMatrix;
            break;
        case D3DTS_WORLD:
            world = *pMatrix;
            break;
        default:
            return D3D_OK;
        }
        dirty |= DIRTY_TRANSFORM;
        return D3D_OK;
    }
    HRESULT GetTransform(D3DTRANSFORMSTATETYPE State, D3DMATRIX *pMatrix) override
    {
        if (pMatrix == NULL)
        {
            return D3DERR_INVALIDCALL;
        }
        switch ((int)State)
        {
        case D3DTS_VIEW:
            *pMatrix = view;
            break;
        case D3DTS_PROJECTION:
            *pMatrix = projection;
            break;
        case D3DTS_TEXTURE0:
            *pMatrix = texture_matrix;
            break;
        case D3DTS_WORLD:
            *pMatrix = world;
            break;
        default:
            matrix_identity(pMatrix);
            break;
        }
        return D3D_OK;
    }
    HRESULT SetViewport(const D3DVIEWPORT9 *pViewport) override
    {
        if (pViewport == NULL)
        {
            return D3DERR_INVALIDCALL;
        }
        viewport = *pViewport;
        dirty |= DIRTY_VIEWPORT;
        return D3D_OK;
    }
    HRESULT GetViewport(D3DVIEWPORT9 *pViewport) override
    {
        if (pViewport == NULL)
        {
            return D3DERR_INVALIDCALL;
        }
        *pViewport = viewport;
        return D3D_OK;
    }
    HRESULT SetRenderState(D3DRENDERSTATETYPE State, DWORD Value) override
    {
        if ((DWORD)State >= 256)
        {
            return D3DERR_INVALIDCALL;
        }
        render_states[State] = Value;
        switch (State)
        {
        case D3DRS_ZENABLE:
        case D3DRS_ZWRITEENABLE:
        case D3DRS_ZFUNC:
            dirty |= DIRTY_DEPTH;
            break;
        case D3DRS_ALPHABLENDENABLE:
        case D3DRS_SRCBLEND:
        case D3DRS_DESTBLEND:
        case D3DRS_BLENDOP:
        case D3DRS_SEPARATEALPHABLENDENABLE:
        case D3DRS_SRCBLENDALPHA:
        case D3DRS_DESTBLENDALPHA:
        case D3DRS_BLENDOPALPHA:
            dirty |= DIRTY_BLEND;
            break;
        case D3DRS_CULLMODE:
            dirty |= DIRTY_CULL;
            break;
        case D3DRS_COLORWRITEENABLE:
            dirty |= DIRTY_COLOR_MASK;
            break;
        default:
            break;
        }
        return D3D_OK;
    }
    HRESULT GetRenderState(D3DRENDERSTATETYPE State, DWORD *pValue) override
    {
        if ((DWORD)State >= 256 || pValue == NULL)
        {
            return D3DERR_INVALIDCALL;
        }
        *pValue = render_states[State];
        return D3D_OK;
    }
    HRESULT GetTexture(DWORD Stage, IDirect3DBaseTexture9 **ppTexture) override
    {
        if (ppTexture == NULL)
        {
            return D3DERR_INVALIDCALL;
        }
        *ppTexture = Stage == 0 ? texture : NULL;
        if (*ppTexture != NULL)
        {
            (*ppTexture)->AddRef();
        }
        return D3D_OK;
    }
    HRESULT SetTexture(DWORD Stage, IDirect3DBaseTexture9 *pTexture) override
    {
        if (Stage != 0)
        {
            if (pTexture != NULL)
            {
                PORT_UNIMPLEMENTED();
            }
            return D3D_OK;
        }
        PortTexture *t = static_cast<PortTexture *>(static_cast<IDirect3DTexture9 *>(pTexture));
        if (t == texture)
        {
            return D3D_OK;
        }
        if (t != NULL)
        {
            t->AddRef();
        }
        if (texture != NULL)
        {
            texture->Release();
        }
        texture = t;
        return D3D_OK;
    }
    HRESULT GetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD *pValue) override
    {
        if (Stage >= 8 || (DWORD)Type >= 33 || pValue == NULL)
        {
            return D3DERR_INVALIDCALL;
        }
        *pValue = stage_states[Stage][Type];
        return D3D_OK;
    }
    HRESULT SetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD Value) override
    {
        if (Stage >= 8 || (DWORD)Type >= 33)
        {
            return D3DERR_INVALIDCALL;
        }
        stage_states[Stage][Type] = Value;
        if (Stage != 0 && Type == D3DTSS_COLOROP && Value != D3DTOP_DISABLE)
        {
            PORT_UNIMPLEMENTED();
        }
        return D3D_OK;
    }
    HRESULT GetSamplerState(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD *pValue) override
    {
        if (Sampler >= 8 || (DWORD)Type >= 14 || pValue == NULL)
        {
            return D3DERR_INVALIDCALL;
        }
        *pValue = sampler_states[Sampler][Type];
        return D3D_OK;
    }
    HRESULT SetSamplerState(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD Value) override
    {
        if (Sampler >= 8 || (DWORD)Type >= 14)
        {
            return D3DERR_INVALIDCALL;
        }
        sampler_states[Sampler][Type] = Value;
        if (Sampler == 0)
        {
            dirty |= DIRTY_SAMPLER;
        }
        return D3D_OK;
    }
    HRESULT DrawPrimitive(D3DPRIMITIVETYPE PrimitiveType, UINT StartVertex, UINT PrimitiveCount) override
    {
        if (stream == NULL || stream_stride == 0)
        {
            return D3DERR_INVALIDCALL;
        }
        size_t start = stream_offset + (size_t)StartVertex * stream_stride;
        if (start >= stream->data.size())
        {
            return D3DERR_INVALIDCALL;
        }
        return draw(PrimitiveType, PrimitiveCount, stream->data.data() + start, stream_stride);
    }
    HRESULT DrawPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType, UINT PrimitiveCount, const void *pVertexStreamZeroData,
                            UINT VertexStreamZeroStride) override
    {
        // As in D3D, drawing from user memory unbinds stream 0.
        if (stream != NULL)
        {
            stream->Release();
            stream = NULL;
        }
        return draw(PrimitiveType, PrimitiveCount, pVertexStreamZeroData, VertexStreamZeroStride);
    }
    HRESULT SetFVF(DWORD FVF) override
    {
        fvf = FVF;
        return D3D_OK;
    }
    HRESULT GetFVF(DWORD *pFVF) override
    {
        if (pFVF == NULL)
        {
            return D3DERR_INVALIDCALL;
        }
        *pFVF = fvf;
        return D3D_OK;
    }
    HRESULT SetVertexShader(IDirect3DVertexShader9 *pShader) override
    {
        if (pShader != NULL)
        {
            PORT_UNIMPLEMENTED();
        }
        return D3D_OK;
    }
    HRESULT SetStreamSource(UINT StreamNumber, IDirect3DVertexBuffer9 *pStreamData, UINT OffsetInBytes,
                            UINT Stride) override
    {
        if (StreamNumber != 0)
        {
            PORT_UNIMPLEMENTED();
            return D3D_OK;
        }
        PortVertexBuffer *vb = static_cast<PortVertexBuffer *>(pStreamData);
        if (vb != NULL)
        {
            vb->AddRef();
        }
        if (stream != NULL)
        {
            stream->Release();
        }
        stream = vb;
        stream_offset = OffsetInBytes;
        stream_stride = Stride;
        return D3D_OK;
    }
    HRESULT SetPixelShader(IDirect3DPixelShader9 *pShader) override
    {
        if (pShader != NULL)
        {
            PORT_UNIMPLEMENTED();
        }
        return D3D_OK;
    }
};

HRESULT PortTexture::GetDevice(IDirect3DDevice9 **ppDevice)
{
    if (ppDevice == NULL || g_device == NULL)
    {
        return D3DERR_INVALIDCALL;
    }
    g_device->AddRef();
    *ppDevice = g_device;
    return D3D_OK;
}

HRESULT PortSurface::GetDevice(IDirect3DDevice9 **ppDevice)
{
    return owner->GetDevice(ppDevice);
}

HRESULT PortVertexBuffer::GetDevice(IDirect3DDevice9 **ppDevice)
{
    if (ppDevice == NULL || g_device == NULL)
    {
        return D3DERR_INVALIDCALL;
    }
    g_device->AddRef();
    *ppDevice = g_device;
    return D3D_OK;
}

PortTexture::~PortTexture()
{
    release_gl();
}

void PortTexture::release_gl()
{
    if (gl_texture == 0 && fbo == 0)
    {
        return;
    }
    if (g_device == NULL)
    {
        // The context is gone and took the objects with it.
    }
    else if (on_gl_thread())
    {
        if (fbo != 0)
        {
            gl::DeleteFramebuffers(1, &fbo);
            g_device->dirty |= DIRTY_FRAMEBUFFER;
        }
        if (gl_texture != 0)
        {
            if (g_device->bound_texture == gl_texture)
            {
                g_device->bound_texture = ~0u;
            }
            gl::DeleteTextures(1, &gl_texture);
        }
    }
    else
    {
        std::lock_guard<std::mutex> guard(g_dead_mutex);
        if (fbo != 0)
        {
            g_dead_framebuffers.push_back(fbo);
        }
        if (gl_texture != 0)
        {
            g_dead_textures.push_back(gl_texture);
        }
    }
    gl_texture = 0;
    fbo = 0;
}

void PortTexture::sync_gl()
{
    if (gl_texture == 0)
    {
        gl::GenTextures(1, &gl_texture);
        gl::BindTexture(GL_TEXTURE_2D, gl_texture);
        g_device->bound_texture = ~0u;
        gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
        gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        gl::TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
        if (!render_target)
        {
            std::lock_guard<std::recursive_mutex> guard(mutex);
            dirty_rect = {0, 0, (LONG)width, (LONG)height};
            dirty = true;
        }
        else
        {
            // D3D leaves new render targets undefined; start them black
            // and transparent.
            std::vector<uint32_t> zero((size_t)width * height, 0);
            gl::PixelStorei(GL_UNPACK_ROW_LENGTH, 0);
            gl::PixelStorei(GL_UNPACK_ALIGNMENT, 4);
            gl::TexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV,
                              zero.data());
        }
    }
    if (dirty && !render_target)
    {
        std::lock_guard<std::recursive_mutex> guard(mutex);
        if (dirty && !locked)
        {
            upload(dirty_rect);
            dirty = false;
        }
    }
}

void PortTexture::upload(const RECT &rect)
{
    LONG w = rect.right - rect.left;
    LONG h = rect.bottom - rect.top;
    if (w <= 0 || h <= 0)
    {
        return;
    }
    gl::BindTexture(GL_TEXTURE_2D, gl_texture);
    g_device->bound_texture = ~0u;
    gl::PixelStorei(GL_UNPACK_ALIGNMENT, 4);
    const uint8_t *src = pixels.data() + (size_t)rect.top * pitch + (size_t)rect.left * bpp;
    if (format == D3DFMT_A8R8G8B8 && pitch % 4 == 0)
    {
        gl::PixelStorei(GL_UNPACK_ROW_LENGTH, pitch / 4);
        gl::TexSubImage2D(GL_TEXTURE_2D, 0, rect.left, rect.top, w, h, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, src);
        gl::PixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        return;
    }
    std::vector<uint32_t> converted((size_t)w * h);
    for (LONG y = 0; y < h; y++)
    {
        port_d3d_decode_pixels(format, src + (size_t)y * pitch, converted.data() + (size_t)y * w, w);
    }
    gl::PixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    gl::TexSubImage2D(GL_TEXTURE_2D, 0, rect.left, rect.top, w, h, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV,
                      converted.data());
}

GLuint PortTexture::framebuffer()
{
    if (fbo != 0)
    {
        return fbo;
    }
    sync_gl();
    g_device->ensure_depth(width, height);
    gl::GenFramebuffers(1, &fbo);
    gl::BindFramebuffer(GL_FRAMEBUFFER, fbo);
    gl::FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gl_texture, 0);
    gl::FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, g_device->depth_buffer);
    GLenum status = gl::CheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        gl_log("framebuffer for %ux%u texture incomplete (0x%x)", width, height, status);
    }
    g_device->dirty |= DIRTY_FRAMEBUFFER;
    return fbo;
}

void PortTexture::read_back(const RECT &rect)
{
    LONG w = rect.right - rect.left;
    LONG h = rect.bottom - rect.top;
    pixels.resize((size_t)pitch * height);
    if (w <= 0 || h <= 0)
    {
        return;
    }
    gl::BindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer());
    gl::ReadBuffer(GL_COLOR_ATTACHMENT0);
    gl::PixelStorei(GL_PACK_ALIGNMENT, 4);
    gl::PixelStorei(GL_PACK_ROW_LENGTH, pitch / 4);
    gl::ReadPixels(rect.left, rect.top, w, h, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV,
                   pixels.data() + (size_t)rect.top * pitch + (size_t)rect.left * 4);
    gl::PixelStorei(GL_PACK_ROW_LENGTH, 0);
    g_device->dirty |= DIRTY_FRAMEBUFFER;
}

HRESULT PortTexture::LockRect(UINT Level, D3DLOCKED_RECT *pLockedRect, const RECT *pRect, DWORD Flags)
{
    if (Level != 0 || pLockedRect == NULL || bpp == 0)
    {
        return D3DERR_INVALIDCALL;
    }
    RECT r = pRect != NULL ? *pRect : RECT{0, 0, (LONG)width, (LONG)height};
    if (r.left < 0 || r.top < 0 || r.right > (LONG)width || r.bottom > (LONG)height || r.left > r.right ||
        r.top > r.bottom)
    {
        return D3DERR_INVALIDCALL;
    }
    mutex.lock();
    if (locked)
    {
        mutex.unlock();
        return D3DERR_INVALIDCALL;
    }
    if (render_target)
    {
        if (!on_gl_thread())
        {
            mutex.unlock();
            return D3DERR_INVALIDCALL;
        }
        read_back(r);
    }
    locked = true;
    lock_flags = Flags;
    lock_rect = r;
    pLockedRect->Pitch = pitch;
    pLockedRect->pBits = pixels.data() + (size_t)r.top * pitch + (size_t)r.left * bpp;
    // The mutex stays held until UnlockRect.
    return D3D_OK;
}

HRESULT PortTexture::UnlockRect(UINT Level)
{
    if (Level != 0 || !locked)
    {
        return D3DERR_INVALIDCALL;
    }
    locked = false;
    if (!(lock_flags & D3DLOCK_READONLY))
    {
        if (render_target)
        {
            if (on_gl_thread())
            {
                upload(lock_rect);
            }
        }
        else
        {
            mark_dirty(lock_rect);
        }
    }
    mutex.unlock();
    return D3D_OK;
}

// ---------------------------------------------------------------------------

PortDevice::~PortDevice()
{
    if (texture != NULL)
    {
        texture->Release();
        texture = NULL;
    }
    if (stream != NULL)
    {
        stream->Release();
        stream = NULL;
    }
    if (render_target != NULL)
    {
        render_target->Release();
        render_target = NULL;
    }
    if (back_buffer != NULL)
    {
        back_buffer->release_gl();
        back_buffer->Release();
        back_buffer = NULL;
    }
    if (context != NULL && gl_ready)
    {
        delete_dead_objects();
        for (auto &layout : layouts)
        {
            gl::DeleteVertexArrays(1, &layout.second);
        }
        gl::DeleteBuffers(1, &vertex_buffer);
        gl::DeleteSamplers(1, &sampler);
        gl::DeleteTextures(1, &white_texture);
        gl::DeleteRenderbuffers(1, &depth_buffer);
        gl::DeleteProgram(program);
    }
    if (context != NULL)
    {
        SDL_GL_MakeCurrent(window, NULL);
        SDL_GL_DeleteContext(context);
    }
    if (g_device == this)
    {
        g_device = NULL;
    }
    if (g_own_window != NULL)
    {
        SDL_DestroyWindow(g_own_window);
        g_own_window = NULL;
    }
    if (g_we_initialized_video)
    {
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        g_we_initialized_video = false;
    }
    d3d->Release();
}

GLuint compile_shader(GLenum type, const char *source)
{
    GLuint shader = gl::CreateShader(type);
    gl::ShaderSource(shader, 1, &source, NULL);
    gl::CompileShader(shader);
    GLint ok = 0;
    gl::GetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok)
    {
        char log[4096];
        gl::GetShaderInfoLog(shader, sizeof(log), NULL, log);
        gl_log("shader compile error: %s", log);
        gl::DeleteShader(shader);
        return 0;
    }
    return shader;
}

bool PortDevice::init(SDL_Window *target, const D3DPRESENT_PARAMETERS &pp)
{
    window = target;
    params = pp;
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
    context = SDL_GL_CreateContext(window);
    if (context == NULL)
    {
        gl_log("cannot create an OpenGL 3.3 core context: %s", SDL_GetError());
        return false;
    }
    SDL_GL_MakeCurrent(window, context);
    if (!load_gl_functions())
    {
        return false;
    }
    gl_ready = true;
    gl_log("OpenGL %s, %s", (const char *)gl::GetString(GL_VERSION), (const char *)gl::GetString(GL_RENDERER));

    GLuint vs = compile_shader(GL_VERTEX_SHADER, kShaderVs);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, kShaderFs);
    if (vs == 0 || fs == 0)
    {
        return false;
    }
    program = gl::CreateProgram();
    gl::AttachShader(program, vs);
    gl::AttachShader(program, fs);
    gl::BindAttribLocation(program, ATTR_POS, "a_pos");
    gl::BindAttribLocation(program, ATTR_DIFFUSE, "a_diffuse");
    gl::BindAttribLocation(program, ATTR_SPECULAR, "a_specular");
    gl::BindAttribLocation(program, ATTR_TEX, "a_tex");
    gl::BindFragDataLocation(program, 0, "o_color");
    gl::LinkProgram(program);
    gl::DeleteShader(vs);
    gl::DeleteShader(fs);
    GLint linked = 0;
    gl::GetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked)
    {
        char log[4096];
        gl::GetProgramInfoLog(program, sizeof(log), NULL, log);
        gl_log("shader link error: %s", log);
        return false;
    }
    gl::UseProgram(program);
    loc.wvp = gl::GetUniformLocation(program, "u_wvp");
    loc.wv = gl::GetUniformLocation(program, "u_wv");
    loc.tex_matrix = gl::GetUniformLocation(program, "u_tex_matrix");
    loc.viewport = gl::GetUniformLocation(program, "u_viewport");
    loc.vflags = gl::GetUniformLocation(program, "u_vflags");
    loc.fog = gl::GetUniformLocation(program, "u_fog");
    loc.texture = gl::GetUniformLocation(program, "u_texture");
    loc.ops = gl::GetUniformLocation(program, "u_ops");
    loc.color_args = gl::GetUniformLocation(program, "u_color_args");
    loc.alpha_args = gl::GetUniformLocation(program, "u_alpha_args");
    loc.alpha_test = gl::GetUniformLocation(program, "u_alpha_test");
    loc.tfactor = gl::GetUniformLocation(program, "u_tfactor");
    loc.fog_color = gl::GetUniformLocation(program, "u_fog_color");
    gl::Uniform1i(loc.texture, 0);

    vertex_buffer_size = 8u << 20;
    gl::GenBuffers(1, &vertex_buffer);
    gl::BindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
    gl::BufferData(GL_ARRAY_BUFFER, vertex_buffer_size, NULL, GL_STREAM_DRAW);
    vertex_buffer_used = 0;

    gl::GenSamplers(1, &sampler);
    gl::BindSampler(0, sampler);
    gl::ActiveTexture(GL_TEXTURE0);

    uint32_t white = 0xffffffffu;
    gl::GenTextures(1, &white_texture);
    gl::BindTexture(GL_TEXTURE_2D, white_texture);
    gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    gl::TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, &white);

    gl::GenRenderbuffers(1, &depth_buffer);

    // Defaults for attributes the vertex format leaves out.
    gl::VertexAttrib4f(ATTR_DIFFUSE, 1.0f, 1.0f, 1.0f, 1.0f);
    gl::VertexAttrib4f(ATTR_SPECULAR, 0.0f, 0.0f, 0.0f, 0.0f);
    gl::VertexAttrib4f(ATTR_TEX, 0.0f, 0.0f, 0.0f, 1.0f);

    const char *dump = getenv("TH16_GL_DUMP_DIR");
    if (dump != NULL && dump[0] != '\0')
    {
        dump_dir = dump;
        const char *every = getenv("TH16_GL_DUMP_EVERY");
        dump_every = every != NULL ? (uint32_t)atoi(every) : 60;
    }
    const char *trace_env = getenv("TH16_GL_TRACE_FRAME");
    trace_frame = trace_env != NULL ? (uint32_t)atoi(trace_env) : 0;
    const char *exit_env = getenv("TH16_GL_EXIT_AFTER");
    exit_after = exit_env != NULL ? (uint32_t)atoi(exit_env) : 0;

    if (!create_back_buffer())
    {
        return false;
    }
    reset_state();
    apply_present_params();
    return true;
}

bool PortDevice::create_back_buffer()
{
    if (back_buffer != NULL)
    {
        if (render_target == &back_buffer->surface)
        {
            render_target->Release();
            render_target = NULL;
        }
        back_buffer->release_gl();
        back_buffer->Release();
        back_buffer = NULL;
    }
    if (render_target != NULL)
    {
        render_target->Release();
        render_target = NULL;
    }
    UINT w = params.BackBufferWidth != 0 ? params.BackBufferWidth : 640;
    UINT h = params.BackBufferHeight != 0 ? params.BackBufferHeight : 480;
    if (params.BackBufferWidth == 0 && window != NULL)
    {
        int ww, wh;
        SDL_GetWindowSize(window, &ww, &wh);
        w = ww;
        h = wh;
        params.BackBufferWidth = w;
        params.BackBufferHeight = h;
    }
    if (params.BackBufferFormat == D3DFMT_UNKNOWN)
    {
        params.BackBufferFormat = D3DFMT_X8R8G8B8;
    }
    back_buffer = new PortTexture(w, h, D3DFMT_A8R8G8B8, D3DUSAGE_RENDERTARGET, D3DPOOL_DEFAULT);
    back_buffer->is_back_buffer = true;
    back_buffer->format = params.BackBufferFormat == D3DFMT_X8R8G8B8 ? D3DFMT_X8R8G8B8 : D3DFMT_A8R8G8B8;
    back_buffer->framebuffer();
    back_buffer->AddRef();
    render_target = &back_buffer->surface;
    viewport = {0, 0, w, h, 0.0f, 1.0f};
    dirty = DIRTY_ALL;
    // A fresh frame starts black, as after Reset in D3D.
    gl::BindFramebuffer(GL_FRAMEBUFFER, back_buffer->fbo);
    gl::Disable(GL_SCISSOR_TEST);
    gl::ColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    gl::DepthMask(GL_TRUE);
    gl::ClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    gl::ClearDepth(1.0);
    gl::Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    return true;
}

void PortDevice::ensure_depth(uint32_t w, uint32_t h)
{
    if (w <= depth_width && h <= depth_height)
    {
        return;
    }
    depth_width = w > depth_width ? w : depth_width;
    depth_height = h > depth_height ? h : depth_height;
    // Reallocating the storage keeps every FBO's attachment pointing at
    // it: all render targets share one depth buffer, as with D3D's single
    // auto depth stencil surface.
    gl::BindRenderbuffer(GL_RENDERBUFFER, depth_buffer);
    gl::RenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, depth_width, depth_height);
}

void PortDevice::reset_state()
{
    memset(render_states, 0, sizeof(render_states));
    render_states[D3DRS_ZENABLE] = params.EnableAutoDepthStencil ? D3DZB_TRUE : D3DZB_FALSE;
    render_states[D3DRS_FILLMODE] = D3DFILL_SOLID;
    render_states[D3DRS_SHADEMODE] = D3DSHADE_GOURAUD;
    render_states[D3DRS_ZWRITEENABLE] = TRUE;
    render_states[D3DRS_LASTPIXEL] = TRUE;
    render_states[D3DRS_SRCBLEND] = D3DBLEND_ONE;
    render_states[D3DRS_DESTBLEND] = D3DBLEND_ZERO;
    render_states[D3DRS_CULLMODE] = D3DCULL_CCW;
    render_states[D3DRS_ZFUNC] = D3DCMP_LESSEQUAL;
    render_states[D3DRS_ALPHAFUNC] = D3DCMP_ALWAYS;
    render_states[D3DRS_FOGEND] = 0x3f800000;     // 1.0f
    render_states[D3DRS_FOGDENSITY] = 0x3f800000; // 1.0f
    render_states[D3DRS_TEXTUREFACTOR] = 0xffffffff;
    render_states[D3DRS_CLIPPING] = TRUE;
    render_states[D3DRS_LIGHTING] = TRUE;
    render_states[D3DRS_COLORVERTEX] = TRUE;
    render_states[D3DRS_COLORWRITEENABLE] = 0xf;
    render_states[D3DRS_BLENDOP] = D3DBLENDOP_ADD;
    render_states[D3DRS_SRCBLENDALPHA] = D3DBLEND_ONE;
    render_states[D3DRS_DESTBLENDALPHA] = D3DBLEND_ZERO;
    render_states[D3DRS_BLENDOPALPHA] = D3DBLENDOP_ADD;
    memset(stage_states, 0, sizeof(stage_states));
    for (int i = 0; i < 8; i++)
    {
        stage_states[i][D3DTSS_COLOROP] = i == 0 ? D3DTOP_MODULATE : D3DTOP_DISABLE;
        stage_states[i][D3DTSS_ALPHAOP] = i == 0 ? D3DTOP_SELECTARG1 : D3DTOP_DISABLE;
        stage_states[i][D3DTSS_COLORARG1] = D3DTA_TEXTURE;
        stage_states[i][D3DTSS_COLORARG2] = D3DTA_CURRENT;
        stage_states[i][D3DTSS_ALPHAARG1] = D3DTA_TEXTURE;
        stage_states[i][D3DTSS_ALPHAARG2] = D3DTA_CURRENT;
        stage_states[i][D3DTSS_COLORARG0] = D3DTA_CURRENT;
        stage_states[i][D3DTSS_ALPHAARG0] = D3DTA_CURRENT;
        stage_states[i][D3DTSS_RESULTARG] = D3DTA_CURRENT;
        stage_states[i][D3DTSS_TEXCOORDINDEX] = i;
    }
    memset(sampler_states, 0, sizeof(sampler_states));
    for (int i = 0; i < 8; i++)
    {
        sampler_states[i][D3DSAMP_ADDRESSU] = D3DTADDRESS_WRAP;
        sampler_states[i][D3DSAMP_ADDRESSV] = D3DTADDRESS_WRAP;
        sampler_states[i][D3DSAMP_ADDRESSW] = D3DTADDRESS_WRAP;
        sampler_states[i][D3DSAMP_MAGFILTER] = D3DTEXF_POINT;
        sampler_states[i][D3DSAMP_MINFILTER] = D3DTEXF_POINT;
        sampler_states[i][D3DSAMP_MIPFILTER] = D3DTEXF_NONE;
        sampler_states[i][D3DSAMP_MAXANISOTROPY] = 1;
    }
    matrix_identity(&world);
    matrix_identity(&view);
    matrix_identity(&projection);
    matrix_identity(&texture_matrix);
    matrix_identity(&wvp);
    matrix_identity(&wv);
    fvf = 0;
    if (texture != NULL)
    {
        texture->Release();
        texture = NULL;
    }
    if (stream != NULL)
    {
        stream->Release();
        stream = NULL;
    }
    dirty = DIRTY_ALL;
    cache_valid = false;
}

void PortDevice::apply_present_params()
{
    if (window == NULL)
    {
        return;
    }
    // Exclusive full screen becomes a borderless full-screen window.
    Uint32 flags = SDL_GetWindowFlags(window);
    bool fullscreen = (flags & SDL_WINDOW_FULLSCREEN) != 0;
    if (!params.Windowed != fullscreen)
    {
        SDL_SetWindowFullscreen(window, params.Windowed ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
    }
    // Vsync paces the game only if the display runs at 60 Hz; otherwise
    // Present waits on a 60 Hz timer (pace_frame).
    bool want_vsync = params.PresentationInterval != D3DPRESENT_INTERVAL_IMMEDIATE;
    int display = SDL_GetWindowDisplayIndex(window);
    SDL_DisplayMode mode;
    bool display_60 = display >= 0 && SDL_GetCurrentDisplayMode(display, &mode) == 0 && mode.refresh_rate >= 59 &&
                      mode.refresh_rate <= 61;
    vsync = want_vsync && display_60 && env_flag("TH16_GL_VSYNC", true);
    if (SDL_GL_SetSwapInterval(vsync ? 1 : 0) != 0)
    {
        vsync = false;
    }
    pace_allowed = want_vsync && env_flag("TH16_GL_PACE", true);
    pace = pace_allowed && !vsync;
    next_frame_time = 0.0;
    vsync_check_frames = 0;
}

void PortDevice::delete_dead_objects()
{
    std::lock_guard<std::mutex> guard(g_dead_mutex);
    if (!g_dead_textures.empty())
    {
        for (GLuint t : g_dead_textures)
        {
            if (bound_texture == t)
            {
                bound_texture = ~0u;
            }
        }
        gl::DeleteTextures((GLsizei)g_dead_textures.size(), g_dead_textures.data());
        g_dead_textures.clear();
    }
    if (!g_dead_framebuffers.empty())
    {
        gl::DeleteFramebuffers((GLsizei)g_dead_framebuffers.size(), g_dead_framebuffers.data());
        g_dead_framebuffers.clear();
        dirty |= DIRTY_FRAMEBUFFER;
    }
}

void PortDevice::target_size(uint32_t *w, uint32_t *h)
{
    PortTexture *t = as_surface(render_target)->owner;
    *w = t->width;
    *h = t->height;
}

uint64_t layout_key(DWORD format, UINT stride)
{
    return ((uint64_t)stride << 32) | format;
}

GLuint PortDevice::layout_for(DWORD format, UINT stride)
{
    uint64_t key = layout_key(format, stride);
    for (auto &layout : layouts)
    {
        if (layout.first == key)
        {
            return layout.second;
        }
    }
    GLuint vao;
    gl::GenVertexArrays(1, &vao);
    gl::BindVertexArray(vao);
    gl::BindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
    uintptr_t offset = 0;
    if ((format & 0xe) == D3DFVF_XYZRHW)
    {
        gl::EnableVertexAttribArray(ATTR_POS);
        gl::VertexAttribPointer(ATTR_POS, 4, GL_FLOAT, GL_FALSE, stride, (const void *)offset);
        offset += 16;
    }
    else
    {
        gl::EnableVertexAttribArray(ATTR_POS);
        gl::VertexAttribPointer(ATTR_POS, 3, GL_FLOAT, GL_FALSE, stride, (const void *)offset);
        offset += 12;
        // D3DFVF_XYZB1..B5: blend weights, unused here.
        if ((format & 0xe) >= 0x6)
        {
            offset += (((format & 0xe) - 0x6) / 2 + 1) * 4;
        }
    }
    if (format & D3DFVF_NORMAL)
    {
        offset += 12;
    }
    if (format & D3DFVF_PSIZE)
    {
        offset += 4;
    }
    if (format & D3DFVF_DIFFUSE)
    {
        gl::EnableVertexAttribArray(ATTR_DIFFUSE);
        gl::VertexAttribPointer(ATTR_DIFFUSE, GL_BGRA, GL_UNSIGNED_BYTE, GL_TRUE, stride, (const void *)offset);
        offset += 4;
    }
    if (format & D3DFVF_SPECULAR)
    {
        gl::EnableVertexAttribArray(ATTR_SPECULAR);
        gl::VertexAttribPointer(ATTR_SPECULAR, GL_BGRA, GL_UNSIGNED_BYTE, GL_TRUE, stride, (const void *)offset);
        offset += 4;
    }
    if (((format >> 8) & 0xf) != 0)
    {
        // Coordinate set 0's size: D3DFVF_TEXCOORDSIZEn bits, 0 means 2.
        static const int sizes[4] = {2, 3, 4, 1};
        int size = sizes[(format >> 16) & 3];
        gl::EnableVertexAttribArray(ATTR_TEX);
        gl::VertexAttribPointer(ATTR_TEX, size, GL_FLOAT, GL_FALSE, stride, (const void *)offset);
    }
    layouts.push_back(std::make_pair(key, vao));
    bound_vao = vao;
    return vao;
}

// Sends the uniforms that changed since the last draw.
void PortDevice::set_uniforms(bool pretransformed)
{
    UniformCache u;
    memset(&u, 0, sizeof(u));
    u.wvp = wvp;
    u.wv = wv;
    u.tex_matrix = texture_matrix;
    u.viewport[0] = (float)viewport.X;
    u.viewport[1] = (float)viewport.Y;
    u.viewport[2] = (float)(viewport.Width != 0 ? viewport.Width : 1);
    u.viewport[3] = (float)(viewport.Height != 0 ? viewport.Height : 1);

    DWORD ttf = stage_states[0][D3DTSS_TEXTURETRANSFORMFLAGS];
    bool fog_on = render_states[D3DRS_FOGENABLE] && !pretransformed;
    DWORD fog_mode = render_states[D3DRS_FOGTABLEMODE] != D3DFOG_NONE ? render_states[D3DRS_FOGTABLEMODE]
                                                                      : render_states[D3DRS_FOGVERTEXMODE];
    if (fog_mode == D3DFOG_NONE)
    {
        fog_on = false;
    }
    u.vflags[0] = pretransformed ? 1 : 0;
    // D3D ignores the texture transform for pre-transformed vertices and
    // for D3DTTFF_COUNT1.
    u.vflags[1] = !pretransformed && (ttf & 0xff) >= D3DTTFF_COUNT2 ? 1 : 0;
    u.vflags[2] = fog_on ? (int)fog_mode : 0;
    u.vflags[3] = render_states[D3DRS_RANGEFOGENABLE] ? 1 : 0;
    u.fog[0] = dword_to_float(render_states[D3DRS_FOGSTART]);
    u.fog[1] = dword_to_float(render_states[D3DRS_FOGEND]);
    u.fog[2] = dword_to_float(render_states[D3DRS_FOGDENSITY]);
    color_to_vec4(render_states[D3DRS_FOGCOLOR], u.fog_color);
    color_to_vec4(render_states[D3DRS_TEXTUREFACTOR], u.tfactor);

    // Stage 0. Without a texture, an operation that reads it passes the
    // diffuse color through instead (what D3D drivers do).
    DWORD cop = stage_states[0][D3DTSS_COLOROP];
    DWORD aop = stage_states[0][D3DTSS_ALPHAOP];
    DWORD c1 = stage_states[0][D3DTSS_COLORARG1], c2 = stage_states[0][D3DTSS_COLORARG2],
          c0 = stage_states[0][D3DTSS_COLORARG0];
    DWORD a1 = stage_states[0][D3DTSS_ALPHAARG1], a2 = stage_states[0][D3DTSS_ALPHAARG2],
          a0 = stage_states[0][D3DTSS_ALPHAARG0];
    auto invalid = [&](DWORD op, DWORD x1, DWORD x2, DWORD x0) {
        if (op == D3DTOP_DISABLE || texture != NULL)
        {
            return false;
        }
        if ((x1 & D3DTA_SELECTMASK) == D3DTA_TEXTURE && op != D3DTOP_SELECTARG2)
        {
            return true;
        }
        if ((x2 & D3DTA_SELECTMASK) == D3DTA_TEXTURE && op != D3DTOP_SELECTARG1)
        {
            return true;
        }
        return (x0 & D3DTA_SELECTMASK) == D3DTA_TEXTURE && (op == D3DTOP_MULTIPLYADD || op == D3DTOP_LERP);
    };
    if (invalid(cop, c1, c2, c0))
    {
        cop = D3DTOP_SELECTARG1;
        c1 = D3DTA_CURRENT;
    }
    if (invalid(aop, a1, a2, a0))
    {
        aop = D3DTOP_SELECTARG1;
        a1 = D3DTA_CURRENT;
    }
    if (cop != D3DTOP_DISABLE && aop == D3DTOP_DISABLE)
    {
        aop = D3DTOP_SELECTARG1;
        a1 = D3DTA_CURRENT;
    }
    u.ops[0] = (GLint)cop;
    u.ops[1] = (GLint)aop;
    u.ops[2] = fog_on ? 1 : 0;
    u.color_args[0] = (GLint)c1;
    u.color_args[1] = (GLint)c2;
    u.color_args[2] = (GLint)c0;
    u.alpha_args[0] = (GLint)a1;
    u.alpha_args[1] = (GLint)a2;
    u.alpha_args[2] = (GLint)a0;
    u.alpha_test[0] = render_states[D3DRS_ALPHATESTENABLE] ? 1 : 0;
    u.alpha_test[1] = (GLint)render_states[D3DRS_ALPHAFUNC];
    u.alpha_test[2] = (GLint)(render_states[D3DRS_ALPHAREF] & 0xff);

#define PORT_UNIFORM_CHANGED(field) (!cache_valid || memcmp(&u.field, &cache.field, sizeof(u.field)) != 0)
    // Pre-transformed draws do not read the matrices (and wvp may be stale
    // for them): leave whatever was sent last.
    if (!pretransformed || !cache_valid)
    {
        if (PORT_UNIFORM_CHANGED(wvp))
        {
            gl::UniformMatrix4fv(loc.wvp, 1, GL_FALSE, &u.wvp.m[0][0]);
        }
        if (PORT_UNIFORM_CHANGED(wv))
        {
            gl::UniformMatrix4fv(loc.wv, 1, GL_FALSE, &u.wv.m[0][0]);
        }
        if (PORT_UNIFORM_CHANGED(tex_matrix))
        {
            gl::UniformMatrix4fv(loc.tex_matrix, 1, GL_FALSE, &u.tex_matrix.m[0][0]);
        }
    }
    else
    {
        u.wvp = cache.wvp;
        u.wv = cache.wv;
        u.tex_matrix = cache.tex_matrix;
    }
    if (PORT_UNIFORM_CHANGED(viewport))
    {
        gl::Uniform4fv(loc.viewport, 1, u.viewport);
    }
    if (PORT_UNIFORM_CHANGED(vflags))
    {
        gl::Uniform4iv(loc.vflags, 1, u.vflags);
    }
    if (PORT_UNIFORM_CHANGED(fog))
    {
        gl::Uniform4fv(loc.fog, 1, u.fog);
    }
    if (PORT_UNIFORM_CHANGED(fog_color))
    {
        gl::Uniform4fv(loc.fog_color, 1, u.fog_color);
    }
    if (PORT_UNIFORM_CHANGED(tfactor))
    {
        gl::Uniform4fv(loc.tfactor, 1, u.tfactor);
    }
    if (PORT_UNIFORM_CHANGED(ops))
    {
        gl::Uniform4iv(loc.ops, 1, u.ops);
    }
    if (PORT_UNIFORM_CHANGED(color_args))
    {
        gl::Uniform4iv(loc.color_args, 1, u.color_args);
    }
    if (PORT_UNIFORM_CHANGED(alpha_args))
    {
        gl::Uniform4iv(loc.alpha_args, 1, u.alpha_args);
    }
    if (PORT_UNIFORM_CHANGED(alpha_test))
    {
        gl::Uniform4iv(loc.alpha_test, 1, u.alpha_test);
    }
#undef PORT_UNIFORM_CHANGED
    cache = u;
    cache_valid = true;
}

void PortDevice::apply_state(bool pretransformed)
{
    delete_dead_objects();
    if (dirty & DIRTY_FRAMEBUFFER)
    {
        gl::BindFramebuffer(GL_FRAMEBUFFER, as_surface(render_target)->owner->framebuffer());
        dirty &= ~DIRTY_FRAMEBUFFER;
    }
    if (dirty & DIRTY_VIEWPORT)
    {
        gl::Viewport(viewport.X, viewport.Y, viewport.Width, viewport.Height);
        gl::DepthRange(viewport.MinZ, viewport.MaxZ);
        dirty &= ~DIRTY_VIEWPORT;
    }
    if (dirty & DIRTY_BLEND)
    {
        if (render_states[D3DRS_ALPHABLENDENABLE])
        {
            gl::Enable(GL_BLEND);
            GLenum src, dst, src_a, dst_a;
            gl_blend_factors(render_states[D3DRS_SRCBLEND], render_states[D3DRS_DESTBLEND], &src, &dst);
            if (render_states[D3DRS_SEPARATEALPHABLENDENABLE])
            {
                gl_blend_factors(render_states[D3DRS_SRCBLENDALPHA], render_states[D3DRS_DESTBLENDALPHA], &src_a,
                                 &dst_a);
                gl::BlendFuncSeparate(src, dst, src_a, dst_a);
                gl::BlendEquationSeparate(gl_blend_op(render_states[D3DRS_BLENDOP]),
                                          gl_blend_op(render_states[D3DRS_BLENDOPALPHA]));
            }
            else
            {
                gl::BlendFunc(src, dst);
                gl::BlendEquation(gl_blend_op(render_states[D3DRS_BLENDOP]));
            }
        }
        else
        {
            gl::Disable(GL_BLEND);
        }
        dirty &= ~DIRTY_BLEND;
    }
    if (dirty & DIRTY_DEPTH)
    {
        if (render_states[D3DRS_ZENABLE] != D3DZB_FALSE)
        {
            gl::Enable(GL_DEPTH_TEST);
            gl::DepthFunc(gl_compare(render_states[D3DRS_ZFUNC]));
        }
        else
        {
            gl::Disable(GL_DEPTH_TEST);
        }
        gl::DepthMask(render_states[D3DRS_ZWRITEENABLE] ? GL_TRUE : GL_FALSE);
        dirty &= ~DIRTY_DEPTH;
    }
    if (dirty & DIRTY_COLOR_MASK)
    {
        DWORD mask = render_states[D3DRS_COLORWRITEENABLE];
        gl::ColorMask((mask & 1) != 0, (mask & 2) != 0, (mask & 4) != 0, (mask & 8) != 0);
        dirty &= ~DIRTY_COLOR_MASK;
    }
    if (dirty & DIRTY_CULL)
    {
        // The y flip turns D3D's clockwise (y down) into GL's
        // counter-clockwise.
        switch (render_states[D3DRS_CULLMODE])
        {
        case D3DCULL_CW:
            gl::Enable(GL_CULL_FACE);
            gl::FrontFace(GL_CW);
            gl::CullFace(GL_BACK);
            break;
        case D3DCULL_CCW:
            gl::Enable(GL_CULL_FACE);
            gl::FrontFace(GL_CCW);
            gl::CullFace(GL_BACK);
            break;
        default:
            gl::Disable(GL_CULL_FACE);
            break;
        }
        dirty &= ~DIRTY_CULL;
    }
    if (dirty & DIRTY_SAMPLER)
    {
        DWORD *s = sampler_states[0];
        bool mag_linear = s[D3DSAMP_MAGFILTER] >= D3DTEXF_LINEAR;
        bool min_linear = s[D3DSAMP_MINFILTER] >= D3DTEXF_LINEAR;
        gl::SamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, mag_linear ? GL_LINEAR : GL_NEAREST);
        gl::SamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, min_linear ? GL_LINEAR : GL_NEAREST);
        gl::SamplerParameteri(sampler, GL_TEXTURE_WRAP_S, gl_address(s[D3DSAMP_ADDRESSU]));
        gl::SamplerParameteri(sampler, GL_TEXTURE_WRAP_T, gl_address(s[D3DSAMP_ADDRESSV]));
        float border[4];
        color_to_vec4(s[D3DSAMP_BORDERCOLOR], border);
        gl::SamplerParameterfv(sampler, GL_TEXTURE_BORDER_COLOR, border);
        dirty &= ~DIRTY_SAMPLER;
    }
    if ((dirty & DIRTY_TRANSFORM) && !pretransformed)
    {
        matrix_multiply(&wv, world, view);
        matrix_multiply(&wvp, wv, projection);
        dirty &= ~DIRTY_TRANSFORM;
    }
    if (depth_clamp != (int)pretransformed)
    {
        // D3D does not clip pre-transformed vertices against near and far.
        if (pretransformed)
        {
            gl::Enable(GL_DEPTH_CLAMP);
        }
        else
        {
            gl::Disable(GL_DEPTH_CLAMP);
        }
        depth_clamp = pretransformed;
    }
    GLuint tex = white_texture;
    if (texture != NULL)
    {
        if (texture->render_target && as_surface(render_target)->owner == texture)
        {
            // Sampling the texture being drawn to is undefined in both
            // APIs; the game never does it on purpose.
        }
        texture->sync_gl();
        tex = texture->gl_texture;
    }
    if (bound_texture != tex)
    {
        gl::BindTexture(GL_TEXTURE_2D, tex);
        bound_texture = tex;
    }
    set_uniforms(pretransformed);
}

HRESULT PortDevice::draw(D3DPRIMITIVETYPE type, UINT count, const void *data, UINT stride)
{
    if (data == NULL || stride == 0)
    {
        return D3DERR_INVALIDCALL;
    }
    if (count == 0)
    {
        return D3D_OK;
    }
    GLenum mode;
    size_t vertices;
    switch (type)
    {
    case D3DPT_POINTLIST:
        mode = GL_POINTS;
        vertices = count;
        break;
    case D3DPT_LINELIST:
        mode = GL_LINES;
        vertices = (size_t)count * 2;
        break;
    case D3DPT_LINESTRIP:
        mode = GL_LINE_STRIP;
        vertices = (size_t)count + 1;
        break;
    case D3DPT_TRIANGLELIST:
        mode = GL_TRIANGLES;
        vertices = (size_t)count * 3;
        break;
    case D3DPT_TRIANGLESTRIP:
        mode = GL_TRIANGLE_STRIP;
        vertices = (size_t)count + 2;
        break;
    case D3DPT_TRIANGLEFAN:
        mode = GL_TRIANGLE_FAN;
        vertices = (size_t)count + 2;
        break;
    default:
        return D3DERR_INVALIDCALL;
    }
    bool pretransformed = (fvf & 0xe) == D3DFVF_XYZRHW;
    if (tracing())
    {
        const float *v = (const float *)data;
        PortTexture *rt = as_surface(render_target)->owner;
        gl_log("draw type %d n %u fvf %03x stride %u rt %p(%ux%u) vp %u,%u %ux%u tex %p(%ux%u) blend %d %u/%u op %u "
               "atest %u ref %u cop %u/%u aop %u z %u/%u fog %u v0 %.1f,%.1f,%.3f,%.3f c0 %08x",
               type, count, fvf, stride, (void *)rt, rt->width, rt->height, viewport.X, viewport.Y, viewport.Width,
               viewport.Height, (void *)texture, texture ? texture->width : 0, texture ? texture->height : 0,
               render_states[D3DRS_ALPHABLENDENABLE], render_states[D3DRS_SRCBLEND], render_states[D3DRS_DESTBLEND],
               render_states[D3DRS_BLENDOP], render_states[D3DRS_ALPHATESTENABLE], render_states[D3DRS_ALPHAREF],
               stage_states[0][D3DTSS_COLOROP], stage_states[0][D3DTSS_COLORARG2], stage_states[0][D3DTSS_ALPHAOP],
               render_states[D3DRS_ZENABLE], render_states[D3DRS_ZWRITEENABLE], render_states[D3DRS_FOGENABLE], v[0],
               v[1], v[2], pretransformed ? v[3] : 0.0f, (fvf & D3DFVF_DIFFUSE) ? ((const uint32_t *)data)[pretransformed ? 4 : 3] : 0);
    }
    apply_state(pretransformed);

    size_t bytes = vertices * stride;
    if (bytes > vertex_buffer_size)
    {
        vertex_buffer_size = bytes * 2;
        gl::BindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
        gl::BufferData(GL_ARRAY_BUFFER, vertex_buffer_size, NULL, GL_STREAM_DRAW);
        vertex_buffer_used = 0;
    }
    // The ring buffer offset is a multiple of the stride, so the draw can
    // start at a vertex index with the layout's offsets fixed at 0.
    size_t offset = (vertex_buffer_used + stride - 1) / stride * stride;
    gl::BindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
    if (offset + bytes > vertex_buffer_size)
    {
        gl::BufferData(GL_ARRAY_BUFFER, vertex_buffer_size, NULL, GL_STREAM_DRAW);
        offset = 0;
    }
    void *dst = gl::MapBufferRange(GL_ARRAY_BUFFER, offset, bytes,
                                   GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT | GL_MAP_INVALIDATE_RANGE_BIT);
    if (dst == NULL)
    {
        return D3DERR_INVALIDCALL;
    }
    memcpy(dst, data, bytes);
    gl::UnmapBuffer(GL_ARRAY_BUFFER);
    vertex_buffer_used = offset + bytes;

    GLuint vao = layout_for(fvf, stride);
    if (bound_vao != vao)
    {
        gl::BindVertexArray(vao);
        bound_vao = vao;
    }
    gl::DrawArrays(mode, (GLint)(offset / stride), (GLsizei)vertices);
    return D3D_OK;
}

HRESULT PortDevice::Clear(DWORD Count, const D3DRECT *pRects, DWORD Flags, D3DCOLOR Color, float Z, DWORD Stencil)
{
    if (tracing())
    {
        gl_log("clear n %u flags %x color %08x rect %ld,%ld,%ld,%ld vp %u,%u %ux%u", Count, Flags, Color,
               pRects ? (long)pRects[0].x1 : -1L, pRects ? (long)pRects[0].y1 : -1L, pRects ? (long)pRects[0].x2 : -1L,
               pRects ? (long)pRects[0].y2 : -1L, viewport.X, viewport.Y, viewport.Width, viewport.Height);
    }
    delete_dead_objects();
    if (dirty & DIRTY_FRAMEBUFFER)
    {
        gl::BindFramebuffer(GL_FRAMEBUFFER, as_surface(render_target)->owner->framebuffer());
        dirty &= ~DIRTY_FRAMEBUFFER;
    }
    GLbitfield bits = 0;
    if (Flags & D3DCLEAR_TARGET)
    {
        float c[4];
        color_to_vec4(Color, c);
        gl::ClearColor(c[0], c[1], c[2], c[3]);
        gl::ColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        dirty |= DIRTY_COLOR_MASK;
        bits |= GL_COLOR_BUFFER_BIT;
    }
    if (Flags & D3DCLEAR_ZBUFFER)
    {
        gl::ClearDepth(Z);
        gl::DepthMask(GL_TRUE);
        dirty |= DIRTY_DEPTH;
        bits |= GL_DEPTH_BUFFER_BIT;
    }
    if (bits == 0)
    {
        return D3D_OK;
    }
    // Clears are clipped to the viewport; no rectangles means all of it.
    uint32_t tw, th;
    target_size(&tw, &th);
    LONG vx0 = viewport.X, vy0 = viewport.Y;
    LONG vx1 = vx0 + viewport.Width, vy1 = vy0 + viewport.Height;
    vx1 = vx1 > (LONG)tw ? tw : vx1;
    vy1 = vy1 > (LONG)th ? th : vy1;
    gl::Enable(GL_SCISSOR_TEST);
    DWORD n = pRects != NULL ? Count : 0;
    for (DWORD i = 0; i < (n != 0 ? n : 1); i++)
    {
        LONG x0 = vx0, y0 = vy0, x1 = vx1, y1 = vy1;
        if (n != 0)
        {
            x0 = pRects[i].x1 > x0 ? pRects[i].x1 : x0;
            y0 = pRects[i].y1 > y0 ? pRects[i].y1 : y0;
            x1 = pRects[i].x2 < x1 ? pRects[i].x2 : x1;
            y1 = pRects[i].y2 < y1 ? pRects[i].y2 : y1;
        }
        if (x1 <= x0 || y1 <= y0)
        {
            continue;
        }
        gl::Scissor(x0, y0, x1 - x0, y1 - y0);
        gl::Clear(bits);
    }
    gl::Disable(GL_SCISSOR_TEST);
    return D3D_OK;
}

HRESULT PortDevice::SetRenderTarget(DWORD RenderTargetIndex, IDirect3DSurface9 *pRenderTarget)
{
    if (RenderTargetIndex != 0 || pRenderTarget == NULL)
    {
        return D3DERR_INVALIDCALL;
    }
    PortTexture *owner = as_surface(pRenderTarget)->owner;
    if (tracing())
    {
        gl_log("set render target %p (%ux%u)%s", (void *)owner, owner->width, owner->height,
               owner->is_back_buffer ? " back buffer" : "");
    }
    if (!owner->render_target)
    {
        return D3DERR_INVALIDCALL;
    }
    pRenderTarget->AddRef();
    render_target->Release();
    render_target = pRenderTarget;
    // D3D resets the viewport to the whole new target.
    viewport = {0, 0, owner->width, owner->height, 0.0f, 1.0f};
    dirty |= DIRTY_FRAMEBUFFER | DIRTY_VIEWPORT;
    return D3D_OK;
}

HRESULT PortDevice::UpdateSurface(IDirect3DSurface9 *pSourceSurface, const RECT *pSourceRect,
                                  IDirect3DSurface9 *pDestinationSurface, const POINT *pDestPoint)
{
    if (pSourceSurface == NULL || pDestinationSurface == NULL)
    {
        return D3DERR_INVALIDCALL;
    }
    std::vector<uint32_t> pixels;
    uint32_t w, h;
    HRESULT hr = port_d3d_read_surface(pSourceSurface, pSourceRect, pixels, &w, &h);
    if (hr != D3D_OK)
    {
        return hr;
    }
    RECT dst;
    dst.left = pDestPoint != NULL ? pDestPoint->x : 0;
    dst.top = pDestPoint != NULL ? pDestPoint->y : 0;
    dst.right = dst.left + w;
    dst.bottom = dst.top + h;
    return port_d3d_write_surface(pDestinationSurface, &dst, pixels.data(), w, h);
}

HRESULT PortDevice::StretchRect(IDirect3DSurface9 *pSourceSurface, const RECT *pSourceRect,
                                IDirect3DSurface9 *pDestSurface, const RECT *pDestRect, D3DTEXTUREFILTERTYPE Filter)
{
    if (pSourceSurface == NULL || pDestSurface == NULL)
    {
        return D3DERR_INVALIDCALL;
    }
    PortTexture *src = as_surface(pSourceSurface)->owner;
    PortTexture *dst = as_surface(pDestSurface)->owner;
    RECT s = pSourceRect != NULL ? *pSourceRect : RECT{0, 0, (LONG)src->width, (LONG)src->height};
    RECT d = pDestRect != NULL ? *pDestRect : RECT{0, 0, (LONG)dst->width, (LONG)dst->height};
    if (src->render_target && dst->render_target)
    {
        src->sync_gl();
        dst->sync_gl();
        gl::BindFramebuffer(GL_READ_FRAMEBUFFER, src->framebuffer());
        gl::BindFramebuffer(GL_DRAW_FRAMEBUFFER, dst->framebuffer());
        gl::Disable(GL_SCISSOR_TEST);
        gl::BlitFramebuffer(s.left, s.top, s.right, s.bottom, d.left, d.top, d.right, d.bottom, GL_COLOR_BUFFER_BIT,
                            Filter == D3DTEXF_POINT || Filter == D3DTEXF_NONE ? GL_NEAREST : GL_LINEAR);
        dirty |= DIRTY_FRAMEBUFFER;
        return D3D_OK;
    }
    PORT_UNIMPLEMENTED();
    return D3DERR_INVALIDCALL;
}

void PortDevice::pace_frame()
{
    if (vsync && !pace && pace_allowed)
    {
        double now = now_seconds();
        if (vsync_check_frames == 0)
        {
            vsync_check_start = now;
        }
        if (++vsync_check_frames == 60)
        {
            // 60 swaps in under half a second: vsync is not pacing.
            if (now - vsync_check_start < 0.5)
            {
                gl_log("vsync does not limit the frame rate; pacing with a timer");
                pace = true;
            }
            vsync_check_frames = 0;
        }
    }
    if (!pace)
    {
        return;
    }
    const double period = 1.0 / 60.0;
    double now = now_seconds();
    if (next_frame_time == 0.0 || now - next_frame_time > 4 * period)
    {
        next_frame_time = now;
    }
    next_frame_time += period;
    double wait = next_frame_time - now;
    if (wait > 0.002)
    {
        struct timespec ts;
        double sleep_for = wait - 0.0015;
        ts.tv_sec = (time_t)sleep_for;
        ts.tv_nsec = (long)((sleep_for - ts.tv_sec) * 1e9);
        while (nanosleep(&ts, &ts) != 0 && errno == EINTR)
        {
        }
    }
    while (now_seconds() < next_frame_time)
    {
    }
}

void PortDevice::dump_frame()
{
    uint32_t w = back_buffer->width;
    uint32_t h = back_buffer->height;
    std::vector<uint32_t> pixels((size_t)w * h);
    gl::BindFramebuffer(GL_READ_FRAMEBUFFER, back_buffer->framebuffer());
    gl::ReadBuffer(GL_COLOR_ATTACHMENT0);
    gl::PixelStorei(GL_PACK_ALIGNMENT, 4);
    gl::PixelStorei(GL_PACK_ROW_LENGTH, 0);
    gl::ReadPixels(0, 0, w, h, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, pixels.data());
    char path[1024];
    snprintf(path, sizeof(path), "%s/frame_%06u.png", dump_dir.c_str(), frame_count);
    if (!write_png(path, pixels.data(), w, h))
    {
        gl_log("cannot write %s", path);
    }
}

HRESULT PortDevice::Present(const RECT *pSourceRect, const RECT *pDestRect, HWND hDestWindowOverride,
                            const void *pDirtyRegion)
{
    delete_dead_objects();
    frame_count++;
    if (!dump_dir.empty() && dump_every != 0 && frame_count % dump_every == 0)
    {
        dump_frame();
    }
    if (window != NULL)
    {
        int dw, dh;
        SDL_GL_GetDrawableSize(window, &dw, &dh);
        uint32_t bw = back_buffer->width;
        uint32_t bh = back_buffer->height;
        if (pSourceRect != NULL)
        {
            PORT_UNIMPLEMENTED();
        }
        gl::BindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        gl::BindFramebuffer(GL_READ_FRAMEBUFFER, back_buffer->framebuffer());
        gl::ReadBuffer(GL_COLOR_ATTACHMENT0);
        gl::Disable(GL_SCISSOR_TEST);
        gl::ColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        gl::Viewport(0, 0, dw, dh);
        gl::ClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        gl::Clear(GL_COLOR_BUFFER_BIT);
        // Scale to fit, keeping the aspect ratio; flip, since the back
        // buffer's first row is the top.
        int out_w = dw, out_h = dh;
        if ((int64_t)dw * bh > (int64_t)dh * bw)
        {
            out_w = (int)((int64_t)dh * bw / bh);
        }
        else
        {
            out_h = (int)((int64_t)dw * bh / bw);
        }
        int x0 = (dw - out_w) / 2;
        int y0 = (dh - out_h) / 2;
        GLenum filter = (uint32_t)out_w == bw && (uint32_t)out_h == bh ? GL_NEAREST : GL_LINEAR;
        gl::BlitFramebuffer(0, 0, bw, bh, x0, y0 + out_h, x0 + out_w, y0, GL_COLOR_BUFFER_BIT, filter);
        SDL_GL_SwapWindow(window);
    }
    dirty = DIRTY_ALL;
    pace_frame();
    if (exit_after != 0 && frame_count >= exit_after)
    {
        gl_log("TH16_GL_EXIT_AFTER: exiting after %u frames", frame_count);
        fflush(stdout);
        fflush(stderr);
        _exit(0);
    }
    return D3D_OK;
}

HRESULT PortDevice::Reset(D3DPRESENT_PARAMETERS *pPresentationParameters)
{
    if (pPresentationParameters == NULL)
    {
        return D3DERR_INVALIDCALL;
    }
    params = *pPresentationParameters;
    if (window != g_attached_window && g_attached_window != NULL)
    {
        window = g_attached_window;
        SDL_GL_MakeCurrent(window, context);
    }
    create_back_buffer();
    reset_state();
    apply_present_params();
    *pPresentationParameters = params;
    return D3D_OK;
}

// ---------------------------------------------------------------------------
// IDirect3D9

struct PortDirect3D9 final : public PortComObject<IDirect3D9>
{
    UINT GetAdapterCount() override
    {
        return 1;
    }
    HRESULT GetAdapterDisplayMode(UINT Adapter, D3DDISPLAYMODE *pMode) override
    {
        if (pMode == NULL)
        {
            return D3DERR_INVALIDCALL;
        }
        pMode->Width = 640;
        pMode->Height = 480;
        SDL_DisplayMode mode;
        if (SDL_WasInit(SDL_INIT_VIDEO) && SDL_GetDesktopDisplayMode(0, &mode) == 0)
        {
            pMode->Width = mode.w;
            pMode->Height = mode.h;
        }
        // The renderer runs the game at 60 frames per second whatever the
        // display does (see apply_present_params), so report 60 Hz.
        pMode->RefreshRate = 60;
        pMode->Format = D3DFMT_X8R8G8B8;
        return D3D_OK;
    }
    HRESULT CheckDeviceType(UINT Adapter, D3DDEVTYPE DevType, D3DFORMAT AdapterFormat, D3DFORMAT BackBufferFormat,
                            BOOL bWindowed) override
    {
        return DevType == D3DDEVTYPE_HAL ? D3D_OK : D3DERR_NOTAVAILABLE;
    }
    HRESULT CheckDeviceFormat(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT AdapterFormat, DWORD Usage,
                              D3DRESOURCETYPE RType, D3DFORMAT CheckFormat) override
    {
        if (Usage & D3DUSAGE_RENDERTARGET)
        {
            return CheckFormat == D3DFMT_A8R8G8B8 || CheckFormat == D3DFMT_X8R8G8B8 ? D3D_OK : D3DERR_NOTAVAILABLE;
        }
        return port_d3d_format_bpp(CheckFormat) != 0 ? D3D_OK : D3DERR_NOTAVAILABLE;
    }
    HRESULT GetDeviceCaps(UINT Adapter, D3DDEVTYPE DeviceType, D3DCAPS9 *pCaps) override
    {
        if (pCaps == NULL)
        {
            return D3DERR_INVALIDCALL;
        }
        memset(pCaps, 0, sizeof(*pCaps));
        pCaps->DeviceType = D3DDEVTYPE_HAL;
        pCaps->PresentationIntervals = D3DPRESENT_INTERVAL_ONE | D3DPRESENT_INTERVAL_IMMEDIATE;
        pCaps->DevCaps = D3DDEVCAPS_HWTRANSFORMANDLIGHT;
        pCaps->PrimitiveMiscCaps = D3DPMISCCAPS_BLENDOP | D3DPMISCCAPS_SEPARATEALPHABLEND | 0x70 /* cull modes */ |
                                   0x80 /* COLORWRITEENABLE */;
        pCaps->ZCmpCaps = 0xff;
        pCaps->AlphaCmpCaps = 0xff;
        pCaps->SrcBlendCaps = 0x3fff;
        pCaps->DestBlendCaps = 0x3fff;
        pCaps->ShadeCaps = 0x00084208;
        pCaps->TextureCaps = 0x00000004 /* ALPHA */ | 0x00000040 /* PERSPECTIVE */;
        pCaps->TextureFilterCaps = 0x03030300;
        pCaps->TextureAddressCaps = 0x3f;
        pCaps->LineCaps = 0x1f;
        GLint max_size = 0;
        if (g_device != NULL && on_gl_thread())
        {
            gl::GetIntegerv(GL_MAX_TEXTURE_SIZE, &max_size);
        }
        max_size = max_size <= 0 || max_size > 8192 ? 8192 : max_size;
        pCaps->MaxTextureWidth = max_size;
        pCaps->MaxTextureHeight = max_size;
        pCaps->MaxTextureRepeat = 8192;
        pCaps->MaxTextureAspectRatio = 8192;
        pCaps->MaxAnisotropy = 16;
        pCaps->MaxVertexW = 1e10f;
        pCaps->GuardBandLeft = -1e8f;
        pCaps->GuardBandTop = -1e8f;
        pCaps->GuardBandRight = 1e8f;
        pCaps->GuardBandBottom = 1e8f;
        pCaps->FVFCaps = 8;
        pCaps->TextureOpCaps = 0x03ffffff;
        pCaps->MaxTextureBlendStages = 8;
        pCaps->MaxSimultaneousTextures = 8;
        pCaps->MaxPointSize = 1.0f;
        pCaps->MaxPrimitiveCount = 0xffffff;
        pCaps->MaxVertexIndex = 0xffffff;
        pCaps->MaxStreams = 1;
        pCaps->MaxStreamStride = 255;
        pCaps->NumSimultaneousRTs = 1;
        pCaps->MasterAdapterOrdinal = 0;
        pCaps->NumberOfAdaptersInGroup = 1;
        return D3D_OK;
    }
    HRESULT CreateDevice(UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags,
                         D3DPRESENT_PARAMETERS *pPresentationParameters,
                         IDirect3DDevice9 **ppReturnedDeviceInterface) override;
};

HRESULT PortDirect3D9::CreateDevice(UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags,
                                    D3DPRESENT_PARAMETERS *pPresentationParameters,
                                    IDirect3DDevice9 **ppReturnedDeviceInterface)
{
    if (ppReturnedDeviceInterface == NULL || pPresentationParameters == NULL)
    {
        return D3DERR_INVALIDCALL;
    }
    *ppReturnedDeviceInterface = NULL;
    if (DeviceType != D3DDEVTYPE_HAL || g_device != NULL)
    {
        return D3DERR_NOTAVAILABLE;
    }
    SDL_Window *window = g_attached_window;
    if (window == NULL)
    {
        // Nothing attached a window: open one, so the game can run while
        // the window code is a stub.
        if (!SDL_WasInit(SDL_INIT_VIDEO))
        {
            if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0)
            {
                gl_log("SDL video init failed: %s", SDL_GetError());
                return D3DERR_NOTAVAILABLE;
            }
            g_we_initialized_video = true;
        }
        if (g_own_window == NULL)
        {
            port_gl_prepare_window();
            int w = pPresentationParameters->BackBufferWidth != 0 ? pPresentationParameters->BackBufferWidth : 640;
            int h = pPresentationParameters->BackBufferHeight != 0 ? pPresentationParameters->BackBufferHeight : 480;
            g_own_window = SDL_CreateWindow("th16", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, w, h,
                                            port_gl_window_flags() | SDL_WINDOW_ALLOW_HIGHDPI);
            if (g_own_window == NULL)
            {
                gl_log("cannot create a window: %s", SDL_GetError());
                return D3DERR_NOTAVAILABLE;
            }
        }
        window = g_own_window;
    }
    PortDevice *device = new PortDevice(this);
    g_device = device;
    g_gl_thread = std::this_thread::get_id();
    if (!device->init(window, *pPresentationParameters))
    {
        device->Release();
        return D3DERR_NOTAVAILABLE;
    }
    *pPresentationParameters = device->params;
    *ppReturnedDeviceInterface = device;
    return D3D_OK;
}

} // namespace

// ---------------------------------------------------------------------------
// Shared with d3dx9_tex.cpp

HRESULT port_d3d_create_texture(IDirect3DDevice9 *device, UINT width, UINT height, DWORD usage, D3DFORMAT format,
                                D3DPOOL pool, IDirect3DTexture9 **texture)
{
    if (texture == NULL)
    {
        return D3DERR_INVALIDCALL;
    }
    *texture = NULL;
    if (width == 0 || height == 0 || width > 16384 || height > 16384)
    {
        return D3DERR_INVALIDCALL;
    }
    if (usage & D3DUSAGE_RENDERTARGET)
    {
        if (format != D3DFMT_A8R8G8B8 && format != D3DFMT_X8R8G8B8)
        {
            return D3DERR_NOTAVAILABLE;
        }
    }
    else if (port_d3d_format_bpp(format) == 0)
    {
        return D3DERR_NOTAVAILABLE;
    }
    *texture = new PortTexture(width, height, format, usage, pool);
    return D3D_OK;
}

HRESULT port_d3d_read_surface(IDirect3DSurface9 *surface, const RECT *rect, std::vector<uint32_t> &pixels,
                              uint32_t *width, uint32_t *height)
{
    if (surface == NULL)
    {
        return D3DERR_INVALIDCALL;
    }
    PortTexture *t = as_surface(surface)->owner;
    RECT r = rect != NULL ? *rect : RECT{0, 0, (LONG)t->width, (LONG)t->height};
    // Clip to the surface (D3DX reads outside as transparent black).
    RECT c = r;
    c.left = c.left < 0 ? 0 : c.left;
    c.top = c.top < 0 ? 0 : c.top;
    c.right = c.right > (LONG)t->width ? (LONG)t->width : c.right;
    c.bottom = c.bottom > (LONG)t->height ? (LONG)t->height : c.bottom;
    if (r.right <= r.left || r.bottom <= r.top)
    {
        return D3DERR_INVALIDCALL;
    }
    uint32_t w = r.right - r.left;
    uint32_t h = r.bottom - r.top;
    pixels.assign((size_t)w * h, 0);
    *width = w;
    *height = h;
    if (c.right <= c.left || c.bottom <= c.top)
    {
        return D3D_OK;
    }
    std::lock_guard<std::recursive_mutex> guard(t->mutex);
    if (t->render_target)
    {
        if (!on_gl_thread())
        {
            return D3DERR_INVALIDCALL;
        }
        t->read_back(c);
    }
    for (LONG y = c.top; y < c.bottom; y++)
    {
        port_d3d_decode_pixels(t->format, t->pixels.data() + (size_t)y * t->pitch + (size_t)c.left * t->bpp,
                               pixels.data() + (size_t)(y - r.top) * w + (c.left - r.left), c.right - c.left);
    }
    return D3D_OK;
}

HRESULT port_d3d_write_surface(IDirect3DSurface9 *surface, const RECT *rect, const uint32_t *pixels, uint32_t width,
                               uint32_t height)
{
    if (surface == NULL)
    {
        return D3DERR_INVALIDCALL;
    }
    PortTexture *t = as_surface(surface)->owner;
    RECT r = rect != NULL ? *rect : RECT{0, 0, (LONG)t->width, (LONG)t->height};
    if ((uint32_t)(r.right - r.left) != width || (uint32_t)(r.bottom - r.top) != height)
    {
        return D3DERR_INVALIDCALL;
    }
    RECT c = r;
    c.left = c.left < 0 ? 0 : c.left;
    c.top = c.top < 0 ? 0 : c.top;
    c.right = c.right > (LONG)t->width ? (LONG)t->width : c.right;
    c.bottom = c.bottom > (LONG)t->height ? (LONG)t->height : c.bottom;
    if (c.right <= c.left || c.bottom <= c.top)
    {
        return D3D_OK;
    }
    std::lock_guard<std::recursive_mutex> guard(t->mutex);
    if (t->render_target)
    {
        if (!on_gl_thread())
        {
            return D3DERR_INVALIDCALL;
        }
        t->pixels.resize((size_t)t->pitch * t->height);
    }
    for (LONG y = c.top; y < c.bottom; y++)
    {
        port_d3d_encode_pixels(t->format, pixels + (size_t)(y - r.top) * width + (c.left - r.left),
                               t->pixels.data() + (size_t)y * t->pitch + (size_t)c.left * t->bpp, c.right - c.left);
    }
    if (t->render_target)
    {
        t->sync_gl();
        t->upload(c);
    }
    else
    {
        t->mark_dirty(c);
    }
    return D3D_OK;
}

// ---------------------------------------------------------------------------
// Window interface (d3d9_gl.h)

uint32_t port_gl_window_flags()
{
    return SDL_WINDOW_OPENGL;
}

void port_gl_prepare_window()
{
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    // Everything is drawn into FBOs; the window only receives the blit.
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);
}

void port_gl_attach_window(SDL_Window *window)
{
    g_attached_window = window;
    if (g_device != NULL && window != NULL && on_gl_thread() && g_device->window != window)
    {
        g_device->window = window;
        SDL_GL_MakeCurrent(window, g_device->context);
        g_device->apply_present_params();
    }
}

void port_gl_detach_window(SDL_Window *window)
{
    if (g_attached_window == window)
    {
        g_attached_window = NULL;
    }
    if (g_device != NULL && g_device->window == window)
    {
        g_device->window = NULL;
    }
}

SDL_Window *port_gl_window()
{
    if (g_device != NULL && g_device->window != NULL)
    {
        return g_device->window;
    }
    return g_attached_window != NULL ? g_attached_window : g_own_window;
}

bool port_gl_window_to_back_buffer(int x, int y, int *out_x, int *out_y)
{
    if (g_device == NULL || g_device->window == NULL)
    {
        return false;
    }
    int ww, wh;
    SDL_GetWindowSize(g_device->window, &ww, &wh);
    uint32_t bw = g_device->back_buffer->width;
    uint32_t bh = g_device->back_buffer->height;
    if (ww <= 0 || wh <= 0)
    {
        return false;
    }
    int out_w = ww, out_h = wh;
    if ((int64_t)ww * bh > (int64_t)wh * bw)
    {
        out_w = (int)((int64_t)wh * bw / bh);
    }
    else
    {
        out_h = (int)((int64_t)ww * bh / bw);
    }
    int x0 = (ww - out_w) / 2;
    int y0 = (wh - out_h) / 2;
    if (x < x0 || y < y0 || x >= x0 + out_w || y >= y0 + out_h)
    {
        return false;
    }
    *out_x = (int)((int64_t)(x - x0) * bw / out_w);
    *out_y = (int)((int64_t)(y - y0) * bh / out_h);
    return true;
}

extern "C" IDirect3D9 *Direct3DCreate9(UINT SDKVersion)
{
    return new PortDirect3D9();
}
