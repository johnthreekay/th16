// Loading and unloading .anm files.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "AnmManager.h"
#include "FileSystem.h"
#include "GameErrorContext.h"
#include "Supervisor.h"

// Kept out of line as in AsciiManager.cpp: every object file that calls
// sprintf has to say so, or the linker may keep its inlinable copy.
DECOMP_NOINLINE int __CRTDECL _vsnprintf_l(char *buffer, size_t count, const char *format, _locale_t locale, va_list args);

static_assert(sizeof(AnmRawEntry) == 0x40, "AnmRawEntry size");
static_assert(sizeof(AnmLoadedD3D) == 0x18, "AnmLoadedD3D size");
static_assert(sizeof(AnmLoaded) == 0x13c, "AnmLoaded size");

// Debug logging, compiled out of the release build.
// FUNCTION: TH16 0x470240
HARNESS_CALLED void anm_log(const char *fmt, ...)
{
}

// TODO: the original keeps this and the counts in stack slots; register allocation differs throughout.
// FUNCTION: TH16 0x46cdd0
i32 AnmLoaded::load(const char *path)
{
    char buf[0x104];
    sprintf(buf, "%s", path);
    AnmRawEntry *data = (AnmRawEntry *)file_read_all(buf, NULL, 0);
    if (data == NULL)
    {
        return -1;
    }
    anm_file = data;
    strcpy(name, path);
    i32 num_scripts = data->num_scripts;
    i32 num_sprites = data->num_sprites;
    i32 num_entries = 1;
    for (AnmRawEntry *entry = data; entry->offset_to_next != 0; num_entries++)
    {
        entry = (AnmRawEntry *)((u8 *)entry + entry->offset_to_next);
        num_scripts += entry->num_scripts;
        num_sprites += entry->num_sprites;
    }
    entry_count = num_entries;
    d3d = (AnmLoadedD3D *)malloc(num_entries * sizeof(AnmLoadedD3D));
    memset(d3d, 0, num_entries * sizeof(AnmLoadedD3D));
    sprites = (AnmLoadedSprite *)malloc(num_sprites * sizeof(AnmLoadedSprite));
    scripts = (u8 **)malloc(num_scripts * sizeof(u8 *));
    script_count = num_scripts;
    sprite_count = num_sprites;
    AnmRawEntry *entry = data;
    for (i32 i = 0;; i++)
    {
        if (entry == NULL)
        {
            g_GameErrorContext.fatal("\x83" "A\x83j\x83\x81\x82\xaa\x93\xc7\x82\xdd\x8d\x9e\x82\xdf\x82\xdc\x82\xb9\x82\xf1\x81" "B\x83" "f\x81[\x83^\x82\xaa\x8e\xb8\x82\xed\x82\xea\x82\xc4\x82\xe9\x82\xa9\x89\xf3\x82\xea\x82\xc4\x82\xa2\x82\xdc\x82\xb7\r\n");
            break;
        }
        if (load_entry(i, entry) < 0)
        {
            break;
        }
        if (entry->offset_to_next == 0)
        {
            vms = (AnmVm *)malloc(num_scripts * sizeof(AnmVm));
            memset(vms, 0, num_scripts * sizeof(AnmVm));
            break;
        }
        entry = (AnmRawEntry *)((u8 *)entry + entry->offset_to_next);
    }
    return 0;
}

// FUNCTION: TH16 0x46cf80
AnmLoaded *AnmManager::do_preload_anm(i32 slot, const char *path)
{
    anm_log("::preloadAnim : %s\n", path);
    if (slot >= 0x1f)
    {
        g_GameErrorContext.fatal("\x83" "e\x83N\x83X\x83`\x83\x83\x8ai\x94[\x90\xe6\x82\xaa\x91\xab\x82\xe8\x82\xdc\x82\xb9\x82\xf1\r\n");
        return NULL;
    }
    AnmLoaded *anm = new AnmLoaded;
    loaded_anms[slot] = anm;
    anm->slot_num = slot;
    if (anm->load(path) != 0)
    {
        delete anm;
        return NULL;
    }
    return anm;
}

