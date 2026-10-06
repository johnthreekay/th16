#include <string.h>

#include "AnmManager.h"
#include "Spellcard.h"
#include "UpdateFunc.h"

// GLOBAL: TH16 0x4a6db0
Spellcard *g_Spellcard;

Spellcard::Spellcard()
{
    memset(this, 0, sizeof(Spellcard));
    flags_0 |= 2;
    g_Spellcard = this;
}

// FUNCTION: TH16 0x417700
i32 Spellcard::initialize()
{
    UpdateFunc *f = g_UpdateFuncRegistry->create_func((UpdateFuncCallback)on_tick_callback);
    f->flags &= ~UPDATE_FUNC_ACTIVE;
    f->arg = this;
    g_UpdateFuncRegistry->register_on_tick(f, 0x1e);
    on_tick = f;

    f = g_UpdateFuncRegistry->create_func((UpdateFuncCallback)on_draw_callback);
    f->flags &= ~UPDATE_FUNC_ACTIVE;
    f->arg = this;
    g_UpdateFuncRegistry->register_on_draw(f, 0xc);
    on_draw = f;

    time.reset();
    return 0;
}

// TODO: the original clears each id before loading the next one to push.
// FUNCTION: TH16 0x417790
Spellcard::~Spellcard()
{
    AnmManager::unload_vm(text_anm_ids[0]);
    text_anm_ids[0].id = 0;
    AnmManager::unload_vm(text_anm_ids[1]);
    text_anm_ids[1].id = 0;
    AnmManager::unload_vm(text_anm_ids[2]);
    text_anm_ids[2].id = 0;
    g_UpdateFuncRegistry->unregister_locked(on_tick);
    g_UpdateFuncRegistry->unregister_locked(on_draw);
    g_Spellcard = NULL;
}

// FUNCTION: TH16 0x4178a0
Spellcard *Spellcard::create()
{
    Spellcard *s = new Spellcard();
    if (s->initialize() != 0)
    {
        delete s;
        return NULL;
    }
    return s;
}

// FUNCTION: TH16 0x417ee0
i32 __fastcall Spellcard::on_tick_callback(Spellcard *self)
{
    return self->on_tick_body();
}

// FUNCTION: TH16 0x417ef0
i32 __fastcall Spellcard::on_draw_callback(Spellcard *self)
{
    return self->on_draw_body();
}
