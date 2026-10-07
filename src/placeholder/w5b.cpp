// Stand-ins compiled with /GL for functions that wave 5 range B
// (0x420000-0x440000) calls but are not decompiled, where LTCG has to see a
// body (custom conventions). Each forwards to an opaque stub.
#include <math.h>

#include "../Laser.h"
#include "../Player.h"

int w5b_placeholder_sink(void *object, int value);

// STUB: TH16 0x445a30
HARNESS_CALLED i32 Player::compute_damage_to_enemy(Float3 *pos, Float2 *size, f32 angle, f32 radius, i32 *hit, i32 unk_5,
                                                  i32 unk_6, i32 enemy_id)
{
    // The real body needs an 8-aligned frame, which spreads to its callers.
    *hit = (i32)atan2f(angle, radius) + w5b_placeholder_sink(pos, w5b_placeholder_sink(size, (i32)(radius + angle) + unk_5 + unk_6 + enemy_id));
    return w5b_placeholder_sink(this, *hit);
}

// STUB: TH16 0x404220
HARNESS_CALLED i32 __stdcall line_intersection(f32 *out_x, f32 *out_y, f32 x1, f32 y1, f32 angle1, f32 x2, f32 y2,
                                               f32 angle2)
{
    *out_x = x1 + x2 * angle1;
    *out_y = y1 + y2 * angle2;
    return w5b_placeholder_sink(out_x, (i32)*out_y);
}
