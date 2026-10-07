#include <stdlib.h>
#include <string.h>

#include "AnmManager.h"
#include "AsciiManager.h"
#include "Bomb.h"
#include "BulletManager.h"
#include "Enemy.h"
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

// The ANM manager slots of front.anm and the stage logo's anm file.
#define ANM_SLOT_FRONT 5
#define ANM_SLOT_STAGE_LOGO 6

// Deletes the faces and text.
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
    delete_vm_inline_and_clear(id_54);
    delete_vm_inline_and_clear(text_line_1);
    delete_vm_inline_and_clear(text_line_2);
    delete_vm_inline_and_clear(furigana_1);
    delete_vm_inline_and_clear(furigana_2);
    delete_vm_inline_and_clear(intro);
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

// Takes on_draw_2_callback's address for initialize. In the original the
// callback is not entered with known alignment (on_draw_2_body realigns
// itself), so AsciiInf::create_number, which only on_draw_2_body calls,
// keeps an unpadded frame; taking the address in an inline helper node keeps
// LTCG from handing GameThread::thread_start's alignment down to it.
static inline UpdateFuncCallback gui_on_draw_2_callback()
{
    return (UpdateFuncCallback)Gui::on_draw_2_callback;
}

// Loads front.anm and the stage's files and registers the update
// functions (inactive until setup_stage_hud).
// FUNCTION: TH16 0x426b00
i32 Gui::initialize()
{
    front_anm = AnmManager::preload_anm(ANM_SLOT_FRONT, "front.anm");
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

    f = g_UpdateFuncRegistry->create_func(gui_on_draw_2_callback());
    f->flags &= ~UPDATE_FUNC_ACTIVE;
    f->arg = this;
    g_UpdateFuncRegistry->register_on_draw(f, 0x30);
    on_draw_2 = f;
    return 0;
}

// Loads the stage logo and the dialogue file for the character, unless a
// stage restart kept the latter in g_msg_file_cache.
// FUNCTION: TH16 0x426c10
i32 Gui::load_stage_files()
{
    stage_logo_anm = AnmManager::preload_anm(ANM_SLOT_STAGE_LOGO, g_stage_data->logo_anm_filename);
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
            // "The data is corrupt."
            g_GameErrorContext.log("\x83" "f\x81[\x83^\x82\xaa\x89\xf3\x82\xea\x82\xc4\x82\xa2\x82\xdc\x82\xb7\r\n");
            return -1;
        }
    }
    time_in_stage.reset();
    current_score = g_Globals.score;
    boss_timer_seconds = -1;
    boss_timer_shown_seconds = -1;
    return 0;
}

