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
#include "ZunAsm.h"

// GLOBAL: TH16 0x4a6db0
Spellcard *g_Spellcard;

// The difficulty of each spell card, indexed by spell id. The 119 cards
// (#001 to #119) are listed in groups of their versions per difficulty;
// the last slot is unused.
// GLOBAL: TH16 0x491700
const i8 g_spell_difficulty[0x78] = {
    // Main game: one line per card, in its Easy to Lunatic versions.
    DIFFICULTY_EASY, DIFFICULTY_NORMAL, DIFFICULTY_HARD, DIFFICULTY_LUNATIC, // #001-#004
    DIFFICULTY_EASY, DIFFICULTY_NORMAL, DIFFICULTY_HARD, DIFFICULTY_LUNATIC, // #005-#008
    DIFFICULTY_EASY, DIFFICULTY_NORMAL, DIFFICULTY_HARD, DIFFICULTY_LUNATIC, // #009-#012
    DIFFICULTY_EASY, DIFFICULTY_NORMAL, DIFFICULTY_HARD, DIFFICULTY_LUNATIC, // #013-#016
    DIFFICULTY_EASY, DIFFICULTY_NORMAL, DIFFICULTY_HARD, DIFFICULTY_LUNATIC, // #017-#020
    // A card with only Hard and Lunatic versions.
    DIFFICULTY_HARD, DIFFICULTY_LUNATIC,                                     // #021-#022
    DIFFICULTY_EASY, DIFFICULTY_NORMAL, DIFFICULTY_HARD, DIFFICULTY_LUNATIC, // #023-#026
    DIFFICULTY_EASY, DIFFICULTY_NORMAL, DIFFICULTY_HARD, DIFFICULTY_LUNATIC, // #027-#030
    DIFFICULTY_EASY, DIFFICULTY_NORMAL, DIFFICULTY_HARD, DIFFICULTY_LUNATIC, // #031-#034
    DIFFICULTY_EASY, DIFFICULTY_NORMAL, DIFFICULTY_HARD, DIFFICULTY_LUNATIC, // #035-#038
    DIFFICULTY_EASY, DIFFICULTY_NORMAL, DIFFICULTY_HARD, DIFFICULTY_LUNATIC, // #039-#042
    DIFFICULTY_EASY, DIFFICULTY_NORMAL, DIFFICULTY_HARD, DIFFICULTY_LUNATIC, // #043-#046
    DIFFICULTY_EASY, DIFFICULTY_NORMAL, DIFFICULTY_HARD, DIFFICULTY_LUNATIC, // #047-#050
    DIFFICULTY_EASY, DIFFICULTY_NORMAL, DIFFICULTY_HARD, DIFFICULTY_LUNATIC, // #051-#054
    DIFFICULTY_EASY, DIFFICULTY_NORMAL, DIFFICULTY_HARD, DIFFICULTY_LUNATIC, // #055-#058
    DIFFICULTY_EASY, DIFFICULTY_NORMAL, DIFFICULTY_HARD, DIFFICULTY_LUNATIC, // #059-#062
    DIFFICULTY_EASY, DIFFICULTY_NORMAL, DIFFICULTY_HARD, DIFFICULTY_LUNATIC, // #063-#066
    DIFFICULTY_EASY, DIFFICULTY_NORMAL, DIFFICULTY_HARD, DIFFICULTY_LUNATIC, // #067-#070
    DIFFICULTY_EASY, DIFFICULTY_NORMAL, DIFFICULTY_HARD, DIFFICULTY_LUNATIC, // #071-#074
    DIFFICULTY_EASY, DIFFICULTY_NORMAL, DIFFICULTY_HARD, DIFFICULTY_LUNATIC, // #075-#078
    DIFFICULTY_EASY, DIFFICULTY_NORMAL, DIFFICULTY_HARD, DIFFICULTY_LUNATIC, // #079-#082
    DIFFICULTY_EASY, DIFFICULTY_NORMAL, DIFFICULTY_HARD, DIFFICULTY_LUNATIC, // #083-#086
    DIFFICULTY_EASY, DIFFICULTY_NORMAL, DIFFICULTY_HARD, DIFFICULTY_LUNATIC, // #087-#090
    DIFFICULTY_EASY, DIFFICULTY_NORMAL, DIFFICULTY_HARD, DIFFICULTY_LUNATIC, // #091-#094
    DIFFICULTY_EASY, DIFFICULTY_NORMAL, DIFFICULTY_HARD, DIFFICULTY_LUNATIC, // #095-#098
    DIFFICULTY_EASY, DIFFICULTY_NORMAL, DIFFICULTY_HARD, DIFFICULTY_LUNATIC, // #099-#102
    DIFFICULTY_EASY, DIFFICULTY_NORMAL, DIFFICULTY_HARD, DIFFICULTY_LUNATIC, // #103-#106
    // Extra stage.
    DIFFICULTY_EXTRA, DIFFICULTY_EXTRA, DIFFICULTY_EXTRA, DIFFICULTY_EXTRA, DIFFICULTY_EXTRA, // #107-#111
    DIFFICULTY_EXTRA, DIFFICULTY_EXTRA, DIFFICULTY_EXTRA, DIFFICULTY_EXTRA, DIFFICULTY_EXTRA, // #112-#116
    DIFFICULTY_EXTRA, DIFFICULTY_EXTRA, DIFFICULTY_EXTRA,                                     // #117-#119
};

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

