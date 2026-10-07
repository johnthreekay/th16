#include <stdlib.h>
#include <string.h>

#include "AnmManager.h"
#include "AsciiManager.h"
#include "BulletManager.h"
#include "EnemyManager.h"
#include "GameThread.h"
#include "Input.h"
#include "Laser.h"
#include "Player.h"
#include "Scorefile.h"
#include "SoundManager.h"
#include "Spellcard.h"
#include "CriticalSections.h"
#include "Ecl.h"
#include "FileSystem.h"
#include "GameErrorContext.h"
#include "Globals.h"
#include "Gui.h"
#include "StageData.h"
#include "Supervisor.h"
#include "UpdateFunc.h"

// GLOBAL: TH16 0x4a6dcc
Gui *g_Gui;

// GLOBAL: TH16 0x4a6dd0
MsgFile *g_msg_file_cache;

// TODO: inlined delete_vm loads the child list before storing the flags,
// and the loop does not reuse this's register for the id pointer.
// FUNCTION: TH16 0x4264a0
GuiMsgVm::~GuiMsgVm()
{
    delete_vm_and_clear(player_face);
    AnmManager *anm = g_AnmManager;
    for (i32 i = 0; i < 4; i++)
    {
        anm->delete_vm_inline(enemy_faces[i]);
        enemy_faces[i].id = 0;
    }
    anm->delete_vm_inline(id_54);
    id_54.id = 0;
    anm->delete_vm_inline(text_line_1);
    text_line_1.id = 0;
    anm->delete_vm_inline(text_line_2);
    text_line_2.id = 0;
    anm->delete_vm_inline(furigana_1);
    furigana_1.id = 0;
    anm->delete_vm_inline(furigana_2);
    furigana_2.id = 0;
    anm->delete_vm_inline(intro);
    intro.id = 0;
    anm->delete_vm_inline(textbox);
    textbox.id = 0;
}

// SYNTHETIC: TH16 0x427950
// GuiMsgVm::`scalar deleting destructor'

// FUNCTION: TH16 0x4268c0
Gui::Gui()
{
    memset(this, 0, sizeof(Gui));
    flags |= 2;
    g_Gui = this;
}

// TODO: the original frame has 4 more bytes and saves esi in the
// prologue; ours saves it after the early returns.
// FUNCTION: TH16 0x426b00
i32 Gui::initialize()
{
    front_anm = AnmManager::preload_anm(5, "front.anm");
    if (front_anm == NULL)
    {
        // "The data is corrupt."
        g_GameErrorContext.log("\x83" "f\x81[\x83^\x82\xaa\x89\xf3\x82\xea\x82\xc4\x82\xa2\x82\xdc\x82\xb7\r\n");
        return -1;
    }
    if (load_stage_files() != 0)
    {
        return -1;
    }

    UpdateFunc *f = g_UpdateFuncRegistry->create_func((UpdateFuncCallback)on_tick_callback);
    f->flags &= ~UPDATE_FUNC_ACTIVE;
    f->arg = this;
    g_UpdateFuncRegistry->register_on_tick(f, 0x20);
    on_tick = f;

    // create_func, inlined here in the original. The caller's arg store
    // goes first to come out in the original's order.
    f = new UpdateFunc;
    f->flags |= UPDATE_FUNC_HEAP_ALLOCATED;
    f->function = (UpdateFuncCallback)on_draw_1_callback;
    f->on_registration = NULL;
    f->on_cleanup = NULL;
    f->arg = this;
    f->flags &= ~UPDATE_FUNC_ACTIVE;
    g_UpdateFuncRegistry->register_on_draw(f, 0x33);
    on_draw_1 = f;

    f = g_UpdateFuncRegistry->create_func((UpdateFuncCallback)on_draw_2_callback);
    f->flags &= ~UPDATE_FUNC_ACTIVE;
    f->arg = this;
    g_UpdateFuncRegistry->register_on_draw(f, 0x30);
    on_draw_2 = f;
    return 0;
}

// TODO: ours saves esi/edi only around the strcpy branch; the original
// saves them in the prologue.
// FUNCTION: TH16 0x426c10
i32 Gui::load_stage_files()
{
    stage_logo_anm = AnmManager::preload_anm(6, g_stage_data->logo_anm_filename);
    if (stage_logo_anm == NULL)
    {
        // "The data is corrupt."
        g_GameErrorContext.log("\x83" "f\x81[\x83^\x82\xaa\x89\xf3\x82\xea\x82\xc4\x82\xa2\x82\xdc\x82\xb7\r\n");
        return -1;
    }
    if (g_msg_file_cache != NULL)
    {
        msg_file = g_msg_file_cache;
        debug_log("%s load Skip\n", g_stage_data->msg_files[g_Globals.subshot + g_Globals.character]);
        g_msg_file_cache = NULL;
    }
    else
    {
        strcpy(g_ecl_path, "");
        strcat(g_ecl_path, g_stage_data->msg_files[g_Globals.subshot + g_Globals.character]);
        msg_file = (MsgFile *)file_read_all(g_ecl_path, NULL, 0);
        if (msg_file == NULL)
        {
            g_GameErrorContext.log("\x83" "f\x81[\x83^\x82\xaa\x89\xf3\x82\xea\x82\xc4\x82\xa2\x82\xdc\x82\xb7\r\n");
            return -1;
        }
    }
    time_in_stage.reset();
    current_score = g_Globals.score;
    unk_1d0 = -1;
    unk_1d8 = -1;
    return 0;
}

// TODO: inlined delete_vm loads the child list before storing the flags.
// FUNCTION: TH16 0x427730
void Gui::release_stage_files()
{
    if (!(g_Globals.flags_lo_45c & 9))
    {
        g_AnmManager->unload_anm(6);
    }
    else
    {
        g_AnmManager->disable_vms_from_anm_file(stage_logo_anm);
    }
    stage_logo_anm = NULL;
    if (msg != NULL)
    {
        delete msg;
        msg = NULL;
    }
    if (!(g_Globals.flags_lo_45c & 9))
    {
        if (msg_file != NULL)
        {
            free(msg_file);
            msg_file = NULL;
        }
        msg_file = NULL;
        g_msg_file_cache = NULL;
    }
    else
    {
        g_msg_file_cache = msg_file;
    }
    if (on_tick != NULL)
    {
        on_tick->flags &= ~UPDATE_FUNC_ACTIVE;
    }
    delete_vm_and_clear(id_c8);
    delete_vm_and_clear(id_cc);
    delete_vm_and_clear(boss_id_d8);
    for (i32 i = 0; i < 9; i++)
    {
        boss_star_ids[i].id = 0;
    }
    id_100.id = 0;
    AnmManager *anm = g_AnmManager;
    for (i32 i = 0; i < 10; i++)
    {
        anm->delete_vm_inline(ids_a0[i]);
        ids_a0[i].id = 0;
    }
    boss_star_count = 0;
    for (i32 i = 0; i < 3; i++)
    {
        boss_bars[i].unk_4c = 0;
    }
    flags_1ac |= 0xe0;
}