// TODO: the original frame has 8 unused bytes (sub esp, 8; ours has none), otherwise the same code.
// FUNCTION: TH16 0x46d020
AnmLoaded *__stdcall AnmManager::preload_anm(i32 slot, const char *path)
{
    AnmManager *mgr = g_AnmManager;
    if (mgr->loaded_anms[slot] != NULL)
    {
        anm_log("::preloadAnim already : %s\n", path);
        return mgr->loaded_anms[slot];
    }
    AnmLoaded *anm = mgr->do_preload_anm(slot, path);
    if (anm == NULL)
    {
        return NULL;
    }
    // The loading thread creates the textures; wait for it unless the game
    // is shutting down.
    anm->load_wait = 1;
    do
    {
        if (g_Supervisor.flags & 0x180)
        {
            break;
        }
        Sleep(1);
    } while (anm->load_wait != 0);
    anm_log("::preloadAnimEnd : %s\n", path);
    return anm;
}

// TODO: code matches; our frame leaves 8 unused bytes between buf and the /GS cookie (0x124 vs 0x11c).
// FUNCTION: TH16 0x46d0c0
i32 AnmLoaded::load_entry(i32 index, AnmRawEntry *entry)
{
    char buf[0x10c];
    i32 size;
    if (entry->version != 8)
    {
        g_GameErrorContext.fatal("\x83" "A\x83j\x83\x81\x82\xcc\x83o\x81[\x83W\x83\x87\x83\x93\x82\xaa\x88\xe1\x82\xa2\x82\xdc\x82\xb7\r\n");
        return -1;
    }
    if (!entry->has_data)
    {
        const char *image_path = (const char *)entry + entry->image_path;
        // Names starting with @ are render targets, not files.
        if (image_path[0] != '@')
        {
            sprintf(buf, "%s", image_path);
            void *image = file_read_all(buf, &size, 1);
            if (image == NULL)
            {
                g_GameErrorContext.fatal("\x83" "e\x83N\x83X\x83`\x83\x83 %s \x82\xaa\x93\xc7\x82\xdd\x8d\x9e\x82\xdf\x82\xdc\x82\xb9\x82\xf1\x81" "B\x83" "f\x81[\x83^\x82\xaa\x8e\xb8\x82\xed\x82\xea\x82\xc4\x82\xe9\x82\xa9\x89\xf3\x82\xea\x82\xc4\x82\xa2\x82\xdc\x82\xb7\r\n", image_path);
                return -1;
            }
            d3d[index].src_data_size = size;
            d3d[index].src_data = image;
        }
    }
    return 1;
}

// FUNCTION: TH16 0x46d720
void AnmManager::unload_anm_out_of_line(i32 slot)
{
    if (slot < sizeof(loaded_anms) / sizeof(loaded_anms[0]) && loaded_anms[slot] != NULL)
    {
        loaded_anms[slot]->release();
        delete loaded_anms[slot];
        loaded_anms[slot] = NULL;
    }
}

// GLOBAL: TH16 0x491b90
i32 g_anm_format_bpp[9] = {4, 4, 2, 2, 3, 2, 2, 1, 1};

// GLOBAL: TH16 0x491bb4
D3DFORMAT g_anm_d3d_formats[9] = {
    D3DFMT_UNKNOWN, D3DFMT_A8R8G8B8, D3DFMT_A1R5G5B5, D3DFMT_R5G6B5, D3DFMT_R8G8B8,
    D3DFMT_A4R4G4B4, D3DFMT_A8R3G3B2, D3DFMT_A8, D3DFMT_R3G3B2,
};

// Textures drawn at a smaller window size get scaled down.
#define ANM_TEXTURE_DOWNSCALED(entry) ((entry)->low_res_scale && 2.0f > g_screen_coord_scale)

