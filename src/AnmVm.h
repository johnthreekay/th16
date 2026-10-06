#pragma once

#include <stdlib.h>

#include <d3d9.h>

#include "decomp.h"
#include "types.h"

// Handle to a running VM, 0 when none. A class in ZUN's code: it is returned
// through a hidden pointer and constructed to 0 before its owner's memset.
struct AnmId
{
    i32 id;

    AnmId()
    {
        id = 0;
    }
};

// One running ANM script. Only the parts decompiled code uses so far
// (layout: ExpHP's zAnmVm, 0x5fc bytes).
struct AnmVm
{
    u8 unk_0[0x1c];
    i32 anm_loaded_index;
    i32 sprite_id;
    i32 script_id;
    i32 instr_offset;
    u8 unk_2c[0x520 - 0x2c];
    D3DCOLOR color_1;
    u8 unk_524[0x530 - 0x524];
    u32 flags_lo;
    u32 flags_hi;
    AnmId id;
    u8 unk_53c[0x5b8 - 0x53c];
    // Allocated by ANM instruction 508.
    void *ins_508_extra_data;
    u32 ins_508_extra_data_size;
    u8 unk_5c0[0x5fc - 0x5c0];

    // Callers compile with EH cleanup for this, which LTCG then removes
    // because it sees the body cannot throw. The stand-in body lives in
    // src/placeholder/ (built with /GL) until 0x4093f0 is decompiled.
    AnmVm();
    void wipe();

    // Inlined into every owner's destructor; the original also has an
    // out-of-line copy at 0x4093b0.
    ~AnmVm()
    {
        if (ins_508_extra_data != NULL)
        {
            free(ins_508_extra_data);
        }
        ins_508_extra_data = NULL;
        ins_508_extra_data_size = 0;
        id.id = 0;
        instr_offset = -1;
    }
};
