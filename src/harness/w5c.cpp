// Stand-in callers for wave 5 range C (0x440000-0x44c000) functions whose
// shape depends on code that is not decompiled yet.
#include "../Player.h"

// EnemyData::step_logic (0x41c443, 0x41c5c5) and 0x425410 apply the
// player's damage sources to enemies: rectangles with a rotation in xmm3,
// circles with 0.
i32 harness_w5c_damage(Float3 *pos, Float3 *size, f32 rotation, Float3 *hit_pos, i32 id, i32 rect)
{
    i32 hit = 0;
    i32 total;
    if (rect)
    {
        total = g_Player->compute_damage_to_enemy(pos, size, rotation, 0.0f, &hit, hit_pos, 1, id);
    }
    else
    {
        total = g_Player->compute_damage_to_enemy(pos, NULL, 0.0f, size->x * 0.5f, &hit, hit_pos, 0, id);
    }
    return total + hit;
}