// FUNCTION: TH16 0x46c920
HARNESS_CALLED i32 AnmManager::load_texture_from_file(AnmLoadedD3D *d3d, i32 format, i32 unused, i32 width,
                                                     i32 height, i32 offset_x, i32 offset_y)
{
    IDirect3DTexture9 *texture;
    IDirect3DSurface9 *src_surface;
    IDirect3DSurface9 *dst_surface;
    D3DSURFACE_DESC desc;

    d3d->flags &= ~ANM_D3D_RENDER_TARGET;
    if (D3DXCreateTextureFromFileInMemoryEx(g_Supervisor.d3d_device, d3d->src_data, d3d->src_data_size, 0, 0, 0, 0,
                                            g_anm_d3d_formats[format], D3DPOOL_MANAGED, D3DX_FILTER_NONE,
                                            D3DX_DEFAULT, 0, NULL, NULL, &texture) != D3D_OK)
    {
        return -1;
    }
    texture->GetSurfaceLevel(0, &src_surface);
    src_surface->GetDesc(&desc);
    convert_texture(texture);
    if (desc.Width == width && desc.Height == height && !ANM_TEXTURE_DOWNSCALED((AnmRawEntry *)d3d->entry))
    {
        d3d->texture = texture;
        if (src_surface != NULL)
        {
            src_surface->Release();
            src_surface = NULL;
        }
    }
    else
    {
        i32 crop_width = width;
        i32 crop_height = height;
        if (offset_x + width > (i32)desc.Width)
        {
            crop_width = desc.Width - offset_x;
        }
        if (offset_y + height > (i32)desc.Height)
        {
            crop_height = desc.Height - offset_y;
        }
        if (ANM_TEXTURE_DOWNSCALED((AnmRawEntry *)d3d->entry))
        {
            width = width * g_screen_coord_scale * 0.5f;
            height = height * g_screen_coord_scale * 0.5f;
        }
        D3DXCreateTexture(g_Supervisor.d3d_device, width, height, 0, 0, g_anm_d3d_formats[format], D3DPOOL_MANAGED,
                          &d3d->texture);
        d3d->texture->GetSurfaceLevel(0, &dst_surface);
        RECT src_rect;
        src_rect.left = offset_x;
        src_rect.top = offset_y;
        src_rect.right = crop_width + offset_x;
        src_rect.bottom = crop_height + offset_y;
        D3DXLoadSurfaceFromSurface(dst_surface, NULL, NULL, src_surface, NULL, &src_rect,
                                   ANM_TEXTURE_DOWNSCALED((AnmRawEntry *)d3d->entry)
                                       ? D3DX_FILTER_TRIANGLE | D3DX_FILTER_MIRROR_U | D3DX_FILTER_MIRROR_V
                                       : D3DX_FILTER_NONE,
                                   0);
        if (src_surface != NULL)
        {
            src_surface->Release();
            src_surface = NULL;
        }
        if (dst_surface != NULL)
        {
            dst_surface->Release();
            dst_surface = NULL;
        }
        if (texture != NULL)
        {
            texture->Release();
            texture = NULL;
        }
    }
    convert_texture(d3d->texture);
    d3d->bytes_per_pixel = g_anm_format_bpp[format];
    return d3d->bytes_per_pixel * width * height;
}

// TODO: the original keeps d3d, data and size in ebx/edi/esi and loads the texture before the stores.
// FUNCTION: TH16 0x46c8b0
HARNESS_CALLED i32 AnmManager::reload_texture(AnmLoadedD3D *d3d, void *data, u32 size, i32 unused_3,
                                              i32 unused_4, i32 unused_5)
{
    IDirect3DSurface9 *surface = NULL;

    d3d->flags &= ~ANM_D3D_RENDER_TARGET;
    d3d->src_data_size = size;
    d3d->texture->GetSurfaceLevel(0, &surface);
    D3DXLoadSurfaceFromFileInMemory(surface, NULL, NULL, data, size, NULL, D3DX_FILTER_NONE, 0, NULL);
    surface->Release();
    convert_texture(d3d->texture);
    d3d->bytes_per_pixel = 4;
    return 0;
}

