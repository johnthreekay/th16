// A reader for globals that the decompiled code only writes.
//
// Not ZUN's code and never run: it gives link-time code generation calls
// or address uses the decompiled code does not have, so that it compiles
// the real functions as in the original (see README.md, "Placeholders and
// stand-in callers"). The harness files are split the way the work was;
// regrouping them changes LTCG's choices for unrelated functions.
#include "../Supervisor.h"
#include "../UpdateFunc.h"
#include "../Globals.h"
#include "../LoadingThread.h"
#include "../PauseMenu.h"
#include "../PopupManager.h"

// LTCG drops stores to globals nothing reads, and the original keeps them
// (GameThread::thread_start at 0x42cb60, Supervisor at 0x43c630 and
// 0x43c6a0): some of these are only written in the decompiled code.
int harness_unit6_read_globals()
{
    return g_unk_4a6ef0 + g_arcade_width + g_arcade_height + g_game_2d_origin_x + g_game_2d_origin_y +
           (int)g_PauseMenu + (int)g_PopupManager + (int)g_LoadingThread + g_frame_pacing.mode;
}

#include "../ReplayManager.h"
#include "../Scorefile.h"
