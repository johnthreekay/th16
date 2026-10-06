// Shot type callbacks: the functions a .sht file's shooters name by index
// (g_sht_*_funcs), called with the bullet in ecx.
#include "Player.h"

#include "EffectManager.h"
#include "Rng.h"
#include "SoundManager.h"

i32 __fastcall sht_on_init_445ed0(PlayerBullet *bullet);
i32 __fastcall sht_on_init_446200(PlayerBullet *bullet);
i32 __fastcall sht_on_init_4470e0(PlayerBullet *bullet);
i32 __fastcall sht_on_init_447450(PlayerBullet *bullet);
i32 __fastcall sht_on_init_4474d0(PlayerBullet *bullet);
i32 __fastcall sht_on_tick_445ee0(PlayerBullet *bullet);
i32 __fastcall sht_on_tick_446260(PlayerBullet *bullet);
i32 __fastcall sht_on_tick_446e00(PlayerBullet *bullet);
i32 __fastcall sht_on_tick_4470f0(PlayerBullet *bullet);
i32 __fastcall sht_on_tick_447480(PlayerBullet *bullet);
i32 __fastcall sht_on_hit_4460c0(PlayerBullet *bullet, i32 unk, i32 enemy, f32 x, f32 y);
i32 __fastcall sht_on_hit_446870(PlayerBullet *bullet, i32 unk, i32 enemy, f32 x, f32 y);
i32 __fastcall sht_on_hit_446e20(PlayerBullet *bullet, i32 unk, i32 enemy, f32 x, f32 y);
i32 __fastcall sht_on_hit_446f80(PlayerBullet *bullet, i32 unk, i32 enemy, f32 x, f32 y);
i32 __fastcall sht_on_hit_447270(PlayerBullet *bullet, i32 unk, i32 enemy, f32 x, f32 y);
i32 __fastcall sht_on_hit_447320(PlayerBullet *bullet, i32 unk, i32 enemy, f32 x, f32 y);
i32 __fastcall damage_source_on_hit_445d40(PlayerDamageSource *source, i32 unk, i32 enemy, f32 x, f32 y);
i32 __fastcall damage_source_on_hit_4474a0(PlayerDamageSource *source, i32 unk, i32 enemy, f32 x, f32 y);

// GLOBAL: TH16 0x4919c0
ShtBulletFunc const g_sht_on_init_funcs[7] = {
    NULL,
    sht_on_init_445ed0,
    sht_on_init_446200,
    sht_on_init_4470e0,
    sht_on_init_447450,
    sht_on_init_4474d0,
    NULL,
};

// GLOBAL: TH16 0x4919a0
ShtBulletFunc const g_sht_on_tick_funcs[8] = {
    NULL,
    sht_on_tick_445ee0,
    sht_on_tick_446260,
    sht_on_tick_446e00,
    sht_on_tick_4470f0,
    sht_on_tick_447480,
    NULL,
    NULL,
};

// GLOBAL: TH16 0x491980
ShtHitFunc const g_sht_on_hit_funcs[8] = {
    NULL,
    sht_on_hit_4460c0,
    sht_on_hit_446870,
    sht_on_hit_446e20,
    sht_on_hit_446f80,
    sht_on_hit_447270,
    sht_on_hit_447320,
    NULL,
};

// Nothing uses the third callback; its table has only the empty entry.
// GLOBAL: TH16 0x4a6f04
ShtBulletFunc g_sht_func_3_table[1];

// GLOBAL: TH16 0x4919dc
DamageSourceHitFunc const g_damage_source_hit_funcs[4] = {
    NULL,
    damage_source_on_hit_445d40,
    damage_source_on_hit_4474a0,
    NULL,
};

// FUNCTION: TH16 0x445ed0
i32 __fastcall sht_on_init_445ed0(PlayerBullet *bullet)
{
    bullet->unk_90 = 0;
    return 0;
}

// FUNCTION: TH16 0x446200
i32 __fastcall sht_on_init_446200(PlayerBullet *bullet)
{
    Player *player = g_Player;
    bullet->unk_a0 = 0;
    g_SoundManager.play_sound_at_position(0x14, player->inner.pos.x);
    // The original reuses g_Player across the sound call, which LTCG knows
    // leaves it alone; with the sound code still a placeholder, spell that out.
    PlayerDamageSource *source =
        bullet->damage_source_index == 0 ? NULL : &player->inner.damage_sources[bullet->damage_source_index - 1];
    source->unk_80 = 1;
    source->unk_14 = 0.0f;
    bullet->flags &= ~1;
    return 0;
}

