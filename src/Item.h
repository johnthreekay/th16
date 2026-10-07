#pragma once

#include "AnmVm.h"
#include "Globals.h"
#include "UpdateFunc.h"
#include "ZunMath.h"
#include "ZunTimer.h"
#include "types.h"

// Intrusive list link at the start of every Item, also used for the free
// list heads (ExpHP: zItemList, zItemListHead). In an item, head points at
// the free list the item belongs to.
struct ItemList
{
    ItemList *head;
    ItemList *next;
    ItemList *prev;
    ItemList *unk_c;
};

// Item::item_type (ExpHP's zItemType).
enum ItemType
{
    ITEM_POWER = 1,
    ITEM_POINT = 2,
    ITEM_BIG_POWER = 3,
    ITEM_LIFE_PIECE = 4,
    ITEM_LIFE = 5,
    ITEM_BOMB_PIECE = 6,
    ITEM_BOMB = 7,
    // Full power.
    ITEM_F = 8,
    // The cancel items: small point items worth g_cancel_item_piv[type - 9]
    // of PIV, flying to the player on their own.
    ITEM_PIV_5 = 9,
    ITEM_PIV_10 = 10,
    ITEM_PIV_20 = 11,
    ITEM_PIV_30 = 12,
    ITEM_PIV_40 = 13,
    ITEM_PIV_50 = 14,
    // Spawns as a bomb piece and counts Globals::item_spawn_count.
    ITEM_DDC = 15,
    // Season items (the season gauge); drawn on their own layer.
    ITEM_SEASON = 16,
};

// Item::state.
enum ItemState
{
    ITEM_STATE_FREE = 0,
    ITEM_STATE_FALLING = 1,
    // Cancel items: thrown up, then auto-collected once they fall.
    ITEM_STATE_RISING = 2,
    // Season items: thrown out, slowing down.
    ITEM_STATE_SEASON = 3,
    // Flying to the player (above the collection line, or forced).
    ITEM_STATE_AUTOCOLLECT = 4,
    // Pulled in by the player's attraction box.
    ITEM_STATE_ATTRACTED = 5,
    // Waits intangibility_frames before appearing (cancel and season
    // items, staggered when many spawn at once).
    ITEM_STATE_DELAYED = 6,
};

// One item (power, point, season...). Layout from ExpHP's th-re-data.
struct Item
{
    ItemList node;
    AnmVm vm;
    AnmVm vm_2;
    Float3 position;
    Float3 velocity;
    f32 speed;
    f32 angle;
    ZunTimer time;
    ZunTimer timer_c3c;
    // An ItemState.
    i32 state;
    // An ItemType.
    i32 item_type;
    // The type for cancel and season items, 0 for others; on_draw_body
    // then sets 1 while the offscreen arrow (vm_2) is drawn, else 0.
    i32 unk_c58;
    f32 speed_towards_player;
    i32 intangibility_frames;
    // ItemManager::unk_1c972e8 (always 0) when spawned.
    i32 unk_c64;
    i32 force_autocollect;
    u8 unk_c6c[0xc78 - 0xc6c];

    Item();
    ~Item();
    // Puts the item back on the front of its free list.
    void release();

    // Links the item in at the front of the list.
    void release_to(ItemList *list)
    {
        if (list->next != NULL)
        {
            node.next = list->next;
            list->next->prev = &node;
        }
        if (list->unk_c != NULL)
        {
            list->unk_c = &node;
        }
        list->next = &node;
        node.prev = list;
    }
    // Adds value * 100 to the PIV, capped at its maximum. Works on
    // g_Globals only; LTCG drops this (ExpHP: Globals::collect_piv).
    HARNESS_CALLED void collect_piv(f32 value);
    // Starts the item's sprite scripts for its type.
    i32 init_anm();
    // The flash and sound of a life, bomb or season item appearing.
    i32 spawn_effect();
    // Collection of the full power item (ExpHP: Globals::collect_furu_powah).
    void collect_full_power();
    void collect_power();
    void collect_big_power();
    void collect_point();
};

// The point of collection: items collected above this line, or while
// everything is being auto-collected (ITEM_STATE_AUTOCOLLECT), are worth
// the most.
inline i32 item_collect_line()
{
    return g_Globals.character == CHARACTER_MARISA ? 148 : 128;
}

// ANM scripts of each item type: the item and its offscreen arrow.
extern const i32 g_item_anm_scripts[17][2];

// Every item and the two free lists (ExpHP: zItemManagerInner).
struct ItemManagerInner
{
    // 0x258 normal items followed by 0x1000 cancel items.
    Item items[0x1258];
    ItemList normal_freelist;
    ItemList cancel_freelist;
    f32 slowdown;
};

// Owns every item (ExpHP: zItemManager).
struct ItemManager
{
    u32 flags;
    UpdateFunc *on_tick;
    UpdateFunc *on_draw_1;
    u32 unk_c;
    UpdateFunc *on_draw_2;
    ItemManagerInner inner;
    // LoLK leftover (ExpHP: __lolk_snapshot_inner).
    ItemManagerInner snapshot;
    i32 num_items_onscreen;
    i32 total_items_created;
    // Cleared every tick and never counted up, so cancel items always get
    // the shortest spawn delay.
    i32 num_cancel_items_this_frame;
    // Always 0.
    i32 unk_1c972e8;

    ItemManager();
    ~ItemManager();
    static ItemManager *create();
    i32 initialize();
    void destroy_all();
    i32 on_tick_body();
    // Layer 0 draws the season items (ITEM_SEASON), 1 everything else.
    DECOMP_NOINLINE i32 on_draw_body(i32 layer);
    static i32 __fastcall on_tick_callback(ItemManager *mgr);
    static i32 __fastcall on_draw_1_callback(ItemManager *mgr);
    static i32 __fastcall on_draw_2_callback(ItemManager *mgr);

    // Takes an item from the free list for its kind (bullet cancel items
    // have their own) and launches it. Works on g_ItemManager; LTCG dropped
    // this. unk_3 and unk_6 are never read.
    HARNESS_CALLED Item *spawn_item(i32 type, Float3 *pos, i32 unk_3, f32 angle, f32 speed, i32 unk_6,
                                    i32 force_autocollect);
};

extern ItemManager *g_ItemManager;
