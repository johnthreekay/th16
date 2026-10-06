// Placeholders for functions unit 6 calls but other units own.
#include "../AnmManager.h"

// GLOBAL: TH16 0x4c0f48
AnmManager *g_AnmManager;

// STUB: TH16 0x465a80
void AnmManager::flush_sprites()
{
}

// STUB: TH16 0x468490
void AnmManager::draw_vm(AnmVm *vm)
{
}

// STUB: TH16 0x46efa0
AnmVm *AnmManager::get_vm_with_id(AnmId id)
{
    return NULL;
}

// STUB: TH16 0x46f0b0
void __stdcall AnmManager::interrupt_tree(AnmId id, i32 interrupt)
{
}

// STUB: TH16 0x46d020
AnmLoaded *__stdcall AnmManager::preload_anm(i32 slot, const char *path)
{
    return NULL;
}

// STUB: TH16 0x46d690
i32 AnmManager::sub_46d690()
{
    return 0;
}

#include "../Gui.h"

// GLOBAL: TH16 0x4a6dcc
Gui *g_Gui;

// STUB: TH16 0x42bcf0
void Gui::sub_42bcf0(i32 unk, i32 kind)
{
}

// STUB: TH16 0x45edb0
void AnmLoaded::set_sprite(AnmVm *vm, i32 sprite)
{
}
