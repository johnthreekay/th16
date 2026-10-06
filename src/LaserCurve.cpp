#include <stdlib.h>

#include "Laser.h"

// Placeholder for 0x4370a0 (not decompiled yet).
i32 LaserCurveInf::initialize(void *params)
{
    return unit5_placeholder(this);
}

// Placeholder for 0x438cb0 (not decompiled yet).
void LaserCurveInf::run_ex()
{
    unit5_placeholder(this);
}

// Placeholder for 0x4377d0 (not decompiled yet).
i32 LaserCurveInf::on_tick()
{
    return unit5_placeholder(this);
}

// Placeholder for 0x438750 (not decompiled yet).
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

// Placeholder for 0x439d60 (not decompiled yet).
i32 LaserCurveInf::method_1c(i32 a, i32 b, i32 c, i32 d, i32 e, i32 f)
{
    return unit5_placeholder(this);
}

// Placeholder for 0x4397d0 (not decompiled yet).
i32 LaserCurveInf::cancel_as_bomb_rectangle(Float3 *a, Float3 *b, f32 angle, i32 d, i32 e)
{
    return unit5_placeholder(this);
}

// Placeholder for 0x43a2f0 (not decompiled yet).
i32 LaserCurveInf::cancel_as_bomb_circle(Float3 *pos, f32 radius, i32 c, i32 d)
{
    return unit5_placeholder(this);
}

// Placeholder for 0x43a620 (not decompiled yet).
i32 LaserCurveInf::cancel(i32 mode, i32 b)
{
    return unit5_placeholder(this);
}

// Placeholder for 0x43a760 (not decompiled yet).
i32 LaserCurveInf::method_30(i32 a, i32 b)
{
    return unit5_placeholder(this);
}

// Placeholder for 0x437cf0 (not decompiled yet).
i32 LaserCurveInf::check_graze_or_kill(i32 a)
{
    return unit5_placeholder(this);
}

// Placeholder for 0x4395b0 (not decompiled yet).
i32 LaserCurveInf::method_3c()
{
    return unit5_placeholder(this);
}

// Placeholder for 0x439460 (not decompiled yet).
i32 LaserCurveInf::method_40()
{
    return unit5_placeholder(this);
}

// Placeholder for 0x4392c0 (not decompiled yet).
i32 LaserCurveInf::method_44()
{
    return unit5_placeholder(this);
}

// Placeholder for 0x439730 (not decompiled yet).
i32 LaserCurveInf::method_60()
{
    return unit5_placeholder(this);
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