// TODO: the original keeps raw and format in stack slots and width/height in edi/ebx; register allocation differs.
// FUNCTION: TH16 0x46cb60
i32 __stdcall AnmManager::load_texture_from_data(AnmLoadedD3D *d3d, AnmRawTexture *raw, i32 format, i32 width,
                                                 i32 height)
{
    IDirect3DSurface9 *surface = NULL;

    d3d->flags &= ~ANM_D3D_RENDER_TARGET;
    RECT src_rect = {0, 0, raw->width, raw->height};
    RECT dst_rect = {0, 0, raw->width, raw->height};
    if (ANM_TEXTURE_DOWNSCALED((AnmRawEntry *)d3d->entry))
    {
        dst_rect.right = raw->width * g_screen_coord_scale * 0.5f;
        dst_rect.bottom = raw->height * g_screen_coord_scale * 0.5f;
        width = width * g_screen_coord_scale * 0.5f;
        height = height * g_screen_coord_scale * 0.5f;
    }
    if (D3DXCreateTexture(g_Supervisor.d3d_device, width, height, 1, 0, g_anm_d3d_formats[format], D3DPOOL_MANAGED,
                          &d3d->texture) != D3D_OK)
    {
        if (surface != NULL)
        {
            surface->Release();
        }
        return -1;
    }
    d3d->texture->GetSurfaceLevel(0, &surface);
    D3DXLoadSurfaceFromMemory(surface, NULL, &dst_rect, raw->data, g_anm_d3d_formats[raw->format],
                              raw->width * g_anm_format_bpp[raw->format], NULL, &src_rect,
                              ANM_TEXTURE_DOWNSCALED((AnmRawEntry *)d3d->entry)
                                  ? D3DX_FILTER_TRIANGLE | D3DX_FILTER_MIRROR_U | D3DX_FILTER_MIRROR_V
                                  : D3DX_FILTER_NONE,
                              0);
    d3d->bytes_per_pixel = g_anm_format_bpp[format];
    if (surface != NULL)
    {
        surface->Release();
    }
    return g_anm_format_bpp[format] * width * height;
}

// FUNCTION: TH16 0x46cd30
i32 __stdcall AnmManager::create_empty_texture(AnmLoadedD3D *d3d, i32 width, i32 height, i32 format)
{
    d3d->flags &= ~ANM_D3D_RENDER_TARGET;
    D3DXCreateTexture(g_Supervisor.d3d_device, width, height, 1, 0, g_anm_d3d_formats[format], D3DPOOL_MANAGED,
                      &d3d->texture);
    d3d->bytes_per_pixel = g_anm_format_bpp[format];
    return d3d->bytes_per_pixel * width * height;
}

// FUNCTION: TH16 0x46cd80
i32 __stdcall AnmManager::create_render_target(AnmLoadedD3D *d3d, i32 width, i32 height)
{
    d3d->flags |= ANM_D3D_RENDER_TARGET;
    g_Supervisor.d3d_device->CreateTexture(width, height, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT,
                                           &d3d->texture, NULL);
    d3d->bytes_per_pixel = g_Supervisor.present_params.BackBufferFormat == D3DFMT_A8R8G8B8 ? 4 : 2;
    return 0;
}

