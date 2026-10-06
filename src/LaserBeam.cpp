#include "Laser.h"

// FUNCTION: TH16 0x43ac00
i32 LaserBeamInf::on_tick()
{
    return 0;
}

// FUNCTION: TH16 0x43ac10
i32 LaserBeamInf::on_draw()
{
    return 0;
}

// FUNCTION: TH16 0x43ac20
i32 LaserBeamInf::on_destroy()
{
    return 0;
}

// FUNCTION: TH16 0x43ac30
i32 LaserBeamInf::method_1c(i32 a, i32 b, i32 c, i32 d, i32 e, i32 f)
{
    return 0;
}

// FUNCTION: TH16 0x43ac40
i32 LaserBeamInf::cancel(i32 mode, i32 b)
{
    if (b == 0)
    {
        pending_delete = 1;
    }
    return 0;
}

// FUNCTION: TH16 0x43ac60
i32 LaserBeamInf::method_30(i32 a, i32 b)
{
    return 0;
}

// FUNCTION: TH16 0x43ac70
void LaserBeamInf::run_ex()
{
}

// Placeholder for 0x43a860 (not decompiled yet).
i32 LaserBeamInf::initialize(void *params)
{
    return laser_placeholder(this);
}
