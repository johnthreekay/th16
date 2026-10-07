// Callbacks that ANM VMs run through their index_of_on_* fields and the
// ins_508 table. Each effect kind has its own set; which kind is which is
// not known yet, so they are numbered as in ExpHP's names.
#include <string.h>

#include "AnmManager.h"
#include "AnmVm.h"
#include "EffectManager.h"
#include "Rng.h"
#include "ZunMath.h"

// The copy of ZunMath.h's sincosmul that effect kind 3's object file has
// (TH16 keeps one per object file). A static of its own so that it can be
// annotated.
// FUNCTION: TH16 0x406cc0
static void __fastcall effect3_sincosmul(Float3 *dst, f32 angle, f32 radius)
{
    __asm {
        mov eax, dst
        fld angle
        fsincos
        fmul radius
        fstp [eax]
        fmul radius
        fstp [eax+4]
    }
}

// ins_508 data of effect kind 2.
struct AnmEffect2Data
{
    AnmId vm_ids[200];
    // Per child: where its first curve ended and the bezier term it ended
    // with, and how far along it is (0 new, 1 on the way out, 2 back).
    Float3 unk_320[200];
    Float3 unk_c80[200];
    i32 states[200];
    // The points the curves run between: near the VM, circling it at 150
    // and at 300 pixels.
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
    AnmEffect2Data *data = (AnmEffect2Data *)vm->extra_data;
    memset(data, 0, sizeof(AnmEffect2Data));
    data->timer = 0;
    return 0;
}

// The copy of ZunMath.h's sincosmul that effect kind 2's object file has.
// FUNCTION: TH16 0x406470
static void __fastcall effect2_sincosmul(Float3 *dst, f32 angle, f32 radius)
{
    __asm {
        mov eax, dst
        fld angle
        fsincos
        fmul radius
        fstp [eax]
        fmul radius
        fstp [eax+4]
    }
}

// Gives a new child VM its color and flight time.
static __forceinline void effect2_setup_child(AnmId *id, ZunColor color, AnmVm *vm)
{
    AnmVm *child = g_AnmManager->get_vm_with_id(*id);
    if (child == NULL)
    {
        id->id = 0;
    }
    child->color_1 = color;
    child->int_vars[0] = vm->int_vars[0];
}