// Ends the stage's part of the HUD: across a stage restart the stage logo
// and the dialogue file stay loaded (the latter in g_msg_file_cache).
// FUNCTION: TH16 0x427730
void Gui::release_stage_files()
{
    if (!(g_Globals.flags_lo_45c & GLOBALS_STAGE_RESTART_MASK))
    {
        g_AnmManager->unload_anm(ANM_SLOT_STAGE_LOGO);
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
    if (!(g_Globals.flags_lo_45c & GLOBALS_STAGE_RESTART_MASK))
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
    delete_vm_and_clear(spell_notice_id);
    delete_vm_and_clear(notice_id);
    delete_vm_and_clear(boss_marker_id);
    for (i32 i = 0; i < 9; i++)
    {
        boss_star_ids[i].id = 0;
    }
    boss_star_id_9.id = 0;
    AnmManager *anm = g_AnmManager;
    for (i32 i = 0; i < 10; i++)
    {
        anm->delete_vm_inline(bonus_digit_ids[i]);
        bonus_digit_ids[i].id = 0;
    }
    boss_star_count = 0;
    for (i32 i = 0; i < 3; i++)
    {
        boss_bars[i].vms_created = 0;
    }
    hud_flags |= GUI_FLAGS_STAGE_RELEASED;
}

// FUNCTION: TH16 0x427970
HARNESS_CALLED void Gui::release_msg()
{
    if (msg != NULL)
    {
        delete msg;
        msg = NULL;
    }
    if (!(g_Globals.flags_lo_45c & GLOBALS_STAGE_RESTART_MASK))
    {
        g_AnmManager->unload_anm(ANM_SLOT_STAGE_LOGO);
        stage_logo_anm = NULL;
        if (msg_file != NULL)
        {
            free(msg_file);
            msg_file = NULL;
        }
        msg_file = NULL;
    }
}

// Also deletes the HUD's own VMs and disables front.anm's.
// FUNCTION: TH16 0x427a20
Gui::~Gui()
{
    release_stage_files();
    g_UpdateFuncRegistry->unregister_locked(on_tick);
    g_UpdateFuncRegistry->unregister_locked(on_draw_1);
    g_UpdateFuncRegistry->unregister_locked(on_draw_2);
    on_tick = NULL;
    delete_vm_and_clear(id_104);
    delete_vm_and_clear(overlay_ids[2]);
    AnmManager *anm = g_AnmManager;
    for (i32 i = 0; i < 10; i++)
    {
        anm->delete_vm_inline(bonus_digit_ids[i]);
        bonus_digit_ids[i].id = 0;
    }
    delete_vm_inline_and_clear(boss_timer_tens_id);
    delete_vm_inline_and_clear(boss_timer_ones_id);
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

// The push ecx/pop ecx around the call pads the frame for 8-byte stack
// alignment: GameThread::thread_start realigns and Gui::initialize, which
// it calls through Gui::create, registers this callback.
// FUNCTION: TH16 0x429af0
i32 __fastcall Gui::on_tick_callback(Gui *self)
{
    return self->on_tick_body();
}

// FUNCTION: TH16 0x429b00
i32 __fastcall Gui::on_draw_1_callback(Gui *self)
{
    return UPDATE_FUNC_CONTINUE;
}

// FUNCTION: TH16 0x429b10
i32 __fastcall Gui::on_draw_2_callback(Gui *self)
{
    return self->on_draw_2_body();
}

// The player's face script in its anm file, per character.
// GLOBAL: TH16 0x492260
static const i32 g_msg_player_face_scripts[4] = {26, 16, 22, 35};

// ANM interrupts the dialogue sends to faces and text: fade out and go,
// highlight a face (or show text), darken a face (or fade text out), shake
// a face, and the first expression (MSG_PLAYER_FACE / MSG_BOSS_FACE add the
// expression number).
#define MSG_ANM_REMOVE 1
#define MSG_ANM_ACTIVE 2
#define MSG_ANM_INACTIVE 3
#define MSG_ANM_SHAKE 7
#define MSG_ANM_FACE_0 0x11

// The font of the text: 0, or 1 after MSG_TEXT_FONT_2.
#define MSG_TEXT_FONT() ((flags >> 1) & 1)
// The current side's text color.
#define MSG_TEXT_COLOR() ((&side_text_color_0)[active_side])
// The speech bubble shape: per side, and per bubble type set by
// MSG_BUBBLE_TYPE.
#define MSG_TEXTBOX_KIND() (active_side + ((flags >> 2) & 0xf) * 2)

// Bit numbers of the shot and skip buttons (InputState::hold_time indices),
// and how long one of them is held before skipping starts.
#define MSG_BUTTON_SHOT_BIT 0
#define MSG_BUTTON_SKIP_BIT 9
#define MSG_SKIP_HOLD_TIME 0x14

// Runs the dialogue instructions whose time has come; -1 once the script
// has ended. Holding shot or skip in a skippable script runs every
// instruction at once, and MSG_TEXT_PAUSE waits for a key. Then keeps the
// text next to the speech bubble.
// TODO: ours gets a /GS cookie (from the Float3 locals of MSG_TEXT_ADD and
// the bubble code; still unexplained) and uses ebx; the original keeps the
// instruction pointer in ecx and reloads it after calls.
// FUNCTION: TH16 0x42a1d0
HARNESS_CALLED i32 GuiMsgVm::run()
{
    if (ecl_resume_timer > 0)
    {
        ecl_resume_timer--;
    }
    if (g_InputState.input_rising & (INPUT_SHOT | INPUT_SKIP))
    {
        flags |= MSG_FLAG_KEY_PRESSED;
    }
    // Skipping: jump to the next instruction's time.
    if ((flags & MSG_FLAG_CAN_SKIP) == MSG_FLAG_CAN_SKIP &&
        ((g_InputState.input & INPUT_SKIP) && (u32)g_InputState.hold_time[MSG_BUTTON_SKIP_BIT] >= MSG_SKIP_HOLD_TIME ||
         (g_InputState.input & INPUT_SHOT) && (u32)g_InputState.hold_time[MSG_BUTTON_SHOT_BIT] >= MSG_SKIP_HOLD_TIME))
    {
        time_in_script.set(instr()->time);
    }
    while (time_in_script.current >= instr()->time)
    {
        switch (instr()->opcode)
        {
        case MSG_TEXT_FONT_2:
            flags |= MSG_FLAG_FONT_2;
            break;
        // Never used by the scripts: replace a line outright.
        case MSG_TEXT_LINE_1:
        {
            AnmVm *vm = get_vm_or_clear(text_line_1);
            const char *text = decode_msg_string(instr()->args.s);
            g_AnmManager->draw_text(vm, MSG_TEXT_COLOR(), 0, MSG_TEXT_FONT() + 4, 0, 0, text);
            AnmManager::interrupt_tree(text_line_1, MSG_ANM_ACTIVE);
            break;
        }
        case MSG_TEXT_LINE_2:
        {
            AnmVm *vm = get_vm_or_clear(text_line_2);
            const char *text = decode_msg_string(instr()->args.s);
            g_AnmManager->draw_text(vm, MSG_TEXT_COLOR(), 0, MSG_TEXT_FONT() + 4, 0, 0, text);
            AnmManager::interrupt_tree(text_line_2, MSG_ANM_ACTIVE);
            break;
        }
        case MSG_BUBBLE_POS:
            bubble_x = instr()->args.f[0] * 2.0f;
            bubble_y = instr()->args.f[1] * 2.0f;
            break;
        // The next line of text, or furigana ("|x,y,text") for the line
        // being written. The bubble grows to the longest line.
        case MSG_TEXT_ADD:
            if (next_text_line == 0)
            {
                if (text_cleared == 0)
                {
                    bubble_width = 0.0f;
                    g_AnmManager->draw_text(text_line_1.find_or_clear(), MSG_TEXT_COLOR(), 0, MSG_TEXT_FONT(), 0, 0,
                                            "  ");
                    g_AnmManager->draw_text(text_line_2.find_or_clear(), MSG_TEXT_COLOR(), 0, MSG_TEXT_FONT(), 0, 0,
                                            "  ");
                    g_AnmManager->draw_text(furigana_1.find_or_clear(), MSG_TEXT_COLOR(), 0, MSG_TEXT_FONT(), 0, 1,
                                            "  ");
                    g_AnmManager->draw_text(furigana_2.find_or_clear(), MSG_TEXT_COLOR(), 0, MSG_TEXT_FONT(), 0, 1,
                                            "  ");
                    text_cleared = 1;
                    AnmManager::interrupt_tree(text_line_1, MSG_ANM_INACTIVE);
                    AnmManager::interrupt_tree(text_line_2, MSG_ANM_INACTIVE);
                    AnmManager::interrupt_tree(furigana_1, MSG_ANM_INACTIVE);
                    AnmManager::interrupt_tree(furigana_2, MSG_ANM_INACTIVE);
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
                    furigana_1.set_entity_pos((Float3 *)&bubble_x);
                    AnmManager::interrupt_tree_and_run(furigana_1, MSG_ANM_ACTIVE);
                }
                else
                {
                    f32 width = (strlen(text) / 2 * 16 - 28) * 2.0f;
                    bubble_width = width > bubble_width ? width : bubble_width;
                    set_textbox(bubble_x, bubble_y, bubble_width, MSG_TEXTBOX_KIND());
                    set_textbox_width(bubble_width, MSG_TEXTBOX_KIND());
                    g_AnmManager->draw_text(text_line_1.find_or_clear(), MSG_TEXT_COLOR(), 0, MSG_TEXT_FONT(), 0, 0,
                                            text);
                    if (active_side >= 1)
                    {
                        Float3 pos = *(Float3 *)&bubble_x;
                        text_line_1.set_entity_pos(&pos);
                        furigana_1.set_entity_pos(&pos);
                        text_line_2.set_entity_pos(&pos);
                        furigana_2.set_entity_pos(&pos);
                    }
                    else
                    {
                        Float3 pos = *(Float3 *)&bubble_x;
                        text_line_1.set_entity_pos(&pos);
                    }
                    AnmManager::interrupt_tree_and_run(text_line_1, MSG_ANM_ACTIVE);
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
                    furigana_2.set_entity_pos((Float3 *)&bubble_x);
                    AnmManager::interrupt_tree(furigana_2, MSG_ANM_ACTIVE);
                }
                else
                {
                    f32 width = (strlen(text) / 2 * 16 - 28) * 2.0f;
                    bubble_width = width > bubble_width ? width : bubble_width;
                    set_textbox(bubble_x, bubble_y, bubble_width, MSG_TEXTBOX_KIND() + 8);
                    set_textbox_width(bubble_width, MSG_TEXTBOX_KIND() + 8);
                    g_AnmManager->draw_text(text_line_2.find_or_clear(), MSG_TEXT_COLOR(), 0, MSG_TEXT_FONT(), 0, 0,
                                            text);
                    if (active_side >= 1)
                    {
                        Float3 pos = *(Float3 *)&bubble_x;
                        text_line_1.set_entity_pos(&pos);
                        furigana_1.set_entity_pos(&pos);
                        text_line_2.set_entity_pos(&pos);
                        furigana_2.set_entity_pos(&pos);
                    }
                    else
                    {
                        Float3 pos = *(Float3 *)&bubble_x;
                        text_line_2.set_entity_pos(&pos);
                    }
                    AnmManager::interrupt_tree_and_run(text_line_2, MSG_ANM_ACTIVE);
                    next_text_line = 0;
                    text_cleared = 0;
                }
            }
            break;
        case MSG_TEXT_CLEAR:
            delete_vm_and_clear(textbox);
            AnmManager::interrupt_tree(text_line_1, MSG_ANM_INACTIVE);
            AnmManager::interrupt_tree(text_line_2, MSG_ANM_INACTIVE);
            AnmManager::interrupt_tree(furigana_1, MSG_ANM_INACTIVE);
            AnmManager::interrupt_tree(furigana_2, MSG_ANM_INACTIVE);
            break;
        // Argument 0: the character's own face; otherwise the face from ECL
        // anm slot 5.
        case MSG_PLAYER_SHOW:
            if (instr()->args.i[0] == 0)
            {
                player_face = g_Player->anm_file->create_effect(g_msg_player_face_scripts[g_Globals.character], -1, NULL);
            }
            else
            {
                player_face = g_EnemyManager->anim_statement_anms[5]->create_effect(0xb, -1, NULL);
            }
            break;
        case MSG_BOSS_SHOW:
        {
            i32 i = instr()->args.i[0];
            StageBoss *boss = &g_stage_data->bosses[i];
            enemy_faces[i] =
                g_EnemyManager->anim_statement_anms[boss->face_anm_slot]->create_effect(boss->face_script, -1, NULL);
            unk_1c0 = 0;
            break;
        }
        case MSG_BOSS_SHOW_SECOND:
        {
            StageBoss *boss = &g_stage_data->bosses[1];
            enemy_faces[1] =
                g_EnemyManager->anim_statement_anms[boss->face_anm_slot]->create_effect(boss->face_script, -1, NULL);
            unk_1c0 = 0;
            break;
        }
        case MSG_TEXT_OFFSET_Y:
            get_vm_or_clear(text_line_1)->pos_2.y = instr()->args.i[0];
            get_vm_or_clear(text_line_2)->pos_2.y = instr()->args.i[0];
            get_vm_or_clear(furigana_1)->pos_2.y = instr()->args.i[0];
            get_vm_or_clear(furigana_2)->pos_2.y = instr()->args.i[0];
            break;
        case MSG_PLAYER_SHAKE:
            AnmManager::interrupt_tree(player_face, MSG_ANM_SHAKE);
            break;
        case MSG_BOSS_SHAKE:
            AnmManager::interrupt_tree(enemy_faces[0], MSG_ANM_SHAKE);
            AnmManager::interrupt_tree(enemy_faces[1], MSG_ANM_SHAKE);
            break;
        case MSG_PLAYER_HIDE:
            AnmManager::interrupt_tree(player_face, MSG_ANM_REMOVE);
            player_face.id = 0;
            break;
        case MSG_BOSS_HIDE:
            AnmManager::interrupt_tree(enemy_faces[instr()->args.i[0]], MSG_ANM_REMOVE);
            enemy_faces[instr()->args.i[0]].id = 0;
            AnmManager::interrupt_tree(intro, MSG_ANM_REMOVE);
            break;
        case MSG_TEXTBOX_HIDE:
            AnmManager::interrupt_tree(text_line_1, MSG_ANM_REMOVE);
            AnmManager::interrupt_tree(text_line_2, MSG_ANM_REMOVE);
            AnmManager::interrupt_tree(furigana_1, MSG_ANM_REMOVE);
            AnmManager::interrupt_tree(furigana_2, MSG_ANM_REMOVE);
            delete_vm_and_clear(textbox);
            break;
        // Argument 0 picks the player (0) or a boss (argument 1).
        case MSG_PORTRAIT_DARKEN:
            if (instr()->args.i[0] == 0)
            {
                AnmManager::interrupt_tree_and_run(player_face, MSG_ANM_INACTIVE);
            }
            else
            {
                AnmManager::interrupt_tree_and_run(enemy_faces[instr()->args.i[1]], MSG_ANM_INACTIVE);
            }
            break;
        case MSG_PORTRAIT_HIGHLIGHT:
            if (instr()->args.i[0] == 0)
            {
                AnmManager::interrupt_tree_and_run(player_face, MSG_ANM_ACTIVE);
            }
            else
            {
                AnmManager::interrupt_tree_and_run(enemy_faces[instr()->args.i[1]], MSG_ANM_ACTIVE);
            }
            break;
        // The speaker instructions highlight the speaker's face, darken the
        // others and reset the text.
        case MSG_SPEAKER_PLAYER:
            for (i32 i = 0; i < 4; i++)
            {
                AnmManager::interrupt_tree_and_run(enemy_faces[i], MSG_ANM_INACTIVE);
            }
            AnmManager::interrupt_tree_and_run(player_face, MSG_ANM_ACTIVE);
            AnmManager::interrupt_tree(id_54, MSG_ANM_ACTIVE);
            active_side = 0;
            get_vm_or_clear(text_line_1)->pos_2.y = 0.0f;
            get_vm_or_clear(text_line_2)->pos_2.y = 0.0f;
            get_vm_or_clear(furigana_1)->pos_2.y = 0.0f;
            get_vm_or_clear(furigana_2)->pos_2.y = 0.0f;
            flags &= ~2;
            next_text_line = 0;
            text_cleared = 0;
            break;
        case MSG_SPEAKER_BOSS:
            AnmManager::interrupt_tree_and_run(player_face, MSG_ANM_INACTIVE);
            for (i32 i = 0; i < 4; i++)
            {
                if (i == instr()->args.i[0])
                {
                    AnmManager::interrupt_tree_and_run(enemy_faces[instr()->args.i[0]], MSG_ANM_ACTIVE);
                }
                else
                {
                    AnmManager::interrupt_tree_and_run(enemy_faces[i], MSG_ANM_INACTIVE);
                }
            }
            AnmManager::interrupt_tree(id_54, MSG_ANM_INACTIVE);
            active_side = 1;
            get_vm_or_clear(text_line_1)->pos_2.y = 0.0f;
            get_vm_or_clear(text_line_2)->pos_2.y = 0.0f;
            get_vm_or_clear(furigana_1)->pos_2.y = 0.0f;
            get_vm_or_clear(furigana_2)->pos_2.y = 0.0f;
            flags &= ~2;
            next_text_line = 0;
            text_cleared = 0;
            break;
        case MSG_SPEAKER_SIDE:
            AnmManager::interrupt_tree(id_54, MSG_ANM_INACTIVE);
            active_side = instr()->args.i[0];
            get_vm_or_clear(text_line_1)->pos_2.y = 0.0f;
            get_vm_or_clear(text_line_2)->pos_2.y = 0.0f;
            get_vm_or_clear(furigana_1)->pos_2.y = 0.0f;
            get_vm_or_clear(furigana_2)->pos_2.y = 0.0f;
            flags &= ~2;
            next_text_line = 0;
            text_cleared = 0;
            break;
        case MSG_SPEAKER_NONE:
        {
            AnmManager::interrupt_tree_and_run(player_face, MSG_ANM_INACTIVE);
            for (i32 i = 0; i < 4; i++)
            {
                AnmManager::interrupt_tree_and_run(enemy_faces[i], MSG_ANM_INACTIVE);
            }
            AnmManager::interrupt_tree(id_54, MSG_ANM_INACTIVE);
            active_side = 0;
            AnmVm *vm = g_AnmManager->get_vm_with_id(text_line_1);
            if (vm != NULL)
            {
                vm->entity_pos = (&side_text_pos_0)[active_side];
            }
            vm = g_AnmManager->get_vm_with_id(text_line_2);
            if (vm != NULL)
            {
                vm->entity_pos = (&side_text_pos_0)[active_side];
            }
            get_vm_or_clear(text_line_1)->pos_2.y = 0.0f;
            get_vm_or_clear(text_line_2)->pos_2.y = 0.0f;
            vm = g_AnmManager->get_vm_with_id(furigana_1);
            if (vm != NULL)
            {
                vm->entity_pos = (&side_text_pos_0)[active_side];
            }
            vm = g_AnmManager->get_vm_with_id(furigana_2);
            if (vm != NULL)
            {
                vm->entity_pos = (&side_text_pos_0)[active_side];
            }
            get_vm_or_clear(furigana_1)->pos_2.y = 0.0f;
            get_vm_or_clear(furigana_2)->pos_2.y = 0.0f;
            flags &= ~2;
            next_text_line = 0;
            text_cleared = 0;
            break;
        }
        case MSG_SKIPPABLE:
            ((GuiMsgVmFlags *)&flags)->skippable = instr()->args.s[0];
            break;
        // Change a face's expression.
        case MSG_PLAYER_FACE:
            AnmManager::interrupt_tree_and_run(player_face, instr()->args.i[0] + MSG_ANM_FACE_0);
            break;
        case MSG_BOSS_FACE:
            AnmManager::interrupt_tree_and_run(enemy_faces[instr()->args.i[1]], instr()->args.i[0] + MSG_ANM_FACE_0);
            break;
        // Waits for the given time or a key (shot or enter). Holding shot or
        // skip in a skippable script ends the wait too.
        case MSG_TEXT_PAUSE:
            if (pause_timer.current <= 0)
            {
                pause_timer.set_value(instr()->args.i[0]);
            }
            pause_timer--;
            if (!(g_InputState.input_rising & (INPUT_ENTER | INPUT_SHOT)) && pause_timer.current > 0)
            {
                if ((flags & MSG_FLAG_CAN_SKIP) != MSG_FLAG_CAN_SKIP ||
                    ((u32)g_InputState.get_hold_time(MSG_BUTTON_SKIP_BIT) < MSG_SKIP_HOLD_TIME &&
                     (u32)g_InputState.get_hold_time(MSG_BUTTON_SHOT_BIT) < MSG_SKIP_HOLD_TIME))
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
            text_cleared = 0;
            break;
        case MSG_ECL_RESUME:
            ecl_resume_timer = 1;
            break;
        // Starts the boss theme and shows its title (stage logo script 2).
        case MSG_MUSIC_BOSS:
            g_Supervisor.play_bgm(1, g_stage_data->music_ids[1]);
            g_Gui->stage_logo_anm->create_effect(2, -1, NULL);
            break;
        // The boss's name and title, and the boss marker.
        case MSG_INTRO:
        {
            StageBoss *boss = &g_stage_data->bosses[instr()->args.i[0]];
            intro = g_EnemyManager->anim_statement_anms[boss->intro_anm_slot]->create_effect(boss->intro_script, -1,
                                                                                            NULL);
            g_Gui->show_boss_marker();
            break;
        }
        case MSG_STAGE_END:
            stage_clear();
            break;
        // Fades the music out, faster on stage 6.
        case MSG_MUSIC_END:
            if (g_Globals.stage_num == 6)
            {
                g_Supervisor.fade_out_bgm(2.0f);
            }
            else
            {
                g_Supervisor.fade_out_bgm(8.0f);
            }
            break;
        case MSG_MUSIC_FADE:
            g_Supervisor.fade_out_bgm(instr()->args.f[0]);
            break;
        case MSG_BUBBLE_TYPE:
            ((GuiMsgVmFlags *)&flags)->textbox_type = instr()->args.i[0];
            break;
        case MSG_LIGHTS_OUT:
            Gui::show_lights_out();
            break;
        case MSG_TEXTBOX_SHOW:
        case MSG_ROUTE_SELECT:
            break;
        case MSG_END:
            return -1;
        }
        current_instr = (u8 *)current_instr + instr()->args_size + 4;
    }
    time_in_script.tick();
waiting:
    // Keep the text next to the speech bubble.
    i32 script = textbox_kind + FRONT_ANM_BUBBLE_BODY;
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

// Puts vm just outside the bubble's body, on the side of the speaker.
// TODO: the original aligns its frame to 8 bytes and adds two of the
// vector components the other way round.
// FUNCTION: TH16 0x42b480
void GuiMsgVm::update_callout(AnmVm *vm)
{
    i32 script = textbox_kind + FRONT_ANM_BUBBLE_BODY;
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

// The pause menu hides the dialogue and shows it again.
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

// A new speech bubble of the given kind at (x, y), with its body and edge
// stretched to width.
// FUNCTION: TH16 0x42ba30
HARNESS_CALLED void GuiMsgVm::set_textbox(f32 x, f32 y, f32 width, i32 kind)
{
    delete_vm_and_clear(textbox);
    AnmLoaded *anm = g_Gui->front_anm;
    Float3 pos(x, y, 0.0f);
    textbox = anm->create_vm(kind + FRONT_ANM_BUBBLE, &pos, 0.0f, -1, 0);
    find_child_of(textbox, kind + FRONT_ANM_BUBBLE_BODY)->float_vars[0] = width;
    find_child_of(textbox, kind + FRONT_ANM_BUBBLE_EDGE)->float_vars[0] = width;
    textbox_kind = kind;
}

// Stretches the bubble's body and edge to width plus a 16-pixel margin.
// FUNCTION: TH16 0x42bb30
HARNESS_CALLED void GuiMsgVm::set_textbox_width(f32 width, i32 kind)
{
    find_child_of(textbox, kind + FRONT_ANM_BUBBLE_BODY)->float_vars[0] = width + 16.0f;
    find_child_of(textbox, kind + FRONT_ANM_BUBBLE_EDGE)->float_vars[0] = width + 16.0f;
}

// TODO: the original aligns its frame to 8 bytes, which LTCG adds for
// Gui::show_notice's sake.
// FUNCTION: TH16 0x42bc10
void Gui::update_score()
{
    Gui *gui = g_Gui;
    if (g_Globals.score != gui->current_score)
    {
        // A 32nd of the gap per frame, at least 1 and at most 579934 (score
        // units of 10), never slowing down until it arrives.
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
        // The flag is set before it is tested, so the hiscore notice never
        // shows.
        g_Globals.flags_lo_45c |= GLOBALS_HISCORE_BEATEN;
        if (!(g_Globals.flags_lo_45c & GLOBALS_HISCORE_BEATEN))
        {
            gui->show_notice(0, GUI_NOTICE_HISCORE);
        }
    }
}

// Shows a HUD notice (GuiNotice). The spell card bonus also spells out
// bonus in digits, leaving out leading zeros and adding the commas it needs.
// Every caller passes 0-4, which is how the original's jump table goes
// without a bounds check; the __assume reproduces that.
// TODO: in the digit loop the original keeps the manager in ebx and spills the counter; ours does the reverse.
// FUNCTION: TH16 0x42bcf0
HARNESS_CALLED void Gui::show_notice(i32 bonus, i32 kind)
{
    switch (kind)
    {
    case GUI_NOTICE_SPELL_BONUS:
    {
        delete_vm_and_clear(spell_notice_id);
        spell_notice_id = front_anm->create_effect(FRONT_ANM_SPELL_BONUS, -1, NULL);
        i32 divisor = 10000000;
        i32 rest = bonus;
        i32 shown = 0;
        AnmManager *anm;
        AnmVm *vm;
        for (i32 i = 0; i < 8; i++)
        {
            delete_vm_and_clear(bonus_digit_ids[i]);
            bonus_digit_ids[i] = g_AsciiManager->ascii_anm->create_effect(i + ASCII_ANM_BONUS_DIGITS, -1, NULL);
            anm = g_AnmManager;
            i32 digit = rest / divisor;
            rest = rest % divisor;
            if (digit != 0)
            {
                shown = 1;
            }
            vm = anm->get_vm_with_id(bonus_digit_ids[i]);
            if (vm != NULL)
            {
                anm->loaded_anms[vm->anm_loaded_index]->set_sprite(vm, digit + ASCII_ANM_SPRITE_DIGIT_0);
            }
            if (!shown)
            {
                vm = anm->get_vm_with_id(bonus_digit_ids[i]);
                if (vm != NULL)
                {
                    vm->clear_flag_lo_2_tree_inline();
                }
            }
            else
            {
                vm = anm->get_vm_with_id(bonus_digit_ids[i]);
                if (vm != NULL)
                {
                    vm->set_flag_lo_2_tree_inline();
                }
            }
            divisor /= 10;
        }
        delete_vm_and_clear(bonus_digit_ids[8]);
        if (bonus >= 1000000)
        {
            bonus_digit_ids[8] = g_AsciiManager->ascii_anm->create_effect(ASCII_ANM_BONUS_COMMA_1, -1, NULL);
            anm = g_AnmManager;
            vm = anm->get_vm_with_id(bonus_digit_ids[8]);
            if (vm != NULL)
            {
                anm->loaded_anms[vm->anm_loaded_index]->set_sprite(vm, ASCII_ANM_SPRITE_COMMA);
            }
        }
        delete_vm_and_clear(bonus_digit_ids[9]);
        if (bonus >= 1000)
        {
            bonus_digit_ids[9] = g_AsciiManager->ascii_anm->create_effect(ASCII_ANM_BONUS_COMMA_2, -1, NULL);
            anm = g_AnmManager;
            vm = anm->get_vm_with_id(bonus_digit_ids[9]);
            if (vm != NULL)
            {
                anm->loaded_anms[vm->anm_loaded_index]->set_sprite(vm, ASCII_ANM_SPRITE_COMMA);
            }
        }
        spell_bonus_shown = 1;
        overlay_ids[GUI_OVERLAY_SPELL_BONUS_BACK] = front_anm->create_effect(FRONT_ANM_SPELL_BONUS_BACK, -1, NULL);
        break;
    }
    case GUI_NOTICE_BONUS_FAILED:
        delete_vm_and_clear(spell_notice_id);
        spell_notice_id = front_anm->create_effect(FRONT_ANM_BONUS_FAILED, -1, NULL);
        spell_bonus_shown = 1;
        overlay_ids[GUI_OVERLAY_SPELL_BONUS_BACK] = front_anm->create_effect(FRONT_ANM_SPELL_BONUS_BACK, -1, NULL);
        break;
    case GUI_NOTICE_FULL_POWER:
        delete_vm_and_clear(notice_id);
        notice_id = front_anm->create_effect(FRONT_ANM_FULL_POWER, -1, NULL);
        break;
    case GUI_NOTICE_HISCORE:
        delete_vm_and_clear(notice_id);
        notice_id = front_anm->create_effect(FRONT_ANM_HISCORE, -1, NULL);
        break;
    case GUI_NOTICE_EXTEND:
        delete_vm_and_clear(notice_id);
        notice_id = front_anm->create_effect(FRONT_ANM_EXTEND, -1, NULL);
        break;
    case GUI_NOTICE_6:
        delete_vm_and_clear(spell_notice_id);
        spell_notice_id = front_anm->create_effect(FRONT_ANM_NOTICE_6, -1, NULL);
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

// The stage clear bonus is a million points per stage number.
// FUNCTION: TH16 0x42c070
void Gui::show_stage_clear_bonus()
{
    Gui *gui = g_Gui;
    gui->overlay_ids[GUI_OVERLAY_STAGE_CLEAR_BONUS] =
        create_vm_inline(gui->front_anm, FRONT_ANM_STAGE_CLEAR_BONUS, NULL, 0.0f, -1);
    gui->stage_clear_bonus = g_Globals.stage_num * 1000000;
    g_Globals.add_to_score(gui->stage_clear_bonus);
    gui->hud_flags |= GUI_STAGE_CLEAR_BONUS;
    gui->notice_timer.reset();
}

// FUNCTION: TH16 0x42c1b0
HARNESS_CALLED void Gui::hide_stage_clear_bonus()
{
    AnmManager::interrupt_tree(overlay_ids[GUI_OVERLAY_STAGE_CLEAR_BONUS], 1);
    AnmManager::interrupt_tree(overlay_ids[GUI_OVERLAY_2], 1);
    hud_flags &= ~GUI_STAGE_CLEAR_BONUS;
    notice_timer.reset();
}

// Not in game mode 8 or the demo.
// FUNCTION: TH16 0x42c240
void show_stage_logo()
{
    if (g_Supervisor.gamemode_to_switch_to != 8 && !(g_Globals.flags_hi_45c & GLOBALS_HI_DEMO_PLAY))
    {
        g_Gui->stage_logo_anm->create_effect(0, -1, NULL);
    }
}

// Interrupt 2 shows a full piece of the counter and 3 an empty one; the
// piece after the last full one shows the score progress towards the next
// extend in fifths (interrupt 7 + n).
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

// Like update_lives, with the bomb fragments (of 5) in the next piece.
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

// The marker is the stage table's for boss 0 from chapter 0x29 on, and
// boss 1's before that.
// TODO: the original tests the script again after adding 0xa4 instead of
// using the add's flags.
// FUNCTION: TH16 0x42c480
void Gui::show_boss_marker()
{
    if (get_vm_or_clear(boss_marker_id) != NULL)
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
        script += FRONT_ANM_BOSS_MARKER;
        if (script >= 0)
        {
            boss_marker_id = front_anm->create_effect(script, -1, NULL);
        }
    }
}

// FUNCTION: TH16 0x42c4f0
void Gui::hide_chapter_result()
{
    AnmManager::interrupt_tree(overlay_ids[GUI_OVERLAY_CHAPTER_RESULT], 1);
    hud_flags = hud_flags & ~GUI_CHAPTER_RESULT_DONE | GUI_CHAPTER_RESULT_COUNTING;
    notice_timer.reset();
}

// FUNCTION: TH16 0x42c580
void Gui::hide_chapter_result_vm()
{
    AnmVm *vm = g_AnmManager->get_vm_with_id(g_Gui->overlay_ids[GUI_OVERLAY_CHAPTER_RESULT]);
    if (vm != NULL)
    {
        vm->clear_flag_lo_2_tree_inline();
    }
}

// FUNCTION: TH16 0x42c5c0
void Gui::show_chapter_result_vm()
{
    AnmVm *vm = g_AnmManager->get_vm_with_id(g_Gui->overlay_ids[GUI_OVERLAY_CHAPTER_RESULT]);
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

// Fills the season gauge bar towards the next level and shows the level
// (interrupt 7 + level), switching the gauge's look (interrupt 2 or 3) when
// the first level is reached or lost.
// TODO: the original keeps g_AnmManager and then the level in ebx; ours spills both (get_vm_with_id is an opaque stub here).
// FUNCTION: TH16 0x42c600
void Gui::update_season_gauge()
{
    AnmManager *anm = g_AnmManager;
    Gui *gui = g_Gui;
    AnmVm *gauge = anm->get_vm_with_id(find_child_id_of(anm, gui->season_gauge_id, FRONT_ANM_SEASON_GAUGE_BAR));
    AnmVm *level_vm = anm->get_vm_with_id(find_child_id_of(anm, gui->season_gauge_id, FRONT_ANM_SEASON_GAUGE_LEVEL));
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

// The two digit VMs take their spell card look.
// FUNCTION: TH16 0x4175d0
HARNESS_CALLED void Gui::boss_timer_on_spell_start()
{
    AnmVm *vm = boss_timer_tens_vm;
    vm->interrupt(2);
    vm->run();
    vm = boss_timer_ones_vm;
    vm->interrupt(2);
    vm->run();
}

// FUNCTION: TH16 0x417650
HARNESS_CALLED void Gui::boss_timer_on_spell_end()
{
    AnmVm *vm = boss_timer_tens_vm;
    vm->interrupt(3);
    vm->run();
    vm = boss_timer_ones_vm;
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
HARNESS_CALLED AnmId AnmLoaded::create_ui_effect(i32 script, i32 unused, AnmVm **out)
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

// Starts a dialogue script: creates the text and furigana VMs (text.anm
// scripts 0 and 1; the second of each gets interrupt 7, presumably to make
// it the lower line),
// clears bullets, lasers and enemies, and puts the bubble at its default
// place.
// TODO: ours gets a /GS cookie and keeps the create_effect results in a
// local; the original has no cookie and reuses script's argument slot for them.
// FUNCTION: TH16 0x429b20
GuiMsgVm::GuiMsgVm(void *script)
{
    memset(this, 0, sizeof(GuiMsgVm));
    time_alive.reset();
    time_in_script.reset();
    unk_154 = 0;
    pause_timer.reset();
    current_instr = script;
    text_line_1 = g_Supervisor.text_anm->create_effect(0, -1, NULL);
    text_line_2 = g_Supervisor.text_anm->create_effect(0, -1, NULL);
    AnmManager::interrupt_tree_and_run(text_line_2, 7);
    // Both lines and both furigana VMs use 21-pixel glyphs.
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
    text_cleared = 0;
    side_text_color_0 = 0;
    side_text_color_1 = 0;
    side_text_color_2 = 0;
    side_text_color_3 = 0;
    active_side = 0;
    side_text_pos_0 = Float3(16.0f, 0.0f, 0.0f);
    side_text_pos_1 = Float3(16.0f, 0.0f, 0.0f);
    side_text_pos_2 = Float3(16.0f, 0.0f, 0.0f);
    side_text_pos_3 = Float3(16.0f, 0.0f, 0.0f);
    // Dialogue starts on a clean screen.
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
    flags &= ~MSG_FLAG_KEY_PRESSED;
    bubble_x = 384.0f;
    bubble_y = 640.0f;
    bubble_z = 0.0f;
    bubble_width = 320.0f;
}

// ECL dialogRead. Script -1 starts the boss theme and -3 the stage theme
// (unless spell practice already plays it), with their titles; -2 ends
// the stage, or the game when the spell practice card was captured;
// otherwise the dialogue script with that number starts.
// FUNCTION: TH16 0x429ff0
void Gui::start_dialogue(i32 script)
{
    __asm finit;
    if (script == -1 || script == -3)
    {
        i32 boss = script == -1;
        StageData *stage = g_stage_data;
        if (g_Globals.game_mode == GAME_MODE_SPELL_PRACTICE && g_GameThread->replay_mode == 0)
        {
            char path[0x100];
            strcpy(path, stage->music_names[boss]);
            append_wav_extension(path);
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
            stage_clear();
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

// AnmLoaded::create_effect as LTCG inlined it into setup_stage_hud.
static __forceinline AnmId create_effect_inline(AnmLoaded *anm, i32 script, i32 layer, AnmVm **out)
{
    ENTER_CS(CS_ANM_MANAGER);
    anm->vm_count++;
    AnmVm *vm = g_AnmManager->allocate_vm();
    if (out != NULL)
    {
        *out = vm;
    }
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
    vm->entity_pos = D3DXVECTOR3(0.0f, 0.0f, 0.0f);
    vm->rotation.z = 0.0f;
    vm->run();
    vm->mode_of_create_child = 0;
    AnmId id;
    id = g_AnmManager->insert_in_world_list_back(vm);
    LEAVE_CS(CS_ANM_MANAGER);
    return id;
}

// AnmManager::interrupt_tree as LTCG inlined it into setup_stage_hud.
static __forceinline void interrupt_tree_inline(AnmId id, i32 interrupt)
{
    AnmVm *vm = g_AnmManager->get_vm_with_id(id);
    if (vm == NULL)
    {
        return;
    }
    vm->interrupt(interrupt);
    for (ZunList<AnmVm> *node = vm->list_of_children.next; node != NULL; node = node->next)
    {
        node->entry->interrupt(interrupt);
    }
}

// Sets the HUD up for a stage: the life and bomb counters, the boss timer,
// the stage logo, the demo and difficulty markers and the season gauge.
// TODO: ours gets a /GS cookie for pos (see README) and realigns through ebx; the original realigns plainly.
// FUNCTION: TH16 0x426d70
void Gui::setup_stage_hud()
{
    Gui *gui = g_Gui;
    if (gui->on_tick != NULL)
    {
        gui->on_tick->flags |= UPDATE_FUNC_ACTIVE;
    }
    if (gui->on_draw_1 != NULL)
    {
        gui->on_draw_1->flags |= UPDATE_FUNC_ACTIVE;
    }
    if (gui->on_draw_2 != NULL)
    {
        gui->on_draw_2->flags |= UPDATE_FUNC_ACTIVE;
    }
    if (gui->hud_frame_id.id == 0)
    {
        gui->hud_frame_id = gui->front_anm->create_ui_vm_at_origin(FRONT_ANM_HUD_FRAME, 0);
    }
    if (gui->life_counter_vms[0] == NULL)
    {
        for (u32 i = 0; i < 8; i++)
        {
            gui->life_counter_ids[i] =
                gui->front_anm->create_ui_effect(i + FRONT_ANM_LIFE_COUNTER, 0, &gui->life_counter_vms[i]);
        }
        for (u32 i = 0; i < 8; i++)
        {
            gui->bomb_counter_ids[i] =
                gui->front_anm->create_ui_effect(i + FRONT_ANM_BOMB_COUNTER, 0, &gui->bomb_counter_vms[i]);
        }
        // The boss timer's tens and ones digits, hidden for now.
        for (i32 i = 0; i < 2; i++)
        {
            (&gui->boss_timer_tens_id)[i] = create_effect_inline(
                g_AsciiManager->ascii_anm, i + ASCII_ANM_BOSS_TIMER_DIGITS, -1, &(&gui->boss_timer_tens_vm)[i]);
            (&gui->boss_timer_tens_vm)[i]->clear_flag_lo_2_tree_inline();
            (&gui->boss_timer_tens_vm)[i]->flags_hi &= ~ANM_VM_LAYER_KIND_MASK;
        }
    }
    gui->update_lives(g_Globals.lives, g_Globals.life_fragments);
    gui->update_bombs(g_Globals.bombs, g_Globals.bomb_fragments);
    // The stage theme's title.
    if (g_Supervisor.gamemode_to_switch_to != 8 && !(g_Globals.flags_hi_45c & GLOBALS_HI_DEMO_PLAY) &&
        g_Globals.game_mode != GAME_MODE_SPELL_PRACTICE)
    {
        create_effect_inline(gui->stage_logo_anm, 1, -1, NULL);
    }
    if (g_Globals.flags_hi_45c & GLOBALS_HI_DEMO_PLAY)
    {
        create_effect_inline(gui->front_anm, FRONT_ANM_DEMO_PLAY, -1, NULL);
    }
    if (gui->enemy_marker_id.id == 0)
    {
        gui->enemy_marker_id = create_effect_inline(gui->front_anm, FRONT_ANM_ENEMY_MARKER, -1, NULL);
    }
    if (g_Globals.stage_num == 1 && g_GameThread->replay_mode == 0 && g_Globals.continues_used == 0)
    {
        AnmId id = create_effect_inline(gui->front_anm, FRONT_ANM_GAME_START, -1, NULL);
        Float3 pos(0.0f, g_Globals.character == CHARACTER_MARISA ? 148 : 128, 0.0f);
        AnmVm *vm = g_AnmManager->get_vm_with_id(id);
        if (vm != NULL)
        {
            vm->entity_pos = pos;
        }
    }
    if (g_Supervisor.unk_700 != 0)
    {
        gui->id_104 = create_effect_inline(gui->front_anm, g_Globals.difficulty + FRONT_ANM_DIFFICULTY_2, -1, NULL);
        AnmManager::interrupt_tree(gui->id_104, 3);
    }
    gui->difficulty_id = create_effect_inline(gui->front_anm, g_Globals.difficulty + FRONT_ANM_DIFFICULTY, -1, NULL);
    interrupt_tree_inline(gui->id_104, 3);
    gui->boss_star_count = 0;
    for (i32 i = 0; i < 3; i++)
    {
        gui->boss_bars[i].vms_created = 0;
    }
    if (g_Supervisor.unk_700 != 0)
    {
        gui->release_ready = 0;
        gui->season_gauge_id = create_effect_inline(gui->front_anm, FRONT_ANM_SEASON_GAUGE, -1, NULL);
        AnmManager *anm = g_AnmManager;
        AnmVm *vm = anm->get_vm_with_id(find_child_id_of(anm, gui->season_gauge_id, FRONT_ANM_SEASON_GAUGE_ICON));
        if (vm != NULL)
        {
            anm->loaded_anms[vm->anm_loaded_index]->set_sprite(vm, g_Globals.subseason + FRONT_ANM_SPRITE_SUBSEASON);
        }
    }
    update_season_gauge();
    if (gui->lights_out_id.id != 0)
    {
        AnmManager::interrupt_tree(gui->lights_out_id, 1);
        gui->lights_out_id.id = 0;
    }
}

// TODO: the original realigns the frame (and esp, -8) and has 4 more bytes of it.
// FUNCTION: TH16 0x426780
void Gui::show_lights_out()
{
    Gui *gui = g_Gui;
    gui->lights_out_id = gui->front_anm->create_vm_inline(FRONT_ANM_LIGHTS_OUT, NULL, 0.0f, -1);
}

// EnemyManager::get_boss as LTCG inlined it into Gui::on_tick_body: the
// boss with that index, NULL without one (and, like get_boss, the last
// enemy of the list if the id is not found).
static __forceinline EnemyInf *get_boss_inline(EnemyManager *enemies, i32 i)
{
    EnemyInf *e = NULL;
    i32 id = enemies->inner.boss_ids[i];
    if (!id)
    {
        return NULL;
    }
    for (EnemyList *node = enemies->active_enemy_list_head; node != NULL; node = node->next)
    {
        e = node->entry;
        if (e->enemy_id == id)
        {
            return e;
        }
    }
    return e;
}

// Deletes the seven VMs of a boss life bar.
static __forceinline void delete_boss_bar_vms(GuiBossBar *bar)
{
    if (bar->vms_created)
    {
        AnmManager *anm = g_AnmManager;
        for (i32 j = 0; j < 7; j++)
        {
            anm->delete_vm_inline(bar->ids[j]);
            bar->ids[j].id = 0;
        }
        bar->vms_created = 0;
    }
}

// The HUD's frame: the chapter result's count-up, the season gauge moving
// out of the player's way, the boss timer, life bars and stars, the
// dialogue, the enemy marker below the game area and the subseason
// gauge's glow.
// TODO: written for behaviour; register allocation and block order are not matched yet.
// FUNCTION: TH16 0x427cf0
i32 Gui::on_tick_body()
{
    if (hud_flags & GUI_STAGE_CLEAR_BONUS)
    {
        notice_timer.tick_in_place();
    }
    if (hud_flags & GUI_CHAPTER_RESULT_MASK)
    {
        notice_timer.tick();
        if ((hud_flags & GUI_CHAPTER_RESULT_MASK) == GUI_CHAPTER_RESULT_COUNTING && notice_timer.current >= 90)
        {
            if (chapter_percent > 0.0f)
            {
                if (notice_timer.current % 4 == 0)
                {
                    g_SoundManager.play_sound_centered(0x27, 0);
                }
                chapter_percent -= 1.0f;
                chapter_bonus += chapter_bonus_step;
            }
            else
            {
                if (notice_timer.current != 90)
                {
                    g_SoundManager.play_sound_centered(0x2f, 0);
                }
                chapter_percent = chapter_percent_final;
                chapter_bonus = chapter_bonus_final;
                chapter_bonus_step = 0;
                hud_flags = hud_flags & ~GUI_CHAPTER_RESULT_COUNTING | GUI_CHAPTER_RESULT_DONE;
            }
        }
        if (notice_timer.current >= chapter_result_duration)
        {
            hide_chapter_result();
            hud_flags &= ~GUI_CHAPTER_RESULT_MASK;
        }
    }
    if (spell_bonus_shown != 0 && g_AnmManager->get_vm_with_id(overlay_ids[GUI_OVERLAY_SPELL_BONUS_BACK]) == NULL)
    {
        overlay_ids[GUI_OVERLAY_SPELL_BONUS_BACK].id = 0;
        spell_bonus_shown = 0;
    }
    // The season gauge (bottom left) moves away while the player is near it.
    Player *player = g_Player;
    if (!(hud_flags & GUI_SEASON_GAUGE_AWAY))
    {
        if (player != NULL && player->inner.pos.y > 400.0f && -64.0f > player->inner.pos.x)
        {
            AnmManager::interrupt_tree(season_gauge_id, 5);
            hud_flags |= GUI_SEASON_GAUGE_AWAY;
        }
    }
    else if (player != NULL && (384.0f > player->inner.pos.y || player->inner.pos.x > -64.0f))
    {
        AnmManager::interrupt_tree(season_gauge_id, 4);
        hud_flags &= ~GUI_SEASON_GAUGE_AWAY;
    }

    // The boss timer: moves away while the player is near it (at the top,
    // or at the bottom with Spellcard flag 0x100), and ticks in its last
    // seconds.
    EnemyManager *enemies = g_EnemyManager;
    if (enemies != NULL && boss_timer_seconds >= 0 && enemies->get_boss(0) != NULL && !enemies->inner.boss_bit && msg == NULL &&
        !(*(u32 *)&g_GameThread->flags & GAME_THREAD_MUSIC_RESTART))
    {
        boss_timer_tens_vm->set_flag_lo_2_tree();
        boss_timer_ones_vm->set_flag_lo_2_tree();
        u32 shown = hud_flags & GUI_BOSS_TIMER_STATE_MASK;
        if (shown == 0)
        {
            if ((!(g_Spellcard->flags & 0x100) && 128.0f > g_Player->inner.pos.y) ||
                ((g_Spellcard->flags & 0x100) && g_Player->inner.pos.y > 320.0f))
            {
                hud_flags = hud_flags & ~GUI_BOSS_TIMER_HIDDEN | GUI_BOSS_TIMER_AWAY;
                anm_vm_interrupt_5(boss_timer_tens_vm);
                anm_vm_interrupt_5(boss_timer_ones_vm);
            }
        }
        else if (shown == GUI_BOSS_TIMER_AWAY)
        {
            if ((!(g_Spellcard->flags & 0x100) && 160.0f > g_Player->inner.pos.y) ||
                ((g_Spellcard->flags & 0x100) && g_Player->inner.pos.y > 288.0f))
            {
                anm_vm_interrupt_4(boss_timer_tens_vm);
                anm_vm_interrupt_4(boss_timer_ones_vm);
                hud_flags &= ~GUI_BOSS_TIMER_STATE_MASK;
            }
        }
        else
        {
            // Coming back from hidden: restore the spell card look first.
            if (g_Spellcard->flags & 1)
            {
                anm_vm_interrupt_2_run(boss_timer_tens_vm);
                anm_vm_interrupt_2_run(boss_timer_ones_vm);
            }
            else
            {
                anm_vm_interrupt_3_run(boss_timer_tens_vm);
                anm_vm_interrupt_3_run(boss_timer_ones_vm);
            }
            anm_vm_interrupt_4_run(boss_timer_tens_vm);
            anm_vm_interrupt_4_run(boss_timer_ones_vm);
            hud_flags &= ~GUI_BOSS_TIMER_STATE_MASK;
        }
        if (boss_timer_seconds < boss_timer_shown_seconds)
        {
            if (boss_timer_seconds < 2)
            {
                boss_timer_tens_vm->interrupt_out_of_line(9);
                boss_timer_ones_vm->interrupt_out_of_line(9);
                g_SoundManager.play_sound_centered(0xc, 0);
            }
            else if (boss_timer_seconds < 5)
            {
                boss_timer_tens_vm->interrupt_out_of_line(8);
                boss_timer_ones_vm->interrupt_out_of_line(8);
                g_SoundManager.play_sound_centered(0xb, 0);
            }
        }
        else if (boss_timer_seconds > boss_timer_shown_seconds)
        {
            boss_timer_tens_vm->interrupt_out_of_line(7);
            boss_timer_ones_vm->interrupt_out_of_line(7);
        }
        if (boss_timer_seconds != boss_timer_shown_seconds)
        {
            boss_timer_tens_vm->set_sprite(boss_timer_seconds / 10 + ASCII_ANM_SPRITE_DIGIT_0);
            boss_timer_ones_vm->set_sprite(boss_timer_seconds % 10 + ASCII_ANM_SPRITE_DIGIT_0);
        }
        boss_timer_shown_seconds = boss_timer_seconds;
    }
    else
    {
        boss_timer_tens_vm->clear_flag_lo_2_tree_inline();
        boss_timer_ones_vm->clear_flag_lo_2_tree_inline();
        hud_flags = hud_flags & ~GUI_BOSS_TIMER_AWAY | GUI_BOSS_TIMER_HIDDEN;
    }

    // The life bars of the two bosses.
    if (g_EnemyManager != NULL && !g_EnemyManager->inner.boss_bit)
    {
        for (i32 i = 0; i < 2; i++)
        {
            GuiBossBar *bar = &boss_bars[i];
            EnemyInf *boss = get_boss_inline(g_EnemyManager, i);
            if (boss == NULL)
            {
                bar->shown = 0.0f;
                bar->life_markers[0].position = 0.0f;
                bar->life_markers[1].position = 0.0f;
                bar->life_markers[2].position = 0.0f;
                bar->life_markers[3].position = 0.0f;
                delete_boss_bar_vms(bar);
                if (i == 0)
                {
                    g_AnmManager->delete_vm(boss_marker_id);
                    boss_marker_id.id = 0;
                }
                continue;
            }
            if (boss->enemy.life.current >= 100000 || (boss->enemy.flags_low & 0x31) ||
                boss->enemy.set_invuln.current > 0 || msg != NULL)
            {
                delete_boss_bar_vms(bar);
                continue;
            }
            bar->life = boss->enemy.life.current;
            f32 fill = (f32)boss->enemy.life.current / (f32)boss->enemy.life.maximum;
            bar->fill = fill;
            if (fill > bar->shown)
            {
                bar->shown += 0.025f;
            }
            if (bar->shown > fill)
            {
                bar->shown = fill;
            }
            if (bar->vms_created == 0)
            {
                bar->ids[0] = front_anm->create_effect(FRONT_ANM_BOSS_BAR, -1, NULL);
                bar->ids[1] = front_anm->create_effect(FRONT_ANM_BOSS_BAR_2, -1, NULL);
                bar->ids[2] = front_anm->create_effect(FRONT_ANM_BOSS_BAR_3, -1, NULL);
                bar->ids[3] = front_anm->create_effect(FRONT_ANM_BOSS_BAR_MARKER, -1, NULL);
                bar->ids[4] = front_anm->create_effect(FRONT_ANM_BOSS_BAR_MARKER, -1, NULL);
                bar->ids[5] = front_anm->create_effect(FRONT_ANM_BOSS_BAR_MARKER, -1, NULL);
                bar->ids[6] = front_anm->create_effect(FRONT_ANM_BOSS_BAR_MARKER, -1, NULL);
                bar->vms_created = 1;
            }
            show_boss_marker();
            AnmVm *vm = get_vm_or_clear(bar->ids[0]);
            vm->rotation.x = bar->shown * -ZUN_2PI;
            vm->flags_lo |= ANM_VM_ROTATION_CHANGED;
            // Separate floats: a Float3 local gets a /GS cookie in our
            // build (see README).
            f32 pos_x = boss->enemy.final_pos.pos.x * 2.0f;
            f32 pos_y = boss->enemy.final_pos.pos.y * 2.0f;
            f32 pos_z = boss->enemy.final_pos.pos.z;
            vm->entity_pos.x = pos_x;
            vm->entity_pos.y = pos_y;
            vm->entity_pos.z = pos_z;
            vm = get_vm_or_clear(bar->ids[1]);
            vm->entity_pos.x = pos_x;
            vm->entity_pos.y = pos_y;
            vm->entity_pos.z = pos_z;
            vm = get_vm_or_clear(bar->ids[2]);
            vm->entity_pos.x = pos_x;
            vm->entity_pos.y = pos_y;
            vm->entity_pos.z = pos_z;
            f32 marker_z = 0.0f;
            for (i32 j = 0; j < 4; j++)
            {
                AnmVm *marker = get_vm_or_clear(bar->ids[3 + j]);
                if (bar->life_markers[j].position != 0.0f && bar->life_markers[j].position < bar->shown)
                {
                    marker->set_flag_lo_2_tree_inline();
                    f32 angle = normalize_angle(-ZUN_PI - bar->life_markers[j].position * ZUN_2PI);
                    marker->flags_lo |= ANM_VM_ROTATION_CHANGED;
                    marker->rotation.z = angle;
                    f32 s = zun_sinf(angle);
                    f32 c = zun_cosf(angle);
                    marker_z += pos_z;
                    marker->entity_pos.x = c * 0.0f - s * 112.0f + pos_x;
                    marker->entity_pos.y = c * 112.0f + s * 0.0f + pos_y;
                    marker->entity_pos.z = marker_z;
                }
                else
                {
                    marker->clear_flag_lo_2_tree_inline();
                }
            }
            if (bar->faded != 0)
            {
                EnemyInf *b = g_EnemyManager->get_boss(i);
                f32 dx = b->enemy.final_pos.pos.x - g_Player->inner.pos.x;
                f32 dy = b->enemy.final_pos.pos.y - g_Player->inner.pos.y;
                if (dx * dx + dy * dy >= 9216.0f)
                {
                    for (i32 j = 0; j < 7; j++)
                    {
                        AnmManager::interrupt_tree(bar->ids[j], 2);
                    }
                    bar->faded = 0;
                }
            }
            else
            {
                EnemyInf *b = g_EnemyManager->get_boss(i);
                f32 dy = b->enemy.final_pos.pos.y - g_Player->inner.pos.y;
                f32 dx = b->enemy.final_pos.pos.x - g_Player->inner.pos.x;
                if (6400.0f > dx * dx + dy * dy)
                {
                    for (i32 j = 0; j < 7; j++)
                    {
                        AnmManager::interrupt_tree(bar->ids[j], 3);
                    }
                    bar->faded = 1;
                }
            }
        }
    }

    // The boss's remaining spell card stars (boss_star_ids runs into
    // boss_star_id_9).
    AnmId *stars = boss_star_ids;
    for (u32 i = 0; i < 10; i++)
    {
        if ((i32)i < boss_star_count)
        {
            if (stars[i].id == 0)
            {
                stars[i] = front_anm->create_effect(i + FRONT_ANM_BOSS_STARS, -1, NULL);
            }
        }
        else if (stars[i].id != 0)
        {
            AnmManager::interrupt_tree(stars[i], 1);
            stars[i].id = 0;
        }
    }

    if (msg != NULL)
    {
        if (msg->run() != 0)
        {
            delete msg;
            msg = NULL;
        }
        else
        {
            msg->time_alive.tick_in_place();
        }
    }

    // The enemy marker below the game area follows the boss, fading out
    // near the player and changing color (interrupts 7-10) as the boss's
    // life in its attack runs low.
    if (g_EnemyManager != NULL)
    {
        EnemyInf *boss = get_boss_inline(g_EnemyManager, 0);
        if (boss != NULL && !((boss->enemy.flags_low >> 5) & 1) && !(boss->enemy.flags_low & 1))
        {
            AnmVm *vm = get_vm_or_clear(enemy_marker_id);
            vm->set_flag_lo_2_tree_inline();
            u32 level = hud_flags & GUI_ENEMY_MARKER_LEVEL_MASK;
            if (g_Spellcard->flags & 1)
            {
                if (level == 0)
                {
                    if (boss->enemy.life.remaining_for_cur_attack < 2000)
                    {
                        vm->interrupt_out_of_line(7);
                        hud_flags = hud_flags & ~4 | 2;
                    }
                }
                else if (level == 2)
                {
                    if (boss->enemy.life.remaining_for_cur_attack < 1000)
                    {
                        vm->interrupt_out_of_line(8);
                        hud_flags = hud_flags & ~2 | 4;
                    }
                }
                else if (level == 4)
                {
                    if (boss->enemy.life.remaining_for_cur_attack < 400)
                    {
                        vm->interrupt_out_of_line(9);
                        hud_flags |= 6;
                    }
                }
                else if (level == 6)
                {
                    if (boss->enemy.life.remaining_for_cur_attack > 400)
                    {
                        vm->interrupt_out_of_line(10);
                        hud_flags &= ~6;
                    }
                }
            }
            else if (level == 0)
            {
                if (boss->enemy.life.remaining_for_cur_attack < 700)
                {
                    vm->interrupt_out_of_line(7);
                    hud_flags = hud_flags & ~4 | 2;
                }
            }
            else if (level == 2)
            {
                if (boss->enemy.life.remaining_for_cur_attack < 400)
                {
                    vm->interrupt_out_of_line(8);
                    hud_flags = hud_flags & ~2 | 4;
                }
            }
            else if (level == 4)
            {
                if (boss->enemy.life.remaining_for_cur_attack < 200)
                {
                    vm->interrupt_out_of_line(9);
                    hud_flags |= 6;
                }
            }
            else if (level == 6)
            {
                if (boss->enemy.life.remaining_for_cur_attack > 200)
                {
                    vm->interrupt_out_of_line(10);
                    hud_flags &= ~6;
                }
            }
            vm->entity_pos.y = 960.0f;
            vm->entity_pos.x = (boss->enemy.final_pos.pos.x + 32.0f + 192.0f) * 2.0f;
            // The original calls zun_fabsf here; that call makes LTCG stop
            // inlining fabs into zun_fabsf (0x405240) in our build.
            f32 distance = fabsf(boss->enemy.final_pos.pos.x - g_Player->inner.pos.x);
            if (distance >= 64.0f)
            {
                vm->color_1.a = 0xff;
            }
            else
            {
                vm->color_1.a = (i32)(distance * 191.0f * (1.0f / 64.0f)) + 0x40;
            }
            if (-192.0f > boss->enemy.final_pos.pos.x || boss->enemy.final_pos.pos.x > 192.0f)
            {
                vm->color_1.a = 0;
            }
        }
        else
        {
            AnmVm *vm = g_AnmManager->get_vm_with_id(enemy_marker_id);
            if (vm != NULL)
            {
                vm->clear_flag_lo_2_tree_inline();
            }
        }
    }

    // The subseason gauge lights up while a release is possible.
    if (g_SubseasonBomb != NULL && g_SubseasonBomb->can_activate())
    {
        if (release_ready == 0)
        {
            AnmManager::interrupt_tree_and_run(find_child_id_of(g_AnmManager, season_gauge_id, FRONT_ANM_SEASON_GAUGE_RELEASE),
                                               2);
        }
        release_ready = 1;
    }
    else
    {
        if (release_ready == 1)
        {
            AnmManager::interrupt_tree_and_run(find_child_id_of(g_AnmManager, season_gauge_id, FRONT_ANM_SEASON_GAUGE_RELEASE),
                                               3);
        }
        release_ready = 0;
    }
    time_in_stage.tick();
    return UPDATE_FUNC_CONTINUE;
}

// The original formats the percentage inline. Written out in on_draw_2_body,
// the double argument makes LTCG realign it early enough to pad the frame of
// AsciiInf::create_number (0x4082b0); a plain inline helper keeps the double
// in its own call graph node, as for CStreamingSound::get_play_time.
static inline void draw_percentage(Float3 *pos, f32 percentage)
{
    g_AsciiManager->create_stringf(pos, "%3.1f%%", (double)percentage);
}

// The HUD's text: the stage clear bonus, the chapter result, the spell
// card's capture time and record, the score, hiscore, next extend, bomb
// fragments, power, point item value and graze, the boss timer's
// hundredths and the season level.
// TODO: written for behaviour; the original aligns its frame to 64 bytes, and register allocation and the text-setting store order are not matched yet.
// FUNCTION: TH16 0x428e70
i32 Gui::on_draw_2_body()
{
    Float3 pos;
    AsciiInf *ascii;
    if (g_AnmManager->get_vm_with_id(overlay_ids[GUI_OVERLAY_STAGE_CLEAR_BONUS]) == NULL)
    {
        overlay_ids[GUI_OVERLAY_STAGE_CLEAR_BONUS].id = 0;
    }
    else
    {
        pos = Float3(224.0f, 200.0f, 0.0f);
        AnmVm *vm = get_vm_or_clear(overlay_ids[GUI_OVERLAY_STAGE_CLEAR_BONUS]);
        ascii = g_AsciiManager;
        ascii->color.a = vm->color_1.a;
        ascii->group = 2;
        ascii->font_id = 4;
        ascii->align_h = 0;
        ascii->align_v = 0;
        ascii->create_number(&pos, stage_clear_bonus);
        ascii = g_AsciiManager;
        ascii->color.a = 0xff;
        ascii->font_id = 0;
        ascii->group = 0;
        ascii->align_h = 1;
        ascii->align_v = 1;
    }
    if (g_AnmManager->get_vm_with_id(overlay_ids[GUI_OVERLAY_CHAPTER_RESULT]) == NULL)
    {
        overlay_ids[GUI_OVERLAY_CHAPTER_RESULT].id = 0;
    }
    else
    {
        AnmVm *vm = g_AnmManager->get_vm_with_id(overlay_ids[GUI_OVERLAY_CHAPTER_RESULT]);
        if (vm != NULL && (vm->flags_lo >> 1) & 1)
        {
            pos = Float3(300.0f, 226.0f, 0.0f);
            vm = get_vm_or_clear(overlay_ids[GUI_OVERLAY_CHAPTER_RESULT]);
            ascii = g_AsciiManager;
            ascii->color.a = vm->color_1.a;
            ascii->group = 2;
            ascii->font_id = 2;
            ascii->align_h = 2;
            ascii->align_v = 0;
            ascii->create_stringf(&pos, "%d", chapter_result_count);
            pos.x = 308.0f;
            pos.y = 246.0f;
            draw_percentage(&pos, chapter_percent);
            pos.x = 300.0f;
            pos.y = 266.0f;
            g_AsciiManager->create_stringf(&pos, "%3d", chapter_result_count_2);
            pos.y = 286.0f;
            g_AsciiManager->create_stringf(&pos, "%6d", chapter_bonus);
            pos.y = 296.0f;
            ascii = g_AsciiManager;
            ascii->color.d3d = 0xff8080ff;
            ascii->create_stringf(&pos, "  +%d", chapter_bonus / 50000 * 10);
            ascii = g_AsciiManager;
            ascii->color.a = 0xff;
            ascii->font_id = 0;
            ascii->group = 0;
            ascii->align_h = 1;
            ascii->align_v = 1;
            ascii->color.d3d = 0xffffffff;
        }
    }
    // Under the spell card bonus notice: the card's time (Spellcard::unk_90
    // frames) and, in grey, the time Spellcard::decode_time_code gives.
    ascii = g_AsciiManager;
    if (spell_bonus_shown != 0)
    {
        pos = Float3(224.0f, 144.0f, 0.0f);
        AnmVm *vm = g_AnmManager->get_vm_with_id(overlay_ids[GUI_OVERLAY_SPELL_BONUS_BACK]);
        if (vm == NULL)
        {
            overlay_ids[GUI_OVERLAY_SPELL_BONUS_BACK].id = 0;
        }
        if (g_Spellcard == NULL)
        {
            spell_bonus_shown = 0;
            g_AnmManager->delete_vm(overlay_ids[GUI_OVERLAY_SPELL_BONUS_BACK]);
            overlay_ids[GUI_OVERLAY_SPELL_BONUS_BACK].id = 0;
        }
        else if (vm == NULL)
        {
            spell_bonus_shown = 0;
        }
        else
        {
            ascii->color.a = vm->color_1.a;
            ascii->group = 2;
            ascii->font_id = 4;
            i32 seconds = g_Spellcard->unk_90 / 60;
            ascii->create_stringf(&pos, "%3d.", seconds >= 1000 ? 999 : seconds);
            pos.x = 268.0f;
            pos.y = 150.0f;
            ascii = g_AsciiManager;
            ascii->scale.x = 0.6f;
            ascii->scale.y = 0.6f;
            i32 frames = g_Spellcard->unk_90;
            ascii->create_stringf(&pos, "%.2ds", frames % 60 * 100 / 60);
            ascii = g_AsciiManager;
            pos.x = 224.0f;
            pos.y = 160.0f;
            ascii->color.d3d = 0xff808080;
            ascii->color.a = vm->color_1.a;
            ascii->scale.x = 1.0f;
            ascii->scale.y = 1.0f;
            i32 time_seconds;
            i32 time_hundredths;
            g_Spellcard->decode_time_code(&time_seconds, &time_hundredths);
            ascii->create_stringf(&pos, "%3d.", time_seconds);
            ascii = g_AsciiManager;
            pos.x = 268.0f;
            pos.y = 166.0f;
            ascii->scale.x = 0.6f;
            ascii->scale.y = 0.6f;
            ascii->create_stringf(&pos, "%.2ds", time_hundredths);
            ascii = g_AsciiManager;
            ascii->color.a = 0xff;
            ascii->scale.x = 1.0f;
            ascii->scale.y = 1.0f;
            ascii->font_id = 0;
            ascii->group = 0;
            ascii->color.d3d = 0xffffffff;
        }
    }

    // Hiscore and score.
    ascii->color.d3d = 0xff808080;
    ascii->font_id = 4;
    pos = Float3(620.0f, 42.0f, 0.0f);
    ascii->color.a = life_counter_vms[0]->color_1.a;
    ascii->align_h = 2;
    ascii->align_v = 1;
    ascii->create_number_with_digit(&pos, g_Globals.hiscore, g_Globals.hiscore_continues);
    ascii = g_AsciiManager;
    pos.y = 64.0f;
    ascii->color.d3d = 0xffffffff;
    ascii->color.a = life_counter_vms[0]->color_1.a;
    ascii->create_number_with_digit(&pos, current_score, g_Globals.continues_used);

    // The next extend.
    ascii = g_AsciiManager;
    ascii->align_h = 1;
    ascii->align_v = 1;
    ascii->color.d3d = 0xff80c0f0;
    ascii->align_h = 2;
    ascii->align_v = 1;
    pos = Float3(618.0f, 118.0f, 0.0f);
    ascii->color.a = life_counter_vms[0]->color_1.a;
    ascii->scale.x = 0.6f;
    ascii->scale.y = 0.6f;
    if ((u32)get_score_extend_quota() < 900000000)
    {
        g_AsciiManager->create_number(&pos, get_score_extend_quota() * 10);
    }

    // Bomb fragments.
    ascii = g_AsciiManager;
    ascii->color.d3d = 0xffffffff;
    ascii->align_h = 1;
    ascii->align_v = 1;
    pos = Float3(576.0f, 158.0f, 0.0f);
    ascii->color.a = life_counter_vms[0]->color_1.a;
    ascii->scale.x = 0.6f;
    ascii->scale.y = 0.6f;
    ascii->create_stringf(&pos, "%3d", g_Globals.bomb_fragments);
    pos.x = 597.0f;
    g_AsciiManager->create_stringf(&pos, "/%d", 5);

    // Power.
    ascii = g_AsciiManager;
    pos = Float3(540.0f, 182.0f, 0.0f);
    ascii->color.d3d = 0xffff8030;
    ascii->scale.x = 1.0f;
    ascii->scale.y = 1.0f;
    ascii->color.a = life_counter_vms[0]->color_1.a;
    ascii->create_stringf(&pos, "%d.", g_Globals.power / g_Globals.power_per_level);
    ascii = g_AsciiManager;
    pos.x = 560.0f;
    pos.y += 7.0f;
    ascii->scale.x = 0.6f;
    ascii->scale.y = 0.6f;
    ascii->create_stringf(&pos, "%.2d", g_Globals.power % g_Globals.power_per_level * 100 / g_Globals.power_per_level);
    ascii = g_AsciiManager;
    pos.x = 574.0f;
    pos.y -= 7.0f;
    ascii->scale.x = 1.0f;
    ascii->scale.y = 1.0f;
    ascii->create_stringf(&pos, "/%d.", g_Globals.max_power / g_Globals.power_per_level);
    ascii = g_AsciiManager;
    pos.x = 606.0f;
    pos.y += 7.0f;
    ascii->scale.x = 0.6f;
    ascii->scale.y = 0.6f;
    ascii->create_stringf(&pos, "00");

    // PIV and graze.
    ascii = g_AsciiManager;
    ascii->color.d3d = 0xff40c0ff;
    ascii->scale.x = 1.0f;
    ascii->scale.y = 1.0f;
    ascii->align_h = 2;
    ascii->align_v = 1;
    pos = Float3(620.0f, 204.0f, 0.0f);
    ascii->color.a = life_counter_vms[0]->color_1.a;
    i32 piv = g_Globals.piv / 100;
    ascii->create_number(&pos, piv - piv % 10);
    ascii = g_AsciiManager;
    ascii->color.d3d = 0xffffffff;
    pos.y = 226.0f;
    ascii->color.a = life_counter_vms[0]->color_1.a;
    ascii->create_number(&pos, g_Globals.graze);
    ascii = g_AsciiManager;
    ascii->color.d3d = 0xffffffff;
    ascii->align_h = 1;
    ascii->align_v = 1;
    ascii->scale.x = 1.0f;
    ascii->scale.y = 1.0f;
    ascii->color.a = 0xff;
    ascii->font_id = 0;
    ascii->group = 0;

    // The boss timer's dot and hundredths, next to its digits.
    if (g_EnemyManager != NULL && boss_timer_seconds >= 0 && g_EnemyManager->get_boss(0) != NULL &&
        !g_EnemyManager->inner.boss_bit && msg == NULL && !(*(u32 *)&g_GameThread->flags & GAME_THREAD_MUSIC_RESTART))
    {
        AnmVm *vm = boss_timer_tens_vm;
        f32 x = vm->pos.x + 16.0f;
        f32 y = vm->pos.y - 7.0f;
        pos = Float3(x, y, 0.0f);
        ascii->color = vm->color_1;
        ascii->font_id = 4;
        ascii->group = 2;
        ascii->create_stringf(&pos, ".");
        ascii = g_AsciiManager;
        pos.x = x + 8.0f;
        pos.y = y + 6.0f;
        ascii->scale.x = 0.6f;
        ascii->scale.y = 0.6f;
        ascii->create_stringf(&pos, "%.2d", boss_timer_hundredths);
        ascii = g_AsciiManager;
        ascii->scale.x = 1.0f;
        ascii->scale.y = 1.0f;
        ascii->color.d3d = 0xffffffff;
        ascii->group = 0;
        ascii->font_id = 0;
    }

    // The season level, faint while the gauge is out of the player's way.
    D3DCOLOR level_colors[7] = {0x60606060, 0xa0b0b080, 0xb0b8b880, 0xc0c0c080, 0xd0d0d080, 0xe0e0e080, 0xffffff30};
    pos = Float3(-132.0f, 446.0f, 0.0f);
    ascii->color.d3d = level_colors[g_Globals.season_level()];
    if (hud_flags & GUI_SEASON_GAUGE_AWAY)
    {
        ascii->color.a = 0x40;
    }
    ascii->group = 1;
    ascii->font_id = 2;
    ascii->align_h = 0;
    ascii->align_v = 2;
    ascii->create_number(&pos, g_Globals.season_level());
    ascii = g_AsciiManager;
    ascii->color.d3d = 0xffffffff;
    ascii->color.a = 0xff;
    ascii->font_id = 0;
    ascii->group = 0;
    ascii->align_h = 1;
    ascii->align_v = 1;
    return UPDATE_FUNC_CONTINUE;
}
