// The four characters' bombs: begin starts the ANM scripts (from the
// character's pl0X.anm), the sound and the screen shake, on_tick runs every
// frame until it returns nonzero, cancel_bullets cancels bullets (as
// bomb cancels, which drop season items). The player stays invincible
// while a bomb runs (and for 120 frames from the start of all but
// Reimu's), and a bomb after a spell card's first second costs its bonus.
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "Bomb.h"

#include "BulletManager.h"
#include "EffectManager.h"
#include "EnemyManager.h"
#include "Laser.h"
#include "Player.h"
#include "Rng.h"
#include "ScreenEffect.h"
#include "SoundManager.h"
#include "Spellcard.h"
#include "ZunMath.h"

// Scripts of the character's pl0X.anm that the bombs start: the main VM
// (anm_id) and the secondary one, Reimu's orbs, and the beam pieces under
// Marisa's main VM.
enum
{
    REIMU_BOMB_ORB_SCRIPT = 15,
    REIMU_BOMB_SECONDARY_SCRIPT = 23,
    CIRNO_BOMB_SCRIPT = 10,
    CIRNO_BOMB_SECONDARY_SCRIPT = 13,
    AYA_BOMB_SCRIPT = 14,
    AYA_BOMB_SECONDARY_SCRIPT = 19,
    MARISA_BOMB_SCRIPT = 17,
    MARISA_BOMB_SECONDARY_SCRIPT = 25,
    MARISA_BOMB_BEAM_SCRIPT = 24,
};

// The copy of ZunMath.h's sincosmul in Cirno's bomb's object file (TH16
// keeps one per object file). A static of its own so that it can be
// annotated.
// FUNCTION: TH16 0x40f570
static void __fastcall cirno_sincosmul(Float3 *dst, f32 angle, f32 radius)
{
    __asm {
        mov eax, dst
        fld angle
        fsincos
        fmul radius
        fstp [eax]
        fmul radius
        fstp [eax+4]
    }
}

// The same for Marisa's bomb.
// FUNCTION: TH16 0x410130
static void __fastcall marisa_sincosmul(Float3 *dst, f32 angle, f32 radius)
{
    __asm {
        mov eax, dst
        fld angle
        fsincos
        fmul radius
        fstp [eax]
        fmul radius
        fstp [eax+4]
    }
}

// A bomb ends a spell card's bonus once the card has run a second.
static inline void spellcard_on_bomb()
{
    Spellcard *spellcard = g_Spellcard;
    if (spellcard->flags & SPELLCARD_ACTIVE)
    {
        if (spellcard->time.current >= 60)
        {
            spellcard->bonus = 0;
            spellcard->flags &= ~(SPELLCARD_CAPTURABLE | SPELLCARD_FLAG_20);
        }
        else if (g_MainBomb->in_use == 1)
        {
            spellcard->flags |= SPELLCARD_FLAG_20;
        }
    }
}

// A vertical band of wind through the player's column, tilted and sent
// sideways by the player's horizontal movement.
// TODO: player and &pos trade registers (esi/edi) with the original, which
// also pushes the sound argument later (get_vm instead of a direct
// get_vm_with_id call removed the /GS cookie).
// FUNCTION: TH16 0x40e780
i32 BombAyaAInf::begin()
{
    Player *player = g_Player;
    pos = player->inner.pos;
    D3DXVECTOR3 pos_2;
    pos_2 = player->inner.pos;
    pos.y = 224.0f;
    angle = player->inner.attempted_delta_pos_subpixel.x * (1.0f / 128.0f) * 0.017453292f * 0.5f - ZUN_PI / 2;
    speed = player->inner.attempted_delta_pos_subpixel.x * (1.0f / 128.0f) * 0.05f;
    g_SoundManager.play_sound_centered(30, 0);

    anm_id = player->anm_file->create_vm(AYA_BOMB_SCRIPT, &pos, 0.0f, -1, 0);
    AnmVm *vm = get_vm(anm_id);
    if (vm != NULL)
    {
        vm->rotation.z = angle;
        vm->flags_lo |= ANM_VM_ROTATION_CHANGED;
    }
    AnmLoaded *anm = g_Player->anm_file;
    anm_id_secondary = anm->create_vm(AYA_BOMB_SECONDARY_SCRIPT, &pos_2, 0.0f, -1, 0);
    spellcard_on_bomb();
    g_Player->inner.iframes = 120;
    g_EnemyManager->inner.bomb_count++;
    ScreenEffect::create_inline(SCREEN_EFFECT_SHAKE_WITH_RAMP, 0, 8, 300, 30, 0);
    return 0;
}

