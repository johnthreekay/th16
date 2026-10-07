#pragma once

#include <d3dx9math.h>

#include "AnmVm.h"
#include "ZunTimer.h"
#include "UpdateFunc.h"
#include "decomp.h"
#include "types.h"

enum SpellcardFlags
{
    SPELLCARD_ACTIVE = 1 << 0,
    // Cleared when the player gets hit or bombs: no bonus.
    SPELLCARD_CAPTURABLE = 1 << 1,
    // The name banner moved out of the player's way.
    SPELLCARD_TEXT_MOVED = 1 << 2,
    SPELLCARD_NO_BONUS_DECAY = 1 << 3,
    SPELLCARD_FLAG_20 = 1 << 5,
    SPELLCARD_TIMING = 1 << 6,
    SPELLCARD_FLAG_80 = 1 << 7,
    SPELLCARD_TEXT_AT_BOTTOM = 1 << 8,
    SPELLCARD_FLAG_200 = 1 << 9,
};

// The spell card being declared, its bonus and its on-screen name. Layout
// from ExpHP (zSpellcard).
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
    i32 unk_88;
    i32 ticks;
    i32 unk_90;
    u8 unk_94[0xa4 - 0x94];
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
};

// Difficulty (0-3, 4 for Extra) of each spell card.
extern i8 g_spell_difficulty[0x78];
HARNESS_CALLED i32 count_spells_of_difficulty(i32 difficulty);

extern Spellcard *g_Spellcard;
