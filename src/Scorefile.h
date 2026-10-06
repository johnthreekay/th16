#pragma once

#include "types.h"

// The decrypted contents of scoreth16.dat. Only the parts unit 6 needs so
// far.
struct Scorefile
{
    u8 unk_0[0x19fa6];
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
