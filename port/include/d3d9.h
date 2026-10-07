// Stand-in for the DirectX SDK's d3d9.h (with d3d9types.h and d3d9caps.h)
// in the portable build. Types, constants and structures keep the SDK's
// names, values and 32-bit layouts (Supervisor embeds D3DCAPS9,
// D3DPRESENT_PARAMETERS and friends at fixed offsets). The interfaces are
// C++ abstract classes with the methods the game calls; the platform layer
// implements them (port/src/d3d9_*.cpp).
#pragma once

#include <windows.h>

// ---------------------------------------------------------------------------
// d3d9types.h

typedef DWORD D3DCOLOR;

#define D3DCOLOR_ARGB(a, r, g, b) \
    ((D3DCOLOR)((((a) & 0xff) << 24) | (((r) & 0xff) << 16) | (((g) & 0xff) << 8) | ((b) & 0xff)))
#define D3DCOLOR_RGBA(r, g, b, a) D3DCOLOR_ARGB(a, r, g, b)
#define D3DCOLOR_XRGB(r, g, b) D3DCOLOR_ARGB(0xff, r, g, b)

typedef struct _D3DVECTOR
{
    float x;
    float y;
    float z;
} D3DVECTOR;

typedef struct _D3DCOLORVALUE
{
    float r;
    float g;
    float b;
    float a;
} D3DCOLORVALUE;

typedef struct _D3DRECT
{
    LONG x1;
    LONG y1;
    LONG x2;
    LONG y2;
} D3DRECT;

typedef struct _D3DMATRIX
{
    union {
        struct
        {
            float _11, _12, _13, _14;
            float _21, _22, _23, _24;
            float _31, _32, _33, _34;
            float _41, _42, _43, _44;
        };
        float m[4][4];
    };
} D3DMATRIX;

typedef struct _D3DVIEWPORT9
{
    DWORD X;
    DWORD Y;
    DWORD Width;
    DWORD Height;
    float MinZ;
    float MaxZ;
} D3DVIEWPORT9;

typedef enum _D3DFORMAT {
    D3DFMT_UNKNOWN = 0,
    D3DFMT_R8G8B8 = 20,
    D3DFMT_A8R8G8B8 = 21,
    D3DFMT_X8R8G8B8 = 22,
    D3DFMT_R5G6B5 = 23,
    D3DFMT_X1R5G5B5 = 24,
    D3DFMT_A1R5G5B5 = 25,
    D3DFMT_A4R4G4B4 = 26,
    D3DFMT_R3G3B2 = 27,
    D3DFMT_A8 = 28,
    D3DFMT_A8R3G3B2 = 29,
    D3DFMT_X4R4G4B4 = 30,
    D3DFMT_A2B10G10R10 = 31,
    D3DFMT_A8B8G8R8 = 32,
    D3DFMT_X8B8G8R8 = 33,
    D3DFMT_G16R16 = 34,
    D3DFMT_A2R10G10B10 = 35,
    D3DFMT_A16B16G16R16 = 36,
    D3DFMT_A8P8 = 40,
    D3DFMT_P8 = 41,
    D3DFMT_L8 = 50,
    D3DFMT_A8L8 = 51,
    D3DFMT_A4L4 = 52,
    D3DFMT_D16_LOCKABLE = 70,
    D3DFMT_D32 = 71,
    D3DFMT_D15S1 = 73,
    D3DFMT_D24S8 = 75,
    D3DFMT_D24X8 = 77,
    D3DFMT_D24X4S4 = 79,
    D3DFMT_D16 = 80,
    D3DFMT_VERTEXDATA = 100,
    D3DFMT_INDEX16 = 101,
    D3DFMT_INDEX32 = 102,
    D3DFMT_FORCE_DWORD = 0x7fffffff
} D3DFORMAT;

typedef enum _D3DDEVTYPE {
    D3DDEVTYPE_HAL = 1,
    D3DDEVTYPE_REF = 2,
    D3DDEVTYPE_SW = 3,
    D3DDEVTYPE_NULLREF = 4,
    D3DDEVTYPE_FORCE_DWORD = 0x7fffffff
} D3DDEVTYPE;

typedef enum _D3DMULTISAMPLE_TYPE {
    D3DMULTISAMPLE_NONE = 0,
    D3DMULTISAMPLE_NONMASKABLE = 1,
    D3DMULTISAMPLE_2_SAMPLES = 2,
    D3DMULTISAMPLE_4_SAMPLES = 4,
    D3DMULTISAMPLE_8_SAMPLES = 8,
    D3DMULTISAMPLE_FORCE_DWORD = 0x7fffffff
} D3DMULTISAMPLE_TYPE;

typedef enum _D3DSWAPEFFECT {
    D3DSWAPEFFECT_DISCARD = 1,
    D3DSWAPEFFECT_FLIP = 2,
    D3DSWAPEFFECT_COPY = 3,
    D3DSWAPEFFECT_FORCE_DWORD = 0x7fffffff
} D3DSWAPEFFECT;

