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

struct Scorefile
{
    ScorefileCharacter characters[5];
};

extern Scorefile *g_Scorefile;
