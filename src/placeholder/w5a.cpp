// Placeholders compiled with /GL for functions that wave 5 range A
// (0x401000-0x420000) calls, where LTCG has to see a body to pick the
// original's calling convention. Each forwards to an opaque stub.
#include "../Player.h"

int w5a_placeholder_sink(void *object, int value);

// 0x445a30 is not decompiled yet; its callers (EnemyData::step_logic,
// ecl_ext_damage_425410) pass angle in xmm3.
DECOMP_NOINLINE i32 enm_compute_damage_sources(D3DXVECTOR3 *pos, D3DXVECTOR2 *size, f32 radius, f32 angle,
                                               i32 *hit, D3DXVECTOR3 *hit_pos, i32 is_bomb, i32 enemy_id)
{
    if (g_Player->inner.pos.x == angle)
    {
        return 0;
    }
    *hit = w5a_placeholder_sink(size, (i32)radius);
    return w5a_placeholder_sink(pos, (i32)(hit_pos != NULL) + is_bomb + enemy_id);
}
