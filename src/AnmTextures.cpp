#include <d3dx9.h>

#include "AnmManager.h"
#include "Supervisor.h"

// FUNCTION: TH16 0x4595c0
HARNESS_CALLED void AnmManager::release_textures()
{
    AnmLoaded **anm = loaded_anms;
    for (i32 i = 0; i < ANM_SLOT_COUNT; i++, anm++)
    {
        if (*anm == NULL)
        {
            continue;
        }
        for (i32 j = 0; j < (*anm)->entry_count; j++)
        {
            if (((*anm)->d3d[j].flags & ANM_D3D_RENDER_TARGET) && (*anm)->d3d[j].texture != NULL)
            {
                (*anm)->d3d[j].texture->Release();
                (*anm)->d3d[j].texture = NULL;
            }
        }
    }
}

inline void AnmLoadedD3D::create_render_target(i32 width, i32 height)
{
    flags |= 1;
    supervisor_d3d_device()->CreateTexture(width, height, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
                                           D3DPOOL_DEFAULT, &texture, NULL);
    bytes_per_pixel = g_Supervisor.present_params.BackBufferFormat == D3DFMT_A8R8G8B8 ? 4 : 2;
}

// TODO: ours swaps esi and edi (the entry and the texture width); create_render_target's call through supervisor_d3d_device() gave it ebx back.
// FUNCTION: TH16 0x459640
HARNESS_CALLED void AnmManager::create_d3d_textures_for_loaded_anms()
{
    AnmLoaded **anm = loaded_anms;
    for (i32 i = 0; i < ANM_SLOT_COUNT; i++, anm++)
    {
        if (*anm == NULL)
        {
            continue;
        }
        for (i32 j = 0; j < (*anm)->entry_count; j++)
        {
            AnmLoadedD3D *d3d = &(*anm)->d3d[j];
            if (*(u8 *)&d3d->flags & ANM_D3D_RENDER_TARGET)
            {
                AnmRawEntry *entry = (AnmRawEntry *)d3d->entry;
                d3d->create_render_target(entry->width, entry->height);
            }
        }
    }
}

// FUNCTION: TH16 0x459700
HARNESS_CALLED void AnmManager::take_screenshots()
{
    for (i32 i = 0; i < 4; i++)
    {
        AnmScreenCopy *s = &screen_copies[i];
        if (s->anm_slot < 0)
        {
            continue;
        }
        if (loaded_anms[s->anm_slot]->d3d[s->entry].texture != NULL)
        {
            flush_sprites();
            IDirect3DSurface9 *surface;
            if (loaded_anms[s->anm_slot]->d3d[s->entry].texture->GetSurfaceLevel(0, &surface) == D3D_OK)
            {
                RECT src;
                RECT dst;
                src.left = s->src_x;
                src.top = s->src_y;
                src.right = s->src_x + s->src_width;
                src.bottom = s->src_y + s->src_height;
                dst.left = s->dst_x;
                dst.top = s->dst_y;
                dst.right = s->dst_x + s->dst_width;
                dst.bottom = s->dst_y + s->dst_height;
                if (D3DXLoadSurfaceFromSurface(surface, NULL, &dst, g_Supervisor.back_buffer, NULL, &src,
                                               D3DX_FILTER_POINT, 0) == D3D_OK)
                {
                    loaded_anms[s->anm_slot]->d3d[s->entry].texture->AddDirtyRect(NULL);
                }
                surface->Release();
            }
        }
        s->anm_slot = -1;
    }
}
