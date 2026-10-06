#include <string.h>

#include "GameThread.h"
#include "Globals.h"
#include "Item.h"

#include "BulletManager.h"
#include "EffectManager.h"
#include "SoundManager.h"

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
