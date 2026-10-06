#pragma once

#include "decomp.h"
#include "types.h"

// One running ANM script. Only the size and the constructor are known to
// this code so far (layout: ExpHP's zAnmVm, 0x5fc bytes).
struct AnmVm
{
    u8 data[0x5fc];

    // LTCG saw that the constructor cannot throw, so callers have no EH
    // cleanup for it. Say so while it is still a stub.
    __declspec(nothrow) AnmVm();
    __declspec(nothrow) ~AnmVm();
};
