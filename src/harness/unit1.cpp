// An escaping address of a screen global (see s6.cpp) and a stand-in caller
// for ECL's random angles.
//
// Not ZUN's code and never run: it gives link-time code generation calls
// or address uses the decompiled code does not have, so that it compiles
// the real functions as in the original (see README.md, "Placeholders and
// stand-in callers"). The harness files are split the way the work was;
// regrouping them changes LTCG's choices for unrelated functions.
#include "../AnmVm.h"
#include "../AsciiManager.h"
#include "../CriticalSections.h"
#include "../Supervisor.h"
#include "../Interp.h"
#include "../Rng.h"
#include "../ZunMath.h"

// AsciiInf::create_string (0x408140) and draw_string (0x408650) reload
// the screen scale.
f32 *harness_screen_coord_scale_ptr()
{
    return &g_screen_coord_scale;
}

// Stands for ECL's random movement (0x41ffed and three more calls), the
// only callers of randf_neg_1_to_1_times_pi (0x406320). With just those in
// view, all on g_replay_safe_rng, the function compiles one instruction
// longer than the original; a call on another Rng keeps it as it was.
f32 harness_rand_angle(Rng *rng)
{
    return rng->randf_neg_1_to_1_times_pi() / 3.0f;
}
