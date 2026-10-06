#pragma once

#include "AnmManager.h"
#include "UpdateFunc.h"
#include "types.h"

// Only the parts unit 6 needs so far; layout from ExpHP's th-re-data.
struct AsciiManager
{
    u8 unk_0[8];
    UpdateFunc *on_tick_func;
    UpdateFunc *on_draw_func_1;
    u8 unk_10[0x19240 - 0x10];
    AnmLoaded *ascii_anm;
    AnmId unk_19244;
    AnmId now_loading_id;
    UpdateFunc *on_draw_func_2;
    UpdateFunc *on_draw_func_3;
};

extern AsciiManager *g_AsciiManager;
