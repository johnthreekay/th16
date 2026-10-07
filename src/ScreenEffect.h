#pragma once

#include <d3d9.h>
#include <string.h>

#include "UpdateFunc.h"
#include "ZunMath.h"
#include "ZunTimer.h"
#include "decomp.h"
#include "types.h"

// Debug logging, compiled out of the release build (0x45d410).
void screen_debug_log(const char *fmt, ...);

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

    ScreenEffect()
    {
    }
    // The constructor of the static instance (0x4d9dd0), which logs its
    // creation; create clears the effects it allocates itself.
    ScreenEffect(const char *log)
    {
        screen_debug_log(log);
        memset(this, 0, sizeof(ScreenEffect));
        flags |= 2;
    }
    ~ScreenEffect();

    // Creates an effect and registers its update functions (ExpHP:
    // ScreenEffect::operator new).
    static ScreenEffect *LTCG_FASTCALL create(i32 mode, i32 arg_18, i32 arg_1c, i32 arg_20, i32 arg_24,
                                             i32 draw_priority);
    void initialize(i32 mode, i32 arg_18, i32 arg_1c, i32 arg_20, i32 arg_24, i32 draw_priority);

    // create and initialize as LTCG inlines them, with the switch folded
    // away, into callers that pass a constant mode (the bombs, the stage).
    // initialize's out-of-line copy (0x45d1a0) is this body too.
    static __forceinline ScreenEffect *create_inline(i32 mode, i32 arg_18, i32 arg_1c, i32 arg_20, i32 arg_24,
                                                    i32 draw_priority)
    {
        ScreenEffect *effect = new ScreenEffect;
        memset(effect, 0, sizeof(ScreenEffect));
        effect->flags |= 2;
        effect->initialize_inline(mode, arg_18, arg_1c, arg_20, arg_24, draw_priority);
        return effect;
    }
    __forceinline void initialize_inline(i32 mode, i32 arg_18, i32 arg_1c, i32 arg_20, i32 arg_24,
                                         i32 draw_priority);

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

