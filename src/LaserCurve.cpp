#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "AnmManager.h"
#include "CriticalSections.h"
#include "EffectManager.h"
#include "Laser.h"

// Placeholder (not decompiled yet).
// STUB: TH16 0x4370a0
i32 LaserCurveInf::initialize(void *params)
{
    return unit5_placeholder(this);
}

// Placeholder (not decompiled yet).
// STUB: TH16 0x438cb0
void LaserCurveInf::run_ex()
{
    unit5_placeholder(this);
}

// Placeholder (not decompiled yet).
// STUB: TH16 0x438750
i32 LaserCurveInf::on_draw()
{
    return unit5_placeholder(this);
}

// FUNCTION: TH16 0x437760
i32 LaserCurveInf::on_destroy()
{
    LaserCurveNode *node = nodes.next;
    while (node != NULL)
    {
        LaserCurveNode *next = node->next;
        delete node;
        node = next;
    }
    if (unk_1528 != NULL)
    {
        free(unk_1528);
        unk_1528 = NULL;
    }
    if (unk_1524 != NULL)
    {
        free(unk_1524);
        unk_1524 = NULL;
    }
    return 0;
}

// Placeholder (not decompiled yet).
// STUB: TH16 0x439d60
i32 LaserCurveInf::method_1c(i32 a, i32 b, i32 c, i32 d, i32 e, i32 f)
{
    return unit5_placeholder(this);
}

// Cancels the segments inside a bomb's rectangle (an effect and items on
// every tenth), then cuts the laser: a hit head is dropped, the laser ends
// at the first hit run, and every later unhit run of at least 4 segments
// becomes a new curvy laser continuing this one's nodes.
// TODO: the original keeps center, size and the loop state in different registers and stack slots; the run loops are laid out differently.
// FUNCTION: TH16 0x4397d0
i32 LaserCurveInf::cancel_as_bomb_rectangle(Float3 *center, Float3 *size, f32 rect_angle, i32 mode, i32 e)
{
    if (e != 0 && countdown_5c8 != 0)
    {
        return 0;
    }
    i32 count = 0;
    u8 *hit = (u8 *)malloc(inner.segment_count);
    memset(hit, 0, inner.segment_count);
    LaserCurveSegment *segment = (LaserCurveSegment *)unk_1524;
    i32 i;
    for (i = 0; i < inner.segment_count; i++, segment++)
    {
        Float3 seg_pos = segment->pos;
        f32 half_w = size->x;
        f32 half_h = size->y;
        f32 dx = seg_pos.x - center->x;
        f32 dy = seg_pos.y - center->y;
        if (rect_angle != 0.0f)
        {
            f32 neg_angle = -rect_angle;
            f32 s = zun_sinf(neg_angle);
            f32 c = zun_cosf(neg_angle);
            f32 rx = dx * c - dy * s;
            dy = dy * c + dx * s;
            dx = rx;
        }
        if (half_w * 0.5f < (f32)fabs(dx) || half_h * 0.5f < (f32)fabs(dy))
        {
            continue;
        }
        count++;
        hit[i] = 1;
        if (i % 10 == 0)
        {
            gen_items_from_cancel(&seg_pos, mode);
            g_EffectManager->track_inline(
                g_BulletManager->bullet_anm->create_vm(inner.color * 2 + 0xd1, &seg_pos, 0.0f, -1, 0));
        }
    }
    if (count != 0)
    {
        if (count >= inner.segment_count)
        {
            pending_delete = 1;
            if (hit != NULL)
            {
                free(hit);
            }
            return count;
        }
        i32 j;
        for (j = 0; j < inner.segment_count; j++)
        {
            if (!hit[j])
            {
                break;
            }
        }
        if (j != 0)
        {
            for (i32 k = 0; k + j < inner.segment_count; k++)
            {
                hit[k] = hit[k + j];
            }
            timer_40 += (f32)-j;
            inner.segment_count -= j;
            if (inner.segment_count < 4)
            {
                pending_delete = 1;
                goto done;
            }
            j = 0;
        }
        for (; j < inner.segment_count; j++)
        {
            if (hit[j])
            {
                break;
            }
        }
        i32 head = j;
        while (j < inner.segment_count)
        {
            for (; j < inner.segment_count; j++)
            {
                if (!hit[j])
                {
                    break;
                }
            }
            if (j >= i)
            {
                break;
            }
            i32 run = 0;
            i32 start = j;
            for (; j < inner.segment_count; j++, run++)
            {
                if (hit[j])
                {
                    break;
                }
            }
            if (run >= 4)
            {
                LaserCurveInner params = inner;
                params.segment_count = run;
                params.shot_sfx = -1;
                params.source_nodes = &nodes;
                params.source_time = timer_40.current_f - (f32)start;
                g_LaserManager->allocate_new_laser(LASER_CURVE, &params);
            }
        }
        if (head >= 4)
        {
            inner.segment_count = head;
        }
        else
        {
            pending_delete = 1;
        }
    }
done:
    if (hit != NULL)
    {
        free(hit);
    }
    return count;
}