typedef enum _D3DPOOL {
    D3DPOOL_DEFAULT = 0,
    D3DPOOL_MANAGED = 1,
    D3DPOOL_SYSTEMMEM = 2,
    D3DPOOL_SCRATCH = 3,
    D3DPOOL_FORCE_DWORD = 0x7fffffff
} D3DPOOL;

typedef enum _D3DRESOURCETYPE {
    D3DRTYPE_SURFACE = 1,
    D3DRTYPE_VOLUME = 2,
    D3DRTYPE_TEXTURE = 3,
    D3DRTYPE_VOLUMETEXTURE = 4,
    D3DRTYPE_CUBETEXTURE = 5,
    D3DRTYPE_VERTEXBUFFER = 6,
    D3DRTYPE_INDEXBUFFER = 7,
    D3DRTYPE_FORCE_DWORD = 0x7fffffff
} D3DRESOURCETYPE;

typedef enum _D3DBACKBUFFER_TYPE {
    D3DBACKBUFFER_TYPE_MONO = 0,
    D3DBACKBUFFER_TYPE_LEFT = 1,
    D3DBACKBUFFER_TYPE_RIGHT = 2,
    D3DBACKBUFFER_TYPE_FORCE_DWORD = 0x7fffffff
} D3DBACKBUFFER_TYPE;

typedef enum _D3DPRIMITIVETYPE {
    D3DPT_POINTLIST = 1,
    D3DPT_LINELIST = 2,
    D3DPT_LINESTRIP = 3,
    D3DPT_TRIANGLELIST = 4,
    D3DPT_TRIANGLESTRIP = 5,
    D3DPT_TRIANGLEFAN = 6,
    D3DPT_FORCE_DWORD = 0x7fffffff
} D3DPRIMITIVETYPE;

typedef enum _D3DTRANSFORMSTATETYPE {
    D3DTS_VIEW = 2,
    D3DTS_PROJECTION = 3,
    D3DTS_TEXTURE0 = 16,
    D3DTS_TEXTURE1 = 17,
    D3DTS_TEXTURE2 = 18,
    D3DTS_TEXTURE3 = 19,
    D3DTS_TEXTURE4 = 20,
    D3DTS_TEXTURE5 = 21,
    D3DTS_TEXTURE6 = 22,
    D3DTS_TEXTURE7 = 23,
    D3DTS_FORCE_DWORD = 0x7fffffff
} D3DTRANSFORMSTATETYPE;

#define D3DTS_WORLDMATRIX(index) (D3DTRANSFORMSTATETYPE)((index) + 256)
#define D3DTS_WORLD D3DTS_WORLDMATRIX(0)

typedef enum _D3DRENDERSTATETYPE {
    D3DRS_ZENABLE = 7,
    D3DRS_FILLMODE = 8,
    D3DRS_SHADEMODE = 9,
    D3DRS_ZWRITEENABLE = 14,
    D3DRS_ALPHATESTENABLE = 15,
    D3DRS_LASTPIXEL = 16,
    D3DRS_SRCBLEND = 19,
    D3DRS_DESTBLEND = 20,
    D3DRS_CULLMODE = 22,
    D3DRS_ZFUNC = 23,
    D3DRS_ALPHAREF = 24,
    D3DRS_ALPHAFUNC = 25,
    D3DRS_DITHERENABLE = 26,
    D3DRS_ALPHABLENDENABLE = 27,
    D3DRS_FOGENABLE = 28,
    D3DRS_SPECULARENABLE = 29,
    D3DRS_FOGCOLOR = 34,
    D3DRS_FOGTABLEMODE = 35,
    D3DRS_FOGSTART = 36,
    D3DRS_FOGEND = 37,
    D3DRS_FOGDENSITY = 38,
    D3DRS_RANGEFOGENABLE = 48,
    D3DRS_STENCILENABLE = 52,
    D3DRS_STENCILFAIL = 53,
    D3DRS_STENCILZFAIL = 54,
    D3DRS_STENCILPASS = 55,
    D3DRS_STENCILFUNC = 56,
    D3DRS_STENCILREF = 57,
    D3DRS_STENCILMASK = 58,
    D3DRS_STENCILWRITEMASK = 59,
    D3DRS_TEXTUREFACTOR = 60,
    D3DRS_WRAP0 = 128,
    D3DRS_CLIPPING = 136,
    D3DRS_LIGHTING = 137,
    D3DRS_AMBIENT = 139,
    D3DRS_FOGVERTEXMODE = 140,
    D3DRS_COLORVERTEX = 141,
    D3DRS_LOCALVIEWER = 142,
    D3DRS_NORMALIZENORMALS = 143,
    D3DRS_DIFFUSEMATERIALSOURCE = 145,
    D3DRS_SPECULARMATERIALSOURCE = 146,
    D3DRS_AMBIENTMATERIALSOURCE = 147,
    D3DRS_EMISSIVEMATERIALSOURCE = 148,
    D3DRS_VERTEXBLEND = 151,
    D3DRS_CLIPPLANEENABLE = 152,
    D3DRS_POINTSIZE = 154,
    D3DRS_POINTSIZE_MIN = 155,
    D3DRS_POINTSPRITEENABLE = 156,
    D3DRS_POINTSCALEENABLE = 157,
    D3DRS_MULTISAMPLEANTIALIAS = 161,
    D3DRS_MULTISAMPLEMASK = 162,
    D3DRS_COLORWRITEENABLE = 168,
    D3DRS_BLENDOP = 171,
    D3DRS_SCISSORTESTENABLE = 174,
    D3DRS_ANTIALIASEDLINEENABLE = 176,
    D3DRS_SEPARATEALPHABLENDENABLE = 206,
    D3DRS_SRCBLENDALPHA = 207,
    D3DRS_DESTBLENDALPHA = 208,
    D3DRS_BLENDOPALPHA = 209,
    D3DRS_FORCE_DWORD = 0x7fffffff
} D3DRENDERSTATETYPE;

