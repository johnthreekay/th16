// Stand-in callers for the wave 4 interpreter work (EnemyData's ECL
// instructions).
#include <math.h>

#include "../ZunMath.h"

// More of the undecompiled atan2f users (see harness_homing_angle): the
// ECL instructions added enough code without double temporaries that LTCG
// stopped inlining the CRT math helpers into zun_fabsf, zun_cosf and
// shoot_bullets.
f32 harness_w4a_atan2(Float3 *a, Float3 *b)
{
    return atan2f(b->y - a->y, b->x - a->x) + atan2f(a->y - b->y, a->x - b->x);
}
