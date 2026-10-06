#pragma once

#include "AnmVm.h"
#include "types.h"

// The fog effect an enemy can carry: a ring of ANM VMs (ExpHP: zFog). Its
// vertex buffers come from malloc.
struct Fog
{
    i32 vm_count;
    i32 unk_4;
    AnmId main_vm;
    AnmId *vm_ids;
    AnmVm **vms;
    void *buffer_14;
    void *buffer_18;

    // 0x409550
    ~Fog();
};
