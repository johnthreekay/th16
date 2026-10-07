#include <stdlib.h>
#include <stddef.h>
#include <math.h>
#include <string.h>

#include "Player.h"

#include "Bomb.h"
#include "FileSystem.h"
#include "EffectManager.h"
#include "Gui.h"
#include "AnmManager.h"
#include "SoundManager.h"
#include "Spellcard.h"
#include "Globals.h"
#include "Item.h"
#include "PopupManager.h"
#include "Input.h"
#include "UpdateFunc.h"

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

// The counterpart of interrupt_options, used when a game starts. Works on
// g_Player.
// FUNCTION: TH16 0x42ca80
HARNESS_CALLED void Player::resume_options()
{
    inner.flags |= 2;
    for (i32 i = 0; i < 4; i++)
    {
        AnmManager::interrupt_tree(inner.main_options[i].anm_id_b0, 3);
        AnmManager::interrupt_tree(inner.main_options[i].anm_id_b4, 3);
    }
    for (i32 i = 0; i < 8; i++)
    {
        AnmManager::interrupt_tree(inner.subseason_options[i].anm_id_b0, 3);
        AnmManager::interrupt_tree(inner.subseason_options[i].anm_id_b4, 3);
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

// TODO: ours gets a /GS cookie (from pos and the inlined set_entity_pos) and keeps the option pointer in esi, not edi+0x60.
// FUNCTION: TH16 0x442380
HARNESS_CALLED i32 Player::update_options(PlayerOption *options, i32 count)
{
    for (i32 i = 0; i < count; i++)
    {
        PlayerOption *option = &options[i];
        if (!option->active)
        {
            continue;
        }
        if (!(inner.flags & 2))
        {
            i32 focused = inner.is_focused != 0;
            option->scaled_preferred_pos.x = (&option->scaled_preferred_pos_rel_to_player)[focused].x + inner.pos_subpixel.x;
            option->scaled_preferred_pos.y = inner.pos_subpixel.y + (&option->scaled_preferred_pos_rel_to_player)[focused].y;
            if (option->on_update != NULL)
            {
                option->on_update(&inner.main_options[i]);
            }
        }
        else
        {
            option->scaled_preferred_pos.x = inner.pos_subpixel.x;
            option->scaled_preferred_pos.y = inner.pos_subpixel.y;
            if (inner.unk_16074 >= 30)
            {
                option->active = 0;
                AnmManager::interrupt_tree(option->anm_id_b0, 1);
                AnmManager::interrupt_tree(option->anm_id_b4, 1);
                continue;
            }
        }
        if (!option->should_instajump)
        {
            if (inner.percent_moved_by_options < 30)
            {
                goto place;
            }
            i32 dx = (option->scaled_preferred_pos.x - option->scaled_cur_pos.x) * inner.percent_moved_by_options / 100;
            i32 dy = (option->scaled_preferred_pos.y - option->scaled_cur_pos.y) * inner.percent_moved_by_options / 100;
            if (dx != 0 || dy != 0)
            {
                option->scaled_cur_pos.x += dx;
                option->scaled_cur_pos.y += dy;
                goto place;
            }
        }
        else
        {
            option->should_instajump = 0;
        }
        option->scaled_cur_pos = option->scaled_preferred_pos;
    place:
        Float3 pos(option->scaled_cur_pos.x / 128.0f, option->scaled_cur_pos.y / 128.0f, 0.0f);
        option->anm_id_b0.set_entity_pos(&pos);
        option->anm_id_b4.set_entity_pos(&pos);
    }
    return 0;
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

// FUNCTION: TH16 0x443790
i32 Player::read_sht_file(ShtFile **out, const char *path)
{
    *out = (ShtFile *)file_read_all(path, NULL, 0);
    if (*out == NULL)
    {
        return -1;
    }
    for (i32 i = 0; i < (*out)->sht_off_count; i++)
    {
        (*out)->shooter_arrays[i] = (ShtShooter *)((u8 *)(*out)->shooters + (u32)(*out)->shooter_arrays[i]);
        for (ShtShooter *shooter = (*out)->shooter_arrays[i]; shooter->fire_rate >= 0; shooter++)
        {
            shooter->func_on_init = g_sht_on_init_funcs[(i32)shooter->func_on_init];
            shooter->func_on_tick = g_sht_on_tick_funcs[(i32)shooter->func_on_tick];
            shooter->func_3 = g_sht_func_3_table[(i32)shooter->func_3];
            shooter->func_on_hit = g_sht_on_hit_funcs[(i32)shooter->func_on_hit];
        }
    }
    return 0;
}

// FUNCTION: TH16 0x4449b0
HARNESS_CALLED i32 Player::create_damage_source(D3DXVECTOR3 *pos, f32 radius, f32 unk, i32 time, i32 damage)
{
    i32 index = inner.last_created_damage_source_index;
    for (i32 i = 0; i < 0x100; i++)
    {
        index++;
        if (index >= 0x100)
        {
            index = 0;
        }
        PlayerDamageSource *source = &inner.damage_sources[index];
        if (!(source->flags & 1))
        {
            source->flags = (source->flags & ~4) | 3;
            memset(&source->pos, 0, sizeof(source->pos));
            source->pos.pos = *pos;
            source->radius = radius;
            source->unk_8 = unk;
            source->timer_60 = time;
            source->damage = damage;
            source->total_damage_dealt = 0;
            source->unk_7c = 9999999;
            source->unk_80 = 1;
            source->unk_90 = 0;
            source->unk_84 = 0;
            break;
        }
    }
    inner.last_created_damage_source_index = index;
    return index + 1;
}

// TODO: the original stores both halves of the position before reading x
// back; ours reads x back between the stores.
// FUNCTION: TH16 0x4476d0
HARNESS_CALLED void Player::set_position_subpixel(Int2 *pos)
{
    inner.pos_subpixel.x = pos->x;
    inner.pos_subpixel.y = pos->y;
    inner.pos.x = inner.pos_subpixel.x / 128.0f;
    inner.pos.y = inner.pos_subpixel.y / 128.0f;
    inner.main_options[0].should_instajump = 1;
    inner.main_options[1].should_instajump = 1;
    inner.main_options[2].should_instajump = 1;
    inner.main_options[3].should_instajump = 1;
}

// GLOBAL: TH16 0x4a6f00
ShtFile *g_cached_sht_file;
// GLOBAL: TH16 0x4a6efc
ShtFile *g_cached_sht_file_subseason;

static_assert(offsetof(Player, inner) == 0x610, "Player::inner");
static_assert(offsetof(Player, snapshot_inner) == 0x166a0, "Player::snapshot_inner");
static_assert(offsetof(Player, sht_file) == 0x2c788, "Player::sht_file");
static_assert(sizeof(Player) == 0x2c828, "Player");

// FUNCTION: TH16 0x440ec0
PlayerInner::PlayerInner()
{
}

// FUNCTION: TH16 0x441a50
Player::~Player()
{
    g_UpdateFuncRegistry->unregister_locked(on_tick);
    g_UpdateFuncRegistry->unregister_locked(on_draw);
    g_Player = NULL;
    if (g_Globals.flags_lo_45c & 1)
    {
        g_AnmManager->disable_vms_from_anm_file(anm_file);
        g_AnmManager->disable_vms_from_anm_file(subseason_anm_file);
        g_cached_sht_file = sht_file;
        g_cached_sht_file_subseason = sht_file_subseason;
    }
    else
    {
        g_AnmManager->unload_anm(9);
        if (sht_file != NULL)
        {
            free(sht_file);
            sht_file = NULL;
        }
        g_cached_sht_file = NULL;
        g_AnmManager->unload_anm(0x1e);
        if (sht_file_subseason != NULL)
        {
            free(sht_file_subseason);
            sht_file_subseason = NULL;
        }
        g_cached_sht_file_subseason = NULL;
    }
}

// FUNCTION: TH16 0x441c60
Player *Player::create()
{
    Player *player = new Player;
    if (player->initialize() != 0)
    {
        delete player;
        return NULL;
    }
    return player;
}

// FUNCTION: TH16 0x441740
HARNESS_CALLED void Player::reset()
{
    inner.state = 1;
    inner.shoot_key_short_timer = -1;
    inner.shoot_key_long_timer = -1;
    inner.time_in_state.reset();
    inner.time_in_stage.reset();
    inner.timer_3c.reset();
    inner.flags &= ~9;
    delete_vm_and_clear(inner.anm_id_focused_hitbox);
    inner.anm_id_focused_hitbox.id = 0;
    delete_vm_and_clear(snapshot_inner.anm_id_focused_hitbox);
    snapshot_inner.anm_id_focused_hitbox.id = 0;
    delete_vm_and_clear(inner.anm_id_15fa0);
    inner.anm_id_15fa0.id = 0;
    delete_vm_and_clear(snapshot_inner.anm_id_15fa0);
    snapshot_inner.anm_id_15fa0.id = 0;
    g_Gui->update_lives(g_Globals.lives, g_Globals.life_fragments);
    interrupt_options();
    inner.repopulate_options();
    inner.flags &= ~4;
    inner.speed_multiplier = 1.0f;
    inner.option_lasers[0] = 0;
    inner.option_lasers[1] = 0;
    inner.option_lasers[2] = 0;
    inner.option_lasers[3] = 0;
    unk_2c7d0 = 0;
    unk_2c7d4 = 0;
    unk_2c7d8 = 0;
    inner.last_created_damage_source_index = 0;
    player_scale_i.end_time = 0;
    player_scale = 1.0f;
    damage_multiplier = 1.0f;
}

// TODO: the original clears eax before the pops in both early returns; ours
// does it after popping edi and esi.
// FUNCTION: TH16 0x445360
i32 Player::shoot_one_bullet(i32 shooter_ref, i32 time, PlayerInner *inner)
{
    ShtShooter *shooter = get_shooter(shooter_ref);
    if (shooter->unk_21 == 2)
    {
        i32 option = (i8)shooter->option - 1;
        if (option >= 100)
        {
            option -= 100;
        }
        if (this->inner.option_lasers[((shooter_ref & 0xf0000) != 0) * 8 + option] != 0)
        {
            return 0;
        }
    }
    PlayerBullet *bullet = this->inner.bullets;
    i32 i;
    for (i = 0; i < 0x100; i++, bullet++)
    {
        if (bullet->state == 0)
        {
            break;
        }
    }
    if (i >= 0x100)
    {
        return 0;
    }
    return bullet->create(shooter_ref, time, inner) != 0 ? -1 : 0;
}

// TODO: the original realigns its frame (and esp, -8), most likely for
// PlayerBullet::create, an opaque stub here.
// FUNCTION: TH16 0x445470
i32 Player::do_shooting(i32 short_time, i32 long_time)
{
    i32 index = 0;
    i32 level = g_Globals.power / g_Globals.power_per_level;
    if (inner.is_focused)
    {
        level += sht_file->num_power_levels + 1;
    }
    for (ShtShooter *shooter = sht_file->shooter_arrays[level]; shooter->fire_rate >= 0; shooter++, index++)
    {
        i32 fire;
        if (shooter->fire_rate_long == 0)
        {
            fire = short_time % shooter->fire_rate == shooter->start_delay;
        }
        else
        {
            fire = long_time % shooter->fire_rate_long == shooter->start_delay_long;
        }
        if (fire)
        {
            shoot_one_bullet(level << 8 | index, short_time, &inner);
        }
    }
    index = 0;
    i32 season_level = g_Globals.season_level();
    for (ShtShooter *shooter = sht_file_subseason->shooter_arrays[season_level]; shooter->fire_rate >= 0;
         shooter++, index++)
    {
        i32 fire;
        if (shooter->fire_rate_long == 0)
        {
            fire = short_time % shooter->fire_rate == shooter->start_delay;
        }
        else
        {
            fire = long_time % shooter->fire_rate_long == shooter->start_delay_long;
        }
        if (fire)
        {
            shoot_one_bullet((season_level | 0x100) << 8 | index, short_time, &inner);
        }
    }
    return 0;
}

// TODO: the original saves ecx and edi on entry (most likely an LTCG
// convention asked for by Player::on_tick, a stub here); ours saves edi only
// around the short timer part.
// FUNCTION: TH16 0x4455d0
i32 Player::tick_shooting_state()
{
    if (inner.state == 1)
    {
        if (inner.shoot_key_short_timer.current < 0)
        {
            if (!(g_InputState.input & INPUT_SHOT))
            {
                goto long_timer;
            }
            if (inner.shoot_key_long_timer.current < 0)
            {
                inner.shoot_key_long_timer.set_value(0);
            }
            set_shoot_key_short_timer(0);
        }
        if (inner.shoot_key_short_timer.current != inner.shoot_key_short_timer.previous)
        {
            do_shooting(inner.shoot_key_short_timer.current, inner.shoot_key_long_timer.current);
        }
        if (inner.shoot_key_short_timer.current >= 14)
        {
            if (g_InputState.input & INPUT_SHOT)
            {
                inner.shoot_key_short_timer -= 14;
            }
            else
            {
                inner.shoot_key_short_timer.set_value(-1);
            }
        }
        else
        {
            inner.shoot_key_short_timer++;
        }
    long_timer:
        if (inner.shoot_key_long_timer.current >= 0)
        {
            if (inner.shoot_key_long_timer.current >= 0x77)
            {
                if (g_InputState.input & INPUT_SHOT)
                {
                    inner.shoot_key_long_timer -= 0x77;
                }
                else
                {
                    inner.shoot_key_long_timer.set_value(-1);
                }
            }
            else
            {
                inner.shoot_key_long_timer++;
            }
        }
    }
    else
    {
        unk_2c790 = 0;
        unk_2c794 = 0;
    }
    return 0;
}

// TODO: our spawn_item is an ordinary thiscall (/INCLUDE keeps it so),
// where the original's LTCG dropped this and folded unk_3 and unk_6; the
// graze counters and the midpoint are also scheduled differently.
// FUNCTION: TH16 0x444cf0
HARNESS_CALLED void Player::do_graze(Float3 *pos)
{
    g_Globals.graze = g_Globals.graze + 1 > 99999999 ? 99999999 : g_Globals.graze + 1;
    g_Globals.graze_in_chapter = g_Globals.graze_in_chapter + 1 > 99999999 ? 99999999 : g_Globals.graze_in_chapter + 1;
    Player *player = g_Player;
    Float3 mid;
    mid.x = (player->inner.pos.x + pos->x) * 0.5f;
    mid.y = (pos->y + player->inner.pos.y) * 0.5f;
    mid.z = 0.0f;
    g_EffectManager->effect_anm->create_vm(0x18, &mid, 0.0f, -1, 0);
    g_PopupManager->generate_small_score_popup(&mid, g_Globals.graze_in_chapter, 0xffc0c0ff);
    g_SoundManager.play_sound_at_position(0x2a, pos->x);
    g_ItemManager->spawn_item(0x10, pos, 0, atan2f(pos->y - player->inner.pos.y, pos->x - player->inner.pos.x), 1.9f,
                              0, 0);
}

// TODO: the original realigns its frame (and esp, -8) and orders the
// rotation and the bounds differently (same convention and logic).
// FUNCTION: TH16 0x443af0
HARNESS_CALLED i32 Player::check_hit_rotated_rect(Float3 *pos, f32 angle, f32 width, f32 length, i32 graze_only)
{
    f32 neg_angle = -angle;
    D3DXVECTOR3 d = inner.pos - *pos;
    f32 s = zun_sinf(neg_angle);
    f32 c = zun_cosf(neg_angle);
    D3DXVECTOR3 r(d.x * c - d.y * s, d.y * c + d.x * s, 0.0f);
    D3DXVECTOR3 lo = r - hurtbox_halfsize * 16.0f;
    D3DXVECTOR3 hi = r + hurtbox_halfsize * 16.0f;
    if (lo.x > length || lo.y > width * 0.5f || 0.0f > hi.x || width * -0.5f > hi.y)
    {
        return 0;
    }
    lo = r - hurtbox_halfsize;
    hi = r + hurtbox_halfsize;
    if (lo.x > length || lo.y > width * 0.5f || 0.0f > hi.x || width * -0.5f > hi.y)
    {
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
    if (inner.iframes.current > 0)
    {
        return 0;
    }
    die();
    return 1;
}
