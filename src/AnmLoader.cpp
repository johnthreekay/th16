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

// TODO: the original frame has 4 more bytes and saves esi at the start.
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

// TODO: the original frame has 8 bytes of unused slots and saves esi/edi at the start.
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

// TODO: the original frame has 4 more bytes and saves esi/edi at the start.
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

// FUNCTION: TH16 0x46d690
HARNESS_CALLED i32 AnmManager::sub_46d690()
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
