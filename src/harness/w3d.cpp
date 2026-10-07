// Stand-in callers for wave 3 range D (0x43dc30-0x44f710: Player,
// PauseMenu, ReplayManager, PopupManager, the title screen) whose shape
// depends on how the rest of the game calls them.
#include "../AnmManager.h"
#include "../MainMenu.h"
#include "../Player.h"
#include "../Supervisor.h"
#include "../ReplayManager.h"
#include "../Scorefile.h"
#include "../ZunMath.h"

// Like PauseMenu's snapshot of the game screen (0x43f62c).
i32 harness_w3d_copy_screen(AnmId id, f32 scale)
{
    return g_AnmManager->copy_screen_to_sprite(id, (i32)(scale * 32.0f), (i32)(scale * 16.0f), (i32)(scale * 384.0f),
                                               (i32)(scale * 448.0f));
}

// The camera setup (0x43c858) computes its projection from a half field of
// view.
f32 harness_w3d_tan(f32 *fov, f32 scale)
{
    return fov[1] / zun_tanf(fov[0] * scale);
}

// The pause menu and the replay save menu (0x4406b1, 0x453dda).
i32 harness_w3d_replay_end_stage(i32 stage)
{
    return g_ReplayManager->set_end_stage(stage);
}

// GameThread::thread_start (0x42d027).
void harness_w3d_replay_start_stage()
{
    g_ReplayManager->start_stage();
}

// GameThread's stage setup (0x42dd2d, 0x42df45).
void harness_w3d_replay_begin_stage()
{
    g_ReplayManager->begin_stage();
}

// The title menus (0x44c8c0 and others) always interrupt children of
// anm_ids[0] with 29.
void harness_w3d_title_interrupt(TitleInf *menu, i32 script)
{
    menu->interrupt_child(0, script, 0x1d);
}

// Supervisor's game mode switch creates and destroys the title screen.
void harness_w3d_title(i32 create)
{
    if (create)
    {
        TitleInf::create();
    }
    else
    {
        TitleInf::destroy();
    }
}

// Some code in the original takes the address of g_unk_4d9d90, so stores to
// it stay in order with stores through pointers (TitleInf::thread_start).
i32 *harness_w3d_unk_4d9d90()
{
    return &g_unk_4d9d90;
}

// The title menu (0x44b621).
i32 harness_w3d_scorefile()
{
    return g_Scorefile->any_cleared();
}

// GameThread creates the player (0x42ce15) and deletes it in its
// destructor (0x42d439).
void harness_w3d_player(i32 create)
{
    if (create)
    {
        Player::create();
    }
    else if (g_Player != NULL)
    {
        delete g_Player;
    }
}

// Player::initialize (0x441025) takes the cached .sht files back.
ShtFile *harness_w3d_cached_sht()
{
    return (ShtFile *)((u8 *)g_cached_sht_file + (u32)g_cached_sht_file_subseason);
}

// GameThread's stage setup (0x42dcf3, 0x42df0b).
void harness_w3d_player_reset()
{
    g_Player->reset();
}

// Bullets, enemies and lasers graze the player (0x412651, 0x4336f1, ...).
void harness_w3d_graze(Float3 *pos)
{
    g_Player->do_graze(pos);
}

// Enemies and lasers (0x41c99b, 0x433615, ...) test rotated rectangles.
i32 harness_w3d_rotated_rect(Float3 *pos, f32 angle, f32 width, f32 length, i32 graze_only)
{
    return g_Player->check_hit_rotated_rect(pos, angle, width, length, graze_only) +
           g_Player->check_hit_rotated_rect(pos, width, angle, length * 2.0f, 0);
}