typedef enum _D3DTEXTURESTAGESTATETYPE {
    D3DTSS_COLOROP = 1,
    D3DTSS_COLORARG1 = 2,
    D3DTSS_COLORARG2 = 3,
    D3DTSS_ALPHAOP = 4,
    D3DTSS_ALPHAARG1 = 5,
    D3DTSS_ALPHAARG2 = 6,
    D3DTSS_BUMPENVMAT00 = 7,
    D3DTSS_BUMPENVMAT01 = 8,
    D3DTSS_BUMPENVMAT10 = 9,
    D3DTSS_BUMPENVMAT11 = 10,
    D3DTSS_TEXCOORDINDEX = 11,
    D3DTSS_BUMPENVLSCALE = 22,
    D3DTSS_BUMPENVLOFFSET = 23,
    D3DTSS_TEXTURETRANSFORMFLAGS = 24,
    D3DTSS_COLORARG0 = 26,
    D3DTSS_ALPHAARG0 = 27,
    D3DTSS_RESULTARG = 28,
    D3DTSS_CONSTANT = 32,
    D3DTSS_FORCE_DWORD = 0x7fffffff
} D3DTEXTURESTAGESTATETYPE;

typedef enum _D3DSAMPLERSTATETYPE {
    D3DSAMP_ADDRESSU = 1,
    D3DSAMP_ADDRESSV = 2,
    D3DSAMP_ADDRESSW = 3,
    D3DSAMP_BORDERCOLOR = 4,
    D3DSAMP_MAGFILTER = 5,
    D3DSAMP_MINFILTER = 6,
    D3DSAMP_MIPFILTER = 7,
    D3DSAMP_MIPMAPLODBIAS = 8,
    D3DSAMP_MAXMIPLEVEL = 9,
    D3DSAMP_MAXANISOTROPY = 10,
    D3DSAMP_SRGBTEXTURE = 11,
    D3DSAMP_ELEMENTINDEX = 12,
    D3DSAMP_DMAPOFFSET = 13,
    D3DSAMP_FORCE_DWORD = 0x7fffffff
} D3DSAMPLERSTATETYPE;

typedef enum _D3DTEXTUREOP {
    D3DTOP_DISABLE = 1,
    D3DTOP_SELECTARG1 = 2,
    D3DTOP_SELECTARG2 = 3,
    D3DTOP_MODULATE = 4,
    D3DTOP_MODULATE2X = 5,
    D3DTOP_MODULATE4X = 6,
    D3DTOP_ADD = 7,
    D3DTOP_ADDSIGNED = 8,
    D3DTOP_ADDSIGNED2X = 9,
    D3DTOP_SUBTRACT = 10,
    D3DTOP_ADDSMOOTH = 11,
    D3DTOP_BLENDDIFFUSEALPHA = 12,
    D3DTOP_BLENDTEXTUREALPHA = 13,
    D3DTOP_BLENDFACTORALPHA = 14,
    D3DTOP_BLENDTEXTUREALPHAPM = 15,
    D3DTOP_BLENDCURRENTALPHA = 16,
    D3DTOP_PREMODULATE = 17,
    D3DTOP_MODULATEALPHA_ADDCOLOR = 18,
    D3DTOP_MODULATECOLOR_ADDALPHA = 19,
    D3DTOP_MODULATEINVALPHA_ADDCOLOR = 20,
    D3DTOP_MODULATEINVCOLOR_ADDALPHA = 21,
    D3DTOP_BUMPENVMAP = 22,
    D3DTOP_BUMPENVMAPLUMINANCE = 23,
    D3DTOP_DOTPRODUCT3 = 24,
    D3DTOP_MULTIPLYADD = 25,
    D3DTOP_LERP = 26,
    D3DTOP_FORCE_DWORD = 0x7fffffff
} D3DTEXTUREOP;

