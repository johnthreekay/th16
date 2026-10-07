// Placeholders for functions the wave 4 interpreter work (EnemyData's ECL
// instructions) calls but that are not decompiled yet, and data LTCG must
// not see. Compiled without /GL.
#include <d3dx9math.h>

#include "../Enemy.h"
#include "../Spellcard.h"

// Never written; the original loads it for every use.
// GLOBAL: TH16 0x4c10c8
D3DXVECTOR2 g_zero_vec2;

// The hooks ECL 634 installs; only entry 0, never set.
// GLOBAL: TH16 0x4a6dc4
void *g_ecl_unknown_634_funcs[1];


