#include <math.h>
#include <string.h>

#include "GameThread.h"
#include "Globals.h"
#include "Item.h"

#include "Bomb.h"
#include "BulletManager.h"
#include "EffectManager.h"
#include "Gui.h"
#include "Input.h"
#include "Player.h"
#include "PopupManager.h"
#include "Rng.h"
#include "SoundManager.h"
#include "ZunList.h"

// GLOBAL: TH16 0x4a6ddc
ItemManager *g_ItemManager;

// GLOBAL: TH16 0x4917f8
const i32 g_item_anm_scripts[17][2] = {
    {-1, -1}, {112, 134}, {113, 135}, {114, 136}, {115, 137}, {116, 138},
    {117, 139}, {118, 140}, {119, 141}, {120, -1}, {121, -1}, {122, -1},
    {123, -1}, {124, -1}, {125, -1}, {-1, -1}, {129, -1},
};

i32 unit5_placeholder(void *object);
i32 get_piv_rounded();

// This file's copy of ZunMath.h's sincosmul, which TH16 keeps once per
// object file. A static of its own so that it can be annotated.
// FUNCTION: TH16 0x430df0
static void __fastcall item_sincosmul(Float3 *dst, f32 angle, f32 radius)
{
#ifdef TH16_PORT
    port_sincosmul(&dst->x, angle, radius);
#else
    __asm {
        mov eax, dst
        fld angle
        fsincos
        fmul radius
        fstp [eax]
        fmul radius
        fstp [eax+4]
    }
#endif
}

// FUNCTION: TH16 0x42f0b0
ItemManager::ItemManager()
{
    memset(this, 0, sizeof(ItemManager));
    g_ItemManager = this;
    flags |= 2;
}

// FUNCTION: TH16 0x42f1a0
Item::Item()
{
}

// FUNCTION: TH16 0x42f1e0
Item::~Item()
{
}

// FUNCTION: TH16 0x42f260
DECOMP_NOINLINE i32 ItemManager::initialize()
{
    UpdateFunc *f = g_UpdateFuncRegistry->create_func((UpdateFuncCallback)on_tick_callback);
    f->flags &= ~UPDATE_FUNC_ACTIVE;
    f->arg = this;
    g_UpdateFuncRegistry->register_on_tick(f, 0x1d);
    on_tick = f;
    f = g_UpdateFuncRegistry->create_func((UpdateFuncCallback)on_draw_1_callback);
    f->flags &= ~UPDATE_FUNC_ACTIVE;
    f->arg = this;
    g_UpdateFuncRegistry->register_on_draw(f, 0x21);
    on_draw_1 = f;
    f = g_UpdateFuncRegistry->create_func((UpdateFuncCallback)on_draw_2_callback);
    f->flags &= ~UPDATE_FUNC_ACTIVE;
    f->arg = this;
    g_UpdateFuncRegistry->register_on_draw(f, 0x13);
    on_draw_2 = f;
    destroy_all();
    return 0;
}

// FUNCTION: TH16 0x42f2d0
ItemManager::~ItemManager()
{
    g_UpdateFuncRegistry->unregister_locked(on_tick);
    g_UpdateFuncRegistry->unregister_locked(on_draw_1);
    g_UpdateFuncRegistry->unregister_locked(on_draw_2);
    g_ItemManager = NULL;
}

// FUNCTION: TH16 0x42f440
ItemManager *ItemManager::create()
{
    ItemManager *mgr = new ItemManager();
    if (mgr->initialize() != 0)
    {
        delete mgr;
        return NULL;
    }
    return mgr;
}

// GLOBAL: TH16 0x4917e0
// The PIV value of the bullet cancel items (types 9 to 14).
static const f32 g_cancel_item_piv[6] = {5.0f, 10.0f, 20.0f, 30.0f, 40.0f, 50.0f};

