#include <stdlib.h>
#include <string.h>

#include "AnmManager.h"
#include "AsciiManager.h"
#include "CriticalSections.h"
#include "Ending.h"
#include "FileSystem.h"
#include "GameErrorContext.h"
#include "Gui.h"
#include "Globals.h"
#include "Input.h"
#include "Scorefile.h"
#include "ScreenEffect.h"
#include "SoundManager.h"
#include "Ecl.h"
#include "Supervisor.h"
#include "UpdateFunc.h"

// GLOBAL: TH16 0x4a6dbc
Ending *g_Ending;

// Cleared when the ending goes away; also used by the screen effects.
extern i32 g_unk_4c0f40;

// How many frames the bit at 0x114 / 4 of the raw button state has been
// held.
extern u32 g_hardware_input_held_4a51c4;

#define BUTTON_SHOT (1 << 0)
#define BUTTON_SKIP (1 << 9)

extern u32 g_hardware_input_pressed;

i32 ending_load_anm();

Ending::Ending()
{
    g_Ending = this;
    memset(this, 0, sizeof(Ending));
    flags_0 |= 2;
}

// FUNCTION: TH16 0x419450
Ending::~Ending()
{
    g_UpdateFuncRegistry->unregister_locked(on_tick);
    g_UpdateFuncRegistry->unregister_locked(on_draw);
    delete child;
    child = NULL;
    g_AnmManager->unload_anm(20);
    g_AnmManager->unload_anm(21);
    g_AnmManager->unload_anm(22);
    g_AnmManager->unload_anm(23);
    if (script_file != NULL)
    {
        free(script_file);
        script_file = NULL;
    }
    script_file = NULL;
    g_Ending = NULL;
    g_unk_4c0f40 = 0;
}

// FUNCTION: TH16 0x419640
Ending *Ending::create()
{
    Ending *e = new Ending();
    if (e->initialize() != 0)
    {
        delete e;
        return NULL;
    }
    return e;
}

// FUNCTION: TH16 0x4196a0
void Ending::destroy()
{
    delete g_Ending;
}

// FUNCTION: TH16 0x4196c0
i32 Ending::on_tick_body()
{
    EndingChildF0 *c = child;
    if (c->run() == 0)
    {
        c->timer_4.tick();
        ticks++;
        if (!(child->flags & ENDING_CHILD_WAITING) && !(flags & ENDING_FLAG_2) && child->flags & ENDING_CHILD_SKIPPABLE)
        {
            if (g_hardware_input & BUTTON_SKIP || (g_hardware_input & BUTTON_SHOT && g_hardware_input_held_4a51c4 >= 20))
            {
                if (ticks % 12 != 0)
                {
                    return UPDATE_FUNC_RESTART_FROM_FIRST;
                }
            }
        }
    }
    else
    {
        g_Supervisor.gamemode_to_switch_to = g_Supervisor.flags & SUPERVISOR_FLAG_2000 ? 2 : 16;
    }
    return 1;
}

// FUNCTION: TH16 0x4197a0
i32 __fastcall Ending::on_tick_callback(Ending *self)
{
    return self->on_tick_body();
}

// FUNCTION: TH16 0x4197b0
i32 __fastcall Ending::on_draw_callback(Ending *self)
{
    return 1;
}

// GLOBAL: TH16 0x491780
const char *const g_ending_files[8] = {
    "e01.msg", "e02.msg", "e03.msg", "e04.msg", "e05.msg", "e06.msg", "e07.msg", "e08.msg",
};

// GLOBAL: TH16 0x4917a0
const char *const g_staff_files[4] = {
    "staff1.msg",
    "staff2.msg",
    "staff3.msg",
    "staff4.msg",
};

// FUNCTION: TH16 0x4190b0
EndingChildF0::~EndingChildF0()
{
    thread.join_if_running();
    AnmManager *anm = g_AnmManager;
    for (i32 i = 0; i < 5; i++)
    {
        anm->delete_vm_inline(anm_ids[i]);
        anm_ids[i].id = 0;
    }
}

