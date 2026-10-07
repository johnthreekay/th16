// Placeholders for functions the wave 4 interpreter work (EnemyData's ECL
// instructions) calls but that are not decompiled yet, and data LTCG must
// not see. Compiled without /GL.
#include <d3dx9math.h>

#include "../Enemy.h"
#include "../Spellcard.h"

// GLOBAL: TH16 0x4c10c8
// Never written; the original loads it for every use.
D3DXVECTOR2 g_zero_vec2;

// GLOBAL: TH16 0x4a6dc4
// The hooks ECL 634 installs; only entry 0, never set.
void *g_ecl_unknown_634_funcs[1];

// STUB: TH16 0x417f00
void Spellcard::start(i32 spell_id, const char *name, i32 arg_2, i32 arg_3)
{
}

// STUB: TH16 0x425410
// One of the damage hooks ECL's flagExtDmg installs.
int __fastcall ecl_ext_damage_425410(EnemyData *enemy, int damage)
{
    return damage;
}