// Player::angle_to_player as LTCG inlined it into on_tick_body's state 4.
static __forceinline f32 angle_to_player_inline(Float3 *pos)
{
    f32 dy = g_Player->inner.pos.y - pos->y;
    f32 dx = g_Player->inner.pos.x - pos->x;
    if (dy == 0.0f && dx == 0.0f)
    {
        return ZUN_PI / 2;
    }
    return (f32)atan2((double)dy, (double)dx);
}

// The original calls Player::angle_to_player in state 5 and has it inlined
// in state 4. Either form makes LTCG realign on_tick_body early enough to
// give Item::init_anm (0x430c90) a padded frame in our build, so both go
// through these out-of-line helpers until that is understood.
static DECOMP_NOINLINE f32 item_angle_to_player(Float3 *pos)
{
    return g_Player->angle_to_player(pos);
}

static DECOMP_NOINLINE f32 item_angle_to_player_4(Float3 *pos)
{
    return angle_to_player_inline(pos);
}

// Whether an item has left the bottom or a side of the play area.
static __forceinline i32 item_offscreen(Item *item)
{
    return item->position.y > 472.0f || fabsf(item->position.x) >= 200.0f;
}

// Moves every item: delayed spawns (state 6), falling items (1), cancel
// items rising before they fall (2), season items (3), items flying to the
// player (4: auto-collected, 5: attracted), then collection, attraction and
// the sprite VMs.
// TODO: the original realigns its frame (and esp, -8); register allocation and the order of the position updates differ.
// FUNCTION: TH16 0x42f4e0
i32 ItemManager::on_tick_body()
{
    Player *player = g_Player;
    Item *item = inner.items;
    unk_1c972e4 = 0;
    num_items_onscreen = 0;
    for (i32 i = 0; i < 0x1258; i++, item++)
    {
        if (item->state == 0)
        {
            continue;
        }
        if (item->state == 6)
        {
            if (--item->intangibility_frames >= 0)
            {
                continue;
            }
            item->init_anm();
            player = g_Player;
            continue;
        }
        if (item->state == 1)
        {
            goto state_1;
        }
        if (item->state == 2)
        {
            item->position += item->velocity * g_game_speed;
            item->velocity.y += g_game_speed * 0.03f;
            if (item->velocity.y >= 0.0f)
            {
                item->speed_towards_player = player->sht_file->grazebox_radius;
                if (item->item_type != 16)
                {
                    goto start_autocollect;
                }
                item->state = 1;
                goto state_1;
            }
            if (item_offscreen(item))
            {
                // release(), inlined here.
                ItemList *head = item->node.head;
                item->state = 0;
                item->release_to(head);
                continue;
            }
            goto collect;
        }
        if (item->state == 3)
        {
            item->position += item->velocity * g_game_speed;
            item->velocity.y += g_game_speed * 0.03f;
            item->speed -= 0.03f;
            item_sincosmul(&item->velocity, item->angle, item->speed);
            if ((g_MainBomb->in_use == 1 && g_MainBomb->timer.current < 60) ||
                (g_SubseasonBomb->in_use == 1 && g_SubseasonBomb->timer.current < 10000))
            {
                item->force_autocollect = 1;
            }
            if (item->speed <= 0.0f)
            {
                player = g_Player;
                item->velocity.x = 0.0f;
                item->velocity.z = 0.0f;
                item->velocity.y = 0.0f;
                item->speed = 0.0f;
                item->angle = ZUN_PI / 2;
                item->speed_towards_player = player->sht_file->grazebox_radius;
                item->state = item->force_autocollect != 0 ? 4 : 1;
                goto state_1;
            }
            if (!item_offscreen(item))
            {
                player = g_Player;
                goto collect;
            }
            item->release();
            player = g_Player;
            continue;
        }
        if (item->state == 4)
        {
            goto state_4;
        }
        if (item->state != 5)
        {
            goto collect;
        }
        if ((player->inner.state != 2 && player->inner.state != 4 && (f32)item_collect_line() > player->inner.pos.y) ||
            (g_MainBomb->in_use == 1 && g_MainBomb->timer.current < 60) || g_SubseasonBomb->is_active_before(10000) ||
            g_Gui->msg != NULL)
        {
            goto autocollect;
        }
        item_sincosmul(&item->velocity, item_angle_to_player(&item->position), item->speed_towards_player);
        item->position += item->velocity * g_game_speed;
        if (item->speed_towards_player < 12.0f)
        {
            item->speed_towards_player += 0.2f;
        }
        player = g_Player;
        if (player->inner.state == 4)
        {
            item->state = 1;
            item->velocity.x = 0.0f;
            item->velocity.y = 0.0f;
        }
        goto collect;

    state_1:
        if (item->intangibility_frames > 0)
        {
            if (--item->intangibility_frames > 0)
            {
                continue;
            }
            item->spawn_effect();
            player = g_Player;
            continue;
        }
        if ((player->inner.state != 2 && player->inner.state != 4 && (f32)item_collect_line() > player->inner.pos.y) ||
            (g_MainBomb->in_use == 1 && g_MainBomb->timer.current < 60) ||
            (g_SubseasonBomb->in_use == 1 && g_SubseasonBomb->timer.current < 10000) || g_Gui->msg != NULL)
        {
            goto autocollect;
        }
        item->position += item->velocity * g_game_speed * inner.slowdown;
        item->velocity.y += g_game_speed * 0.03f * inner.slowdown;
        if (item->time.current >= 32)
        {
            if (inner.slowdown < 1.0f)
            {
                item->vm.pos = Float3(g_replay_unsafe_rng.randf_neg_1_to_1(), g_replay_unsafe_rng.randf_neg_1_to_1(), 0.0f);
                item->vm.color_1.g = 0xa0;
                item->vm.color_1.r = 0xff;
            }
            else
            {
                item->vm.color_1.g = 0xff;
                item->vm.color_1.r = 0xff;
            }
            item->vm.color_1.a = 0xff;
            item->vm.color_1.b = 0xff;
            player = g_Player;
        }
        if (item->velocity.y >= 0.0f)
        {
            item->velocity.x = 0.0f;
        }
        if (item->velocity.y > 2.0f)
        {
            item->velocity.y = 2.0f;
        }
        if (item_offscreen(item))
        {
            item->release();
            continue;
        }
        goto collect;

    autocollect:
        item->speed_towards_player = player->sht_file->grazebox_radius;
    start_autocollect:
        item->state = 4;
    state_4:
        if (item->item_type != 9 && item->item_type != 10 && item->item_type != 11 && item->item_type != 12 &&
            item->item_type != 13 && item->item_type != 14)
        {
            g_Globals.unk_dc = 8;
        }
        item_sincosmul(&item->velocity, item_angle_to_player_4(&item->position), item->speed_towards_player);
        item->position += item->velocity * g_game_speed;
        if (item->speed_towards_player < 12.0f)
        {
            item->speed_towards_player += 0.2f;
        }
        player = g_Player;
        if (player->inner.state == 4)
        {
            item->state = 1;
            item->velocity.x = 0.0f;
            item->velocity.y = 0.0f;
        }

    collect:
        if (player->inner.state != 2)
        {
            Float3 half(0.0f, 0.0f, 0.0f);
            if (player->item_collect_box.min_pos.x <= item->position.x + half.x &&
                player->item_collect_box.min_pos.y <= item->position.y + half.y &&
                item->position.x <= player->item_collect_box.max_pos.x &&
                item->position.y <= player->item_collect_box.max_pos.y)
            {
                switch (item->item_type)
                {
                case 8:
                    item->collect_full_power();
                    break;
                case 1:
                    item->collect_power();
                    break;
                case 3:
                    item->collect_big_power();
                    break;
                case 2:
                    item->collect_point();
                    break;
                case 5:
                    if (g_Globals.collect_extend(0))
                    {
                        g_SoundManager.play_sound_centered(0x11, 0);
                        g_Gui->sub_42bcf0(0, 4);
                    }
                    break;
                case 6:
                    g_Globals.collect_bomb_fragment(0);
                    break;
                case 7:
                    g_Globals.collect_bomb(0);
                    break;
                case 9:
                case 10:
                case 11:
                case 12:
                case 13:
                case 14:
                {
                    f32 value = g_cancel_item_piv[item->item_type - 9];
                    item->collect_piv(value);
                    g_SubseasonBomb->release_bonus += value;
                    g_Globals.add_to_score(get_piv_rounded() / 100 * 10);
                    break;
                }
                case 16:
                    if (g_Globals.collect_season_item(0))
                    {
                        g_Player->inner.repopulate_options();
                        g_PopupManager->generate_small_score_popup(&item->position, -1, 0xffffff40);
                        g_SoundManager.play_sound_at_position(0x3f, item->position.x);
                    }
                    g_Globals.add_to_score(10);
                    Gui::update_season_gauge();
                    break;
                default:
                    goto collected;
                }
                player = g_Player;
            collected:
                g_SoundManager.play_sound_at_position(0x25, item->position.x);
                item->release();
                continue;
            }
            if (item->state != 5 && item->state != 4 && item->state != 3)
            {
                u32 focused = g_InputState.input & INPUT_FOCUS;
                if ((focused && player->item_attract_box_focused.min_pos.x <= item->position.x + half.x &&
                     player->item_attract_box_focused.min_pos.y <= item->position.y + half.y &&
                     item->position.x <= player->item_attract_box_focused.max_pos.x &&
                     item->position.y <= player->item_attract_box_focused.max_pos.y) ||
                    (!focused && player->item_attract_box_unfocused.min_pos.x <= item->position.x + half.x &&
                     player->item_attract_box_unfocused.min_pos.y <= item->position.y + half.y &&
                     item->position.x <= player->item_attract_box_unfocused.max_pos.x &&
                     item->position.y <= player->item_attract_box_unfocused.max_pos.y))
                {
                    if (item->item_type != 9 && item->item_type != 10 && item->item_type != 11 &&
                        item->item_type != 12 && item->item_type != 13 && item->item_type != 14)
                    {
                        item->state = 5;
                        item->speed_towards_player = player->sht_file->grazebox_radius / 3.0f;
                    }
                }
            }
        }
        if (item->vm.flags_lo & ANM_VM_VISIBLE)
        {
            item->vm.run();
            player = g_Player;
        }
        if (item->vm_2.flags_lo & ANM_VM_VISIBLE)
        {
            item->vm_2.run();
            player = g_Player;
        }
        item->time.tick();
        num_items_onscreen++;
    }
    if (inner.slowdown < 1.0f)
    {
        inner.slowdown += 0.1f;
    }
    return 1;
}

