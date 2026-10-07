// Stand-in callers for wave 4 range E (0x430000-0x450000: lasers, the
// supervisor, PauseMenu, Player, ReplayManager, the score file, the title
// screen) whose shape depends on how the rest of the game calls them.
#include "../Scorefile.h"

// The title screen's cheat key sequence (0x4529cf).
void harness_w4e_unlock_all()
{
    g_Scorefile->unlock_all();
}

#include "../PauseMenu.h"

// The pause menu's tick (0x43f980) and its openers (0x43f0f0, 0x43f350)
// switch states too.
void harness_w4e_pause_set_state(PauseMenu *menu, i32 state)
{
    menu->set_state(state);
}
