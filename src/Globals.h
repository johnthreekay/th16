#pragma once

#include "types.h"

// Scales how fast timers and animations run. ECL slows it down for final
// boss deaths; many objects also swap it in and out around their updates.
extern f32 g_game_speed;

// The state of the current run that replays save, laid out as one block.
// Names follow ExpHP's statics.
struct Globals
{
    i32 stage_num;
    // Set while a stage loads; replays put it back to 1 afterwards.
    i32 loading_stage_num;
    i32 chapter;
    i32 time_in_stage;
    i32 time_in_chapter;
    i32 character;
    // Always zero in TH16; character + subshot picks the shot type.
    i32 subshot;
    i32 subseason;
    // Current score divided by 10.
    i32 score;
    i32 difficulty;
    i32 continues_used;
    i32 rank;
    i32 graze;
    i32 graze_in_chapter;
    i32 spell_id;
    i32 miss_count;
    i32 unk_40;
    i32 point_items_collected;
    i32 piv;
    i32 initial_piv;
    i32 max_piv;
    i32 power;
    i32 power_copy;
    i32 power_per_level;
    i32 unk_60;
    i32 lives;
    i32 life_fragments;
    i32 next_score_extend;
    i32 bombs;
    i32 bomb_fragments;
    i32 season_power;
    i32 max_season_power;
    // Season power a release at each level uses up.
    i32 season_power_level_deltas[7];
    i32 unk_9c[4];
    // The season level is the number of these the season power reaches.
    i32 season_power_level_requirements[6];
    i32 max_season_power_copy;

    // How many season power requirements the current power meets.
    i32 season_level()
    {
        i32 level = 0;
        for (i32 i = 0; i < 6; i++)
        {
            if (season_power < season_power_level_requirements[i])
            {
                break;
            }
            level++;
        }
        return level;
    }
};

extern Globals g_Globals;