// FUNCTION: TH16 0x4308f0
i32 __fastcall ItemManager::on_tick_callback(ItemManager *mgr)
{
    if (g_GameThread != NULL)
    {
        if (g_GameThread->flags.flag_0 | g_GameThread->flags.paused)
        {
            return 1;
        }
        if (g_GameThread->flags.flag_10)
        {
            return 1;
        }
    }
    return mgr->on_tick_body();
}

// FUNCTION: TH16 0x430920
i32 __fastcall ItemManager::on_draw_1_callback(ItemManager *mgr)
{
    if (g_GameThread != NULL && g_GameThread->flags.paused)
    {
        return 1;
    }
    return mgr->on_draw_body(1);
}

// Items above the top of the screen show their arrow instead, fading in
// over the 32 pixels above it.
// FUNCTION: TH16 0x4307a0
i32 ItemManager::on_draw_body(i32 layer)
{
    Item *item = inner.items;
    for (i32 i = 0; i < 0x1258; i++, item++)
    {
        if (item->state == 0 || !(item->vm.flags_lo & ANM_VM_VISIBLE) || item->intangibility_frames > 0)
        {
            continue;
        }
        if (layer == 0)
        {
            if (item->item_type != 16)
            {
                continue;
            }
        }
        else if (layer == 1 && item->item_type == 16)
        {
            continue;
        }
        item->vm.entity_pos = item->position;
        item->vm_2.entity_pos = item->position;
        if (item->vm.pos.y < -8.0f)
        {
            if (item->vm_2.flags_lo & ANM_VM_VISIBLE)
            {
                f32 distance = item->vm_2.pos.y + 8.0f;
                item->vm_2.pos.y = 8.0f;
                if (distance >= 32.0f)
                {
                    item->vm_2.color_1.a = 0xff;
                }
                else
                {
                    item->vm_2.color_1.a = distance * (1.0f / 32.0f) * 255.0f;
                }
                g_AnmManager->draw_vm(&item->vm_2);
            }
            item->unk_c58 = 1;
        }
        else
        {
            g_AnmManager->draw_vm(&item->vm);
            item->unk_c58 = 0;
        }
    }
    return 1;
}

