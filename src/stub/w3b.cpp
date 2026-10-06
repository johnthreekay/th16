// Placeholders for functions wave 3 range B (0x4190b0-0x42b480) calls but
// that are not decompiled yet. Compiled without /GL, so calls into them stay
// opaque.
#include "../AnmManager.h"

// Opaque work for the /GL placeholders in src/placeholder/w3b.cpp.
int w3b_placeholder_sink(void *object, int value)
{
    return value;
}

// STUB: TH16 0x46d720
void AnmManager::unload_anm_46d720(i32 slot)
{
    unload_anm(slot);
}
