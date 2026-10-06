// Stand-in callers for wave 3, range A (0x401000-0x4190b0).
#include "../AnmManager.h"
#include "../EffectManager.h"
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
