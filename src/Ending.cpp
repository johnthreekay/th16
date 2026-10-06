#include <stdlib.h>
#include <string.h>

#include "AnmManager.h"
#include "CriticalSections.h"
#include "Ending.h"
#include "Supervisor.h"
#include "UpdateFunc.h"

// GLOBAL: TH16 0x4a6dbc
Ending *g_Ending;

// Cleared when the ending goes away; also used by the screen effects.
extern i32 g_unk_4c0f40;

// Raw button state as read from the devices, and how many frames the bit
// at 0x114 / 4 has been held. ExpHP: HARDWARE_INPUT.
extern u32 g_hardware_input;
extern u32 g_hardware_input_held_4a51c4;

#define BUTTON_SHOT (1 << 0)
#define BUTTON_SKIP (1 << 9)

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
        c->timer_4++;
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
        g_Supervisor.wanted_gamemode = g_Supervisor.flags & SUPERVISOR_FLAG_2000 ? 2 : 16;
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