// The wind slides sideways across the screen, damaging and cancelling in a
// rectangle, with sparkles all over.
// FUNCTION: TH16 0x40e9d0
i32 BombAyaAInf::on_tick()
{
    AnmVm *vm = get_vm_or_clear(anm_id);
    g_Player->inner.iframes = 40;
    if (vm == NULL)
    {
        AnmManager::interrupt_tree(anm_id_secondary, 1);
        anm_id.id = 0;
        return -1;
    }
    if (timer.current == 300)
    {
        AnmManager::interrupt_tree(anm_id, 1);
    }
    vm->entity_pos.x += speed;
    pos.x += speed;
    g_Player->create_rect_damage_source(&pos, 640.0f, 118.0f, angle, 1, 18);

    D3DXVECTOR3 effect_pos;
    effect_pos.x = g_replay_safe_rng.randf_neg_1_to_1() * 192.0f;
    effect_pos.y = g_replay_safe_rng.randf_0_to_1() * 448.0f;
    effect_pos.z = 0.0f;
    EffectManager *effects = g_EffectManager;
    AnmVm *effect = g_EffectManager->get_tracked_vm(effects->create_tracked_inline(3, &effect_pos));
    // Blend mode 1.
    effect->flags_lo &= ~0x1c0;
    effect->flags_lo |= 0x20;
    cancel_bullets();
    return 0;
}

// FUNCTION: TH16 0x40f070
i32 BombCirnoAInf::begin()
{
    Player *player = g_Player;
    pos = player->inner.pos;
    angle = -ZUN_PI / 2;
    g_SoundManager.play_sound_centered(30, 0);

    anm_id = player->anm_file->create_vm(CIRNO_BOMB_SCRIPT, &pos, 0.0f, -1, 0);
    AnmLoaded *anm = g_Player->anm_file;
    anm_id_secondary = anm->create_vm(CIRNO_BOMB_SECONDARY_SCRIPT, &pos, 0.0f, -1, 0);
    spellcard_on_bomb();
    g_Player->inner.iframes = 120;
    g_EnemyManager->inner.bomb_count++;
    ScreenEffect::create_inline(SCREEN_EFFECT_SHAKE_WITH_RAMP, 0, 8, 300, 30, 0);
    return 0;
}

// Two damage sources grow with the circle (radius 16 to 176 in the first
// second, then to 208), with sparkles inside it for most of the bomb, then
// all over the screen.
// FUNCTION: TH16 0x40f240
i32 BombCirnoAInf::on_tick()
{
    AnmVm *vm = get_vm_or_clear(anm_id);
    g_Player->inner.iframes = 40;
    if (vm == NULL)
    {
        AnmManager::interrupt_tree(anm_id_secondary, 1);
        anm_id.id = 0;
        return -1;
    }
    if (timer.current == 0)
    {
        g_Player->create_damage_source(&pos, 16.0f, 160.0f / 60.0f, 60, 15);
    }
    else if (timer.current == 60)
    {
        g_Player->create_damage_source(&pos, 176.0f, 32.0f / 290.0f, 290, 15);
    }
    if (timer.current < 250 && timer.current >= 30)
    {
        f32 scale = vm->scale.x;
        f32 angle = g_replay_safe_rng.randf_neg_1_to_1() * ZUN_PI;
        D3DXVECTOR3 effect_pos;
        cirno_sincosmul(&effect_pos, angle, g_replay_safe_rng.randf_0_to_1() * scale);
        effect_pos.z = 0.0f;
        Float3 *center = &pos;
        effect_pos += *center;
        AnmVm *effect = g_EffectManager->get_tracked_vm(g_EffectManager->create_tracked(3, &effect_pos, 0));
        // Blend mode 1.
        effect->flags_lo &= ~0x1c0;
        effect->flags_lo |= 0x20;
    }
    else if (timer.current >= 250)
    {
        D3DXVECTOR3 effect_pos;
        effect_pos.x = g_replay_safe_rng.randf_neg_1_to_1() * 192.0f;
        effect_pos.y = g_replay_safe_rng.randf_0_to_1() * 448.0f;
        effect_pos.z = 0.0f;
        AnmVm *effect = g_EffectManager->get_tracked_vm(g_EffectManager->create_tracked(3, &effect_pos, 0));
        effect->flags_lo &= ~0x1c0;
        effect->flags_lo |= 0x20;
    }
    cancel_bullets();
    return 0;
}

