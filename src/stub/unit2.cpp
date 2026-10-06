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

// GLOBAL: TH16 0x4a6ef8
Player *g_Player;

// STUB: TH16 0x42c600
void Gui::update_season_gauge()
{
}

// STUB: TH16 0x4440e0
void PlayerInner::repopulate_options()
{
}

// STUB: TH16 0x45f980
i32 AnmVm::run()
{
    return 0;
}

// Opaque sink for the /GL placeholders in src/placeholder/unit2.cpp.
void placeholder_sink(int a, float b)
{
}
