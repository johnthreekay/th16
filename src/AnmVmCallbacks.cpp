// Callbacks that ANM VMs run through their index_of_on_* fields and the
// ins_508 table. Each effect kind has its own set; which kind is which is
// not known yet, so they are numbered as in ExpHP's names.
#include <string.h>

#include "AnmManager.h"
#include "AnmVm.h"

// ins_508 data of effect kind 2.
struct AnmEffect2Data
{
    AnmId vm_ids[200];
    u8 unk_320[0x1900 - 0x320];
    Float3 unk_1900;
    Float3 unk_190c;
    Float3 unk_1918;
    u8 unk_1924[4];
    ZunTimer timer;
};

// FUNCTION: TH16 0x405670
int __fastcall anm_effect_2_init(AnmVm *vm, i32 arg)
{
    vm->alloc_extra_data(sizeof(AnmEffect2Data));
    AnmEffect2Data *data = (AnmEffect2Data *)vm->ins_508_extra_data;
    memset(data, 0, sizeof(AnmEffect2Data));
    data->timer = 0;
    return 0;
}

// FUNCTION: TH16 0x405ec0
int __fastcall anm_effect_2_on_draw(AnmVm *vm)
{
    return 0;
}

// FUNCTION: TH16 0x405ed0
int __fastcall anm_effect_2_on_destroy(AnmVm *vm)
{
    AnmEffect2Data *data = (AnmEffect2Data *)vm->ins_508_extra_data;
    AnmManager *anm_manager = g_AnmManager;
    for (i32 i = 0; i < 200; i++)
    {
        AnmVm *child = anm_manager->get_vm_with_id(data->vm_ids[i]);
        if (child == NULL)
        {
            data->vm_ids[i].id = 0;
        }
        else
        {
            child->flags_lo &= ~1;
            child->instr_offset = -1;
        }
    }
    return 0;
}

// FUNCTION: TH16 0x405f20
int __fastcall anm_effect_2_on_switch(AnmVm *vm, i32 n)
{
    switch (n)
    {
    case 1:
        ((AnmEffect2Data *)vm->ins_508_extra_data)->timer += 300.0f;
        break;
    }
    return 0;
}

// A snapshot of the VM with the given id, 0 for none.
static AnmId snapshot_of_vm_id(AnmId id)
{
    if (id.id == 0)
    {
        return id;
    }
    return g_AnmManager->store_snapshot_of_vm(g_AnmManager->get_vm_with_id(id), NULL, 0);
}

// Copies the child VMs along with the VM: into snapshots (mode 0) or back
// out of them (mode 1).
// FUNCTION: TH16 0x405fa0
int __fastcall anm_effect_2_on_copy_2(AnmVm *vm, const AnmVm *other, i32 mode)
{
    AnmEffect2Data *dst = (AnmEffect2Data *)vm->ins_508_extra_data;
    AnmEffect2Data *src = (AnmEffect2Data *)other->ins_508_extra_data;
    if (mode == 0)
    {
        for (i32 i = 0; i < 200; i++)
        {
            dst->vm_ids[i] = snapshot_of_vm_id(src->vm_ids[i]);
        }
    }
    else if (mode == 1)
    {
        for (i32 i = 0; i < 200; i++)
        {
            dst->vm_ids[i] = g_AnmManager->restore_snapshot(src->vm_ids[i]);
        }
    }
    return 0;
}

// Saves the child VMs into the snapshot buffer after the VM's own data
// (mode 0) or reads them back (mode 1). The copy of the extra data in the
// buffer keeps only a flag per child.
// TODO: the original advances the buffer register in place for the cursor
// and clears child_size later in mode 1 (scheduling).
// FUNCTION: TH16 0x406040
int __fastcall anm_effect_2_on_copy_1(AnmVm *vm, u8 *buffer, i32 *size, i32 mode)
{
    AnmEffect2Data *data = (AnmEffect2Data *)vm->ins_508_extra_data;
    *size += sizeof(AnmEffect2Data);
    AnmEffect2Data *saved = (AnmEffect2Data *)buffer;
    u8 *cursor = buffer + sizeof(AnmEffect2Data);
    i32 child_size;
    if (mode == 0)
    {
        for (i32 i = 0; i < 200; i++)
        {
            child_size = 0;
            AnmVm *child = g_AnmManager->get_snapshot_vm_with_id_inline(data->vm_ids[i]);
            if (child != NULL)
            {
                g_AnmManager->serialize_vm_tree(cursor, child, &child_size);
                *size += child_size;
                cursor += child_size;
                saved->vm_ids[i].id = 0xff;
            }
            else
            {
                saved->vm_ids[i].id = 0;
            }
        }
    }
    else if (mode == 1)
    {
        for (i32 i = 0; i < 200; i++)
        {
            if (saved->vm_ids[i].id != 0)
            {
                child_size = 0;
                AnmId id = g_AnmManager->deserialize_vm_tree(cursor, NULL, &child_size);
                data->vm_ids[i] = id;
                *size += child_size;
                cursor += child_size;
            }
            else
            {
                data->vm_ids[i].id = 0;
            }
        }
    }
    return 0;
}

// FUNCTION: TH16 0x406910
int __fastcall anm_effect_3_on_destroy(AnmVm *vm)
{
    return 0;
}

// FUNCTION: TH16 0x406920
int __fastcall anm_effect_3_on_switch(AnmVm *vm, i32 n)
{
    return 0;
}

// ins_508 data of effect kind 1: four VMs, then a fifth that runs until
// those have all ended.
struct AnmEffect1Data
{
    AnmVm vms[5];
    i32 unk_1dec;
    i32 frame_count;
};

// TODO: the original aligns its frame to 8 bytes (LTCG; AnmVm::run is
// still a stub here).
// FUNCTION: TH16 0x407330
int __fastcall anm_effect_1_on_tick(AnmVm *vm)
{
    AnmEffect1Data *data = (AnmEffect1Data *)vm->ins_508_extra_data;
    i32 finished = 0;
    for (i32 i = 0; i < 4; i++)
    {
        if (data->vms[i].run())
        {
            finished++;
        }
    }
    if (finished >= 4)
    {
        return 1;
    }
    data->vms[4].run();
    data->frame_count++;
    return 0;
}

// FUNCTION: TH16 0x4078f0
int __fastcall anm_effect_1_on_destroy(AnmVm *vm)
{
    return 0;
}
