// The game's own BGM streaming code (src/DSUtil.cpp: CSoundManager,
// CStreamingSound, CWaveFile) on the DirectSound mixer, without the rest of
// the game:
//
//   th16_dsound_game_test <dir with thbgm.fmt> <thbgm.dat> [track]
//
// Creates the stream the way SoundManager::open_bgm does, starts a track
// with the steps of SoundManager::update_sound_thread's BGM command 2, and
// refills it on a thread written like SoundManager::bgm_thread_proc. The
// mixer runs in manual mode; every output frame is compared with the track
// in thbgm.dat: through the loop point, across CSound::Pause/Unpause, and
// after CStreamingSound::seek. Default track: th16_01.wav (the title
// screen).
//
// This file stands in for the Win32 layer DSUtil.cpp needs (files over
// stdio, critical sections, SetEvent) and for the game globals it reads
// (g_SoundManager.bgm_volume and bgm_file_offset, get_runtime).
#include <math.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <dsound.h>

#include "CriticalSections.h"
#include "SoundManager.h"
#include "dsound_sdl.h"

static int g_checks = 0;
static int g_failures = 0;

#define CHECK(cond)                                                                  \
    do                                                                               \
    {                                                                                \
        g_checks++;                                                                  \
        if (!(cond))                                                                 \
        {                                                                            \
            g_failures++;                                                            \
            fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
        }                                                                            \
    } while (0)

// ---------------------------------------------------------------------------
// Game globals

#define TABLE_ROW(i) {i, 0, 0, 0, 0, 0}
#define TABLE_ROWS10(i)                                                                                         \
    TABLE_ROW(i), TABLE_ROW(i + 1), TABLE_ROW(i + 2), TABLE_ROW(i + 3), TABLE_ROW(i + 4), TABLE_ROW(i + 5), \
        TABLE_ROW(i + 6), TABLE_ROW(i + 7), TABLE_ROW(i + 8), TABLE_ROW(i + 9)
// Only ids, so that SoundManager's constructor finds a row for each.
SoundEffectData g_sound_effect_table[SOUND_EFFECT_COUNT] = {
    TABLE_ROWS10(0),  TABLE_ROWS10(10), TABLE_ROWS10(20), TABLE_ROWS10(30),
    TABLE_ROWS10(40), TABLE_ROWS10(50), TABLE_ROWS10(60), TABLE_ROW(70),
    TABLE_ROW(71),    TABLE_ROW(72),    TABLE_ROW(73),    TABLE_ROW(74),
    TABLE_ROW(75),    TABLE_ROW(76),    TABLE_ROW(77),
};
SoundManager g_SoundManager;
// SupervisorSetup.cpp defines it for the game.
extern "C" const GUID GUID_NULL = {0, 0, 0, {0, 0, 0, 0, 0, 0, 0, 0}};

// The game's clock, here the audio mixed so far.
static std::atomic<uint64_t> g_frames_mixed{0};
double get_runtime()
{
    return g_frames_mixed / 44100.0;
}

// SoundManager.cpp defines it inline.
CSoundManager::~CSoundManager()
{
    if (m_pDS != NULL)
    {
        m_pDS->Release();
        m_pDS = NULL;
    }
}

// ---------------------------------------------------------------------------
// Win32

struct TestEvent
{
    std::mutex mutex;
    std::condition_variable cv;
    bool signaled = false;
    bool handling = false;
};