#define D3DTA_SELECTMASK 0x0000000f
#define D3DTA_DIFFUSE 0x00000000
#define D3DTA_CURRENT 0x00000001
#define D3DTA_TEXTURE 0x00000002
#define D3DTA_TFACTOR 0x00000003
#define D3DTA_SPECULAR 0x00000004
#define D3DTA_TEMP 0x00000005
#define D3DTA_CONSTANT 0x00000006
#define D3DTA_COMPLEMENT 0x00000010
#define D3DTA_ALPHAREPLICATE 0x00000020

typedef enum _D3DTEXTUREFILTERTYPE {
    D3DTEXF_NONE = 0,
    D3DTEXF_POINT = 1,
    D3DTEXF_LINEAR = 2,
    D3DTEXF_ANISOTROPIC = 3,
    D3DTEXF_PYRAMIDALQUAD = 6,
    D3DTEXF_GAUSSIANQUAD = 7,
    D3DTEXF_FORCE_DWORD = 0x7fffffff
} D3DTEXTUREFILTERTYPE;

typedef enum _D3DTEXTUREADDRESS {
    D3DTADDRESS_WRAP = 1,
    D3DTADDRESS_MIRROR = 2,
    D3DTADDRESS_CLAMP = 3,
    D3DTADDRESS_BORDER = 4,
    D3DTADDRESS_MIRRORONCE = 5,
    D3DTADDRESS_FORCE_DWORD = 0x7fffffff
} D3DTEXTUREADDRESS;

typedef enum _D3DTEXTURETRANSFORMFLAGS {
    D3DTTFF_DISABLE = 0,
    D3DTTFF_COUNT1 = 1,
    D3DTTFF_COUNT2 = 2,
    D3DTTFF_COUNT3 = 3,
    D3DTTFF_COUNT4 = 4,
    D3DTTFF_PROJECTED = 256,
    D3DTTFF_FORCE_DWORD = 0x7fffffff
} D3DTEXTURETRANSFORMFLAGS;

typedef enum _D3DBLEND {
    D3DBLEND_ZERO = 1,
    D3DBLEND_ONE = 2,
    D3DBLEND_SRCCOLOR = 3,
    D3DBLEND_INVSRCCOLOR = 4,
    D3DBLEND_SRCALPHA = 5,
    D3DBLEND_INVSRCALPHA = 6,
    D3DBLEND_DESTALPHA = 7,
    D3DBLEND_INVDESTALPHA = 8,
    D3DBLEND_DESTCOLOR = 9,
    D3DBLEND_INVDESTCOLOR = 10,
    D3DBLEND_SRCALPHASAT = 11,
    D3DBLEND_BOTHSRCALPHA = 12,
    D3DBLEND_BOTHINVSRCALPHA = 13,
    D3DBLEND_BLENDFACTOR = 14,
    D3DBLEND_INVBLENDFACTOR = 15,
    D3DBLEND_FORCE_DWORD = 0x7fffffff
} D3DBLEND;

typedef enum _D3DBLENDOP {
    D3DBLENDOP_ADD = 1,
    D3DBLENDOP_SUBTRACT = 2,
    D3DBLENDOP_REVSUBTRACT = 3,
    D3DBLENDOP_MIN = 4,
    D3DBLENDOP_MAX = 5,
    D3DBLENDOP_FORCE_DWORD = 0x7fffffff
} D3DBLENDOP;

typedef enum _D3DCMPFUNC {
    D3DCMP_NEVER = 1,
    D3DCMP_LESS = 2,
    D3DCMP_EQUAL = 3,
    D3DCMP_LESSEQUAL = 4,
    D3DCMP_GREATER = 5,
    D3DCMP_NOTEQUAL = 6,
    D3DCMP_GREATEREQUAL = 7,
    D3DCMP_ALWAYS = 8,
    D3DCMP_FORCE_DWORD = 0x7fffffff
} D3DCMPFUNC;

typedef enum _D3DFOGMODE {
    D3DFOG_NONE = 0,
    D3DFOG_EXP = 1,
    D3DFOG_EXP2 = 2,
    D3DFOG_LINEAR = 3,
    D3DFOG_FORCE_DWORD = 0x7fffffff
} D3DFOGMODE;

typedef enum _D3DCULL {
    D3DCULL_NONE = 1,
    D3DCULL_CW = 2,
    D3DCULL_CCW = 3,
    D3DCULL_FORCE_DWORD = 0x7fffffff
} D3DCULL;

typedef enum _D3DSHADEMODE {
    D3DSHADE_FLAT = 1,
    D3DSHADE_GOURAUD = 2,
    D3DSHADE_PHONG = 3,
    D3DSHADE_FORCE_DWORD = 0x7fffffff
} D3DSHADEMODE;

