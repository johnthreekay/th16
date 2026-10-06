// Stand-in callers for unit 8a (0x45d420-0x469000).
#include "../AnmManager.h"
#include "../AnmVm.h"

// Like the instruction interpreter in AnmVm::run (0x45f980), which reads
// float arguments through get_float_var.
f32 harness_anm_get_float_var(AnmVm *vm, f32 *args)
{
    return vm->get_float_var(args[0]) + vm->get_float_var(args[1]);
}
