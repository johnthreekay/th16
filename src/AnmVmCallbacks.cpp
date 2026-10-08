// The EffectManager effects: VMs with their own extra data and callbacks,
// reached through the index_of_on_* fields (AnmCallbackIndex). Effect 0 is
// the masked effect (four VMs drawn as an alpha mask, a fifth through it),
// effect 1 the gather effect (children flying in to the VM), effects 2 and 3
// a jagged line that grows by a random segment each frame. ExpHP numbers
// them by their callback index: effect_1 to effect_3.
#include <string.h>

#include "AnmManager.h"
#include "AnmVm.h"
#include "BulletManager.h"
#include "EffectManager.h"
#include "Fog.h"
#include "Gui.h"
#include "Laser.h"
#include "Rng.h"
#include "ZunAsm.h"
#include "ZunMath.h"

// The copy of sincosmul (ZunAsm.h) that the jagged line's object file has
// (TH16 keeps one per object file). A static of its own so that it can be
// annotated.
// FUNCTION: TH16 0x406cc0
static void __fastcall jagged_line_sincosmul(Float3 *dst, f32 angle, f32 radius)
{
    ZUN_ASM_SINCOSMUL(dst, angle, radius);
}

// Extra data of the gather effect (EffectManager effect 1): up to 200
// child VMs that fly in towards the VM.
struct AnmGatherEffectData
{
    AnmId vm_ids[200];
    // Per child: where its first curve ended and the bezier term it ended
    // with, and how far along it is (0 new, 1 on the way out, 2 back).
    Float3 mids[200];
    Float3 mid_tangents[200];
    i32 states[200];
    // The points the curves run between: the start area circles the VM at
    // 300 pixels, the middle area at 150 (turned by F6), the end is the VM.
    Float3 mid_center;
    Float3 start_center;
    Float3 end_center;
    u8 unk_1924[4];
    ZunTimer timer;
};

// The position create_effect passes is not used.
// FUNCTION: TH16 0x405670
int __fastcall anm_gather_effect_init(AnmVm *vm, D3DXVECTOR3 *pos)
{
    vm->alloc_extra_data(sizeof(AnmGatherEffectData));
    AnmGatherEffectData *data = (AnmGatherEffectData *)vm->extra_data;
    memset(data, 0, sizeof(AnmGatherEffectData));
    data->timer = 0;
    return 0;
}

// The copy of sincosmul (ZunAsm.h) that the gather effect's object file has.
// FUNCTION: TH16 0x406470
static void __fastcall gather_sincosmul(Float3 *dst, f32 angle, f32 radius)
{
    ZUN_ASM_SINCOSMUL(dst, angle, radius);
}

// Gives a new child VM its color and flight time.
static __forceinline void gather_setup_child(AnmId *id, ZunColor color, AnmVm *vm)
{
    AnmVm *child = g_AnmManager->get_vm_with_id(*id);
    if (child == NULL)
    {
        id->id = 0;
    }
    child->color_1 = color;
    child->int_vars[0] = vm->int_vars[0];
}

