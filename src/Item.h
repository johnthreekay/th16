#pragma once

#include "AnmVm.h"
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
    i32 state;
    i32 item_type;
    i32 unk_c58;
    f32 speed_towards_player;
    i32 intangibility_frames;
    i32 unk_c64;
    i32 force_autocollect;
    u8 unk_c6c[0xc78 - 0xc6c];

    Item();
    ~Item();
    // Puts the item back on the front of its free list.
    void release();
    // Adds value * 100 to the PIV, capped at its maximum. Works on
    // g_Globals only; LTCG drops this (ExpHP: Globals::collect_piv).
    HARNESS_CALLED void collect_piv(f32 value);
    // Starts the item's sprite scripts for its type.
    i32 init_anm();
    // The flash and sound of a life, bomb or season item appearing.
    i32 spawn_effect();
};

// ANM scripts of each item type: the item and its offscreen arrow.
extern const i32 g_item_anm_scripts[17][2];

struct ItemManagerInner
{
    // 0x258 normal items followed by 0x1000 cancel items.
    Item items[0x1258];
    ItemList normal_freelist;
    ItemList cancel_freelist;
    f32 slowdown;
};

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
    i32 unk_1c972e4;
    i32 unk_1c972e8;

    ItemManager();
    ~ItemManager();
    static ItemManager *create();
    i32 initialize();
    void destroy_all();
    i32 on_tick_body();
    i32 on_draw_body(i32 layer);
    static i32 __fastcall on_tick_callback(ItemManager *mgr);
    static i32 __fastcall on_draw_1_callback(ItemManager *mgr);
    static i32 __fastcall on_draw_2_callback(ItemManager *mgr);
};

extern ItemManager *g_ItemManager;