// FUNCTION: TH16 0x427970
HARNESS_CALLED void Gui::release_msg()
{
    if (msg != NULL)
    {
        delete msg;
        msg = NULL;
    }
    if (!(g_Globals.flags_lo_45c & 9))
    {
        g_AnmManager->unload_anm(6);
        stage_logo_anm = NULL;
        if (msg_file != NULL)
        {
            free(msg_file);
            msg_file = NULL;
        }
        msg_file = NULL;
    }
}

// TODO: inlined delete_vm loads the child list before storing the flags,
// and some id clears are scheduled after the next push.
// FUNCTION: TH16 0x427a20
Gui::~Gui()
{
    release_stage_files();
    g_UpdateFuncRegistry->unregister_locked(on_tick);
    g_UpdateFuncRegistry->unregister_locked(on_draw_1);
    g_UpdateFuncRegistry->unregister_locked(on_draw_2);
    on_tick = NULL;
    delete_vm_and_clear(id_104);
    delete_vm_and_clear(ids_11c[2]);
    AnmManager *anm = g_AnmManager;
    for (i32 i = 0; i < 10; i++)
    {
        anm->delete_vm_inline(ids_a0[i]);
        ids_a0[i].id = 0;
    }
    anm->delete_vm_inline(id_4c);
    id_4c.id = 0;
    anm->delete_vm_inline(id_50);
    id_50.id = 0;
    g_AnmManager->disable_vms_from_anm_file(front_anm);
    g_Gui = NULL;
}

// FUNCTION: TH16 0x427ca0
Gui *Gui::create()
{
    Gui *gui = new Gui();
    if (gui->initialize() != 0)
    {
        delete gui;
        return NULL;
    }
    return gui;
}

// TODO: the original calls on_tick_body with the stack realigned (push ecx)
// instead of jumping to it; LTCG did that for the real body's sake.
// FUNCTION: TH16 0x429af0
i32 __fastcall Gui::on_tick_callback(Gui *self)
{
    return self->on_tick_body();
}

// FUNCTION: TH16 0x429b00
i32 __fastcall Gui::on_draw_1_callback(Gui *self)
{
    return 1;
}

// FUNCTION: TH16 0x429b10
i32 __fastcall Gui::on_draw_2_callback(Gui *self)
{
    return self->on_draw_2_body();
}

// GLOBAL: TH16 0x492260
// The player's face script in its anm file, per character.
static const i32 g_msg_player_face_scripts[4] = {26, 16, 22, 35};

// Fills a text VM with the text or (blank) clears it, in the current
// side's color.
#define MSG_TEXT_FONT() ((flags >> 1) & 1)
#define MSG_TEXT_COLOR() ((&unk_1a0)[active_side])
// The speech bubble shape: per side, and per bubble type set by
// instruction 29.
#define MSG_TEXTBOX_KIND() (active_side + ((flags >> 2) & 0xf) * 2)

