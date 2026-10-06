#pragma once

#include <string.h>

#include "UpdateFunc.h"
#include "ZunTimer.h"
#include "decomp.h"
#include "types.h"

// Full-screen effects: shakes, fades, flashes. Fire and forget: each one
// deletes itself when done. Layout from ExpHP (zScreenEffect).
struct ScreenEffect
{
    u32 flags;
    UpdateFunc *on_tick_func;
    UpdateFunc *on_draw_func;
    // Picks the callbacks (see initialize).
    i32 mode;
    u8 unk_10[4];
    i32 unk_14;
    // Meaning depends on the mode (duration, strength, color, ...).
    i32 arg_18;
    i32 arg_1c;
    i32 arg_20;
    i32 arg_24;
    u8 unk_28[4];
    ZunTimer timer;

    ScreenEffect()
    {
        memset(this, 0, sizeof(ScreenEffect));
        flags |= 2;
    }

    // 0x45d150 (ExpHP: ScreenEffect::operator new).
    static ScreenEffect *create(i32 mode, i32 arg_18, i32 arg_1c, i32 arg_20, i32 arg_24, i32 draw_priority);
    // 0x45d1a0
    void initialize(i32 mode, i32 arg_18, i32 arg_1c, i32 arg_20, i32 arg_24, i32 draw_priority);

    // 0x45c630, 0x45c900, 0x45c9e0, 0x45cac0, 0x45cbd0, 0x45cd30, 0x45cf10
    static int __fastcall on_tick_a(void *arg);
    static int __fastcall on_tick_b(void *arg);
    static int __fastcall on_tick_c(void *arg);
    static int __fastcall on_tick_d(void *arg);
    static int __fastcall on_tick_e(void *arg);
    static int __fastcall on_tick_shake(void *arg);
    static int __fastcall on_tick_f(void *arg);
    // 0x45c860, 0x45c990, 0x45cb50, 0x45cba0, 0x45ccc0
    static int __fastcall on_draw_a(void *arg);
    static int __fastcall on_draw_b(void *arg);
    static int __fastcall on_draw_c(void *arg);
    static int __fastcall on_draw_d(void *arg);
    static int __fastcall on_draw_e(void *arg);
    // 0x45d130
    static int __fastcall on_cleanup(void *arg);
};

enum ScreenEffectMode
{
    SCREEN_EFFECT_SHAKE = 1,
    SCREEN_EFFECT_FADE_IN = 2,
    SCREEN_EFFECT_FADE_OUT = 3,
    SCREEN_EFFECT_SHAKE_2 = 8,
};
