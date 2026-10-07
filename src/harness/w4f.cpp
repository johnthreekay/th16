// Stand-in callers for wave 4 range F (0x450000-0x4748e0) functions whose
// shape depends on code that is not decompiled yet.
#include "../MainMenu.h"

extern i32 g_spell_practice_last_stage;
extern i32 g_spell_practice_last_row;
extern i32 g_spell_practice_last_index;
extern i32 g_practice_last_stage;

// The practice menus (0x450f10, 0x455495, 0x455a09, 0x456a7d) start from
// the last choice; without a reader LTCG drops the stores.
i32 harness_w4f_practice_last(i32 stage)
{
    g_practice_last_stage = stage;
    return g_spell_practice_last_stage + g_spell_practice_last_row + g_spell_practice_last_index +
           g_practice_last_stage;
}