// Cancels the segments inside a bomb's circle (an effect and items on every
// tenth), then cuts them off the laser: all hit deletes it, a hit head is
// dropped, and otherwise everything before the end of the first hit run.
// Returns the number of segments hit.
// FUNCTION: TH16 0x43a2f0
i32 LaserCurveInf::cancel_as_bomb_circle(Float3 *pos, f32 radius, i32 mode, i32 d)
{
    if (d != 0 && countdown_5c8 != 0)
    {
        return 0;
    }
    i32 count = 0;
    u8 hit[0x100];
    memset(hit, 0, sizeof(hit));
    radius = radius * radius;
    LaserCurveSegment *segment = (LaserCurveSegment *)unk_1524;
    i32 i;
    for (i = 0; i < inner.segment_count; i++, segment++)
    {
        Float3 seg_pos = segment->pos;
        if ((pos->x - seg_pos.x) * (pos->x - seg_pos.x) + (pos->y - seg_pos.y) * (pos->y - seg_pos.y) > radius)
        {
            continue;
        }
        count++;
        hit[i] = 1;
        if (i % 10 == 0)
        {
            gen_items_from_cancel(&seg_pos, mode);
            g_BulletManager->bullet_anm->create_vm(inner.color * 2 + 0xd1, &seg_pos, 0.0f, -1, 0);
        }
    }
    if (count != 0)
    {
        if (count >= i)
        {
            pending_delete = 1;
            return count;
        }
        i32 j;
        for (j = 0; j < i; j++)
        {
            if (!hit[j])
            {
                break;
            }
        }
        if (j != 0)
        {
            for (i32 k = 0; k < inner.segment_count - j; k++)
            {
                ((LaserCurveSegment *)unk_1524)[k] = ((LaserCurveSegment *)unk_1524)[k + j];
            }
            timer_40 += (f32)-j;
            inner.segment_count -= j;
            return count;
        }
        i32 head = 0;
        for (; j < i; j++, head++)
        {
            if (hit[j])
            {
                break;
            }
        }
        if (j < i && head != 0)
        {
            for (; j < i; j++)
            {
                if (!hit[j])
                {
                    break;
                }
            }
            if (j >= i)
            {
                inner.segment_count = head;
            }
            else
            {
                for (i32 k = 0; k < inner.segment_count - j; k++)
                {
                    ((LaserCurveSegment *)unk_1524)[k] = ((LaserCurveSegment *)unk_1524)[k + j];
                }
                inner.segment_count -= j;
            }
        }
    }
    return count;
}

