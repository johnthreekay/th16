#pragma once

#include "AnmManager.h"
#include "MenuHelper.h"
#include "UpdateFunc.h"
#include "ZunTimer.h"
#include "types.h"

// The in-game manual: help.anm pages, loaded on the loading thread.
// Layout from ExpHP's th-re-data (zHelpManual).
struct HelpManual
{
    u32 flags;
    UpdateFunc *on_tick;
    UpdateFunc *on_draw;
    i32 state;
    ZunTimer timer;
    MenuHelper menu;
    AnmId page_vms[10];
    i32 unk_124;
    f32 unk_128;
    AnmLoaded *help_anm;
    u8 *file_data;
    i32 substate;
    char file_name[0x80];
    i32 file_size;

    HelpManual();
    ~HelpManual();
    static HelpManual *create();
    static void destroy();
    i32 initialize();
    i32 on_tick_body();
    static i32 __fastcall on_tick_callback(HelpManual *manual);
    static i32 __fastcall on_draw_callback(HelpManual *manual);
};

extern HelpManual *g_HelpManual;
