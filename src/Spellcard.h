#pragma once

#include "ZunTimer.h"
#include "types.h"

enum SpellcardFlags
{
    SPELLCARD_ACTIVE = 1 << 0,
};

// Partial: only what unit 2 (Stage, Bomb) uses so far. Layout from ExpHP.
struct Spellcard
{
    u8 unk_0[0x20];
    ZunTimer time;
    u8 unk_34[0x78 - 0x34];
    u32 flags;
};

extern Spellcard *g_Spellcard;