// Dialogue instructions; names from truth's msgmap.
// TODO: ours gets a /GS cookie (from the Float3 locals of instruction 17 and
// the bubble code; still unexplained) and uses ebx; the original keeps the
// instruction pointer in ecx and reloads it after calls.
// FUNCTION: TH16 0x42a1d0
HARNESS_CALLED i32 GuiMsgVm::run()
{
    if (unk_18c > 0)
    {
        unk_18c--;
    }
    if (g_InputState.input_rising & 0x201)
    {
        flags |= 0x40;
    }
    // Skipping: jump to the next instruction's time.
    if ((flags & 0x41) == 0x41 && ((g_InputState.input & (1 << 9)) && (u32)g_InputState.hold_time[9] >= 0x14 ||
                                   (g_InputState.input & (1 << 0)) && (u32)g_InputState.hold_time[0] >= 0x14))
    {
        time_in_script.set(instr()->time);
    }
    while (time_in_script.current >= instr()->time)
    {
        switch (instr()->opcode)
        {
        case 26:
            flags |= 2;
            break;
        // textLine1, textLine2
        case 15:
        {
            AnmVm *vm = get_vm_or_clear(text_line_1);
            const char *text = decode_msg_string(instr()->args.s);
            g_AnmManager->draw_text(vm, MSG_TEXT_COLOR(), 0, MSG_TEXT_FONT() + 4, 0, 0, text);
            AnmManager::interrupt_tree(text_line_1, 2);
            break;
        }
        case 16:
        {
            AnmVm *vm = get_vm_or_clear(text_line_2);
            const char *text = decode_msg_string(instr()->args.s);
            g_AnmManager->draw_text(vm, MSG_TEXT_COLOR(), 0, MSG_TEXT_FONT() + 4, 0, 0, text);
            AnmManager::interrupt_tree(text_line_2, 2);
            break;
        }
        // bubblePos
        case 28:
            unk_1b0 = instr()->args.f[0] * 2.0f;
            unk_1b4 = instr()->args.f[1] * 2.0f;
            break;
        // textAdd: the next line of text, or furigana ("|x,y,text") for the
        // line being written.
        case 17:
            if (next_text_line == 0)
            {
                if (unk_198 == 0)
                {
                    unk_1bc = 0.0f;
                    g_AnmManager->draw_text(text_line_1.find_or_clear(), MSG_TEXT_COLOR(), 0, MSG_TEXT_FONT(), 0, 0,
                                            "  ");
                    g_AnmManager->draw_text(text_line_2.find_or_clear(), MSG_TEXT_COLOR(), 0, MSG_TEXT_FONT(), 0, 0,
                                            "  ");
                    g_AnmManager->draw_text(furigana_1.find_or_clear(), MSG_TEXT_COLOR(), 0, MSG_TEXT_FONT(), 0, 1,
                                            "  ");
                    g_AnmManager->draw_text(furigana_2.find_or_clear(), MSG_TEXT_COLOR(), 0, MSG_TEXT_FONT(), 0, 1,
                                            "  ");
                    unk_198 = 1;
                    AnmManager::interrupt_tree(text_line_1, 3);
                    AnmManager::interrupt_tree(text_line_2, 3);
                    AnmManager::interrupt_tree(furigana_1, 3);
                    AnmManager::interrupt_tree(furigana_2, 3);
                }
                const char *text = decode_msg_string(instr()->args.s);
                if (text[0] == '|')
                {
                    i32 x = atoi(text + 1);
                    const char *rest = strchr(text + 1, ',') + 1;
                    i32 y = atoi(rest);
                    rest = strchr(rest, ',');
                    furigana_1.find_or_clear()->flags_hi |= 0x1000;
                    g_AnmManager->draw_text(furigana_1.find_or_clear(), 0, 0xa0a0a0, 2, x, y, rest + 1);
                    furigana_1.set_entity_pos((Float3 *)&unk_1b0);
                    AnmManager::interrupt_tree_and_run(furigana_1, 2);
                }
                else
                {
                    f32 width = (strlen(text) / 2 * 16 - 28) * 2.0f;
                    unk_1bc = width > unk_1bc ? width : unk_1bc;
                    set_textbox(unk_1b0, unk_1b4, unk_1bc, MSG_TEXTBOX_KIND());
                    set_textbox_width(unk_1bc, MSG_TEXTBOX_KIND());
                    g_AnmManager->draw_text(text_line_1.find_or_clear(), MSG_TEXT_COLOR(), 0, MSG_TEXT_FONT(), 0, 0,
                                            text);
                    if (active_side >= 1)
                    {
                        Float3 pos = *(Float3 *)&unk_1b0;
                        text_line_1.set_entity_pos(&pos);
                        furigana_1.set_entity_pos(&pos);
                        text_line_2.set_entity_pos(&pos);
                        furigana_2.set_entity_pos(&pos);
                    }
                    else
                    {
                        Float3 pos = *(Float3 *)&unk_1b0;
                        text_line_1.set_entity_pos(&pos);
                    }
                    AnmManager::interrupt_tree_and_run(text_line_1, 2);
                    next_text_line++;
                }
            }
            else
            {
                const char *text = decode_msg_string(instr()->args.s);
                if (text[0] == '|')
                {
                    i32 x = atoi(text + 1);
                    const char *rest = strchr(text + 1, ',') + 1;
                    i32 y = atoi(rest);
                    rest = strchr(rest, ',');
                    furigana_2.find_or_clear()->flags_hi |= 0x1000;
                    g_AnmManager->draw_text(furigana_2.find_or_clear(), 0, 0xa0a0a0, 2, x, y, rest + 1);
                    furigana_2.set_entity_pos((Float3 *)&unk_1b0);
                    AnmManager::interrupt_tree(furigana_2, 2);
                }
                else
                {
                    f32 width = (strlen(text) / 2 * 16 - 28) * 2.0f;
                    unk_1bc = width > unk_1bc ? width : unk_1bc;
                    set_textbox(unk_1b0, unk_1b4, unk_1bc, MSG_TEXTBOX_KIND() + 8);
                    set_textbox_width(unk_1bc, MSG_TEXTBOX_KIND() + 8);
                    g_AnmManager->draw_text(text_line_2.find_or_clear(), MSG_TEXT_COLOR(), 0, MSG_TEXT_FONT(), 0, 0,
                                            text);
                    if (active_side >= 1)
                    {
                        Float3 pos = *(Float3 *)&unk_1b0;
                        text_line_1.set_entity_pos(&pos);
                        furigana_1.set_entity_pos(&pos);
                        text_line_2.set_entity_pos(&pos);
                        furigana_2.set_entity_pos(&pos);
                    }
                    else
                    {
                        Float3 pos = *(Float3 *)&unk_1b0;
                        text_line_2.set_entity_pos(&pos);
                    }
                    AnmManager::interrupt_tree_and_run(text_line_2, 2);
                    next_text_line = 0;
                    unk_198 = 0;
                }
            }
            break;
        // textClear
        case 18:
            delete_vm_and_clear(textbox);
            AnmManager::interrupt_tree(text_line_1, 3);
            AnmManager::interrupt_tree(text_line_2, 3);
            AnmManager::interrupt_tree(furigana_1, 3);
            AnmManager::interrupt_tree(furigana_2, 3);
            break;
        // playerShow
        case 1:
            if (instr()->args.i[0] == 0)
            {
                player_face = g_Player->anm_file->create_effect(g_msg_player_face_scripts[g_Globals.character], -1, NULL);
            }
            else
            {
                player_face = g_EnemyManager->anim_statement_anms[5]->create_effect(0xb, -1, NULL);
            }
            break;
        // bossShow
        case 2:
        {
            i32 i = instr()->args.i[0];
            StageBoss *boss = &g_stage_data->bosses[i];
            enemy_faces[i] =
                g_EnemyManager->anim_statement_anms[boss->face_anm_slot]->create_effect(boss->face_script, -1, NULL);
            unk_1c0 = 0;
            break;
        }
        case 31:
        {
            StageBoss *boss = &g_stage_data->bosses[1];
            enemy_faces[1] =
                g_EnemyManager->anim_statement_anms[boss->face_anm_slot]->create_effect(boss->face_script, -1, NULL);
            unk_1c0 = 0;
            break;
        }
        // textOffsetY
        case 25:
            get_vm_or_clear(text_line_1)->pos_2.y = instr()->args.i[0];
            get_vm_or_clear(text_line_2)->pos_2.y = instr()->args.i[0];
            get_vm_or_clear(furigana_1)->pos_2.y = instr()->args.i[0];
            get_vm_or_clear(furigana_2)->pos_2.y = instr()->args.i[0];
            break;
        // playerShake, bossShake
        case 23:
            AnmManager::interrupt_tree(player_face, 7);
            break;
        case 24:
            AnmManager::interrupt_tree(enemy_faces[0], 7);
            AnmManager::interrupt_tree(enemy_faces[1], 7);
            break;
        // playerHide, bossHide, textboxHide
        case 4:
            AnmManager::interrupt_tree(player_face, 1);
            player_face.id = 0;
            break;
        case 5:
            AnmManager::interrupt_tree(enemy_faces[instr()->args.i[0]], 1);
            enemy_faces[instr()->args.i[0]].id = 0;
            AnmManager::interrupt_tree(intro, 1);
            break;
        case 6:
            AnmManager::interrupt_tree(text_line_1, 1);
            AnmManager::interrupt_tree(text_line_2, 1);
            AnmManager::interrupt_tree(furigana_1, 1);
            AnmManager::interrupt_tree(furigana_2, 1);
            delete_vm_and_clear(textbox);
            break;
        // portraitDarken, portraitHighlight
        case 33:
            if (instr()->args.i[0] == 0)
            {
                AnmManager::interrupt_tree_and_run(player_face, 3);
            }
            else
            {
                AnmManager::interrupt_tree_and_run(enemy_faces[instr()->args.i[1]], 3);
            }
            break;
        case 34:
            if (instr()->args.i[0] == 0)
            {
                AnmManager::interrupt_tree_and_run(player_face, 2);
            }
            else
            {
                AnmManager::interrupt_tree_and_run(enemy_faces[instr()->args.i[1]], 2);
            }
            break;
        // speakerPlayer
        case 7:
            for (i32 i = 0; i < 4; i++)
            {
                AnmManager::interrupt_tree_and_run(enemy_faces[i], 3);
            }
            AnmManager::interrupt_tree_and_run(player_face, 2);
            AnmManager::interrupt_tree(id_54, 2);
            active_side = 0;
            get_vm_or_clear(text_line_1)->pos_2.y = 0.0f;
            get_vm_or_clear(text_line_2)->pos_2.y = 0.0f;
            get_vm_or_clear(furigana_1)->pos_2.y = 0.0f;
            get_vm_or_clear(furigana_2)->pos_2.y = 0.0f;
            flags &= ~2;
            next_text_line = 0;
            unk_198 = 0;
            break;
        // speakerBoss
        case 8:
            AnmManager::interrupt_tree_and_run(player_face, 3);
            for (i32 i = 0; i < 4; i++)
            {
                if (i == instr()->args.i[0])
                {
                    AnmManager::interrupt_tree_and_run(enemy_faces[instr()->args.i[0]], 2);
                }
                else
                {
                    AnmManager::interrupt_tree_and_run(enemy_faces[i], 3);
                }
            }
            AnmManager::interrupt_tree(id_54, 3);
            active_side = 1;
            get_vm_or_clear(text_line_1)->pos_2.y = 0.0f;
            get_vm_or_clear(text_line_2)->pos_2.y = 0.0f;
            get_vm_or_clear(furigana_1)->pos_2.y = 0.0f;
            get_vm_or_clear(furigana_2)->pos_2.y = 0.0f;
            flags &= ~2;
            next_text_line = 0;
            unk_198 = 0;
            break;
        case 32:
            AnmManager::interrupt_tree(id_54, 3);
            active_side = instr()->args.i[0];
            get_vm_or_clear(text_line_1)->pos_2.y = 0.0f;
            get_vm_or_clear(text_line_2)->pos_2.y = 0.0f;
            get_vm_or_clear(furigana_1)->pos_2.y = 0.0f;
            get_vm_or_clear(furigana_2)->pos_2.y = 0.0f;
            flags &= ~2;
            next_text_line = 0;
            unk_198 = 0;
            break;
        // speakerNone
        case 9:
        {
            AnmManager::interrupt_tree_and_run(player_face, 3);
            for (i32 i = 0; i < 4; i++)
            {
                AnmManager::interrupt_tree_and_run(enemy_faces[i], 3);
            }
            AnmManager::interrupt_tree(id_54, 3);
            active_side = 0;
            AnmVm *vm = g_AnmManager->get_vm_with_id(text_line_1);
            if (vm != NULL)
            {
                vm->entity_pos = (&vec_15c)[active_side];
            }
            vm = g_AnmManager->get_vm_with_id(text_line_2);
            if (vm != NULL)
            {
                vm->entity_pos = (&vec_15c)[active_side];
            }
            get_vm_or_clear(text_line_1)->pos_2.y = 0.0f;
            get_vm_or_clear(text_line_2)->pos_2.y = 0.0f;
            vm = g_AnmManager->get_vm_with_id(furigana_1);
            if (vm != NULL)
            {
                vm->entity_pos = (&vec_15c)[active_side];
            }
            vm = g_AnmManager->get_vm_with_id(furigana_2);
            if (vm != NULL)
            {
                vm->entity_pos = (&vec_15c)[active_side];
            }
            get_vm_or_clear(furigana_1)->pos_2.y = 0.0f;
            get_vm_or_clear(furigana_2)->pos_2.y = 0.0f;
            flags &= ~2;
            next_text_line = 0;
            unk_198 = 0;
            break;
        }
        // skippable: bit 0 of flags.
        case 10:
            ((GuiMsgVmFlags *)&flags)->skippable = instr()->args.s[0];
            break;
        // playerFace, bossFace
        case 13:
            AnmManager::interrupt_tree_and_run(player_face, instr()->args.i[0] + 0x11);
            break;
        case 14:
            AnmManager::interrupt_tree_and_run(enemy_faces[instr()->args.i[1]], instr()->args.i[0] + 0x11);
            break;
        // textPause: wait for the given time or a key.
        case 11:
            if (pause_timer.current <= 0)
            {
                pause_timer.set_value(instr()->args.i[0]);
            }
            pause_timer--;
            if (!(g_InputState.input_rising & 0x80001) && pause_timer.current > 0)
            {
                if ((flags & 0x41) != 0x41 ||
                    ((u32)g_InputState.get_hold_time(9) < 0x14 && (u32)g_InputState.get_hold_time(0) < 0x14))
                {
                    goto waiting;
                }
            }
            else
            {
                g_SoundManager.play_sound_centered(0, 0);
            }
            pause_timer.set_value(0);
            next_text_line = 0;
            unk_198 = 0;
            break;
        // eclResume
        case 12:
            unk_18c = 1;
            break;
        // musicBoss
        case 19:
            g_Supervisor.play_bgm(1, g_stage_data->music_ids[1]);
            g_Gui->stage_logo_anm->create_effect(2, -1, NULL);
            break;
        // intro
        case 20:
        {
            StageBoss *boss = &g_stage_data->bosses[instr()->args.i[0]];
            intro = g_EnemyManager->anim_statement_anms[boss->intro_anm_slot]->create_effect(boss->intro_script, -1,
                                                                                            NULL);
            g_Gui->show_boss_marker();
            break;
        }
        // stageEnd
        case 21:
            stage_clear_42e150();
            break;
        // musicEnd
        case 22:
            if (g_Globals.stage_num == 6)
            {
                g_Supervisor.fade_out_bgm(2.0f);
            }
            else
            {
                g_Supervisor.fade_out_bgm(8.0f);
            }
            break;
        // musicFade
        case 27:
            g_Supervisor.fade_out_bgm(instr()->args.f[0]);
            break;
        // bubbleType: bits 2-5 of flags.
        case 29:
            ((GuiMsgVmFlags *)&flags)->textbox_type = instr()->args.i[0];
            break;
        // lightsOut
        case 35:
            Gui::create_vm_110();
            break;
        case 3:
        case 30:
            break;
        // end
        case 0:
            return -1;
        }
        current_instr = (u8 *)current_instr + instr()->args_size + 4;
    }
    time_in_script.tick();
waiting:
    // Keep the text next to the speech bubble.
    i32 script = textbox_kind + 0xb4;
    if (get_vm_or_clear(textbox) == NULL)
    {
        return 0;
    }
    AnmVm *bubble = get_vm_or_clear(textbox)->search_children(script, 0);
    if (bubble == NULL)
    {
        return 0;
    }
    Float3 pos;
    pos = bubble->pos + bubble->entity_pos + bubble->pos_2;
    bubble->transform_coords(&pos);
    f32 scale = 2.0f / g_screen_coord_scale;
    pos.y *= scale;
    pos.x *= scale;
    if (active_side >= 1)
    {
        if (bubble->scale.x < 1.0f)
        {
            pos.x += (bubble->scale.x + 0.125f) * 32.0f - 6.0f;
        }
        else
        {
            pos.x += 26.0f;
        }
    }
    else
    {
        pos.x -= 36.0f;
    }
    AnmManager *anm = g_AnmManager;
    AnmVm *vm = anm->get_vm_with_id(text_line_1);
    if (vm != NULL)
    {
        vm->entity_pos = pos;
    }
    vm = anm->get_vm_with_id(furigana_1);
    if (vm != NULL)
    {
        vm->entity_pos = pos;
    }
    vm = anm->get_vm_with_id(text_line_2);
    if (vm != NULL)
    {
        vm->entity_pos = pos;
    }
    vm = anm->get_vm_with_id(furigana_2);
    if (vm != NULL)
    {
        vm->entity_pos = pos;
    }
    return 0;
}

