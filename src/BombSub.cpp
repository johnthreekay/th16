// The season releases. Each one runs two ANM scripts from the subseason
// file (pl0Xsub.anm) around the player: an inner circle, whose scale is the
// damage and cancel radius, and an outer ring. The scripts take their radii
// (float variables 0 and 1) and duration (int variable 3) from the tables
// below, by the season level the release started at. The release ends with
// the inner VM and then cools down. Its cancels are release cancels, which
// drop PIV items worth more at higher levels.
#include "Bomb.h"

#include "BulletManager.h"
#include "EnemyManager.h"
#include "Globals.h"
#include "Laser.h"
#include "Player.h"
#include "SoundManager.h"
#include "ZunMath.h"

// Per season level: how long the release lasts and how far it reaches
// (ExpHP's names; "fall" is autumn).

// GLOBAL: TH16 0x491e70
const i32 g_release_duration_doyou[7] = {0, 40, 40, 40, 40, 40, 40};
// GLOBAL: TH16 0x491e8c
const f32 g_release_radius_2_doyou[7] = {0.0f, 30.0f, 30.0f, 30.0f, 30.0f, 30.0f, 30.0f};
// GLOBAL: TH16 0x491eb0
const f32 g_release_radius_doyou[7] = {0.0f, 50.0f, 52.0f, 54.0f, 56.0f, 58.0f, 60.0f};
// GLOBAL: TH16 0x491ed4
const i32 g_release_duration_fall[7] = {0, 40, 55, 70, 90, 110, 130};
// GLOBAL: TH16 0x491ef0
const f32 g_release_radius_2_fall[7] = {0.0f, 40.0f, 40.0f, 40.0f, 40.0f, 40.0f, 40.0f};
// GLOBAL: TH16 0x491f14
const f32 g_release_radius_fall[7] = {0.0f, 60.0f, 60.0f, 60.0f, 60.0f, 60.0f, 60.0f};
// GLOBAL: TH16 0x491f38
const i32 g_release_duration_summer[7] = {0, 30, 30, 30, 30, 30, 30};
// GLOBAL: TH16 0x491f54
const f32 g_release_radius_summer[7] = {0.0f, 40.0f, 42.0f, 44.0f, 46.0f, 48.0f, 50.0f};
// GLOBAL: TH16 0x491f78
const i32 g_release_duration_winter[7] = {0, 30, 70, 110, 150, 170, 190};
// GLOBAL: TH16 0x491f94
const f32 g_release_radius_winter[7] = {0.0f, 30.0f, 35.0f, 40.0f, 45.0f, 50.0f, 60.0f};
// GLOBAL: TH16 0x491fb8
const i32 g_release_duration_spring[7] = {0, 10, 13, 16, 20, 20, 20};
// GLOBAL: TH16 0x491fd4
const f32 g_release_radius_spring[7] = {0.0f, 60.0f, 100.0f, 140.0f, 190.0f, 240.0f, 300.0f};

// Doyou: 10 frames of invincibility; the circle deals 100 a frame.
// FUNCTION: TH16 0x40e0f0
i32 BombAllSubInf::begin()
{
    Player *player = g_Player;
    pos = player->inner.pos;
    angle = -ZUN_PI / 2;
    g_SoundManager.play_sound_centered(74, 0);

    anm_id = player->subseason_anm_file->create_vm(3, &pos, 0.0f, -1, 0);
    AnmVm *vm = get_vm_or_clear(anm_id);
    vm->float_vars[0] = g_release_radius_doyou[g_Globals.season_level()];
    vm->float_vars[1] = g_release_radius_2_doyou[g_Globals.season_level()];
    vm->int_vars[3] = g_release_duration_doyou[g_Globals.season_level()];

    AnmLoaded *anm = g_Player->subseason_anm_file;
    anm_id_secondary = anm->create_vm(4, &pos, 0.0f, -1, 0);
    vm = get_vm_or_clear(anm_id_secondary);
    vm->float_vars[0] = g_release_radius_doyou[g_Globals.season_level()] + 8.0f;
    vm->float_vars[1] = g_release_radius_2_doyou[g_Globals.season_level()] + 8.0f;
    vm->int_vars[3] = g_release_duration_doyou[g_Globals.season_level()];

    g_Player->inner.iframes = 10;
    g_EnemyManager->inner.bomb_count++;
    return 0;
}

