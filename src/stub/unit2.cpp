// Placeholders for functions unit 2 (Stage, Bomb) calls that other units
// own, plus the globals they live in. Compiled without /GL, so these stay
// opaque calls with standard conventions.
#include "../AnmManager.h"
#include "../AsciiManager.h"
#include "../EnemyManager.h"
#include "../Gui.h"
#include "../Player.h"
#include "../Spellcard.h"
#include "../Timer.h"

// GLOBAL: TH16 0x4c0f48
AnmManager *g_AnmManager;

// GLOBAL: TH16 0x4a6d98
AsciiManager *g_AsciiManager;

// GLOBAL: TH16 0x4a6db0
Spellcard *g_Spellcard;

// GLOBAL: TH16 0x4a6dc0
EnemyManager *g_EnemyManager;

// GLOBAL: TH16 0x4a6dcc
Gui *g_Gui;

// GLOBAL: TH16 0x4a6ef8
Player *g_Player;

// STUB: TH16 0x406490
void Timer::set_value(i32 value)
{
    set(value);
}

// STUB: TH16 0x408260
void AsciiManager::sprintf(D3DXVECTOR3 *pos, const char *fmt, ...)
{
}

// STUB: TH16 0x42c390
void Gui::update_bombs(i32 bombs, i32 fragments)
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

// STUB: TH16 0x46efa0
AnmVm *AnmManager::get_vm_with_id(AnmId id)
{
    return NULL;
}

// Opaque sink for the /GL placeholders in src/placeholder/unit2.cpp.
void placeholder_sink(int a, float b)
{
}