// TODO: the original aligns its frame to 8 bytes and adds two of the
// vector components the other way round.
// FUNCTION: TH16 0x42b480
void GuiMsgVm::update_callout(AnmVm *vm)
{
    i32 script = textbox_kind + 0xb4;
    if (get_vm_or_clear(textbox) == NULL)
    {
        return;
    }
    AnmVm *bubble = get_vm_or_clear(textbox)->search_children(script, 0);
    if (bubble == NULL)
    {
        return;
    }
    Float3 pos;
    pos = bubble->pos + bubble->entity_pos + bubble->pos_2;
    bubble->transform_coords(&pos);
    f32 scale = 2.0f / g_screen_coord_scale;
    pos.x *= scale;
    pos.y *= scale;
    if (active_side >= 1)
    {
        if (bubble->scale.x < 1.0f)
        {
            pos.x += bubble->scale.x * 32.0f - 6.0f;
        }
        else
        {
            pos.x += 26.0f;
        }
    }
    else
    {
        pos.x -= 36.0f;
    }
    vm->entity_pos = pos;
}

// FUNCTION: TH16 0x42b5f0
i32 __fastcall Gui::textbox_on_draw(AnmVm *vm)
{
    g_Gui->msg->update_callout(vm);
    return 0;
}

// FUNCTION: TH16 0x42b610
void GuiMsgVm::hide()
{
    AnmManager *anm = g_AnmManager;
    AnmVm *vm = anm->get_vm_with_id(player_face);
    if (vm != NULL)
    {
        vm->clear_flag_lo_2_tree_inline();
    }
    for (i32 i = 0; i < 4; i++)
    {
        vm = anm->get_vm_with_id(enemy_faces[i]);
        if (vm != NULL)
        {
            vm->clear_flag_lo_2_tree_inline();
        }
    }
    vm = anm->get_vm_with_id(id_54);
    if (vm != NULL)
    {
        vm->clear_flag_lo_2_tree_inline();
    }
    vm = anm->get_vm_with_id(text_line_1);
    if (vm != NULL)
    {
        vm->clear_flag_lo_2_tree_inline();
    }
    vm = anm->get_vm_with_id(text_line_2);
    if (vm != NULL)
    {
        vm->clear_flag_lo_2_tree_inline();
    }
    vm = anm->get_vm_with_id(furigana_1);
    if (vm != NULL)
    {
        vm->clear_flag_lo_2_tree_inline();
    }
    vm = anm->get_vm_with_id(furigana_2);
    if (vm != NULL)
    {
        vm->clear_flag_lo_2_tree_inline();
    }
    vm = anm->get_vm_with_id(intro);
    if (vm != NULL)
    {
        vm->clear_flag_lo_2_tree_inline();
    }
    vm = anm->get_vm_with_id(id_70);
    if (vm != NULL)
    {
        vm->clear_flag_lo_2_tree_inline();
    }
    vm = anm->get_vm_with_id(textbox);
    if (vm != NULL)
    {
        vm->clear_flag_lo_2_tree_inline();
    }
}

