#pragma once

#include <d3d9.h>

#include "UpdateFunc.h"
#include "ZunMath.h"
#include "ZunTimer.h"
#include "decomp.h"
#include "types.h"

// Full-screen fades, flashes and screen shake. Layout from ExpHP's
// th-re-data (zScreenEffect). The meaning of the creation arguments depends
// on the mode; arg_18 is the duration in frames for most of them.
struct ScreenEffect
{
    u32 flags;
    UpdateFunc *on_tick;
    UpdateFunc *on_draw;
    i32 mode;
    i32 unk_10;
    // Alpha of the drawn rectangle, 0-255.
    i32 alpha;
    i32 arg_18;
    i32 arg_1c;
    i32 arg_20;
    i32 arg_24;
    i32 unk_28;
    ZunTimer timer;

    ~ScreenEffect();

    // Creates an effect and registers its update functions (ExpHP:
    // ScreenEffect::operator new).
    static ScreenEffect *LTCG_FASTCALL create(i32 mode, i32 arg_18, i32 arg_1c, i32 arg_20, i32 arg_24,
                                             i32 draw_priority);
    void initialize(i32 mode, i32 arg_18, i32 arg_1c, i32 arg_20, i32 arg_24, i32 draw_priority);

    static i32 __fastcall on_tick_fade_in(ScreenEffect *self);
    static i32 __fastcall on_tick_shake(ScreenEffect *self);
    static i32 __fastcall on_tick_fade_out(ScreenEffect *self);
    static i32 __fastcall on_tick_flash(ScreenEffect *self);
    static i32 __fastcall on_tick_hold(ScreenEffect *self);
    static i32 __fastcall on_tick_pulse(ScreenEffect *self);
    static i32 __fastcall on_tick_shake_with_ramp(ScreenEffect *self);
    static i32 __fastcall on_draw_viewport(ScreenEffect *self);
    static i32 __fastcall on_draw_screen(ScreenEffect *self);
    static i32 __fastcall on_draw_screen_2(ScreenEffect *self);
    static i32 __fastcall on_draw_arcade(ScreenEffect *self);
    static i32 __fastcall on_draw_arcade_2(ScreenEffect *self);
    // on_cleanup of the on_tick function: deletes the effect. Code that
    // creates effects inline (the bombs) uses it too.
    static i32 __fastcall on_cleanup(ScreenEffect *self);
};
