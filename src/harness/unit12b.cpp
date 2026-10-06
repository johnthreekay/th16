// Stand-in callers for unit 12b (0x401000-0x411860, second pass) functions
// whose shape depends on code not decompiled yet.
#include "../BulletManager.h"
#include "../Input.h"
#include "../Stage.h"

// The menus (0x45108b, 0x452801) read the keyboard into global buffers.
i32 harness_get_keyboard_state(u8 *keys)
{
    return get_keyboard_state(keys);
}

// The game thread creates the stage (0x42cfea, 0x42d040) and destroys it
// (0x42d3bd, ...); ECL jumps the stage script (0x422777) and starts the
// fog interpolation (0x42283f, with varying values).
void harness_stage(const char *path, i32 n, CameraSky *sky)
{
    Stage::create(path);
    g_Stage->start_std_vms();
    g_Stage->jump_to_label(n);
    g_Stage->start_fade_out();
    g_Stage2->start_fade_in();
    g_Stage->inner.set_sky_interp(n, n + 1, sky);
    delete g_Stage;
}

// ECL builds fog colors (0x422823) and steps them (InterpCameraSky::step).
CameraSky harness_camera_sky(f32 a, f32 b, f32 c, f32 d, f32 e, f32 f, CameraSky *other)
{
    return CameraSky(a, b, c, d, e, f) + *other;
}

// Other bullet cancels (the player around 0x442669) use other modes.
void harness_cancel_rectangle(D3DXVECTOR3 *pos, D3DXVECTOR3 *size, f32 angle, i32 mode)
{
    g_BulletManager->cancel_rectangle_as_bomb(pos, size, angle, mode);
}