// FUNCTION: TH16 0x40f930
i32 BombMarisaAInf::begin()
{
    Player *player = g_Player;
    pos = player->inner.pos;
    angle = -ZUN_PI / 2;
    g_SoundManager.play_sound_centered(49, 0);

    anm_id = player->anm_file->create_vm(MARISA_BOMB_SCRIPT, &pos, 0.0f, -1, 0);
    spellcard_on_bomb();
    g_Player->inner.iframes = 120;
    g_EnemyManager->inner.bomb_count++;
    ScreenEffect::create_inline(SCREEN_EFFECT_SHAKE_WITH_RAMP, 3, 60, 240, 30, 0);
    AnmLoaded *anm = g_Player->anm_file;
    anm_id_secondary = anm->create_vm(MARISA_BOMB_SECONDARY_SCRIPT, &pos, 0.0f, -1, 0);
    g_Player->inner.flags |= PLAYER_FLAG_NO_SHOOTING;
    return 0;
}

// Marisa's master spark: follows the player (slowed to a fifth) and turns
// with their horizontal movement; every third frame three rectangles of
// damage along the beam. After 300 frames the beam fades and the player
// can move and shoot again.
// TODO: ours gets a /GS cookie for beam_pos (it goes away without the
// interrupt_tree calls, also when those go through an inline helper), and
// sums beam_pos and pos in a different operand order.
// FUNCTION: TH16 0x40fb00
i32 BombMarisaAInf::on_tick()
{
    AnmVm *vm = get_vm_or_clear(anm_id);
    g_Player->inner.iframes = 40;
    if (vm == NULL)
    {
        AnmManager::interrupt_tree(anm_id_secondary, 1);
        return -1;
    }
    if (timer.current > 300)
    {
        return 0;
    }
    if (timer.current == 300)
    {
        AnmManager::interrupt_tree(anm_id, 1);
        AnmManager::interrupt_tree(anm_id_secondary, 1);
        g_Player->inner.flags &= ~PLAYER_FLAG_NO_SHOOTING;
        g_Player->inner.speed_multiplier = 1.0f;
    }
    vm->rotation.z = angle;
    vm->flags_lo |= ANM_VM_ROTATION_CHANGED;
    Player *player = g_Player;
    if (0.0f > player->inner.attempted_delta_pos_subpixel.x)
    {
        angle -= 0.0026179939f;
    }
    else if (player->inner.attempted_delta_pos_subpixel.x > 0.0f)
    {
        angle += 0.0026179939f;
    }
    pos = player->inner.pos;
    player->inner.speed_multiplier = 0.2f;
    if (timer.current != timer.previous && timer.current % 3 == 0)
    {
        D3DXVECTOR3 beam_pos;
        beam_pos.z = 0.0f;
        marisa_sincosmul(&beam_pos, angle, 208.0f);
        beam_pos += pos;
        g_Player->get_damage_source(g_Player->create_rect_damage_source(&beam_pos, 512.0f, 32.0f, angle, 0, 60))
            ->flags |= DAMAGE_SOURCE_BOMB;
        marisa_sincosmul(&beam_pos, angle, 240.0f);
        beam_pos += pos;
        g_Player->get_damage_source(g_Player->create_rect_damage_source(&beam_pos, 512.0f, 128.0f, angle, 0, 20))
            ->flags |= DAMAGE_SOURCE_BOMB;
        marisa_sincosmul(&beam_pos, angle, 304.0f);
        beam_pos += pos;
        g_Player->get_damage_source(g_Player->create_rect_damage_source(&beam_pos, 512.0f, 256.0f, angle, 0, 20))
            ->flags |= DAMAGE_SOURCE_BOMB;
    }
    vm = get_vm(anm_id);
    if (vm != NULL)
    {
        vm->entity_pos = pos;
    }
    vm = get_vm(anm_id_secondary);
    if (vm != NULL)
    {
        vm->entity_pos = pos;
    }
    cancel_bullets();
    return 0;
}

