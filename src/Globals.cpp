#include "Globals.h"
#include "Gui.h"
#include "SoundManager.h"

// Statically zero except for the difficulty, which starts at Normal (the
// fields before it, stage_num to score, are listed as 0).
// GLOBAL: TH16 0x4a5790
Globals g_Globals = {0, 0, 0, 0, 0, 0, 0, 0, 0, DIFFICULTY_NORMAL};

// The scores (divided by 10) of the score extends, for the extra stage and
// for the main game.
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
HARNESS_CALLED i32 score_extend_quota_out_of_line()
{
    return get_score_extend_quota();
}

// TODO: the original reserves an unused stack slot (push ecx) and saves esi
// up front, probably stack alignment for Gui::show_notice (see add_to_score).
// Adds power up to the maximum (with the full power notice when it gets
// there).
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

// One more point of season power: returns whether the season level went
// up.
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

// Sets the season power needed for a level and recomputes where that level
// begins.
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

// One more life, up to MAX_LIVES: returns whether there was room for it.
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

// One more bomb, up to MAX_BOMBS (with a sound when it fits).
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
        g_SoundManager.play_sound_centered(SE_CARDGET_2, 0);
    }
    g_Gui->update_bombs(g_Globals.bombs, g_Globals.bomb_fragments);
}

// One more bomb fragment; five make a bomb. None count with the bombs
// full.
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

// The extend notice. The dead double is not ZUN's code: it stands in for
// whatever made LTCG's stack alignment pass count Gui::show_notice as a
// callee that wants an aligned frame. In an inline helper it belongs to the
// helper's call graph node, so add_to_score realigns through ebx (with a
// 4-byte slot and esi saved up front) like the original, while show_notice
// and its other callers are left alone (a dead double in show_notice itself
// costs Spellcard::end and Item::init_anm their matches).
static inline void show_extend_notice()
{
    double unused = 0.0;
    (void)unused;
    g_Gui->show_notice(0, GUI_NOTICE_EXTEND);
}

// Adds to the score (in units of 10), with an extra life and its notice for
// every extend score passed, up to the 9999999990 cap.
// FUNCTION: TH16 0x43e080
HARNESS_CALLED void Globals::add_to_score(i32 amount)
{
    g_Globals.score += amount / 10;
    while (g_Globals.score >= score_extend_quota_out_of_line())
    {
        if (g_Globals.collect_extend(0))
        {
            g_SoundManager.play_sound_centered(SE_EXTEND, 0);
            show_extend_notice();
        }
        g_Globals.next_score_extend_index++;
    }
    if (g_Globals.score >= SCORE_MAX + 1)
    {
        g_Globals.score = SCORE_MAX;
    }
}
