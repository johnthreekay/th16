#pragma once

#include <string.h>

#include "UpdateFunc.h"
#include "decomp.h"
#include "types.h"

// Measures the frame rate and draws it in the corner (ExpHP: zFpsCounter).
struct FpsCounter
{
    u32 flags;
    UpdateFunc *on_tick;
    UpdateFunc *on_draw;
    u8 unk_c[4];
    // Seconds since startup when the current measurement began.
    double last_time;
    // Measurements in a row above 65 fps.
    i32 too_fast_count;
    u32 frame_count;
    double total_actual;
    double total_expected;
    f32 fps;
    u8 unk_34[0x88 - 0x34];

    // Inlined into Supervisor::on_registration.
    FpsCounter()
    {
        memset(this, 0, sizeof(FpsCounter));
        flags |= 2;
    }
    ~FpsCounter();
    HARNESS_CALLED int update();
    int draw();
    static int __fastcall on_draw_callback(FpsCounter *counter);
};

extern FpsCounter *g_FpsCounter;
