// The four characters' bombs: begin sets up the ANM scripts and screen
// shake, on_tick runs every frame until it returns nonzero, method_10
// cancels bullets.
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

// TODO: player and &pos trade registers (esi/edi) with the original, which
// also pushes the sound argument later.
// FUNCTION: TH16 0x40e780
i32 BombAyaAInf::begin()
{
    Player *player = g_Player;
    pos = player->inner.pos;
    D3DXVECTOR3 pos_2;
    pos_2 = player->inner.pos;
    pos.y = 224.0f;
    angle = player->inner.unk_16050 * (1.0f / 128.0f) * 0.017453292f * 0.5f - ZUN_PI / 2;
    speed = player->inner.unk_16050 * (1.0f / 128.0f) * 0.05f;
    g_SoundManager.play_sound_centered(30, 0);

    anm_id = player->anm_file->create_vm(14, &pos, 0.0f, -1, 0);
    AnmVm *vm = g_AnmManager->get_vm_with_id(anm_id);
    if (vm != NULL)
    {
        vm->rotation.z = angle;
        vm->flags_lo |= ANM_VM_ROTATION_CHANGED;
    }
    AnmLoaded *anm = g_Player->anm_file;
    anm_id_64 = anm->create_vm(19, &pos_2, 0.0f, -1, 0);
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
        AnmManager::interrupt_tree(anm_id_64, 1);
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
    effect->flags_lo &= ~0x1c0;
    effect->flags_lo |= 0x20;
    method_10();
    return 0;
}

// FUNCTION: TH16 0x40f070
i32 BombCirnoAInf::begin()
{
    Player *player = g_Player;
    pos = player->inner.pos;
    angle = -ZUN_PI / 2;
    g_SoundManager.play_sound_centered(30, 0);

    anm_id = player->anm_file->create_vm(10, &pos, 0.0f, -1, 0);
    AnmLoaded *anm = g_Player->anm_file;
    anm_id_64 = anm->create_vm(13, &pos, 0.0f, -1, 0);
    spellcard_on_bomb();
    g_Player->inner.iframes = 120;
    g_EnemyManager->inner.bomb_count++;
    ScreenEffect::create_inline(SCREEN_EFFECT_SHAKE_WITH_RAMP, 0, 8, 300, 30, 0);
    return 0;
}

// Sparkles inside the growing circle for most of the bomb, then all over
// the screen.
// TODO: the first sparkle's x adds pos.x to the offset where the original
// adds the offset to pos.x (operand order of one addss).
// FUNCTION: TH16 0x40f240
i32 BombCirnoAInf::on_tick()
{
    AnmVm *vm = get_vm_or_clear(anm_id);
    g_Player->inner.iframes = 40;
    if (vm == NULL)
    {
        AnmManager::interrupt_tree(anm_id_64, 1);
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
        effect_pos += pos;
        AnmVm *effect = g_EffectManager->get_tracked_vm(g_EffectManager->create_tracked(3, &effect_pos, 0));
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
    method_10();
    return 0;
}

// FUNCTION: TH16 0x40f930
i32 BombMarisaAInf::begin()
{
    Player *player = g_Player;
    pos = player->inner.pos;
    angle = -ZUN_PI / 2;
    g_SoundManager.play_sound_centered(49, 0);

    anm_id = player->anm_file->create_vm(17, &pos, 0.0f, -1, 0);
    spellcard_on_bomb();
    g_Player->inner.iframes = 120;
    g_EnemyManager->inner.bomb_count++;
    ScreenEffect::create_inline(SCREEN_EFFECT_SHAKE_WITH_RAMP, 3, 60, 240, 30, 0);
    AnmLoaded *anm = g_Player->anm_file;
    anm_id_64 = anm->create_vm(25, &pos, 0.0f, -1, 0);
    g_Player->inner.flags |= 4;
    return 0;
}

// Marisa's master spark: turns with the player's movement and keeps three
// stretches of damage along the beam.
// TODO: ours gets a /GS cookie for beam_pos and swaps edi/ebx (the ANM
// manager and the VM).
// FUNCTION: TH16 0x40fb00
i32 BombMarisaAInf::on_tick()
{
    AnmManager *anm = g_AnmManager;
    AnmVm *vm = anm->get_vm_with_id(anm_id);
    if (vm == NULL)
    {
        anm_id.id = 0;
    }
    g_Player->inner.iframes = 40;
    if (vm == NULL)
    {
        AnmManager::interrupt_tree(anm_id_64, 1);
        return -1;
    }
    if (timer.current > 300)
    {
        return 0;
    }
    if (timer.current == 300)
    {
        AnmManager::interrupt_tree(anm_id, 1);
        AnmManager::interrupt_tree(anm_id_64, 1);
        g_Player->inner.flags &= ~4;
        g_Player->inner.speed_multiplier = 1.0f;
    }
    vm->flags_lo |= 4;
    vm->rotation.z = angle;
    Player *player = g_Player;
    if (0.0f > player->inner.unk_16050)
    {
        angle -= 0.0026179939f;
    }
    else if (player->inner.unk_16050 > 0.0f)
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
        beam_pos.x = pos.x + beam_pos.x;
        beam_pos.y = pos.y + beam_pos.y;
        beam_pos.z = pos.z + beam_pos.z;
        g_Player->get_damage_source(g_Player->create_rect_damage_source(&beam_pos, 512.0f, 32.0f, angle, 0, 60))->flags |= 4;
        marisa_sincosmul(&beam_pos, angle, 240.0f);
        beam_pos.x = pos.x + beam_pos.x;
        beam_pos.y = pos.y + beam_pos.y;
        beam_pos.z = pos.z + beam_pos.z;
        g_Player->get_damage_source(g_Player->create_rect_damage_source(&beam_pos, 512.0f, 128.0f, angle, 0, 20))->flags |= 4;
        marisa_sincosmul(&beam_pos, angle, 304.0f);
        beam_pos.x = pos.x + beam_pos.x;
        beam_pos.y = pos.y + beam_pos.y;
        beam_pos.z = pos.z + beam_pos.z;
        g_Player->get_damage_source(g_Player->create_rect_damage_source(&beam_pos, 512.0f, 256.0f, angle, 0, 20))->flags |= 4;
    }
    vm = anm->get_vm_with_id(anm_id);
    if (vm != NULL)
    {
        vm->entity_pos = pos;
    }
    vm = anm->get_vm_with_id(anm_id_64);
    if (vm != NULL)
    {
        vm->entity_pos = pos;
    }
    method_10();
    return 0;
}

