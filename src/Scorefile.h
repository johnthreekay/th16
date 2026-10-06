#pragma once

#include "types.h"

// Capture history of one spell card. Index 0 is the main game, 1 spell
// practice.
struct ScorefileSpell
{
    char name[0x80];
    i32 captures[2];
    i32 attempts[2];
    u8 unk_90[0x9c - 0x90];
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
    u8 unk_5168[0x5318 - 0x5168];
};

// The decrypted contents of scoreth16.dat. Only the parts decompiled code
// needs so far.
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
};

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
