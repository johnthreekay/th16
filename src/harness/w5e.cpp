// A stand-in caller for AnmVm::get_own_transformed_pos.
//
// Not ZUN's code and never run: it gives link-time code generation calls
// or address uses the decompiled code does not have, so that it compiles
// the real functions as in the original (see docs/workflow.md, "Placeholders and
// stand-in callers"). The harness files are split the way the work was;
// regrouping them changes LTCG's choices for unrelated functions.
#include "../AnmManager.h"

// Takes the address of a local double out of LTCG's view
// (src/stub/Opaque.cpp), which makes the caller's frame 8-byte aligned.
void w4b_opaque_double(double *value);

// A caller of AnmVm::get_own_transformed_pos (0x406c40) with an 8-byte
// aligned frame, like several of its original callers. Without it LTCG
// compiles get_own_transformed_pos and the ANM drawing code around it
// (0x406a70, 0x465c40, 0x468490, ...) differently.
f32 harness_w5e_transformed_pos(AnmVm *vm)
{
    double aligned = 0.0;
    w4b_opaque_double(&aligned);
    Float3 pos;
    vm->get_own_transformed_pos(&pos);
    return pos.x;
}
