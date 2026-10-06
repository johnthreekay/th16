// Stand-in callers for the second pass over units 5 and 6 (GameThread,
// items, lasers, Supervisor, menus, Player, replays).
#include "../Player.h"
#include "../PopupManager.h"

// Like Player::initialize (0x441109) and Player::on_tick (0x44294f).
void harness_player_set_position(Player *player, i32 respawn)
{
    if (respawn)
    {
        player->set_position(0.0f, 480.0f);
    }
    else
    {
        player->set_position(0.0f, 400.0f);
    }
}

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
