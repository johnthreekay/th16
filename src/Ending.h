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

// One instruction of an ending script (eNN.msg, staffN.msg): a time, an
// opcode and size bytes of arguments.
struct EndingInstr
{
    u16 time;
    u8 opcode;
    u8 size;
    i32 args[1];
};

// Runs the ending script. ExpHP: zEndingChildF0.
struct EndingChildF0
{
    u8 unk_0[4];
    ZunTimer timer_4;
    // Script time.
    ZunTimer timer_18;
    // Counts down the waits of instructions 5 and 6.
    ZunTimer timer_2c;
    // The five text lines.
    AnmId anm_ids[5];
    EndingInstr *instr;
    u8 unk_58[0x70 - 0x58];
    union
    {
        i32 anm_slot;
        // The file the loading thread reads (instruction 7).
        const char *anm_filename;
    };
    u32 flags;
    // The text line instruction 3 writes next.
    i32 line_index;
    D3DCOLOR text_color;
    // Indexed by anm_index.
    AnmLoaded *anms[4];
    // Pictures started by instruction 8.
    AnmId vm_ids[16];
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
    // 0x419170. Reads a script file, replacing script_file. Every caller
    // goes through g_Ending, so LTCG replaced this with the global.
    HARNESS_CALLED void *load_script(const char *filename);
    i32 on_tick_body();
    static i32 __fastcall on_tick_callback(Ending *self);
    static i32 __fastcall on_draw_callback(Ending *self);
};

extern Ending *g_Ending;
