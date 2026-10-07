#pragma once

#include "AnmManager.h"
#include "Camera.h"
#include "UpdateFunc.h"
#include "ZunTimer.h"
#include "types.h"

// One floating score number. Layout from ExpHP's th-re-data
// (zAsciiPopupString).
struct PopupString
{
    char digits[8];
    u8 unk_8[4];
    Float3 pos;
    f32 unk_18;
    D3DCOLOR color;
    ZunTimer time;
    u8 unk_34[0x3c - 0x34];
    union
    {
        i32 unk_3c;
        struct
        {
            u8 active;
            u8 num_digits;
        };
    };
    // The season release bonus strings (13 and up): the bonus (negative
    // for none) and its multiplier.
    i32 bonus;
    f32 bonus_rate;
};

// The small score popups over collected items and destroyed enemies.
// ExpHP: zAsciiPopupManager.
struct PopupManager
{
    u32 flags;
    UpdateFunc *on_tick_func;
    UpdateFunc *on_draw_func;
    AnmLoaded *ascii_anm;
    i32 next_index;
    i32 unk_14;
    AnmVm vm;
    PopupString strings[18];
    // The copy kept for the LoLK-style pause snapshot.
    PopupString snapshot_strings[18];

    DECOMP_NOINLINE PopupManager();
    ~PopupManager();
    int initialize();
    static PopupManager *create();

    int on_tick();
    int on_draw();
    static int __fastcall on_tick_thunk(void *arg);
    static int __fastcall on_draw_thunk(void *arg);

    // Shows value rising from pos in one of the first 10 strings. Works on
    // g_PopupManager; LTCG dropped this.
    HARNESS_CALLED void generate_small_score_popup(Float3 *pos, i32 value, D3DCOLOR color);
};

extern PopupManager *g_PopupManager;
