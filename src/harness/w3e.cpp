// Stand-in callers for wave 3 range E (0x44f710-0x4630f0) functions whose
// shape depends on how the rest of the game calls them.
#include "../AnmManager.h"
#include "../GameWindow.h"
#include "../MainMenu.h"
#include "../Supervisor.h"
#include "../SoundManager.h"
#include "../TextHelper.h"
#include "../Thread.h"

// Like the BGM code of update_sound_thread (0x45e3de, 0x45e410). The real
// caller of open_bgm is LoadingThread::thread_start (0x43aefa).
i32 harness_w3e_bgm(i32 slot, const char *name, SoundManager *other)
{
    // open_bgm uses this; a second object keeps LTCG from folding it.
    other->open_bgm("thbgm.dat");
    return g_SoundManager.preload_bgm(slot, name) + g_SoundManager.preload_bgm(slot + 1, "x") +
           g_SoundManager.play_preloaded_bgm(slot) + g_SoundManager.play_preloaded_bgm(2);
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

HARNESS_CALLED i32 check_startup_shortcut();

void w3e_opaque_double(double *value);

