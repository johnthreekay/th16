// The four characters' bombs: begin sets up the ANM scripts and screen
// shake, on_tick runs every frame until it returns nonzero, method_10
// cancels bullets.
#include <stdlib.h>
#include <string.h>

#include "Bomb.h"

#include "BulletManager.h"
#include "EnemyManager.h"
#include "Laser.h"
#include "Player.h"
#include "ScreenEffect.h"
#include "SoundManager.h"
#include "Spellcard.h"
#include "ZunMath.h"

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
    ScreenEffect::create(SCREEN_EFFECT_SHAKE_2, 0, 8, 300, 30, 0);
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
    ScreenEffect::create(SCREEN_EFFECT_SHAKE_2, 0, 8, 300, 30, 0);
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
    ScreenEffect::create(SCREEN_EFFECT_SHAKE_2, 3, 60, 240, 30, 0);
    AnmLoaded *anm = g_Player->anm_file;
    anm_id_64 = anm->create_vm(25, &pos, 0.0f, -1, 0);
    g_Player->inner.flags |= 4;
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
    ScreenEffect::create(SCREEN_EFFECT_SHAKE, 8, 6, 6, 0, 0);
    in_use = 0;
}