// TODO: the original reloads g_resolution_x/y after storing them into the entry; register allocation and block order differ.
// FUNCTION: TH16 0x46d3b0
HARNESS_CALLED i32 AnmManager::setup_entry(AnmLoaded *anm, i32 index, i32 first_sprite, i32 first_script,
                                           AnmRawEntry *entry)
{
    D3DSURFACE_DESC desc;
    AnmLoadedSprite sprite;

    if (entry == NULL)
    {
        g_GameErrorContext.fatal("\x83" "A\x83j\x83\x81\x82\xaa\x93\xc7\x82\xdd\x8d\x9e\x82\xdf\x82\xdc\x82\xb9\x82\xf1\x81" "B\x83" "f\x81[\x83^\x82\xaa\x8e\xb8\x82\xed\x82\xea\x82\xc4\x82\xe9\x82\xa9\x89\xf3\x82\xea\x82\xc4\x82\xa2\x82\xdc\x82\xb7\r\n");
        return -1;
    }
    if (entry->version != 8)
    {
        g_GameErrorContext.fatal("\x83" "A\x83j\x83\x81\x82\xcc\x83o\x81[\x83W\x83\x87\x83\x93\x82\xaa\x88\xe1\x82\xa2\x82\xdc\x82\xb7\r\n");
        return -1;
    }
    anm->d3d[index].entry = entry;
    if (!entry->has_data)
    {
        const char *image_path = (const char *)entry + entry->image_path;
        if (image_path[0] == '@')
        {
            if (image_path[1] == 'R')
            {
                entry->width = g_resolution_x;
                entry->height = g_resolution_y;
                create_render_target(&anm->d3d[index], g_resolution_x, g_resolution_y);
            }
            else
            {
                anm->texture_memory +=
                    create_empty_texture(&anm->d3d[index], entry->width, entry->height, entry->format);
            }
        }
        else
        {
            i32 size = g_AnmManager->load_texture_from_file(&anm->d3d[index], entry->format, 0, entry->width,
                                                            entry->height, (i16)entry->offset_x, (i16)entry->offset_y);
            if (size < 0)
            {
                g_GameErrorContext.fatal("\x83" "e\x83N\x83X\x83`\x83\x83 %s \x82\xaa\x8d\xec\x90\xac\x82\xc5\x82\xab\x82\xdc\x82\xb9\x82\xf1\x81" "B\x83" "f\x81[\x83^\x82\xaa\x8e\xb8\x82\xed\x82\xea\x82\xc4\x82\xe9\x82\xa9\x89\xf3\x82\xea\x82\xc4\x82\xa2\x82\xdc\x82\xb7\r\n", image_path);
                return -1;
            }
            anm->texture_memory += size;
        }
    }
    else
    {
        i32 size = load_texture_from_data(&anm->d3d[index], (AnmRawTexture *)((u8 *)entry + entry->texture),
                                          entry->format, entry->width, entry->height);
        if (size < 0)
        {
            g_GameErrorContext.fatal("\x83" "e\x83N\x83X\x83`\x83\x83\x82\xaa\x8d\xec\x90\xac\x82\xc5\x82\xab\x82\xdc\x82\xb9\x82\xf1\x81" "B\x83" "f\x81[\x83^\x82\xaa\x8e\xb8\x82\xed\x82\xea\x82\xc4\x82\xe9\x82\xa9\x89\xf3\x82\xea\x82\xc4\x82\xa2\x82\xdc\x82\xb7\r\n");
            return -1;
        }
        anm->texture_memory += size;
    }
    anm->d3d[index].texture->GetLevelDesc(0, &desc);

    u32 *offsets = (u32 *)(entry + 1);
    for (i32 i = 0; i < entry->num_sprites; i++, offsets++)
    {
        AnmRawSprite *raw = (AnmRawSprite *)((u8 *)entry + *offsets);
        sprite.anm_slot = anm->slot_num;
        sprite.image_file_num_in_all = anm->slot_num << 8 | index;
        sprite.image_file_num_in_anm = index;
        sprite.bitmap_width = desc.Width;
        sprite.pixel_scale.x = sprite.bitmap_width / entry->width;
        sprite.bitmap_height = desc.Height;
        sprite.pixel_scale.y = sprite.bitmap_height / entry->height;
        sprite.start_pixel_inclusive.x = raw->x * sprite.pixel_scale.x;
        sprite.start_pixel_inclusive.y = raw->y * sprite.pixel_scale.y;
        sprite.end_pixel_exclusive.x = (raw->width + raw->x) * sprite.pixel_scale.x;
        sprite.end_pixel_exclusive.y = (raw->height + raw->y) * sprite.pixel_scale.y;
        anm->load_sprite(first_sprite++, &sprite);
    }
    for (i32 i = 0; i < entry->num_scripts; i++, offsets += 2)
    {
        anm->scripts[first_script + i] = (u8 *)entry + offsets[1];
    }
    return 1;
}

// TODO: the inlined timer resets address [vms + offset] where ours uses [offset + vms] (reccmp: 100%*).
// FUNCTION: TH16 0x46d1c0
AnmLoaded *__stdcall AnmManager::load_next_entry(AnmLoaded *anm)
{
    i32 first_script = 0;
    i32 i = 0;
    i32 first_sprite = 0;
    i32 index = 0;
    AnmRawEntry *entry = (AnmRawEntry *)anm->anm_file;
    BOOL loaded = FALSE;
    while (true)
    {
        if (i == anm->load_wait - 1)
        {
            if (g_AnmManager->setup_entry(anm, index, first_sprite, first_script, entry) < 0)
            {
                anm->load_wait = 0;
                return NULL;
            }
            loaded = TRUE;
        }
        index++;
        first_sprite += entry->num_sprites;
        first_script += entry->num_scripts;
        if (entry->offset_to_next == 0)
        {
            break;
        }
        i++;
        entry = (AnmRawEntry *)((u8 *)entry + entry->offset_to_next);
        if (i == anm->load_wait || loaded)
        {
            anm->load_wait++;
            return anm;
        }
    }
    for (i32 j = 0; j < anm->script_count; j++)
    {
        (anm->vms + j)->wipe();
        anm->init_script_vm(anm->vms + j, j);
        (anm->vms + j)->script_time = -1;
        (anm->vms + j)->time_in_script = -1;
        (anm->vms + j)->run();
    }
    anm->load_wait = 0;
    return anm;
}

