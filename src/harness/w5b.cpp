// Stand-in callers for wave 5 range B (0x420000-0x440000) functions whose
// shape depends on code that is not decompiled yet.
#include "../Player.h"

// EnemyData::step_logic (0x41c443, 0x41c5c5) passes other values for the
// arguments the hurtbox hook leaves 0.
i32 harness_w5b_enemy_damage(Float3 *pos, Float2 *size, f32 radius, f32 angle, i32 *hit, i32 a, i32 b, i32 id)
{
    return g_Player->compute_damage_to_enemy(pos, size, angle, radius, hit, a, b, id);
}
