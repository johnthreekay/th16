#include <string.h>

#include "AnmManager.h"
#include "AsciiManager.h"
#include "Globals.h"
#include "Scorefile.h"
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

// FUNCTION: TH16 0x417790
Spellcard::~Spellcard()
{
    delete_vm_and_clear(text_anm_ids[0]);
    delete_vm_and_clear(text_anm_ids[1]);
    delete_vm_and_clear(text_anm_ids[2]);
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

// FUNCTION: TH16 0x417530
i32 Spellcard::is_time_code_bad()
{
    i32 a = time_code % 100;
    i32 b = time_code / 100 % 1000;
    a = (a + 67) % 100;
    b = (b + 934) % 1000;
    return time_code / 100000 - 22 != b + a;
}

// FUNCTION: TH16 0x4176d0
HARNESS_CALLED i32 count_spells_of_difficulty(i32 difficulty)
{
    i32 count = 0;
    for (i32 i = 0; i < 0x77; i++)
    {
        if (g_spell_difficulty[i] == difficulty)
        {
            count++;
        }
    }
    return count;
}

// FUNCTION: TH16 0x417d70
i32 Spellcard::on_draw_body()
{
    if (!(flags & SPELLCARD_ACTIVE))
    {
        return 1;
    }
    AnmVm *vm = g_AnmManager->get_vm_with_id(text_anm_ids[2]);
    if (vm == NULL)
    {
        text_anm_ids[2].id = 0;
        return 1;
    }
    D3DXVECTOR3 pos;
    AsciiManager *ascii = g_AsciiManager;
    pos.y = 35.0f;
    pos.z = 0.0f;
    ascii->font_id = 2;
    ascii->group = 2;
    ascii->color.a = vm->color_1.a;
    if (flags & SPELLCARD_CAPTURABLE)
    {
        pos.x = 266.0f;
        ascii->sprintf(&pos, "%8d", bonus);
    }
    else
    {
        // "$" is the font's "bonus failed" glyph.
        pos.x = 282.0f;
        ascii->sprintf(&pos, "$");
    }
    pos.x = 360.0f;
    pos.y = 35.0f;
    pos.z = 0.0f;
    i32 practice = g_Globals.game_mode == 2;
    i32 captures = g_Scorefile->characters[g_Globals.subshot + g_Globals.character].spells[spell_id].captures[practice];
    if (captures >= 100)
    {
        g_AsciiManager->sprintf(&pos, "MASTER");
    }
    else
    {
        i32 attempts = g_Scorefile->characters[g_Globals.subshot + g_Globals.character].spells[spell_id].attempts[practice];
        if (attempts >= 100)
        {
            g_AsciiManager->sprintf(&pos, "%.2d/99+", captures);
        }
        else
        {
            g_AsciiManager->sprintf(&pos, "%.2d/%.2d", captures, attempts);
        }
    }
    g_AsciiManager->font_id = 0;
    g_AsciiManager->group = 0;
    g_AsciiManager->color.a = 0xff;
    return 1;
}
