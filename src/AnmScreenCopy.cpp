// AnmManager's queue of back buffer copies, used by the pause menu to keep
// a picture of the game behind it.
#include <stddef.h>

#include "AnmManager.h"

static_assert(offsetof(AnmManager, screen_copies) == 0x20, "AnmManager::screen_copies");
static_assert(offsetof(AnmManager, stat_scripts_started) == 0xc0, "AnmManager::stat_scripts_started");

// FUNCTION: TH16 0x440c60
i32 AnmManager::queue_screen_copy(i32 anm_slot, i32 entry, i32 src_x, i32 src_y, i32 src_width, i32 src_height,
                                  i32 dst_x, i32 dst_y, i32 dst_width, i32 dst_height)
{
    for (u32 i = 0; i < 4; i++)
    {
        if (screen_copies[i].anm_slot < 0)
        {
            AnmScreenCopy *copy = &screen_copies[i];
            copy->anm_slot = anm_slot;
            copy->entry = entry;
            copy->src_x = src_x;
            copy->src_y = src_y;
            copy->src_width = src_width;
            copy->src_height = src_height;
            copy->dst_x = dst_x;
            copy->dst_y = dst_y;
            copy->dst_width = dst_width;
            copy->dst_height = dst_height;
            return 0;
        }
    }
    return 0;
}

// TODO: register allocation: the original keeps the id in esi (pushing it
// from there) and the manager in edi, with no ebx.
// FUNCTION: TH16 0x440cd0
HARNESS_CALLED i32 AnmManager::copy_screen_to_sprite(AnmId id, i32 src_x, i32 src_y, i32 src_width, i32 src_height)
{
    AnmVm *vm = get_vm_or_clear(id);
    AnmLoadedSprite *sprite = &loaded_anms[vm->anm_loaded_index]->sprites[vm->sprite_id];
    return queue_screen_copy(vm->anm_loaded_index, sprite->image_file_num_in_anm, src_x, src_y, src_width,
                             src_height, (i32)sprite->start_pixel_inclusive.x, (i32)sprite->start_pixel_inclusive.y,
                             (i32)sprite->sprite_width, (i32)sprite->sprite_height);
}
