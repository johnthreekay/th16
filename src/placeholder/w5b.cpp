// Stand-ins compiled with /GL for functions that wave 5 range B
// (0x420000-0x440000) calls but are not decompiled, where LTCG has to see a
// body (custom conventions). Each forwards to an opaque stub.
#include <math.h>

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
