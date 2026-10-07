// Stand-in callers for wave 3 range E (0x44f710-0x4630f0) functions whose
// shape depends on how the rest of the game calls them.
#include "../AnmManager.h"
#include "../GameWindow.h"
#include "../Supervisor.h"
#include "../SoundManager.h"
#include "../TextHelper.h"
#include "../Thread.h"

// Like WinMain and the Window frame functions around a device reset.
void harness_w3e_device_reset()
{
    g_AnmManager->release_textures();
    g_AnmManager->create_d3d_textures_for_loaded_anms();
    g_AnmManager->release_textures();
    g_AnmManager->create_d3d_textures_for_loaded_anms();
}


// Like Supervisor::teardown_everything (0x43b6e5).
void harness_w3e_teardown_text()
{
    g_TextHelper.release_buffer();
}

// Like the text VM drawing at 0x46da88 and draw_rtext (0x46dbf9, 0x46dd60).
void harness_w3e_draw_text(RECT *rect, i32 x, i32 size, D3DCOLOR color, D3DCOLOR shadow, const char *text,
                           IDirect3DTexture9 *texture, i32 font, i32 spacing)
{
    draw_text(rect, x, size, color, shadow, text, texture, font, spacing, 1);
    draw_text(rect, x * 2, size, color, shadow, text, texture, font, 0, 0);
    draw_text(rect + 1, x, size + 1, shadow, color, text, texture, font + 1, spacing * 2, spacing);
}

// Like LoadingThread::thread_start (0x43aefa), and the BGM code of the
// stages and menus.
i32 harness_w3e_bgm(i32 slot, const char *name, SoundManager *other)
{
    // open_bgm uses this; a second object keeps LTCG from folding it.
    g_SoundManager.open_bgm("thbgm.dat");
    other->open_bgm("thbgm.dat");
    return g_SoundManager.preload_bgm(slot, name) + g_SoundManager.preload_bgm(slot + 1, "x") +
           g_SoundManager.play_preloaded_bgm(slot) + g_SoundManager.play_preloaded_bgm(2);
}

// Like WinMain's frame loop (around 0x459f79). With every caller of the
// frame functions known, LTCG lays out their frames (and those of what they
// call) for the stack alignment it can prove instead of realigning them.
// The second object keeps LTCG from folding this, which WinMain passes in
// ecx.
i32 harness_w3e_frame_loop(GameWindow *other)
{
    if (g_GameWindow.flags & 0x40)
    {
        return g_GameWindow.do_frame_sleeping() + other->do_frame_sleeping();
    }
    if (g_Supervisor.config.frame_skip == 0)
    {
        return g_GameWindow.do_frame() + other->do_frame();
    }
    return g_GameWindow.do_frame_frameskip() + other->do_frame_frameskip();
}

// The globals from g_unk_4d9d20 to g_unk_4d9d90 are fields of g_GameWindow,
// whose address the Window methods take. Stores through pointers can
// therefore change them, and code reloads them after such stores; taking
// their addresses here tells LTCG the same.
void *harness_w3e_window_field_addresses(i32 i)
{
    void *fields[] = {&g_resolution_x, &g_resolution_y, &g_screen_coord_scale};
    return fields[i];
}

// Like WinMain's shutdown (0x459902).
void harness_w3e_sound_release()
{
    g_SoundManager.release();
}

HARNESS_CALLED i32 check_startup_shortcut();

void w3e_opaque_double(double *value);

// Like WinMain (0x459aa1), whose frame is realigned to 8 bytes: LTCG then
// knows the stack is aligned in check_startup_shortcut and resolve_shortcut.
i32 harness_w3e_startup()
{
    double aligned;
    w3e_opaque_double(&aligned);
    return check_startup_shortcut();
}

unsigned replay_list_thread(void *arg);

// Like the replay menu (0x451844), which starts the thread.
void harness_w3e_replay_thread(ThreadInf *thread, void *arg)
{
    thread->restart((ThreadStart)replay_list_thread, arg);
}
