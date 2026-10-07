// AnmManager::convert_texture: fixing up the colors of transparent texels.
#include "AnmManager.h"

// Pixel layouts of the texture formats convert_texture handles.
struct TexelA8R8G8B8
{
    u8 b;
    u8 g;
    u8 r;
    u8 a;
};

struct TexelA1R5G5B5
{
    u16 b : 5;
    u16 g : 5;
    u16 r : 5;
    u16 a : 1;
};

struct TexelA4R4G4B4
{
    u16 b : 4;
    u16 g : 4;
    u16 r : 4;
    u16 a : 4;
};

struct TexelA8R3G3B2
{
    u16 b : 2;
    u16 g : 3;
    u16 r : 3;
    u16 a : 8;
};

// Gives every fully transparent texel the average color of its opaque
// neighbors, so that filtering does not blend in black at sprite edges.
#define BLEED_TRANSPARENT_TEXELS(T)                                                                                 \
    for (u32 y = 0; y < desc.Height; y++)                                                                           \
    {                                                                                                               \
        T *texel = (T *)((u8 *)locked.pBits + locked.Pitch * y);                                                    \
        for (u32 x = 0; x < desc.Width; x++, texel++)                                                               \
        {                                                                                                           \
            if (texel->a != 0)                                                                                      \
            {                                                                                                       \
                continue;                                                                                           \
            }                                                                                                       \
            u32 r = 0;                                                                                              \
            u32 g = 0;                                                                                              \
            u32 b = 0;                                                                                              \
            u32 count = 0;                                                                                          \
            if (x != 0 && texel[-1].a != 0)                                                                         \
            {                                                                                                       \
                r = texel[-1].r;                                                                                    \
                g = texel[-1].g;                                                                                    \
                b = texel[-1].b;                                                                                    \
                count = 1;                                                                                          \
            }                                                                                                       \
            if (x < desc.Width - 1 && texel[1].a != 0)                                                              \
            {                                                                                                       \
                r += texel[1].r;                                                                                    \
                g += texel[1].g;                                                                                    \
                b += texel[1].b;                                                                                    \
                count++;                                                                                            \
            }                                                                                                       \
            if (y > 0)                                                                                              \
            {                                                                                                       \
                T *above = texel - locked.Pitch / (i32)sizeof(T);                                                   \
                if (above->a != 0)                                                                                  \
                {                                                                                                   \
                    r += above->r;                                                                                  \
                    g += above->g;                                                                                  \
                    b += above->b;                                                                                  \
                    count++;                                                                                        \
                }                                                                                                   \
            }                                                                                                       \
            if (y < desc.Height - 1)                                                                                \
            {                                                                                                       \
                T *below = texel + locked.Pitch / (i32)sizeof(T);                                                   \
                if (below->a != 0)                                                                                  \
                {                                                                                                   \
                    r += below->r;                                                                                  \
                    g += below->g;                                                                                  \
                    b += below->b;                                                                                  \
                    count++;                                                                                        \
                }                                                                                                   \
            }                                                                                                       \
            if (count > 1)                                                                                          \
            {                                                                                                       \
                r /= count;                                                                                         \
                g /= count;                                                                                         \
                b /= count;                                                                                         \
            }                                                                                                       \
            texel->r = r;                                                                                           \
            texel->g = g;                                                                                           \
            texel->b = b;                                                                                           \
        }                                                                                                           \
    }

// FUNCTION: TH16 0x46c0d0
void __stdcall AnmManager::convert_texture(IDirect3DTexture9 *texture)
{
    IDirect3DSurface9 *surface = NULL;
    texture->GetSurfaceLevel(0, &surface);
    D3DSURFACE_DESC desc;
    surface->GetDesc(&desc);
    struct
    {
        D3DLOCKED_RECT rect;
        i32 pad;
    } locked_;
    surface->LockRect(&locked_.rect, NULL, 0);
#define locked locked_.rect
    switch (desc.Format)
    {
    case D3DFMT_A8R8G8B8:
        BLEED_TRANSPARENT_TEXELS(TexelA8R8G8B8);
        break;
    case D3DFMT_A1R5G5B5:
        BLEED_TRANSPARENT_TEXELS(TexelA1R5G5B5);
        break;
    case D3DFMT_A4R4G4B4:
        BLEED_TRANSPARENT_TEXELS(TexelA4R4G4B4);
        break;
    case D3DFMT_A8R3G3B2:
        BLEED_TRANSPARENT_TEXELS(TexelA8R3G3B2);
        break;
    }
    surface->UnlockRect();
    surface->Release();
}