typedef enum _D3DFILLMODE {
    D3DFILL_POINT = 1,
    D3DFILL_WIREFRAME = 2,
    D3DFILL_SOLID = 3,
    D3DFILL_FORCE_DWORD = 0x7fffffff
} D3DFILLMODE;

typedef enum _D3DZBUFFERTYPE {
    D3DZB_FALSE = 0,
    D3DZB_TRUE = 1,
    D3DZB_USEW = 2,
    D3DZB_FORCE_DWORD = 0x7fffffff
} D3DZBUFFERTYPE;

#define D3DCLEAR_TARGET 0x00000001
#define D3DCLEAR_ZBUFFER 0x00000002
#define D3DCLEAR_STENCIL 0x00000004

#define D3DFVF_RESERVED0 0x001
#define D3DFVF_XYZ 0x002
#define D3DFVF_XYZRHW 0x004
#define D3DFVF_NORMAL 0x010
#define D3DFVF_PSIZE 0x020
#define D3DFVF_DIFFUSE 0x040
#define D3DFVF_SPECULAR 0x080
#define D3DFVF_TEX0 0x000
#define D3DFVF_TEX1 0x100
#define D3DFVF_TEX2 0x200

#define D3DUSAGE_RENDERTARGET 0x00000001
#define D3DUSAGE_DEPTHSTENCIL 0x00000002
#define D3DUSAGE_WRITEONLY 0x00000008
#define D3DUSAGE_DYNAMIC 0x00000200

#define D3DLOCK_READONLY 0x00000010
#define D3DLOCK_DISCARD 0x00002000
#define D3DLOCK_NOOVERWRITE 0x00001000
#define D3DLOCK_NOSYSLOCK 0x00000800

#define D3DPRESENT_INTERVAL_DEFAULT 0x00000000
#define D3DPRESENT_INTERVAL_ONE 0x00000001
#define D3DPRESENT_INTERVAL_TWO 0x00000002
#define D3DPRESENT_INTERVAL_IMMEDIATE 0x80000000

#define D3DPRESENTFLAG_LOCKABLE_BACKBUFFER 0x00000001

#define D3DCREATE_FPU_PRESERVE 0x00000002
#define D3DCREATE_MULTITHREADED 0x00000004
#define D3DCREATE_PUREDEVICE 0x00000010
#define D3DCREATE_SOFTWARE_VERTEXPROCESSING 0x00000020
#define D3DCREATE_HARDWARE_VERTEXPROCESSING 0x00000040
#define D3DCREATE_MIXED_VERTEXPROCESSING 0x00000080

#define D3DADAPTER_DEFAULT 0
#define D3D_SDK_VERSION 32

typedef struct _D3DPRESENT_PARAMETERS_
{
    UINT BackBufferWidth;
    UINT BackBufferHeight;
    D3DFORMAT BackBufferFormat;
    UINT BackBufferCount;
    D3DMULTISAMPLE_TYPE MultiSampleType;
    DWORD MultiSampleQuality;
    D3DSWAPEFFECT SwapEffect;
    HWND hDeviceWindow;
    BOOL Windowed;
    BOOL EnableAutoDepthStencil;
    D3DFORMAT AutoDepthStencilFormat;
    DWORD Flags;
    UINT FullScreen_RefreshRateInHz;
    UINT PresentationInterval;
} D3DPRESENT_PARAMETERS;

typedef struct _D3DDISPLAYMODE
{
    UINT Width;
    UINT Height;
    UINT RefreshRate;
    D3DFORMAT Format;
} D3DDISPLAYMODE;

typedef struct _D3DSURFACE_DESC
{
    D3DFORMAT Format;
    D3DRESOURCETYPE Type;
    DWORD Usage;
    D3DPOOL Pool;
    D3DMULTISAMPLE_TYPE MultiSampleType;
    DWORD MultiSampleQuality;
    UINT Width;
    UINT Height;
} D3DSURFACE_DESC;

typedef struct _D3DVERTEXBUFFER_DESC
{
    D3DFORMAT Format;
    D3DRESOURCETYPE Type;
    DWORD Usage;
    D3DPOOL Pool;
    UINT Size;
    DWORD FVF;
} D3DVERTEXBUFFER_DESC;

typedef struct _D3DLOCKED_RECT
{
    INT Pitch;
    void *pBits;
} D3DLOCKED_RECT;

typedef struct _D3DRASTER_STATUS
{
    BOOL InVBlank;
    UINT ScanLine;
} D3DRASTER_STATUS;

// ---------------------------------------------------------------------------
// d3d9caps.h

typedef struct _D3DVSHADERCAPS2_0
{
    DWORD Caps;
    INT DynamicFlowControlDepth;
    INT NumTemps;
    INT StaticFlowControlDepth;
} D3DVSHADERCAPS2_0;

typedef struct _D3DPSHADERCAPS2_0
{
    DWORD Caps;
    INT DynamicFlowControlDepth;
    INT NumTemps;
    INT StaticFlowControlDepth;
    INT NumInstructionSlots;
} D3DPSHADERCAPS2_0;

