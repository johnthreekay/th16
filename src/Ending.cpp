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

// Defined in GameThread.cpp.
extern i32 g_cancel_screen_effects;

// The ending's anm files go in ANM manager slots 20-23 (ANM_SLOT_ENDING_FIRST on).

// text.anm script of the first text line.
#define ENDING_TEXT_ANM_LINE 0x2e

// Holding skip, or shot for 20 frames, fast-forwards.
#define ENDING_FAST_FORWARD_HELD()                                                                                     \
    (g_hardware_input & INPUT_SKIP || (g_hardware_input & INPUT_SHOT && g_hardware_input_held_4a51c4 >= 20))

i32 ending_load_anm();

Ending::Ending()
{
    g_Ending = this;
    memset(this, 0, sizeof(Ending));
    flags_0 |= 2;
}

// Unloads the ending's anm files and script; clears g_cancel_screen_effects.
// FUNCTION: TH16 0x419450
Ending::~Ending()
{
    g_UpdateFuncRegistry->unregister_locked(on_tick);
    g_UpdateFuncRegistry->unregister_locked(on_draw);
    delete script_vm;
    script_vm = NULL;
    g_AnmManager->unload_anm(ANM_SLOT_ENDING_FIRST);
    g_AnmManager->unload_anm(ANM_SLOT_ENDING_FIRST + 1);
    g_AnmManager->unload_anm(ANM_SLOT_ENDING_FIRST + 2);
    g_AnmManager->unload_anm(ANM_SLOT_ENDING_FIRST + 3);
    if (script_file != NULL)
    {
        free(script_file);
        script_file = NULL;
    }
    script_file = NULL;
    g_Ending = NULL;
    g_cancel_screen_effects = 0;
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

// Runs the script; fast-forwarding runs it again in the same frame (11 of
// every 12 frames). At the end the game goes back to the title (or mode
// 16).
// FUNCTION: TH16 0x4196c0
i32 Ending::on_tick_body()
{
    EndingScriptVm *c = script_vm;
    if (c->run() == 0)
    {
        c->time_alive.tick();
        ticks++;
        if (!(script_vm->flags & ENDING_SCRIPT_WAITING) && !(flags & ENDING_FIRST_EVER) && script_vm->flags & ENDING_SCRIPT_SKIPPABLE)
        {
            if (ENDING_FAST_FORWARD_HELD())
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
        g_Supervisor.gamemode_to_switch_to =
            g_Supervisor.flags & SUPERVISOR_IDLE_ON_EXIT ? GAMEMODE_IDLE : GAMEMODE_TITLE_SCORE_ENTRY;
    }
    return UPDATE_FUNC_CONTINUE;
}

// FUNCTION: TH16 0x4197a0
i32 __fastcall Ending::on_tick_callback(Ending *self)
{
    return self->on_tick_body();
}

// FUNCTION: TH16 0x4197b0
i32 __fastcall Ending::on_draw_callback(Ending *self)
{
    return UPDATE_FUNC_CONTINUE;
}

// Indexed by Ending::ending_index.
// GLOBAL: TH16 0x491780
const char *const g_ending_files[8] = {
    "e01.msg", "e02.msg", "e03.msg", "e04.msg", "e05.msg", "e06.msg", "e07.msg", "e08.msg",
};

// Per difficulty (Easy also stands for the rest).
// GLOBAL: TH16 0x4917a0
const char *const g_staff_files[4] = {
    "staff1.msg",
    "staff2.msg",
    "staff3.msg",
    "staff4.msg",
};

// Waits for the loading thread and deletes the text lines.
// FUNCTION: TH16 0x4190b0
EndingScriptVm::~EndingScriptVm()
{
    thread.join_if_running();
    AnmManager *anm = g_AnmManager;
    for (i32 i = 0; i < 5; i++)
    {
        anm->delete_vm_inline(line_ids[i]);
        line_ids[i].id = 0;
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

// TODO: the original frame has 4 more bytes: the spilled UpdateFunc pointer
// keeps [ebp-0x10] to itself, where ours shares it with the AnmId result.
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

    // The score file keeps a bit per difficulty for each ending (Easy is
    // bit 0 alone), and entry 8 for having seen any.
    ending_index = g_Globals.subshot + g_Globals.character;
    ending_index = g_Globals.continues_used != 0 ? ending_index * 2 + 1 : ending_index * 2;
    if (g_Scorefile->endings_seen[ending_index] == 0)
    {
        flags |= ENDING_NEW;
    }
    if (g_Scorefile->endings_seen[8] == 0)
    {
        flags |= ENDING_FIRST_EVER;
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
    script_vm = new EndingScriptVm((u8 *)script_file + ((i32 *)script_file)[1]);
    if (ending_index >= 8)
    {
        script_vm->flags |= ENDING_SCRIPT_SKIPPABLE;
    }
    return 0;
}

// Creates the five text lines (16-pixel glyphs) and starts the script.
// FUNCTION: TH16 0x4197c0
EndingScriptVm::EndingScriptVm(void *script)
{
    memset(this, 0, sizeof(EndingScriptVm));
    for (u32 i = 0; i < 5; i++)
    {
        line_ids[i] = g_Supervisor.text_anm->create_effect(i + ENDING_TEXT_ANM_LINE, -1, NULL);
        get_vm_or_clear(line_ids[i])->font_dims[0] = 16;
        get_vm_or_clear(line_ids[i])->font_dims[1] = 16;
        get_vm_or_clear(line_ids[i])->flags_hi &= ~ANM_VM_TEXT_NO_OUTLINE;
    }
    instr = (EndingInstr *)script;
    time_alive.reset();
    script_time.reset();
    wait_timer.reset();
    flags |= ENDING_SCRIPT_MUSIC;
    text_color = 0xffffff;
}

// The loading thread of ENDING_LOAD_ANM.
// FUNCTION: TH16 0x41a310
i32 ending_load_anm()
{
    EndingScriptVm *script_vm = g_Ending->script_vm;
    script_vm->anms[script_vm->anm_index] =
        AnmManager::preload_anm(script_vm->anm_index + ANM_SLOT_ENDING_FIRST, script_vm->anm_filename);
    script_vm->flags &= ~ENDING_SCRIPT_WAITING;
    g_AsciiManager->hide_now_loading_inline();
    return 0;
}

// FUNCTION: TH16 0x41a360
HARNESS_CALLED void AsciiInf::hide_now_loading()
{
    AnmManager::interrupt_tree(now_loading_id, 1);
    now_loading_id.id = 0;
}

// FUNCTION: TH16 0x41a390
HARNESS_CALLED void AsciiInf::show_now_loading(f32 x, f32 y)
{
    show_now_loading_inline(x, y);
}

#define ENDING_NEXT_INSTR(instr) ((EndingInstr *)((u8 *)(instr) + (instr)->size + 4))

// Waits (ENDING_WAIT, ENDING_PAGE_WAIT) end on shot or enter; fast-forward
// (not for a new ending) ends them every 6 frames.
// FUNCTION: TH16 0x4199f0
i32 EndingScriptVm::run()
{
    if (flags & ENDING_SCRIPT_WAITING)
    {
        return 0;
    }
    while (script_time.current >= instr->time)
    {
        switch (instr->opcode)
        {
        case ENDING_END:
            return -1;
        case ENDING_TEXT:
            if (line_index == 0)
            {
                for (i32 i = 0; i < 5; i++)
                {
                    g_AnmManager->draw_text(get_vm_or_clear(line_ids[i]), 0xffffff, 0, 0, 0, 0, " ");
                    AnmManager::interrupt_tree(line_ids[i], 3);
                }
                AnmManager *anm = g_AnmManager;
                AnmVm *vm = anm->get_vm_with_id(line_ids[0]);
                if (vm == NULL)
                {
                    line_ids[0].id = 0;
                }
                anm->draw_text(vm, text_color, 0, 0, 0, 0, decode_msg_string((const char *)instr->args));
                AnmManager::interrupt_tree(line_ids[0], 2);
                line_index++;
            }
            else
            {
                g_AnmManager->draw_text(get_vm_or_clear(line_ids[line_index]), text_color, 0, 0, 0, 0,
                                        decode_msg_string((const char *)instr->args));
                AnmManager::interrupt_tree(line_ids[line_index], 2);
                line_index++;
                if (line_index >= 5)
                {
                    line_index = 0;
                }
            }
            break;
        case ENDING_TEXT_HIDE:
            for (i32 i = 0; i < 5; i++)
            {
                AnmManager::interrupt_tree(line_ids[i], 3);
            }
            break;
        case ENDING_WAIT:
            if (wait_timer.current <= 0)
            {
                wait_timer.set_value(instr->args[0]);
            }
            wait_timer--;
            if (instr->args[0] < 0)
            {
                wait_timer.set_value(999);
            }
            if (g_hardware_input_pressed & (INPUT_ENTER | INPUT_SHOT) || wait_timer.current <= 0)
            {
                g_SoundManager.play_sound_centered(SE_PLST00, 0);
                wait_timer.set_value(0);
                break;
            }
            if (g_Ending->flags & ENDING_NEW)
            {
                return 0;
            }
            if (!ENDING_FAST_FORWARD_HELD())
            {
                return 0;
            }
            if (wait_timer.current % 6 != 0)
            {
                return 0;
            }
            wait_timer.set_value(0);
            break;
        case ENDING_PAGE_WAIT:
            if (wait_timer.current <= 0)
            {
                wait_timer.set_value(instr->args[0]);
            }
            wait_timer--;
            if (!(g_hardware_input_pressed & (INPUT_ENTER | INPUT_SHOT)) && wait_timer.current > 0)
            {
                if (g_Ending->flags & ENDING_NEW)
                {
                    return 0;
                }
                if (!ENDING_FAST_FORWARD_HELD())
                {
                    return 0;
                }
                if (wait_timer.current % 6 != 0)
                {
                    return 0;
                }
            }
            else
            {
                g_SoundManager.play_sound_centered(SE_PLST00, 0);
            }
            wait_timer.set_value(0);
            line_index = 0;
            g_cancel_screen_effects = 0;
            break;
        case ENDING_LOAD_ANM:
        {
            g_AsciiManager->show_now_loading(480.0f, 392.0f);
            i32 slot = instr->args[0] + ANM_SLOT_ENDING_FIRST;
            if (slot >= 0)
            {
                g_AnmManager->unload_anm_out_of_line(slot);
            }
            flags |= ENDING_SCRIPT_WAITING;
            anm_filename = (const char *)&instr->args[1];
            anm_index = instr->args[0];
            thread.restart((ThreadStart)ending_load_anm, this);
            instr = ENDING_NEXT_INSTR(instr);
            return 0;
        }
        case ENDING_PICTURE:
            delete_vm_and_clear(picture_ids[instr->args[0]]);
            picture_ids[instr->args[0]] = anms[instr->args[1]]->create_effect(instr->args[2], -1, NULL);
            break;
        case ENDING_PICTURE_NORMAL:
            if (g_Globals.difficulty == DIFFICULTY_NORMAL)
            {
                delete_vm_and_clear(picture_ids[instr->args[0]]);
                picture_ids[instr->args[0]] = anms[instr->args[1]]->create_effect(instr->args[2], -1, NULL);
            }
            break;
        case ENDING_PICTURE_HARD:
            if (g_Globals.difficulty == DIFFICULTY_HARD)
            {
                delete_vm_and_clear(picture_ids[instr->args[0]]);
                picture_ids[instr->args[0]] = anms[instr->args[1]]->create_effect(instr->args[2], -1, NULL);
            }
            break;
        case ENDING_PICTURE_LUNATIC:
            if (g_Globals.difficulty == DIFFICULTY_LUNATIC)
            {
                delete_vm_and_clear(picture_ids[instr->args[0]]);
                picture_ids[instr->args[0]] = anms[instr->args[1]]->create_effect(instr->args[2], -1, NULL);
            }
            break;
        case ENDING_TEXT_COLOR:
            text_color = instr->args[0];
            break;
        case ENDING_MUSIC:
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
        case ENDING_MUSIC_FADE:
        {
            // Supervisor::fade_out_bgm(3.0f), inlined.
            g_SoundManager.modify_bgm(BGM_FADE_OUT,
                                      g_game_speed != 0.0f && !(g_game_speed > 1.0f) ? 3.0f / g_game_speed : 3.0f,
                                      "");
            flags &= ~ENDING_SCRIPT_MUSIC;
            break;
        }
        case ENDING_STAFF_ROLL:
        {
            for (i32 i = 0; i < 5; i++)
            {
                delete_vm_and_clear(line_ids[i]);
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
            memset(this, 0, sizeof(EndingScriptVm));
            instr = (EndingInstr *)((u8 *)data + ((i32 *)data)[1]);
            time_alive.reset_inline();
            script_time.reset_inline();
            wait_timer.reset_inline();
            flags |= ENDING_SCRIPT_SKIPPABLE;
            text_color = 0xffffff;
            continue;
        }
        case ENDING_FADE_IN:
            ScreenEffect::create_inline(SCREEN_EFFECT_FADE_IN_VIEWPORT, instr->args[0], 0, 0, 0, 0x54);
            break;
        case ENDING_FADE_OUT:
            ScreenEffect::create_inline(SCREEN_EFFECT_FADE_OUT_VIEWPORT, instr->args[0], 0, 0, 0, 0x54);
            break;
        }
        instr = ENDING_NEXT_INSTR(instr);
    }
    script_time.tick_split();
    return 0;
}