// AnmLoaded::create_vm as LTCG inlined it into some callers.
static __forceinline AnmId create_vm_inline(AnmLoaded *anm, i32 script, D3DXVECTOR3 *pos, f32 rotation, i32 layer)
{
    ENTER_CS(CS_ANM_MANAGER);
    anm->vm_count++;
    AnmVm *vm = g_AnmManager->allocate_vm();
    anm->copy_vm(vm, script);
    vm->flags_hi |= ANM_VM_CREATED_BY_GAME;
    if (layer >= 0)
    {
        vm->layer = layer;
        if (layer <= 23)
        {
            vm->flags_hi &= ~ANM_VM_LAYER_UI;
            vm->flags_hi |= ANM_VM_LAYER_SET;
        }
    }
    if (pos == NULL)
    {
        vm->entity_pos = D3DXVECTOR3(0.0f, 0.0f, 0.0f);
    }
    else
    {
        vm->entity_pos = *pos;
    }
    vm->rotation.z = rotation;
    vm->run();
    vm->mode_of_create_child = 0;
    AnmId id;
    id = g_AnmManager->insert_in_world_list_back(vm);
    LEAVE_CS(CS_ANM_MANAGER);
    return id;
}

// Cancels the laser, leaving a cancel effect on every third segment.
// FUNCTION: TH16 0x43a620
i32 LaserCurveInf::cancel(i32 mode, i32 b)
{
    if (b != 0 && countdown_5c8 != 0)
    {
        return 0;
    }
    LaserCurveSegment *segment = (LaserCurveSegment *)unk_1524;
    for (i32 i = 0; i < inner.segment_count; i++, segment++)
    {
        D3DXVECTOR3 pos = segment->pos;
        if (i % 3 == 0)
        {
            AnmLoaded *anm = g_BulletManager->bullet_anm;
            create_vm_inline(anm, inner.color * 2 + 0xd1, &pos, 0.0f, -1);
        }
    }
    state = 1;
    return 0;
}

// 2 if a circle at pos touches the laser's rectangle, else 0.
// TODO: the original loads dx, dy and the sine into registers and multiplies by the cosine in xmm0; ours multiplies from memory.
// FUNCTION: TH16 0x43a760
i32 LaserCurveInf::method_30(Float3 *pos, f32 radius)
{
    f32 dx = pos->x - position.x;
    f32 dy = pos->y - position.y;
    f32 a = -angle;
    f32 s = zun_sinf(a);
    f32 c = zun_cosf(a);
    f32 x = dx * c - dy * s;
    f32 y = dx * s + dy * c;
    D3DXVECTOR2 lo(x - radius, y - radius);
    D3DXVECTOR2 hi(x + radius, y + radius);
    if (lo.x > unk_70 || lo.y > width / 2 || hi.x < 0.0f || hi.y < -width / 2)
    {
        return 0;
    }
    return 2;
}



// Counts down ex_state[11]'s timer; when it runs out, flips ex_flags bit
// 0x100 and returns 1.
// TODO: the original keeps the multiply of the speed by 1.0f (see ZunTimer::operator--).
// FUNCTION: TH16 0x439730
i32 LaserCurveInf::method_60()
{
    ex_state[11].timer.decrement(1.0f);
    if (ex_state[11].timer.current <= 0)
    {
        ex_flags ^= 0x100;
        return 1;
    }
    return 0;
}

// FUNCTION: TH16 0x431190
HARNESS_CALLED LaserCurveNode *LaserCurveInf::append_node(f32 value)
{
    LaserCurveNode *node = &nodes;
    while (node->next != NULL)
    {
        node = node->next;
    }
    node->next = new LaserCurveNode;
    node->unk_c = value;
    node->next->unk_8 = value;
    node->next->next = NULL;
    node->next->prev = node;
    return node->next;
}

// FUNCTION: TH16 0x43a840
i32 __fastcall LaserCurveInf::on_sprite_set(AnmVm *vm, i32 sprite)
{
    return ((LaserCurveInf *)vm->associated_game_entity)->bullet_color + 0x20c;
}
