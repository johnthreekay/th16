#include <d3dx9math.h>

#include "CriticalSections.h"
#include "FpsCounter.h"
#include "AsciiManager.h"
#include "GameThread.h"
#include "Supervisor.h"
#include "UpdateFunc.h"

static_assert(sizeof(FpsCounter) == 0x88, "FpsCounter size");

double LTCG_VECTORCALL get_runtime();

// SYNTHETIC: TH16 0x426280
// FpsCounter::`scalar deleting destructor'

FpsCounter::~FpsCounter()
{
    g_UpdateFuncRegistry->unregister_locked(on_draw);
    g_FpsCounter = NULL;
}

// Once a second: measures the frame rate (counting skipped frames), counts
// measurements above 65 fps, and while a game runs (not loading or in a
// menu) adds the second to the actual and expected frame totals that the
// slowdown rate of replays and scores comes from (above 57 fps counts as
// a full 60).
// FUNCTION: TH16 0x4262f0
HARNESS_CALLED int FpsCounter::update()
{
    double now = get_runtime();
    if (last_time > now)
    {
        last_time = now;
    }
    now -= last_time;
    if (now >= 1.0)
    {
        last_time += now;
        fps = frame_count / now;
        if (fps > 65.0f)
        {
            too_fast_count++;
        }
        else
        {
            too_fast_count = 0;
        }
        if (g_GameThread != NULL)
        {
            if (!g_GameThread->flags.loading && !g_GameThread->flags.in_menu)
            {
                total_expected += 60.0;
                if (fps > 57.0f)
                {
                    total_actual += 60.0;
                }
                else
                {
                    total_actual += fps;
                }
            }
            g_GameThread->flags.ticked_while_loading = 0;
        }
        frame_count = 0;
    }
    frame_count += g_Supervisor.config.frame_skip + 1;
    return UPDATE_FUNC_CONTINUE;
}

// Draws the frame rate in the bottom right corner (red below 30, pink
// below 40); not in the ending (game mode 15) or on the title (4).
// FUNCTION: TH16 0x4263e0
int FpsCounter::draw()
{
    if (g_Supervisor.gamemode_to_switch_to == 15 || g_Supervisor.gamemode_to_switch_to == 4)
    {
        return UPDATE_FUNC_CONTINUE;
    }
    f32 fps = this->fps;
    if (g_AsciiManager == NULL)
    {
        return UPDATE_FUNC_CONTINUE;
    }
    D3DCOLOR color;
    if (fps < 30.0f)
    {
        color = 0xff5050ff;
    }
    else if (fps < 40.0f)
    {
        color = 0xffa0a0ff;
    }
    else
    {
        color = 0xffffffff;
    }
    g_AsciiManager->color.d3d = color;
    D3DXVECTOR3 pos;
    pos.x = 588.0f;
    pos.y = 470.0f;
    pos.z = 0.0f;
    g_AsciiManager->create_debug_stringf(&pos, "%2.1ffps", fps + 0.05);
    g_AsciiManager->color.d3d = 0xffffffff;
    return UPDATE_FUNC_CONTINUE;
}

// FUNCTION: TH16 0x426490
int __fastcall FpsCounter::on_draw_callback(FpsCounter *counter)
{
    return counter->draw();
}

// GLOBAL: TH16 0x4a6dc8
FpsCounter *g_FpsCounter;
