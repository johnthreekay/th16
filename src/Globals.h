#pragma once

#include "ZunMath.h"
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

// The playable characters (Globals::character). TH16 has no subshots
// (Globals::subshot is always 0), so character + subshot, which indexes the
// per-shot-type tables, is the character.
enum Character
{
    CHARACTER_REIMU = 0,
    CHARACTER_CIRNO = 1,
    CHARACTER_AYA = 2,
    CHARACTER_MARISA = 3,
};

// The subseasons (Globals::subseason), the season of the player's release:
// each has its own release, options and pl0Xsub.sht/.anm files.
enum Subseason
{
    SUBSEASON_SPRING = 0,
    SUBSEASON_SUMMER = 1,
    SUBSEASON_AUTUMN = 2,
    SUBSEASON_WINTER = 3,
    // The extra stage's.
    SUBSEASON_DOYOU = 4,
};

// Globals::game_mode.
enum GameMode
{
    GAME_MODE_NORMAL = 0,
    GAME_MODE_STAGE_PRACTICE = 1,
    GAME_MODE_SPELL_PRACTICE = 2,
};

// Bits of Globals::flags_lo_45c (bits 0-3 of the flag word at 0x45c),
// which say how the next GameThread starts.
enum GlobalsFlagsLo
{
    // The same stage starts again (retry or continue): the stage, dialogue,
    // player and enemy resources stay loaded.
    GLOBALS_SAME_STAGE_AGAIN = 1 << 0,
    // Going on to the next stage: the game objects stay.
    GLOBALS_NEXT_STAGE = 1 << 1,
    // The hiscore has been beaten this game.
    GLOBALS_HISCORE_BEATEN = 1 << 2,
    // Continuing after a game over: continues_used is kept.
    GLOBALS_CONTINUED = 1 << 3,
    // The stage's ECL and dialogue stay loaded across either restart.
    GLOBALS_STAGE_RESTART_MASK = GLOBALS_SAME_STAGE_AGAIN | GLOBALS_CONTINUED,
};

// Bits of Globals::flags_hi_45c (bit 6 on of the flag word).
enum GlobalsFlagsHi
{
    // The title screen's demo replay is playing.
    GLOBALS_HI_DEMO_PLAY = 1 << 0,
    // Cleared when the demo starts; not otherwise used in TH16.
    GLOBALS_HI_2 = 1 << 1,
};

// GlobalsFlagsHi bits as GameThread sees them in the whole flag word.
#define GLOBALS_WORD_DEMO_PLAY (GLOBALS_HI_DEMO_PLAY << 6)

#define MAX_LIVES 8
#define MAX_BOMBS 8
#define BOMB_FRAGMENTS_PER_BOMB 5
#define SEASON_LEVEL_MAX 6
#define SCORE_MAX 999999999

// The game state that replays save and restore, starting at 0x4a5790
// (the TH06 equivalent is part of GameManager): stage, character, score,
// lives, bombs, power, point item value and season power. Field names from
// ExpHP's th-re-data statics; replays save the first 0x224 bytes (his
// zReplaySavedGlobals).
struct Globals
{
    i32 stage_num;
    // Set to the stage number while a stage loads; replays set it back to
    // 1 (ExpHP). A retry or continue compares it with stage_num.
    i32 weird_stage_num;
    // Set by ECL as the stage goes on (0x29, 0x2b and others are tested).
    i32 chapter;
    i32 time_in_stage;
    i32 time_in_chapter;
    // Character.
    i32 character;
    // Always 0 in TH16 (the shot type is the subseason).
    i32 subshot;
    // Subseason.
    i32 subseason;
    // Score divided by 10.
    u32 score;
    // Difficulty.
    i32 difficulty;
    i32 continues_used;
    // Never written (ExpHP).
    i32 rank;
    i32 graze;
    i32 graze_in_chapter;
    // The card being practiced in spell practice.
    i32 spell_id;
    i32 miss_count;
    // Not used.
    i32 unk_40;
    i32 num_point_items_collected;
    // Point item value.
    i32 piv;
    i32 initial_piv;
    i32 max_piv;
    i32 power;
    i32 max_power;
    // Always 100.
    i32 power_per_level;
    // Not used.
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
    u8 unk_c8[0xd0 - 0xc8];
    // The score and number of items collected at full value (above the
    // collection line or by autocollection), a leftover of DDC's bonus.
    i32 full_value_item_score;
    // Zeroed for a new game; not otherwise used.
    i32 unk_d4;
    i32 full_value_item_count;
    // Set to 8 when an item starts flying to the player; not read.
    i32 unk_dc;
    // Where the player was when the last full value item was collected.
    Float3 last_collect_pos;
    i32 item_spawn_count;
    i32 enemies_spawned_in_chapter;
    i32 enemies_destroyed_in_chapter;
    char music_filename[0x100];
    u8 unk_1f8[0x200 - 0x1f8];
    // Per-stage values: unk_204 + the stage number is zeroed when a stage
    // starts (with unk_224); nothing else uses them in TH16.
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
    i32 hiscore;
    i32 hiscore_continues;
    // The difficulty to go back to after the title screen's demo replay.
    i32 difficulty_before_demo;
    u8 unk_234[0x45c - 0x234];
    // GlobalsFlagsLo.
    u32 flags_lo_45c : 4;
    // GameMode.
    u32 game_mode : 2;
    // GlobalsFlagsHi.
    u32 flags_hi_45c : 26;

    // Members that do not use this; LTCG dropped it. The item code passes
    // an argument these never read (callers push whatever is in ecx).
    i32 collect_extend(i32 unused);
    void collect_bomb(i32 unused);
    void collect_bomb_fragment(i32 unused);
    i32 collect_season_item(i32 unused);
    HARNESS_CALLED void init_season_level_delta(i32 level, i32 delta);

    // Returns whether the power level changed.
    i32 add_power(i32 amount);
    void set_game_mode(u32 mode);
    // amount is divided by 10; also awards score extends.
    HARNESS_CALLED void add_to_score(i32 amount);
    // Zeroes unk_200 to unk_224.
    void reset_224();
    // Resets power, bombs, fragments and counters for a new game.
    void reset_for_new_game();

    // The season level (0-6) the season power has reached.
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

// The score (divided by 10) of the next extend.
i32 get_score_extend_quota();
// How far the season power is from the current level to the next (1 at
// the top level).
HARNESS_CALLED f32 get_season_gauge_fill_ratio();