// The damage source of each frame lasts that frame only.
// FUNCTION: TH16 0x40e330
i32 BombAllSubInf::on_tick()
{
    AnmVm *vm = get_vm_or_clear(anm_id);
    if (vm == NULL)
    {
        start_release_cooldown();
        return -1;
    }
    D3DXVECTOR3 pos = vm->world_pos();
    g_Player->create_damage_source(&pos, vm->scale.x, 0.0f, 1, 100);
    cancel_bullets();
    return 0;
}

// FUNCTION: TH16 0x40e3a0
i32 BombAllSubInf::on_draw()
{
    return 1;
}

// FUNCTION: TH16 0x40e3b0
i32 BombAllSubInf::compute_damage(i32 enemy_pos, i32 enemy_size)
{
    return 0;
}

// The releases' cancel_bullets: everything inside the inner circle.
// FUNCTION: TH16 0x40e3c0
i32 BombAllSubInf::cancel_bullets()
{
    AnmVm *vm = get_vm_or_clear(anm_id);
    if (vm == NULL)
    {
        return 0;
    }
    D3DXVECTOR3 pos = vm->world_pos();
    g_BulletManager->cancel_radius_as_bomb(&pos, vm->scale.x, 4);
    g_LaserManager->cancel_in_radius_inline(&pos, vm->scale.x, 4, 1);
    return 0;
}

// FUNCTION: TH16 0x40e480
void BombAllSubInf::end_at_stage_clear()
{
}

// Autumn: 30 frames of invincibility; the circle deals 30 a frame.
// FUNCTION: TH16 0x40ec70
i32 BombAyaSubInf::begin()
{
    Player *player = g_Player;
    pos = player->inner.pos;
    angle = -ZUN_PI / 2;
    g_SoundManager.play_sound_centered(74, 0);

    anm_id = player->subseason_anm_file->create_vm(3, &pos, 0.0f, -1, 0);
    AnmVm *vm = get_vm_or_clear(anm_id);
    vm->float_vars[0] = g_release_radius_fall[g_Globals.season_level()];
    vm->float_vars[1] = g_release_radius_2_fall[g_Globals.season_level()];
    vm->int_vars[3] = g_release_duration_fall[g_Globals.season_level()];

    AnmLoaded *anm = g_Player->subseason_anm_file;
    anm_id_secondary = anm->create_vm(4, &pos, 0.0f, -1, 0);
    vm = get_vm_or_clear(anm_id_secondary);
    vm->float_vars[0] = g_release_radius_fall[g_Globals.season_level()] + 8.0f;
    vm->float_vars[1] = g_release_radius_2_fall[g_Globals.season_level()] + 8.0f;
    vm->int_vars[3] = g_release_duration_fall[g_Globals.season_level()];

    g_Player->inner.iframes = 30;
    g_EnemyManager->inner.bomb_count++;
    return 0;
}

// The autumn release follows the player and speeds them up by half.
// FUNCTION: TH16 0x40eeb0
i32 BombAyaSubInf::on_tick()
{
    AnmVm *vm = get_vm_or_clear(anm_id);
    Player *player = g_Player;
    player->inner.speed_multiplier = 1.5f;
    if (vm == NULL)
    {
        start_release_cooldown();
        return -1;
    }
    vm->entity_pos = player->inner.pos;
    AnmVm *vm_2 = get_vm(anm_id_secondary);
    if (vm_2 != NULL)
    {
        vm_2->entity_pos = player->inner.pos;
    }
    D3DXVECTOR3 pos = vm->world_pos();
    player->create_damage_source(&pos, vm->scale.x, 0.0f, 1, 30);
    cancel_bullets();
    return 0;
}

// FUNCTION: TH16 0x40ef80
i32 BombAyaSubInf::on_draw()
{
    return 1;
}

// FUNCTION: TH16 0x40ef90
i32 BombAyaSubInf::compute_damage(i32 enemy_pos, i32 enemy_size)
{
    return 0;
}

// FUNCTION: TH16 0x40efa0
i32 BombAyaSubInf::cancel_bullets()
{
    AnmVm *vm = get_vm_or_clear(anm_id);
    if (vm == NULL)
    {
        return 0;
    }
    D3DXVECTOR3 pos = vm->world_pos();
    g_BulletManager->cancel_radius_as_bomb(&pos, vm->scale.x, 4);
    g_LaserManager->cancel_in_radius_inline(&pos, vm->scale.x, 4, 1);
    return 0;
}