// FUNCTION: TH16 0x430940
i32 __fastcall ItemManager::on_draw_2_callback(ItemManager *mgr)
{
    if (g_GameThread != NULL && g_GameThread->flags.paused)
    {
        return 1;
    }
    return mgr->on_draw_body(0);
}

// FUNCTION: TH16 0x430d90
void Item::release()
{
    ItemList *head = node.head;
    state = 0;
    if (head->next != NULL)
    {
        node.next = head->next;
        head->next->prev = &node;
    }
    if (head->unk_c != NULL)
    {
        head->unk_c = &node;
    }
    head->next = &node;
    node.prev = head;
}

// FUNCTION: TH16 0x430dc0
HARNESS_CALLED void Item::collect_piv(f32 value)
{
    g_Globals.piv += (i32)(value * 100.0f);
    if (g_Globals.piv > g_Globals.max_piv)
    {
        g_Globals.piv = g_Globals.max_piv;
    }
}

// FUNCTION: TH16 0x4184a0
void ItemManager::destroy_all()
{
    total_items_created = 0;
    unk_1c972e8 = 0;
    memset(inner.items, 0, sizeof(inner.items));
    ItemList *list = &inner.normal_freelist;
    list->head = NULL;
    list->next = NULL;
    list->prev = NULL;
    list->unk_c = NULL;
    for (i32 i = 0; i < 0x258; i++)
    {
        Item *item = &inner.items[i];
        item->node.next = NULL;
        item->node.head = list;
        item->node.prev = NULL;
        item->node.unk_c = NULL;
        item->release_to(list);
    }
    list = &inner.cancel_freelist;
    list->head = NULL;
    list->next = NULL;
    list->prev = NULL;
    list->unk_c = NULL;
    for (i32 i = 0x258; i < 0x1258; i++)
    {
        Item *item = &inner.items[i];
        item->node.next = NULL;
        item->node.head = list;
        item->node.prev = NULL;
        item->node.unk_c = NULL;
        item->release_to(list);
    }
    inner.slowdown = 1.0f;
}