extern "C" {

BOOL SetEvent(HANDLE hEvent)
{
    TestEvent *event = (TestEvent *)hEvent;
    std::lock_guard<std::mutex> guard(event->mutex);
    event->signaled = true;
    event->cv.notify_all();
    return TRUE;
}

static DWORD g_last_error = 0;

DWORD GetLastError(void)
{
    return g_last_error;
}

DWORD FormatMessageA(DWORD dwFlags, LPCVOID lpSource, DWORD dwMessageId, DWORD dwLanguageId, LPSTR lpBuffer,
                     DWORD nSize, va_list *Arguments)
{
    char *message = strdup("error\r\n");
    *(char **)lpBuffer = message;
    return (DWORD)strlen(message);
}

HLOCAL LocalFree(HLOCAL mem)
{
    free(mem);
    return NULL;
}

HANDLE CreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
                   LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition,
                   DWORD dwFlagsAndAttributes, HANDLE hTemplateFile)
{
    FILE *f = fopen(lpFileName, "rb");
    if (f == NULL)
    {
        g_last_error = 2;
        return INVALID_HANDLE_VALUE;
    }
    return (HANDLE)f;
}

DWORD SetFilePointer(HANDLE hFile, LONG lDistanceToMove, PLONG lpDistanceToMoveHigh, DWORD dwMoveMethod)
{
    FILE *f = (FILE *)hFile;
    fseek(f, lDistanceToMove, dwMoveMethod == FILE_BEGIN ? SEEK_SET : dwMoveMethod == FILE_CURRENT ? SEEK_CUR : SEEK_END);
    return (DWORD)ftell(f);
}

BOOL ReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, LPDWORD lpNumberOfBytesRead,
              LPOVERLAPPED lpOverlapped)
{
    *lpNumberOfBytesRead = (DWORD)fread(lpBuffer, 1, nNumberOfBytesToRead, (FILE *)hFile);
    return TRUE;
}

BOOL CloseHandle(HANDLE hObject)
{
    if (hObject == NULL || hObject == INVALID_HANDLE_VALUE)
    {
        return FALSE;
    }
    fclose((FILE *)hObject);
    return TRUE;
}

void Sleep(DWORD dwMilliseconds)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(dwMilliseconds));
}

void InitializeCriticalSection(LPCRITICAL_SECTION lpCriticalSection)
{
    memset(lpCriticalSection, 0, sizeof(*lpCriticalSection));
    lpCriticalSection->LockSemaphore = (HANDLE) new std::recursive_mutex();
}

void EnterCriticalSection(LPCRITICAL_SECTION lpCriticalSection)
{
    ((std::recursive_mutex *)lpCriticalSection->LockSemaphore)->lock();
}

void LeaveCriticalSection(LPCRITICAL_SECTION lpCriticalSection)
{
    ((std::recursive_mutex *)lpCriticalSection->LockSemaphore)->unlock();
}

} // extern "C"

// ---------------------------------------------------------------------------
// The test

static std::vector<uint8_t> read_file(const std::string &path)
{
    std::vector<uint8_t> data;
    FILE *f = fopen(path.c_str(), "rb");
    if (f == NULL)
    {
        return data;
    }
    fseek(f, 0, SEEK_END);
    data.resize(ftell(f));
    fseek(f, 0, SEEK_SET);
    if (fread(data.data(), 1, data.size(), f) != data.size())
    {
        data.clear();
    }
    fclose(f);
    return data;
}

static TestEvent g_bgm_event;
static CStreamingSound *g_stream = NULL;
static std::atomic<bool> g_quit{false};

// SoundManager::bgm_thread_proc, with g_quit for its WM_QUIT.
static void bgm_thread()
{
    while (!g_quit)
    {
        {
            std::unique_lock<std::mutex> lock(g_bgm_event.mutex);
            if (!g_bgm_event.cv.wait_for(lock, std::chrono::milliseconds(50), [] { return g_bgm_event.signaled; }))
            {
                continue;
            }
            g_bgm_event.signaled = false;
            g_bgm_event.handling = true;
        }
        if (g_stream != NULL && g_stream->m_playing)
        {
            g_stream->m_refilling = TRUE;
            g_stream->HandleWaveStreamNotification(TRUE);
            g_stream->m_refilling = FALSE;
        }
        std::lock_guard<std::mutex> guard(g_bgm_event.mutex);
        g_bgm_event.handling = false;
        g_bgm_event.cv.notify_all();
    }
}