// FUNCTION: TH16 0x40f060
void BombAyaSubInf::end_at_stage_clear()
{
}

// Summer: 10 frames of invincibility; the circle deals 100 a frame.
// FUNCTION: TH16 0x40f590
i32 BombCirnoSubInf::begin()
{
    Player *player = g_Player;
    pos = player->inner.pos;
    angle = -ZUN_PI / 2;
    g_SoundManager.play_sound_centered(74, 0);

    anm_id = player->subseason_anm_file->create_vm(3, &pos, 0.0f, -1, 0);
    AnmVm *vm = get_vm_or_clear(anm_id);
    vm->float_vars[0] = g_release_radius_summer[g_Globals.season_level()];
    vm->float_vars[1] = g_release_radius_summer[g_Globals.season_level()];
    vm->int_vars[3] = g_release_duration_summer[g_Globals.season_level()];

    AnmLoaded *anm = g_Player->subseason_anm_file;
    anm_id_secondary = anm->create_vm(4, &pos, 0.0f, -1, 0);
    vm = get_vm_or_clear(anm_id_secondary);
    vm->float_vars[0] = g_release_radius_summer[g_Globals.season_level()] + 8.0f;
    vm->float_vars[1] = g_release_radius_summer[g_Globals.season_level()] + 8.0f;
    vm->int_vars[3] = g_release_duration_summer[g_Globals.season_level()];

    g_Player->inner.iframes = 10;
    g_EnemyManager->inner.bomb_count++;
    return 0;
}

// FUNCTION: TH16 0x40f7d0
i32 BombCirnoSubInf::on_tick()
{
    AnmVm *vm = get_vm_or_clear(anm_id);
    if (vm == NULL)
    {
        start_release_cooldown();
        return -1;
    }
    D3DXVECTOR3 pos = vm->world_pos();
    g_Player->create_damage_source(&pos, vm->scale.x, 0.0f, 1, 100);
    cancel_bullets();
    return 0;
}

// FUNCTION: TH16 0x40f840
i32 BombCirnoSubInf::on_draw()
{
    return 1;
}

// FUNCTION: TH16 0x40f850
i32 BombCirnoSubInf::compute_damage(i32 enemy_pos, i32 enemy_size)
{
    return 0;
}

// FUNCTION: TH16 0x40f860
i32 BombCirnoSubInf::cancel_bullets()
{
    AnmVm *vm = get_vm_or_clear(anm_id);
    if (vm == NULL)
    {
        return 0;
    }
    D3DXVECTOR3 pos = vm->world_pos();
    g_BulletManager->cancel_radius_as_bomb(&pos, vm->scale.x, 4);
    g_LaserManager->cancel_in_radius_inline(&pos, vm->scale.x, 4, 1);
    return 0;
}

// FUNCTION: TH16 0x40f920
void BombCirnoSubInf::end_at_stage_clear()
{
}

// Winter: 30 frames of invincibility; the circle deals 9 a frame, and the
// outer ring is a fifth larger.
// FUNCTION: TH16 0x410150
i32 BombMarisaSubInf::begin()
{
    Player *player = g_Player;
    pos = player->inner.pos;
    angle = -ZUN_PI / 2;
    g_SoundManager.play_sound_centered(74, 0);

    anm_id = player->subseason_anm_file->create_vm(20, &pos, 0.0f, -1, 0);
    AnmVm *vm = get_vm_or_clear(anm_id);
    vm->float_vars[0] = g_release_radius_winter[g_Globals.season_level()];
    vm->float_vars[1] = g_release_radius_winter[g_Globals.season_level()];
    vm->int_vars[3] = g_release_duration_winter[g_Globals.season_level()];

    AnmLoaded *anm = g_Player->subseason_anm_file;
    anm_id_secondary = anm->create_vm(21, &pos, 0.0f, -1, 0);
    vm = get_vm_or_clear(anm_id_secondary);
    vm->float_vars[0] = g_release_radius_winter[g_Globals.season_level()] +
                        g_release_radius_winter[g_Globals.season_level()] * 0.2f;
    vm->float_vars[1] = g_release_radius_winter[g_Globals.season_level()] +
                        g_release_radius_winter[g_Globals.season_level()] * 0.2f;
    vm->int_vars[3] = g_release_duration_winter[g_Globals.season_level()];

    g_Player->inner.iframes = 30;
    g_EnemyManager->inner.bomb_count++;
    return 0;
}

