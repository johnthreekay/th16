#pragma once

#include <windows.h>
#include <mmreg.h>
#include <mmsystem.h>

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

#define WAVEFILE_READ 1

// The DirectSound sample's CWaveFile, reading from thbgm.dat. TH06's layout,
// with ZUN's file handle in place of the mmio one.
struct CWaveFile
{
    WAVEFORMATEX *m_pwfx;
    MMCKINFO m_ck;
    MMCKINFO m_ckRiff;
    DWORD m_dwSize;
    MMIOINFO m_mmioinfoOut;
    DWORD m_dwFlags;
    BOOL m_bIsReadingFromMemory;
    BYTE *m_pbData;
    BYTE *m_pbDataCur;
    ULONG m_ulDataSize;
    HANDLE m_file;
    // The track being read.
    ThBgmFormat *m_track;
    const char *m_filename;
    // File position saved by CSound::Pause.
    DWORD m_paused_position;
    // Set once the stream has reached the end of the track and looped.
    BOOL m_looped;

    // 0x4718a0. Switches to a track of thbgm.dat.
    HRESULT open_bgm(ThBgmFormat *track, i32 unk);

    CWaveFile()
    {
        m_track = NULL;
        m_pwfx = NULL;
        m_dwSize = 0;
        m_bIsReadingFromMemory = FALSE;
    }

    ~CWaveFile()
    {
        Close();
    }

    // Opens thbgm.dat for reading at the start of a track.
    HRESULT Open(const char *filename, ThBgmFormat *track)
    {
        m_dwFlags = WAVEFILE_READ;
        m_bIsReadingFromMemory = FALSE;
        return open_file(filename, track);
    }

    HRESULT Close()
    {
        if (m_dwFlags == WAVEFILE_READ)
        {
            CloseHandle(m_file);
            m_file = INVALID_HANDLE_VALUE;
        }
        return S_OK;
    }

    // 0x4717e0. Open's out-of-line part.
    HRESULT open_file(const char *filename, ThBgmFormat *track);
    // 0x471930. Seeks to offset bytes into the track, or to the loop start
    // when loop is set.
    HARNESS_CALLED HRESULT ResetFile(bool loop, DWORD offset);
    HRESULT OpenFromMemory(BYTE *pbData, ULONG ulDataSize, ThBgmFormat *track)
    {
        m_track = track;
        m_ulDataSize = ulDataSize;
        m_pbData = pbData;
        m_pbDataCur = m_pbData;
        m_bIsReadingFromMemory = TRUE;
        return S_OK;
    }

    // ResetFile as LTCG inlined it into the CSound constructor.
    HRESULT reset_file_inline(bool loop, DWORD offset);
    // 0x471a30
    HARNESS_CALLED HRESULT Read(BYTE *pBuffer, DWORD dwSizeToRead, DWORD *pdwSizeRead);
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

class CStreamingSound;

// The DirectSound sample's CSoundManager.
struct CSoundManager
{
    struct IDirectSound8 *m_pDS;

    // 0x470250. Every caller asks for 44.1 kHz 16-bit stereo; LTCG folded
    // the arguments.
    HARNESS_CALLED HRESULT SetPrimaryBufferFormat(DWORD dwPrimaryChannels, DWORD dwPrimaryFreq,
                                                  DWORD dwPrimaryBitRate);
    // 0x470320. Streams a track of thbgm.dat. The file name, flags and
    // notification count are the same at every call site; LTCG folded them.
    HARNESS_CALLED HRESULT CreateStreaming(CStreamingSound **ppStreamingSound, const char *strWaveFileName,
                                           DWORD dwCreationFlags, GUID guid3DAlgorithm, DWORD dwNotifyCount,
                                           DWORD dwNotifySize, HANDLE hNotifyEvent, ThBgmFormat *track);
    // 0x470680. The same for a track already in memory.
    HARNESS_CALLED HRESULT CreateStreamingFromMemory(CStreamingSound **ppStreamingSound, BYTE *pbData,
                                                     ULONG ulDataSize, ThBgmFormat *track, DWORD dwCreationFlags,
                                                     GUID guid3DAlgorithm, DWORD dwNotifyCount, DWORD dwNotifySize,
                                                     HANDLE hNotifyEvent);
};

// The DirectSound sample's CSound with ZUN's fades, pausing and track
// switching. Its doubles are only 4-aligned: MSVC would otherwise pad the
// vtable pointer to 8 bytes.
#pragma pack(push, 4)
// VTABLE: TH16 0x4943a0
class CSound
{
  public:
    struct IDirectSoundBuffer **m_apDSBuffer;
    DWORD m_dwDSBufferSize;
    CWaveFile *m_pWaveFile;
    DWORD m_dwNumBuffers;
    // ZUN's fade state (see BgmStream).
    i32 m_fade_time_left;
    i32 m_fade_duration;
    i32 m_fade_mode;
    // The arguments of the last Play.
    DWORD m_play_priority;
    DWORD m_play_flags;
    u8 unk_28[4];
    i32 unk_2c;
    // get_runtime() at Play and at Pause, and the time spent paused.
    double m_start_time;
    double m_pause_time;
    double m_paused_total;
    double unk_48;
    BOOL m_playing;
    BOOL m_paused;
    // The DSBUFFERDESC the buffer was created with.
    u8 m_desc[0x24];
    CSoundManager *m_manager;

