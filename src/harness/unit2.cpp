// Stand-in callers for unit 2 (Stage, Bomb) functions whose shape depends
// on code not decompiled yet.
#include "../BulletManager.h"
#include "../Player.h"
#include "../SoundManager.h"
#include "../ZunTimer.h"

// Callers of ZunTimer::decrement all over the game (0x40a6d9, 0x412146, ...)
// pass 1.
void harness_timer_decrement(ZunTimer *t)
{
    (*t)--;
}

// ZunTimer::set_value is called with many different values (0x40dc23 passes
// 60), so LTCG must not fold its argument.
void harness_timer_set_value(ZunTimer *t, i32 value)
{
    t->set_value(value);
}

// Bullet cancels elsewhere use other modes than the releases' 4.
void harness_cancel_radius_as_bomb(D3DXVECTOR3 *pos, f32 radius, i32 mode)
{
    g_BulletManager->cancel_radius_as_bomb(pos, radius, mode);
}

// Other sounds, ANM scripts and damage sources than the bombs' constants,
// so LTCG keeps those arguments (only the last create_vm argument is the
// same everywhere in the original).
void harness_varied_calls(i32 n, f32 f, D3DXVECTOR3 *pos)
{
    g_SoundManager.play_sound_centered(n, 0);
    g_Player->subseason_anm_file->create_vm(n, pos, f, n, 0);
    g_Player->create_damage_source(pos, f, f, n, n);
}
