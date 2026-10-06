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
    ScorefileSpell spells[0x78];
    u8 unk_51f8[0x5318 - 0x51f8];
};

// The decrypted contents of scoreth16.dat. Only the parts decompiled code
// needs so far.
struct Scorefile
{
    ScorefileCharacter characters[5];
    u8 unk_19f78[0x19fa6 - 0x19f78];
    // Set once a track has played, unlocking it in the music room.
    u8 bgm_unlocked[0x1a3ac - 0x19fa6];
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
