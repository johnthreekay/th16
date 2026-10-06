// Placeholders for functions unit 2 (Stage, Bomb) calls that other units
// own, plus the globals they live in that no other file defines yet.
// Compiled without /GL, so these stay opaque calls with standard
// conventions.
#include "../AnmManager.h"
#include "../AsciiManager.h"
#include "../BulletManager.h"
#include "../EnemyManager.h"
#include "../Gui.h"
#include "../Player.h"
#include "../Spellcard.h"
#include "../ZunTimer.h"

// GLOBAL: TH16 0x4a6dac
BulletManager *g_BulletManager;

// GLOBAL: TH16 0x4a6db0
Spellcard *g_Spellcard;

// GLOBAL: TH16 0x4a6ef8
Player *g_Player;

// STUB: TH16 0x407b20
void AnmLoaded::copy_vm(AnmVm *vm, i32 script)
{
}

// STUB: TH16 0x408260
void AsciiManager::sprintf(D3DXVECTOR3 *pos, const char *fmt, ...)
{
}

// STUB: TH16 0x42c600
void Gui::update_season_gauge()
{
}

// STUB: TH16 0x4440e0
void PlayerInner::repopulate_options()
{
}

// STUB: TH16 0x45f980
void AnmVm::run()
{
}

// STUB: TH16 0x46e7d0
AnmId __stdcall AnmManager::insert_in_world_list_back(AnmVm *vm)
{
    return vm->id;
}

// STUB: TH16 0x46f600
AnmVm *AnmManager::allocate_vm()
{
    return NULL;
}

// Opaque sink for the /GL placeholders in src/placeholder/unit2.cpp.
void placeholder_sink(int a, float b)
{
}
