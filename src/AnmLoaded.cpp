#include <string.h>

#include "AnmManager.h"

// FUNCTION: TH16 0x45edb0
i32 AnmLoaded::set_sprite(AnmVm *vm, i32 sprite)
{
    if (anm_file == NULL)
    {
        return -1;
    }
    vm->sprite_id = sprite;
    AnmLoadedSprite *s = &sprites[sprite];
    vm->uv_quad_of_sprite[0].x = vm->uv_quad_of_sprite[2].x = s->uv_start.x;
    vm->uv_quad_of_sprite[1].x = vm->uv_quad_of_sprite[3].x = s->uv_end.x;
    vm->uv_quad_of_sprite[0].y = vm->uv_quad_of_sprite[1].y = s->uv_start.y;
    vm->uv_quad_of_sprite[2].y = vm->uv_quad_of_sprite[3].y = s->uv_end.y;
    vm->sprite_size.x = s->sprite_width;
    vm->sprite_size.y = s->sprite_height;
    D3DXMatrixIdentity(&vm->matrix_3d0);
    D3DXMatrixIdentity(&vm->matrix_450);
    vm->matrix_3d0.m[0][0] = vm->sprite_size.x / 256.0f;
    vm->matrix_3d0.m[1][1] = vm->sprite_size.y / 256.0f;
    vm->matrix_450.m[0][0] = vm->sprite_size.x / s->bitmap_width * s->unk_3c.x;
    vm->matrix_450.m[1][1] = vm->sprite_size.y / s->bitmap_height * s->unk_3c.y;
    vm->matrix_410 = vm->matrix_3d0;
    return 0;
}

// FUNCTION: TH16 0x45f020
i32 AnmLoaded::init_script_vm(AnmVm *vm, i32 script)
{
    if (scripts[script] == NULL)
    {
        memset(vm, 0, sizeof(AnmVm));
        return -1;
    }
    vm->wipe();
    vm->unk_49c = script;
    vm->anm_loaded_index = slot_num;
    vm->flags_lo &= ~(ANM_VM_FLAG_LO_800 | ANM_VM_FLAG_LO_1000);
    vm->flags_hi &= ~ANM_VM_FLAG_HI_4000;
    vm->script_id = script;
    vm->flags_hi |= ANM_VM_FLAG_HI_8000;
    vm->instr_offset = 0;
    vm->timer_1c.reset();
    vm->script_time.reset();
    vm->flags_lo &= ~ANM_VM_VISIBLE;
    return 0;
}

// FUNCTION: TH16 0x45f160
void AnmLoaded::set_vm_script(AnmVm *vm, i32 script)
{
    if (scripts[script] == NULL || load_wait != 0)
    {
        memset(vm, 0, sizeof(AnmVm));
        return;
    }
    vm->unk_49c = script;
    vm->anm_loaded_index = slot_num;
    vm->flags_lo &= ~(ANM_VM_FLAG_LO_800 | ANM_VM_FLAG_LO_1000);
    vm->flags_hi &= ~ANM_VM_FLAG_HI_4000;
    vm->script_id = script;
    vm->flags_hi |= ANM_VM_FLAG_HI_8000;
    vm->instr_offset = 0;
    vm->timer_1c.reset();
    vm->script_time.reset();
    vm->flags_lo &= ~ANM_VM_VISIBLE;
    vm->run();
    g_AnmManager->unk_c0++;
    if ((vm->flags_hi & (ANM_VM_FLAG_HI_4000 | ANM_VM_FLAG_HI_8000)) == ANM_VM_FLAG_HI_8000)
    {
        vm->flags_hi &= ~ANM_VM_FLAG_HI_8000;
        vm->flags_hi |= ANM_VM_FLAG_HI_4000;
    }
}