// TODO: same operations, different stack slot layout and scheduling (the original's frame is 0x90 bytes, ours 0xa4).
// Spawns four child VMs per frame for 50 frames and flies each along two
// bezier curves: out from a point that circles the VM to one that circles
// it closer, then back to the VM.
// FUNCTION: TH16 0x405700
int __fastcall anm_effect_2_on_tick(AnmVm *vm)
{
    i32 alive = 0;
    AnmEffect2Data *data = (AnmEffect2Data *)vm->extra_data;
    Float3 offset;
    Float3 pos = vm->entity_pos;
    data->unk_1900 = data->unk_190c = data->unk_1918 = pos;
    effect2_sincosmul(&offset, vm->rotation.z, 300.0f);
    offset.z = 0.0f;
    data->unk_190c += offset;
    effect2_sincosmul(&offset, vm->float_vars_4_to_6.z + vm->rotation.z, 150.0f);
    offset.z = 0.0f;
    data->unk_1900 += offset;
    if (data->timer.current != data->timer.previous && data->timer.current < 50)
    {
        i32 n = data->timer.current * 4;
        data->vm_ids[n] = g_EffectManager->effect_anm->create_effect(0x99, -1, NULL);
        data->vm_ids[n + 1] = g_EffectManager->effect_anm->create_effect(0x99, -1, NULL);
        data->vm_ids[n + 2] = g_EffectManager->effect_anm->create_effect(0x99, -1, NULL);
        data->vm_ids[n + 3] = g_EffectManager->effect_anm->create_effect(0x9a, -1, NULL);
        ZunColor color = vm->color_1;
        effect2_setup_child(&data->vm_ids[n], color, vm);
        effect2_setup_child(&data->vm_ids[n + 1], color, vm);
        effect2_setup_child(&data->vm_ids[n + 2], color, vm);
        // The last one in the complementary color.
        color.r = 0x2c - color.r;
        color.g = 0x2c - color.g;
        color.b = 0x2c - color.b;
        effect2_setup_child(&data->vm_ids[n + 3], color, vm);
    }
    for (i32 i = 0; i < 200; i++)
    {
        AnmVm *child = g_AnmManager->get_vm_with_id(data->vm_ids[i]);
        if (child == NULL)
        {
            data->vm_ids[i].id = 0;
            continue;
        }
        child->slowdown = vm->get_slowdown_factor();
        if (data->states[i] == 0)
        {
            Float3 start = data->unk_190c;
            f32 angle = g_replay_safe_rng.randf_neg_1_to_1() * ZUN_PI;
            effect2_sincosmul(&offset, angle, g_replay_safe_rng.randf_0_to_1() * 150.0f);
            offset.z = 0.0f;
            start += offset;
            Float3 mid = data->unk_1900;
            angle = g_replay_safe_rng.randf_neg_1_to_1() * ZUN_PI;
            effect2_sincosmul(&offset, angle, g_replay_safe_rng.randf_0_to_1() * 50.0f);
            offset.z = 0.0f;
            mid += offset;
            Float3 bezier_2;
            Float3 bezier_1;
            // Out of the start towards the middle and on to the end.
            D3DXVec3Normalize(&bezier_2, &(mid - start));
            D3DXVec3Normalize(&bezier_1, &(data->unk_1918 - mid));
            bezier_2 += bezier_1;
            f32 speed = g_replay_safe_rng.randf_0_to_1() * 200.0f + 200.0f;
            D3DXVec3Normalize(&bezier_2, &bezier_2);
            bezier_2 *= speed;
            speed = g_replay_safe_rng.randf_0_to_1() * 100.0f + 100.0f;
            D3DXVec3Normalize(&bezier_1, &(mid - start));
            bezier_1 *= speed;
            child->set_pos_bezier(vm->int_vars[0], &start, &bezier_1, &mid, &bezier_2);
            data->unk_320[i] = mid;
            data->unk_c80[i] = bezier_2;
            data->states[i] = 1;
        }
        else if (child->time_in_script.current >= vm->int_vars[0] + 1 && data->states[i] == 1)
        {
            Float3 end = vm->entity_pos;
            f32 angle = g_replay_safe_rng.randf_neg_1_to_1() * ZUN_PI;
            effect2_sincosmul(&offset, angle, g_replay_safe_rng.randf_0_to_1() * 20.0f);
            offset.z = 0.0f;
            end += offset;
            Float3 bezier_2;
            angle = g_replay_safe_rng.randf_neg_1_to_1() * ZUN_PI;
            effect2_sincosmul(&bezier_2, angle, g_replay_safe_rng.randf_0_to_1() * 20.0f);
            bezier_2.z = 0.0f;
            child->set_pos_bezier(vm->int_vars[0], &data->unk_320[i], &data->unk_c80[i], &end, &bezier_2);
            data->states[i] = 2;
        }
        alive++;
    }
    if (alive == 0)
    {
        return -1;
    }
    data->timer.tick_split();
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
    AnmEffect2Data *data = (AnmEffect2Data *)vm->extra_data;
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
        ((AnmEffect2Data *)vm->extra_data)->timer += 300.0f;
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
// TODO: the mode 0 loop spills its counter (the original keeps it in ebx)
// since get_vm_with_id has a visible body (it matched against the opaque
// stub).
// FUNCTION: TH16 0x405fa0
int __fastcall anm_effect_2_on_copy_2(AnmVm *vm, const AnmVm *other, i32 mode)
{
    AnmEffect2Data *dst = (AnmEffect2Data *)vm->extra_data;
    AnmEffect2Data *src = (AnmEffect2Data *)other->extra_data;
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
// FUNCTION: TH16 0x406040
int __fastcall anm_effect_2_on_copy_1(AnmVm *vm, u8 *buffer, i32 *size, i32 mode)
{
    AnmEffect2Data *data = (AnmEffect2Data *)vm->extra_data;
    *size += sizeof(AnmEffect2Data);
    AnmEffect2Data *saved = (AnmEffect2Data *)buffer;
    buffer += sizeof(AnmEffect2Data);
    i32 child_size;
    if (mode == 0)
    {
        for (i32 i = 0; i < 200; i++)
        {
            child_size = 0;
            AnmVm *child = g_AnmManager->get_snapshot_vm_with_id_inline(data->vm_ids[i]);
            if (child != NULL)
            {
                g_AnmManager->save_vm_tree((AnmVm *)buffer, child, &child_size);
                *size += child_size;
                buffer += child_size;
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
                AnmId id = g_AnmManager->load_vm_tree((AnmVm *)buffer, NULL, &child_size);
                data->vm_ids[i] = id;
                *size += child_size;
                buffer += child_size;
            }
            else
            {
                data->vm_ids[i].id = 0;
            }
        }
    }
    return 0;
}

// ins_508 data of effect kind 3: a fan of colored points that grows by
// one each frame.
struct AnmEffect3Data
{
    Float2 offsets[64];
    ZunColor colors[64];
    f32 angle;
    ZunTimer timer;
};

// FUNCTION: TH16 0x406510
int __fastcall anm_effect_3_init(AnmVm *vm, D3DXVECTOR3 *pos)
{
    vm->alloc_extra_data(sizeof(AnmEffect3Data));
    AnmEffect3Data *data = (AnmEffect3Data *)vm->extra_data;
    memset(data, 0, sizeof(AnmEffect3Data));
    data->offsets[0].x = pos->x;
    data->offsets[0].y = pos->y;
    data->offsets[0].x += 320.0f;
    data->offsets[0].y += 16.0f;
    data->angle = g_replay_unsafe_rng.randf_neg_1_to_1() * ZUN_PI;
    data->timer = 1;
    i32 j = 0;
    for (i32 i = 0; i < 64; i++)
    {
        data->colors[i].d3d = 0xff0080ff;
        if (i < 8)
        {
            data->colors[i].r = ~(i << 5);
        }
        if (i >= 32)
        {
            data->colors[i].a = ~(j++ << 4);
        }
    }
    vm->entity_pos = *pos;
    vm->set_layer(15);
    vm->flags_lo = vm->flags_lo & ~ANM_VM_BLEND_MODE_MASK | (1 << ANM_VM_BLEND_MODE_SHIFT);
    vm->set_alpha1_time(0x40, 0, 0xff, 0);
    return 0;
}

// FUNCTION: TH16 0x406690
int __fastcall anm_effect_3_on_tick(AnmVm *vm)
{
    AnmEffect3Data *data = (AnmEffect3Data *)vm->extra_data;
    i32 n = data->timer.current;
    if (n < 64)
    {
        if (n != data->timer.previous)
        {
            for (i32 i = 0; i < n; i++)
            {
                if (data->colors[i].a >= 0x10)
                {
                    data->colors[i].a -= 0x10;
                }
                else
                {
                    data->colors[i].a = 0;
                }
            }
            Float2 *point = &data->offsets[n];
            effect3_sincosmul((Float3 *)point, data->angle, g_replay_unsafe_rng.randf_0_to_1() * 5.0f + 4.0f);
            point->x += point[-1].x;
            point->y += point[-1].y;
            data->angle = wrap_angle(g_replay_unsafe_rng.randf_neg_to(ZUN_PI) / 5.0f + data->angle);
        }
        data->timer.tick_split();
        return 0;
    }
    return 1;
}

// TODO: the original aligns the frame to 8 bytes (and esp, -8) and adds
// entity_pos.x + pos.x in the other order.
// FUNCTION: TH16 0x406860
int __fastcall anm_effect_3_on_draw(AnmVm *vm)
{
    AnmEffect3Data *data = (AnmEffect3Data *)vm->extra_data;
    g_AnmManager->setup_render_state_for_vm(vm);
    Float3 pos;
    pos = vm->entity_pos + vm->pos + vm->pos_2;
    vm->transform_coords(&pos);
    g_AnmManager->draw_triangle_fan(data->timer.current, &pos, data->offsets, data->colors);
    return 0;
}

// FUNCTION: TH16 0x406930
int __fastcall anm_effect_3b_init(AnmVm *vm, D3DXVECTOR3 *pos)
{
    vm->alloc_extra_data(sizeof(AnmEffect3Data));
    AnmEffect3Data *data = (AnmEffect3Data *)vm->extra_data;
    memset(data, 0, sizeof(AnmEffect3Data));
    data->offsets[0].x = 0.0f;
    data->offsets[0].y = 0.0f;
    data->angle = g_replay_unsafe_rng.randf_neg_1_to_1() * ZUN_PI;
    data->timer = 1;
    i32 j = 0;
    for (i32 i = 0; i < 64; i++)
    {
        data->colors[i].d3d = 0xff505050;
        if (i < 8)
        {
            data->colors[i].b = ~(i << 5);
        }
        if (i >= 32)
        {
            data->colors[i].a = ~(j++ << 4);
        }
    }
    vm->entity_pos = *pos;
    vm->flags_lo &= ~ANM_VM_BLEND_MODE_MASK;
    vm->set_layer(19);
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

// FUNCTION: TH16 0x4071a0
int __fastcall anm_effect_1_init(AnmVm *vm, D3DXVECTOR3 *pos)
{
    vm->alloc_extra_data(sizeof(AnmEffect1Data));
    AnmEffect1Data *data = (AnmEffect1Data *)vm->extra_data;
    memset(data, 0, sizeof(AnmEffect1Data));
    vm->set_layer(0);
    D3DXVECTOR3 center(320.0f, 240.0f, 0.0f);
    for (i32 i = 0; i < 4; i++)
    {
        g_EffectManager->effect_anm->copy_vm_and_run(&data->vms[i], i + 3);
        data->vms[i].entity_pos = center;
    }
    data->unk_1dec = 0;
    vm->set_layer(40);
    data->vms[3].entity_pos = data->vms[2].entity_pos = data->vms[1].entity_pos = data->vms[0].entity_pos =
        D3DXVECTOR3(320.0f, 240.0f, 0.0f);
    g_EffectManager->effect_anm->copy_vm_and_run(&data->vms[4], 0xc5);
    return 0;
}

// TODO: the original aligns its frame to 8 bytes (and esp, -8; not from AnmVm::run, whose other direct callers do not).
// FUNCTION: TH16 0x407330
int __fastcall anm_effect_1_on_tick(AnmVm *vm)
{
    AnmEffect1Data *data = (AnmEffect1Data *)vm->extra_data;
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

// TODO: scheduling: the original loads 240.0f before 320.0f and the zero z
// only after the four x/y stores.
// FUNCTION: TH16 0x407900
int __fastcall anm_effect_1_on_switch(AnmVm *vm, i32 n)
{
    AnmEffect1Data *data = (AnmEffect1Data *)vm->extra_data;
    switch (n)
    {
    case 1:
        for (i32 i = 0; i < 4; i++)
        {
            g_EffectManager->effect_anm->copy_vm_and_run(&data->vms[i], i + 7);
        }
        return 0;
    case 7:
        data->unk_1dec = 0;
        vm->set_layer(30);
        break;
    case 8:
        data->unk_1dec = 1;
        vm->set_layer(23);
        break;
    case 9:
        data->unk_1dec = 0;
        vm->set_layer(36);
        break;
    case 10:
        data->unk_1dec = 3;
        vm->set_layer(30);
        break;
    default:
        return 0;
    }
    data->vms[0].entity_pos = data->vms[1].entity_pos = data->vms[2].entity_pos = data->vms[3].entity_pos =
        D3DXVECTOR3(320.0f, 240.0f, 0.0f);
    return 0;
}

// FUNCTION: TH16 0x4078f0
int __fastcall anm_effect_1_on_destroy(AnmVm *vm)
{
    return 0;
}
