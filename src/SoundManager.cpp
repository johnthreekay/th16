#include <dsound.h>

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "CriticalSections.h"
#include "FileSystem.h"
#include "GameErrorContext.h"
#include "SoundManager.h"
#include "Supervisor.h"

// GLOBAL: TH16 0x4d9e10
SoundManager g_SoundManager;
// SYNTHETIC: TH16 0x4011b0
// ??__Eg_SoundManager@@YAXXZ

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

static_assert(offsetof(SoundManager, bgm_format) == 0x1980, "SoundManager layout");
static_assert(offsetof(SoundManager, selected_bgm_name) == 0x1984, "SoundManager layout");
static_assert(offsetof(SoundManager, sound_buffers) == 0x1a84, "SoundManager layout");
static_assert(offsetof(SoundManager, bgm_name) == 0x22e0, "SoundManager layout");
static_assert(offsetof(SoundManager, bgm_commands) == 0x23e0, "SoundManager layout");
static_assert(offsetof(SoundManager, bgm_stream) == 0x5660, "SoundManager layout");
static_assert(offsetof(SoundManager, init_thread) == 0x5674, "SoundManager layout");
static_assert(offsetof(SoundManager, preload_format) == 0x187c, "SoundManager layout");
static_assert(offsetof(SoundManager, preload_current) == 0x197c, "SoundManager layout");
static_assert(offsetof(SoundManager, preload_names) == 0x4560, "SoundManager layout");
static_assert(offsetof(SoundManager, bgm_dat_name) == 0x5560, "SoundManager layout");
static_assert(sizeof(SoundManager) == 0x5698, "SoundManager layout");
static_assert(offsetof(BgmStream, refilling) == 0x9c, "BgmStream layout");
static_assert(sizeof(ThBgmFormat) == 0x34, "ThBgmFormat layout");
static_assert(sizeof(SoundEffectData) == 0x14, "SoundEffectData layout");

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

// FUNCTION: TH16 0x42e4d0
void SoundManager::pause_sounds()
{
    g_SoundManager.queued_ids[0] = -1;
    for (i32 i = 0; i < 78; i++)
    {
        SoundBufferEntry *sound = &g_SoundManager.sound_buffers[i];
        sound->was_playing = 0;
        if (sound->buffer != NULL)
        {
            DWORD status;
            sound->buffer->GetStatus(&status);
            sound->was_playing = status & DSBSTATUS_PLAYING;
            sound->buffer->Stop();
        }
    }
}

