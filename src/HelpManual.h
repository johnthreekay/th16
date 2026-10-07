#pragma once

#include "AnmManager.h"
#include "MenuHelper.h"
#include "UpdateFunc.h"
#include "ZunTimer.h"
#include "types.h"

// HelpManual::state.
enum HelpManualState
{
    HELP_STATE_START = 0,
    HELP_STATE_RUN = 1,
    // Closing: closed is set 30 frames in.
    HELP_STATE_CLOSE = 2,
};

// HelpManual::substate while running.
enum HelpManualSubstate
{
    // Shows the list of the nine pages.
    HELP_SUBSTATE_SETUP = 0,
    // Choosing a page.
    HELP_SUBSTATE_LIST = 1,
    // The worker thread reads the page's picture (help_NN.png).
    HELP_SUBSTATE_LOADING = 2,
    HELP_SUBSTATE_LOADED = 3,
    // Showing a page: up and down turn it, a cancel key goes back to
    // the list.
    HELP_SUBSTATE_PAGE = 4,
    // Turning to the next or previous page.
    HELP_SUBSTATE_TURNING = 5,
};

// The manual (from the title screen or the pause menu): nine pages whose
// pictures (help_01.png to help_09.png) are loaded into help.anm's second
// texture one at a time.
// Layout from ExpHP's th-re-data (zHelpManual).
struct HelpManual
{
    u32 flags;
    UpdateFunc *on_tick;
    UpdateFunc *on_draw;
    // HelpManualState.
    i32 state;
    ZunTimer timer;
    MenuHelper menu;
    // The list's nine entries, then the page itself.
    AnmId page_vms[10];
    // Set once the manual has closed; the menu that opened it then
    // deletes it.
    i32 closed;
    // Horizontal position of the VMs (128 from the title, 32 from the
    // pause menu).
    f32 x_offset;
    AnmLoaded *help_anm;
    // The page picture as read, until it becomes the texture.
    u8 *file_data;
    // HelpManualSubstate.
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
