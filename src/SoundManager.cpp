#include <dsound.h>

#include <string.h>

#include "FileSystem.h"
#include "GameErrorContext.h"
#include "SoundManager.h"

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