// FUNCTION: TH16 0x430c90
i32 Item::init_anm()
{
    if (item_type == 16)
    {
        state = 3;
        g_BulletManager->bullet_anm->copy_vm_and_run(&vm, g_Globals.subseason + 0x81);
    }
    else
    {
        state = 2;
        g_BulletManager->bullet_anm->copy_vm_and_run(&vm, g_item_anm_scripts[item_type][0]);
    }
    vm_2.flags_lo &= ~ANM_VM_VISIBLE;
    vm_2.instr_offset = -1;
    return 0;
}

// FUNCTION: TH16 0x430d10
i32 Item::spawn_effect()
{
    if (item_type == 4 || item_type == 6 || item_type == 15 || item_type == 5 || item_type == 7)
    {
        g_EffectManager->effect_anm->create_vm(0x65, &position, 0.0f, -1, 0);
        if (item_type == 4 || item_type == 5)
        {
            g_SoundManager.play_sound_centered(0x4a, 0);
        }
        else
        {
            g_SoundManager.play_sound_centered(0x30, 0);
        }
    }
    return 0;
}

// TODO: the original reserves 8 bytes of unused locals and saves esi up
// front; ours shrink-wraps the push of esi into the normal-item branch.
// FUNCTION: TH16 0x430960
HARNESS_CALLED Item *ItemManager::spawn_item(i32 type, Float3 *pos, i32 unk_3, f32 angle, f32 speed, i32 unk_6,
                              i32 force_autocollect)
{
    ItemManager *mgr = g_ItemManager;
    Item *item;
    mgr->total_items_created++;
    if (type == 9 || type == 10 || type == 11 || type == 12 || type == 13 || type == 14 || type == 16)
    {
        item = (Item *)mgr->inner.cancel_freelist.next;
        if (item != NULL)
        {
            item->unk_c64 = mgr->unk_1c972e8;
            if (mgr->unk_1c972e4 >= 0x400)
            {
                item->intangibility_frames = mgr->total_items_created % 32 + 16;
            }
            else if (mgr->unk_1c972e4 >= 0x200)
            {
                item->intangibility_frames = mgr->total_items_created % 16 + 8;
            }
            else if (mgr->unk_1c972e4 >= 0x100)
            {
                item->intangibility_frames = mgr->total_items_created % 8 + 4;
            }
            else
            {
                item->intangibility_frames = mgr->total_items_created % 4;
            }
            item->state = 6;
            item->item_type = type;
            item->unk_c58 = type;
            item->position = *pos;
            item_sincosmul(&item->velocity, angle, speed);
            item->velocity.z = 0.0f;
            item->time = 0;
            item->angle = angle;
            item->speed = speed;
            item->force_autocollect = force_autocollect;
            if (item->node.next != NULL)
            {
                item->node.next->prev = item->node.prev;
            }
            if (item->node.prev != NULL)
            {
                item->node.prev->next = item->node.next;
            }
            item->node.next = NULL;
            item->node.prev = NULL;
        }
    }
    else
    {
        item = (Item *)mgr->inner.normal_freelist.next;
        if (item != NULL)
        {
            item->state = 1;
            item->position = *pos;
            if (item->position.x <= -192.0f)
            {
                item->position.x = -192.0f;
            }
            else if (item->position.x >= 192.0f)
            {
                item->position.x = 192.0f;
            }
            if (type == 15)
            {
                g_Globals.item_spawn_count++;
            }
            i32 anm_type = type != 15 ? type : 6;
            item_sincosmul(&item->velocity, angle, speed);
            item->velocity.z = 0.0f;
            item->time.set_value(0);
            item->speed = 0.0f;
            item->speed_towards_player = 0.0f;
            item->intangibility_frames = 0;
            item->item_type = anm_type;
            item->spawn_effect();
            item->unk_c58 = 0;
            g_BulletManager->bullet_anm->copy_vm_and_run(&item->vm, g_item_anm_scripts[anm_type][0]);
            g_BulletManager->bullet_anm->copy_vm_and_run(&item->vm_2, g_item_anm_scripts[anm_type][1]);
            item->force_autocollect = force_autocollect;
            item->vm.color_1.d3d = 0xffffffff;
            ((ZunList<void> *)&item->node)->unlink();
        }
    }
    return item;
}

