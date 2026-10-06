#pragma once

#include <windows.h>
#include <mmreg.h>

#include "decomp.h"
#include "types.h"

// Only the parts decompiled code needs so far.

// One track of thbgm.fmt.
struct ThBgmFormat
{
    char name[16];
    i32 start_offset;
    i32 unk_14;
    i32 intro_size;
    i32 total_size;
    WAVEFORMATEX format;
    u8 unk_32[2];
};

// The DirectSound sample's CWaveFile, reading from thbgm.dat.
struct CWaveFile
{
    // 0x4718a0. Switches to a track of thbgm.dat.
    HRESULT open_bgm(ThBgmFormat *track, i32 unk);
};

// The BGM stream (an adapted DirectSound sample CStreamingSound). Fields
// from 0x14 on are ZUN's fade state.
struct BgmStream
{
    void *vtable;
    struct IDirectSoundBuffer **buffers;
    u8 unk_8[0xc - 0x8];
    CWaveFile *wave_file;
    u8 unk_10[0x14 - 0x10];
    i32 fade_time_left;
    i32 fade_duration;
    // 1: fade out and stop, 2: fade in, 3/4: like 2/1 but quieter.
    i32 fade_mode;
    u8 unk_20[0x50 - 0x20];
    i32 unk_50;
    u8 unk_54[0x9c - 0x54];
    // Set while the streaming thread refills the buffer.
    i32 refilling;

    void set_volume(i32 volume);
    // 0x471270 (CSound::Stop).
    HRESULT stop(i32 unk);
    // 0x4714c0. Refills the part of the buffer that has played. The
    // argument is the same at every call site; LTCG folded it.
    HRESULT handle_wave_stream_notification(i32 unused);

    // Deletes the stream through its virtual destructor.
    void destroy();
};

// Only the virtual destructor, so that BgmStream can be deleted the way
// ZUN's code does it.
class CStreamingSound
{
  public:
    virtual ~CStreamingSound();
};

inline void BgmStream::destroy()
{
    delete (CStreamingSound *)this;
}

// A row of the sound effect table.
struct SoundEffectData
{
    i32 id;
    // Index into g_sound_file_names.
    i32 file_index;
    i16 volume;
    i16 unk_a;
    // Play flags (DSBPLAY_LOOPING).
    i32 play_flags;
    i32 unk_10;
};

#define SOUND_EFFECT_COUNT 78
#define SOUND_QUEUE_SIZE 12

enum BgmCommand
{
    BGM_PLAY_WAV = 1,
    BGM_PLAY = 2,
    BGM_STOP = 3,
    BGM_STOP_4 = 4,
    BGM_FADE_OUT = 5,
};

struct SoundEffectData;

// One loaded sound effect.
struct SoundBufferEntry
{
    struct IDirectSoundBuffer *buffer;
    i32 unk_4;
    // Entry of the sound effect table this slot plays.
    SoundEffectData *data;
    i32 id;
    i32 pan;
    i32 unk_14;

    // Restarts the sound at a pan, at the configured volume.
    void play(i32 pan);
};

// A request to the sound thread (SoundManager::modify_bgm).
struct BgmCommandEntry
{
    i32 command;
    i32 arg;
    i32 unk_8;
    char name[0x100];
};

#define SOUND_FILE_COUNT 0x43

// What the sound threads should do (SoundManager::thread_state).
enum SoundThreadState
{
    SOUND_THREAD_RUNNING = 0,
    SOUND_THREAD_DONE = 1,
    SOUND_THREAD_QUIT = 2,
};

struct SoundManager
{
    struct IDirectSound8 *dsound;
    struct IDirectSoundBuffer *init_sound_buffer;
    HWND game_window;
    struct CSoundManager *manager;
    DWORD bgm_thread_id;
    HANDLE bgm_thread;
    u8 unk_18[0x1c - 0x18];
    // Sounds to start this frame (-1 for none), how many times each was
    // requested (-1 to stop it instead) and their pans.
    i32 queued_ids[SOUND_QUEUE_SIZE];
    i32 queued_counts[SOUND_QUEUE_SIZE];
    i32 queued_pans[SOUND_QUEUE_SIZE][0x80];
    u8 unk_187c[0x1980 - 0x187c];
    // thbgm.fmt.
    ThBgmFormat *bgm_format;
    // File name of the BGM that select_bgm last switched to.
    char bgm_name[0x100];
    SoundBufferEntry sound_buffers[0x4e];
    // The se_*.wav files, read by the loading thread.
    u8 *sound_file_data[SOUND_FILE_COUNT];
    u8 unk_22e0[0x23e0 - 0x22e0];
    BgmCommandEntry bgm_commands[0x1f];
    u8 unk_4594[0x5660 - 0x4594];
    BgmStream *bgm_stream;
    u8 unk_5664[0x5668 - 0x5664];
    HANDLE bgm_event;
    u8 unk_566c[0x5674 - 0x566c];
    HANDLE init_thread;
    HANDLE load_thread;
    DWORD init_thread_id;
    i32 thread_state;
    HWND window;
    i32 init_done;
    i32 bgm_volume;
    i32 se_volume;
    // DirectSound volume (hundredths of dB) for the BGM.
    i32 bgm_db;

    // Queues a command for the sound thread.
    void modify_bgm(i32 command, i32 arg, const char *name);

    // 0x45d510
    i32 initialize(HWND window);
    // Stops both sound threads, waiting for them to finish.
    static i32 stop_threads();
    // Thread procedures. ZUN passes these cdecl functions to CreateThread.
    static void thread_init(void *arg);
    static void thread_load_sound_files(void *arg);
    // Refills the BGM stream when DirectSound signals bgm_event, until it
    // gets WM_QUIT.
    static DWORD WINAPI bgm_thread_proc(void *arg);
    // Index of a track in thbgm.fmt by file name (directories ignored), 0
    // if there is none.
    i32 find_bgm(const char *path);
    // Stops the BGM, ends its streaming thread and frees the stream.
    void stop_bgm();
    // Clears the sound queue, stops the sound threads and picks up the
    // volume settings.
    i32 reset();
    // Stops one sound, or every sound when id is negative (remembering
    // which were playing).
    void stop_sound(i32 id);
    // Points the BGM stream at another track. Reaches the manager through
    // g_SoundManager; LTCG dropped this.
    i32 select_bgm(const char *path);

    // Members that do not use this; LTCG dropped it.
    static i32 update_sound_thread();
    static void tick_bgm_fade();
    // The second argument is 0 at every call site; LTCG folded it, so
    // callers push whatever register is handy.
    HARNESS_CALLED void play_sound_centered(i32 id, i32 unused);
    // 0x45e1f0. Pans by the x coordinate.
    void play_sound_at_position(i32 id, f32 x);
};

extern SoundManager g_SoundManager;

// Finds a RIFF chunk by its tag among size bytes of chunks; returns its
// data and stores its size (TH06: GetWavFormatData).
HARNESS_CALLED WAVEFORMATEX *__stdcall get_wav_chunk(u8 *data, const char *tag, i32 *chunk_size, u32 size);
extern const char *const g_sound_file_names[SOUND_FILE_COUNT];
extern SoundEffectData g_sound_effect_table[SOUND_EFFECT_COUNT];

void play_sound_centered_stub(i32 id, i32 unused);
