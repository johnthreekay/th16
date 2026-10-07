// Stand-in callers for unit 8a (0x45d420-0x469000).
#include "../AnmManager.h"
#include "../AnmVm.h"
#include "../SoundManager.h"

// Like SoundManager's wave loader (0x45e990), which looks up "fmt " and
// "data".
WAVEFORMATEX harness_sound_get_wav_chunk(u8 *data, u32 size)
{
    i32 chunk_size;
    WAVEFORMATEX *fmt = get_wav_chunk(data, "fmt ", &chunk_size, size - 12);
    if (get_wav_chunk(data, "data", &chunk_size, size) == NULL)
    {
        return *fmt;
    }
    return *fmt;
}

// Like the BGM commands of update_sound_thread (0x45e330).
i32 harness_sound_select_bgm(const char *path)
{
    return g_SoundManager.select_bgm(path);
}

// Like the sound update and pause code (0x45e330, 0x43e5f0).
void harness_sound_misc(i32 id)
{
    g_SoundManager.stop_bgm();
    g_SoundManager.reset();
    g_SoundManager.stop_sound(id);
    g_SoundManager.stop_sound(-1);
}

// Like Supervisor's per-frame drawing setup, which resets the batches.
void harness_anm_reset_vertex_buffers()
{
    g_AnmManager->reset_vertex_buffers();
}

