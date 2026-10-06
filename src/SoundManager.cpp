#include <dsound.h>

#include <string.h>

#include "FileSystem.h"
#include "GameErrorContext.h"
#include "SoundManager.h"
#include "Supervisor.h"

// GLOBAL: TH16 0x4a2a30
SoundEffectData g_sound_effect_table[SOUND_EFFECT_COUNT] = {
    {0, 0, -1900, 0, 0, 0}, {1, 0, -2100, 0, 0, 0}, {3, 1, -1200, 5, 0, 1}, {4, 1, -1500, 5, 0, 1},
    {2, 2, -1100, 100, 0, 1}, {28, 3, -700, 100, 0, 1}, {29, 4, -700, 100, 0, 1}, {21, 5, -1900, 50, 0, 1},
    {22, 6, -2200, 50, 0, 1}, {23, 7, -2400, 50, 0, 1}, {7, 8, -500, 100, 0, 0}, {9, 9, -400, 100, 0, 0},
    {10, 10, -800, 10, 0, 0}, {32, 11, -1500, 10, 0, 1}, {33, 12, -300, 100, 0, 1}, {27, 5, -1100, 50, 0, 1},
    {18, 13, -1300, 50, 0, 1}, {19, 14, -1400, 50, 0, 1}, {5, 15, -900, 100, 0, 1}, {34, 16, -880, 0, 0, 1},
    {36, 39, -880, 0, 0, 1}, {37, 17, -1500, 0, 0, 1}, {24, 5, -300, 20, 0, 1}, {25, 6, -1800, 20, 0, 1},
    {26, 7, -1800, 20, 0, 1}, {38, 18, -1100, 50, 0, 1}, {39, 19, -1300, 50, 0, 1}, {40, 20, -1500, 50, 0, 1},
    {11, 21, -500, 100, 0, 1}, {42, 22, -600, 20, 0, 1}, {43, 22, -700, 20, 0, 1}, {76, 65, 0, 20, 0, 1},
    {13, 23, -100, 90, 0, 1}, {41, 18, -500, 50, 0, 1}, {14, 24, -800, 100, 0, 1}, {15, 25, -800, 100, 0, 1},
    {35, 26, -500, 0, 0, 1}, {12, 27, -300, 100, 0, 1}, {16, 28, 0, 100, 0, 1}, {44, 29, 0, 100, 0, 1},
    {45, 29, -600, 100, 0, 1}, {8, 8, -300, 100, 0, 0}, {30, 30, -300, 100, 0, 1}, {31, 31, -300, 100, 0, 1},
    {17, 32, -100, 100, 0, 1}, {46, 33, 0, 100, 0, 1}, {49, 34, -200, 100, 0, 1}, {47, 35, 0, 100, 0, 1},
    {48, 36, 0, 100, 0, 1}, {6, 37, -500, 100, 0, 1}, {20, 38, -3000, 0, 1, 1}, {50, 40, -500, 0, 0, 1},
    {51, 41, 0, 0, 0, 1}, {52, 42, 0, 0, 0, 1}, {53, 42, -300, 0, 0, 1}, {54, 43, -300, 100, 0, 1},
    {55, 44, -1500, 0, 1, 1}, {56, 45, 0, 100, 0, 1}, {57, 46, -200, 100, 0, 1}, {58, 47, -200, 100, 0, 1},
    {59, 48, -500, 100, 0, 1}, {60, 49, -500, 100, 0, 1}, {61, 50, -500, 100, 0, 1}, {62, 51, 0, 100, 0, 1},
    {63, 52, 0, 100, 0, 1}, {64, 53, -900, 5, 0, 1}, {65, 54, -900, 5, 0, 1}, {66, 55, 0, 100, 0, 1},
    {67, 56, 0, 100, 0, 1}, {68, 57, -2500, 5, 0, 1}, {69, 58, -500, 100, 0, 1}, {70, 59, 0, 100, 0, 1},
    {71, 60, -400, 0, 0, 1}, {72, 61, -500, 0, 0, 1}, {73, 62, -500, 0, 0, 1}, {74, 63, 0, 100, 0, 1},
    {75, 64, 0, 100, 0, 1}, {77, 66, 0, 100, 0, 1},
};

