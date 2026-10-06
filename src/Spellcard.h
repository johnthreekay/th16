#pragma once

#include <d3dx9math.h>

#include "AnmVm.h"
#include "Timer.h"
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
    Timer time;
    char name[0x40];
    i32 spell_id;
    u32 flags;
    i32 bonus;
    i32 bonus_max;
    i32 timeout;
    i32 unk_88;
    i32 ticks;
    i32 unk_90;
    u8 unk_94[0xa8 - 0x94];
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
};

extern Spellcard *g_Spellcard;