// The orbs start on the bomb's first frame (on_tick).
// FUNCTION: TH16 0x410d10
i32 BombReimuAInf::begin()
{
    Player *player = g_Player;
    pos = player->inner.pos;
    g_SoundManager.play_sound_centered(49, 0);
    spellcard_on_bomb();
    g_EnemyManager->inner.bomb_count++;
    if (reimu_orbs != NULL)
    {
        free(reimu_orbs);
        reimu_orbs = NULL;
    }
    reimu_orbs = (BombReimuAOrbs *)malloc(sizeof(BombReimuAOrbs));
    memset(reimu_orbs, 0, sizeof(BombReimuAOrbs));
    AnmLoaded *anm = g_Player->anm_file;
    anm_id_secondary = anm->create_vm(REIMU_BOMB_SECONDARY_SCRIPT, &pos, 0.0f, -1, 0);
    return 0;
}

// The wind's rectangle.
// FUNCTION: TH16 0x40ec10
i32 BombAyaAInf::cancel_bullets()
{
    D3DXVECTOR3 size;
    size.x = 640.0f;
    size.y = 118.0f;
    g_BulletManager->cancel_rectangle_as_bomb(&pos, &size, angle, 5);
    g_LaserManager->cancel_in_rectangle(&pos, &size, angle, 5, 1);
    return 0;
}

// Every beam VM (MARISA_BOMB_BEAM_SCRIPT) under the bomb's VM cancels
// bullets and lasers in its rectangle.
// TODO: the original realigns its frame (and esp, -8), reads the parent's
// world_pos from its stack slot and keeps the loop unrotated.
// FUNCTION: TH16 0x40fe80
i32 BombMarisaAInf::cancel_bullets()
{
    for (i32 i = 0;; i++)
    {
        if (get_vm_or_clear(anm_id) == NULL)
        {
            return 0;
        }
        AnmVm *vm = get_vm_or_clear(anm_id)->search_children(MARISA_BOMB_BEAM_SCRIPT, i);
        if (vm == NULL)
        {
            return 0;
        }
        D3DXVECTOR3 size;
        size.x = vm->scale.x * 48.0f;
        size.y = vm->scale.y * 160.0f;
        D3DXVECTOR3 p = vm->world_pos_inline();
        g_BulletManager->cancel_rectangle_as_bomb(&p, &size, angle, 5);
        g_LaserManager->cancel_in_rectangle_inline(&p, &size, angle, 5, 1);
    }
}

// The radius grows from 16 to 176 over the first second, then to 208.
// FUNCTION: TH16 0x40f4c0
i32 BombCirnoAInf::cancel_bullets()
{
    f32 radius;
    if (timer.current <= 60)
    {
        radius = timer.current_f * 160.0f / 60.0f + 16.0f;
    }
    else
    {
        radius = (timer.current_f - 60.0f) * 32.0f / 290.0f + 176.0f;
    }
    g_BulletManager->cancel_radius_as_bomb(&pos, radius, 5);
    g_LaserManager->cancel_in_radius_inline(&pos, radius, 5, 1);
    return 0;
}

// The orb's motion: its pos is the first field of a PosVel.
static inline PosVel *orb_motion(BombReimuAOrb *orb)
{
    return &orb->motion;
}

