#include <math.h>

#include "Player.h"

#include "Bomb.h"
#include "EffectManager.h"
#include "Gui.h"
#include "AnmManager.h"
#include "SoundManager.h"
#include "Spellcard.h"

// FUNCTION: TH16 0x440d50
void Player::set_shoot_key_short_timer(i32 time)
{
    inner.shoot_key_short_timer = time;
}

// FUNCTION: TH16 0x440dc0
void Player::interrupt_options()
{
    inner.flags &= ~2;
    for (i32 i = 0; i < 4; i++)
    {
        AnmManager::interrupt_tree(inner.main_options[i].anm_id_b0, 2);
        AnmManager::interrupt_tree(inner.main_options[i].anm_id_b4, 2);
    }
    for (i32 i = 0; i < 8; i++)
    {
        AnmManager::interrupt_tree(inner.subseason_options[i].anm_id_b0, 2);
        AnmManager::interrupt_tree(inner.subseason_options[i].anm_id_b4, 2);
    }
    inner.unk_16074 = 0;
}

// FUNCTION: TH16 0x440e40
HARNESS_CALLED void Player::set_position(f32 x, f32 y)
{
    inner.pos_subpixel.x = (i32)(x * 128.0f);
    inner.pos_subpixel.y = (i32)(y * 128.0f);
    inner.pos.x = inner.pos_subpixel.x / 128.0f;
    inner.pos.y = inner.pos_subpixel.y / 128.0f;
    inner.main_options[0].should_instajump = 1;
    inner.main_options[1].should_instajump = 1;
    inner.main_options[2].should_instajump = 1;
    inner.main_options[3].should_instajump = 1;
}

// FUNCTION: TH16 0x443840
HARNESS_CALLED f32 Player::angle_to_player(Float3 *pos)
{
    f32 dy = g_Player->inner.pos.y - pos->y;
    f32 dx = g_Player->inner.pos.x - pos->x;
    if (dy == 0.0f && dx == 0.0f)
    {
        return ZUN_PI / 2;
    }
    return atan2f(dy, dx);
}

// TODO: register allocation: the original keeps size in ecx and the player
// in edx (moving it to ecx for die), and lo.y/hi.y in xmm5/xmm2.
// FUNCTION: TH16 0x4438c0
HARNESS_CALLED i32 Player::check_hit_rect(Float3 *pos, Float3 *size, i32 graze_only)
{
    D3DXVECTOR3 half = *size * 0.5f;
    D3DXVECTOR3 hi = *pos + half;
    D3DXVECTOR3 lo = *pos - half;
    if (hurtbox.min_pos.x > hi.x || hurtbox.min_pos.y > hi.y || lo.x > hurtbox.max_pos.x ||
        lo.y > hurtbox.max_pos.y)
    {
        half = D3DXVECTOR3(24.0f, 24.0f, 24.0f);
        hi = *pos + half;
        lo = *pos - half;
        if (hurtbox.min_pos.x > hi.x || hurtbox.min_pos.y > hi.y || lo.x > hurtbox.max_pos.x ||
            lo.y > hurtbox.max_pos.y)
        {
            return 0;
        }
        return 2;
    }
    if (g_Gui != NULL && g_Gui->msg != NULL)
    {
        return 0;
    }
    if (graze_only)
    {
        return 2;
    }
    if (inner.state == 2 || inner.state == 4 || inner.state == 3)
    {
        return 0;
    }
    if (inner.iframes.current <= 0)
    {
        die();
    }
    return 1;
}

// TODO: the original has an 8-byte frame, subtracts y before x, and puts
// the return 0 for an open dialogue right after its test.
// FUNCTION: TH16 0x4439e0
HARNESS_CALLED i32 Player::check_hit_circle(Float3 *pos, f32 radius, i32 graze_only)
{
    D3DXVECTOR3 d = inner.pos - *pos;
    f32 hitbox = sht_file->hitbox_radius;
    f32 dist_sq = d.x * d.x + d.y * d.y;
    if (inner.flags & 0x10)
    {
        hitbox *= player_scale * 3.6f;
    }
    if (dist_sq >= hitbox * hitbox + radius * radius)
    {
        f32 graze = radius / 2.5f;
        graze = 40.0f > graze ? 40.0f : graze;
        if (dist_sq >= (graze + hitbox) * (graze + hitbox) + radius * radius)
        {
            return 0;
        }
        return 2;
    }
    if (g_Gui != NULL && g_Gui->msg != NULL)
    {
        return 0;
    }
    if (graze_only)
    {
        return 2;
    }
    if (inner.state == 2 || inner.state == 4 || inner.state == 3)
    {
        return 0;
    }
    if (inner.iframes.current <= 0)
    {
        die();
    }
    return 1;
}

// TODO: the original reserves 8 bytes of locals where ours has 4.
// FUNCTION: TH16 0x443f10
void Player::die()
{
    if (!(inner.flags & 8))
    {
        g_SoundManager.play_sound_centered(2, 0);
    }
    g_EffectManager->effect_anm->create_vm(0x1d, &inner.pos, 0.0f, -1, 0);
    if (g_Spellcard->flags & 1)
    {
        if (g_Spellcard->time.current >= 60)
        {
            g_Spellcard->bonus = 0;
            g_Spellcard->flags &= ~0x22;
        }
        else if (g_MainBomb->in_use == 1)
        {
            g_Spellcard->flags |= 0x20;
        }
    }
    inner.time_in_state.reset();
    inner.state = 4;
    inner.iframes = 6;
    anm_file->copy_vm_and_run(&vm, 0);
}

// TODO: the original keeps a stack slot (push ecx) around the call instead
// of tail-jumping to on_tick_body.
// FUNCTION: TH16 0x443720
i32 __fastcall Player::on_tick_callback(Player *player)
{
    return player->on_tick_body();
}

// TODO: the original pushes the player (push ecx/pop ecx) as an unused
// stack slot, like on_tick_callback.
// FUNCTION: TH16 0x443730
i32 __fastcall Player::on_draw_callback(Player *player)
{
    if (player->inner.state != 2)
    {
        player->vm.entity_pos = player->inner.pos;
        player->vm.flags_hi = (player->vm.flags_hi & ~ANM_VM_LAYER_UI) | ANM_VM_LAYER_SET;
        g_AnmManager->draw_vm(&player->vm);
    }
    return 1;
}

// FUNCTION: TH16 0x444070
void Player::start_respawn()
{
    inner.time_in_state = 60;
    inner.state = 1;
}
