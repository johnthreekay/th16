// Stand-in callers for wave 3, range C (0x42b480-0x43dc30), for functions
// whose shape depends on how they are called.
#include "../EnemyManager.h"
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
    w3c_stub_sink(&g_window_flags);
}

// The HUD and title code read these;
// without a reader LTCG drops the stores.
i32 harness_w3c_read_hud_origin()
{
    return g_arcade_hud_origin_x + g_arcade_hud_origin_y + g_title_return_point;
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

// Like the game start code at 0x42e171.
void harness_w3c_player_resume_options()
{
    g_Player->resume_options();
}

Supervisor *w3c_stub_supervisor();

