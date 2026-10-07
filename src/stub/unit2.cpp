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


// Opaque sink for the /GL placeholders in src/placeholder/unit2.cpp.
void placeholder_sink(int a, float b)
{
}
