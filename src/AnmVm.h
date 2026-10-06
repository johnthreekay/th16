#pragma once

#include <stdlib.h>

#include "decomp.h"
#include "types.h"

// One running ANM script. Only the size and the parts unit 5 touches are
// known to this code so far (layout: ExpHP's zAnmVm, 0x5fc bytes).
struct AnmVm
{
    u8 unk_0[0x28];
    i32 unk_28;
    u8 unk_2c[0x538 - 0x2c];
    i32 unk_538;
    u8 unk_53c[0x5b8 - 0x53c];
    void *unk_5b8;
    i32 unk_5bc;
    u8 unk_5c0[0x5fc - 0x5c0];

    // Callers compile with EH cleanup for this, which LTCG then removes
    // because it sees the body cannot throw. The stand-in body lives in
    // src/harness/ (built with /GL) until 0x4093f0 is decompiled.
    AnmVm();

    // Inlined into every owner's destructor; the original also has an
    // out-of-line copy at 0x4093b0.
    ~AnmVm()
    {
        if (unk_5b8 != NULL)
        {
            free(unk_5b8);
        }
        unk_5b8 = NULL;
        unk_5bc = 0;
        unk_538 = 0;
        unk_28 = -1;
    }
};
