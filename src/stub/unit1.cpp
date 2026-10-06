// Placeholders for functions unit 1 calls but has not decompiled yet, and
// globals it uses that belong to other modules.
#include "../AnmManager.h"
#include "../AsciiManager.h"
#include "../Supervisor.h"

// GLOBAL: TH16 0x4c0f48
AnmManager *g_AnmManager;

// GLOBAL: TH16 0x4d9d34
f32 g_screen_coord_scale = 1.0f;

// STUB: TH16 0x46d020
AnmLoaded *__stdcall AnmManager::preload_anm(i32 slot, const char *name)
{
    return (AnmLoaded *)name + slot;
}

// STUB: TH16 0x46d770
AnmLoaded::~AnmLoaded() noexcept(false)
{
    slot_num = 0;
}

// STUB: TH16 0x45edb0
void AnmLoaded::set_sprite(AnmVm *vm, i32 sprite)
{
    vm->sprite_id = sprite;
}

// STUB: TH16 0x408650
void AsciiInf::draw_string(AsciiStr *str)
{
    str->font_id = 0;
}

// STUB: TH16 0x465a80
void AnmManager::flush_sprites()
{
    unk_d0 = 0.0f;
}

// STUB: TH16 0x43c710
void __stdcall Supervisor::swap_transform_matrices(Camera *camera)
{
    camera->camera_index = 0;
}

// STUB: TH16 0x46efa0
AnmVm *AnmManager::get_vm_with_id(AnmId id)
{
    return (AnmVm *)(unk_0 + id.id);
}
