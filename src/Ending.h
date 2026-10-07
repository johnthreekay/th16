#pragma once

#include "AnmManager.h"
#include "AnmVm.h"
#include "Thread.h"
#include "UpdateFunc.h"
#include "ZunTimer.h"
#include "types.h"

// EndingScriptVm::flags.
enum EndingScriptFlags
{
    // Set when the script starts; the music fade clears it. Not read.
    ENDING_SCRIPT_MUSIC = 1 << 0,
    // Holding skip or shot fast-forwards the script (the staff roll, and
    // the endings past the eighth).
    ENDING_SCRIPT_SKIPPABLE = 1 << 1,
    // Waiting for ENDING_LOAD_ANM's loading thread.
    ENDING_SCRIPT_WAITING = 1 << 2,
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

// Ending script opcodes (no community names; named after what they do).
enum EndingOpcode
{
    ENDING_END = 0,
    // Writes the next of the five text lines (an obfuscated string),
    // clearing the page first when it starts a new one.
    ENDING_TEXT = 3,
    // Fades the text out.
    ENDING_TEXT_HIDE = 4,
    // Waits the given time (forever if negative) or for a key.
    ENDING_WAIT = 5,
    // Like ENDING_WAIT, then the next line starts a new page.
    ENDING_PAGE_WAIT = 6,
    // Loads an anm file into slot 20 + n on a thread, waiting for it.
    ENDING_LOAD_ANM = 7,
    // Picture slot, anm slot, script: replaces a picture.
    ENDING_PICTURE = 8,
    ENDING_TEXT_COLOR = 9,
    // Plays a music file (and BGM 15, or 16 for th16_14).
    ENDING_MUSIC = 10,
    // Fades the music out over 3 seconds.
    ENDING_MUSIC_FADE = 11,
    // Goes on to the staff roll for the difficulty.
    ENDING_STAFF_ROLL = 12,
    // Screen effects 0 and 5 for the given time.
    ENDING_SCREEN_EFFECT_0 = 13,
    ENDING_SCREEN_EFFECT_5 = 14,
    // ENDING_PICTURE on Normal, Hard or Lunatic only.
    ENDING_PICTURE_NORMAL = 15,
    ENDING_PICTURE_HARD = 16,
    ENDING_PICTURE_LUNATIC = 17,
};

// Runs an ending or staff roll script. ExpHP: zEndingChildF0.
struct EndingScriptVm
{
    u8 unk_0[4];
    // Frames since the script started (ticked by Ending::on_tick_body).
    ZunTimer time_alive;
    ZunTimer script_time;
    // Counts down the waits of ENDING_WAIT and ENDING_PAGE_WAIT.
    ZunTimer wait_timer;
    // The five text lines.
    AnmId line_ids[5];
    EndingInstr *instr;
    u8 unk_58[0x70 - 0x58];
    union
    {
        i32 anm_slot;
        // The file the loading thread reads (ENDING_LOAD_ANM).
        const char *anm_filename;
    };
    // EndingScriptFlags.
    u32 flags;
    // The text line ENDING_TEXT writes next.
    i32 line_index;
    D3DCOLOR text_color;
    // Indexed by anm_index.
    AnmLoaded *anms[4];
    // Pictures started by ENDING_PICTURE.
    AnmId picture_ids[16];
    ThreadInf thread;
    // ENDING_LOAD_ANM's slot (20 + anm_index in the ANM manager).
    i32 anm_index;

    EndingScriptVm(void *script);
    ~EndingScriptVm();
    // Runs the instructions whose time has come; -1 at the end.
    i32 run();
};

// Ending::flags.
enum EndingFlags
{
    // The player has not seen this ending before: waits cannot be skipped.
    ENDING_NEW = 1 << 0,
    // No ending had been seen before: the staff roll cannot be
    // fast-forwarded.
    ENDING_FIRST_EVER = 1 << 1,
};

// The ending scene: the character's ending (eNN.msg, a bad ending after a
// continue), then the staff roll. Layout from ExpHP (zEnding).
struct Ending
{
    u32 flags_0;
    UpdateFunc *on_tick;
    UpdateFunc *on_draw;
    u8 unk_c[4];
    // The loaded ending script file.
    void *script_file;
    EndingScriptVm *script_vm;
    // Character * 2, plus 1 for the bad ending.
    i32 ending_index;
    // EndingFlags.
    u32 flags;
    i32 ticks;

    Ending();
    ~Ending();

    static Ending *create();
    static void destroy();
    // Records the ending in the score file, loads its script and starts it.
    i32 initialize();
    // 0x419170. Reads a script file, replacing script_file. Every caller
    // goes through g_Ending, so LTCG replaced this with the global.
    HARNESS_CALLED void *load_script(const char *filename);
    i32 on_tick_body();
    static i32 __fastcall on_tick_callback(Ending *self);
    static i32 __fastcall on_draw_callback(Ending *self);
};

extern Ending *g_Ending;