// The direction temporaries reuse offset, as the original's stack slots
// show (it computes mid - start once for both normalizations).
// TODO: different stack slot layout (the original's frame is 0x90 bytes,
// ours 0xa8).
// Spawns four child VMs per frame for 50 frames and flies each along two
// bezier curves: out from a point that circles the VM to one that circles
// it closer, then back to the VM.
// FUNCTION: TH16 0x405700
int __fastcall anm_gather_effect_on_tick(AnmVm *vm)
{
    i32 alive = 0;
    AnmGatherEffectData *data = (AnmGatherEffectData *)vm->extra_data;
    Float3 offset;
    Float3 pos = vm->entity_pos;
    data->mid_center = data->start_center = data->end_center = pos;
    gather_sincosmul(&offset, vm->rotation.z, 300.0f);
    offset.z = 0.0f;
    data->start_center += offset;
    gather_sincosmul(&offset, vm->float_vars_4_to_6.z + vm->rotation.z, 150.0f);
    offset.z = 0.0f;
    data->mid_center += offset;
    if (data->timer.current != data->timer.previous && data->timer.current < 50)
    {
        i32 n = data->timer.current * 4;
        data->vm_ids[n] = create_effect_via_pointer(g_EffectManager->effect_anm,
                                                    EFFECT_SCRIPT_GATHER_PARTICLE, -1, NULL);
        data->vm_ids[n + 1] = create_effect_via_pointer(g_EffectManager->effect_anm,
                                                        EFFECT_SCRIPT_GATHER_PARTICLE, -1, NULL);
        data->vm_ids[n + 2] = create_effect_via_pointer(g_EffectManager->effect_anm,
                                                        EFFECT_SCRIPT_GATHER_PARTICLE, -1, NULL);
        data->vm_ids[n + 3] = create_effect_via_pointer(g_EffectManager->effect_anm,
                                                        EFFECT_SCRIPT_GATHER_PARTICLE_2, -1, NULL);
        ZunColor color = vm->color_1;
        gather_setup_child(&data->vm_ids[n], color, vm);
        gather_setup_child(&data->vm_ids[n + 1], color, vm);
        gather_setup_child(&data->vm_ids[n + 2], color, vm);
        // The last one in the complementary color.
        color.r = 0x2c - color.r;
        color.g = 0x2c - color.g;
        color.b = 0x2c - color.b;
        gather_setup_child(&data->vm_ids[n + 3], color, vm);
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
            Float3 start = data->start_center;
            f32 angle = g_replay_safe_rng.randf_neg_1_to_1() * ZUN_PI;
            gather_sincosmul(&offset, angle, g_replay_safe_rng.randf_0_to_1() * 150.0f);
            offset.z = 0.0f;
            start += offset;
            Float3 mid = data->mid_center;
            angle = g_replay_safe_rng.randf_neg_1_to_1() * ZUN_PI;
            gather_sincosmul(&offset, angle, g_replay_safe_rng.randf_0_to_1() * 50.0f);
            offset.z = 0.0f;
            mid += offset;
            Float3 bezier_2;
            Float3 bezier_1;
            // Out of the start towards the middle and on to the end.
            offset = mid - start;
            D3DXVec3Normalize(&bezier_2, &offset);
            offset = data->end_center - mid;
            D3DXVec3Normalize(&bezier_1, &offset);
            bezier_2 += bezier_1;
            f32 speed = g_replay_safe_rng.randf_0_to_1() * 200.0f + 200.0f;
            D3DXVec3Normalize(&bezier_2, &bezier_2);
            bezier_2 *= speed;
            offset = mid - start;
            speed = g_replay_safe_rng.randf_0_to_1() * 100.0f + 100.0f;
            D3DXVec3Normalize(&bezier_1, &offset);
            bezier_1 *= speed;
            child->set_pos_bezier(vm->int_vars[0], &start, &bezier_1, &mid, &bezier_2);
            data->mids[i] = mid;
            data->mid_tangents[i] = bezier_2;
            data->states[i] = 1;
        }
        else if (child->time_in_script.current >= vm->int_vars[0] + 1 && data->states[i] == 1)
        {
            Float3 end = vm->entity_pos;
            f32 angle = g_replay_safe_rng.randf_neg_1_to_1() * ZUN_PI;
            gather_sincosmul(&offset, angle, g_replay_safe_rng.randf_0_to_1() * 20.0f);
            offset.z = 0.0f;
            end += offset;
            Float3 bezier_2;
            angle = g_replay_safe_rng.randf_neg_1_to_1() * ZUN_PI;
            gather_sincosmul(&bezier_2, angle, g_replay_safe_rng.randf_0_to_1() * 20.0f);
            bezier_2.z = 0.0f;
            child->set_pos_bezier(vm->int_vars[0], &data->mids[i], &data->mid_tangents[i], &end, &bezier_2);
            data->states[i] = 2;
        }
        alive++;
    }
    if (alive == 0)
    {
        return -1;
    }
    data->timer.tick_goto();
    return 0;
}