// TODO: the original realigns its frame to 8 bytes (ebx frame, 8 bytes of
// locals), like add_power's other callers; see Globals::add_to_score.
// FUNCTION: TH16 0x4303a0
void Item::collect_full_power()
{
    if (g_Globals.power >= g_Globals.max_power)
    {
        i32 piv = g_Globals.piv + 10000;
        if (piv > g_Globals.max_piv)
        {
            piv = g_Globals.max_piv;
        }
        g_Globals.piv = piv;
        g_PopupManager->generate_small_score_popup(&position, 100, 0xff40ff40);
        g_SoundManager.play_sound_at_position(0xd, position.x);
    }
    if (g_Globals.add_power(g_Globals.max_power))
    {
        g_Player->inner.repopulate_options();
        g_PopupManager->generate_small_score_popup(&position, -1, 0xffffff40);
        g_SoundManager.play_sound_at_position(0xd, position.x);
    }
}

// TODO: the original takes piv % 10 with idiv and keeps both roundings, and
// realigns its frame to 8 bytes (see collect_full_power).
// FUNCTION: TH16 0x430100
void Item::collect_power()
{
    f32 player_y = g_Player->inner.pos.y;
    i32 value;
    if (g_Globals.power >= g_Globals.max_power)
    {
        i32 line = item_collect_line();
        if ((f32)line >= player_y || state == 4)
        {
            value = g_Globals.piv / 100;
            value -= value % 10;
            value = value / 10 * 10;
            if (value <= 0)
            {
                value = 10;
            }
            g_PopupManager->generate_small_score_popup(&position, value, 0xffffff00);
            g_Globals.unk_d8++;
            g_Globals.unk_d0 += value;
            g_Globals.last_collect_pos = g_Player->inner.pos;
            g_PopupManager->generate_small_score_popup(&position, value, -1);
        }
        else
        {
            i32 base = g_Globals.piv / 100;
            base -= base % 10;
            value = base * 3 / 4 - base * 3 / 4 * ((i32)player_y - line) / 450;
            value = value / 10 * 10;
            if (value <= 0)
            {
                value = 10;
            }
            g_PopupManager->generate_small_score_popup(&position, value, -1);
            g_PopupManager->generate_small_score_popup(&position, value, -1);
        }
    }
    else
    {
        if (g_Globals.add_power(1))
        {
            g_Player->inner.repopulate_options();
            g_PopupManager->generate_small_score_popup(&position, -1, 0xffffff40);
            g_SoundManager.play_sound_at_position(0xd, position.x);
        }
        value = 100;
    }
    g_Globals.add_to_score(value);
    if ((f32)item_collect_line() >= player_y || state == 4)
    {
        g_Globals.unk_d8++;
        g_Globals.unk_d0 += value;
        g_Globals.last_collect_pos = g_Player->inner.pos;
    }
}

