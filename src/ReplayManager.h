#pragma once

#include "types.h"

// Records and plays back replays. Only what unit 6 needs so far; layout
// from ExpHP's th-re-data (zReplayManager).
struct ReplayManager
{
    u8 unk_0[0x31c];

    ~ReplayManager();
};

extern ReplayManager *g_ReplayManager;
