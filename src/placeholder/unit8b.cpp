// Placeholders compiled with /GL for functions other units own, where LTCG
// has to see a body (see src/placeholder/unit2.cpp). Each forwards to an
// opaque stub.
#include "../Enemy.h"

int unit8b_placeholder_sink(void *object, int value);

// EnemyInf's overrides of the ECL VM hooks. Without them LTCG would
// devirtualize the SptInf calls in EclRunContext's argument getters.

// STUB: TH16 0x41dca0
int EnemyInf::run_over_300()
{
    return unit8b_placeholder_sink(this, 0);
}

// STUB: TH16 0x423810
int EnemyInf::get_int_global(int var)
{
    return unit8b_placeholder_sink(this, var);
}

// STUB: TH16 0x424110
f32 EnemyInf::get_float_global(int var)
{
    return (f32)unit8b_placeholder_sink(this, var);
}

// STUB: TH16 0x472030
HARNESS_CALLED i32 EclRunContext::ecl_run(f32 speed)
{
    return unit8b_placeholder_sink(this, (int)(speed * 2.0f));
}
