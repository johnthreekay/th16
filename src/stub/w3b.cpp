// Placeholders for functions wave 3 range B (0x4190b0-0x42b480) calls but
// that are not decompiled yet. Compiled without /GL, so calls into them stay
// opaque.
#include "../AnmManager.h"
#include "../Enemy.h"
#include "../Interp.h"

// Opaque work for the /GL placeholders in src/placeholder/w3b.cpp.
int w3b_placeholder_sink(void *object, int value)
{
    return value;
}

// STUB: TH16 0x41c330
int EnemyData::step_logic()
{
    return 0;
}

// STUB: TH16 0x41cbd0
void EnemyData::update_fog()
{
}

// STUB: TH16 0x41dcb0
int EnemyData::ecl_run_over_300()
{
    return 0;
}