// FUNCTION: TH16 0x42b820
void GuiMsgVm::show()
{
    AnmManager *anm = g_AnmManager;
    AnmVm *vm = anm->get_vm_with_id(player_face);
    if (vm != NULL)
    {
        vm->set_flag_lo_2_tree_inline();
    }
    for (i32 i = 0; i < 4; i++)
    {
        vm = anm->get_vm_with_id(enemy_faces[i]);
        if (vm != NULL)
        {
            vm->set_flag_lo_2_tree_inline();
        }
    }
    vm = anm->get_vm_with_id(id_54);
    if (vm != NULL)
    {
        vm->set_flag_lo_2_tree_inline();
    }
    vm = anm->get_vm_with_id(text_line_1);
    if (vm != NULL)
    {
        vm->set_flag_lo_2_tree_inline();
    }
    vm = anm->get_vm_with_id(text_line_2);
    if (vm != NULL)
    {
        vm->set_flag_lo_2_tree_inline();
    }
    vm = anm->get_vm_with_id(furigana_1);
    if (vm != NULL)
    {
        vm->set_flag_lo_2_tree_inline();
    }
    vm = anm->get_vm_with_id(furigana_2);
    if (vm != NULL)
    {
        vm->set_flag_lo_2_tree_inline();
    }
    vm = anm->get_vm_with_id(intro);
    if (vm != NULL)
    {
        vm->set_flag_lo_2_tree_inline();
    }
    vm = anm->get_vm_with_id(id_70);
    if (vm != NULL)
    {
        vm->set_flag_lo_2_tree_inline();
    }
    vm = anm->get_vm_with_id(textbox);
    if (vm != NULL)
    {
        vm->set_flag_lo_2_tree_inline();
    }
}

// TODO: the original keeps g_AnmManager in ebx across the lookups; LTCG
// knows get_vm_with_id and search_children leave it alone.
// FUNCTION: TH16 0x42ba30
HARNESS_CALLED void GuiMsgVm::set_textbox(f32 x, f32 y, f32 width, i32 kind)
{
    delete_vm_and_clear(textbox);
    Float3 pos(x, y, 0.0f);
    textbox = g_Gui->front_anm->create_vm(kind + 0xe4, &pos, 0.0f, -1, 0);
    find_child_of(textbox, kind + 0xb4)->float_vars[0] = width;
    find_child_of(textbox, kind + 0xd4)->float_vars[0] = width;
    textbox_kind = kind;
}

// TODO: the original keeps g_AnmManager in edi and the width in xmm1
// across the lookups (LTCG knows the callees leave them alone).
// FUNCTION: TH16 0x42bb30
HARNESS_CALLED void GuiMsgVm::set_textbox_width(f32 width, i32 kind)
{
    width += 16.0f;
    find_child_of(textbox, kind + 0xb4)->float_vars[0] = width;
    find_child_of(textbox, kind + 0xd4)->float_vars[0] = width;
}

