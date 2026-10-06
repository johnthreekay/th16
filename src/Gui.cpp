#include <stdlib.h>
#include <string.h>

#include "AnmManager.h"
#include "CriticalSections.h"
#include "Ecl.h"
#include "FileSystem.h"
#include "GameErrorContext.h"
#include "Globals.h"
#include "Gui.h"
#include "StageData.h"
#include "UpdateFunc.h"

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
