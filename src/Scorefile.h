#pragma once

#include <stdlib.h>

#include "decomp.h"
#include "types.h"

// Capture history of one spell card. Index 0 is the main game, 1 spell
// practice.
struct ScorefileSpell
{
    char name[0x80];
    i32 captures[2];
    i32 attempts[2];
    i32 id;
    i32 difficulty;
    u8 unk_98[0x9c - 0x98];
};

struct ScorefilePractice
{
    u32 score;
    u8 cleared;
    u8 unlocked;
    u16 unk_6;
};

// Per-character part of th16 score data; the fifth one sums up all
// characters. Layout from the offsets the game uses.
struct ScorefileCharacter
{
    u8 unk_0[0x8d8];
    // 119 spell cards; the play time follows them.
    ScorefileSpell spells[0x77];
    u8 unk_515c[0x5160 - 0x515c];
    // In hundredths of a second.
    __int64 play_time;
    // Per difficulty (the unlock cheat counts six).
    i32 play_counts[6];
    u8 unk_5180[0x5184 - 0x5180];
    // Nonzero once the game was cleared on each difficulty.
    i32 clears[5];
    u8 unk_5198[0x51a0 - 0x5198];
    // Stage practice, per difficulty and stage.
    ScorefilePractice practices[5][8];
    u8 unk_52e0[0x5318 - 0x52e0];
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
    // 0x44a930. The title screen's unlock cheat.
    HARNESS_CALLED void unlock_all();

    // 0x4497e0 (ExpHP: sub_4497e0__reads_scorefile). Loads scoreth16.dat.
    Scorefile();
    // 0x449880. Checks the file read by the constructor and copies its
    // valid sections over the defaults, or starts a new file header.
    i32 load_sections();
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
i32 scorefile_save_449a00();
static_assert(sizeof(Scorefile) == 0x1a3ac, "Scorefile size");
static_assert(offsetof(ScorefileCharacter, play_time) == 0x5160, "ScorefileCharacter::play_time");
static_assert(offsetof(ScorefileCharacter, clears) == 0x5184, "ScorefileCharacter::clears");
static_assert(offsetof(ScorefileCharacter, practices) == 0x51a0, "ScorefileCharacter::practices");
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

// The header of scoreth16.dat, in front of the compressed sections.
struct ScorefileHeader
{
    // "TH61"
    u32 magic;
    u32 file_size;
    u16 version;
    u16 unk_a;
    u32 unk_c;
    u32 compressed_size;
    // Of the sections after decompression.
    u32 size;
};

// One entry of a high score table.
struct ScorefileScore
{
    u32 score;
    u8 stage;
    u8 unk_5;
    char name[9];
    u8 unk_f;
    u32 unk_10;
    u32 unk_14;
    u32 unk_18;
    u32 unk_1c;
};

// The real layout of the character sections ('CR'), which start 8 bytes
// into Scorefile, after the two buffer pointers; ScorefileCharacter views
// the same data from the start of Scorefile.
struct ScorefileChara
{
    ScorefileSection header;
    i32 character;
    // Per difficulty, best first.
    ScorefileScore scores[6][10];
    u8 unk_790[0x8d0 - 0x790];
    ScorefileSpell spells[0x77];
    u8 unk_5154[0x5318 - 0x5154];

    // 0x4493c0. The defaults of a new score file.
    void init();
};
static_assert(sizeof(ScorefileChara) == 0x5318, "ScorefileChara size");

// The status section ('ST'), at 0x19f80 in Scorefile.
struct ScorefileStatus
{
    ScorefileSection header;
    char name[9];
    u8 unk_15[0x50 - 0x15];
    u16 random[0x1ee];

    // 0x449720. The defaults of a new score file.
    void init();
};
static_assert(sizeof(ScorefileStatus) == 0x42c, "ScorefileStatus size");

// Scorefile's real layout, as its own member functions see it.
struct ScorefileData
{
    ScorefileHeader *file;
    // The decompressed sections of the file.
    u8 *sections;
    ScorefileChara charas[5];
    ScorefileStatus status;
};
static_assert(sizeof(ScorefileData) == sizeof(Scorefile), "ScorefileData size");
