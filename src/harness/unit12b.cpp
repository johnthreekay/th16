// A stand-in caller for collision_test_circle_rect.
//
// Not ZUN's code and never run: it gives link-time code generation calls
// or address uses the decompiled code does not have, so that it compiles
// the real functions as in the original (see docs/workflow.md, "Placeholders and
// stand-in callers"). The harness files are split the way the work was;
// regrouping them changes LTCG's choices for unrelated functions.
#include "../BulletManager.h"
#include "../Collision.h"
#include "../EffectManager.h"
#include "../Input.h"
#include "../Player.h"
#include "../Stage.h"
#include "../ZunMath.h"

// Stands for the bullets' and the player's circle tests (0x416eb6,
// 0x416f0b, 0x445c30). With only the decompiled calls in view, LTCG treats
// collision_test_circle_rect differently, and
// BulletManager::cancel_rectangle_as_bomb (0x416e20) calls it differently.
i32 harness_collision(f32 *a, f32 *b, f32 angle, f32 r)
{
    return collision_test_circle_rect(a[0], a[1], b[0], b[1], angle, a[2], b[2], r) +
           collision_test_circle_rect(b[0], b[1], a[0], a[1], r, b[2], a[2], angle);
}