// 0x130 bytes.
typedef struct _D3DCAPS9
{
    D3DDEVTYPE DeviceType;
    UINT AdapterOrdinal;
    DWORD Caps;
    DWORD Caps2;
    DWORD Caps3;
    DWORD PresentationIntervals;
    DWORD CursorCaps;
    DWORD DevCaps;
    DWORD PrimitiveMiscCaps;
    DWORD RasterCaps;
    DWORD ZCmpCaps;
    DWORD SrcBlendCaps;
    DWORD DestBlendCaps;
    DWORD AlphaCmpCaps;
    DWORD ShadeCaps;
    DWORD TextureCaps;
    DWORD TextureFilterCaps;
    DWORD CubeTextureFilterCaps;
    DWORD VolumeTextureFilterCaps;
    DWORD TextureAddressCaps;
    DWORD VolumeTextureAddressCaps;
    DWORD LineCaps;
    DWORD MaxTextureWidth;
    DWORD MaxTextureHeight;
    DWORD MaxVolumeExtent;
    DWORD MaxTextureRepeat;
    DWORD MaxTextureAspectRatio;
    DWORD MaxAnisotropy;
    float MaxVertexW;
    float GuardBandLeft;
    float GuardBandTop;
    float GuardBandRight;
    float GuardBandBottom;
    float ExtentsAdjust;
    DWORD StencilCaps;
    DWORD FVFCaps;
    DWORD TextureOpCaps;
    DWORD MaxTextureBlendStages;
    DWORD MaxSimultaneousTextures;
    DWORD VertexProcessingCaps;
    DWORD MaxActiveLights;
    DWORD MaxUserClipPlanes;
    DWORD MaxVertexBlendMatrices;
    DWORD MaxVertexBlendMatrixIndex;
    float MaxPointSize;
    DWORD MaxPrimitiveCount;
    DWORD MaxVertexIndex;
    DWORD MaxStreams;
    DWORD MaxStreamStride;
    DWORD VertexShaderVersion;
    DWORD MaxVertexShaderConst;
    DWORD PixelShaderVersion;
    float PixelShader1xMaxValue;
    DWORD DevCaps2;
    float MaxNpatchTessellationLevel;
    DWORD Reserved5;
    UINT MasterAdapterOrdinal;
    UINT AdapterOrdinalInGroup;
    UINT NumberOfAdaptersInGroup;
    DWORD DeclTypes;
    DWORD NumSimultaneousRTs;
    DWORD StretchRectFilterCaps;
    D3DVSHADERCAPS2_0 VS20Caps;
    D3DPSHADERCAPS2_0 PS20Caps;
    DWORD VertexTextureFilterCaps;
    DWORD MaxVShaderInstructionsExecuted;
    DWORD MaxPShaderInstructionsExecuted;
    DWORD MaxVertexShader30InstructionSlots;
    DWORD MaxPixelShader30InstructionSlots;
} D3DCAPS9;

#define D3DTEXOPCAPS_DISABLE 0x00000001
#define D3DTEXOPCAPS_SELECTARG1 0x00000002
#define D3DTEXOPCAPS_SELECTARG2 0x00000004
#define D3DTEXOPCAPS_MODULATE 0x00000008
#define D3DTEXOPCAPS_MODULATE2X 0x00000010
#define D3DTEXOPCAPS_MODULATE4X 0x00000020
#define D3DTEXOPCAPS_ADD 0x00000040
#define D3DTEXOPCAPS_ADDSIGNED 0x00000080
#define D3DTEXOPCAPS_SUBTRACT 0x00000200

#define D3DPMISCCAPS_BLENDOP 0x00000800
#define D3DPMISCCAPS_SEPARATEALPHABLEND 0x00020000

#define D3DDEVCAPS_HWTRANSFORMANDLIGHT 0x00010000

// ---------------------------------------------------------------------------
// Errors

#define _FACD3D 0x876
#define MAKE_D3DHRESULT(code) MAKE_HRESULT(1, _FACD3D, code)
#define MAKE_D3DSTATUS(code) MAKE_HRESULT(0, _FACD3D, code)

#define D3D_OK S_OK
#define D3DERR_WRONGTEXTUREFORMAT MAKE_D3DHRESULT(2072)
#define D3DERR_UNSUPPORTEDTEXTUREFILTER MAKE_D3DHRESULT(2082)
#define D3DERR_DRIVERINTERNALERROR MAKE_D3DHRESULT(2087)
#define D3DERR_NOTFOUND MAKE_D3DHRESULT(2150)
#define D3DERR_MOREDATA MAKE_D3DHRESULT(2151)
#define D3DERR_DEVICELOST MAKE_D3DHRESULT(2152)
#define D3DERR_DEVICENOTRESET MAKE_D3DHRESULT(2153)
#define D3DERR_NOTAVAILABLE MAKE_D3DHRESULT(2154)
#define D3DERR_OUTOFVIDEOMEMORY MAKE_D3DHRESULT(380)
#define D3DERR_INVALIDDEVICE MAKE_D3DHRESULT(2155)
#define D3DERR_INVALIDCALL MAKE_D3DHRESULT(2156)
#define D3DERR_WASSTILLDRAWING MAKE_D3DHRESULT(540)

