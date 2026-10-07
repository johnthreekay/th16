#include <math.h>
#include <stddef.h>
#include <string.h>

#include "AnmManager.h"
#include "AsciiManager.h"
#include "Globals.h"
#include "GameThread.h"
#include "Gui.h"
#include "ReplayManager.h"
#include "Scorefile.h"
#include "SoundManager.h"
#include "Spellcard.h"
#include "Stage.h"
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
    AsciiInf *ascii = g_AsciiManager;
    pos.y = 35.0f;
    pos.z = 0.0f;
    ascii->font_id = 2;
    ascii->group = 2;
    ascii->color.a = vm->color_1.a;
    if (flags & SPELLCARD_CAPTURABLE)
    {
        pos.x = 266.0f;
        ascii->create_stringf(&pos, "%8d", bonus);
    }
    else
    {
        // "$" is the font's "bonus failed" glyph.
        pos.x = 282.0f;
        ascii->create_stringf(&pos, "$");
    }
    pos.x = 360.0f;
    pos.y = 35.0f;
    pos.z = 0.0f;
    i32 practice = g_Globals.game_mode == 2;
    i32 captures = g_Scorefile->characters[g_Globals.subshot + g_Globals.character].spells[spell_id].captures[practice];
    if (captures >= 100)
    {
        g_AsciiManager->create_stringf(&pos, "MASTER");
    }
    else
    {
        i32 attempts = g_Scorefile->characters[g_Globals.subshot + g_Globals.character].spells[spell_id].attempts[practice];
        if (attempts >= 100)
        {
            g_AsciiManager->create_stringf(&pos, "%.2d/99+", captures);
        }
        else
        {
            g_AsciiManager->create_stringf(&pos, "%.2d/%.2d", captures, attempts);
        }
    }
    g_AsciiManager->font_id = 0;
    g_AsciiManager->group = 0;
    g_AsciiManager->color.a = 0xff;
    return 1;
}

// FUNCTION: TH16 0x426840
HARNESS_CALLED void Spellcard::decode_time_code(i32 *seconds, i32 *hundredths)
{
    if (is_time_code_bad())
    {
        *seconds = 999;
        *hundredths = 99;
    }
    else
    {
        *seconds = (time_code / 100 % 1000 + 934) % 1000;
        *hundredths = (time_code % 100 + 67) % 100;
    }
}

// FUNCTION: TH16 0x4182f0
HARNESS_CALLED void Spellcard::end()
{
    if (!(flags & 1))
    {
        return;
    }
    g_Stage->stage_flags |= STAGE_FLAG_1;
    AnmManager::interrupt_tree(text_anm_ids[0], 1);
    AnmManager::interrupt_tree(text_anm_ids[1], 1);
    AnmManager::interrupt_tree(text_anm_ids[2], 1);
    flags &= ~1;
    delete_vm_and_clear(background_anm_id);
    flags &= ~0x20;
    g_Gui->interrupt_spell_vms_3();
    delete_vm_and_clear(boss_anm_id);
    if (flags & 2)
    {
        g_Globals.add_to_score(bonus);
        g_Gui->sub_42bcf0(bonus, 0);
        if (g_ReplayManager->mode != 1)
        {
            i32 practice = g_Globals.game_mode == 2;
            ScorefileSpell *spell = &g_Scorefile->characters[g_Globals.subshot + g_Globals.character].spells[spell_id];
            if (spell->captures[practice] < 99999)
            {
                spell->captures[practice]++;
            }
            spell = &g_Scorefile->characters[4].spells[spell_id];
            if (spell->captures[practice] < 99999)
            {
                spell->captures[practice]++;
            }
        }
        g_SoundManager.play_sound_centered(0x2e, 0);
    }
    else
    {
        g_Gui->sub_42bcf0(0, 1);
    }
    if (flags & 0x80)
    {
        g_SoundManager.play_sound_centered(0x45, 0);
    }
}

double LTCG_VECTORCALL get_runtime();

static_assert(offsetof(Spellcard, start_time) == 0x94, "Spellcard layout");
static_assert(offsetof(Spellcard, time_code) == 0xa4, "Spellcard layout");
static_assert(sizeof(Spellcard) == 0xbc, "Spellcard size");

// TODO: ours aligns the frame to 64 bytes for the doubles (the original
// does not), keeps the rounded time on the stack across floor instead of
// reloading it, and increments unk_88 through a register.
// FUNCTION: TH16 0x417bc0
void Spellcard::measure_real_time()
{
    Spellcard *sc = g_Spellcard;
    if (sc->flags & 1)
    {
        if (!(sc->flags & 0x40))
        {
            sc->start_time = get_runtime();
            sc->flags |= 0x40;
        }
        return;
    }
    if (!(sc->flags & 0x40))
    {
        return;
    }
    sc->unk_90 = sc->ticks;
    double elapsed = get_runtime() - sc->start_time;
    double rest = fmod(elapsed, 0.0167);
    sc->real_time_taken = elapsed - rest;
    if (rest >= 0.00835)
    {
        sc->real_time_taken += 0.0167;
    }
    double whole = floor(sc->real_time_taken);
    i32 seconds = (i32)whole;
    double fraction = sc->real_time_taken - whole;
    if (seconds >= 1000)
    {
        seconds = 999;
    }
    sc->real_time_taken = 0.0;
    sc->flags &= ~0x40;
    i32 hundredths = (i32)(fraction * 100.0);
    sc->time_code = ((seconds + 22 + hundredths) * 1000 + (seconds + 66) % 1000) * 100 + (hundredths + 33) % 100;
    if (g_GameThread->replay_mode == 0)
    {
        ((RpyGamestate *)g_ReplayManager->stage_gamestate_snapshots[g_Globals.stage_num])
            ->spell_time_codes[sc->unk_88] = sc->time_code;
        sc->unk_88++;
    }
    else
    {
        sc->time_code = g_ReplayManager->stages[g_Globals.stage_num].gamestate_at_stage_begin->spell_time_codes[sc->unk_88];
        if (sc->is_time_code_bad())
        {
            sc->time_code = 0x6ad1584;
        }
        sc->unk_88++;
    }
}
