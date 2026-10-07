#pragma once

#include <stdlib.h>
#include <time.h>

#include "decomp.h"
#include "types.h"

// Capture history of one spell card. Index 0 is the main game, 1 spell
// practice.
struct ScorefileSpell
{
    char name[0x80];
    i32 captures[2];
    i32 attempts[2];
    u8 unk_90[0x98 - 0x90];
    // Spell practice high score, divided by 10.
    i32 practice_score;
};

// One high score entry.
struct ScorefileScore
{
    // Divided by 10; the last digit is continues_used.
    u32 score;
    i8 stage;
    i8 continues_used;
    char name[10];
    __time64_t timestamp;
    f32 slowdown;
    i32 subseason;
};

// Stage practice record of one stage.
struct ScorefilePractice
{
    // Divided by 10, like the in-game score.
    i32 high_score;
    u8 unk_4;
    // Nonzero once the stage can be practiced.
    u8 unlocked;
    u8 unk_6[2];
};

// Per-character part of th16 score data; the fifth one sums up all
// characters. Layout from the offsets the game uses.
struct ScorefileCharacter
{
    u8 unk_0[0x18];
    // The top ten of each difficulty.
    ScorefileScore scores[5][10];
    u8 unk_658[0x8d8 - 0x658];
    // 119 spell cards; the play time follows them.
    ScorefileSpell spells[0x77];
    // Games played (shown in the player data).
    i32 play_count;
    // In hundredths of a second.
    __int64 play_time;
    // Games played per difficulty.
    i32 difficulty_play_counts[7];
    // Nonzero once the game was cleared on each difficulty.
    i32 clears[5];
    // Stage practice, per difficulty and stage (1-6 used).
    ScorefilePractice practice[5][8];
    u8 unk_52d8[0x5318 - 0x52d8];
};

// The decrypted contents of scoreth16.dat. Only the parts decompiled code
// needs so far. 0x1a3ac bytes (new Scorefile at 0x43af25), so 4-byte packed
// despite play_time.
#pragma pack(push, 4)
struct Scorefile
{
    ScorefileCharacter characters[5];
    u8 unk_19f78[0x19f96 - 0x19f78];
    // Per ending (e01-e08): bit 0 seen, bits 1-3 cleared on Normal, Hard,
    // Lunatic. Entry 8 is set once any ending has been seen.
    u8 endings_seen[9];
    u8 unk_19f9f[0x19fa6 - 0x19f9f];
    // Set once a track has played, unlocking it in the music room.
    u8 bgm_unlocked[0x19fc8 - 0x19fa6];
    // Total of every character's play_time.
    __int64 play_time;
    u8 unk_19fd0[0x1a3ac - 0x19fd0];

    // 0x44a800. Whether the character cleared any of the main
    // difficulties.
    i32 has_cleared(i32 character);
    // Members that reach the score data through g_Scorefile; LTCG dropped
    // this. 0x44a850: whether any character cleared the main game.
    HARNESS_CALLED i32 any_cleared();
    // 0x44a8e0. Whether every character cleared the difficulty.
    HARNESS_CALLED i32 all_cleared(i32 difficulty);

    // 0x4497e0 (ExpHP: sub_4497e0__reads_scorefile). Loads scoreth16.dat.
    Scorefile();
    // Inlined into LoadingThread's destructor. Frees the two buffers kept
    // in the first 8 bytes (inside characters[0] as laid out here).
    ~Scorefile()
    {
        void **buffers = (void **)this;
        if (buffers[0] != NULL)
        {
            free(buffers[0]);
            buffers[0] = NULL;
        }
        if (buffers[1] != NULL)
        {
            free(buffers[1]);
            buffers[1] = NULL;
        }
    }
};
#pragma pack(pop)

// 0x449a00 (ExpHP: sub_449a00_writes_score_file). Saves g_Scorefile.
void scorefile_save_449a00();
static_assert(sizeof(Scorefile) == 0x1a3ac, "Scorefile size");
static_assert(offsetof(ScorefileCharacter, play_time) == 0x5160, "ScorefileCharacter::play_time");
static_assert(offsetof(ScorefileCharacter, clears) == 0x5184, "ScorefileCharacter::clears");
static_assert(offsetof(ScorefileCharacter, scores) == 0x18, "ScorefileCharacter::scores");
static_assert(sizeof(ScorefileScore) == 0x20, "ScorefileScore size");
static_assert(offsetof(ScorefileCharacter, practice) == 0x5198, "ScorefileCharacter::practice");
static_assert(sizeof(ScorefileCharacter) == 0x5318, "ScorefileCharacter size");
static_assert(offsetof(Scorefile, endings_seen) == 0x19f96, "Scorefile::endings_seen");
static_assert(offsetof(Scorefile, bgm_unlocked) == 0x19fa6, "Scorefile::bgm_unlocked");
static_assert(offsetof(Scorefile, play_time) == 0x19fc8, "Scorefile::play_time");

extern Scorefile *g_Scorefile;

// Each section of scoreth16.dat starts with this header.
struct ScorefileSection
{
    u16 magic;
    u16 version;
    u32 checksum;
    u32 size;

    // Sum of the bytes after the checksum.
    u32 compute_checksum(i32 size);
};
