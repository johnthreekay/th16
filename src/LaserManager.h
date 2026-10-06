#pragma once

#include <d3dx9math.h>

#include "types.h"

// Partial: only what unit 2 (Stage, Bomb) uses so far. Layouts from ExpHP.

// Base of the laser classes. Only the virtual function unit 2 calls is
// named; the others are placeholders so it lands in the right slot.
class LaserBaseClass
{
  public:
    virtual void vfunc_0() = 0;
    virtual void vfunc_4() = 0;
    virtual void vfunc_8() = 0;
    virtual void vfunc_c() = 0;
    virtual void vfunc_10() = 0;
    virtual void vfunc_14() = 0;
    virtual void vfunc_18() = 0;
    virtual void vfunc_1c() = 0;
    virtual void vfunc_20() = 0;
    virtual void cancel_in_radius(D3DXVECTOR3 *pos, f32 radius, i32 a, i32 b) = 0;

    u32 unk_4;
    LaserBaseClass *next;
    u32 unk_c;
    i32 state;
};

struct LaserManager
{
    u8 unk_0[0x14];
    // next of the dummy laser at 0xc that heads the list.
    LaserBaseClass *list_head;
    u8 unk_18[0x5ec - 0x18];
    // Center of the last radius cancel.
    D3DXVECTOR3 cancel_pos;
    u8 unk_5f8[0x610 - 0x5f8];

    void cancel_in_radius(D3DXVECTOR3 *pos, f32 radius, i32 a, i32 b)
    {
        cancel_pos = *pos;
        LaserBaseClass *laser = list_head;
        while (laser != NULL)
        {
            LaserBaseClass *next = laser->next;
            if (laser->state != 1)
            {
                laser->cancel_in_radius(pos, radius, a, b);
            }
            laser = next;
        }
    }
};

extern LaserManager *g_LaserManager;
