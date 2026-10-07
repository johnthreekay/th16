#include <math.h>
#include <stddef.h>
#include <string.h>

#include "AnmManager.h"
#include "AsciiManager.h"
#include "Bomb.h"
#include "Enemy.h"
#include "EnemyManager.h"
#include "Globals.h"
#include "GameThread.h"
#include "Gui.h"
#include "Player.h"
#include "ReplayManager.h"
#include "Scorefile.h"
#include "SoundManager.h"
#include "Spellcard.h"
#include "EffectManager.h"
#include "StageData.h"
#include "Supervisor.h"
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
    AnmVm *vm = get_vm_or_clear(text_anm_ids[2]);
    if (vm == NULL)
    {
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

// TODO: this and name trade esi/edi, and later code differs in register allocation.
// FUNCTION: TH16 0x417f00
void Spellcard::start(i32 spell_id, const char *name, i32 arg_2, i32 arg_3)
{
    __asm finit;
    time = 0;
    this->spell_id = spell_id;
    strcpy(this->name, name);
    flags |= 3;
    flags &= ~0x98;
    if (g_ReplayManager->mode != 1)
    {
        strcpy(g_Scorefile->characters[g_Globals.subshot + g_Globals.character].spells[spell_id].name, name);
        i32 practice = g_Globals.game_mode == 2;
        ScorefileSpell *spell = &g_Scorefile->characters[g_Globals.subshot + g_Globals.character].spells[spell_id];
        if (spell->attempts[practice] < 99999)
        {
            spell->attempts[practice]++;
        }
        strcpy(g_Scorefile->characters[4].spells[spell_id].name, name);
        spell = &g_Scorefile->characters[4].spells[spell_id];
        if (spell->attempts[practice] < 99999)
        {
            spell->attempts[practice]++;
        }
    }
    g_Gui->interrupt_spell_vms_2();
    flags &= ~0x20;
    ticks = 1;
    flags &= ~0x40;
    text_anm_ids[0] = g_AsciiManager->ascii_anm->create_effect(0, -1, NULL);
    text_anm_ids[1] = g_Supervisor.text_anm->create_effect(2, -1, NULL);
    text_anm_ids[2] = g_AsciiManager->ascii_anm->create_effect(1, -1, NULL);
    AnmManager *anm = g_AnmManager;
    g_AnmManager->draw_text_right(get_vm_or_clear(text_anm_ids[1]), 0xffffff, 0, 0, 0, name);
    g_SoundManager.play_sound_centered(0x21, 0);
    boss_anm_id = g_EffectManager->effect_anm->create_effect(0xd, -1, NULL);
    EnemyInf *boss = NULL;
    i32 boss_id = g_EnemyManager->inner.boss_ids[0];
    if (boss_id != 0)
    {
        for (EnemyList *node = g_EnemyManager->active_enemy_list_head; node != NULL; node = node->next)
        {
            boss = node->entry;
            if (boss->enemy_id == boss_id)
            {
                break;
            }
        }
    }
    boss_pos = boss->enemy.final_pos.pos;
    AnmVm *vm = g_AnmManager->get_vm_with_id(boss_anm_id);
    if (vm != NULL)
    {
        vm->entity_pos = boss->enemy.final_pos.pos;
    }
    find_child_of(boss_anm_id, 0xb)->int_vars[2] = arg_2;
    find_child_of(boss_anm_id, 0xc)->int_vars[2] = arg_2;
    timeout = arg_2;
    i32 bonuses[5] = {500000, 1000000, 1500000, 2000000, 1000000};
    bonus = bonuses[g_Globals.difficulty] * g_Globals.stage_num;
    bonus_max = bonus >= 1000000000 ? 999999999 : bonus;
    g_EffectManager->effect_anm->create_effect(0x14, -1, NULL);
    StageBoss *stage_boss = &g_stage_data->bosses[g_Globals.chapter < 43 && g_stage_data->bosses[1].spell_bg_anm_slot != -1];
    background_anm_id = g_EnemyManager->anim_statement_anms[stage_boss->spell_bg_anm_slot]->create_effect(
        stage_boss->spell_bg_script, -1, NULL);
    flags = (flags & ~0x200) | ((stage_boss->spell_flag_200 << 9) & 0x200);
    stage_boss = &g_stage_data->bosses[arg_3];
    if (stage_boss->spell_anm_slot != -1)
    {
        g_EnemyManager->anim_statement_anms[stage_boss->spell_anm_slot]->create_effect(stage_boss->spell_script, -1,
                                                                                       NULL);
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

// TODO: the inlined timer tick keeps the frame in xmm0 (ours xmm1), the
// boss smoothing is scheduled differently (boss_pos += (pos - boss_pos) *
// 0.05f gives the original's code but a /GS cookie), and the original
// duplicates the return.
// FUNCTION: TH16 0x417930
i32 Spellcard::on_tick_body()
{
    if (!(flags & SPELLCARD_ACTIVE))
    {
        return 1;
    }
    ticks++;
    if (time.current >= 60 && !(flags & SPELLCARD_FLAG_200))
    {
        g_Stage->stage_flags &= ~STAGE_FLAG_1;
    }
    if (time.current >= 300 && !(flags & SPELLCARD_NO_BONUS_DECAY))
    {
        bonus = (bonus - (bonus_max - bonus_max / 3) / (timeout - 300)) / 10 * 10;
    }
    time.tick();
    if (time.current >= 120)
    {
        Player *player = g_Player;
        if (!(flags & SPELLCARD_TEXT_MOVED))
        {
            if ((!(flags & SPELLCARD_TEXT_AT_BOTTOM) && 96.0f > player->inner.pos.y) ||
                ((flags & SPELLCARD_TEXT_AT_BOTTOM) && player->inner.pos.y > 352.0f))
            {
                for (i32 i = 0; i < 3; i++)
                {
                    AnmManager::interrupt_tree(text_anm_ids[i], 3);
                }
                flags |= SPELLCARD_TEXT_MOVED;
            }
        }
        else if ((!(flags & SPELLCARD_TEXT_AT_BOTTOM) && player->inner.pos.y > 128.0f) ||
                 ((flags & SPELLCARD_TEXT_AT_BOTTOM) && 320.0f > player->inner.pos.y))
        {
            for (i32 i = 0; i < 3; i++)
            {
                AnmManager::interrupt_tree(text_anm_ids[i], 2);
            }
            flags &= ~SPELLCARD_TEXT_MOVED;
        }
    }
    EnemyInf *boss = g_EnemyManager->find_enemy_by_id(g_EnemyManager->inner.boss_ids[0]);
    f32 x = (boss->enemy.final_pos.pos.x - boss_pos.x) * 0.05f + boss_pos.x;
    f32 y = (boss->enemy.final_pos.pos.y - boss_pos.y) * 0.05f + boss_pos.y;
    f32 z = (boss->enemy.final_pos.pos.z - boss_pos.z) * 0.05f + boss_pos.z;
    boss_pos.x = x;
    boss_pos.y = y;
    boss_pos.z = z;
    AnmVm *vm = g_AnmManager->get_vm_with_id(boss_anm_id);
    if (vm != NULL)
    {
        vm->entity_pos = boss_pos;
    }
    if (flags & SPELLCARD_FLAG_20)
    {
        if (g_MainBomb->in_use == 1)
        {
            return 1;
        }
        flags &= ~SPELLCARD_FLAG_20;
    }
    return 1;
}