// TODO: the original loads the last dword of the sprite before the first movups store.
// FUNCTION: TH16 0x46d8a0
void AnmLoaded::load_sprite(i32 index, AnmLoadedSprite *sprite)
{
    sprites[index] = *sprite;
    sprites[index].uv_start.x = sprites[index].start_pixel_inclusive.x / sprites[index].bitmap_width;
    sprites[index].uv_end.x = sprites[index].end_pixel_exclusive.x / sprites[index].bitmap_width;
    sprites[index].uv_start.y = sprites[index].start_pixel_inclusive.y / sprites[index].bitmap_height;
    sprites[index].uv_end.y = sprites[index].end_pixel_exclusive.y / sprites[index].bitmap_height;
    sprites[index].sprite_width =
        (sprites[index].end_pixel_exclusive.x - sprites[index].start_pixel_inclusive.x) / sprite->pixel_scale.x;
    sprites[index].sprite_height =
        (sprites[index].end_pixel_exclusive.y - sprites[index].start_pixel_inclusive.y) / sprite->pixel_scale.y;
}

// FUNCTION: TH16 0x46d690
HARNESS_CALLED i32 AnmManager::service_pending_loads()
{
    for (u32 i = 0; i < 0x1f; i++)
    {
        if (loaded_anms[i] == NULL)
        {
            continue;
        }
        if (loaded_anms[i]->unload_requested)
        {
            unload_anm(i);
            loaded_anms[i]->unload_requested = 0;
        }
        else if (loaded_anms[i]->load_wait != 0)
        {
            return load_next_entry(loaded_anms[i]) != NULL ? 0 : -1;
        }
    }
    return 0;
}

// FUNCTION: TH16 0x46d770
void AnmLoaded::release()
{
    if (anm_file == NULL)
    {
        return;
    }
    g_AnmManager->disable_vms_from_anm_file(this);
    for (i32 i = 0; i < entry_count; i++)
    {
        AnmLoadedD3D *entry = &d3d[i];
        if (entry->texture != NULL)
        {
            entry->texture->Release();
            entry->texture = NULL;
        }
        if (entry->src_data != NULL)
        {
            free(entry->src_data);
            entry->src_data = NULL;
        }
    }
    if (d3d != NULL)
    {
        free(d3d);
        d3d = NULL;
    }
    if (sprites != NULL)
    {
        free(sprites);
        sprites = NULL;
    }
    if (scripts != NULL)
    {
        free(scripts);
        scripts = NULL;
    }
    if (unk_138 != NULL)
    {
        free(unk_138);
        unk_138 = NULL;
    }
    if (anm_file != NULL)
    {
        free(anm_file);
        anm_file = NULL;
    }
    if (vms != NULL)
    {
        free(vms);
        vms = NULL;
    }
}

// One anchoring's quad in the vertex buffer.
struct AnmQuadXyzTex
{
    RenderVertexXyzTex v[4];
};

