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

// AnmManager::draw_vm (0x468b1d) draws circle outlines for render mode 18,
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

// AnmManager::draw_vm (0x468ae2) draws rings for render mode 19.
i32 harness_w4f_ring(AnmManager *other, AnmVm *vm, f32 x, f32 y, f32 radius)
{
    return g_AnmManager->draw_ring(x, y, radius, vm->scale.x, vm->rotation.z, vm->int_vars[0], vm->color_1.d3d) +
           other->draw_ring(x, y, radius, vm->scale.y, vm->rotation.z, vm->int_vars[1], vm->color_1.d3d);
}

// AnmManager::draw_vm (0x468b5e) draws filled circles for render mode 17.
i32 harness_w4f_circle_fill(AnmManager *other, AnmVm *vm, f32 x, f32 y, f32 radius)
{
    return g_AnmManager->draw_circle(x, y, radius, vm->rotation.z, vm->int_vars[0], vm->color_1.d3d,
                                     vm->color_2.d3d) +
           other->draw_circle(x, y, radius, vm->rotation.z, vm->int_vars[1], vm->color_2.d3d, vm->color_1.d3d);
}

// AnmManager::draw_vm (0x46886c) draws lines for render mode 16.
i32 harness_w4f_line(AnmManager *other, AnmVm *vm, f32 x, f32 y, f32 length)
{
    return g_AnmManager->draw_line(x, y, length, vm->rotation.z, vm->color_1.d3d, vm->color_2.d3d,
                                   (vm->flags_hi >> 21) & 3, 0) +
           other->draw_line(x, y, length, vm->rotation.x, vm->color_2.d3d, vm->color_1.d3d, vm->int_vars[0], 0);
}

// AnmManager::draw_vm (0x4688b8) draws rectangles for render mode 17.
i32 harness_w4f_rect(AnmManager *other, AnmVm *vm, f32 x, f32 y, f32 width)
{
    return g_AnmManager->draw_rect(x, y, width, vm->scale.y, vm->rotation.z, vm->color_1.d3d, vm->color_1.d3d,
                                   (vm->flags_hi >> 21) & 3, (vm->flags_hi >> 23) & 3) +
           other->draw_rect(x, y, width, vm->scale.x, vm->rotation.x, vm->color_2.d3d, vm->color_1.d3d,
                            vm->int_vars[0], vm->int_vars[1]);
}

// AnmManager::draw_vm (0x4689a4) draws bordered rectangles for render mode 20.
i32 harness_w4f_rect_bordered(AnmManager *other, AnmVm *vm, f32 x, f32 y, f32 width)
{
    return g_AnmManager->draw_rect_bordered(x, y, width, vm->scale.y, vm->rotation.z, vm->color_1.d3d,
                                            vm->color_2.d3d, (vm->flags_hi >> 21) & 3, (vm->flags_hi >> 23) & 3) +
           other->draw_rect_bordered(x, y, width, vm->scale.x, vm->rotation.x, vm->color_2.d3d, vm->color_1.d3d,
                                     vm->int_vars[0], vm->int_vars[1]);
}

// AnmManager::draw_vm (0x468919) draws rectangle outlines for render mode 18.
i32 harness_w4f_rect_outline(AnmManager *other, AnmVm *vm, f32 x, f32 y, f32 width)
{
    return g_AnmManager->draw_rect_outline(x, y, width, vm->scale.y, vm->rotation.z, vm->color_1.d3d,
                                           vm->color_2.d3d, (vm->flags_hi >> 21) & 3, (vm->flags_hi >> 23) & 3) +
           other->draw_rect_outline(x, y, width, vm->scale.x, vm->rotation.x, vm->color_2.d3d, vm->color_1.d3d,
                                    vm->int_vars[0], vm->int_vars[1]);
}
