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
    Timer timer;
};

// FUNCTION: TH16 0x405670
int __fastcall anm_effect_2_init(AnmVm *vm, i32 arg)
{
    vm->alloc_extra_data(sizeof(AnmEffect2Data));
    AnmEffect2Data *data = (AnmEffect2Data *)vm->ins_508_extra_data;
    memset(data, 0, sizeof(AnmEffect2Data));
    data->timer.set(0);
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

// FUNCTION: TH16 0x4078f0
int __fastcall anm_effect_1_on_destroy(AnmVm *vm)
{
    return 0;
}