// TODO: the original aligns its frame to 8 bytes, which LTCG adds for
// Gui::sub_42bcf0's sake.
// FUNCTION: TH16 0x42bc10
void Gui::update_score()
{
    Gui *gui = g_Gui;
    if (g_Globals.score != gui->current_score)
    {
        u32 step = (g_Globals.score - gui->current_score) >> 5;
        if (step >= 0x8d55e)
        {
            step = 0x8d55e;
        }
        else if (step == 0)
        {
            step = 1;
        }
        if (gui->score_step < (i32)step)
        {
            gui->score_step = step;
        }
        if (gui->score_step > (i32)(g_Globals.score - gui->current_score))
        {
            gui->score_step = g_Globals.score - gui->current_score;
        }
        gui->current_score += gui->score_step;
        if (gui->current_score >= (i32)g_Globals.score)
        {
            gui->score_step = 0;
        }
    }
    if (g_Globals.hiscore < gui->current_score)
    {
        g_Globals.hiscore = gui->current_score;
        g_Globals.hiscore_continues = g_Globals.continues_used;
        g_Globals.flags_lo_45c |= 4;
        if (!(g_Globals.flags_lo_45c & 4))
        {
            gui->sub_42bcf0(0, 3);
        }
    }
}

// Shows a HUD notice: kind 0 is the spell card bonus (unk is the amount),
// 1 bonus failed, 2 full power, 3 hiscore, 4 extend. Every caller passes
// 0-4, which is how the original's jump table goes without a bounds check;
// the __assume reproduces that.
// TODO: in the digit loop the original keeps the manager in ebx and spills the counter; ours does the reverse.
// FUNCTION: TH16 0x42bcf0
HARNESS_CALLED void Gui::sub_42bcf0(i32 unk, i32 kind)
{
    switch (kind)
    {
    case 0:
    {
        delete_vm_and_clear(id_c8);
        id_c8 = front_anm->create_effect(0x3d, -1, NULL);
        i32 divisor = 10000000;
        i32 rest = unk;
        i32 shown = 0;
        AnmManager *anm;
        AnmVm *vm;
        for (i32 i = 0; i < 8; i++)
        {
            delete_vm_and_clear(ids_a0[i]);
            ids_a0[i] = g_AsciiManager->ascii_anm->create_effect(i + 4, -1, NULL);
            anm = g_AnmManager;
            i32 digit = rest / divisor;
            rest = rest % divisor;
            if (digit != 0)
            {
                shown = 1;
            }
            vm = anm->get_vm_with_id(ids_a0[i]);
            if (vm != NULL)
            {
                anm->loaded_anms[vm->anm_loaded_index]->set_sprite(vm, digit + 0xef);
            }
            if (!shown)
            {
                vm = anm->get_vm_with_id(ids_a0[i]);
                if (vm != NULL)
                {
                    vm->clear_flag_lo_2_tree_inline();
                }
            }
            else
            {
                vm = anm->get_vm_with_id(ids_a0[i]);
                if (vm != NULL)
                {
                    vm->set_flag_lo_2_tree_inline();
                }
            }
            divisor /= 10;
        }
        delete_vm_and_clear(ids_a0[8]);
        if (unk >= 1000000)
        {
            ids_a0[8] = g_AsciiManager->ascii_anm->create_effect(0xc, -1, NULL);
            anm = g_AnmManager;
            vm = anm->get_vm_with_id(ids_a0[8]);
            if (vm != NULL)
            {
                anm->loaded_anms[vm->anm_loaded_index]->set_sprite(vm, 0xfd);
            }
        }
        delete_vm_and_clear(ids_a0[9]);
        if (unk >= 1000)
        {
            ids_a0[9] = g_AsciiManager->ascii_anm->create_effect(0xd, -1, NULL);
            anm = g_AnmManager;
            vm = anm->get_vm_with_id(ids_a0[9]);
            if (vm != NULL)
            {
                anm->loaded_anms[vm->anm_loaded_index]->set_sprite(vm, 0xfd);
            }
        }
        unk_14c = 1;
        ids_11c[3] = front_anm->create_effect(0x60, -1, NULL);
        break;
    }
    case 1:
        delete_vm_and_clear(id_c8);
        id_c8 = front_anm->create_effect(0x3e, -1, NULL);
        unk_14c = 1;
        ids_11c[3] = front_anm->create_effect(0x60, -1, NULL);
        break;
    case 2:
        delete_vm_and_clear(id_cc);
        id_cc = front_anm->create_effect(0x3f, -1, NULL);
        break;
    case 3:
        delete_vm_and_clear(id_cc);
        id_cc = front_anm->create_effect(0x40, -1, NULL);
        break;
    case 4:
        delete_vm_and_clear(id_cc);
        id_cc = front_anm->create_effect(0x41, -1, NULL);
        break;
    case 6:
        delete_vm_and_clear(id_c8);
        id_c8 = front_anm->create_effect(0x42, -1, NULL);
        break;
    default:
        __assume(0);
    }
}

// AnmLoaded::create_vm as LTCG inlined it into some callers.
static __forceinline AnmId create_vm_inline(AnmLoaded *anm, i32 script, D3DXVECTOR3 *pos, f32 rotation, i32 layer)
{
    ENTER_CS(CS_ANM_MANAGER);
    anm->vm_count++;
    AnmVm *vm = g_AnmManager->allocate_vm();
    anm->copy_vm(vm, script);
    vm->flags_hi |= ANM_VM_CREATED_BY_GAME;
    if (layer >= 0)
    {
        vm->layer = layer;
        if (layer <= 23)
        {
            vm->flags_hi &= ~ANM_VM_LAYER_UI;
            vm->flags_hi |= ANM_VM_LAYER_SET;
        }
    }
    if (pos == NULL)
    {
        vm->entity_pos = D3DXVECTOR3(0.0f, 0.0f, 0.0f);
    }
    else
    {
        vm->entity_pos = *pos;
    }
    vm->rotation.z = rotation;
    vm->run();
    vm->mode_of_create_child = 0;
    AnmId id;
    id = g_AnmManager->insert_in_world_list_back(vm);
    LEAVE_CS(CS_ANM_MANAGER);
    return id;
}

// FUNCTION: TH16 0x42c070
void Gui::show_stage_clear_bonus()
{
    Gui *gui = g_Gui;
    gui->ids_11c[1] = create_vm_inline(gui->front_anm, 0x78, NULL, 0.0f, -1);
    gui->stage_clear_bonus = g_Globals.stage_num * 1000000;
    g_Globals.add_to_score(gui->stage_clear_bonus);
    gui->flags_1ac |= 0x100;
    gui->timer_1b0.reset();
}

