// Stand-ins compiled with /GL for functions that wave 4 range F
// (0x450000-0x4748e0) calls but has not decompiled, where LTCG has to see a
// body (folded arguments). Each forwards to an opaque stub.
#include "../MainMenu.h"
#include "../ReplayManager.h"

int w4f_placeholder_sink(void *object, int value);

// STUB: TH16 0x4560b0
DECOMP_NOINLINE void TitleInf::load_spell_list(i32 stage, i32 row, i32 *ids, i32 unused)
{
    w4f_placeholder_sink(this, w4f_placeholder_sink(ids, stage + row + unused));
}

