// Placeholders compiled with /GL for functions other units own, where LTCG
// has to see a body (see src/placeholder/unit2.cpp). Each forwards to an
// opaque stub.
#include "../Enemy.h"

int unit8b_placeholder_sink(void *object, int value);

// EnemyInf's overrides of the ECL VM hooks. Without them LTCG would
// devirtualize the SptInf calls in EclRunContext's argument getters.

// STUB: TH16 0x472030
HARNESS_CALLED i32 EclRunContext::ecl_run(f32 speed)
{
    return unit8b_placeholder_sink(this, (int)(speed * 2.0f));
}