// GLOBAL: TH16 0x491a00
const char *const g_sound_file_names[SOUND_FILE_COUNT] = {
    "se_plst00.wav", "se_enep00.wav", "se_pldead00.wav", "se_power0.wav",
    "se_power1.wav", "se_tan00.wav", "se_tan01.wav", "se_tan02.wav",
    "se_ok00.wav", "se_cancel00.wav", "se_select00.wav", "se_gun00.wav",
    "se_cat00.wav", "se_lazer00.wav", "se_lazer01.wav", "se_enep01.wav",
    "se_damage00.wav", "se_item00.wav", "se_kira00.wav", "se_kira01.wav",
    "se_kira02.wav", "se_timeout.wav", "se_graze.wav", "se_powerup.wav",
    "se_pause.wav", "se_cardget.wav", "se_damage01.wav", "se_timeout2.wav",
    "se_invalid.wav", "se_slash.wav", "se_ch00.wav", "se_ch01.wav",
    "se_extend.wav", "se_cardget.wav", "se_nep00.wav", "se_bonus.wav",
    "se_bonus2.wav", "se_enep02.wav", "se_lazer02.wav", "se_nodamage.wav",
    "se_boon00.wav", "se_don00.wav", "se_boon01.wav", "se_ch02.wav",
    "se_ch03.wav", "se_extend2.wav", "se_pin00.wav", "se_pin01.wav",
    "se_lgods1.wav", "se_lgods2.wav", "se_lgods3.wav", "se_lgods4.wav",
    "se_lgodsget.wav", "se_msl.wav", "se_msl2.wav", "se_pldead01.wav",
    "se_heal.wav", "se_msl3.wav", "se_fault.wav", "se_noise.wav",
    "se_etbreak.wav", "se_tan03.wav", "se_wolf.wav", "se_bonus4.wav",
    "se_big.wav", "se_item01.wav", "se_release.wav",
};

// FUNCTION: TH16 0x43dcf0
void SoundManager::tick_bgm_fade()
{
    BgmStream *bgm = g_SoundManager.bgm_stream;
    if (bgm == NULL)
    {
        return;
    }
    if (bgm->fade_mode == 1)
    {
        bgm->fade_time_left--;
        if (bgm->fade_time_left <= 0)
        {
            bgm->fade_mode = 0;
            bgm->buffers[0]->Stop();
        }
        else
        {
            bgm->set_volume(bgm->fade_time_left * 5000 / bgm->fade_duration - 5000);
        }
    }
    bgm = g_SoundManager.bgm_stream;
    if (bgm->fade_mode == 2)
    {
        bgm->fade_time_left--;
        if (bgm->fade_time_left <= 0)
        {
            bgm->fade_mode = 0;
        }
        else
        {
            bgm->set_volume(bgm->fade_time_left * -5000 / bgm->fade_duration);
        }
    }
    bgm = g_SoundManager.bgm_stream;
    if (bgm->fade_mode == 4)
    {
        bgm->fade_time_left--;
        if (bgm->fade_time_left <= 0)
        {
            bgm->fade_mode = 0;
        }
        else
        {
            bgm->set_volume(bgm->fade_time_left * 1000 / bgm->fade_duration - 1000);
        }
    }
    bgm = g_SoundManager.bgm_stream;
    if (bgm->fade_mode == 3)
    {
        bgm->fade_time_left--;
        if (bgm->fade_time_left <= 0)
        {
            bgm->fade_mode = 0;
        }
        else
        {
            bgm->set_volume(bgm->fade_time_left * -1000 / bgm->fade_duration);
        }
    }
}

// FUNCTION: TH16 0x45d420
i32 SoundManager::stop_threads()
{
    if (g_SoundManager.init_thread == NULL)
    {
        return 0;
    }
    if (g_SoundManager.thread_state == SOUND_THREAD_RUNNING)
    {
        g_SoundManager.thread_state = SOUND_THREAD_DONE;
    }
    while (WaitForSingleObject(g_SoundManager.init_thread, 100) == WAIT_TIMEOUT)
    {
        Sleep(1);
    }
    while (WaitForSingleObject(g_SoundManager.load_thread, 100) == WAIT_TIMEOUT)
    {
        Sleep(1);
    }
    CloseHandle(g_SoundManager.init_thread);
    CloseHandle(g_SoundManager.load_thread);
    g_SoundManager.init_thread = NULL;
    g_SoundManager.load_thread = NULL;
    return 0;
}