// FUNCTION: TH16 0x440bd0
void SoundManager::resume_sounds()
{
    for (i32 i = 0; i < 78; i++)
    {
        SoundBufferEntry *sound = &g_SoundManager.sound_buffers[i];
        if (sound->buffer != NULL && sound->was_playing)
        {
            sound->buffer->Play(0, 0, sound->data->play_flags);
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

inline HRESULT CSoundManager::Initialize(HWND hWnd, DWORD dwCoopLevel, DWORD dwPrimaryChannels, DWORD dwPrimaryFreq,
                                         DWORD dwPrimaryBitRate)
{
    if (m_pDS != NULL)
    {
        m_pDS->Release();
        m_pDS = NULL;
    }
    HRESULT hr;
    if (FAILED(hr = DirectSoundCreate8(NULL, &m_pDS, NULL)))
    {
        return hr;
    }
    if (FAILED(hr = m_pDS->SetCooperativeLevel(hWnd, dwCoopLevel)))
    {
        return hr;
    }
    SetPrimaryBufferFormat(dwPrimaryChannels, dwPrimaryFreq, dwPrimaryBitRate);
    return S_OK;
}

inline CSoundManager::~CSoundManager()
{
    if (m_pDS != NULL)
    {
        m_pDS->Release();
        m_pDS = NULL;
    }
}

// TODO: code identical; the loop's end pointer is g_anchor_corners_x+4 in the original (0x4a304c), which reccmp cannot name: our data layout puts the anchor tables before g_sound_effect_table even when they are defined in this file.
// FUNCTION: TH16 0x45d510
i32 SoundManager::initialize(HWND window)
{
    DSBUFFERDESC desc;
    WAVEFORMATEX format;
    void *p1;
    DWORD n1;
    void *p2;
    DWORD n2;

    for (i32 i = 0; i < SOUND_EFFECT_COUNT; i++)
    {
        sound_buffers[i].unk_4 = -1;
        SoundEffectData *data;
        for (data = g_sound_effect_table; data != NULL; data++)
        {
            if (data->id == i)
            {
                break;
            }
        }
        sound_buffers[i].id = i;
        sound_buffers[i].data = data;
    }
    for (i32 i = 0; i < SOUND_QUEUE_SIZE; i++)
    {
        queued_ids[i] = -1;
    }
    manager = new CSoundManager;
    if (manager->Initialize(window, DSSCL_PRIORITY, 2, 44100, 16) < 0)
    {
        g_GameErrorContext.log("DirectSound \x83I\x83u\x83W\x83" "F\x83N\x83g\x82\xcc\x8f\x89\x8a\xfa\x89\xbb\x82\xaa\x8e\xb8\x94s\x82\xb5\x82\xbd\x82\xe6\r\n");
        if (manager != NULL)
        {
            delete manager;
            manager = NULL;
        }
        return -1;
    }
    dsound = manager->m_pDS;
    bgm_thread = NULL;
    memset(&desc, 0, sizeof(DSBUFFERDESC));
    desc.dwSize = sizeof(DSBUFFERDESC);
    desc.dwFlags = DSBCAPS_LOCSOFTWARE | DSBCAPS_GLOBALFOCUS;
    desc.dwBufferBytes = 0x8000;
    memset(&format, 0, sizeof(WAVEFORMATEX));
    format.cbSize = 0;
    format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = 2;
    format.nSamplesPerSec = 44100;
    format.nAvgBytesPerSec = 176400;
    format.nBlockAlign = 4;
    format.wBitsPerSample = 16;
    desc.lpwfxFormat = &format;
    if (dsound->CreateSoundBuffer(&desc, &init_sound_buffer, NULL) < 0)
    {
        return -1;
    }
    if (init_sound_buffer->Lock(0, 0x8000, &p1, &n1, &p2, &n2, 0) < 0)
    {
        return -1;
    }
    memset(p1, 0, 0x8000);
    init_sound_buffer->Unlock(p1, n1, p2, n2);
    init_sound_buffer->Play(0, 0, DSBPLAY_LOOPING);
    bgm_volume = 100;
    se_volume = 100;
    SetTimer(window, 0, 250, NULL);
    game_window = window;
    for (i32 i = 0; i < SOUND_EFFECT_COUNT; i++)
    {
        if (g_SoundManager.thread_state == SOUND_THREAD_QUIT)
        {
            return -1;
        }
        if (g_SoundManager.sound_buffers[i].load(g_sound_file_names[g_sound_effect_table[i].file_index]) != 0)
        {
            g_GameErrorContext.log("error : Sound \x83t\x83@\x83" "C\x83\x8b\x82\xaa\x93\xc7\x82\xdd\x8d\x9e\x82\xdf\x82\xc8\x82\xa2 \x83" "f\x81[\x83^\x82\xf0\x8am\x94" "F %s\r\n", g_sound_file_names[g_sound_effect_table[i].file_index]);
            return -1;
        }
    }
    g_GameErrorContext.log("DirectSound \x82\xcd\x90\xb3\x8f\xed\x82\xc9\x8f\x89\x8a\xfa\x89\xbb\x82\xb3\x82\xea\x82\xdc\x82\xb5\x82\xbd\r\n");
    return 0;
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

// FUNCTION: TH16 0x45d850
HARNESS_CALLED i32 SoundManager::release()
{
    if (bgm_format != NULL)
    {
        free(bgm_format);
        bgm_format = NULL;
    }
    for (i32 i = 0; i < SOUND_EFFECT_COUNT; i++)
    {
        if (sound_buffers[i].buffer != NULL)
        {
            sound_buffers[i].buffer->Release();
            sound_buffers[i].buffer = NULL;
        }
    }
    for (i32 i = 0; i < SOUND_FILE_COUNT; i++)
    {
        if (sound_file_data[i] != NULL)
        {
            free(sound_file_data[i]);
            sound_file_data[i] = NULL;
        }
    }
    if (manager == NULL)
    {
        return 0;
    }
    KillTimer(game_window, 1);
    stop_bgm();
    dsound = NULL;
    init_sound_buffer->Stop();
    if (init_sound_buffer != NULL)
    {
        init_sound_buffer->Release();
        init_sound_buffer = NULL;
    }
    if (bgm_stream != NULL)
    {
        bgm_stream->destroy();
        bgm_stream = NULL;
    }
    if (manager != NULL)
    {
        delete manager;
        manager = NULL;
    }
    for (i32 i = 0; i < 0x10; i++)
    {
        if (preload_data[i] != NULL)
        {
            free(preload_data[i]);
            preload_data[i] = NULL;
        }
    }
    return 0;
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

// FUNCTION: TH16 0x45db10
HARNESS_CALLED i32 SoundManager::open_bgm(const char *path)
{
    strcpy(bgm_dat_name, path);
    if (manager == NULL)
    {
        return -1;
    }
    if (dsound == NULL)
    {
        return -1;
    }
    stop_bgm();
    ThBgmFormat *format = bgm_format;
    DWORD block_align = format->format.nBlockAlign;
    i32 samples_per_sec = format->format.nSamplesPerSec;
    DWORD notify_size = samples_per_sec * block_align * 4 / 8;
    bgm_event = CreateEventA(NULL, FALSE, FALSE, NULL);
    bgm_thread = CreateThread(NULL, 0, bgm_thread_proc, g_Supervisor.main_window, 0, &bgm_thread_id);
    notify_size -= notify_size % block_align;
    if (FAILED(manager->CreateStreaming((CStreamingSound **)&bgm_stream, "thbgm.dat", 0, GUID_NULL, 8, notify_size,
                                        bgm_event, format)))
    {
        return -1;
    }
    return 0;
}

// FUNCTION: TH16 0x45dbf0
HARNESS_CALLED i32 SoundManager::select_bgm(const char *path)
{
    if (g_SoundManager.bgm_stream == NULL)
    {
        return -1;
    }
    i32 i = g_SoundManager.find_bgm(path);
    g_SoundManager.bgm_stream->wave_file->open_bgm(&g_SoundManager.bgm_format[i], 0);
    strcpy(g_SoundManager.selected_bgm_name, path);
    return 0;
}

// TODO: ours keeps slot * 4 in edi (and shifts esi in place); the original indexes with [esi*4] throughout.
// FUNCTION: TH16 0x45dc50
HARNESS_CALLED i32 SoundManager::preload_bgm(i32 slot, const char *name)
{
    if (g_SoundManager.preload_data[slot] != NULL && strcmp(name, g_SoundManager.preload_names[slot]) == 0)
    {
        return 0;
    }
    strcpy(g_SoundManager.preload_names[slot], name);
    if (!(g_Supervisor.config.flags & CONFIG_BGM_IN_MEMORY))
    {
        return 0;
    }
    if (g_SoundManager.manager == NULL)
    {
        return 0;
    }
    if (g_SoundManager.preload_data[slot] != NULL)
    {
        free(g_SoundManager.preload_data[slot]);
        g_SoundManager.preload_data[slot] = NULL;
    }
    ENTER_CS(CS_FILE);
    HANDLE file = CreateFileA(g_SoundManager.bgm_dat_name, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (file == INVALID_HANDLE_VALUE)
    {
        sound_debug_log("error : bgmfile is not find %s\r\n", g_SoundManager.bgm_dat_name);
        LEAVE_CS(CS_FILE);
        return -1;
    }
    i32 track = g_SoundManager.find_bgm(name);
    SetFilePointer(file, g_SoundManager.bgm_format[track].start_offset, NULL, FILE_BEGIN);
    u8 *data = (u8 *)malloc(g_SoundManager.bgm_format[track].unk_14);
    if (data == NULL)
    {
        CloseHandle(file);
        sound_debug_log("error : bgmfile is not find %s\r\n", g_SoundManager.bgm_dat_name);
        g_CriticalSections.leave(CS_FILE);
        return -1;
    }
    DWORD read;
    ReadFile(file, data, g_SoundManager.bgm_format[track].unk_14, &read, NULL);
    CloseHandle(file);
    g_CriticalSections.leave(CS_FILE);
    g_SoundManager.preload_format[slot] = &g_SoundManager.bgm_format[track];
    g_SoundManager.preload_data[slot] = data;
    g_SoundManager.preload_cursor[slot] = data;
    g_SoundManager.preload_size[slot] = g_SoundManager.preload_format[slot]->unk_14;
    return 0;
}

// FUNCTION: TH16 0x45de30
HARNESS_CALLED i32 SoundManager::play_preloaded_bgm(i32 slot)
{
    if (g_SoundManager.manager == NULL)
    {
        return -1;
    }
    if (!g_Supervisor.config.bgm_mode)
    {
        return -1;
    }
    if (g_SoundManager.dsound == NULL)
    {
        return -1;
    }
    if (!(g_Supervisor.config.flags & CONFIG_BGM_IN_MEMORY))
    {
        return g_SoundManager.select_bgm(g_SoundManager.preload_names[slot]);
    }
    if (g_SoundManager.preload_data[slot] == NULL)
    {
        return -1;
    }
    strcpy(g_SoundManager.selected_bgm_name, g_SoundManager.preload_names[slot]);
    ThBgmFormat *format = g_SoundManager.preload_format[slot];
    DWORD block_align = format->format.nBlockAlign;
    i32 samples_per_sec = format->format.nSamplesPerSec;
    DWORD notify_size = samples_per_sec * block_align * 4 / 8;
    g_SoundManager.bgm_event = CreateEventA(NULL, FALSE, FALSE, NULL);
    g_SoundManager.bgm_thread =
        CreateThread(NULL, 0, bgm_thread_proc, g_Supervisor.main_window, 0, &g_SoundManager.bgm_thread_id);
    notify_size -= notify_size % block_align;
    if (FAILED(g_SoundManager.manager->CreateStreamingFromMemory(
            (CStreamingSound **)&g_SoundManager.bgm_stream, g_SoundManager.preload_cursor[slot],
            g_SoundManager.preload_size[slot], g_SoundManager.preload_format[slot], 0, GUID_NULL, 8, notify_size,
            g_SoundManager.bgm_event)))
    {
        return -1;
    }
    g_SoundManager.preload_current = slot;
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

// FUNCTION: TH16 0x45e150
HARNESS_CALLED void SoundManager::play_sound_centered(i32 id, i32 unused)
{
    i32 unk = g_sound_effect_table[id].unk_a;
    i32 i;
    for (i = 0; i < SOUND_QUEUE_SIZE; i++)
    {
        if (queued_ids[i] < 0)
        {
            break;
        }
        if (queued_ids[i] == id)
        {
            if (queued_counts[i] < 60 && queued_counts[i] >= 0)
            {
                queued_pans[i][queued_counts[i]] = 0;
                queued_counts[i]++;
            }
            return;
        }
    }
    if (i >= SOUND_QUEUE_SIZE)
    {
        return;
    }
    queued_ids[i] = id;
    queued_pans[i][0] = 0;
    queued_counts[i] = 1;
    sound_buffers[id].unk_4 = unk;
}

// FUNCTION: TH16 0x45e1f0
HARNESS_CALLED void SoundManager::play_sound_at_position(i32 id, f32 x)
{
    i32 pan = x * 1000.0f / 192.0f;
    i32 unk = g_sound_effect_table[id].unk_a;
    i32 i;
    for (i = 0; i < SOUND_QUEUE_SIZE; i++)
    {
        if (queued_ids[i] < 0)
        {
            break;
        }
        if (queued_ids[i] == id)
        {
            if (queued_counts[i] < 60 && queued_counts[i] >= 0)
            {
                queued_pans[i][queued_counts[i]] = pan;
                queued_counts[i]++;
            }
            return;
        }
    }
    if (i >= SOUND_QUEUE_SIZE)
    {
        return;
    }
    queued_ids[i] = id;
    queued_pans[i][0] = pan;
    queued_counts[i] = 1;
    sound_buffers[id].unk_4 = unk;
}

// FUNCTION: TH16 0x45e2a0
void SoundManager::stop_sound(i32 id)
{
    if (id < 0)
    {
        for (i32 i = 0; i < 0x4e; i++)
        {
            g_SoundManager.sound_buffers[i].was_playing = 0;
            if (g_SoundManager.sound_buffers[i].buffer != NULL)
            {
                DWORD status;
                g_SoundManager.sound_buffers[i].buffer->GetStatus(&status);
                g_SoundManager.sound_buffers[i].was_playing = status & DSBSTATUS_PLAYING;
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

// TODO: ours merges the two SetVolume calls into one; the original keeps both.
// FUNCTION: TH16 0x45e8d0
void SoundBufferEntry::play(i32 pan)
{
    if (buffer == NULL)
    {
        return;
    }
    buffer->Stop();
    buffer->SetCurrentPosition(0);
    buffer->SetPan(pan);
    this->pan = pan;
    if (g_SoundManager.se_volume != 0)
    {
        f32 x = g_SoundManager.se_volume / 100.0f;
        f32 t = (1.0f - x) * (1.0f - x) * (1.0f - x);
        f32 u = 1.0f - t;
        buffer->SetVolume((i32)(u * (data->volume + 5000)) - 5000);
    }
    else
    {
        buffer->SetVolume(-10000);
    }
    buffer->Play(0, 0, data->play_flags);
}

// FUNCTION: TH16 0x45ec50
DWORD WINAPI SoundManager::bgm_thread_proc(void *arg)
{
    BOOL done = FALSE;
    MSG msg;
    do
    {
        DWORD result = MsgWaitForMultipleObjects(1, &g_SoundManager.bgm_event, FALSE, INFINITE, QS_ALLEVENTS);
        if (g_SoundManager.bgm_stream == NULL)
        {
            done = TRUE;
        }
        switch (result)
        {
        case WAIT_OBJECT_0:
            if (g_SoundManager.bgm_stream != NULL && g_SoundManager.bgm_stream->unk_50 != 0)
            {
                g_SoundManager.bgm_stream->refilling = 1;
                g_SoundManager.bgm_stream->handle_wave_stream_notification(0);
                g_SoundManager.bgm_stream->refilling = 0;
            }
            break;
        case WAIT_OBJECT_0 + 1:
            while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE))
            {
                if (msg.message == WM_QUIT)
                {
                    done = TRUE;
                }
            }
            break;
        }
    } while (!done);
    return 0;
}

// FUNCTION: TH16 0x45ed00
void SoundManager::modify_bgm(i32 command, i32 arg, const char *name)
{
    ENTER_CS(CS_SOUND);
    for (i32 i = 0; i < 0x1f; i++)
    {
        if (bgm_commands[i].command == 0)
        {
            bgm_commands[i].command = command;
            bgm_commands[i].arg = arg;
            strcpy(bgm_commands[i].name, name);
            bgm_commands[i].unk_8 = 0;
            break;
        }
    }
    LEAVE_CS(CS_SOUND);
}

#define SOUND_FILE_DATA(entry) (g_SoundManager.sound_file_data[(entry)->data->file_index])
#define SAFE_FREE(p) \
    if ((p) != NULL) \
    { \
        free(p); \
        (p) = NULL; \
    }

// TODO: block layout differs (the original falls through into the failure paths and shares one log call).
// FUNCTION: TH16 0x45e990
i32 SoundBufferEntry::load(const char *name)
{
    if (g_SoundManager.manager == NULL)
    {
        return 0;
    }
    if (buffer != NULL)
    {
        buffer->Release();
        buffer = NULL;
    }
    for (i32 i = 0; i < id; i++)
    {
        if (g_SoundManager.sound_buffers[i].data->file_index == data->file_index)
        {
            g_SoundManager.dsound->DuplicateSoundBuffer(g_SoundManager.sound_buffers[i].buffer, &buffer);
            return 0;
        }
    }
    while (SOUND_FILE_DATA(this) == NULL)
    {
        Sleep(10);
        if (g_SoundManager.thread_state == SOUND_THREAD_QUIT)
        {
            return 0;
        }
    }
    u8 *file = SOUND_FILE_DATA(this);
    if (strncmp((char *)file, "RIFF", 4) != 0)
    {
        g_GameErrorContext.log("Wav \x83t\x83@\x83" "C\x83\x8b\x82\xb6\x82\xe1\x82\xc8\x82\xa2 %s\r\n", name);
        SAFE_FREE(SOUND_FILE_DATA(this));
        return -1;
    }
    file += 4;
    i32 riff_size = *(i32 *)file;
    file += 4;
    if (strncmp((char *)file, "WAVE", 4) != 0)
    {
        g_GameErrorContext.log("Wav \x83t\x83@\x83" "C\x83\x8b\x82\xb6\x82\xe1\x82\xc8\x82\xa2? %s\r\n", name);
        SAFE_FREE(SOUND_FILE_DATA(this));
        return -1;
    }
    file += 4;
    i32 chunk_size;
    WAVEFORMATEX *fmt = get_wav_chunk(file, "fmt ", &chunk_size, riff_size - 12);
    if (fmt == NULL)
    {
        g_GameErrorContext.log("Wav \x83t\x83@\x83" "C\x83\x8b\x82\xb6\x82\xe1\x82\xc8\x82\xa2? %s\r\n", name);
        SAFE_FREE(SOUND_FILE_DATA(this));
        return -1;
    }
    WAVEFORMATEX wfx = *fmt;
    u8 *samples = (u8 *)get_wav_chunk(file, "data", &chunk_size, riff_size - 12);
    if (samples == NULL)
    {
        g_GameErrorContext.log("Wav \x83t\x83@\x83" "C\x83\x8b\x82\xb6\x82\xe1\x82\xc8\x82\xa2? %s\r\n", name);
        SAFE_FREE(SOUND_FILE_DATA(this));
        return -1;
    }
    DSBUFFERDESC desc;
    memset(&desc, 0, sizeof(desc));
    desc.dwSize = sizeof(desc);
    desc.dwFlags = DSBCAPS_GLOBALFOCUS | DSBCAPS_CTRLVOLUME | DSBCAPS_CTRLPAN | DSBCAPS_LOCSOFTWARE;
    desc.dwBufferBytes = chunk_size;
    desc.lpwfxFormat = &wfx;
    if (FAILED(g_SoundManager.dsound->CreateSoundBuffer(&desc, &buffer, NULL)))
    {
        SAFE_FREE(SOUND_FILE_DATA(this));
        return -1;
    }
    void *p1;
    DWORD n1;
    void *p2;
    DWORD n2;
    if (FAILED(buffer->Lock(0, chunk_size, &p1, &n1, &p2, &n2, 0)))
    {
        SAFE_FREE(SOUND_FILE_DATA(this));
        return -1;
    }
    memcpy(p1, samples, n1);
    if (n2 != 0)
    {
        memcpy(p2, samples + n1, n2);
    }
    buffer->Unlock(p1, n1, p2, n2);
    SAFE_FREE(SOUND_FILE_DATA(this));
    sound_debug_log("Create Sound Buffer %s\n", name);
    return 0;
}

// FUNCTION: TH16 0x45eda0
void sound_debug_log(const char *fmt, ...)
{
}

// The DirectSound sample's CSound::GetBuffer, inlined.
static inline IDirectSoundBuffer *bgm_buffer(CStreamingSound *sound)
{
    if (sound->m_apDSBuffer == NULL)
    {
        return NULL;
    }
    if (sound->m_dwNumBuffers <= 0)
    {
        return NULL;
    }
    return sound->m_apDSBuffer[0];
}

// bgm_stream as what it is.
#define BGM_STREAM ((CStreamingSound *)g_SoundManager.bgm_stream)

// Runs the first queued BGM command one step further (commands take several
// calls, counted in unk_8) and plays or stops the queued sound effects.
// Returns the BGM command now first in the queue.
// TODO: 44%; the BGM command switch shares fewer tails than the original.
// FUNCTION: TH16 0x45e330
i32 SoundManager::update_sound_thread()
{
    ENTER_CS(CS_SOUND);
    if (g_SoundManager.manager == NULL)
    {
        LEAVE_CS(CS_SOUND);
        return 0;
    }
    BgmCommandEntry *cmd = g_SoundManager.bgm_commands;
    i32 again;
    do
    {
        again = 0;
        switch (cmd->command)
        {
        case 8:
            if (BGM_STREAM != NULL)
            {
                BGM_STREAM->SetVolume(0);
            }
            goto pop;
        case 1:
            if (g_Supervisor.config.flags & CONFIG_BGM_IN_MEMORY)
            {
                if (cmd->unk_8 != 0)
                {
                    goto step;
                }
                g_SoundManager.stop_bgm();
            }
            g_SoundManager.preload_bgm(cmd->arg, cmd->name);
            again = 1;
            goto pop;
        case 2:
            if ((g_Supervisor.config.flags & CONFIG_BGM_IN_MEMORY) && cmd->arg >= 0)
            {
                switch (cmd->unk_8)
                {
                case 0:
                    if (g_SoundManager.play_preloaded_bgm(cmd->arg) == 0)
                    {
                        goto step;
                    }
                    goto pop;
                case 2:
                    if (BGM_STREAM == NULL)
                    {
                        goto step;
                    }
                    if (BGM_STREAM->Reset(0) >= 0)
                    {
                        goto step;
                    }
                    goto pop;
                case 5: {
                    IDirectSoundBuffer *buffer = bgm_buffer(BGM_STREAM);
                    cmd->arg = BGM_STREAM->m_pWaveFile->m_track->total_size != 0;
                    if (BGM_STREAM->FillBufferWithSound(buffer, cmd->arg, 0) >= 0)
                    {
                        goto step;
                    }
                    goto pop;
                }
                case 7:
                    BGM_STREAM->Play(0, DSBPLAY_LOOPING, 0);
                    goto step;
                }
                if (cmd->unk_8 < 0x14)
                {
                    goto step;
                }
                goto pop;
            }
            if (BGM_STREAM == NULL)
            {
                goto pop;
            }
            switch (cmd->unk_8)
            {
            case 0:
                BGM_STREAM->Stop(FALSE);
                goto step;
            case 1: {
                if (BGM_STREAM->m_refilling)
                {
                    goto done;
                }
                char *name = cmd->arg >= 0 ? g_SoundManager.preload_names[cmd->arg] : cmd->name;
                strcpy(g_SoundManager.bgm_name, name);
                cmd->arg = g_SoundManager.find_bgm(name);
                BGM_STREAM->recreate_buffers(&g_SoundManager.bgm_format[cmd->arg]);
                goto step;
            }
            case 2:
                BGM_STREAM->m_pWaveFile->open_bgm(&g_SoundManager.bgm_format[cmd->arg], 0);
                goto step;
            case 3: {
                IDirectSoundBuffer *buffer = bgm_buffer(BGM_STREAM);
                BGM_STREAM->Reset(0);
                cmd->arg = BGM_STREAM->m_pWaveFile->m_track->total_size != 0;
                if (BGM_STREAM->FillBufferWithSound(buffer, cmd->arg, 0) >= 0)
                {
                    goto step;
                }
                goto pop;
            }
            case 4:
                BGM_STREAM->Play(0, DSBPLAY_LOOPING, 0);
                goto step;
            }
            if (cmd->unk_8 < 7)
            {
                goto step;
            }
            goto pop;
        case 4:
            if (BGM_STREAM == NULL)
            {
                goto pop;
            }
            switch (cmd->unk_8)
            {
            case 0:
                BGM_STREAM->Stop(TRUE);
                goto step;
            case 1:
                if (g_SoundManager.bgm_thread == NULL)
                {
                    goto pop;
                }
                PostThreadMessageA(g_SoundManager.bgm_thread_id, WM_QUIT, 0, 0);
                goto step;
            case 2:
                if (WaitForSingleObject(g_SoundManager.bgm_thread, 0x100) != WAIT_OBJECT_0)
                {
                    PostThreadMessageA(g_SoundManager.bgm_thread_id, WM_QUIT, 0, 0);
                    cmd->unk_8--;
                    goto step;
                }
                g_SoundManager.bgm_thread = NULL;
                goto step;
            case 3:
                CloseHandle(g_SoundManager.bgm_thread);
                CloseHandle(g_SoundManager.bgm_event);
                g_SoundManager.bgm_thread = NULL;
                if (BGM_STREAM != NULL)
                {
                    delete BGM_STREAM;
                }
                g_SoundManager.bgm_stream = NULL;
                goto step;
            case 10:
                goto pop;
            }
            goto step;
        case 3:
            if (BGM_STREAM == NULL)
            {
                goto pop;
            }
            switch (cmd->unk_8)
            {
            case 0:
                BGM_STREAM->Stop(TRUE);
                goto step;
            case 1:
                goto pop;
            }
            goto step;
        case 5: {
            i32 frames = cmd->arg * 60.0f;
            if (BGM_STREAM != NULL)
            {
                BGM_STREAM->m_fade_mode = 1;
                BGM_STREAM->m_fade_time_left = BGM_STREAM->m_fade_duration = frames;
            }
            goto pop;
        }
        case 6:
            if (g_Supervisor.config.bgm_mode == 1)
            {
                if (BGM_STREAM->m_refilling)
                {
                    goto done;
                }
                BGM_STREAM->Pause();
            }
            goto pop;
        case 7:
            if (g_Supervisor.config.bgm_mode == 1)
            {
                if (BGM_STREAM->m_refilling)
                {
                    goto done;
                }
                BGM_STREAM->Unpause();
            }
            goto pop;
        case 9:
            BGM_STREAM->switch_track(&g_SoundManager.bgm_format[g_SoundManager.find_bgm(cmd->name)]);
            goto pop;
        default:
            goto done;
        }
    pop:
        // Drops the command; cmd moves along with the copy.
        for (i32 i = 0; cmd->command != 0;)
        {
            i++;
            *cmd = cmd[1];
            cmd++;
            if (i >= 0x1f)
            {
                break;
            }
        }
    } while (again);
    goto done;
step:
    cmd->unk_8++;
done:
    if (g_Supervisor.config.se_enabled)
    {
        for (i32 i = 0; i < SOUND_QUEUE_SIZE; i++)
        {
            i32 id = g_SoundManager.queued_ids[i];
            if (id < 0)
            {
                break;
            }
            g_SoundManager.queued_ids[i] = -1;
            i32 count = g_SoundManager.queued_counts[i];
            if (count < 0)
            {
                SoundBufferEntry *entry = &g_SoundManager.sound_buffers[id];
                entry->was_playing = 0;
                if (entry->buffer != NULL)
                {
                    DWORD status;
                    entry->buffer->GetStatus(&status);
                    entry->was_playing = status & DSBSTATUS_PLAYING;
                    entry->buffer->Stop();
                }
                g_SoundManager.queued_counts[i] = 0;
            }
            else
            {
                i32 pan = 0;
                for (i32 j = 0; j < count; j++)
                {
                    pan += g_SoundManager.queued_pans[i][j];
                }
                if (count > 0)
                {
                    pan /= count;
                }
                g_SoundManager.queued_counts[i] = 0;
                g_SoundManager.sound_buffers[id].play(pan);
            }
        }
    }
    LEAVE_CS(CS_SOUND);
    return g_SoundManager.bgm_commands[0].command;
}