// Draws the bonus (or the failed glyph) and the capture history next to
// the spell card name.
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
    ascii->font_id = ASCII_FONT_SMALL;
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
    i32 practice = g_Globals.game_mode == GAME_MODE_SPELL_PRACTICE;
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
    g_AsciiManager->font_id = ASCII_FONT_DEFAULT;
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
void Spellcard::start(i32 spell_id, const char *name, i32 time_limit, i32 boss_index)
{
    ZUN_ASM_FINIT();
    time = 0;
    this->spell_id = spell_id;
    strcpy(this->name, name);
    flags |= SPELLCARD_ACTIVE | SPELLCARD_CAPTURABLE;
    flags &= ~(SPELLCARD_TIMED_OUT | SPELLCARD_FLAG_10 | SPELLCARD_NO_BONUS_DECAY);
    if (g_ReplayManager->mode != 1)
    {
        strcpy(g_Scorefile->characters[g_Globals.subshot + g_Globals.character].spells[spell_id].name, name);
        i32 practice = g_Globals.game_mode == GAME_MODE_SPELL_PRACTICE;
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
    g_Gui->boss_timer_on_spell_start();
    flags &= ~SPELLCARD_EARLY_BOMB;
    ticks = 1;
    flags &= ~SPELLCARD_TIMING;
    text_anm_ids[0] = create_effect_via_pointer(g_AsciiManager->ascii_anm, 0, -1, NULL);
    text_anm_ids[1] = create_effect_via_pointer(g_Supervisor.text_anm, 2, -1, NULL);
    text_anm_ids[2] = create_effect_via_pointer(g_AsciiManager->ascii_anm, 1, -1, NULL);
    AnmManager *anm = g_AnmManager;
    g_AnmManager->draw_text_right(get_vm_or_clear(text_anm_ids[1]), 0xffffff, 0, 0, 0, name);
    g_SoundManager.play_sound_centered(SE_CAT00, 0);
    boss_anm_id = create_effect_via_pointer(g_EffectManager->effect_anm, 0xd, -1, NULL);
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
    find_child_of(boss_anm_id, 0xb)->int_vars[2] = time_limit;
    find_child_of(boss_anm_id, 0xc)->int_vars[2] = time_limit;
    timeout = time_limit;
    i32 bonuses[5] = {500000, 1000000, 1500000, 2000000, 1000000};
    bonus = bonuses[g_Globals.difficulty] * g_Globals.stage_num;
    bonus_max = bonus >= 1000000000 ? 999999999 : bonus;
    create_effect_via_pointer(g_EffectManager->effect_anm, 0x14, -1, NULL);
    StageBoss *stage_boss = &g_stage_data->bosses[g_Globals.chapter < 43 && g_stage_data->bosses[1].spell_bg_anm_slot != -1];
    background_anm_id = create_effect_via_pointer(g_EnemyManager->anim_statement_anms[stage_boss->spell_bg_anm_slot],
                                                  stage_boss->spell_bg_script, -1, NULL);
    flags = (flags & ~SPELLCARD_FLAG_200) | ((stage_boss->spell_flag_200 << 9) & SPELLCARD_FLAG_200);
    stage_boss = &g_stage_data->bosses[boss_index];
    if (stage_boss->spell_anm_slot != -1)
    {
        create_effect_via_pointer(g_EnemyManager->anim_statement_anms[stage_boss->spell_anm_slot],
                                  stage_boss->spell_script, -1, NULL);
    }
}

// show_notice is called through its member pointer (gui_show_notice_func):
// as direct calls, show_notice's wish for an aligned stack makes this
// function realign its frame, which the original does not.
// FUNCTION: TH16 0x4182f0
HARNESS_CALLED void Spellcard::end()
{
    if (!(flags & SPELLCARD_ACTIVE))
    {
        return;
    }
    g_Stage->stage_flags |= STAGE_VISIBLE;
    AnmManager::interrupt_tree(text_anm_ids[0], 1);
    AnmManager::interrupt_tree(text_anm_ids[1], 1);
    AnmManager::interrupt_tree(text_anm_ids[2], 1);
    flags &= ~SPELLCARD_ACTIVE;
    delete_vm_and_clear(background_anm_id);
    flags &= ~SPELLCARD_EARLY_BOMB;
    g_Gui->boss_timer_on_spell_end();
    delete_vm_and_clear(boss_anm_id);
    if (flags & SPELLCARD_CAPTURABLE)
    {
        g_Globals.add_to_score(bonus);
        (g_Gui->*gui_show_notice_func())(bonus, GUI_NOTICE_SPELL_BONUS);
        if (g_ReplayManager->mode != 1)
        {
            i32 practice = g_Globals.game_mode == GAME_MODE_SPELL_PRACTICE;
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
        g_SoundManager.play_sound_centered(SE_CARDGET_2, 0);
    }
    else
    {
        (g_Gui->*gui_show_notice_func())(0, GUI_NOTICE_BONUS_FAILED);
    }
    if (flags & SPELLCARD_TIMED_OUT)
    {
        g_SoundManager.play_sound_centered(SE_FAULT, 0);
    }
}

double LTCG_VECTORCALL get_runtime();

static_assert(offsetof(Spellcard, start_time) == 0x94, "Spellcard layout");
static_assert(offsetof(Spellcard, time_code) == 0xa4, "Spellcard layout");
static_assert(sizeof(Spellcard) == 0xbc, "Spellcard size");

// HARNESS_CALLED: with every caller visible it no longer realigns its frame
// to 64 bytes (found by the system agent).
// TODO: ours keeps the rounded time on the stack across floor instead of
// reloading it (a pointer of its own for the second read does not change it).
// FUNCTION: TH16 0x417bc0
HARNESS_CALLED void Spellcard::measure_real_time()
{
    Spellcard *sc = g_Spellcard;
    if (sc->flags & SPELLCARD_ACTIVE)
    {
        if (!(sc->flags & SPELLCARD_TIMING))
        {
            sc->start_time = get_runtime();
            sc->flags |= SPELLCARD_TIMING;
        }
        return;
    }
    if (!(sc->flags & SPELLCARD_TIMING))
    {
        return;
    }
    sc->frames_taken = sc->ticks;
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
    sc->flags &= ~SPELLCARD_TIMING;
    i32 hundredths = (i32)(fraction * 100.0);
    sc->time_code = ((seconds + 22 + hundredths) * 1000 + (seconds + 66) % 1000) * 100 + (hundredths + 33) % 100;
    if (g_GameThread->replay_mode == 0)
    {
        ((RpyGamestate *)g_ReplayManager->stage_gamestate_snapshots[g_Globals.stage_num])
            ->spell_time_codes[sc->cards_in_stage] = sc->time_code;
    }
    else
    {
        sc->time_code = g_ReplayManager->stages[g_Globals.stage_num].gamestate_at_stage_begin->spell_time_codes[sc->cards_in_stage];
        if (sc->is_time_code_bad())
        {
            sc->time_code = 0x6ad1584;
        }
    }
    sc->cards_in_stage++;
}

// TODO: the inlined timer tick keeps the frame in xmm0 (ours xmm1), and in the boss
// smoothing x and the 0.05f constant trade xmm0 and xmm1 (x is loaded last).
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
        g_Stage->stage_flags &= ~STAGE_VISIBLE;
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
    // The boss marker follows the boss with 5% smoothing. Written back through
    // a pointer of its own, so boss_pos is read again for the adds instead
    // of reusing the loads of the subtractions (as in the original).
    D3DXVECTOR3 *p = &boss_pos;
    f32 dx = boss->enemy.final_pos.pos.x - boss_pos.x;
    f32 dy = boss->enemy.final_pos.pos.y - boss_pos.y;
    f32 dz = boss->enemy.final_pos.pos.z - boss_pos.z;
    p->x = dx * 0.05f + p->x;
    p->y = dy * 0.05f + p->y;
    p->z = dz * 0.05f + p->z;
    AnmVm *vm = g_AnmManager->get_vm_with_id(boss_anm_id);
    if (vm != NULL)
    {
        vm->entity_pos = boss_pos;
    }
    if (flags & SPELLCARD_EARLY_BOMB)
    {
        if (g_MainBomb->in_use == 1)
        {
            return 1;
        }
        flags &= ~SPELLCARD_EARLY_BOMB;
        // A return of its own: the original does not merge it with the last.
        return 1;
    }
    return 1;
}
