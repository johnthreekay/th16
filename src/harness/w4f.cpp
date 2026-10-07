// Stand-in callers for wave 4 range F (0x450000-0x4748e0) functions whose
// shape depends on code that is not decompiled yet.
#include "../AnmManager.h"
#include "../MainMenu.h"
#include "../ReplayManager.h"

extern i32 g_spell_practice_last_stage;
extern i32 g_spell_practice_last_row;
extern i32 g_spell_practice_last_index;
extern i32 g_practice_last_stage;
extern i32 g_unk_4a5bf8;

// The practice menus (0x450f10, 0x455495, 0x455a09, 0x456a7d) start from
// the last choice; without a reader LTCG drops the stores.
i32 harness_w4f_practice_last(i32 stage)
{
    g_practice_last_stage = stage;
    return g_spell_practice_last_stage + g_spell_practice_last_row + g_spell_practice_last_index +
           g_practice_last_stage;
}

// with this in ecx (the second object keeps LTCG from folding it).
i32 harness_w4f_circle(AnmManager *other, AnmVm *vm, f32 x, f32 y, f32 radius)
{
    return g_AnmManager->draw_circle_outline(x, y, radius, vm->rotation.z, vm->int_vars[0], vm->color_1.d3d) +
           other->draw_circle_outline(x, y, radius, vm->rotation.z, vm->int_vars[0], vm->color_1.d3d);
}

// The game thread reads the number key picked with a practice stage.
i32 harness_w4f_practice_key()
{
    return g_unk_4a5bf8;
}

// The pause menu (0x4403f1) saves replays too, with 1 as the last argument.
i32 harness_w4f_save_replay(const char *path, const char *name)
{
    return g_ReplayManager->save(path, name, 0, 1);
}