// ---------------------------------------------------------------------------
// Interfaces

struct IDirect3D9;
struct IDirect3DDevice9;
struct IDirect3DResource9;
struct IDirect3DBaseTexture9;
struct IDirect3DTexture9;
struct IDirect3DSurface9;
struct IDirect3DVertexBuffer9;
struct IDirect3DIndexBuffer9;
struct IDirect3DVertexShader9;
struct IDirect3DPixelShader9;
struct IDirect3DSwapChain9;

struct IDirect3D9 : public IUnknown
{
    virtual UINT GetAdapterCount() = 0;
    virtual HRESULT GetAdapterDisplayMode(UINT Adapter, D3DDISPLAYMODE *pMode) = 0;
    virtual HRESULT CheckDeviceType(UINT Adapter, D3DDEVTYPE DevType, D3DFORMAT AdapterFormat,
                                    D3DFORMAT BackBufferFormat, BOOL bWindowed) = 0;
    virtual HRESULT CheckDeviceFormat(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT AdapterFormat, DWORD Usage,
                                      D3DRESOURCETYPE RType, D3DFORMAT CheckFormat) = 0;
    virtual HRESULT GetDeviceCaps(UINT Adapter, D3DDEVTYPE DeviceType, D3DCAPS9 *pCaps) = 0;
    virtual HRESULT CreateDevice(UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags,
                                 D3DPRESENT_PARAMETERS *pPresentationParameters,
                                 IDirect3DDevice9 **ppReturnedDeviceInterface) = 0;
};

struct IDirect3DResource9 : public IUnknown
{
    virtual HRESULT GetDevice(IDirect3DDevice9 **ppDevice) = 0;
    virtual DWORD SetPriority(DWORD PriorityNew) = 0;
    virtual DWORD GetPriority() = 0;
    virtual void PreLoad() = 0;
    virtual D3DRESOURCETYPE GetType() = 0;
};

struct IDirect3DBaseTexture9 : public IDirect3DResource9
{
    virtual DWORD SetLOD(DWORD LODNew) = 0;
    virtual DWORD GetLOD() = 0;
    virtual DWORD GetLevelCount() = 0;
};

struct IDirect3DSurface9 : public IDirect3DResource9
{
    virtual HRESULT GetDesc(D3DSURFACE_DESC *pDesc) = 0;
    virtual HRESULT LockRect(D3DLOCKED_RECT *pLockedRect, const RECT *pRect, DWORD Flags) = 0;
    virtual HRESULT UnlockRect() = 0;
};

struct IDirect3DTexture9 : public IDirect3DBaseTexture9
{
    virtual HRESULT GetLevelDesc(UINT Level, D3DSURFACE_DESC *pDesc) = 0;
    virtual HRESULT GetSurfaceLevel(UINT Level, IDirect3DSurface9 **ppSurfaceLevel) = 0;
    virtual HRESULT LockRect(UINT Level, D3DLOCKED_RECT *pLockedRect, const RECT *pRect, DWORD Flags) = 0;
    virtual HRESULT UnlockRect(UINT Level) = 0;
    virtual HRESULT AddDirtyRect(const RECT *pDirtyRect) = 0;
};

struct IDirect3DVertexBuffer9 : public IDirect3DResource9
{
    virtual HRESULT Lock(UINT OffsetToLock, UINT SizeToLock, void **ppbData, DWORD Flags) = 0;
    virtual HRESULT Unlock() = 0;
    virtual HRESULT GetDesc(D3DVERTEXBUFFER_DESC *pDesc) = 0;
};