// FUNCTION: TH16 0x419170
HARNESS_CALLED void *Ending::load_script(const char *filename)
{
    strcpy(g_ecl_path, "");
    strcat(g_ecl_path, filename);
    void *data = file_read_all(g_ecl_path, NULL, 0);
    if (script_file != NULL)
    {
        free(script_file);
        script_file = NULL;
    }
    script_file = data;
    return data;
}

// TODO: the original frame has 4 more bytes (see show_now_loading).
// FUNCTION: TH16 0x4191f0
i32 Ending::initialize()
{
    UpdateFunc *f = g_UpdateFuncRegistry->create_func((UpdateFuncCallback)on_tick_callback);
    f->flags |= UPDATE_FUNC_ACTIVE;
    f->arg = this;
    g_UpdateFuncRegistry->register_on_tick(f, 0x23);
    on_tick = f;

    // create_func, inlined here in the original.
    f = new UpdateFunc;
    f->flags |= UPDATE_FUNC_HEAP_ALLOCATED;
    f->function = (UpdateFuncCallback)on_draw_callback;
    f->on_registration = NULL;
    f->on_cleanup = NULL;
    f->flags |= UPDATE_FUNC_ACTIVE;
    f->arg = this;
    g_UpdateFuncRegistry->register_on_draw(f, 0x43);
    on_draw = f;

    g_AsciiManager->show_now_loading_inline(480.0f, 392.0f);

    ending_index = g_Globals.subshot + g_Globals.character;
    ending_index = g_Globals.continues_used != 0 ? ending_index * 2 + 1 : ending_index * 2;
    if (g_Scorefile->endings_seen[ending_index] == 0)
    {
        flags |= ENDING_FLAG_1;
    }
    if (g_Scorefile->endings_seen[8] == 0)
    {
        flags |= ENDING_FLAG_2;
    }
    g_Scorefile->endings_seen[ending_index] |= 1;
    if (g_Globals.difficulty == DIFFICULTY_NORMAL)
    {
        g_Scorefile->endings_seen[ending_index] |= 2;
    }
    else if (g_Globals.difficulty == DIFFICULTY_HARD)
    {
        g_Scorefile->endings_seen[ending_index] |= 4;
    }
    else if (g_Globals.difficulty == DIFFICULTY_LUNATIC)
    {
        g_Scorefile->endings_seen[ending_index] |= 8;
    }
    if (ending_index < 8)
    {
        g_Scorefile->endings_seen[8] = 1;
    }

    strcpy(g_ecl_path, "");
    strcat(g_ecl_path, g_ending_files[ending_index]);
    script_file = file_read_all(g_ecl_path, NULL, 0);
    if (script_file == NULL)
    {
        // "The data is corrupted"
        g_GameErrorContext.log("\x83" "f\x81[\x83^\x82\xaa\x89\xf3\x82\xea\x82\xc4\x82\xa2\x82\xdc\x82\xb7\r\n");
        return -1;
    }
    child = new EndingChildF0((u8 *)script_file + ((i32 *)script_file)[1]);
    if (ending_index >= 8)
    {
        child->flags |= ENDING_CHILD_SKIPPABLE;
    }
    return 0;
}

// FUNCTION: TH16 0x4197c0
EndingChildF0::EndingChildF0(void *script)
{
    memset(this, 0, sizeof(EndingChildF0));
    for (u32 i = 0; i < 5; i++)
    {
        anm_ids[i] = g_Supervisor.text_anm->create_effect(i + 0x2e, -1, NULL);
        get_vm_or_clear(anm_ids[i])->font_dims[0] = 16;
        get_vm_or_clear(anm_ids[i])->font_dims[1] = 16;
        get_vm_or_clear(anm_ids[i])->flags_hi &= ~0x1000;
    }
    instr = (EndingInstr *)script;
    timer_4.reset();
    timer_18.reset();
    timer_2c.reset();
    flags |= 1;
    text_color = 0xffffff;
}

