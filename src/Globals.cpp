#include "Globals.h"
#include "Gui.h"

// GLOBAL: TH16 0x4a5790
Globals g_Globals;

// FUNCTION: TH16 0x42e520
DECOMP_NOINLINE void Globals::reset_224()
{
    unk_224 = 0;
    unk_200 = 0;
    unk_204 = 0;
    unk_208 = 0;
    unk_20c = 0;
    unk_210 = 0;
    unk_214 = 0;
    unk_218 = 0;
    unk_21c = 0;
    unk_220 = 0;
}

// FUNCTION: TH16 0x42e590
void Globals::reset_for_new_game()
{
    point_items_collected = 0;
    power = 0;
    power_per_level = 1;
    bombs = 3;
    if (g_Gui != NULL)
    {
        g_Gui->update_bombs(g_Globals.bombs, g_Globals.bomb_fragments);
    }
    bomb_fragments = 0;
    life_fragments = 0;
    next_extend_index = 0;
    unk_d0 = 0;
    unk_d4 = 0;
    unk_d8 = 0;
    unk_dc = 0;
    item_spawn_count = 0;
    reset_224();
    miss_count = 0;
}
