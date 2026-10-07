// Stand-in callers for wave 5 range E (0x458000-0x4748e0: WinMain, the
// sound thread, AnmManager's drawing and setup) whose shape depends on how
// the rest of the game calls them.
#include "../AnmManager.h"

void w4b_opaque_double(double *value);

// Like the ANM on_draw callbacks (0x4073a0 and others in
// g_anm_on_draw_funcs), which call draw_vm themselves. Called through a
// pointer, they cannot have LTCG align their frames, so draw_vm realigns
// its own instead of its callers doing it.
static i32 __fastcall harness_w5e_on_draw(AnmVm *vm)
{
    g_AnmManager->draw_vm(vm);
    return 0;
}

AnmVmFunc harness_w5e_on_draw_ptr()
{
    return harness_w5e_on_draw;
}

// get_own_transformed_pos realigns its own frame in the original; an
// 8-byte aligned caller makes LTCG do the same here instead of realigning
// the callers of draw_vm.
f32 harness_w5e_transformed_pos(AnmVm *vm)
{
    double aligned = 0.0;
    w4b_opaque_double(&aligned);
    Float3 pos;
    vm->get_own_transformed_pos(&pos);
    return pos.x;
}

