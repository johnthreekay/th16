// Stand-in callers for wave 3 range B (0x4190b0-0x42b480), for functions
// whose shape depends on callers that are not decompiled yet.
#include "../AsciiManager.h"
#include "../EffectManager.h"
#include "../EnemyManager.h"
#include "../Item.h"
#include "../Laser.h"

// GameThread's destructor and the menus show and hide the "now loading"
// animation; the original passes the coordinates in xmm1 and xmm2.
void harness_w3b_now_loading(f32 x, f32 y, i32 hide)
{
    if (hide)
    {
        g_AsciiManager->hide_now_loading();
    }
    else
    {
        g_AsciiManager->show_now_loading(x, y);
    }
}

// Bullet cancels, lasers and the ECL spawn items with all kinds of
// arguments; spawn_item reaches the manager through g_ItemManager.
Item *harness_w3b_spawn_item(i32 type, Float3 *pos, f32 angle, f32 speed, i32 force_autocollect)
{
    return g_ItemManager->spawn_item(type, pos, 0, angle, speed, 0, force_autocollect);
}

// The ECL and the lasers' bomb cancels track VMs they created themselves.
i32 harness_w3b_track(AnmLoaded *anm, i32 script, Float3 *pos)
{
    return g_EffectManager->track(anm->create_vm(script, pos, 0.0f, -1, 0));
}

// The ECL looks lasers up by id.
LaserDataInf *harness_w3b_find_laser(i32 id)
{
    return g_LaserManager->find_by_id(id, 0);
}


// The ECL's anm instructions and the player's options start VMs at the
// front of the world list.
AnmId harness_w3b_create_vm_front(AnmLoaded *anm, i32 script, i32 layer)
{
    return anm->create_vm_front(script, layer, 0);
}

// The ECL reads the bosses' variables.
EnemyInf *harness_w3b_get_boss(i32 i)
{
    return g_EnemyManager->get_boss(i);
}

// The ECL reads the enemy count.
int harness_w3b_get_enemy_count()
{
    return g_EnemyManager->get_enemy_count();
}
