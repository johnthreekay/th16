#pragma once

#include <stdlib.h>

#include "Camera.h"
#include "decomp.h"
#include "types.h"

// Only the parts unit 6 (Supervisor, Player, menus) needs so far. Layouts
// from ExpHP's th-re-data.

typedef i32 AnmId;

// One running ANM script.
struct AnmVm
{
    u8 unk_0[0x28];
    i32 instr_offset;
    u8 unk_2c[0x520 - 0x2c];
    D3DCOLOR color_1;
    u8 unk_524[0x538 - 0x524];
    AnmId id;
    u8 unk_53c[0x5b8 - 0x53c];
    // Allocated by ANM instruction 508.
    void *ins_508_extra_data;
    u32 ins_508_extra_data_size;
    u8 unk_5c0[0x5fc - 0x5c0];

    AnmVm();
    ~AnmVm()
    {
        if (ins_508_extra_data != NULL)
        {
            free(ins_508_extra_data);
        }
        ins_508_extra_data = NULL;
        ins_508_extra_data_size = 0;
        id = 0;
        instr_offset = -1;
    }
};

struct AnmLoaded;

struct AnmManager
{
    u8 unk_0[0xd0];
    // Copied from the active camera by Supervisor::swap_transform_matrices.
    Float2 camera_unk_fc;

    void flush_sprites();
    void draw_vm(AnmVm *vm);
    AnmVm *get_vm_with_id(AnmId id);

    // Members that do not use this; LTCG dropped it (ret N, no ecx).
    static void __stdcall interrupt_tree(AnmId id, i32 interrupt);
    static AnmLoaded *__stdcall preload_anm(i32 slot, const char *path);
};

extern AnmManager *g_AnmManager;
