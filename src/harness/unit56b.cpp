// Stand-in callers for the second pass over units 5 and 6 (GameThread,
// items, lasers, Supervisor, menus, Player, replays).
#include <math.h>

#include "../Player.h"
#include "../PopupManager.h"

// Like the bullet and laser collision code (0x412512, 0x41254a, 0x41255e,
// 0x41c8bc) and the many aimed bullets (0x413a33...).
i32 harness_player_hit(Float3 *pos, Float3 *size, f32 radius, i32 graze_only)
{
    i32 n = g_Player->check_hit_rect(pos, size, graze_only);
    n += g_Player->check_hit_rect(size, pos, 0);
    n += g_Player->check_hit_circle(pos, radius, graze_only);
    n += g_Player->check_hit_circle(size, radius * 0.5f, 0);
    return n + (i32)g_Player->angle_to_player(pos) + (i32)g_Player->angle_to_player(size);
}

// Score popups come from many places with varying values (enemy kills,
// grazes, items).
void harness_popup(Float3 *pos, i32 value, D3DCOLOR color)
{
    g_PopupManager->generate_small_score_popup(pos, value, color);
    g_PopupManager->generate_small_score_popup(pos, value * 10, 0xffffffff);
}

// Like replay playback restoring the player's position (0x448fbc).
void harness_player_set_position_subpixel(Int2 *pos)
{
    g_Player->set_position_subpixel(pos);
}

// Stands in for the atan2f users not decompiled yet (the homing shots at
// 0x445ee0 and 0x446870, the item magnet, the ECL, ...). Each spills
// doubles for atan2, and LTCG only realigns frames for double temporaries
// (angle_to_player, zun_atan2f) once the program has enough of them.
f32 harness_homing_angle(Float3 *a, Float3 *b)
{
    return atan2f(b->y - a->y, b->x - a->x) + atan2f(a->y - b->y, a->x - b->x);
}
