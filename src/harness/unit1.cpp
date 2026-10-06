// Stand-in callers for unit 1 (0x402e70-0x409490).
#include "../AnmVm.h"
#include "../CriticalSections.h"
#include "../Interp.h"
#include "../ZunMath.h"

// ECL and the per-object updates write g_GameSpeed all the time. Without a
// write LTCG would treat it as the constant 1.0.
void harness_set_game_speed(f32 speed)
{
    g_GameSpeed = speed;
}

// Like the file loader around 0x45ddbb, which leaves CS_FILE this way.
void harness_leave_cs(int i)
{
    g_CriticalSections.leave(CS_FILE);
    g_CriticalSections.leave(i);
}

// Like the interpolators' step functions (0x406e10 and others).
f32 harness_interp_ratio(i32 mode, i32 time, i32 end_time)
{
    return interp_ratio(mode, (f32)time, (f32)end_time);
}

// Like the VM update loop around 0x45f980.
f32 harness_anm_vm_slowdown(AnmVm *vm)
{
    return vm->get_slowdown_factor() * 2.0f;
}