    // 0x4709b0. The buffer count is 1 at every call site; LTCG folded it.
    CSound(struct IDirectSoundBuffer **apDSBuffer, DWORD dwDSBufferSize, DWORD dwNumBuffers, CWaveFile *pWaveFile);
    // 0x470e10
    virtual ~CSound();

    // 0x470ed0
    HRESULT FillBufferWithSound(struct IDirectSoundBuffer *pDSB, BOOL bRepeatWavIfBufferLarger, DWORD offset);
    // FillBufferWithSound as LTCG inlined it into the constructor.
    HRESULT fill_buffer_inline(struct IDirectSoundBuffer *pDSB, BOOL bRepeatWavIfBufferLarger, DWORD offset);
    // Inline: LTCG split it into the NULL check, left in the callers, and
    // the rest (0x471040, restore_dsound_buffer in DSUtil.cpp).
    HRESULT RestoreBuffer(struct IDirectSoundBuffer *pDSB, BOOL *pbWasRestored);
    // 0x4710b0
    struct IDirectSoundBuffer *GetFreeBuffer();
    // 0x471120. offset is where to start in the track.
    HRESULT Play(DWORD dwPriority, DWORD dwFlags, DWORD offset);
    // 0x4711f0. Volume in hundredths of dB, scaled by the BGM volume
    // setting.
    HRESULT SetVolume(i32 volume);
    // 0x471270. Also closes the file if close_file is set.
    HRESULT Stop(BOOL close_file);
    // 0x4712f0 and 0x471380
    HRESULT Pause();
    HRESULT Unpause();
    // 0x4713f0
    HRESULT Reset();
};
#pragma pack(pop)

// The DirectSound sample's CStreamingSound.
// VTABLE: TH16 0x494398
class CStreamingSound : public CSound
{
  public:
    DWORD m_dwLastPlayPos;
    DWORD m_dwPlayProgress;
    DWORD m_dwNextWriteOffset;
    DWORD unk_8c;
    BOOL m_bFillNextNotificationWithSilence;
    DWORD m_dwNotifySize;
    HANDLE m_hNotifyEvent;
    // Set while the streaming thread refills the buffer.
    BOOL m_refilling;

    // 0x471430
    CStreamingSound(struct IDirectSoundBuffer *pDSBuffer, DWORD dwDSBufferSize, CWaveFile *pWaveFile,
                    DWORD dwNotifySize);
    virtual ~CStreamingSound();

    // 0x4714c0. Always called with bLoopedPlay set; LTCG folded it.
    HARNESS_CALLED HRESULT HandleWaveStreamNotification(BOOL bLoopedPlay);
    // 0x471720
    HRESULT Reset(DWORD offset);
    // 0x470bb0. Creates the buffers again in the format of a track.
    HRESULT recreate_buffers(ThBgmFormat *track);
    // 0x471b00. Switches to another track, continuing at the same time.
    HRESULT switch_track(ThBgmFormat *track);
    // 0x471bd0. Seconds into the track, counting from the loop start after
    // the first loop.
    HARNESS_CALLED double get_play_time();
    // 0x471c90. Restarts the track the given number of seconds in.
    HARNESS_CALLED void seek(double seconds);
};

inline void BgmStream::set_volume(i32 volume)
{
    ((CSound *)this)->SetVolume(volume);
}

inline HRESULT BgmStream::stop(i32 unk)
{
    return ((CSound *)this)->Stop(unk);
}

inline HRESULT BgmStream::handle_wave_stream_notification(i32 unused)
{
    return ((CStreamingSound *)this)->HandleWaveStreamNotification(TRUE);
}

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
    // Set while the game is paused if the buffer was playing.
    i32 was_playing;

    // Restarts the sound at a pan, at the configured volume.
    void play(i32 pan);
    // Creates the buffer from the loaded .wav file (or duplicates the
    // buffer of an earlier entry playing the same file), then frees the
    // file. The name is only for the log.
    i32 load(const char *name);
};

// Debug output; empty in release builds. Kept out of line because it is
// variadic.
void sound_debug_log(const char *fmt, ...);

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
    char selected_bgm_name[0x100];
    SoundBufferEntry sound_buffers[0x4e];
    // The se_*.wav files, read by the loading thread.
    u8 *sound_file_data[SOUND_FILE_COUNT];
    // File name of the BGM playing.
    char bgm_name[0x100];
    BgmCommandEntry bgm_commands[0x1f];
    u8 unk_4454[0x5560 - 0x4454];
    // The BGM archive's file name (thbgm.dat).
    char bgm_dat_name[0x100];
    BgmStream *bgm_stream;
    u8 unk_5664[0x5668 - 0x5664];
    HANDLE bgm_event;
    u8 unk_566c[0x5670 - 0x566c];
    // Where the tracks start in thbgm.dat.
    i32 bgm_file_offset;
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
    // 0x45db10. Opens the BGM archive (thbgm.dat at the only call site,
    // which LTCG folds).
    DECOMP_NOINLINE i32 open_bgm_dat(const char *name);
    // Points the BGM stream at another track. Reaches the manager through
    // g_SoundManager; LTCG dropped this.
    i32 select_bgm(const char *path);

    // Members that do not use this; LTCG dropped it.
    static i32 update_sound_thread();
    static void tick_bgm_fade();
    // Stop every sound effect for the pause menu, remembering which were
    // playing, and start those again.
    static void pause_sounds();
    static void resume_sounds();
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
