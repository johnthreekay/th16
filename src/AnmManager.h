#pragma once

#include "decomp.h"
#include "types.h"

// Handle of a running AnmVm, 0 when none.
struct AnmId
{
    u32 id;

    AnmId()
    {
        id = 0;
    }
};

// One loaded ANM file.
struct AnmLoaded
{
    u8 unk_0[0x13c];

    // Frees what the file owns (ExpHP: AnmLoaded::destructor). Not a real
    // destructor: callers reload the pointer for the delete that follows,
    // and that delete has no null check of its own.
    void release();
};

// Loads and runs every ANM file. Only the parts this code uses so far.
struct AnmManager
{
    u8 unk_0[0x184f4f0];
    // Indexed by the slot given to preload_anm.
    AnmLoaded *loaded_anms[0x20];

    // A member reaching the manager through g_AnmManager in the original,
    // with this dropped by LTCG; static __stdcall gives the same call shape.
    static AnmLoaded *__stdcall preload_anm(i32 slot, const char *name);
};

extern AnmManager *g_AnmManager;
