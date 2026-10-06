#pragma once

#include "ZunMath.h"
#include "decomp.h"
#include "types.h"

// Multiplier for everything that runs at game speed.
extern f32 g_game_speed;

// The state of the game in progress, one global object (ExpHP names its
// methods Globals::*; replays save the first 0x224 bytes, his
// zReplaySavedGlobals).
struct Globals
{
    i32 stage;
    i32 stage_2;
    i32 chapter;
    i32 time_in_stage;
    i32 time_in_chapter;
    i32 character;
    i32 subshot;
    i32 subseason;
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
    i32 next_extend_index;
    i32 bombs;
    i32 bomb_fragments;
    i32 season_power;
    i32 max_season_power;
    i32 season_level_deltas[7];
    u8 unk_9c[0xa8 - 0x9c];
    i32 season_level_requirements[6];
    i32 max_season_power_copy;
    u8 unk_c4[0xd0 - 0xc4];
    i32 unk_d0;
    i32 unk_d4;
    i32 unk_d8;
    i32 unk_dc;
    Float3 last_collect_pos;
    i32 item_spawn_count;
    i32 enemies_spawned_in_chapter;
    i32 enemies_destroyed_in_chapter;
    char music_filename[0x100];
    u8 unk_1f8[0x200 - 0x1f8];
    i32 unk_200;
    i32 unk_204;
    i32 unk_208;
    i32 unk_20c;
    i32 unk_210;
    i32 unk_214;
    i32 unk_218;
    i32 unk_21c;
    i32 unk_220;
    i32 unk_224;

    void reset_224();
    void reset_for_new_game();
};

extern Globals g_Globals;
