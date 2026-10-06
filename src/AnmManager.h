#pragma once

#include "AnmVm.h"
#include "types.h"

// Only the parts unit 1 needs so far. Layout from ExpHP's th-re-data
// (zAnmManager).
struct AnmManager
{
    u8 unk_0[0x184f4f0];
    // Files loaded by preload_anm, by slot.
    AnmLoaded *preloaded[31];

    // Loads an .anm file into a slot. Callers never pass this; LTCG
    // dropped it (the original reaches g_AnmManager directly).
    static AnmLoaded *__stdcall preload_anm(i32 slot, const char *name);
    // NULL if the VM is gone.
    AnmVm *get_vm_with_id(AnmId id);

    void release_preloaded(i32 slot)
    {
        if (preloaded[slot] != NULL)
        {
            delete preloaded[slot];
            preloaded[slot] = NULL;
        }
    }
};

extern AnmManager *g_AnmManager;
