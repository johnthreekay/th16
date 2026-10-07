// Stand-in callers for wave 5 range D (0x44c000-0x458000, the title screen
// menus) functions whose shape depends on code that is not decompiled yet.
#include "../StageData.h"
#include "../Supervisor.h"

// GameThread starts the stage's two themes (0x42d0e3, 0x42d0f2), the
// second with a nonzero first argument.
void harness_w5d_play_bgm_wav()
{
    g_Supervisor.play_bgm_wav(0, g_stage_data->music_names[0]);
    g_Supervisor.play_bgm_wav(1, g_stage_data->music_names[1]);
}