// FUNCTION: TH16 0x45d4d0
void SoundManager::thread_init(void *arg)
{
    g_SoundManager.initialize(g_SoundManager.window);
    while (g_SoundManager.thread_state == SOUND_THREAD_RUNNING)
    {
        Sleep(1);
    }
    g_SoundManager.init_done = 1;
}

// FUNCTION: TH16 0x45d7e0
void SoundManager::thread_load_sound_files(void *arg)
{
    for (i32 i = 0; i < SOUND_FILE_COUNT; i++)
    {
        if (g_SoundManager.thread_state == SOUND_THREAD_QUIT)
        {
            return;
        }
        g_SoundManager.sound_file_data[i] = file_read_all(g_sound_file_names[i], NULL, 0);
        if (g_SoundManager.sound_file_data[i] == NULL)
        {
            g_GameErrorContext.log("error : Sound \x83t\x83@\x83" "C\x83\x8b\x82\xaa\x93\xc7\x82\xdd\x8d\x9e\x82\xdf\x82\xc8\x82\xa2 \x83" "f\x81[\x83^\x82\xf0\x8am\x94" "F %s\r\n", g_sound_file_names[i]);
            return;
        }
    }
    while (g_SoundManager.thread_state == SOUND_THREAD_RUNNING)
    {
        Sleep(1);
    }
}

// FUNCTION: TH16 0x45d9d0
HARNESS_CALLED WAVEFORMATEX *__stdcall get_wav_chunk(u8 *data, const char *tag, i32 *chunk_size, u32 size)
{
    while (size != 0)
    {
        *chunk_size = *(i32 *)(data + 4);
        if (strncmp((char *)data, tag, 4) == 0)
        {
            return (WAVEFORMATEX *)(data + 8);
        }
        size -= *chunk_size + 8;
        data += *chunk_size + 8;
    }
    return NULL;
}

// FUNCTION: TH16 0x45da30
i32 SoundManager::find_bgm(const char *path)
{
    char name[128];
    i32 i = 0;
    const char *slash = strrchr(path, '/');
    if (slash == NULL)
    {
        slash = strrchr(path, '\\');
    }
    if (slash == NULL)
    {
        strcpy(name, path);
    }
    else
    {
        strcpy(name, slash + 1);
    }
    for (; bgm_format[i].name[0] != '\0'; i++)
    {
        if (strcmp(bgm_format[i].name, name) == 0)
        {
            break;
        }
    }
    return bgm_format[i].name[0] != '\0' ? i : 0;
}

// FUNCTION: TH16 0x45dbf0
i32 SoundManager::select_bgm(const char *path)
{
    if (g_SoundManager.bgm_stream == NULL)
    {
        return -1;
    }
    i32 i = g_SoundManager.find_bgm(path);
    g_SoundManager.bgm_stream->wave_file->open_bgm(&g_SoundManager.bgm_format[i], 0);
    strcpy(g_SoundManager.bgm_name, path);
    return 0;
}

// FUNCTION: TH16 0x45df60
void SoundManager::stop_bgm()
{
    if (bgm_stream == NULL)
    {
        return;
    }
    bgm_stream->stop(1);
    if (bgm_thread != NULL)
    {
        PostThreadMessageA(bgm_thread_id, WM_QUIT, 0, 0);
        while (WaitForSingleObject(bgm_thread, 0x100) != WAIT_OBJECT_0)
        {
            PostThreadMessageA(bgm_thread_id, WM_QUIT, 0, 0);
        }
        CloseHandle(bgm_thread);
        CloseHandle(bgm_event);
        bgm_thread = NULL;
    }
    if (bgm_stream != NULL)
    {
        bgm_stream->destroy();
        bgm_stream = NULL;
    }
}

