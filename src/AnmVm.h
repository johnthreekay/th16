#pragma once

#include <stdlib.h>

#include <d3d9.h>

#include "ZunMath.h"
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

enum AnmVmFlagsHi
{
    ANM_VM_CREATED_BY_GAME = 1 << 10,
    ANM_VM_NO_PARENT_POS = 1 << 16,
    ANM_VM_LAYER_SET = 1 << 18,
    ANM_VM_LAYER_UI = 1 << 19,
    ANM_VM_ROTATE_WITH_PARENT = 1 << 23,
};

// One running ANM script. Only the parts decompiled code uses so far
// (layout: ExpHP's zAnmVm, 0x5fc bytes).
struct AnmVm
{
    u8 unk_0[0x18];
    u32 layer;
    i32 anm_loaded_index;
    i32 sprite_id;
    i32 script_id;
    i32 instr_offset;
    Float3 pos;
    Float3 rotation;
    u8 unk_44[0x50 - 0x44];
    Float2 scale;
    u8 unk_58[0x4a0 - 0x58];
    i32 int_vars[4];
    f32 float_vars[4];
    u8 unk_4c0[0x4e0 - 0x4c0];
    Float3 pos_2;
    u8 unk_4ec[0x51c - 0x4ec];
    i32 mode_of_create_child;
    D3DCOLOR color_1;
    u8 unk_524[0x530 - 0x524];
    u32 flags_lo;
    u32 flags_hi;
    AnmId id;
    u8 unk_53c[0x5ac - 0x53c];
    AnmVm *parent;
    AnmVm *unk_5b0;
    u8 unk_5b4[0x5b8 - 0x5b4];
    // Allocated by ANM instruction 508.
    void *ins_508_extra_data;
    u32 ins_508_extra_data_size;
    u8 unk_5c0[0x5e0 - 0x5c0];
    // Position of the game object the VM belongs to.
    Float3 entity_pos;
    u8 unk_5ec[0x5fc - 0x5ec];

    // Callers compile with EH cleanup for this, which LTCG then removes
    // because it sees the body cannot throw. The stand-in body lives in
    // src/placeholder/ (built with /GL) until 0x4093f0 is decompiled.
    AnmVm();
    void wipe();
    // 0x40e490. Position including entity_pos and every parent's.
    Float3 world_pos();
    // 0x45f980
    void run();

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