// FUNCTION: TH16 0x405ec0
int __fastcall anm_gather_effect_on_draw(AnmVm *vm)
{
    return 0;
}

// FUNCTION: TH16 0x405ed0
int __fastcall anm_gather_effect_on_destroy(AnmVm *vm)
{
    AnmGatherEffectData *data = (AnmGatherEffectData *)vm->extra_data;
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
            child->flags_lo &= ~ANM_VM_VISIBLE;
            child->instr_offset = -1;
        }
    }
    return 0;
}

// FUNCTION: TH16 0x405f20
int __fastcall anm_gather_effect_on_switch(AnmVm *vm, i32 n)
{
    switch (n)
    {
    case 1:
        ((AnmGatherEffectData *)vm->extra_data)->timer += 300.0f;
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
// since get_vm_with_id has a visible body (it matched while
// get_vm_with_id was an opaque placeholder).
// FUNCTION: TH16 0x405fa0
int __fastcall anm_gather_effect_on_copy(AnmVm *vm, const AnmVm *other, i32 mode)
{
    AnmGatherEffectData *dst = (AnmGatherEffectData *)vm->extra_data;
    AnmGatherEffectData *src = (AnmGatherEffectData *)other->extra_data;
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
int __fastcall anm_gather_effect_on_serialize(AnmVm *vm, u8 *buffer, i32 *size, i32 mode)
{
    AnmGatherEffectData *data = (AnmGatherEffectData *)vm->extra_data;
    *size += sizeof(AnmGatherEffectData);
    AnmGatherEffectData *saved = (AnmGatherEffectData *)buffer;
    buffer += sizeof(AnmGatherEffectData);
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

// Extra data of the jagged line (EffectManager effects 2 and 3): up to 64
// points, one more each frame, each a random step from the one before.
struct AnmJaggedLineData
{
    Float2 offsets[64];
    ZunColor colors[64];
    f32 angle;
    ZunTimer timer;
};

// FUNCTION: TH16 0x406510
int __fastcall anm_jagged_line_blue_init(AnmVm *vm, D3DXVECTOR3 *pos)
{
    vm->alloc_extra_data(sizeof(AnmJaggedLineData));
    AnmJaggedLineData *data = (AnmJaggedLineData *)vm->extra_data;
    memset(data, 0, sizeof(AnmJaggedLineData));
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
int __fastcall anm_jagged_line_on_tick(AnmVm *vm)
{
    AnmJaggedLineData *data = (AnmJaggedLineData *)vm->extra_data;
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
            jagged_line_sincosmul((Float3 *)point, data->angle, g_replay_unsafe_rng.randf_0_to_1() * 5.0f + 4.0f);
            point->x += point[-1].x;
            point->y += point[-1].y;
            data->angle = wrap_angle(g_replay_unsafe_rng.randf_neg_to(ZUN_PI) / 5.0f + data->angle);
        }
        data->timer.tick_split();
        return 0;
    }
    return 1;
}

// Draws the line as a strip through the stored offsets around the VM's
// position. The dead double math is not ZUN's code: it is enough of it for
// LTCG's double stack alignment pass to realign the frame (and esp, -8)
// like the original, and it also gives entity_pos.x + pos.x the original's
// operand order (one or two plain dead doubles fix only the order).
// FUNCTION: TH16 0x406860
int __fastcall anm_jagged_line_on_draw(AnmVm *vm)
{
    double unused = 0.0;
    unused = unused * 2.0;
    unused = unused * 2.0;
    unused = unused * 2.0;
    (void)unused;
    AnmJaggedLineData *data = (AnmJaggedLineData *)vm->extra_data;
    g_AnmManager->setup_render_state_for_vm(vm);
    Float3 pos;
    pos = vm->entity_pos + vm->pos + vm->pos_2;
    vm->transform_coords(&pos);
    g_AnmManager->draw_triangle_fan(data->timer.current, &pos, data->offsets, data->colors);
    return 0;
}

// FUNCTION: TH16 0x406930
int __fastcall anm_jagged_line_gray_init(AnmVm *vm, D3DXVECTOR3 *pos)
{
    vm->alloc_extra_data(sizeof(AnmJaggedLineData));
    AnmJaggedLineData *data = (AnmJaggedLineData *)vm->extra_data;
    memset(data, 0, sizeof(AnmJaggedLineData));
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
int __fastcall anm_jagged_line_on_destroy(AnmVm *vm)
{
    return 0;
}

// FUNCTION: TH16 0x406920
int __fastcall anm_jagged_line_on_switch(AnmVm *vm, i32 n)
{
    return 0;
}

// Extra data of the masked effect (EffectManager effect 0): four VMs drawn as
// an alpha mask and a fifth drawn through it (anm_on_draw_masked, which reads
// it as AnmMaskData), until the four have ended.
struct AnmMaskedEffectData
{
    AnmVm vms[5];
    // AnmMaskData::mode: 2 masks the arcade region, 0 the whole screen.
    // Interrupts 7-10 change it along with the layer.
    i32 mask_mode;
    i32 frame_count;
};

// FUNCTION: TH16 0x4071a0
int __fastcall anm_masked_effect_init(AnmVm *vm, D3DXVECTOR3 *pos)
{
    vm->alloc_extra_data(sizeof(AnmMaskedEffectData));
    AnmMaskedEffectData *data = (AnmMaskedEffectData *)vm->extra_data;
    memset(data, 0, sizeof(AnmMaskedEffectData));
    vm->set_layer(0);
    D3DXVECTOR3 center(320.0f, 240.0f, 0.0f);
    for (i32 i = 0; i < 4; i++)
    {
        g_EffectManager->effect_anm->copy_vm_and_run(&data->vms[i], i + EFFECT_SCRIPT_MASK_FIRST);
        data->vms[i].entity_pos = center;
    }
    data->mask_mode = 0;
    vm->set_layer(40);
    data->vms[3].entity_pos = data->vms[2].entity_pos = data->vms[1].entity_pos = data->vms[0].entity_pos =
        D3DXVECTOR3(320.0f, 240.0f, 0.0f);
    g_EffectManager->effect_anm->copy_vm_and_run(&data->vms[4], EFFECT_SCRIPT_MASK_OVERLAY);
    return 0;
}

// Steps the four mask VMs and, while any is still running, the overlay;
// returns 1 once all four have finished.
// The dead double is not ZUN's code: it makes LTCG realign the frame
// (and esp, -8) like the original, which AnmVm::run alone does not.
// FUNCTION: TH16 0x407330
int __fastcall anm_masked_effect_on_tick(AnmVm *vm)
{
    double unused = 0.0;
    (void)unused;
    AnmMaskedEffectData *data = (AnmMaskedEffectData *)vm->extra_data;
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
int __fastcall anm_masked_effect_on_switch(AnmVm *vm, i32 n)
{
    AnmMaskedEffectData *data = (AnmMaskedEffectData *)vm->extra_data;
    switch (n)
    {
    case 1:
        for (i32 i = 0; i < 4; i++)
        {
            g_EffectManager->effect_anm->copy_vm_and_run(&data->vms[i], i + EFFECT_SCRIPT_MASK_SECOND_FIRST);
        }
        return 0;
    case 7:
        data->mask_mode = 0;
        vm->set_layer(30);
        break;
    case 8:
        data->mask_mode = 1;
        vm->set_layer(23);
        break;
    case 9:
        data->mask_mode = 0;
        vm->set_layer(36);
        break;
    case 10:
        data->mask_mode = 3;
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
int __fastcall anm_masked_effect_on_destroy(AnmVm *vm)
{
    return 0;
}

// The callbacks of other files that the tables below point at
// (AnmRender.cpp).
i32 __fastcall anm_on_draw_masked(AnmVm *vm);
i32 __fastcall anm_on_tick_fan(AnmVm *vm);
i32 __fastcall anm_on_draw_fan(AnmVm *vm);

// The callback tables AnmVm's index_of_* fields select from (declared in
// AnmVm.h), indexed by AnmCallbackIndex (AnmSpriteMapping for the sprite
// mapping table). Entry 0 is NULL: no callback. All but the on_wait table
// are constant (.rdata in the original).

// GLOBAL: TH16 0x491b0c
AnmVmSwitchFunc const g_anm_on_switch_funcs[4] = {
    NULL,
    anm_masked_effect_on_switch, // ANM_CALLBACK_MASKED_EFFECT
    anm_gather_effect_on_switch, // ANM_CALLBACK_GATHER_EFFECT
    anm_jagged_line_on_switch,   // ANM_CALLBACK_JAGGED_LINE
};

// GLOBAL: TH16 0x491b58
AnmVmFunc const g_anm_on_destroy_funcs[4] = {
    NULL,
    anm_masked_effect_on_destroy, // ANM_CALLBACK_MASKED_EFFECT
    anm_gather_effect_on_destroy, // ANM_CALLBACK_GATHER_EFFECT
    anm_jagged_line_on_destroy,   // ANM_CALLBACK_JAGGED_LINE
};

// GLOBAL: TH16 0x4919e8
AnmVmFunc const g_anm_on_tick_funcs[5] = {
    NULL,
    anm_masked_effect_on_tick, // ANM_CALLBACK_MASKED_EFFECT
    anm_gather_effect_on_tick, // ANM_CALLBACK_GATHER_EFFECT
    anm_jagged_line_on_tick,   // ANM_CALLBACK_JAGGED_LINE
    anm_on_tick_fan,           // ANM_ON_TICK_FAN
};

// GLOBAL: TH16 0x491b1c
AnmVmSpriteFunc const g_anm_sprite_mapping_funcs[4] = {
    NULL,
    bullet_map_sprite,            // ANM_SPRITE_MAPPING_BULLET
    LaserLineInf::on_sprite_set,  // ANM_SPRITE_MAPPING_LASER_LINE
    LaserCurveInf::on_sprite_set, // ANM_SPRITE_MAPPING_LASER_CURVE
};

// Only the NULL entry (in .data, not .rdata, in the original).
// GLOBAL: TH16 0x4c0f44
AnmVmFunc g_anm_on_wait_funcs[1];

// GLOBAL: TH16 0x491b2c
AnmVmFunc const g_anm_on_draw_funcs[7] = {
    NULL,
    anm_on_draw_masked,        // ANM_CALLBACK_MASKED_EFFECT
    anm_gather_effect_on_draw, // ANM_CALLBACK_GATHER_EFFECT
    anm_jagged_line_on_draw,   // ANM_CALLBACK_JAGGED_LINE
    anm_effect_4_on_draw,      // ANM_ON_DRAW_FOG
    Gui::textbox_on_draw,      // ANM_ON_DRAW_TEXTBOX
    anm_on_draw_fan,           // ANM_ON_DRAW_FAN
};

// The copy and serialize tables have one callback each, the gather
// effect's, at index 1 (not ANM_CALLBACK_GATHER_EFFECT, see g_effect_table).
// GLOBAL: TH16 0x491b50
AnmVmCopyFunc const g_anm_on_copy_funcs[2] = {
    NULL,
    anm_gather_effect_on_copy,
};

// GLOBAL: TH16 0x491b48
AnmVmSerializeFunc const g_anm_serialize_funcs[2] = {
    NULL,
    anm_gather_effect_on_serialize,
};