// FUNCTION: TH16 0x446e00
i32 __fastcall sht_on_tick_446e00(PlayerBullet *bullet)
{
    if (bullet->state == 1)
    {
        bullet->pos.speed += 1.0f;
    }
    return 0;
}

// FUNCTION: TH16 0x4470e0
i32 __fastcall sht_on_init_4470e0(PlayerBullet *bullet)
{
    bullet->flags &= ~0x3c;
    bullet->unk_90 = 0;
    return 0;
}

// FUNCTION: TH16 0x447450
i32 __fastcall sht_on_init_447450(PlayerBullet *bullet)
{
    bullet->damage_source()->unk_90 = 2;
    return 0;
}

// FUNCTION: TH16 0x447480
i32 __fastcall sht_on_tick_447480(PlayerBullet *bullet)
{
    if (bullet->state == 1)
    {
        bullet->pos.speed += 0.5f;
    }
    return 0;
}

// TODO: register allocation: the original loads the player into ecx and
// the scaled index into edx, reading the old value straight into eax.
// FUNCTION: TH16 0x4474a0
i32 __fastcall damage_source_on_hit_4474a0(PlayerDamageSource *source, i32 unk, i32 enemy, f32 x, f32 y)
{
    i32 index = source->bullet_index;
    Player *player = g_Player;
    i32 was_hit = player->inner.bullets[index].unk_9c;
    player->inner.bullets[index].unk_9c = 1;
    return was_hit;
}

// FUNCTION: TH16 0x4474d0
i32 __fastcall sht_on_init_4474d0(PlayerBullet *bullet)
{
    bullet->pos.angle.value = wrap_angle(bullet->pos.angle.value +
                                         g_replay_safe_rng.randf_neg_1_to_1() * (ZUN_PI / 180.0f) * 15.0f);
    return 0;
}

// TODO: the original computes the shooter twice from scratch (keeping ref in
// ebx); ours shares the common parts and spills them.
// FUNCTION: TH16 0x445d40
i32 __fastcall damage_source_on_hit_445d40(PlayerDamageSource *source, i32 unk, i32 enemy, f32 x, f32 y)
{
    Player *player = g_Player;
    i32 ref = player->inner.bullets[source->bullet_index].shooter_ref;
    if (player->get_shooter(ref)->func_on_hit != NULL)
    {
        return player->get_shooter(ref)->func_on_hit(&player->inner.bullets[source->bullet_index], unk, enemy, x,
                                                      y);
    }
    return player->inner.bullets[source->bullet_index].hit();
}

// FUNCTION: TH16 0x445e20
i32 PlayerBullet::hit()
{
    AnmVm *vm = get_vm_or_clear(anm_id);
    pos.pos.z = 0.1f;
    vm->interrupt(1);
    pos.speed *= 0.125f;
    state = 2;
    vm->entity_pos = pos.pos;
    damage_source()->flags &= ~1;
    i32 result = unk_9c;
    damage_source_index = 0;
    return result;
}

// TODO: the original pushes interrupt_tree's 1 between the stores to the new
// damage source; ours pushes it first.
// FUNCTION: TH16 0x446e20
i32 __fastcall sht_on_hit_446e20(PlayerBullet *bullet, i32 unk, i32 enemy, f32 x, f32 y)
{
    i32 damage = g_Player->get_shooter(bullet->shooter_ref)->damage;
    PlayerDamageSource *source =
        g_Player->get_damage_source(g_Player->create_damage_source(&bullet->pos.pos, 24.0f, 1.0f, 0x14, damage));
    source->unk_80 = 4;
    source->pos.speed = 0.3f;
    source->pos.angle.value = -ZUN_PI / 2;
    AnmManager::interrupt_tree(bullet->anm_id, 1);
    bullet->state = 2;
    source = bullet->damage_source();
    source->flags &= ~1;
    bullet->damage_source_index = 0;
    bullet->pos.speed = 0.3f;
    source->pos = bullet->pos;
    g_SoundManager.play_sound_at_position(0x41, bullet->pos.pos.x);
    return bullet->unk_9c;
}

// FUNCTION: TH16 0x447270
i32 __fastcall sht_on_hit_447270(PlayerBullet *bullet, i32 unk, i32 enemy, f32 x, f32 y)
{
    f32 angle = wrap_angle(bullet->pos.angle.value + g_replay_unsafe_rng.randf_neg_1_to_1() * (ZUN_PI / 9.0f));
    g_EffectManager->effect_anm->create_vm(0x98, &bullet->pos.pos, angle, -1, 0);
    return bullet->hit();
}
