#pragma once

#include "AnmManager.h"
#include "types.h"

// Only the parts unit 6 needs so far; layout from ExpHP's th-re-data.
struct AsciiManager
{
    u8 unk_0[0x19240];
    AnmLoaded *ascii_anm;
    u8 unk_19244[0x19254 - 0x19244];
};

extern AsciiManager *g_AsciiManager;
