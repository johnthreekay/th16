// Stand-in callers for wave 3, range A (0x401000-0x4190b0).
#include "../AnmManager.h"
#include "../EffectManager.h"
#include "../Fog.h"
#include "../Supervisor.h"

// Like the main menu (0x450d75, 0x4512f2, ...), which starts its cursor
// effect this way.
i32 harness_menu_ui_effect()
{
    return g_EffectManager->create_ui_effect(0, NULL, NULL).id + g_EffectManager->create_ui_effect(0, NULL, NULL).id;
}

// Like the pause menu (0x43ef5b, 0x43f159, ...) and the HUD (0x426dc0).
i32 harness_create_ui_vm(i32 script)
{
    return g_Supervisor.text_anm->create_ui_vm(0x34, 0).id + g_Supervisor.text_anm->create_ui_vm(script, 0).id;
}

// Like StageInner::run_std (0x40bad2) and ECL (0x42275d), which create
// fogs with 7 and 17 points per strip.
Fog *harness_new_fog(i32 enemy)
{
    if (enemy)
    {
        return new Fog(0, 0x11, 0);
    }
    return new Fog(0, 7, 0);
}

// Like EnemyData::update_fog (0x41cc94) and StageInner::step_fog (0x40c527).
void harness_fog_set_rect(Fog *fog, f32 x, f32 y, f32 r)
{
    fog->set_rect(x - r - 16.0f, y - r - 16.0f, r + r + 40.0f, r + r + 40.0f);
    fog->set_rect(x, y, r, r * 2.0f);
}

// The resolution and game-area origin have their addresses taken elsewhere
// in the game (the window setup code), so stores through pointers may change
// them: Fog::set_rect reloads them inside its loops.
i32 *harness_screen_metric_ptr(i32 which)
{
    switch (which)
    {
    case 0:
        return &g_resolution_x;
    case 1:
        return &g_resolution_y;
    case 2:
        return &g_game_2d_origin_x;
    default:
        return &g_game_2d_origin_y;
    }
}