// FUNCTION: TH16 0x46b900
void AnmManager::setup_vertex_buffer()
{
    AnmManager *mgr = g_AnmManager;
    RenderVertexXyzTex *quad = mgr->fog_unit_quad;
    quad[0].pos.x = quad[2].pos.x = -128.0f;
    quad[1].pos.x = quad[3].pos.x = 128.0f;
    quad[0].pos.y = quad[1].pos.y = -128.0f;
    quad[2].pos.y = quad[3].pos.y = 128.0f;
    quad[2].pos.z = quad[3].pos.z = 0.0f;
    quad[0].pos.z = quad[1].pos.z = 0.0f;
    quad[0].uv.x = quad[2].uv.x = 0.0f;
    quad[1].uv.x = quad[3].uv.x = 1.0f;
    quad[0].uv.y = quad[1].uv.y = 0.0f;
    quad[2].uv.y = quad[3].uv.y = 1.0f;
    g_unit_quad_xyz[0].pos = quad[0].pos;
    g_unit_quad_xyz[1].pos = quad[1].pos;
    g_unit_quad_xyz[2].pos = quad[2].pos;
    g_unit_quad_xyz[3].pos = quad[3].pos;
    g_unit_quad_xyz[0].uv.x = quad[0].uv.x;
    g_unit_quad_xyz[0].uv.y = quad[0].uv.y;
    g_unit_quad_xyz[1].uv.x = quad[1].uv.x;
    g_unit_quad_xyz[1].uv.y = quad[1].uv.y;
    g_unit_quad_xyz[2].uv.x = quad[2].uv.x;
    g_unit_quad_xyz[2].uv.y = quad[2].uv.y;
    g_unit_quad_xyz[3].uv.x = quad[3].uv.x;
    g_unit_quad_xyz[3].uv.y = quad[3].uv.y;
    IDirect3DDevice9 *device = g_Supervisor.d3d_device;
    device->CreateVertexBuffer(sizeof(mgr->fog_unit_quad) * 9, 0, D3DFVF_XYZ | D3DFVF_TEX1, D3DPOOL_MANAGED,
                               &mgr->vertex_buffer, NULL);
    AnmQuadXyzTex *buffer;
    mgr->vertex_buffer->Lock(0, 0, (void **)&buffer, 0);
    // One copy per anchoring, at (anchor_y * 3 + anchor_x) quads in.
    buffer[0] = *(AnmQuadXyzTex *)quad;
    quad[0].pos.y += 128.0f;
    quad[1].pos.y += 128.0f;
    quad[2].pos.y += 128.0f;
    quad[3].pos.y += 128.0f;
    buffer[3] = *(AnmQuadXyzTex *)quad;
    quad[0].pos.y -= 256.0f;
    quad[1].pos.y -= 256.0f;
    quad[2].pos.y -= 256.0f;
    quad[3].pos.y -= 256.0f;
    buffer[6] = *(AnmQuadXyzTex *)quad;
    quad[0].pos.x += 128.0f;
    quad[1].pos.x += 128.0f;
    quad[2].pos.x += 128.0f;
    quad[3].pos.x += 128.0f;
    quad[0].pos.y += 128.0f;
    quad[1].pos.y += 128.0f;
    quad[2].pos.y += 128.0f;
    quad[3].pos.y += 128.0f;
    buffer[1] = *(AnmQuadXyzTex *)quad;
    quad[0].pos.y += 128.0f;
    quad[1].pos.y += 128.0f;
    quad[2].pos.y += 128.0f;
    quad[3].pos.y += 128.0f;
    buffer[4] = *(AnmQuadXyzTex *)quad;
    quad[0].pos.y -= 256.0f;
    quad[1].pos.y -= 256.0f;
    quad[2].pos.y -= 256.0f;
    quad[3].pos.y -= 256.0f;
    buffer[7] = *(AnmQuadXyzTex *)quad;
    quad[0].pos.x -= 256.0f;
    quad[1].pos.x -= 256.0f;
    quad[2].pos.x -= 256.0f;
    quad[3].pos.x -= 256.0f;
    quad[0].pos.y += 128.0f;
    quad[1].pos.y += 128.0f;
    quad[2].pos.y += 128.0f;
    quad[3].pos.y += 128.0f;
    buffer[2] = *(AnmQuadXyzTex *)quad;
    quad[0].pos.y += 128.0f;
    quad[1].pos.y += 128.0f;
    quad[2].pos.y += 128.0f;
    quad[3].pos.y += 128.0f;
    buffer[5] = *(AnmQuadXyzTex *)quad;
    quad[0].pos.y -= 256.0f;
    quad[1].pos.y -= 256.0f;
    quad[2].pos.y -= 256.0f;
    quad[3].pos.y -= 256.0f;
    buffer[8] = *(AnmQuadXyzTex *)quad;
    mgr->vertex_buffer->Unlock();
    g_Supervisor.d3d_device->SetStreamSource(0, mgr->vertex_buffer, 0, sizeof(RenderVertexXyzTex));
}
