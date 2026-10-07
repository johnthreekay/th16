#include <stdlib.h>
#include <stddef.h>
#include <math.h>
#include <string.h>

#include "Player.h"

#include "Bomb.h"
#include "Collision.h"
#include "GameErrorContext.h"
#include "FileSystem.h"
#include "EffectManager.h"
#include "EnemyManager.h"
#include "Gui.h"
#include "AnmManager.h"
#include "SoundManager.h"
#include "Spellcard.h"
#include "Globals.h"
#include "Item.h"
#include "PopupManager.h"
#include "Input.h"
#include "UpdateFunc.h"
#include "Supervisor.h"
#include "BulletManager.h"
#include "GameThread.h"
#include "Laser.h"
#include "ReplayManager.h"

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
    // atan2f's body: whether LTCG inlines atan2f here depends on how many
    // other callers it has.
    return (f32)atan2((double)dy, (double)dx);
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

// TODO: the original loads inner.pos.y first and sums x*x + y*y (ours
// y*y + x*x, swapped registers), and puts the return 0 for an open dialogue
// right after its test.
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

// FUNCTION: TH16 0x443cd0
void Player::lose_life()
{
    EffectManager *effects = g_EffectManager;
    i32 index = effects->next_index();
    if (index != -1)
    {
        effects->anm_ids[index] = effects->effect_anm->create_vm(0x1c, &inner.pos, 0.0f, -1, 0);
    }
    g_Globals.lives--;
    g_Globals.bombs = 3;
    if (g_Gui != NULL)
    {
        g_Gui->update_bombs(g_Globals.bombs, g_Globals.bomb_fragments);
    }
    if (g_Globals.lives >= 0)
    {
        g_Gui->update_lives(g_Globals.lives, g_Globals.life_fragments);
    }
    g_Gui->update_bombs(g_Globals.bombs, g_Globals.bomb_fragments);
    inner.state = 2;
    inner.time_in_state.reset();
    inner.iframes = 180;
    anm_file->copy_vm_and_run(&vm, 0);
    for (i32 i = 0; i < 4; i++)
    {
        inner.main_options[i].active = 0;
        AnmManager::interrupt_tree(inner.main_options[i].anm_id_b0, 1);
        AnmManager::interrupt_tree(inner.main_options[i].anm_id_b4, 1);
    }
    inner.num_main_options = 0;
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
    g_EnemyManager->inner.miss_count++;
    g_EnemyManager->inner.can_still_capture_spell = 0;
    if (g_Globals.miss_count < 999999)
    {
        g_Globals.miss_count++;
    }
}

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

// The push ecx/pop ecx around the call (rather than a tail jump) pads the
// frame for 8-byte stack alignment: LTCG knows this callback is entered
// aligned, because Player::initialize registers it and Player::create's
// caller, GameThread::thread_start, realigns its frame.
// FUNCTION: TH16 0x443720
i32 __fastcall Player::on_tick_callback(Player *player)
{
    return player->on_tick_body();
}

// TODO: the original pads the draw_vm call with push ecx/pop ecx for 8-byte
// alignment, like on_tick_callback; ours does not, most likely because LTCG
// does not see draw_vm needing alignment early (draw_vm does not match yet).
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
HARNESS_CALLED i32 Player::read_sht_file(ShtFile **out, const char *path)
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