// TODO: this is in esi where the original has edi (both save esi and edi and
// leave the other unused), and the radial_dist update is scheduled into the
// start_pos copy.
// FUNCTION: TH16 0x410550
void BombReimuAOrb::update()
{
    if (timer.current != timer.previous)
    {
        i32 time = timer.current;
        if (time < 90)
        {
            start_pos = g_Player->inner.pos;
            motion.radial_dist += 1.5f;
            motion.angle.value = wrap_angle(motion.angle.value + ZUN_PI / 30);
        }
        else if (time < (index + 9) * 10)
        {
            start_pos = g_Player->inner.pos;
            motion.angle.value = wrap_angle(motion.angle.value + ZUN_PI / 30);
        }
        else if (time == (index + 9) * 10)
        {
            motion.flags &= ~0xf;
            motion.set_angle(atan2(move.y, move.x));
            motion.speed = sqrtf(move.x * move.x + move.y * move.y);
        }
        else
        {
            if (g_EnemyManager != NULL)
            {
                target = g_EnemyManager->find_closest(&pos, 512.0f);
            }
            if (target.id != 0)
            {
                target_enemy = target.get();
                if (!(target_enemy->enemy.flags_low & 0xc000021))
                {
                    f32 goal = atan2(target_enemy->enemy.final_pos.pos.y - pos.y,
                                     target_enemy->enemy.final_pos.pos.x - pos.x);
                    f32 angle = motion.angle.value;
                    f32 delta;
                    if (goal - angle > ZUN_PI)
                    {
                        delta = goal - (angle + ZUN_2PI);
                    }
                    else if (angle - goal > ZUN_PI)
                    {
                        delta = goal - (angle - ZUN_2PI);
                    }
                    else
                    {
                        delta = goal - angle;
                    }
                    f32 speed = motion.speed;
                    f32 abs_delta = fabs(delta);
                    if (abs_delta >= ZUN_PI / 4)
                    {
                        speed = speed - 0.7f < 1.0f ? 1.0f : speed - 0.7f;
                    }
                    else if (ZUN_PI / 12 > abs_delta)
                    {
                        speed = speed + 0.2f > 8.0f ? 8.0f : speed + 0.2f;
                    }
                    motion.set_angle((motion.angle + delta * 0.1f).value);
                    motion.speed = speed;
                }
            }
            else if (pos.x < -160.0f || pos.x > 160.0f || pos.y < 32.0f || pos.y > 416.0f)
            {
                motion.speed *= 0.9f;
            }
        }
    }
    D3DXVECTOR3 old_pos = pos;
    motion.update_secondary_fields();
    motion.step();
    AnmVm *vm = get_vm(anm_id);
    if (vm != NULL)
    {
        vm->entity_pos = pos;
    }
    move = pos - old_pos;
    timer.tick();
}

// Not ZUN's: calling update through this keeps LTCG from realigning
// on_tick's frame for it, which the original does not do (and which would
// pad BombReimuAOrb::finish's frame).
static DECOMP_NOINLINE void orb_update(BombReimuAOrb *orb)
{
    orb->update();
}

// Starts the orbs at frame 0, steps them, and bursts those whose damage
// source has dealt 300 damage. All burst at frame 200; the bomb ends once
// their VMs are gone.
// TODO: same shape, different register allocation and block order (orb
// loop, the damage source lookups); calls update through the orb_update
// stand-in (see there).
// FUNCTION: TH16 0x410de0
i32 BombReimuAInf::on_tick()
{
    Player *player = g_Player;
    f32 angle = 0.0f;
    player->inner.iframes = 40;
    BombReimuAOrbs *orbs = reimu_orbs;
    AnmVm *vm = g_AnmManager->get_vm_with_id(anm_id_secondary);
    if (vm != NULL)
    {
        vm->entity_pos = player->inner.pos;
    }
    if (timer.current >= 120)
    {
        i32 i;
        BombReimuAOrb *orb = orbs->orbs;
        for (i = 0; i < 8; i++, orb++)
        {
            if (get_vm_or_clear(orb->anm_id) != NULL)
            {
                break;
            }
        }
        if (i == 8)
        {
            AnmManager::interrupt_tree(anm_id_secondary, 1);
            if (reimu_orbs != NULL)
            {
                free(reimu_orbs);
                reimu_orbs = NULL;
            }
            return -1;
        }
    }
    if (timer.current == 200)
    {
        orbs->finish_all();
        AnmManager::interrupt_tree(anm_id_secondary, 1);
        ScreenEffect::create_inline(SCREEN_EFFECT_SHAKE, 8, 6, 6, 0, 0);
        return 0;
    }
    if (timer.current != timer.previous && timer.current == 0)
    {
        BombReimuAOrb *orb = orbs->orbs;
        for (i32 i = 0; i < 8; i++, orb++)
        {
            orb->start(i, &g_Player->inner.pos);
            PosVel *motion = orb_motion(orb);
            motion->flags = (motion->flags & ~0xd) | 2;
            motion->radial_dist = 0.0f;
            motion->angle.value = wrap_angle(angle);
            motion->radial_speed = ZUN_PI / 64;
            g_Player->get_damage_source(orb->damage_source)->damage_limit = 300;
            angle = wrap_angle(angle + ZUN_PI / 4);
        }
    }
    BombReimuAOrb *orb = orbs->orbs;
    for (i32 i = 8; i != 0; i--, orb++)
    {
        if (!orb->active)
        {
            continue;
        }
        orb_update(orb);
        if (g_Player->get_damage_source(orb->damage_source)->total_damage_dealt >= 300)
        {
            orb->finish();
            g_SoundManager.play_sound_at_position(0x1b, orb->pos.x);
            ScreenEffect::create_inline(SCREEN_EFFECT_SHAKE, 8, 6, 6, 0, 0);
        }
        else
        {
            g_Player->get_damage_source(orb->damage_source)->pos.pos = orb->pos;
        }
    }
    cancel_bullets();
    return 0;
}

