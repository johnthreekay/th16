#include "Globals.h"
#include "Gui.h"
#include "SoundManager.h"

// GLOBAL: TH16 0x4a5790
Globals g_Globals;

// GLOBAL: TH16 0x4917c4
const i32 g_score_extend_quotas_extra[7] = {
    1000000, 2000000, 4000000, 6000000, 8000000, 10000000, 1000000000,
};

// GLOBAL: TH16 0x491880
const i32 g_score_extend_quotas_standard[11] = {
    500000, 1000000, 2000000, 4000000, 7000000, 10000000, 15000000, 25000000, 50000000, 100000000, 1000000000,
};

// FUNCTION: TH16 0x42e520
DECOMP_NOINLINE void Globals::reset_224()
{
    unk_224 = 0;
    unk_200 = 0;
    unk_204 = 0;
    unk_208 = 0;
    unk_20c = 0;
    unk_210 = 0;
    unk_214 = 0;
    unk_218 = 0;
    unk_21c = 0;
    unk_220 = 0;
}

// FUNCTION: TH16 0x42e590
void Globals::reset_for_new_game()
{
    num_point_items_collected = 0;
    power = 0;
    power_per_level = 1;
    bombs = 3;
    if (g_Gui != NULL)
    {
        g_Gui->update_bombs(g_Globals.bombs, g_Globals.bomb_fragments);
    }
    bomb_fragments = 0;
    life_fragments = 0;
    next_score_extend_index = 0;
    full_value_item_score = 0;
    unk_d4 = 0;
    full_value_item_count = 0;
    unk_dc = 0;
    item_spawn_count = 0;
    reset_224();
    miss_count = 0;
}

// FUNCTION: TH16 0x43ddd0
i32 get_score_extend_quota()
{
    if (g_Globals.difficulty == DIFFICULTY_EXTRA)
    {
        return g_score_extend_quotas_extra[g_Globals.next_score_extend_index];
    }
    return g_score_extend_quotas_standard[g_Globals.next_score_extend_index];
}

// TODO: the original reserves an unused stack slot (push ecx) and saves esi
// up front, probably stack alignment for Gui::show_notice (see add_to_score).
// FUNCTION: TH16 0x43ddf0
i32 Globals::add_power(i32 amount)
{
    if (power >= max_power)
    {
        return 0;
    }
    power += amount;
    if (power > max_power)
    {
        power = max_power;
        g_Gui->show_notice(0, GUI_NOTICE_FULL_POWER);
    }
    return (power - amount) / power_per_level != power / power_per_level;
}

// FUNCTION: TH16 0x43de50
i32 Globals::collect_season_item(i32 unused)
{
    if (g_Globals.season_power >= g_Globals.max_season_power)
    {
        return 0;
    }
    i32 old_level = g_Globals.season_level();
    g_Globals.season_power++;
    if (g_Globals.season_power > g_Globals.max_season_power)
    {
        g_Globals.season_power = g_Globals.max_season_power;
    }
    return old_level != g_Globals.season_level();
}

// FUNCTION: TH16 0x43deb0
HARNESS_CALLED void Globals::init_season_level_delta(i32 level, i32 delta)
{
    g_Globals.season_level_deltas[level] = delta;
    i32 sum = 0;
    // The original sums with plain unrolled scalar code; without the pragma
    // our build vectorizes this loop. What kept ZUN's from being vectorized
    // is not known yet.
#pragma loop(no_vector)
    for (i32 i = 0; i < level + 1; i++)
    {
        sum += g_Globals.season_level_deltas[i];
    }
    g_Globals.season_level_thresholds[level] = sum;
}

// FUNCTION: TH16 0x43df20
HARNESS_CALLED f32 get_season_gauge_fill_ratio()
{
    i32 level = g_Globals.season_level();
    if (level >= SEASON_LEVEL_MAX)
    {
        return 1.0f;
    }
    return (f32)(g_Globals.season_power - g_Globals.season_level_thresholds[level]) /
           (f32)g_Globals.season_level_deltas[level + 1];
}

// FUNCTION: TH16 0x43df70
i32 Globals::collect_extend(i32 unused)
{
    if (g_Globals.lives >= MAX_LIVES)
    {
        return 0;
    }
    g_Globals.lives++;
    if (g_Globals.lives > MAX_LIVES)
    {
        g_Globals.lives = MAX_LIVES;
    }
    g_Gui->update_lives(g_Globals.lives, g_Globals.life_fragments);
    return 1;
}

// FUNCTION: TH16 0x43dfb0
void Globals::collect_bomb(i32 unused)
{
    g_Globals.bombs++;
    if (g_Globals.bombs > MAX_BOMBS)
    {
        g_Globals.bombs = MAX_BOMBS;
    }
    else
    {
        g_SoundManager.play_sound_centered(0x2e, 0);
    }
    g_Gui->update_bombs(g_Globals.bombs, g_Globals.bomb_fragments);
}

// FUNCTION: TH16 0x43dff0
void Globals::collect_bomb_fragment(i32 unused)
{
    if (g_Globals.bombs >= MAX_BOMBS)
    {
        g_Globals.bomb_fragments = 0;
        return;
    }
    g_Globals.bomb_fragments++;
    if (g_Globals.bomb_fragments >= BOMB_FRAGMENTS_PER_BOMB)
    {
        g_Globals.bomb_fragments = 0;
        g_Globals.collect_bomb(0);
    }
    g_Gui->update_bombs(g_Globals.bombs, g_Globals.bomb_fragments);
}

// TODO: the original aligns its frame to 8 bytes (and esp, -8), which LTCG
// adds for Gui::show_notice's sake; ours does not, so registers differ too.
// FUNCTION: TH16 0x43e080
HARNESS_CALLED void Globals::add_to_score(i32 amount)
{
    g_Globals.score += amount / 10;
    while (g_Globals.score >= get_score_extend_quota())
    {
        if (g_Globals.collect_extend(0))
        {
            g_SoundManager.play_sound_centered(0x11, 0);
            g_Gui->show_notice(0, GUI_NOTICE_EXTEND);
        }
        g_Globals.next_score_extend_index++;
    }
    if (g_Globals.score >= SCORE_MAX + 1)
    {
        g_Globals.score = SCORE_MAX;
    }
}