// TODO: the inlined stop_threads keeps Sleep in ebx; the original calls it through the import each time.
// FUNCTION: TH16 0x45e000
i32 SoundManager::reset()
{
    for (i32 i = 0; i < SOUND_QUEUE_SIZE; i++)
    {
        queued_ids[i] = -1;
    }
    stop_threads();
    if (manager == NULL)
    {
        return -1;
    }
    if (dsound == NULL)
    {
        return 0;
    }
    bgm_volume = g_Supervisor.config.bgm_volume;
    se_volume = g_Supervisor.config.se_volume;
    if (se_volume != 0)
    {
        f32 x = bgm_volume / 100.0f;
        f32 t = (1.0f - x) * (1.0f - x);
        f32 u = 1.0f - t * t;
        bgm_db = -5000 - (i32)(u * -5000.0f);
    }
    else
    {
        bgm_db = -10000;
    }
    return 0;
}

// TODO: the original stores the new slot's pan before its count, keeping i in eax.
// FUNCTION: TH16 0x45e150
HARNESS_CALLED void SoundManager::play_sound_centered(i32 id, i32 unused)
{
    i32 unk = g_sound_effect_table[id].unk_a;
    i32 i;
    for (i = 0; i < SOUND_QUEUE_SIZE; i++)
    {
        if (g_SoundManager.queued_ids[i] < 0)
        {
            break;
        }
        if (g_SoundManager.queued_ids[i] == id)
        {
            if (g_SoundManager.queued_counts[i] < 60 && g_SoundManager.queued_counts[i] >= 0)
            {
                g_SoundManager.queued_pans[i][g_SoundManager.queued_counts[i]] = 0;
                g_SoundManager.queued_counts[i]++;
            }
            return;
        }
    }
    if (i >= SOUND_QUEUE_SIZE)
    {
        return;
    }
    g_SoundManager.queued_ids[i] = id;
    g_SoundManager.queued_counts[i] = 1;
    g_SoundManager.queued_pans[i][0] = 0;
    g_SoundManager.sound_buffers[id].unk_4 = unk;
}

// TODO: the original keeps i in ecx and copies it for the pan index instead of precomputing i * 4.
// FUNCTION: TH16 0x45e1f0
HARNESS_CALLED void SoundManager::play_sound_at_position(i32 id, f32 x)
{
    i32 pan = x * 1000.0f / 192.0f;
    i32 unk = g_sound_effect_table[id].unk_a;
    i32 i;
    for (i = 0; i < SOUND_QUEUE_SIZE; i++)
    {
        if (g_SoundManager.queued_ids[i] < 0)
        {
            break;
        }
        if (g_SoundManager.queued_ids[i] == id)
        {
            if (g_SoundManager.queued_counts[i] < 60 && g_SoundManager.queued_counts[i] >= 0)
            {
                g_SoundManager.queued_pans[i][g_SoundManager.queued_counts[i]] = pan;
                g_SoundManager.queued_counts[i]++;
            }
            return;
        }
    }
    if (i >= SOUND_QUEUE_SIZE)
    {
        return;
    }
    g_SoundManager.queued_ids[i] = id;
    g_SoundManager.queued_pans[i][0] = pan;
    g_SoundManager.queued_counts[i] = 1;
    g_SoundManager.sound_buffers[id].unk_4 = unk;
}

// FUNCTION: TH16 0x45e2a0
void SoundManager::stop_sound(i32 id)
{
    if (id < 0)
    {
        for (i32 i = 0; i < 0x4e; i++)
        {
            g_SoundManager.sound_buffers[i].unk_14 = 0;
            if (g_SoundManager.sound_buffers[i].buffer != NULL)
            {
                DWORD status;
                g_SoundManager.sound_buffers[i].buffer->GetStatus(&status);
                g_SoundManager.sound_buffers[i].unk_14 = status & DSBSTATUS_PLAYING;
                g_SoundManager.sound_buffers[i].buffer->Stop();
            }
        }
        return;
    }
    i32 i;
    for (i = 0; i < SOUND_QUEUE_SIZE; i++)
    {
        if (g_SoundManager.queued_ids[i] < 0)
        {
            break;
        }
        if (g_SoundManager.queued_ids[i] == id)
        {
            g_SoundManager.queued_counts[i] = -1;
            return;
        }
    }
    if (i >= SOUND_QUEUE_SIZE)
    {
        return;
    }
    g_SoundManager.queued_ids[i] = id;
    g_SoundManager.queued_counts[i] = -1;
}