struct IDirect3DDevice9 : public IUnknown
{
    virtual HRESULT TestCooperativeLevel() = 0;
    virtual UINT GetAvailableTextureMem() = 0;
    virtual HRESULT EvictManagedResources() = 0;
    virtual HRESULT GetDirect3D(IDirect3D9 **ppD3D9) = 0;
    virtual HRESULT GetDeviceCaps(D3DCAPS9 *pCaps) = 0;
    virtual HRESULT GetDisplayMode(UINT iSwapChain, D3DDISPLAYMODE *pMode) = 0;
    virtual HRESULT Reset(D3DPRESENT_PARAMETERS *pPresentationParameters) = 0;
    virtual HRESULT Present(const RECT *pSourceRect, const RECT *pDestRect, HWND hDestWindowOverride,
                            const void *pDirtyRegion) = 0;
    virtual HRESULT GetBackBuffer(UINT iSwapChain, UINT iBackBuffer, D3DBACKBUFFER_TYPE Type,
                                  IDirect3DSurface9 **ppBackBuffer) = 0;
    virtual HRESULT GetRasterStatus(UINT iSwapChain, D3DRASTER_STATUS *pRasterStatus) = 0;
    virtual HRESULT CreateTexture(UINT Width, UINT Height, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool,
                                  IDirect3DTexture9 **ppTexture, HANDLE *pSharedHandle) = 0;
    virtual HRESULT CreateVertexBuffer(UINT Length, DWORD Usage, DWORD FVF, D3DPOOL Pool,
                                       IDirect3DVertexBuffer9 **ppVertexBuffer, HANDLE *pSharedHandle) = 0;
    virtual HRESULT CreateOffscreenPlainSurface(UINT Width, UINT Height, D3DFORMAT Format, D3DPOOL Pool,
                                                IDirect3DSurface9 **ppSurface, HANDLE *pSharedHandle) = 0;
    virtual HRESULT UpdateSurface(IDirect3DSurface9 *pSourceSurface, const RECT *pSourceRect,
                                  IDirect3DSurface9 *pDestinationSurface, const POINT *pDestPoint) = 0;
    virtual HRESULT StretchRect(IDirect3DSurface9 *pSourceSurface, const RECT *pSourceRect,
                                IDirect3DSurface9 *pDestSurface, const RECT *pDestRect,
                                D3DTEXTUREFILTERTYPE Filter) = 0;
    virtual HRESULT SetRenderTarget(DWORD RenderTargetIndex, IDirect3DSurface9 *pRenderTarget) = 0;
    virtual HRESULT GetRenderTarget(DWORD RenderTargetIndex, IDirect3DSurface9 **ppRenderTarget) = 0;
    virtual HRESULT BeginScene() = 0;
    virtual HRESULT EndScene() = 0;
    virtual HRESULT Clear(DWORD Count, const D3DRECT *pRects, DWORD Flags, D3DCOLOR Color, float Z,
                          DWORD Stencil) = 0;
    virtual HRESULT SetTransform(D3DTRANSFORMSTATETYPE State, const D3DMATRIX *pMatrix) = 0;
    virtual HRESULT GetTransform(D3DTRANSFORMSTATETYPE State, D3DMATRIX *pMatrix) = 0;
    virtual HRESULT SetViewport(const D3DVIEWPORT9 *pViewport) = 0;
    virtual HRESULT GetViewport(D3DVIEWPORT9 *pViewport) = 0;
    virtual HRESULT SetRenderState(D3DRENDERSTATETYPE State, DWORD Value) = 0;
    virtual HRESULT GetRenderState(D3DRENDERSTATETYPE State, DWORD *pValue) = 0;
    virtual HRESULT GetTexture(DWORD Stage, IDirect3DBaseTexture9 **ppTexture) = 0;
    virtual HRESULT SetTexture(DWORD Stage, IDirect3DBaseTexture9 *pTexture) = 0;
    virtual HRESULT GetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD *pValue) = 0;
    virtual HRESULT SetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD Value) = 0;
    virtual HRESULT GetSamplerState(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD *pValue) = 0;
    virtual HRESULT SetSamplerState(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD Value) = 0;
    virtual HRESULT DrawPrimitive(D3DPRIMITIVETYPE PrimitiveType, UINT StartVertex, UINT PrimitiveCount) = 0;
    virtual HRESULT DrawPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType, UINT PrimitiveCount,
                                    const void *pVertexStreamZeroData, UINT VertexStreamZeroStride) = 0;
    virtual HRESULT SetFVF(DWORD FVF) = 0;
    virtual HRESULT GetFVF(DWORD *pFVF) = 0;
    virtual HRESULT SetVertexShader(IDirect3DVertexShader9 *pShader) = 0;
    virtual HRESULT SetStreamSource(UINT StreamNumber, IDirect3DVertexBuffer9 *pStreamData, UINT OffsetInBytes,
                                    UINT Stride) = 0;
    virtual HRESULT SetPixelShader(IDirect3DPixelShader9 *pShader) = 0;
};

typedef IDirect3D9 *LPDIRECT3D9, *PDIRECT3D9;
typedef IDirect3DDevice9 *LPDIRECT3DDEVICE9, *PDIRECT3DDEVICE9;
typedef IDirect3DResource9 *LPDIRECT3DRESOURCE9;
typedef IDirect3DBaseTexture9 *LPDIRECT3DBASETEXTURE9;
typedef IDirect3DTexture9 *LPDIRECT3DTEXTURE9, *PDIRECT3DTEXTURE9;
typedef IDirect3DSurface9 *LPDIRECT3DSURFACE9, *PDIRECT3DSURFACE9;
typedef IDirect3DVertexBuffer9 *LPDIRECT3DVERTEXBUFFER9, *PDIRECT3DVERTEXBUFFER9;

extern "C" IDirect3D9 *Direct3DCreate9(UINT SDKVersion);