// Mixes `frames` frames one period at a time, letting the BGM thread finish
// each refill before going on (the real stream has 3.5 s of slack, this
// makes the result independent of scheduling), and compares them with the
// track from byte `*offset` on (advanced, wrapping to the loop point), or
// with silence when offset is NULL. Returns the number of matching frames
// before the first difference.
struct Expect
{
    FILE *file;
    const ThBgmFormat *track;
};

static uint64_t mix_and_compare(const Expect &expect, uint64_t frames, uint32_t *offset, double *rms)
{
    const uint32_t period = 512;
    std::vector<int16_t> out(period * 2);
    std::vector<int16_t> expected(period * 2);
    uint64_t matched = 0;
    bool matching = true;
    double energy = 0;
    for (uint64_t done = 0; done < frames; done += period)
    {
        uint32_t count = (uint32_t)std::min<uint64_t>(period, frames - done);
        port_dsound_mix(out.data(), count);
        g_frames_mixed += count;
        if (offset == NULL)
        {
            std::fill(expected.begin(), expected.end(), 0);
        }
        else
        {
            for (uint32_t i = 0; i < count;)
            {
                uint32_t run = std::min<uint32_t>(count - i, (expect.track->total_size - *offset) / 4);
                fseek(expect.file, expect.track->start_offset + *offset, SEEK_SET);
                if (fread(&expected[2 * i], 4, run, expect.file) != run)
                {
                    break;
                }
                i += run;
                *offset += run * 4;
                if (*offset >= (uint32_t)expect.track->total_size)
                {
                    *offset = expect.track->intro_size;
                }
            }
        }
        for (uint32_t i = 0; i < count; i++)
        {
            energy += (double)out[2 * i] * out[2 * i] + (double)out[2 * i + 1] * out[2 * i + 1];
            if (matching && (out[2 * i] != expected[2 * i] || out[2 * i + 1] != expected[2 * i + 1]))
            {
                fprintf(stderr, "    frame %llu is %d/%d, expected %d/%d\n", (unsigned long long)(done + i), out[2 * i],
                        out[2 * i + 1], expected[2 * i], expected[2 * i + 1]);
                matching = false;
            }
            if (matching)
            {
                matched++;
            }
        }
        std::unique_lock<std::mutex> lock(g_bgm_event.mutex);
        g_bgm_event.cv.wait(lock, [] { return !g_bgm_event.signaled && !g_bgm_event.handling; });
    }
    if (rms != NULL)
    {
        *rms = sqrt(energy / (2.0 * frames));
    }
    return matched;
}

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        fprintf(stderr, "usage: %s <dir with thbgm.fmt> <thbgm.dat> [track]\n", argv[0]);
        return 2;
    }
    std::string bgm_path = argv[2];
    std::string track_name = argc > 3 ? argv[3] : "th16_01.wav";
    std::vector<uint8_t> fmt = read_file(std::string(argv[1]) + "/thbgm.fmt");
    if (fmt.empty())
    {
        fprintf(stderr, "no thbgm.fmt in %s\n", argv[1]);
        return 1;
    }
    fmt.resize(fmt.size() + sizeof(ThBgmFormat));
    ThBgmFormat *formats = (ThBgmFormat *)fmt.data();
    ThBgmFormat *track = NULL;
    for (ThBgmFormat *t = formats; t->name[0] != '\0'; t++)
    {
        if (track_name == t->name)
        {
            track = t;
        }
    }
    if (track == NULL)
    {
        fprintf(stderr, "no %s in thbgm.fmt\n", track_name.c_str());
        return 1;
    }
    Expect expect = {fopen(bgm_path.c_str(), "rb"), track};
    if (expect.file == NULL)
    {
        fprintf(stderr, "cannot open %s\n", bgm_path.c_str());
        return 1;
    }
    printf("%s: %d bytes, loop at %d (%.1f s / %.1f s)\n", track->name, track->total_size, track->intro_size,
           track->total_size / 176400.0, track->intro_size / 176400.0);

    // The parts of WinMain and SoundManager::initialize/reset this needs.
    InitializeCriticalSection(&g_CriticalSections.cs[12]);
    g_CriticalSections.enabled = true;
    g_SoundManager.bgm_volume = 100;
    g_SoundManager.bgm_file_offset = 0;
    port_dsound_set_manual(true);
    CSoundManager *manager = new CSoundManager;
    CHECK(DirectSoundCreate8(NULL, &manager->m_pDS, NULL) == DS_OK);
    CHECK(manager->m_pDS->SetCooperativeLevel(NULL, DSSCL_PRIORITY) == DS_OK);
    CHECK(manager->SetPrimaryBufferFormat(2, 44100, 16) == S_OK);

    // SoundManager::open_bgm: the stream starts out on the first track.
    DWORD block_align = formats[0].format.nBlockAlign;
    DWORD notify_size = formats[0].format.nSamplesPerSec * block_align * 4 / 8;
    std::thread thread(bgm_thread);
    notify_size -= notify_size % block_align;
    CHECK(manager->CreateStreaming(&g_stream, bgm_path.c_str(), 0, GUID_NULL, 8, notify_size,
                                   (HANDLE)&g_bgm_event, &formats[0]) == S_OK);
    if (g_stream == NULL)
    {
        return 1;
    }

    // update_sound_thread's BGM command 2, one step per frame.
    const uint64_t frame = 735;
    uint32_t offset = 0;
    g_stream->Stop(FALSE);
    CHECK(mix_and_compare(expect, frame, NULL, NULL) == frame);
    CHECK(g_stream->recreate_buffers(track) == S_OK);
    CHECK(mix_and_compare(expect, frame, NULL, NULL) == frame);
    CHECK(g_stream->m_pWaveFile->open_bgm(track, 0) == S_OK);
    CHECK(g_stream->Reset(0) == S_OK);
    CHECK(g_stream->FillBufferWithSound(g_stream->m_apDSBuffer[0], track->total_size != 0, 0) == S_OK);
    CHECK(mix_and_compare(expect, frame, NULL, NULL) == frame);
    CHECK(g_stream->Play(0, DSBPLAY_LOOPING, 0) == S_OK);

    // Through the loop point and 5 s past it.
    uint64_t frames = track->total_size / 4 + 5 * 44100;
    double rms = 0;
    uint64_t matched = mix_and_compare(expect, frames, &offset, &rms);
    printf("  play:    %llu of %llu frames exact (rms %.0f)\n", (unsigned long long)matched,
           (unsigned long long)frames, rms);
    CHECK(matched == frames);
    CHECK(rms > 1000);

    // The pause menu: silence, then the stream goes on where it stopped.
    CHECK(g_stream->Pause() == S_OK);
    matched = mix_and_compare(expect, 44100, NULL, NULL);
    CHECK(matched == 44100);
    CHECK(g_stream->Unpause() == S_OK);
    frames = 10 * 44100;
    matched = mix_and_compare(expect, frames, &offset, NULL);
    printf("  unpause: %llu of %llu frames exact\n", (unsigned long long)matched, (unsigned long long)frames);
    CHECK(matched == frames);

    // CStreamingSound::seek (the pause menu's BGM restart, replays).
    double seconds = 3.0;
    g_stream->seek(seconds);
    offset = (uint32_t)(44100 * seconds) * 4 - 4;
    matched = mix_and_compare(expect, frames, &offset, NULL);
    printf("  seek:    %llu of %llu frames exact\n", (unsigned long long)matched, (unsigned long long)frames);
    CHECK(matched == frames);

    // SoundManager::stop_bgm.
    g_stream->Stop(TRUE);
    g_quit = true;
    thread.join();
    delete g_stream;
    g_stream = NULL;
    CHECK(mix_and_compare(expect, 1024, NULL, NULL) == 1024);
    delete manager;
    fclose(expect.file);

    printf("%d checks, %d failed\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