// FUNCTION: TH16 0x42c1b0
HARNESS_CALLED void Gui::sub_42c1b0()
{
    AnmManager::interrupt_tree(ids_11c[1], 1);
    AnmManager::interrupt_tree(ids_11c[2], 1);
    flags_1ac &= ~0x100;
    timer_1b0.reset();
}

// FUNCTION: TH16 0x42c240
void show_stage_logo()
{
    if (g_Supervisor.gamemode_to_switch_to != 8 && !(g_Globals.flags_hi_45c & 1))
    {
        g_Gui->stage_logo_anm->create_effect(0, -1, NULL);
    }
}

// FUNCTION: TH16 0x42c280
void Gui::update_lives(i32 lives, u32 fragments)
{
    if (life_counter_vms[0] == NULL)
    {
        return;
    }
    i32 i;
    for (i = 0; i < lives; i++)
    {
        life_counter_vms[i]->interrupt(2);
    }
    if (i < sizeof(life_counter_vms) / sizeof(life_counter_vms[0]))
    {
        i16 progress = fragments * 5 / get_score_extend_quota();
        life_counter_vms[i]->interrupt(progress + 7);
        for (i++; i < sizeof(life_counter_vms) / sizeof(life_counter_vms[0]); i++)
        {
            life_counter_vms[i]->interrupt(3);
        }
    }
}

// FUNCTION: TH16 0x42c390
void Gui::update_bombs(i32 bombs, i32 fragments)
{
    if (bomb_counter_vms[0] == NULL)
    {
        return;
    }
    i32 i;
    for (i = 0; i < bombs; i++)
    {
        bomb_counter_vms[i]->interrupt(2);
    }
    if (i < sizeof(bomb_counter_vms) / sizeof(bomb_counter_vms[0]))
    {
        bomb_counter_vms[i]->interrupt((i16)fragments + 7);
        for (i++; i < sizeof(bomb_counter_vms) / sizeof(bomb_counter_vms[0]); i++)
        {
            bomb_counter_vms[i]->interrupt(3);
        }
    }
}

// TODO: the original tests the script again after adding 0xa4 instead of
// using the add's flags.
// FUNCTION: TH16 0x42c480
void Gui::show_boss_marker()
{
    if (get_vm_or_clear(boss_id_d8) != NULL)
    {
        return;
    }
    i32 script;
    if (g_Globals.chapter >= 0x29)
    {
        script = g_stage_data->bosses[0].marker_script;
    }
    else
    {
        script = g_stage_data->bosses[1].marker_script;
    }
    if (script >= 0)
    {
        script += 0xa4;
        if (script >= 0)
        {
            boss_id_d8 = front_anm->create_effect(script, -1, NULL);
        }
    }
}

// FUNCTION: TH16 0x42c4f0
void Gui::sub_42c4f0()
{
    AnmManager::interrupt_tree(ids_11c[4], 1);
    flags_1ac = flags_1ac & ~0x1000 | 0x800;
    timer_1b0.reset();
}

// FUNCTION: TH16 0x42c580
void Gui::sub_42c580()
{
    AnmVm *vm = g_AnmManager->get_vm_with_id(g_Gui->ids_11c[4]);
    if (vm != NULL)
    {
        vm->clear_flag_lo_2_tree_inline();
    }
}

// FUNCTION: TH16 0x42c5c0
void Gui::sub_42c5c0()
{
    AnmVm *vm = g_AnmManager->get_vm_with_id(g_Gui->ids_11c[4]);
    if (vm != NULL)
    {
        vm->set_flag_lo_2_tree_inline();
    }
}

// The id of the first descendant of the VM running the script (0 if there
// is none, forgetting the id if the VM is gone). LTCG knows get_vm_with_id
// leaves g_AnmManager alone and loads it once; with the opaque stub we have
// to pass it in.
static inline AnmId find_child_id_of(AnmManager *anm, AnmId &id, i32 script)
{
    AnmVm *child = NULL;
    AnmVm *vm = anm->get_vm_with_id(id);
    if (vm == NULL)
    {
        id.id = 0;
    }
    else
    {
        vm = anm->get_vm_with_id(id);
        if (vm == NULL)
        {
            id.id = 0;
        }
        child = vm->search_children(script, 0);
    }
    AnmId result;
    result.id = child != NULL ? child->id.id : 0;
    return result;
}

// TODO: the original keeps g_AnmManager and then the level in ebx; ours spills both (get_vm_with_id is an opaque stub here).
// FUNCTION: TH16 0x42c600
void Gui::update_season_gauge()
{
    AnmManager *anm = g_AnmManager;
    Gui *gui = g_Gui;
    AnmVm *gauge = anm->get_vm_with_id(find_child_id_of(anm, gui->season_gauge_id, 0x73));
    AnmVm *level_vm = anm->get_vm_with_id(find_child_id_of(anm, gui->season_gauge_id, 0x74));
    i32 level = g_Globals.season_level();
    if (level == 0)
    {
        gauge->sprite_size.x = get_season_gauge_fill_ratio() * 100.0f;
        gauge->flags_lo |= ANM_VM_SCALE_CHANGED;
        level_vm->clear_flag_lo_2_tree_inline();
        if (gui->season_gauge_has_level == 1)
        {
            gauge->interrupt(3);
            gauge->run();
        }
        gui->season_gauge_has_level = 0;
    }
    else
    {
        if (g_Globals.season_power < g_Globals.max_season_power)
        {
            gauge->sprite_size.x = get_season_gauge_fill_ratio() * 100.0f;
        }
        else
        {
            gauge->sprite_size.x = 100.0f;
        }
        gauge->flags_lo |= ANM_VM_SCALE_CHANGED;
        level_vm->set_flag_lo_2_tree_inline();
        level_vm->interrupt(level + 7);
        if (gui->season_gauge_has_level == 0)
        {
            gauge->interrupt(2);
            gauge->run();
        }
        gui->season_gauge_has_level = 1;
    }
}

// FUNCTION: TH16 0x42c890
void __fastcall anm_vm_interrupt_4_run(AnmVm *vm)
{
    vm->interrupt(4);
    vm->run();
}

// FUNCTION: TH16 0x42c8c0
void __fastcall anm_vm_interrupt_4(AnmVm *vm)
{
    vm->interrupt(4);
}

// FUNCTION: TH16 0x42c8f0
void __fastcall anm_vm_interrupt_5(AnmVm *vm)
{
    vm->interrupt(5);
}

// FUNCTION: TH16 0x4175d0
HARNESS_CALLED void Gui::interrupt_spell_vms_2()
{
    AnmVm *vm = vm_94;
    vm->interrupt(2);
    vm->run();
    vm = vm_98;
    vm->interrupt(2);
    vm->run();
}

// FUNCTION: TH16 0x417650
HARNESS_CALLED void Gui::interrupt_spell_vms_3()
{
    AnmVm *vm = vm_94;
    vm->interrupt(3);
    vm->run();
    vm = vm_98;
    vm->interrupt(3);
    vm->run();
}

