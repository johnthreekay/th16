// Stand-in callers for unit 12b (0x401000-0x411860, second pass) functions
// whose shape depends on code not decompiled yet.
#include "../BulletManager.h"
#include "../Collision.h"
#include "../EffectManager.h"
#include "../Input.h"
#include "../Player.h"
#include "../Stage.h"
#include "../ZunMath.h"

// The menus (0x45108b, 0x452801) read the keyboard into global buffers.
i32 harness_get_keyboard_state(u8 *keys)
{
    return get_keyboard_state(keys);
}

// The game thread destroys the stage (0x42d3bd, ...) and starts the stage
// transition (begin_stage); ECL jumps the stage script (0x422777) and
// starts the fog interpolation (0x42283f, with varying values).
void harness_stage(const char *path, i32 n, CameraSky *sky)
{
    g_Stage->jump_to_label(n);
    g_Stage->start_enter();
    g_Stage2->start_exit();
    g_Stage->inner.set_sky_interp(n, n + 1, sky);
    delete g_Stage;
}

// Other bullet cancels (the player around 0x442669) use other modes.
void harness_cancel_rectangle(D3DXVECTOR3 *pos, D3DXVECTOR3 *size, f32 angle, i32 mode)
{
    g_BulletManager->cancel_rectangle_as_bomb(pos, size, angle, mode);
}

// ECL (0x41e464) creates tracked effects too.
i32 harness_create_tracked_effect(i32 effect, D3DXVECTOR3 *pos)
{
    return g_EffectManager->create_tracked(effect, pos, 0);
}

// The player's own shots (PlayerBullet::create, 0x44529f) make other
// rectangles.
void harness_rect_damage_source(D3DXVECTOR3 *pos, f32 w, f32 h, f32 angle, i32 a, i32 b)
{
    g_Player->create_rect_damage_source(pos, w, h, angle, a, b);
}

// Bullets (0x416eb6, 0x416f0b) and the player (0x445c30) test circles
// against rectangles.
i32 harness_collision(f32 *a, f32 *b, f32 angle, f32 r)
{
    return collision_test_circle_rect(a[0], a[1], b[0], b[1], angle, a[2], b[2], r) +
           collision_test_circle_rect(b[0], b[1], a[0], a[1], r, b[2], a[2], angle);
}

// An unaligned caller of zun_atan2f, like most of its callers in the
// original, so it realigns its own frame.
f32 harness_sin_cos(f32 x)
{
    return zun_atan2f(x, x + 1.0f);
}
