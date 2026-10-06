#include <string.h>

#include "GameThread.h"
#include "Globals.h"
#include "Item.h"

#include "BulletManager.h"
#include "EffectManager.h"
#include "Player.h"
#include "PopupManager.h"
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

// Placeholder (not decompiled yet).
// STUB: TH16 0x42f4e0
DECOMP_NOINLINE i32 ItemManager::on_tick_body()
{
    return unit5_placeholder(this);
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
Item *ItemManager::spawn_item(i32 type, Float3 *pos, i32 unk_3, f32 angle, f32 speed, i32 unk_6,
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
            sincosmul(&item->velocity, angle, speed);
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
            sincosmul(&item->velocity, angle, speed);
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
