#pragma once

#include <d3dx9math.h>

#include "AnmVm.h"
#include "ZunTimer.h"
#include "UpdateFunc.h"
#include "decomp.h"
#include "types.h"

// Spellcard::flags.
enum SpellcardFlags
{
    SPELLCARD_ACTIVE = 1 << 0,
    // Cleared when the player gets hit or bombs: no bonus.
    SPELLCARD_CAPTURABLE = 1 << 1,
    // The name banner moved out of the player's way.
    SPELLCARD_TEXT_MOVED = 1 << 2,
    // ECL spellTimeout: a survival card. The bonus does not decay, and
    // running out of time is not a timeout (SPELLCARD_TIMED_OUT).
    SPELLCARD_NO_BONUS_DECAY = 1 << 3,
    // Set by ECL 543, which also removes the boss effect; not read.
    SPELLCARD_FLAG_10 = 1 << 4,
    // The player bombed (or was hit while bombing) in the card's first 60
    // frames: the card stays capturable, but while the bomb lasts the
    // enemies take a thirtieth of the damage.
    SPELLCARD_EARLY_BOMB = 1 << 5,
    // measure_real_time's clock is running.
    SPELLCARD_TIMING = 1 << 6,
    // A non-survival card ran out of time (sound at the end).
    SPELLCARD_TIMED_OUT = 1 << 7,
    SPELLCARD_TEXT_AT_BOTTOM = 1 << 8,
    // From StageBoss::spell_flag_200: keeps STAGE_FLAG_1 set after the
    // card's first second.
    SPELLCARD_FLAG_200 = 1 << 9,
};

// The spell card being declared, its bonus and its on-screen name. Layout
// from ExpHP (zSpellcard). Its doubles sit at 4-byte boundaries.
#pragma pack(push, 4)
struct Spellcard
{
    u32 flags_0;
    UpdateFunc *on_tick;
    UpdateFunc *on_draw;
    AnmId background_anm_id;
    // Name banner pieces, interrupted together.
    AnmId text_anm_ids[3];
    // Follows the boss around.
    AnmId boss_anm_id;
    ZunTimer time;
    char name[0x40];
    i32 spell_id;
    u32 flags;
    i32 bonus;
    i32 bonus_max;
    i32 timeout;
    // How many cards the stage has had: indexes the replay's
    // spell_time_codes.
    i32 cards_in_stage;
    i32 ticks;
    // ticks when the clock stopped: the game time the card took, shown
    // with its result.
    i32 frames_taken;
    // Real time (get_runtime) when the card started, and how long it took,
    // rounded to frames. Only 4-byte aligned (see the pack pragma).
    double start_time;
    double real_time_taken;
    // Capture time, encoded with a check value against tampering.
    i32 time_code;
    D3DXVECTOR3 boss_pos;
    u8 unk_b4[0xbc - 0xb4];

    Spellcard();
    ~Spellcard();

    static Spellcard *create();
    i32 initialize();
    i32 on_tick_body();
    i32 on_draw_body();
    static i32 __fastcall on_tick_callback(Spellcard *self);
    static i32 __fastcall on_draw_callback(Spellcard *self);
    i32 is_time_code_bad();
    // Splits the capture time into seconds and hundredths (999 and 99 if
    // it was tampered with). Only called through g_Spellcard, which LTCG
    // put in place of this.
    HARNESS_CALLED void decode_time_code(i32 *seconds, i32 *hundredths);
    // 0x4182f0. Ends the spell card: removes its name and background, pays
    // out the bonus if it was captured and counts the capture. Only called
    // through g_Spellcard.
    HARNESS_CALLED void end();
    // 0x417f00. Starts the spell card: id, decoded name, the time limit in
    // frames (timeout, also shown by the boss effect's timer) and the
    // StageData boss whose spell effect plays.
    void start(i32 spell_id, const char *name, i32 time_limit, i32 boss_index);
    // 0x417bc0. Measures the card's real duration: starts the clock while
    // the card runs, then turns the time into the tamper-checked
    // time_code and records it in (or, during playback, reads it from) the
    // replay. Called once per presented frame.
    static void measure_real_time();
};
#pragma pack(pop)

// Difficulty (0-3, 4 for Extra) of each spell card.
extern i8 g_spell_difficulty[0x78];
HARNESS_CALLED i32 count_spells_of_difficulty(i32 difficulty);

extern Spellcard *g_Spellcard;
