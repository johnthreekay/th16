#pragma once

#include "decomp.h"
#include "types.h"

enum Difficulty
{
    DIFFICULTY_EASY = 0,
    DIFFICULTY_NORMAL = 1,
    DIFFICULTY_HARD = 2,
    DIFFICULTY_LUNATIC = 3,
    DIFFICULTY_EXTRA = 4,
};

#define MAX_LIVES 8
#define MAX_BOMBS 8
#define BOMB_FRAGMENTS_PER_BOMB 5
#define SEASON_LEVEL_MAX 6
#define SCORE_MAX 999999999

// The game state that replays save and restore, starting at 0x4a5790
// (the TH06 equivalent is part of GameManager). Field names from ExpHP's
// th-re-data statics.
struct Globals
{
    i32 stage_num;
    i32 weird_stage_num;
    i32 chapter;
    i32 time_in_stage;
    i32 time_in_chapter;
    i32 character;
    i32 subshot;
    i32 subseason;
    // Score divided by 10.
    u32 score;
    i32 difficulty;
    i32 continues_used;
    i32 rank;
    i32 graze;
    i32 graze_in_chapter;
    i32 spell_id;
    i32 miss_count;
    i32 unk_40;
    i32 num_point_items_collected;
    i32 piv;
    i32 initial_piv;
    i32 max_piv;
    i32 power;
    i32 max_power;
    // Always 100.
    i32 power_per_level;
    i32 unk_60;
    i32 lives;
    i32 life_fragments;
    i32 next_score_extend_index;
    i32 bombs;
    i32 bomb_fragments;
    i32 season_power;
    i32 max_season_power;
    // Season power needed to go from level i - 1 to level i.
    i32 season_level_deltas[10];
    // Season power at which level i begins (index 7 is a copy of the
    // maximum).
    i32 season_level_thresholds[8];
    u8 unk_c8[0x45c - 0xc8];
    u32 flags_lo_45c : 4;
    // 2: spell practice.
    u32 game_mode : 2;
    u32 flags_hi_45c : 26;

    // Members that do not use this; LTCG dropped it. The item code passes
    // an argument these never read (callers push whatever is in ecx).
    i32 collect_extend(i32 unused);
    void collect_bomb(i32 unused);
    void collect_bomb_fragment(i32 unused);
    i32 collect_season_item(i32 unused);
    void init_season_level_delta(i32 level, i32 delta);

    // Returns whether the power level changed.
    i32 add_power(i32 amount);
    void set_game_mode(u32 mode);
    // amount is divided by 10; also awards score extends.
    HARNESS_CALLED void add_to_score(i32 amount);

    i32 season_level()
    {
        i32 level = 0;
        for (i32 i = 1; i < 7; i++)
        {
            if (season_power < season_level_thresholds[i])
            {
                break;
            }
            level++;
        }
        return level;
    }
};

extern Globals g_Globals;

i32 get_score_extend_quota();
HARNESS_CALLED f32 get_season_gauge_fill_ratio();
