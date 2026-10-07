// Placeholders for functions wave 5 range B (0x420000-0x440000) calls but
// that are not decompiled yet. Compiled without /GL, so they stay opaque
// calls with standard conventions.
#include "../types.h"

int w5b_placeholder_sink(void *object, int value)
{
    return value + (int)object;
}
