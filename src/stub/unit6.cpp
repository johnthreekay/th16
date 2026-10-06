// Placeholders for functions unit 6 calls but other units own.
#include "../AnmManager.h"

// STUB: TH16 0x465a80
void AnmManager::flush_sprites()
{
}

// STUB: TH16 0x468490
void AnmManager::draw_vm(AnmVm *vm)
{
}

#include "../Gui.h"

// GLOBAL: TH16 0x4a6dcc
Gui *g_Gui;

// STUB: TH16 0x42c280
void Gui::update_lives(i32 lives, i32 fragments)
{
}

// STUB: TH16 0x42c390
void Gui::update_bombs(i32 bombs, i32 fragments)
{
}

// STUB: TH16 0x42bcf0
void Gui::sub_42bcf0(i32 unk, i32 kind)
{
}

// STUB: TH16 0x45edb0
void AnmLoaded::set_sprite(AnmVm *vm, i32 sprite)
{
}