// TODO: the original realigns its frame to 8 bytes (see collect_full_power).
// FUNCTION: TH16 0x4304a0
void Item::collect_big_power()
{
    f32 player_y = g_Player->inner.pos.y;
    i32 value;
    if (g_Globals.power >= g_Globals.max_power)
    {
        value = 20000;
        g_Globals.add_to_score(20000);
        g_PopupManager->generate_small_score_popup(&position, 20000, 0xff808080);
        g_SoundManager.play_sound_at_position(0xd, position.x);
    }
    else
    {
        value = 100;
        if (g_Globals.add_power(g_Globals.power_per_level))
        {
            g_Player->inner.repopulate_options();
            g_SoundManager.play_sound_at_position(0xd, position.x);
            g_PopupManager->generate_small_score_popup(&position, -1, 0xffffff40);
        }
    }
    g_Globals.add_to_score(value);
    if ((f32)item_collect_line() >= player_y || state == 4)
    {
        g_Globals.unk_d8++;
        g_Globals.unk_d0 += value;
        g_Globals.last_collect_pos = g_Player->inner.pos;
    }
}

// TODO: the original takes piv % 10 with idiv and keeps both roundings
// (ours folds them into one division), so registers differ.
// FUNCTION: TH16 0x430620
void Item::collect_point()
{
    i32 line = item_collect_line();
    Player *player = g_Player;
    i32 value;
    if ((f32)line >= player->inner.pos.y || state == 4)
    {
        value = g_Globals.piv / 100;
        value -= value % 10;
        value = value / 10 * 10;
        if (value <= 0)
        {
            value = 10;
        }
        g_PopupManager->generate_small_score_popup(&position, value, 0xffffff00);
        g_Globals.unk_d8++;
        g_Globals.unk_d0 += value;
        g_Globals.last_collect_pos = player->inner.pos;
    }
    else
    {
        i32 base = g_Globals.piv / 100;
        base -= base % 10;
        value = base * 3 / 4 - base * 3 / 4 * ((i32)player->inner.pos.y - line) / 450;
        value = value / 10 * 10;
        if (value <= 0)
        {
            value = 10;
        }
        g_PopupManager->generate_small_score_popup(&position, value, -1);
    }
    g_Globals.add_to_score(value);
    g_Globals.num_point_items_collected++;
}
