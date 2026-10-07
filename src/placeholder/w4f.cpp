// Stand-ins compiled with /GL for functions that wave 4 range F
// (0x450000-0x4748e0) calls but has not decompiled, where LTCG has to see a
// body (folded arguments). Each forwards to an opaque stub.
#include "../MainMenu.h"
#include "../ReplayManager.h"

int w4f_placeholder_sink(void *object, int value);

// STUB: TH16 0x448400
HARNESS_CALLED i32 ReplayManager::save(const char *path, const char *name, i32 unused, i32 unk_4)
{
    return w4f_placeholder_sink((void *)path, w4f_placeholder_sink((void *)name, unused + unk_4));
}
