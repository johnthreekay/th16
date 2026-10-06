#pragma once

#include "AnmManager.h"
#include "UpdateFunc.h"
#include "ZunMath.h"
#include "types.h"

// Only the parts decompiled code needs so far; layout from ExpHP's
// th-re-data.
struct AsciiManager
{
    u8 unk_0[8];
    UpdateFunc *on_tick_func;
    UpdateFunc *on_draw_func_1;
    u8 unk_10[0x1920c - 0x10];
    // Applies to strings added from now on.
    ZunColor color;
    u8 unk_19210[0x19224 - 0x19210];
    i32 font_id;
    i32 group;
    i32 duration;
    i32 align_h;
    i32 align_v;
    u8 unk_19238[0x19240 - 0x19238];
    AnmLoaded *ascii_anm;
    AnmId unk_19244;
    AnmId now_loading_id;
    UpdateFunc *on_draw_func_2;
    UpdateFunc *on_draw_func_3;

    // 0x408260. Variadic, so __cdecl with this pushed first.
    void sprintf(Float3 *pos, const char *fmt, ...);
    // 0x4084f0. Like sprintf, for debug text.
    void drawf_debug(Float3 *pos, const char *fmt, ...);
};

extern AsciiManager *g_AsciiManager;
