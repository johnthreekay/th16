#pragma once

#include "AnmManager.h"
#include "AnmVm.h"
#include "Thread.h"
#include "UpdateFunc.h"
#include "ZunTimer.h"
#include "types.h"

enum EndingChildFlags
{
    ENDING_CHILD_SKIPPABLE = 1 << 1,
    ENDING_CHILD_WAITING = 1 << 2,
};

// Runs the ending script. ExpHP: zEndingChildF0.
struct EndingChildF0
{
    u8 unk_0[4];
    ZunTimer timer_4;
    ZunTimer timer_18;
    ZunTimer timer_2c;
    AnmId anm_ids[5];
    u8 unk_54[0x70 - 0x54];
    i32 anm_slot;
    u32 flags;
    u8 unk_78[0x80 - 0x78];
    AnmLoaded *anms[(0xd0 - 0x80) / 4];
    ThreadInf thread;
    i32 anm_index;

    EndingChildF0(void *script);
    ~EndingChildF0();
    i32 run();
};

enum EndingFlags
{
    ENDING_FLAG_1 = 1 << 0,
    // Seen this ending before: allow skipping.
    ENDING_FLAG_2 = 1 << 1,
};

// The ending scene. Layout from ExpHP (zEnding).
struct Ending
{
    u32 flags_0;
    UpdateFunc *on_tick;
    UpdateFunc *on_draw;
    u8 unk_c[4];
    // The loaded ending script file.
    void *script_file;
    EndingChildF0 *child;
    i32 ending_index;
    u32 flags;
    i32 ticks;

    Ending();
    ~Ending();

    static Ending *create();
    static void destroy();
    i32 initialize();
    i32 on_tick_body();
    static i32 __fastcall on_tick_callback(Ending *self);
    static i32 __fastcall on_draw_callback(Ending *self);
};

extern Ending *g_Ending;