// Loading thread of instruction 7.
// FUNCTION: TH16 0x41a310
i32 ending_load_anm()
{
    EndingChildF0 *child = g_Ending->child;
    child->anms[child->anm_index] = AnmManager::preload_anm(child->anm_index + 20, child->anm_filename);
    child->flags &= ~ENDING_CHILD_WAITING;
    g_AsciiManager->hide_now_loading_inline();
    return 0;
}

// FUNCTION: TH16 0x41a360
HARNESS_CALLED void AsciiInf::hide_now_loading()
{
    AnmManager::interrupt_tree(now_loading_id, 1);
    now_loading_id.id = 0;
}

// TODO: the original frame has an unused 4-byte slot that ours lacks.
// FUNCTION: TH16 0x41a390
HARNESS_CALLED void AsciiInf::show_now_loading(f32 x, f32 y)
{
    show_now_loading_inline(x, y);
}

#define ENDING_NEXT_INSTR(instr) ((EndingInstr *)((u8 *)(instr) + (instr)->size + 4))

// TODO: register allocation: the original never uses ebx (spills this instead), and play_bgm*
// get this in ecx here (LTCG dropped it in the original).
// FUNCTION: TH16 0x4199f0
i32 EndingChildF0::run()
{
    if (flags & ENDING_CHILD_WAITING)
    {
        return 0;
    }
    while (timer_18.current >= instr->time)
    {
        switch (instr->opcode)
        {
        case 0:
            return -1;
        case 3:
            if (line_index == 0)
            {
                for (i32 i = 0; i < 5; i++)
                {
                    g_AnmManager->draw_text(get_vm_or_clear(anm_ids[i]), 0xffffff, 0, 0, 0, 0, " ");
                    AnmManager::interrupt_tree(anm_ids[i], 3);
                }
                AnmManager *anm = g_AnmManager;
                AnmVm *vm = anm->get_vm_with_id(anm_ids[0]);
                if (vm == NULL)
                {
                    anm_ids[0].id = 0;
                }
                anm->draw_text(vm, text_color, 0, 0, 0, 0, decode_msg_string((const char *)instr->args));
                AnmManager::interrupt_tree(anm_ids[0], 2);
                line_index++;
            }
            else
            {
                g_AnmManager->draw_text(get_vm_or_clear(anm_ids[line_index]), text_color, 0, 0, 0, 0,
                                        decode_msg_string((const char *)instr->args));
                AnmManager::interrupt_tree(anm_ids[line_index], 2);
                line_index++;
                if (line_index >= 5)
                {
                    line_index = 0;
                }
            }
            break;
        case 4:
            for (i32 i = 0; i < 5; i++)
            {
                AnmManager::interrupt_tree(anm_ids[i], 3);
            }
            break;
        case 5:
            if (timer_2c.current <= 0)
            {
                timer_2c.set_value(instr->args[0]);
            }
            timer_2c--;
            if (instr->args[0] < 0)
            {
                timer_2c.set_value(999);
            }
            if (g_hardware_input_pressed & 0x80001 || timer_2c.current <= 0)
            {
                g_SoundManager.play_sound_centered(0, 0);
                timer_2c.set_value(0);
                break;
            }
            if (g_Ending->flags & ENDING_FLAG_1)
            {
                return 0;
            }
            if (!(g_hardware_input & BUTTON_SKIP || (g_hardware_input & BUTTON_SHOT && g_hardware_input_held_4a51c4 >= 20)))
            {
                return 0;
            }
            if (timer_2c.current % 6 != 0)
            {
                return 0;
            }
            timer_2c.set_value(0);
            break;
        case 6:
            if (timer_2c.current <= 0)
            {
                timer_2c.set_value(instr->args[0]);
            }
            timer_2c--;
            if (!(g_hardware_input_pressed & 0x80001) && timer_2c.current > 0)
            {
                if (g_Ending->flags & ENDING_FLAG_1)
                {
                    return 0;
                }
                if (!(g_hardware_input & BUTTON_SKIP ||
                      (g_hardware_input & BUTTON_SHOT && g_hardware_input_held_4a51c4 >= 20)))
                {
                    return 0;
                }
                if (timer_2c.current % 6 != 0)
                {
                    return 0;
                }
            }
            else
            {
                g_SoundManager.play_sound_centered(0, 0);
            }
            timer_2c.set_value(0);
            line_index = 0;
            g_unk_4c0f40 = 0;
            break;
        case 7:
        {
            g_AsciiManager->show_now_loading(480.0f, 392.0f);
            i32 slot = instr->args[0] + 20;
            if (slot >= 0)
            {
                g_AnmManager->unload_anm_46d720(slot);
            }
            flags |= ENDING_CHILD_WAITING;
            anm_filename = (const char *)&instr->args[1];
            anm_index = instr->args[0];
            thread.restart((ThreadStart)ending_load_anm, this);
            instr = ENDING_NEXT_INSTR(instr);
            return 0;
        }
        case 8:
            delete_vm_and_clear(vm_ids[instr->args[0]]);
            vm_ids[instr->args[0]] = anms[instr->args[1]]->create_effect(instr->args[2], -1, NULL);
            break;
        case 15:
            if (g_Globals.difficulty == DIFFICULTY_NORMAL)
            {
                delete_vm_and_clear(vm_ids[instr->args[0]]);
                vm_ids[instr->args[0]] = anms[instr->args[1]]->create_effect(instr->args[2], -1, NULL);
            }
            break;
        case 16:
            if (g_Globals.difficulty == DIFFICULTY_HARD)
            {
                delete_vm_and_clear(vm_ids[instr->args[0]]);
                vm_ids[instr->args[0]] = anms[instr->args[1]]->create_effect(instr->args[2], -1, NULL);
            }
            break;
        case 17:
            if (g_Globals.difficulty == DIFFICULTY_LUNATIC)
            {
                delete_vm_and_clear(vm_ids[instr->args[0]]);
                vm_ids[instr->args[0]] = anms[instr->args[1]]->create_effect(instr->args[2], -1, NULL);
            }
            break;
        case 9:
            text_color = instr->args[0];
            break;
        case 10:
            g_Supervisor.play_bgm_wav(0, (const char *)instr->args);
            if (strcmp((const char *)instr->args, "bgm/th16_14") == 0)
            {
                g_Supervisor.play_bgm(0, 15);
            }
            else
            {
                g_Supervisor.play_bgm(0, 16);
            }
            break;
        case 11:
        {
            // Supervisor::fade_out_bgm(3.0f), inlined.
            f32 seconds = 3.0f;
            if (g_game_speed != 0.0f && !(g_game_speed > 1.0f))
            {
                seconds /= g_game_speed;
            }
            g_SoundManager.modify_bgm(BGM_FADE_OUT, seconds, "");
            flags &= ~1;
            break;
        }
        case 12:
        {
            for (i32 i = 0; i < 5; i++)
            {
                delete_vm_and_clear(anm_ids[i]);
            }
            void *data;
            switch (g_Globals.difficulty)
            {
            case DIFFICULTY_NORMAL:
                data = g_Ending->load_script(g_staff_files[1]);
                break;
            case DIFFICULTY_HARD:
                data = g_Ending->load_script(g_staff_files[2]);
                break;
            case DIFFICULTY_LUNATIC:
                data = g_Ending->load_script(g_staff_files[3]);
                break;
            default:
                data = g_Ending->load_script(g_staff_files[0]);
                break;
            }
            if (data == NULL)
            {
                return -1;
            }
            memset(this, 0, sizeof(EndingChildF0));
            instr = (EndingInstr *)((u8 *)data + ((i32 *)data)[1]);
            timer_4.reset_inline();
            timer_18.reset_inline();
            timer_2c.reset_inline();
            flags |= ENDING_CHILD_SKIPPABLE;
            text_color = 0xffffff;
            continue;
        }
        case 13:
            ScreenEffect::create_inline(0, instr->args[0], 0, 0, 0, 0x54);
            break;
        case 14:
            ScreenEffect::create_inline(5, instr->args[0], 0, 0, 0, 0x54);
            break;
        }
        instr = ENDING_NEXT_INSTR(instr);
    }
    timer_18.tick();
    return 0;
}
