// Placeholders for functions unit 4's code calls but that are not
// decompiled yet. Compiled without /GL, so they stay opaque.
#include "../Enemy.h"
#include "unit4_extern.h"

// STUB: TH16 0x41ba10
EnemyInf::~EnemyInf()
{
}

// STUB: TH16 0x474740
int SptResourceInf::find_sub_by_name(const char *name) throw()
{
    return 0;
}

// STUB: TH16 0x41d1e0
int EnemyInf::on_tick()
{
    return 0;
}

// GLOBAL: TH16 0x4a5788
f32 g_game_speed;

// GLOBAL: TH16 0x490eb0
f32 *g_timer_speeds[1] = {&g_game_speed};

// GLOBAL: TH16 0x4a6dd4
GameThread *g_GameThread;

// GLOBAL: TH16 0x4a6ef8
Player *g_Player;
