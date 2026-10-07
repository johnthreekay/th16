// Stand-in callers for wave 3, range C (0x42b480-0x43dc30), for functions
// whose shape depends on how they are called.
#include "../EnemyManager.h"
#include "../Fog.h"
#include "../Gui.h"
#include "../HelpManual.h"
#include "../Player.h"
#include "../Supervisor.h"

void w3c_stub_sink(void *p);

// The window code passes the addresses of the globals just before these to
// SystemParametersInfo (0x45a70d). In ZUN's code they are probably all
// members of one struct, so stores through pointers may alias the screen
// size: Supervisor::setup_cameras reloads it after every store.
void harness_w3c_expose_screen_globals()
{
    w3c_stub_sink(&g_resolution_x);
    w3c_stub_sink(&g_resolution_y);
    w3c_stub_sink(&g_unk_4d9d1c);
}

// The HUD and title code read these;
// without a reader LTCG drops the stores.
i32 harness_w3c_read_hud_origin()
{
    return g_arcade_hud_origin_x + g_arcade_hud_origin_y + g_unk_4a6f1c;
}

// Like Fog's initialize at 0x418d8a.
void harness_w3c_fog_create_vms(Fog *fog, i32 count)
{
    for (i32 i = 0; i < fog->vm_count - 1; i++)
    {
        fog->vm_ids[i] = g_Supervisor.create_fog_vm(count, 0x3b);
    }
}

// Like HelpManual::on_tick at 0x42eb62, 0x42ed5c and 0x42eec3.
void harness_w3c_help_manual_pages(HelpManual *manual, i32 count)
{
    D3DXVECTOR3 pos(0.0f, 0.0f, 0.0f);
    for (i32 i = 0; i < count; i++)
    {
        manual->page_vms[i] = manual->help_anm->create_ui_vm(i, &pos, 0);
        pos.x += 640.0f;
    }
    manual->page_vms[9] = manual->help_anm->create_ui_vm(9, &pos, 0);
}

// Like Spellcard::spell_end at 0x418377 and 0x418408 (capture bonus or
// bonus failed).
void harness_w3c_gui_spell_bonus(i32 captured, i32 bonus)
{
    if (captured)
    {
        g_Gui->sub_42bcf0(bonus, 0);
    }
    else
    {
        g_Gui->sub_42bcf0(0, 1);
    }
}

// Like ItemManager's on_tick at 0x42fded.
void harness_w3c_gui_notice_4()
{
    g_Gui->sub_42bcf0(0, 4);
}

// Like GameThread::thread_start at 0x42d0a9.
void harness_w3c_enemy_manager_reset()
{
    g_EnemyManager->reset_for_stage(0);
}

// Like the game start code at 0x42e171.
void harness_w3c_player_resume_options()
{
    g_Player->resume_options();
}

Supervisor *w3c_stub_supervisor();

// Like the startup code at 0x459ac8. LTCG kept this there though it is
// always g_Supervisor; an opaque pointer keeps ours from folding it.
int harness_w3c_load_game_config()
{
    return w3c_stub_supervisor()->load_game_config("th16.cfg");
}