// FUNCTION: TH16 0x4173c0
void __fastcall anm_vm_interrupt_2(AnmVm *vm)
{
    vm->interrupt(2);
}

// TODO: same frame difference as create_vm (4 more bytes, esi saved
// before the critical section).
// FUNCTION: TH16 0x42c920
AnmId AnmLoaded::create_ui_effect(i32 script, i32 unused, AnmVm **out)
{
    ENTER_CS(CS_ANM_MANAGER);
    vm_count++;
    AnmVm *vm = g_AnmManager->allocate_vm();
    if (out != NULL)
    {
        *out = vm;
    }
    copy_vm(vm, script);
    vm->flags_hi |= ANM_VM_CREATED_BY_GAME;
    vm->entity_pos = D3DXVECTOR3(0.0f, 0.0f, 0.0f);
    vm->rotation.z = 0.0f;
    vm->run();
    vm->mode_of_create_child = 4;
    AnmId id;
    id = g_AnmManager->insert_in_ui_list_back(vm);
    vm->flags_hi &= ~(ANM_VM_FLAG_HI_4000 | ANM_VM_FLAG_HI_8000);
    LEAVE_CS(CS_ANM_MANAGER);
    return id;
}

// FUNCTION: TH16 0x418440
void __fastcall anm_vm_interrupt_3_run(AnmVm *vm)
{
    vm->interrupt(3);
    vm->run();
}

// FUNCTION: TH16 0x418470
void __fastcall anm_vm_interrupt_2_run(AnmVm *vm)
{
    vm->interrupt(2);
    vm->run();
}

// TODO: the original keeps g_AnmManager in edi across the lookups (LTCG
// knows get_vm_with_id leaves it alone).
// FUNCTION: TH16 0x429b20
GuiMsgVm::GuiMsgVm(void *script)
{
    memset(this, 0, sizeof(GuiMsgVm));
    timer_4.reset();
    time_in_script.reset();
    unk_154 = 0;
    pause_timer.reset();
    current_instr = script;
    text_line_1 = g_Supervisor.text_anm->create_effect(0, -1, NULL);
    text_line_2 = g_Supervisor.text_anm->create_effect(0, -1, NULL);
    AnmManager::interrupt_tree_and_run(text_line_2, 7);
    get_vm_or_clear(text_line_1)->font_dims[0] = 0x15;
    get_vm_or_clear(text_line_1)->font_dims[1] = 0x15;
    get_vm_or_clear(text_line_2)->font_dims[0] = 0x15;
    get_vm_or_clear(text_line_2)->font_dims[1] = 0x15;
    furigana_1 = g_Supervisor.text_anm->create_effect(1, -1, NULL);
    furigana_2 = g_Supervisor.text_anm->create_effect(1, -1, NULL);
    AnmManager::interrupt_tree_and_run(furigana_2, 7);
    get_vm_or_clear(furigana_1)->font_dims[0] = 0x15;
    get_vm_or_clear(furigana_1)->font_dims[1] = 0x15;
    get_vm_or_clear(furigana_2)->font_dims[0] = 0x15;
    get_vm_or_clear(furigana_2)->font_dims[1] = 0x15;
    get_vm_or_clear(text_line_1)->flags_hi |= 0x1000;
    get_vm_or_clear(text_line_2)->flags_hi |= 0x1000;
    get_vm_or_clear(furigana_1)->flags_hi |= 0x1000;
    get_vm_or_clear(furigana_2)->flags_hi |= 0x1000;
    get_vm_or_clear(text_line_1)->index_of_on_draw = 5;
    get_vm_or_clear(text_line_2)->index_of_on_draw = 5;
    get_vm_or_clear(furigana_1)->index_of_on_draw = 5;
    get_vm_or_clear(furigana_2)->index_of_on_draw = 5;
    next_text_line = 0;
    unk_198 = 0;
    unk_1a0 = 0;
    unk_1a4 = 0;
    unk_1a8 = 0;
    unk_1ac = 0;
    active_side = 0;
    vec_15c = Float3(16.0f, 0.0f, 0.0f);
    vec_168 = Float3(16.0f, 0.0f, 0.0f);
    vec_174 = Float3(16.0f, 0.0f, 0.0f);
    vec_180 = Float3(16.0f, 0.0f, 0.0f);
    g_BulletManager->clear_all(0);
    // LaserManager::clear_all(0, 0), inlined.
    LaserDataInf *laser = g_LaserManager->list_head.next;
    while (laser != NULL)
    {
        LaserDataInf *next = laser->next;
        if (laser->state != 1)
        {
            laser->cancel(0, 0);
        }
        laser = next;
    }
    EnemyManager::kill_all();
    flags &= ~0x40;
    unk_1b0 = 384.0f;
    unk_1b4 = 640.0f;
    unk_1b8 = 0.0f;
    unk_1bc = 320.0f;
}

// TODO: the original stores ".wav" as an immediate (see play_bgm_wav).
// FUNCTION: TH16 0x429ff0
void Gui::start_dialogue(i32 script)
{
    __asm finit;
    if (script == -1 || script == -3)
    {
        i32 boss = script == -1;
        StageData *stage = g_stage_data;
        if (g_Globals.game_mode == 2 && g_GameThread->replay_mode == 0)
        {
            char path[0x100];
            strcpy(path, stage->music_names[boss]);
            strcat(path, ".wav");
            if (strcmp(g_SoundManager.bgm_name, path) == 0)
            {
                return;
            }
        }
        i32 track = stage->music_ids[boss];
        if (g_Supervisor.config.flags_2c & 0x10)
        {
            g_SoundManager.modify_bgm(BGM_STOP_4, 0, "dummy");
        }
        g_SoundManager.modify_bgm(BGM_PLAY, boss, "dummy");
        g_Scorefile->bgm_unlocked[track] = 1;
        g_Gui->stage_logo_anm->create_effect(boss + 1, -1, NULL);
    }
    else if (script == -2)
    {
        if (g_Spellcard->flags & SPELLCARD_FLAG_80)
        {
            pause_menu_43f350();
        }
        else
        {
            stage_clear_42e150();
        }
    }
    else
    {
        if (msg != NULL)
        {
            delete msg;
            msg = NULL;
        }
        msg = new GuiMsgVm((u8 *)msg_file + msg_file->scripts[script].offset);
        msg->script_num = script;
    }
}

// TODO: the original realigns the frame (and esp, -8) and has 4 more bytes of it.
// FUNCTION: TH16 0x426780
void Gui::create_vm_110()
{
    Gui *gui = g_Gui;
    gui->id_110 = gui->front_anm->create_vm_inline(0x3c, NULL, 0.0f, -1);
}
