// Stand-in callers for wave 3, range A (0x401000-0x4190b0).
#include "../AnmManager.h"
#include "../BulletManager.h"
#include "../EffectManager.h"
#include "../Enemy.h"
#include "../Fog.h"
#include "../Gui.h"
#include "../PosVel.h"
#include "../Spellcard.h"
#include "../Supervisor.h"
#include "../ZunMath.h"

// Takes the address of a local double (src/stub/w4b.cpp), which makes the
// caller's frame 8-byte aligned.
void w4b_opaque_double(double *value);

// Like the main menu (0x450d75, 0x4512f2, ...), which starts its cursor
// effect this way.
i32 harness_menu_ui_effect()
{
    return g_EffectManager->create_ui_effect(0, NULL, NULL).id + g_EffectManager->create_ui_effect(0, NULL, NULL).id;
}

// Like the pause menu (0x43ef5b, 0x43f159, ...) and the HUD (0x426dc0).
i32 harness_create_ui_vm_at_origin(i32 script)
{
    return g_Supervisor.text_anm->create_ui_vm_at_origin(0x34, 0).id + g_Supervisor.text_anm->create_ui_vm_at_origin(script, 0).id;
}

// Like StageInner::run_std (0x40bad2) and ECL (0x42275d), which create
// fogs with 7 and 17 points per strip.
Fog *harness_new_fog(i32 enemy)
{
    if (enemy)
    {
        return new Fog(0, 0x11, 0);
    }
    return new Fog(0, 7, 0);
}

// Like EnemyData::update_fog (0x41cc94) and StageInner::step_fog (0x40c527).
void harness_fog_set_rect(Fog *fog, f32 x, f32 y, f32 r)
{
    fog->set_rect(x - r - 16.0f, y - r - 16.0f, r + r + 40.0f, r + r + 40.0f);
    fog->set_rect(x, y, r, r * 2.0f);
}

// The resolution and game-area origin have their addresses taken elsewhere
// in the game (the window setup code), so stores through pointers may change
// them: Fog::set_rect reloads them inside its loops.
i32 *harness_screen_metric_ptr(i32 which)
{
    switch (which)
    {
    case 0:
        return &g_resolution_x;
    case 1:
        return &g_resolution_y;
    case 2:
        return &g_game_2d_origin_x;
    default:
        return &g_game_2d_origin_y;
    }
}

// Like the other fan drawers (0x469890 has one caller in the original, but
// LTCG must see it called through g_AnmManager).
void harness_draw_triangle_fan(i32 n, Float3 *center, Float2 *offsets, ZunColor *colors)
{
    g_AnmManager->draw_triangle_fan(n, center, offsets, colors);
}

// Like the bombs (0x4106e1) and ECL (0x41eff8), which point a PosVel at an
// angle.
void harness_posvel_set_angle(PosVel *pv, f32 angle)
{
    pv->set_angle(angle);
    pv->set_angle(angle * 2.0f);
}

// Like the spell card code (0x41804c, 0x418346). Its frames are 8-byte
// aligned, so LTCG knows the stack is aligned in these two, which call
// AnmVm::run (which needs it) without realigning their own frames.
void harness_gui_spell_vms()
{
    double aligned;
    w4b_opaque_double(&aligned);
    g_Gui->interrupt_spell_vms_2();
    g_Gui->interrupt_spell_vms_3();
    w4b_opaque_double(&aligned);
}

// Bullet code passes the zero vector by address (0x412282, 0x412605), so
// stores through pointers may change it as far as LTCG knows.
Float3 *harness_zero_vec_ptr()
{
    return &g_zero_vec;
}

// Like AnmManager::draw_vm (0x468754) and the sprite corner writers
// (0x465fb8), which place a VM by its transformed position.
f32 harness_vm_transformed_x(AnmVm *vm)
{
    Float3 pos;
    vm->get_own_transformed_pos(&pos);
    return pos.x + pos.y;
}

// Like ECL's bullet cancel instructions (0x4221b8, 0x422237).
void harness_cancel_radius(D3DXVECTOR3 *pos, f32 radius, i32 mode)
{
    g_BulletManager->cancel_radius(pos, radius, mode);
    g_BulletManager->cancel_radius(pos, radius * 2.0f, mode + 1);
}

// Like Bullet::cancel (0x41690c), BulletManager::clear_all (0x41703e) and
// the lasers' cancels (0x433b43, ...).
void harness_gen_items_from_cancel(D3DXVECTOR3 *pos, i32 mode)
{
    gen_items_from_cancel(pos, mode);
    gen_items_from_cancel(pos + 1, mode + 1);
}

// Like ECL's spell card end instruction (0x421891).
void harness_spellcard_end()
{
    g_Spellcard->end();
}

// Like ECL (0x41f251, 0x4200e2, ...) and the HUD (0x428c63).
f32 harness_fabsf(f32 x, f32 y)
{
    return zun_fabsf(x) + zun_fabsf(y - x);
}

// Like ECL's shoot instructions (0x420aa0) and the lasers' transforms
// (0x432465, 0x4391b0).
void harness_shoot_bullets(EnemyBulletShooter *props)
{
    g_BulletManager->shoot_bullets(props);
    g_BulletManager->shoot_bullets(props + 1);
}

// Like ECL's bullet clear instructions (0x421656, 0x4216c9).
void harness_clear_bullets()
{
    g_BulletManager->clear_all(0);
}