// FUNCTION: TH16 0x444b20
HARNESS_CALLED i32 Player::create_rect_damage_source(D3DXVECTOR3 *pos, f32 width, f32 height, f32 angle, i32 time,
                                                    i32 damage)
{
    Player *player = g_Player;
    i32 index = player->inner.last_created_damage_source_index;
    for (i32 i = 0; i < 0x100; i++)
    {
        index++;
        if (index >= 0x100)
        {
            index = 0;
        }
        PlayerDamageSource *source = &player->inner.damage_sources[index];
        if (!(source->flags & 1))
        {
            source->flags = (source->flags & ~6) | 1;
            memset(&source->pos, 0, sizeof(source->pos));
            source->pos.pos = *pos;
            source->unk_14 = width;
            source->unk_18 = height;
            source->unk_c = wrap_angle(angle);
            source->unk_10 = 0;
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
    player->inner.last_created_damage_source_index = index;
    return index + 1;
}

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

// GLOBAL: TH16 0x4a6ef8
Player *g_Player;

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

// A PlayerInner nothing uses besides its static constructor (ExpHP:
// STATIC_PLAYER_INNER__CAUSE_THAT_MAKES_SENSE).
// GLOBAL: TH16 0x4c1b10
PlayerInner g_static_player_inner;
// SYNTHETIC: TH16 0x401100
// ??__Eg_static_player_inner@@YAXXZ

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
HARNESS_CALLED Player *Player::create()
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

// TODO: the original realigns its frame (ebx form); the body matches. Not
// for PlayerBullet::create (real code now, no realignment of its own).
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

// atan2 is spelled out so that it stays inline (see angle_to_player); the
// double math realigns the frame, which also gives spawn_item known
// alignment.
// FUNCTION: TH16 0x444cf0
HARNESS_CALLED void Player::do_graze(Float3 *pos)
{
    Player *player = g_Player;
    i32 graze_in_chapter = g_Globals.graze_in_chapter + 1;
    i32 graze = g_Globals.graze + 1;
    g_Globals.graze = graze > 99999999 ? 99999999 : graze;
    g_Globals.graze_in_chapter = graze_in_chapter > 99999999 ? 99999999 : graze_in_chapter;
    Float3 mid = (player->inner.pos + *pos) * 0.5f;
    mid.z = 0.0f;
    g_EffectManager->effect_anm->create_vm(0x18, &mid, 0.0f, -1, 0);
    g_PopupManager->generate_small_score_popup(&mid, g_Globals.graze_in_chapter, 0xffc0c0ff);
    g_SoundManager.play_sound_at_position(0x2a, pos->x);
    g_ItemManager->spawn_item(0x10, pos, 0,
                              (f32)atan2((double)(pos->y - player->inner.pos.y), (double)(pos->x - player->inner.pos.x)),
                              1.9f, 0, 0);
}

// TODO: d.x and d.y take swapped stack slots and the scaled hurtbox bounds
// are computed in a different order (frame and convention match).
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

// TODO: the original walks the sources with two pointers (source and &timer_60.current) and rereads pos/size from the stack; ours keeps one pointer and pos/size in registers.
// FUNCTION: TH16 0x445a30
HARNESS_CALLED i32 Player::compute_damage_to_enemy(Float3 *pos, Float3 *size, f32 rotation, f32 radius,
                                                   i32 *hit_flag, Float3 *hit_pos, i32 no_score, i32 enemy_id)
{
    Player *player = g_Player;
    if (player->inner.time_in_state.current == player->inner.time_in_state.previous)
    {
        return 0;
    }
    i32 total = g_MainBomb->in_use == 0 ? 0 : g_MainBomb->method_c((i32)pos, (i32)size);
    if (hit_flag != NULL)
    {
        *hit_flag = total > 0 ? 1 : 0;
    }
    for (i32 i = 0; i < 0x100; i++)
    {
        PlayerDamageSource *source = &player->inner.damage_sources[i];
        if (!(source->flags & 1))
        {
            continue;
        }
        if (source->timer_60.current == source->timer_60.previous || source->timer_60.current % source->unk_80 != 0)
        {
            continue;
        }
        if (!(source->flags & 2))
        {
            if (size != NULL)
            {
                if (!collision_test_rect_rect(source->pos.pos.x, source->pos.pos.y, source->unk_14, source->unk_18,
                                              source->unk_c, pos->x, pos->y, size->x, size->y, rotation))
                {
                    continue;
                }
            }
            else if (!collision_test_circle_rect(source->pos.pos.x, source->pos.pos.y, source->unk_14,
                                                 source->unk_18, source->unk_c, pos->x, pos->y, radius))
            {
                continue;
            }
        }
        else if (size != NULL)
        {
            if (!collision_test_circle_rect(pos->x, pos->y, size->x, size->y, rotation, source->pos.pos.x,
                                            source->pos.pos.y, source->radius))
            {
                continue;
            }
        }
        else
        {
            f32 dx = source->pos.pos.x - pos->x;
            f32 dy = source->pos.pos.y - pos->y;
            f32 r = source->radius + radius;
            if (dx * dx + dy * dy > r * r)
            {
                continue;
            }
        }
        if (enemy_id != 0)
        {
            if (source->unk_84 == enemy_id)
            {
                continue;
            }
            source->unk_84 = enemy_id;
        }
        if (hit_flag != NULL && (source->flags & 4))
        {
            *hit_flag = 1;
        }
        i32 damage = source->damage;
        if (!no_score)
        {
            if (source->unk_90 != 0)
            {
                source->hit_func = enemy_id;
                damage = g_damage_source_hit_funcs[source->unk_90](source, (i32)pos, (i32)size, rotation, radius);
            }
            source->total_damage_dealt += source->damage;
        }
        total += damage;
        if (hit_pos != NULL)
        {
            *hit_pos = source->pos.pos;
        }
        if (source->unk_7c < 9999999 && source->unk_7c <= source->total_damage_dealt)
        {
            source->flags &= ~1;
            source->damage = 0;
        }
    }
    if (total > player->sht_file->max_damage)
    {
        total = player->sht_file->max_damage;
    }
    if (!no_score && total != 0)
    {
        g_Globals.add_to_score(total / 10 + 10);
    }
    return total;
}

// Whether a point lies inside the playfield.
static __forceinline i32 is_on_screen(Float3 *pos)
{
    return g_early_arcade_offset_x < pos->x && pos->x < g_early_arcade_offset_x + 384.0f &&
           g_early_arcade_offset_y < pos->y && pos->y < g_early_arcade_offset_y + 448.0f;
}

// TODO: the original realigns its frame (ebx form) and reloads g_Player for every bullet; ours keeps it in a register.
// FUNCTION: TH16 0x4456d0
i32 Player::tick_bullets()
{
    for (i32 i = 0; i < 0x100; i++)
    {
        PlayerBullet *bullet = &inner.bullets[i];
        if (bullet->state == 0)
        {
            continue;
        }
        ShtShooter *shooter = g_Player->get_shooter(bullet->shooter_ref);
        if (shooter->func_on_tick != NULL && shooter->func_on_tick(bullet) != 0)
        {
            continue;
        }
        bullet->pos.update_secondary_fields();
        bullet->pos.step();
        AnmVm *vm = g_AnmManager->get_vm_with_id(bullet->anm_id);
        if (vm == NULL)
        {
            bullet->state = 0;
            bullet->anm_id.id = 0;
            if (bullet->damage_source_index != 0)
            {
                g_Player->inner.damage_sources[bullet->damage_source_index - 1].flags &= ~1;
            }
            continue;
        }
        if (shooter->unk_21 != 2)
        {
            Float3 corners[4];
            vm->write_sprite_corners(corners);
            if (bullet->timer_c.current >= 15 && !is_on_screen(&corners[0]) && !is_on_screen(&corners[1]) &&
                !is_on_screen(&corners[2]) && !is_on_screen(&corners[3]))
            {
                {
                    delete_vm_and_clear(bullet->anm_id);
                    bullet->state = 0;
                    if (bullet->damage_source_index != 0)
                    {
                        g_Player->inner.damage_sources[bullet->damage_source_index - 1].flags &= ~1;
                    }
                    continue;
                }
            }
        }
        if (bullet->damage_source_index != 0 && (bullet->flags & 1))
        {
            PlayerDamageSource *source = &g_Player->inner.damage_sources[bullet->damage_source_index - 1];
            source->pos.pos = bullet->pos.pos;
            source->unk_c = bullet->pos.angle.value;
            source->unk_14 = bullet->laser_length;
            source->unk_18 = bullet->unk_a4_f;
            source->damage = bullet->unk_9c;
        }
        vm->entity_pos = bullet->pos.pos;
        if (vm->flags_hi & 0x80)
        {
            vm->flags_lo |= ANM_VM_ROTATION_CHANGED;
            vm->rotation.z = bullet->pos.angle.value;
        }
        bullet->timer_c.tick();
    }
    return 0;
}

// GLOBAL: TH16 0x492c68
const f32 g_player_attract_radii[4] = {100.0f, 100.0f, 100.0f, 100.0f};
// GLOBAL: TH16 0x492c78
const f32 g_player_graze_radii[4] = {5.0f, 5.0f, 5.0f, 5.0f};
// GLOBAL: TH16 0x492c88
const f32 g_player_item_radii[4] = {60.0f, 60.0f, 60.0f, 60.0f};
// GLOBAL: TH16 0x492c98
const f32 g_player_hitbox_radii[4] = {3.0f, 3.0f, 3.0f, 3.0f};
// GLOBAL: TH16 0x492ca8
const char *const g_player_sht_names[4] = {"pl00.sht", "pl02.sht", "pl03.sht", "pl01.sht"};
// GLOBAL: TH16 0x492cb8
const char *const g_subseason_sht_names[5] = {"pl00sub.sht", "pl02sub.sht", "pl03sub.sht", "pl01sub.sht",
                                              "pl04sub.sht"};
// GLOBAL: TH16 0x492ccc
const char *const g_player_anm_names[4] = {"pl00.anm", "pl02.anm", "pl03.anm", "pl01.anm"};
// GLOBAL: TH16 0x492cdc
const char *const g_subseason_anm_names[5] = {"pl00sub.anm", "pl02sub.anm", "pl03sub.anm", "pl01sub.anm",
                                              "pl04sub.anm"};

// FUNCTION: TH16 0x440fb0
i32 Player::initialize()
{
    anm_file = AnmManager::preload_anm(9, g_player_anm_names[g_Globals.character + g_Globals.subshot]);
    if (anm_file == NULL)
    {
        g_GameErrorContext.log("\x8e\xa9\x8b@\x83" "f\x81[\x83^\x82\xaa\x8c\xa9\x82\xc2\x82\xa9\x82\xe8\x82\xdc\x82\xb9\x82\xf1\x81"
                               "B\x83" "f\x81[\x83^\x82\xaa\x89\xf3\x82\xea\x82\xc4\x82\xa2\x82\xdc\x82\xb7\r\n");
        return -1;
    }
    subseason_anm_file = AnmManager::preload_anm(0x1e, g_subseason_anm_names[g_Globals.subseason]);
    if (subseason_anm_file == NULL)
    {
        g_GameErrorContext.log("\x8e\xa9\x8b@\x83" "f\x81[\x83^\x82\xaa\x8c\xa9\x82\xc2\x82\xa9\x82\xe8\x82\xdc\x82\xb9\x82\xf1\x81"
                               "B\x83" "f\x81[\x83^\x82\xaa\x89\xf3\x82\xea\x82\xc4\x82\xa2\x82\xdc\x82\xb7\r\n");
        return -1;
    }
    if (g_cached_sht_file != NULL)
    {
        sht_file = g_cached_sht_file;
        sht_file_subseason = g_cached_sht_file_subseason;
        g_cached_sht_file = NULL;
        g_cached_sht_file_subseason = NULL;
    }
    else
    {
        if (read_sht_file(&sht_file, g_player_sht_names[g_Globals.character + g_Globals.subshot]) != 0)
        {
            g_GameErrorContext.log("\x8e\xa9\x8b@\x83" "f\x81[\x83^\x82\xaa\x8c\xa9\x82\xc2\x82\xa9\x82\xe8\x82\xdc\x82\xb9\x82\xf1\x81"
                                   "B\x83" "f\x81[\x83^\x82\xaa\x89\xf3\x82\xea\x82\xc4\x82\xa2\x82\xdc\x82\xb7\r\n");
            return -1;
        }
        if (read_sht_file(&sht_file_subseason, g_subseason_sht_names[g_Globals.subseason]) != 0)
        {
            g_GameErrorContext.log("\x8e\xa9\x8b@\x83" "f\x81[\x83^\x82\xaa\x8c\xa9\x82\xc2\x82\xa9\x82\xe8\x82\xdc\x82\xb9\x82\xf1\x81"
                                   "B\x83" "f\x81[\x83^\x82\xaa\x89\xf3\x82\xea\x82\xc4\x82\xa2\x82\xdc\x82\xb7\r\n");
            return -1;
        }
    }
    {
        UpdateFunc *f = g_UpdateFuncRegistry->create_func((UpdateFuncCallback)on_tick_callback);
        f->flags &= ~UPDATE_FUNC_ACTIVE;
        f->arg = this;
        g_UpdateFuncRegistry->register_on_tick(f, 0x16);
        on_tick = f;
        f = g_UpdateFuncRegistry->create_func((UpdateFuncCallback)on_draw_callback);
        f->flags &= ~UPDATE_FUNC_ACTIVE;
        f->arg = this;
        g_UpdateFuncRegistry->register_on_draw(f, 0x1d);
        on_draw = f;
    }
    {
        AnmVm *player_vm = &vm;
        anm_file->copy_vm(player_vm, 0);
        player_vm->unk_5b0 = NULL;
        player_vm->parent = NULL;
        player_vm->run();
    }
    set_position(0.0f, 400.0f);
    for (i32 i = 0; i < 4; i++)
    {
        inner.speeds_subpixel[i] = (i32)((&sht_file->move_speed)[i] * 128.0f);
    }
    {
        i32 season_deltas[8] = {0, 100, 130, 160, 200, 250, 300, 0};
        sht_file->power_per_level = 100;
        g_Globals.max_power = sht_file->power_per_level * sht_file->num_power_levels;
        g_Globals.power_per_level = sht_file->power_per_level;
        for (i32 i = 0; i < 8; i++)
        {
            g_Globals.init_season_level_delta(i, season_deltas[i]);
        }
    }
    g_Globals.max_season_power = g_Globals.season_level_thresholds[7];
    inner.shoot_key_short_timer = -1;
    inner.shoot_key_long_timer = -1;
    sht_file->hitbox_radius = g_player_hitbox_radii[g_Globals.character];
    sht_file->itembox_radius = g_player_item_radii[g_Globals.character];
    sht_file->grazebox_radius = g_player_graze_radii[g_Globals.character];
    hurtbox_halfsize.x = hurtbox_halfsize.y = sht_file->hitbox_radius * 0.5f;
    hurtbox_halfsize.z = 5.0f;
    item_attract_box_unfocused_halfsize.x = item_attract_box_unfocused_halfsize.y =
        g_player_item_radii[g_Globals.character] * 0.5f;
    item_attract_box_unfocused_halfsize.z = 5.0f;
    item_attract_box_focused_halfsize.x = item_attract_box_focused_halfsize.y =
        g_player_attract_radii[g_Globals.character] * 0.5f;
    item_attract_box_focused_halfsize.z = 5.0f;
    hurtbox.min_pos = inner.pos - hurtbox_halfsize;
    hurtbox.max_pos = inner.pos + hurtbox_halfsize;
    item_collect_box.min_pos = inner.pos - item_attract_box_unfocused_halfsize;
    item_collect_box.max_pos = inner.pos + item_attract_box_unfocused_halfsize;
    item_attract_box_focused.min_pos = inner.pos - item_attract_box_focused_halfsize;
    item_attract_box_focused.max_pos = inner.pos + item_attract_box_focused_halfsize;
    item_attract_box_unfocused.min_pos = inner.pos - item_attract_box_focused_halfsize;
    item_attract_box_unfocused.max_pos = inner.pos + item_attract_box_focused_halfsize;
    inner.time_in_state.reset_inline();
    inner.timer_3c.reset_inline();
    inner.iframes.reset_inline();
    inner.percent_moved_by_options = 30;
    inner.num_main_options = 0;
    inner.speed_multiplier = 1.0f;
    for (i32 i = 0; i < 4; i++)
    {
        inner.main_options[i].scaled_cur_pos.y = -400 * 128;
    }
    for (i32 i = 0; i < 8; i++)
    {
        inner.subseason_options[i].scaled_cur_pos.y = -400 * 128;
    }
    inner.flags &= ~4;
    player_scale_i.end_time = 0;
    player_scale = 1.0f;
    for (i32 i = 0; i < 0x100; i++)
    {
        inner.bullets[i].index_of_self = i;
    }
    return 0;
}

// GLOBAL: TH16 0x492c20
const Int2 g_player_directions[9] = {{0, 0}, {0, -1}, {0, 1}, {-1, 0}, {1, 0}, {-1, -1}, {-1, 1}, {1, -1}, {1, 1}};

// copy_vm_and_run as LTCG inlined it here.
static __forceinline void player_set_script(Player *player, i32 script)
{
    player->anm_file->copy_vm(&player->vm, script);
    player->vm.unk_5b0 = NULL;
    player->vm.parent = NULL;
    player->vm.run();
}

// TODO: the original realigns its frame (and esp, -8) and keeps 1.0f in xmm2; register allocation differs.
// FUNCTION: TH16 0x441cf0
i32 Player::move()
{
    u32 input = g_InputState.input;
    if ((input & (INPUT_UP | INPUT_LEFT)) == (INPUT_UP | INPUT_LEFT))
    {
        attempted_direction = 5;
    }
    else if ((input & (INPUT_DOWN | INPUT_LEFT)) == (INPUT_DOWN | INPUT_LEFT))
    {
        attempted_direction = 7;
    }
    else if ((input & (INPUT_UP | INPUT_RIGHT)) == (INPUT_UP | INPUT_RIGHT))
    {
        attempted_direction = 6;
    }
    else if ((input & (INPUT_DOWN | INPUT_RIGHT)) == (INPUT_DOWN | INPUT_RIGHT))
    {
        attempted_direction = 8;
    }
    else if (input & INPUT_DOWN)
    {
        attempted_direction = 2;
    }
    else if (input & INPUT_UP)
    {
        attempted_direction = 1;
    }
    else if (input & INPUT_LEFT)
    {
        attempted_direction = 3;
    }
    else if (input & INPUT_RIGHT)
    {
        attempted_direction = 4;
    }
    else
    {
        attempted_direction = 0;
    }
    if (g_EnemyManager != NULL && g_EnemyManager->enemy_count_real != 0 && inner.time_in_stage.current >= 4)
    {
        inner.is_focused = (g_InputState.input >> 3) & 1;
    }
    else
    {
        inner.is_focused = 0;
        inner.percent_moved_by_options = 30;
    }
    i32 dx = g_player_directions[attempted_direction].x;
    i32 dy = g_player_directions[attempted_direction].y;
    i32 speed_x;
    i32 speed_y;
    if (inner.is_focused)
    {
        if (inner.anm_id_focused_hitbox.id == 0)
        {
            inner.anm_id_focused_hitbox = g_EffectManager->effect_anm->create_effect(0x1a, 0xe, NULL);
        }
        AnmVm *vm = get_vm_or_clear(inner.anm_id_focused_hitbox);
        if (vm != NULL)
        {
            if (inner.flags & 0x10)
            {
                f32 scale = (player_scale - 1.0f) * 2.0f;
                vm->scale_2.y = scale + 1.0f;
                vm->scale_2.x = scale + 1.0f;
            }
            else
            {
                vm->scale_2.x = 1.0f;
                vm->scale_2.y = 1.0f;
            }
            vm->flags_lo |= ANM_VM_SCALE_CHANGED;
        }
        speed_x = attempted_direction >= 5 ? inner.speeds_subpixel[3] : inner.speeds_subpixel[1];
        speed_y = attempted_direction >= 5 ? inner.speeds_subpixel[3] : inner.speeds_subpixel[1];
    }
    else
    {
        if (g_AnmManager->get_vm_with_id(inner.anm_id_focused_hitbox) != NULL)
        {
            AnmManager::interrupt_tree(inner.anm_id_focused_hitbox, 1);
        }
        inner.anm_id_focused_hitbox.id = 0;
        speed_x = attempted_direction >= 5 ? inner.speeds_subpixel[2] : inner.speeds_subpixel[0];
        speed_y = attempted_direction >= 5 ? inner.speeds_subpixel[2] : inner.speeds_subpixel[0];
    }
    i32 vx = (f32)(speed_x * dx - (i32)(inner.unk_1607c.x * -128.0f)) * inner.speed_multiplier;
    i32 vy = (f32)(speed_y * dy - (i32)(inner.unk_1607c.y * -128.0f)) * inner.speed_multiplier;
    if (vx < 0 && attempted_velocity.x >= 0)
    {
        player_set_script(this, 1);
    }
    if (vx > 0 && attempted_velocity.x <= 0)
    {
        player_set_script(this, 3);
    }
    if (vx == 0 && attempted_velocity.x < 0)
    {
        player_set_script(this, 2);
    }
    if (vx == 0 && attempted_velocity.x > 0)
    {
        player_set_script(this, 4);
    }
    attempted_velocity.x = vx;
    attempted_velocity.y = vy;
    inner.unk_16050 = vx * g_game_speed;
    inner.unk_16054 = vy * g_game_speed;
    if (attempted_direction != 0)
    {
        inner.last_nonzero_delta_pos_subpixel = *(Float3 *)&inner.unk_16050;
    }
    inner.velocity_subpixel.y = (i32)inner.unk_16054;
    inner.velocity_subpixel.x = (i32)inner.unk_16050;
    inner.pos_subpixel.x += inner.velocity_subpixel.x;
    inner.pos_subpixel.y += inner.velocity_subpixel.y;
    if (inner.pos_subpixel.x < -184 * 128)
    {
        inner.pos_subpixel.x = -184 * 128;
    }
    else if (inner.pos_subpixel.x > 184 * 128)
    {
        inner.pos_subpixel.x = 184 * 128;
    }
    if (inner.pos_subpixel.y < 32 * 128)
    {
        inner.pos_subpixel.y = 32 * 128;
    }
    else if (inner.pos_subpixel.y > 432 * 128)
    {
        inner.pos_subpixel.y = 432 * 128;
    }
    inner.pos.x = inner.pos_subpixel.x / 128.0f;
    inner.pos.y = inner.pos_subpixel.y / 128.0f;
    if (get_vm_or_clear(inner.anm_id_focused_hitbox) != NULL)
    {
        AnmVm *vm = g_AnmManager->get_vm_with_id(inner.anm_id_focused_hitbox);
        if (vm != NULL)
        {
            vm->entity_pos = inner.pos;
        }
    }
    if (inner.flags & 2)
    {
        inner.unk_16074++;
    }
    update_options(inner.main_options, 4);
    update_options(inner.subseason_options, 8);
    if (inner.unk_16074 >= 30)
    {
        inner.num_main_options = 0;
        inner.num_season_options = 0;
    }
    if (inner.timer_15fa4.current > 0)
    {
        if (get_vm_or_clear(inner.anm_id_15fa0) == NULL)
        {
            inner.anm_id_15fa0 = g_EffectManager->effect_anm->create_effect(0x1b, 0xe, NULL);
        }
        AnmVm *vm = g_AnmManager->get_vm_with_id(inner.anm_id_15fa0);
        if (vm != NULL)
        {
            vm->entity_pos = inner.pos;
        }
        inner.timer_15fa4.decrement(1.0f);
        if (inner.timer_15fa4.current <= 0)
        {
            AnmManager::interrupt_tree(inner.anm_id_15fa0, 1);
            inner.timer_15fa4.reset_inline();
            inner.anm_id_15fa0.id = 0;
        }
    }
    return 0;
}

// Where each power level's options start in a .sht file's option position
// list (the options of level n are entries [n - 1] onwards).
// GLOBAL: TH16 0x4a5db0
i32 g_season_option_layouts[5][8] = {
    {0, 1, 3, 6, 10, 15, 21, 28}, {0, 1, 3, 6, 10, 15, 21, 28}, {0, 1, 3, 6, 10, 15, 21, 28},
    {0, 1, 3, 6, 10, 15, 21, 28}, {0, 1, 3, 6, 10, 15, 21, 28},
};
// GLOBAL: TH16 0x4a5e50
i32 g_main_option_layouts[4][8] = {
    {0, 1, 3, 6, 10, 11, 13, 16},
    {0, 1, 3, 6, 10, 11, 13, 16},
    {0, 1, 3, 6, 10, 11, 13, 16},
    {0, 1, 3, 6, 10, 11, 13, 16},
};
// GLOBAL: TH16 0x492be0
const i32 g_season_option_scripts[8] = {1, 1, 1, 0x12, 1, 0, 0, 0};
// GLOBAL: TH16 0x492c00
const i32 g_main_option_scripts[4] = {8, 7, 11, 11};
// The look of the main options at full power.
// GLOBAL: TH16 0x492c10
const i32 g_main_option_max_power_scripts[4] = {9, 8, 12, 13};

// AnmManager::interrupt_tree as LTCG inlined it here.
static __forceinline void interrupt_tree_inline(AnmId id, i32 interrupt)
{
    AnmVm *vm = g_AnmManager->get_vm_with_id(id);
    if (vm == NULL)
    {
        return;
    }
    vm->interrupt(interrupt);
    for (ZunList<AnmVm> *node = vm->list_of_children.next; node != NULL; node = node->next)
    {
        node->entry->interrupt(interrupt);
    }
}

// TODO: ours gets a /GS cookie for the zero position passed to create_vm_inline; the original zeroes one local before the loops and aligns its frame.
// FUNCTION: TH16 0x4440e0
void PlayerInner::repopulate_options()
{
    i32 *layout = g_main_option_layouts[g_Globals.character + g_Globals.subshot];
    i32 level = g_Globals.power / g_Globals.power_per_level;
    options_power_level = level;
    if (g_Globals.power >= g_Globals.max_power)
    {
        for (i32 i = 0; i < level; i++)
        {
            PlayerOption *option = &main_options[i];
            g_AnmManager->delete_vm_inline(option->anm_id_b4);
            option->anm_id_b4.id = 0;
            option->anm_id_b4 =
                g_Player->anm_file->create_vm_front(g_main_option_max_power_scripts[g_Globals.character], -1, 0);
            get_vm_or_clear(option->anm_id_b4)->entity_pos.y = -32.0f;
        }
    }
    else
    {
        for (i32 i = 0; i < 4; i++)
        {
            AnmManager::interrupt_tree(main_options[i].anm_id_b4, 1);
            main_options[i].anm_id_b4.id = 0;
        }
    }
    Player *player = g_Player;
    if (player->inner.num_main_options != level)
    {
        i32 i;
        for (i = 0; i < level; i++)
        {
            PlayerOption *option = &main_options[i];
            option->scaled_cur_pos.x = player->inner.pos_subpixel.x;
            option->scaled_cur_pos.y = player->inner.pos_subpixel.y;
            g_AnmManager->delete_vm_inline(option->anm_id_b0);
            option->anm_id_b0.id = 0;
            option->index = i;
            Float2 *positions = (Float2 *)player->sht_file->option_pos;
            option->scaled_preferred_pos_rel_to_player.x = (i32)(positions[layout[level - 1] + i].x * 128.0f);
            option->scaled_preferred_pos_rel_to_player.y = (i32)(positions[layout[level - 1] + i].y * 128.0f);
            option->unk_6c = (i32)(positions[layout[level - 1] + i + 21].x * 128.0f);
            option->unk_70 = (i32)(positions[layout[level - 1] + i + 21].y * 128.0f);
            Int2 *rel = player->inner.is_focused ? (Int2 *)&option->unk_6c : &option->scaled_preferred_pos_rel_to_player;
            option->scaled_preferred_pos.x = player->inner.pos_subpixel.x + rel->x;
            option->scaled_preferred_pos.y = player->inner.pos_subpixel.y + rel->y;
            option->scaled_cur_pos = option->scaled_preferred_pos;
            Float3 pos(0.0f, 0.0f, 0.0f);
            option->anm_id_b0 = player->anm_file->create_vm_inline(g_main_option_scripts[g_Globals.character], &pos,
                                                                   0.0f, 0xe);
            get_vm_or_clear(option->anm_id_b0)->entity_pos.y = -32.0f;
            option->active = 2;
            player = g_Player;
        }
        for (; i < 4; i++)
        {
            main_options[i].active = 0;
            interrupt_tree_inline(main_options[i].anm_id_b0, 1);
        }
        num_main_options = level;
        g_Player->inner.main_options[0].should_instajump = 1;
        g_Player->inner.main_options[1].should_instajump = 1;
        g_Player->inner.main_options[2].should_instajump = 1;
        g_Player->inner.main_options[3].should_instajump = 1;
    }
    i32 season_level = g_Globals.season_level();
    if (num_season_options != season_level)
    {
        layout = g_season_option_layouts[g_Globals.subseason];
        i32 i;
        for (i = 0; i < season_level; i++)
        {
            PlayerOption *option = &subseason_options[i];
            option->scaled_cur_pos.x = player->inner.pos_subpixel.x;
            option->scaled_cur_pos.y = player->inner.pos_subpixel.y;
            g_AnmManager->delete_vm_inline(option->anm_id_b0);
            option->anm_id_b0.id = 0;
            option->index = i;
            Float2 *positions = (Float2 *)player->sht_file_subseason->option_pos;
            option->scaled_preferred_pos_rel_to_player.x = (i32)(positions[layout[season_level - 1] + i].x * 128.0f);
            option->scaled_preferred_pos_rel_to_player.y = (i32)(positions[layout[season_level - 1] + i].y * 128.0f);
            option->unk_6c = (i32)(positions[layout[season_level - 1] + i + 21].x * 128.0f);
            option->unk_70 = (i32)(positions[layout[season_level - 1] + i + 21].y * 128.0f);
            Int2 *rel = player->inner.is_focused ? (Int2 *)&option->unk_6c : &option->scaled_preferred_pos_rel_to_player;
            option->scaled_preferred_pos.x = player->inner.pos_subpixel.x + rel->x;
            option->scaled_preferred_pos.y = player->inner.pos_subpixel.y + rel->y;
            option->scaled_cur_pos = option->scaled_preferred_pos;
            Float3 pos(0.0f, 0.0f, 0.0f);
            option->anm_id_b0 = g_Player->subseason_anm_file->create_vm_inline(
                g_season_option_scripts[g_Globals.subseason], &pos, 0.0f, 0xe);
            get_vm_or_clear(option->anm_id_b0)->entity_pos.y = -32.0f;
            option->active = 2;
            player = g_Player;
        }
        for (; i < 8; i++)
        {
            subseason_options[i].active = 0;
            interrupt_tree_inline(subseason_options[i].anm_id_b0, 1);
        }
        num_season_options = season_level;
        for (i32 j = 0; j < 8; j++)
        {
            g_Player->inner.subseason_options[j].should_instajump = 1;
        }
    }
}

// SoundManager::stop_sound as LTCG inlined it here.
static __forceinline void stop_sound_inline(i32 id)
{
    i32 i;
    for (i = 0; i < SOUND_QUEUE_SIZE; i++)
    {
        if (g_SoundManager.queued_ids[i] < 0)
        {
            break;
        }
        if (g_SoundManager.queued_ids[i] == id)
        {
            g_SoundManager.queued_counts[i] = -1;
            return;
        }
    }
    if (i >= SOUND_QUEUE_SIZE)
    {
        return;
    }
    g_SoundManager.queued_ids[i] = id;
    g_SoundManager.queued_counts[i] = -1;
}

// TODO: functionally complete; block order and register allocation differ.
// FUNCTION: TH16 0x442560
i32 Player::on_tick_body()
{
    switch (inner.state)
    {
    case 0:
    {
        // Respawning: rise from the bottom, clearing bullets and lasers.
        inner.pos_subpixel.y = 0xf000 - inner.time_in_state.current * 0x2800 / 60;
        inner.pos.y = inner.pos_subpixel.y / 128.0f;
        inner.main_options[0].should_instajump = 1;
        inner.main_options[1].should_instajump = 1;
        inner.main_options[2].should_instajump = 1;
        inner.main_options[3].should_instajump = 1;
        Float3 *center;
        f32 radius;
        if (inner.time_in_state.current >= 30)
        {
            center = &inner.pos;
            g_BulletManager->cancel_radius_as_bomb(center, 640.0f, 0);
            radius = 640.0f;
        }
        else
        {
            center = &unk_2c76c;
            radius = inner.time_in_state.current * 512.0f / 30.0f + 64.0f;
            g_LaserManager->cancel_in_radius(center, radius, 0, 1);
            radius *= 0.25f;
        }
        g_LaserManager->cancel_in_radius(center, radius, 0, 0);
        if (inner.time_in_state.current < 60)
        {
            break;
        }
        inner.state = 1;
        inner.time_in_state.set_value(0);
    }
    case 1:
        if (g_MainBomb != NULL && g_MainBomb->can_activate() && (g_InputState.input_rising & INPUT_BOMB))
        {
            g_MainBomb->activate();
        }
        if (g_SubseasonBomb != NULL && g_SubseasonBomb->can_activate() &&
            (g_InputState.input_rising & INPUT_RELEASE))
        {
            g_SubseasonBomb->activate();
        }
        move();
        break;
    case 4:
        // Hit: a few frames to bomb out of it.
        if (inner.time_in_state.current < 8)
        {
            if (g_MainBomb != NULL && (g_InputState.input_rising & INPUT_BOMB) && g_MainBomb->can_activate())
            {
                g_MainBomb->activate();
                start_respawn();
                if (g_SubseasonBomb != NULL && g_SubseasonBomb->can_activate() &&
                    (g_InputState.input_rising & INPUT_RELEASE))
                {
                    g_SubseasonBomb->activate();
                    start_respawn();
                }
            }
            break;
        }
        lose_life();
    case 2:
        if (inner.time_in_state.current == 3)
        {
            // Drop half a power level as items, spread toward the top.
            g_Globals.power = g_Globals.power - g_Globals.power_per_level / 2 < g_Globals.power_per_level
                                  ? g_Globals.power_per_level
                                  : g_Globals.power - g_Globals.power_per_level / 2;
            f32 dx = 0.0f - inner.pos.x;
            f32 dy = inner.pos.y - 224.0f - inner.pos.y;
            f32 angle;
            if (dy == 0.0f && dx == 0.0f)
            {
                angle = ZUN_PI / 2;
            }
            else
            {
                angle = zun_atan2f(dy, dx);
            }
            i32 items[7] = {1, 1, 1, 1, 1, 1, 1};
            for (i32 i = 0; i < 7; i++)
            {
                g_ItemManager->spawn_item(items[i], &inner.pos, 0, i * ZUN_PI / 28.0f + angle - ZUN_PI / 8, 3.0f, 0,
                                          0);
            }
            inner.repopulate_options();
        }
        if (inner.time_in_state.current < 30)
        {
            break;
        }
        if (g_Globals.lives < 0 && inner.time_in_state.current == 30)
        {
            if (g_ReplayManager->mode != REPLAY_PLAYBACK)
            {
                pause_menu_43f350();
            }
            inner.time_in_state++;
            break;
        }
        inner.state = 0;
        g_game_speed = 1.0f;
        create_damage_source(&inner.pos, 32.0f, 16.0f, 30, 150);
        g_Globals.bombs = 3;
        if (g_Gui != NULL)
        {
            g_Gui->update_bombs(3, g_Globals.bomb_fragments);
        }
        unk_2c76c = inner.pos;
        set_position(0.0f, 480.0f);
        inner.iframes.set_inline(280);
        inner.time_in_state.reset_inline();
        break;
    case 3:
        switch (inner.time_in_state.current)
        {
        case 4:
            break;
        case 15:
            g_LaserManager->clear_all(1, 0);
            break;
        }
        break;
    }
    for (i32 i = 0; i < 0x100; i++)
    {
        PlayerDamageSource *source = &inner.damage_sources[i];
        if (!(source->flags & 1))
        {
            continue;
        }
        source->pos.update_secondary_fields();
        source->pos.step();
        source->radius += source->unk_8;
        source->unk_c = wrap_angle(source->unk_c + source->angular_speed);
        source->unk_84 = 0;
        source->timer_60.decrement(1.0f);
        if (source->timer_60.current <= 0)
        {
            source->flags &= ~1;
        }
    }
    if (inner.iframes.current > 0)
    {
        inner.iframes.decrement(1.0f);
        if (inner.time_in_state.current != inner.time_in_state.previous && inner.time_in_state.current % 3 == 0)
        {
            vm.color_2.d3d = 0xff0000ff;
            vm.flags_lo = (vm.flags_lo & ~ANM_VM_COLOR_MODE_MASK) | ANM_VM_COLOR_MODE_1;
        }
        else
        {
            vm.flags_lo &= ~ANM_VM_COLOR_MODE_MASK;
        }
    }
    else
    {
        vm.flags_lo &= ~ANM_VM_COLOR_MODE_MASK;
        if (inner.flags & 0x20)
        {
            if (inner.time_in_state.current % 8 < 4)
            {
                vm.color_2.d3d = 0xffff0000;
                vm.flags_lo = (vm.flags_lo & ~ANM_VM_COLOR_MODE_MASK) | ANM_VM_COLOR_MODE_1;
            }
            i32 scripts[4] = {4, 4, 4, 4};
            AnmId id = anm_file->create_vm(scripts[g_Globals.character], &inner.pos, 0.0f, -1, 0);
            anm_file->set_sprite(get_vm_or_clear(id), vm.sprite_id);
            g_AnmManager->get_vm_with_id(id)->color_1.d3d = 0xffff0000;
        }
        else if (inner.speed_multiplier > 1.01f)
        {
            if (inner.time_in_state.current % 8 < 4)
            {
                vm.color_2.d3d = 0xffffff00;
                vm.flags_lo = (vm.flags_lo & ~ANM_VM_COLOR_MODE_MASK) | ANM_VM_COLOR_MODE_1;
            }
            i32 scripts[4] = {4, 4, 4, 4};
            AnmId id = anm_file->create_vm(scripts[g_Globals.character], &inner.pos, 0.0f, -1, 0);
            anm_file->set_sprite(g_AnmManager->get_vm_with_id(id), vm.sprite_id);
        }
    }
    inner.speed_multiplier = 1.0f;
    inner.unk_1607c = g_zero_vec;
    vm.run();
    if (inner.flags & 0x10)
    {
        if (player_scale_i.end_time != 0)
        {
            player_scale = player_scale_i.step();
            if (player_scale_i.end_time != 0 && inner.time_in_state.current % 3 == 0)
            {
                vm.scale.x = 1.0f;
                vm.scale.y = 1.0f;
            }
            else
            {
                vm.scale.x = vm.scale.y = player_scale;
            }
        }
        else
        {
            vm.scale.x = vm.scale.y = player_scale;
        }
        vm.flags_lo |= ANM_VM_SCALE_CHANGED;
        f32 scale = player_scale;
        hurtbox.min_pos = inner.pos - hurtbox_halfsize * scale;
        hurtbox.max_pos = inner.pos + hurtbox_halfsize * scale;
        item_collect_box.min_pos = inner.pos - item_attract_box_unfocused_halfsize * 0.5f * scale;
        item_collect_box.max_pos = inner.pos + item_attract_box_unfocused_halfsize * 0.5f * scale;
        item_attract_box_focused.min_pos = inner.pos - item_attract_box_focused_halfsize * scale;
        item_attract_box_focused.max_pos = inner.pos + item_attract_box_focused_halfsize * scale;
        item_attract_box_unfocused.min_pos = inner.pos - item_attract_box_unfocused_halfsize * scale;
        item_attract_box_unfocused.max_pos = inner.pos + item_attract_box_unfocused_halfsize * scale;
    }
    else
    {
        vm.flags_lo |= ANM_VM_SCALE_CHANGED;
        vm.scale.x = 1.0f;
        vm.scale.y = 1.0f;
        hurtbox.min_pos = inner.pos - hurtbox_halfsize;
        hurtbox.max_pos = inner.pos + hurtbox_halfsize;
        item_collect_box.min_pos = inner.pos - item_attract_box_unfocused_halfsize * 0.5f;
        item_collect_box.max_pos = inner.pos + item_attract_box_unfocused_halfsize * 0.5f;
        item_attract_box_focused.min_pos = inner.pos - item_attract_box_focused_halfsize;
        item_attract_box_focused.max_pos = inner.pos + item_attract_box_focused_halfsize;
        item_attract_box_unfocused.min_pos = inner.pos - item_attract_box_unfocused_halfsize;
        item_attract_box_unfocused.max_pos = item_attract_box_unfocused_halfsize + inner.pos;
    }
    inner.time_in_state.tick();
    inner.time_in_stage.tick();
    inner.timer_3c.tick();
    if (g_Gui->msg == NULL && g_EnemyManager != NULL && g_EnemyManager->enemy_count_real != 0 &&
        !(*(u32 *)&g_GameThread->flags & 0x4000) && inner.timer_3c.current >= 20 && !(inner.flags & 4) &&
        !(inner.flags & 0x10))
    {
        tick_shooting_state();
    }
    else
    {
        inner.shoot_key_short_timer = -1;
        inner.shoot_key_long_timer = -1;
        unk_2c790 = 0;
        unk_2c794 = 0;
        stop_sound_inline(0x1e);
        stop_sound_inline(0x37);
    }
    tick_bullets();
    return 1;
}