// The winter release raises the player's damage by half while it lasts.
// FUNCTION: TH16 0x4103e0
i32 BombMarisaSubInf::on_tick()
{
    AnmVm *vm = get_vm_or_clear(anm_id);
    Player *player = g_Player;
    player->damage_multiplier = 1.5f;
    if (vm == NULL)
    {
        start_release_cooldown();
        return -1;
    }
    D3DXVECTOR3 pos = vm->world_pos();
    player->create_damage_source(&pos, vm->scale.x, 0.0f, 1, 9);
    cancel_bullets();
    return 0;
}

// FUNCTION: TH16 0x410460
i32 BombMarisaSubInf::on_draw()
{
    return 1;
}

// FUNCTION: TH16 0x410470
i32 BombMarisaSubInf::compute_damage(i32 enemy_pos, i32 enemy_size)
{
    return 0;
}

// FUNCTION: TH16 0x410480
i32 BombMarisaSubInf::cancel_bullets()
{
    AnmVm *vm = get_vm_or_clear(anm_id);
    if (vm == NULL)
    {
        return 0;
    }
    D3DXVECTOR3 pos = vm->world_pos();
    g_BulletManager->cancel_radius_as_bomb(&pos, vm->scale.x, 4);
    g_LaserManager->cancel_in_radius_inline(&pos, vm->scale.x, 4, 1);
    return 0;
}

// FUNCTION: TH16 0x410540
void BombMarisaSubInf::end_at_stage_clear()
{
}

// Spring: the widest circle; it deals 100 a frame.
// FUNCTION: TH16 0x411460
i32 BombReimuSubInf::begin()
{
    Player *player = g_Player;
    pos = player->inner.pos;
    angle = -ZUN_PI / 2;
    g_SoundManager.play_sound_centered(74, 0);

    anm_id = player->subseason_anm_file->create_vm(3, &pos, 0.0f, -1, 0);
    AnmVm *vm = get_vm_or_clear(anm_id);
    vm->float_vars[0] = g_release_radius_spring[g_Globals.season_level()];
    vm->float_vars[1] = g_release_radius_spring[g_Globals.season_level()];
    vm->int_vars[3] = g_release_duration_spring[g_Globals.season_level()];

    AnmLoaded *anm = g_Player->subseason_anm_file;
    anm_id_secondary = anm->create_vm(4, &pos, 0.0f, -1, 0);
    vm = get_vm_or_clear(anm_id_secondary);
    vm->float_vars[0] = g_release_radius_spring[g_Globals.season_level()] + 16.0f;
    vm->float_vars[1] = g_release_radius_spring[g_Globals.season_level()] + 16.0f;
    vm->int_vars[3] = g_release_duration_spring[g_Globals.season_level()];

    g_Player->inner.iframes = 10;
    g_EnemyManager->inner.bomb_count++;
    return 0;
}

// The spring release keeps the player invincible and only cancels bullets
// in its first frames.
// FUNCTION: TH16 0x4116a0
i32 BombReimuSubInf::on_tick()
{
    AnmVm *vm = get_vm_or_clear(anm_id);
    Player *player = g_Player;
    player->inner.iframes = 50;
    if (vm == NULL)
    {
        start_release_cooldown();
        return -1;
    }
    D3DXVECTOR3 pos = vm->world_pos();
    player->create_damage_source(&pos, vm->scale.x, 0.0f, 1, 100);
    if (timer.current <= 15)
    {
        cancel_bullets();
    }
    return 0;
}

// FUNCTION: TH16 0x411770
i32 BombReimuSubInf::on_draw()
{
    return 1;
}

// FUNCTION: TH16 0x411780
i32 BombReimuSubInf::compute_damage(i32 enemy_pos, i32 enemy_size)
{
    return 0;
}

// FUNCTION: TH16 0x411790
i32 BombReimuSubInf::cancel_bullets()
{
    AnmVm *vm = get_vm_or_clear(anm_id);
    if (vm == NULL)
    {
        return 0;
    }
    D3DXVECTOR3 pos = vm->world_pos();
    g_BulletManager->cancel_radius_as_bomb(&pos, vm->scale.x, 4);
    g_LaserManager->cancel_in_radius_inline(&pos, vm->scale.x, 4, 1);
    return 0;
}

// FUNCTION: TH16 0x411850
void BombReimuSubInf::end_at_stage_clear()
{
}