// Each active orb cancels bullets around it every eighth frame, the orbs
// taking turns.
// FUNCTION: TH16 0x411280
i32 BombReimuAInf::cancel_bullets()
{
    BombReimuAOrb *orb = reimu_orbs->orbs;
    for (i32 i = 0; i < 8; i++, orb++)
    {
        if (orb->active && timer.current % 8 == i)
        {
            g_BulletManager->cancel_radius_as_bomb(&orb->pos, 64.0f, 5);
            g_LaserManager->cancel_in_radius_inline(&orb->pos, 64.0f, 5, 1);
        }
    }
    return 0;
}

// FUNCTION: TH16 0x4109d0
void BombReimuAOrb::start(i32 index, D3DXVECTOR3 *pos)
{
    start_pos = *pos;
    AnmLoaded *anm = g_Player->anm_file;
    anm_id = anm->create_vm(REIMU_BOMB_ORB_SCRIPT, &this->pos, 0.0f, -1, 0);
    active = 1;
    timer.reset();
    this->index = index;
    damage_source = g_Player->create_damage_source(&this->pos, 56.0f, 0.0f, 9999, 0xf);
    PlayerDamageSource *source = g_Player->get_damage_source(damage_source);
    source->flags |= DAMAGE_SOURCE_BOMB;
    source->hit_interval = 3;
}

// The orb bursts: cancels bullets and lasers around it and hurts enemies
// there, unless it already has, in which case it just goes away.
// FUNCTION: TH16 0x410ae0
void BombReimuAOrb::finish()
{
    if (!done)
    {
        if (active)
        {
            g_SoundManager.play_sound_at_position(0x1b, pos.x);
            g_BulletManager->cancel_radius_as_bomb(&pos, 128.0f, 1);
            g_LaserManager->cancel_in_radius(&pos, 128.0f, 1, 1);
            g_Player->get_damage_source(g_Player->create_damage_source(&pos, 64.0f, 8.0f, 0xb, 100))->flags |=
                DAMAGE_SOURCE_BOMB;
        }
        AnmManager::interrupt_tree(anm_id, 1);
        active = 0;
        if (damage_source != 0)
        {
            g_Player->inner.damage_sources[damage_source - 1].flags &= ~DAMAGE_SOURCE_ACTIVE;
        }
        damage_source = 0;
        return;
    }
    delete_vm_and_clear(anm_id);
}

// finish for every orb, with the laser cancel and the VM deletion inlined.
// FUNCTION: TH16 0x410bb0
void BombReimuAOrbs::finish_all()
{
    BombReimuAOrb *orb = orbs;
    for (i32 i = 0; i < 8; i++, orb++)
    {
        if (!orb->done)
        {
            if (orb->active)
            {
                D3DXVECTOR3 *pos = &orb->pos;
                g_SoundManager.play_sound_at_position(0x1b, pos->x);
                g_BulletManager->cancel_radius_as_bomb(pos, 128.0f, 1);
                g_LaserManager->cancel_in_radius_inline(pos, 128.0f, 1, 1);
                g_Player->get_damage_source(g_Player->create_damage_source(pos, 64.0f, 8.0f, 0xb, 100))->flags |=
                    DAMAGE_SOURCE_BOMB;
            }
            AnmManager::interrupt_tree(orb->anm_id, 1);
            orb->active = 0;
            if (orb->damage_source != 0)
            {
                g_Player->inner.damage_sources[orb->damage_source - 1].flags &= ~DAMAGE_SOURCE_ACTIVE;
            }
            orb->damage_source = 0;
            continue;
        }
        delete_vm_inline_and_clear(orb->anm_id);
    }
}

// Bursts the orbs and ends the bomb at once.
// FUNCTION: TH16 0x411320
void BombReimuAInf::end_at_stage_clear()
{
    reimu_orbs->finish_all();
    AnmManager::interrupt_tree(anm_id_secondary, 1);
    if (reimu_orbs != NULL)
    {
        free(reimu_orbs);
        reimu_orbs = NULL;
    }
    ScreenEffect::create_inline(SCREEN_EFFECT_SHAKE, 8, 6, 6, 0, 0);
    in_use = 0;
}