__forceinline void ScreenEffect::initialize_inline(i32 mode, i32 arg_18, i32 arg_1c, i32 arg_20, i32 arg_24,
                                                   i32 draw_priority)
{
    UpdateFunc *f;

    switch (mode)
    {
    case 0:
        f = g_UpdateFuncRegistry->create_func((UpdateFuncCallback)on_tick_fade_in);
        f->arg = this;
        f->flags |= UPDATE_FUNC_ACTIVE;
        g_UpdateFuncRegistry->register_on_tick(f, 0x13);
        on_tick = f;
        f = g_UpdateFuncRegistry->create_func((UpdateFuncCallback)on_draw_viewport);
        f->arg = this;
        f->flags |= UPDATE_FUNC_ACTIVE;
        g_UpdateFuncRegistry->register_on_draw(f, draw_priority);
        on_draw = f;
        break;
    case 1:
        f = g_UpdateFuncRegistry->create_func((UpdateFuncCallback)on_tick_shake);
        f->arg = this;
        f->flags |= UPDATE_FUNC_ACTIVE;
        g_UpdateFuncRegistry->register_on_tick(f, 0x13);
        on_tick = f;
        break;
    case 2:
        f = g_UpdateFuncRegistry->create_func((UpdateFuncCallback)on_tick_fade_out);
        f->arg = this;
        f->flags |= UPDATE_FUNC_ACTIVE;
        g_UpdateFuncRegistry->register_on_tick(f, 0x13);
        on_tick = f;
        f = g_UpdateFuncRegistry->create_func((UpdateFuncCallback)on_draw_screen);
        f->arg = this;
        f->flags |= UPDATE_FUNC_ACTIVE;
        g_UpdateFuncRegistry->register_on_draw(f, draw_priority);
        on_draw = f;
        break;
    case 3:
        alpha = 255;
        f = g_UpdateFuncRegistry->create_func((UpdateFuncCallback)on_tick_fade_in);
        f->arg = this;
        f->flags |= UPDATE_FUNC_ACTIVE;
        g_UpdateFuncRegistry->register_on_tick(f, 0x13);
        on_tick = f;
        f = g_UpdateFuncRegistry->create_func((UpdateFuncCallback)on_draw_screen);
        f->arg = this;
        f->flags |= UPDATE_FUNC_ACTIVE;
        g_UpdateFuncRegistry->register_on_draw(f, draw_priority);
        on_draw = f;
        break;
    case 5:
        f = g_UpdateFuncRegistry->create_func((UpdateFuncCallback)on_tick_fade_out);
        f->arg = this;
        f->flags |= UPDATE_FUNC_ACTIVE;
        g_UpdateFuncRegistry->register_on_tick(f, 0x13);
        on_tick = f;
        f = g_UpdateFuncRegistry->create_func((UpdateFuncCallback)on_draw_viewport);
        f->arg = this;
        f->flags |= UPDATE_FUNC_ACTIVE;
        g_UpdateFuncRegistry->register_on_draw(f, draw_priority);
        on_draw = f;
        break;
    case 4:
        f = g_UpdateFuncRegistry->create_func((UpdateFuncCallback)on_tick_pulse);
        f->flags |= UPDATE_FUNC_ACTIVE;
        f->arg = this;
        g_UpdateFuncRegistry->register_on_tick(f, 0x13);
        on_tick = f;
        f = g_UpdateFuncRegistry->create_func((UpdateFuncCallback)on_draw_arcade_2);
        f->arg = this;
        f->flags |= UPDATE_FUNC_ACTIVE;
        g_UpdateFuncRegistry->register_on_draw(f, draw_priority);
        on_draw = f;
        break;
    case 6:
        f = g_UpdateFuncRegistry->create_func((UpdateFuncCallback)on_tick_flash);
        f->arg = this;
        f->flags |= UPDATE_FUNC_ACTIVE;
        g_UpdateFuncRegistry->register_on_tick(f, 0x13);
        on_tick = f;
        f = g_UpdateFuncRegistry->create_func((UpdateFuncCallback)on_draw_screen_2);
        f->arg = this;
        f->flags |= UPDATE_FUNC_ACTIVE;
        g_UpdateFuncRegistry->register_on_draw(f, draw_priority);
        on_draw = f;
        break;
    case 7:
        f = g_UpdateFuncRegistry->create_func((UpdateFuncCallback)on_tick_flash);
        f->flags |= UPDATE_FUNC_ACTIVE;
        f->arg = this;
        g_UpdateFuncRegistry->register_on_tick(f, 0x13);
        on_tick = f;
        f = g_UpdateFuncRegistry->create_func((UpdateFuncCallback)on_draw_arcade);
        f->arg = this;
        f->flags |= UPDATE_FUNC_ACTIVE;
        g_UpdateFuncRegistry->register_on_draw(f, draw_priority);
        on_draw = f;
        break;
    case 8:
        f = g_UpdateFuncRegistry->create_func((UpdateFuncCallback)on_tick_shake_with_ramp);
        f->arg = this;
        f->flags |= UPDATE_FUNC_ACTIVE;
        g_UpdateFuncRegistry->register_on_tick(f, 0x13);
        on_tick = f;
        break;
    case 9:
        f = g_UpdateFuncRegistry->create_func((UpdateFuncCallback)on_tick_hold);
        f->arg = this;
        f->flags |= UPDATE_FUNC_ACTIVE;
        g_UpdateFuncRegistry->register_on_tick(f, 0x13);
        on_tick = f;
        f = g_UpdateFuncRegistry->create_func((UpdateFuncCallback)on_draw_screen_2);
        f->arg = this;
        f->flags |= UPDATE_FUNC_ACTIVE;
        g_UpdateFuncRegistry->register_on_draw(f, draw_priority);
        on_draw = f;
        break;
    }
    on_tick->on_cleanup = (UpdateFuncCallback)on_cleanup;
    timer.reset();
    this->mode = mode;
    this->arg_18 = arg_18;
    this->arg_1c = arg_1c;
    this->arg_20 = arg_20;
    this->arg_24 = arg_24;
}

// The modes, named after their callbacks.
enum ScreenEffectMode
{
    SCREEN_EFFECT_SHAKE = 1,
    // Covers the screen with a color (on_tick_fade_out).
    SCREEN_EFFECT_FADE_OUT = 2,
    // Uncovers it again (on_tick_fade_in, starting opaque).
    SCREEN_EFFECT_FADE_IN = 3,
    SCREEN_EFFECT_SHAKE_WITH_RAMP = 8,
};
