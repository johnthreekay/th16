// Stand-in callers for the second pass over 0x411860-0x42cb00, for
// functions whose shape depends on how they are called.
#include "../AnmVm.h"
#include "../PosVel.h"

// Like the ECL anm scale instruction at 0x42354a.
void harness_anm_vm_scale_to(AnmVm *vm, i32 time, i32 method, f32 x, f32 y)
{
    vm->scale_to(time, method, x, y);
}

// Like the ECL movement instruction at 0x41fb58.
void harness_posvel_set_ellipse_angle(PosVel *pv, f32 angle)
{
    if (angle > -999999.0)
    {
        pv->set_ellipse_angle(angle);
    }
}