// FUNCTION: TH16 0x410d10
i32 BombReimuAInf::begin()
{
    Player *player = g_Player;
    pos = player->inner.pos;
    g_SoundManager.play_sound_centered(49, 0);
    spellcard_on_bomb();
    g_EnemyManager->inner.bomb_count++;
    if (unk_70 != NULL)
    {
        free(unk_70);
        unk_70 = NULL;
    }
    unk_70 = malloc(0x6c0);
    memset(unk_70, 0, 0x6c0);
    AnmLoaded *anm = g_Player->anm_file;
    anm_id_64 = anm->create_vm(23, &pos, 0.0f, -1, 0);
    return 0;
}

// FUNCTION: TH16 0x40ec10
i32 BombAyaAInf::method_10()
{
    D3DXVECTOR3 size;
    size.x = 640.0f;
    size.y = 118.0f;
    g_BulletManager->cancel_rectangle_as_bomb(&pos, &size, angle, 5);
    g_LaserManager->cancel_in_rectangle(&pos, &size, angle, 5, 1);
    return 0;
}

// Every beam VM (script 24) under the bomb's VM cancels bullets and lasers
// in its rectangle.
// TODO: the original realigns its frame (and esp, -8), reads the parent's world_pos from its stack slot and keeps the loop unrotated.
// FUNCTION: TH16 0x40fe80
i32 BombMarisaAInf::method_10()
{
    for (i32 i = 0;; i++)
    {
        AnmManager *anm = g_AnmManager;
        if (anm->get_vm_with_id(anm_id) == NULL)
        {
            anm_id.id = 0;
            return 0;
        }
        AnmVm *parent = anm->get_vm_with_id(anm_id);
        if (parent == NULL)
        {
            anm_id.id = 0;
        }
        AnmVm *vm = parent->search_children(0x18, i);
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
i32 BombCirnoAInf::method_10()
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

// Each active orb cancels bullets around it every eighth frame, the orbs
// taking turns.
// FUNCTION: TH16 0x411280
i32 BombReimuAInf::method_10()
{
    BombReimuAOrb *orb = ((BombReimuAOrbs *)unk_70)->orbs;
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

// TODO: register choice around the create_vm call (the original keeps the
// return slot in ecx and g_Player in eax).
// FUNCTION: TH16 0x4109d0
void BombReimuAOrb::start(i32 index, D3DXVECTOR3 *pos)
{
    start_pos = *pos;
    anm_id = g_Player->anm_file->create_vm(0xf, &this->pos, 0.0f, -1, 0);
    active = 1;
    timer.reset();
    this->index = index;
    damage_source = g_Player->create_damage_source(&this->pos, 56.0f, 0.0f, 9999, 0xf);
    PlayerDamageSource *source = g_Player->get_damage_source(damage_source);
    source->flags |= 4;
    source->unk_80 = 3;
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
            g_Player->get_damage_source(g_Player->create_damage_source(&pos, 64.0f, 8.0f, 0xb, 100))->flags |= 4;
        }
        AnmManager::interrupt_tree(anm_id, 1);
        active = 0;
        if (damage_source != 0)
        {
            g_Player->inner.damage_sources[damage_source - 1].flags &= ~1;
        }
        damage_source = 0;
        return;
    }
    delete_vm_and_clear(anm_id);
}

// finish for every orb, with the laser cancel and the VM deletion inlined.
// TODO: the original loads the VM's child list after storing flags_hi (as
// in ~EnemyInf).
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
                g_Player->get_damage_source(g_Player->create_damage_source(pos, 64.0f, 8.0f, 0xb, 100))->flags |= 4;
            }
            AnmManager::interrupt_tree(orb->anm_id, 1);
            orb->active = 0;
            if (orb->damage_source != 0)
            {
                g_Player->inner.damage_sources[orb->damage_source - 1].flags &= ~1;
            }
            orb->damage_source = 0;
            continue;
        }
        delete_vm_inline_and_clear(orb->anm_id);
    }
}

// FUNCTION: TH16 0x411320
void BombReimuAInf::method_14()
{
    ((BombReimuAOrbs *)unk_70)->finish_all();
    AnmManager::interrupt_tree(anm_id_64, 1);
    if (unk_70 != NULL)
    {
        free(unk_70);
        unk_70 = NULL;
    }
    ScreenEffect::create_inline(SCREEN_EFFECT_SHAKE, 8, 6, 6, 0, 0);
    in_use = 0;
}
