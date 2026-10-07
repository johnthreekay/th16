// Test harness for the DirectSound mixer (port/src/dsound_sdl.cpp), without
// the game. Checks the mixed PCM itself.
//
//   th16_dsound_test [--data DIR] [--bgm thbgm.dat] [--track NAME] [--realtime]
//
// Without options: the unit tests in manual mode (no SDL device; the test
// calls port_dsound_mix and compares every output frame). --data names a
// directory with thbgm.fmt and se_*.wav taken from th16.dat (th16dat.py):
// the sound effects then load and play the way SoundManager.cpp does it,
// and with --bgm the BGM stream test runs: a copy of DSUtil.cpp's
// CStreamingSound refills from thbgm.dat on a thread of its own, woken by
// the buffer's notifications, and the output must equal the track sample
// for sample, through the loop point. --realtime adds a test through a real
// SDL device; it refuses to run unless SDL_AUDIODRIVER is "disk" or
// "dummy", and with "disk" checks the file SDL_DISKAUDIOFILE names.
//
// The Win32 side: dsound_sdl.cpp signals notifications with SetEvent, which
// this file implements for its own events.
#include <SDL.h>

#include <math.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <dsound.h>

#include "dsound_sdl.h"

// DSUtil.cpp defines it for the game.
extern "C" const GUID IID_IDirectSoundNotify = {0xb0210783, 0x89cd, 0x11d0,
                                                {0xaf, 0x08, 0x00, 0xa0, 0xc9, 0x25, 0xcd, 0x16}};

// ---------------------------------------------------------------------------
// Checks

static int g_checks = 0;
static int g_failures = 0;

#define CHECK(cond)                                                             \
    do                                                                          \
    {                                                                           \
        g_checks++;                                                             \
        if (!(cond))                                                            \
        {                                                                       \
            g_failures++;                                                       \
            fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
        }                                                                       \
    } while (0)

#define CHECK_EQ(a, b)                                                                                    \
    do                                                                                                    \
    {                                                                                                     \
        g_checks++;                                                                                       \
        long long check_a_ = (long long)(a);                                                              \
        long long check_b_ = (long long)(b);                                                              \
        if (check_a_ != check_b_)                                                                         \
        {                                                                                                 \
            g_failures++;                                                                                 \
            fprintf(stderr, "%s:%d: CHECK_EQ failed: %s == %s (%lld != %lld)\n", __FILE__, __LINE__, #a, #b, \
                    check_a_, check_b_);                                                                  \
        }                                                                                                 \
    } while (0)

// ---------------------------------------------------------------------------
// Events: what the Win32 layer's CreateEventA objects are to the game, as far
// as the mixer is concerned (it only calls SetEvent).

struct TestEvent
{
    std::mutex mutex;
    std::condition_variable cv;
    bool signaled = false;
    bool handling = false;
    int count = 0;

    void set()
    {
        std::lock_guard<std::mutex> guard(mutex);
        signaled = true;
        count++;
        cv.notify_all();
    }
    // Auto-reset wait. On success the waiter is "handling" until handled().
    bool wait(int timeout_ms)
    {
        std::unique_lock<std::mutex> lock(mutex);
        if (!cv.wait_for(lock, std::chrono::milliseconds(timeout_ms), [this] { return signaled; }))
        {
            return false;
        }
        signaled = false;
        handling = true;
        return true;
    }
    void handled()
    {
        std::lock_guard<std::mutex> guard(mutex);
        handling = false;
        cv.notify_all();
    }
    // Until no signal is pending and the waiter has finished handling.
    void wait_idle()
    {
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait(lock, [this] { return !signaled && !handling; });
    }
    int take_count()
    {
        std::lock_guard<std::mutex> guard(mutex);
        int n = count;
        count = 0;
        signaled = false;
        return n;
    }
};

extern "C" BOOL SetEvent(HANDLE hEvent)
{
    ((TestEvent *)hEvent)->set();
    return TRUE;
}

// ---------------------------------------------------------------------------
// Helpers

static WAVEFORMATEX make_format(WORD channels, DWORD rate, WORD bits)
{
    WAVEFORMATEX format;
    memset(&format, 0, sizeof(format));
    format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = channels;
    format.nSamplesPerSec = rate;
    format.wBitsPerSample = bits;
    format.nBlockAlign = channels * bits / 8;
    format.nAvgBytesPerSec = rate * format.nBlockAlign;
    return format;
}

static IDirectSoundBuffer *make_buffer(IDirectSound8 *ds, DWORD flags, WORD channels, DWORD rate, WORD bits,
                                       DWORD bytes)
{
    WAVEFORMATEX format = make_format(channels, rate, bits);
    DSBUFFERDESC desc;
    memset(&desc, 0, sizeof(desc));
    desc.dwSize = sizeof(desc);
    desc.dwFlags = flags;
    desc.dwBufferBytes = bytes;
    desc.lpwfxFormat = &format;
    IDirectSoundBuffer *buffer = NULL;
    HRESULT hr = ds->CreateSoundBuffer(&desc, &buffer, NULL);
    CHECK_EQ(hr, DS_OK);
    return buffer;
}

static void fill(IDirectSoundBuffer *buffer, const void *data, DWORD bytes)
{
    void *p1;
    void *p2;
    DWORD n1;
    DWORD n2;
    CHECK_EQ(buffer->Lock(0, bytes, &p1, &n1, &p2, &n2, 0), DS_OK);
    CHECK_EQ(n1, bytes);
    CHECK(p2 == NULL);
    memcpy(p1, data, n1);
    CHECK_EQ(buffer->Unlock(p1, n1, p2, n2), DS_OK);
}

static std::vector<int16_t> mix(uint32_t frames)
{
    std::vector<int16_t> out(frames * 2);
    port_dsound_mix(out.data(), frames);
    return out;
}

static bool all_zero(const std::vector<int16_t> &v)
{
    for (int16_t s : v)
    {
        if (s != 0)
        {
            return false;
        }
    }
    return true;
}

static float db_gain(LONG hundredths)
{
    return powf(10.0f, hundredths / 2000.0f);
}

// ---------------------------------------------------------------------------
// Unit tests (manual mode)

static void test_create(IDirectSound8 *ds)
{
    CHECK_EQ(ds->SetCooperativeLevel(NULL, DSSCL_PRIORITY), DS_OK);

    // The primary buffer, as CSoundManager::SetPrimaryBufferFormat uses it.
    DSBUFFERDESC desc;
    memset(&desc, 0, sizeof(desc));
    desc.dwSize = sizeof(desc);
    desc.dwFlags = DSBCAPS_PRIMARYBUFFER;
    IDirectSoundBuffer *primary = NULL;
    CHECK_EQ(ds->CreateSoundBuffer(&desc, &primary, NULL), DS_OK);
    WAVEFORMATEX format = make_format(2, 44100, 16);
    CHECK_EQ(primary->SetFormat(&format), DS_OK);
    WAVEFORMATEX got;
    DWORD written = 0;
    CHECK_EQ(primary->GetFormat(&got, sizeof(got), &written), DS_OK);
    CHECK_EQ(written, sizeof(WAVEFORMATEX));
    CHECK(memcmp(&got, &format, sizeof(format)) == 0);
    void *p1;
    DWORD n1;
    CHECK(FAILED(primary->Lock(0, 4, &p1, &n1, NULL, NULL, 0)));
    IDirectSoundBuffer *dup = NULL;
    CHECK_EQ(ds->DuplicateSoundBuffer(primary, &dup), DSERR_INVALIDCALL);
    primary->Release();

    // Formats and sizes DirectSound refuses.
    WAVEFORMATEX bad = make_format(2, 44100, 24);
    desc.dwFlags = DSBCAPS_CTRLVOLUME;
    desc.dwBufferBytes = 600;
    desc.lpwfxFormat = &bad;
    IDirectSoundBuffer *buffer = (IDirectSoundBuffer *)1;
    CHECK_EQ(ds->CreateSoundBuffer(&desc, &buffer, NULL), DSERR_BADFORMAT);
    CHECK(buffer == NULL);
    desc.lpwfxFormat = &format;
    desc.dwBufferBytes = 0;
    CHECK_EQ(ds->CreateSoundBuffer(&desc, &buffer, NULL), DSERR_INVALIDPARAM);

    // Notifications need DSBCAPS_CTRLPOSITIONNOTIFY.
    buffer = make_buffer(ds, DSBCAPS_CTRLVOLUME, 2, 44100, 16, 400);
    IDirectSoundNotify *notify = (IDirectSoundNotify *)1;
    CHECK_EQ(buffer->QueryInterface(IID_IDirectSoundNotify, (void **)&notify), E_NOINTERFACE);
    CHECK(notify == NULL);
    DSBCAPS caps;
    caps.dwSize = sizeof(caps);
    CHECK_EQ(buffer->GetCaps(&caps), DS_OK);
    CHECK_EQ(caps.dwBufferBytes, 400);
    CHECK(caps.dwFlags & DSBCAPS_CTRLVOLUME);
    buffer->Release();
    CHECK(all_zero(mix(64)));
}

// A looping 16-bit stereo buffer at the mix rate comes out bit-exact, and
// the cursors follow.
static void test_looping(IDirectSound8 *ds)
{
    const uint32_t frames = 1000;
    std::vector<int16_t> pattern(frames * 2);
    for (uint32_t i = 0; i < frames; i++)
    {
        pattern[2 * i] = (int16_t)(i * 7 - 3000);
        pattern[2 * i + 1] = (int16_t)(1000 - i * 5);
    }
    IDirectSoundBuffer *buffer = make_buffer(ds, DSBCAPS_CTRLVOLUME, 2, 44100, 16, frames * 4);
    fill(buffer, pattern.data(), frames * 4);
    CHECK_EQ(buffer->Play(0, 0, DSBPLAY_LOOPING), DS_OK);
    DWORD status = 0;
    CHECK_EQ(buffer->GetStatus(&status), DS_OK);
    CHECK_EQ(status, DSBSTATUS_PLAYING | DSBSTATUS_LOOPING);

    bool exact = true;
    for (uint32_t call = 0; call < 10; call++)
    {
        std::vector<int16_t> out = mix(250);
        for (uint32_t i = 0; i < 250; i++)
        {
            uint32_t k = (call * 250 + i) % frames;
            if (out[2 * i] != pattern[2 * k] || out[2 * i + 1] != pattern[2 * k + 1])
            {
                exact = false;
            }
        }
    }
    CHECK(exact);
    DWORD play;
    DWORD write;
    CHECK_EQ(buffer->GetCurrentPosition(&play, &write), DS_OK);
    CHECK_EQ(play, 500 * 4);
    // A period (250 frames) and one frame of interpolation ahead.
    CHECK_EQ(write, (500 + 251) * 4);

    CHECK_EQ(buffer->Stop(), DS_OK);
    CHECK_EQ(buffer->GetStatus(&status), DS_OK);
    CHECK_EQ(status, 0);
    CHECK(all_zero(mix(100)));
    CHECK_EQ(buffer->GetCurrentPosition(&play, &write), DS_OK);
    CHECK_EQ(play, 500 * 4);
    CHECK_EQ(write, 500 * 4);

    // Play resumes where Stop left it.
    CHECK_EQ(buffer->Play(0, 0, DSBPLAY_LOOPING), DS_OK);
    std::vector<int16_t> out = mix(10);
    CHECK_EQ(out[0], pattern[2 * 500]);
    CHECK_EQ(out[1], pattern[2 * 500 + 1]);
    CHECK_EQ(out[18], pattern[2 * 509]);
    buffer->Release();
    CHECK(all_zero(mix(16)));
}

// A one-shot 8-bit mono 22.05 kHz effect (the format of most se_*.wav):
// upsampled with linear interpolation, then the buffer stops, rewinds and
// signals DSBPN_OFFSETSTOP.
static void test_one_shot(IDirectSound8 *ds)
{
    const uint32_t frames = 300;
    std::vector<uint8_t> data(frames);
    for (uint32_t i = 0; i < frames; i++)
    {
        data[i] = (uint8_t)(128 + (int)((i * 37) % 200) - 100);
    }
    IDirectSoundBuffer *buffer =
        make_buffer(ds, DSBCAPS_CTRLVOLUME | DSBCAPS_CTRLPOSITIONNOTIFY, 1, 22050, 8, frames);
    fill(buffer, data.data(), frames);
    TestEvent stop_event;
    IDirectSoundNotify *notify = NULL;
    CHECK_EQ(buffer->QueryInterface(IID_IDirectSoundNotify, (void **)&notify), DS_OK);
    DSBPOSITIONNOTIFY position = {DSBPN_OFFSETSTOP, &stop_event};
    CHECK_EQ(notify->SetNotificationPositions(1, &position), DS_OK);
    notify->Release();

    CHECK_EQ(buffer->SetCurrentPosition(0), DS_OK);
    CHECK_EQ(buffer->Play(0, 0, 0), DS_OK);
    std::vector<int16_t> out = mix(1000);
    bool exact = true;
    for (uint32_t j = 0; j < 1000; j++)
    {
        float expected = 0.0f;
        if (j < 2 * frames)
        {
            float a = (data[j / 2] - 128) * 256.0f;
            float b = j / 2 + 1 < frames ? (data[j / 2 + 1] - 128) * 256.0f : a;
            expected = j % 2 == 0 ? a : (a + b) / 2;
        }
        if (out[2 * j] != (int16_t)expected || out[2 * j + 1] != (int16_t)expected)
        {
            if (exact)
            {
                fprintf(stderr, "one-shot: frame %u is %d/%d, expected %d\n", j, out[2 * j], out[2 * j + 1],
                        (int)expected);
            }
            exact = false;
        }
    }
    CHECK(exact);
    DWORD status = 1;
    CHECK_EQ(buffer->GetStatus(&status), DS_OK);
    CHECK_EQ(status, 0);
    DWORD play = 1;
    CHECK_EQ(buffer->GetCurrentPosition(&play, NULL), DS_OK);
    CHECK_EQ(play, 0);
    CHECK_EQ(stop_event.take_count(), 1);

    // Played again from the start; Stop also signals DSBPN_OFFSETSTOP.
    CHECK_EQ(buffer->Play(0, 0, 0), DS_OK);
    out = mix(4);
    CHECK_EQ(out[0], (data[0] - 128) * 256);
    CHECK_EQ(buffer->Stop(), DS_OK);
    CHECK_EQ(stop_event.take_count(), 1);
    buffer->Release();
}

// Volume and pan in hundredths of a decibel, with DirectSound's checks.
static void test_volume_pan(IDirectSound8 *ds)
{
    std::vector<int16_t> data(100, 10000);
    IDirectSoundBuffer *buffer = make_buffer(ds, DSBCAPS_CTRLVOLUME | DSBCAPS_CTRLPAN, 1, 44100, 16, 200);
    fill(buffer, data.data(), 200);
    CHECK_EQ(buffer->Play(0, 0, DSBPLAY_LOOPING), DS_OK);

    std::vector<int16_t> out = mix(8);
    CHECK_EQ(out[0], 10000);
    CHECK_EQ(out[1], 10000);

    // -6 dB: half the amplitude.
    CHECK_EQ(buffer->SetVolume(-600), DS_OK);
    out = mix(8);
    CHECK_EQ(out[0], 5012);
    CHECK_EQ(out[1], 5012);
    LONG value = 0;
    CHECK_EQ(buffer->GetVolume(&value), DS_OK);
    CHECK_EQ(value, -600);

    // Panned right by 10 dB: the left channel at a third.
    CHECK_EQ(buffer->SetVolume(0), DS_OK);
    CHECK_EQ(buffer->SetPan(1000), DS_OK);
    out = mix(8);
    CHECK_EQ(out[0], 3162);
    CHECK_EQ(out[1], 10000);
    CHECK_EQ(buffer->SetPan(DSBPAN_LEFT), DS_OK);
    out = mix(8);
    CHECK_EQ(out[0], 10000);
    CHECK_EQ(out[1], 0);
    CHECK_EQ(buffer->SetPan(0), DS_OK);

    // What SoundBufferEntry::play sets for an effect at -19 dB (se_plst00)
    // with the SE volume at 100%.
    CHECK_EQ(buffer->SetVolume(-1900), DS_OK);
    out = mix(8);
    CHECK_EQ(out[0], (int)lrintf(10000 * db_gain(-1900)));
    CHECK_EQ(out[0], 1122);

    CHECK_EQ(buffer->SetVolume(DSBVOLUME_MIN), DS_OK);
    CHECK(all_zero(mix(8)));
    CHECK_EQ(buffer->SetVolume(DSBVOLUME_MIN - 1), DSERR_INVALIDPARAM);
    CHECK_EQ(buffer->SetVolume(1), DSERR_INVALIDPARAM);
    CHECK_EQ(buffer->SetPan(10001), DSERR_INVALIDPARAM);
    CHECK_EQ(buffer->GetVolume(&value), DS_OK);
    CHECK_EQ(value, DSBVOLUME_MIN);
    buffer->Release();

    // Controls the buffer was not created with.
    buffer = make_buffer(ds, 0, 1, 44100, 16, 200);
    CHECK_EQ(buffer->SetVolume(-100), DSERR_CONTROLUNAVAIL);
    CHECK_EQ(buffer->SetPan(100), DSERR_CONTROLUNAVAIL);
    CHECK_EQ(buffer->SetFrequency(22050), DSERR_CONTROLUNAVAIL);
    buffer->Release();

    // SetFrequency: a 44.1 kHz buffer played at 22.05 kHz.
    std::vector<int16_t> ramp(100);
    for (int i = 0; i < 100; i++)
    {
        ramp[i] = (int16_t)(i * 100);
    }
    buffer = make_buffer(ds, DSBCAPS_CTRLFREQUENCY, 1, 44100, 16, 200);
    fill(buffer, ramp.data(), 200);
    CHECK_EQ(buffer->SetFrequency(22050), DS_OK);
    DWORD frequency = 0;
    CHECK_EQ(buffer->GetFrequency(&frequency), DS_OK);
    CHECK_EQ(frequency, 22050);
    CHECK_EQ(buffer->Play(0, 0, 0), DS_OK);
    out = mix(6);
    CHECK_EQ(out[0], 0);
    CHECK_EQ(out[2], 50);
    CHECK_EQ(out[4], 100);
    CHECK_EQ(out[10], 250);
    CHECK_EQ(buffer->SetFrequency(DSBFREQUENCY_ORIGINAL), DS_OK);
    CHECK_EQ(buffer->GetFrequency(&frequency), DS_OK);
    CHECK_EQ(frequency, 44100);
    buffer->Release();
}

// Lock regions that wrap past the end, the write cursor, and Unlock.
static void test_lock(IDirectSound8 *ds)
{
    const DWORD size = 1000 * 4;
    IDirectSoundBuffer *buffer = make_buffer(ds, DSBCAPS_CTRLVOLUME, 2, 44100, 16, size);
    void *p1;
    void *p2;
    DWORD n1;
    DWORD n2;
    CHECK_EQ(buffer->Lock(3600, 800, &p1, &n1, &p2, &n2, 0), DS_OK);
    CHECK_EQ(n1, 400);
    CHECK_EQ(n2, 400);
    CHECK(p2 != NULL);
    for (int i = 0; i < 100; i++)
    {
        ((int16_t *)p1)[2 * i] = 1111;
        ((int16_t *)p1)[2 * i + 1] = -1111;
        ((int16_t *)p2)[2 * i] = 2222;
        ((int16_t *)p2)[2 * i + 1] = -2222;
    }
    CHECK_EQ(buffer->Unlock(p1, n1, p2, n2), DS_OK);
    // Unlocking twice, or something never locked, fails.
    CHECK_EQ(buffer->Unlock(p1, n1, p2, n2), DSERR_INVALIDPARAM);

    // Without a second pointer the wrapped part is left out.
    CHECK_EQ(buffer->Lock(3600, 800, &p1, &n1, NULL, NULL, 0), DS_OK);
    CHECK_EQ(n1, 400);
    CHECK_EQ(((int16_t *)p1)[0], 1111);
    CHECK_EQ(buffer->Unlock(p1, 0, NULL, 0), DS_OK);
    CHECK_EQ(buffer->Lock(size, 4, &p1, &n1, &p2, &n2, 0), DSERR_INVALIDPARAM);
    CHECK_EQ(buffer->Lock(0, size + 1, &p1, &n1, &p2, &n2, 0), DSERR_INVALIDPARAM);
    CHECK_EQ(buffer->Lock(100, 4, &p1, &n1, &p2, &n2, DSBLOCK_ENTIREBUFFER), DS_OK);
    CHECK_EQ(n1, size - 100);
    CHECK_EQ(n2, 100);
    CHECK_EQ(buffer->Unlock(p1, n1, p2, n2), DS_OK);

    CHECK_EQ(buffer->SetCurrentPosition(3600), DS_OK);
    CHECK_EQ(buffer->Play(0, 0, DSBPLAY_LOOPING), DS_OK);
    std::vector<int16_t> out = mix(200);
    bool exact = true;
    for (int i = 0; i < 200; i++)
    {
        int16_t expected = i < 100 ? 1111 : 2222;
        if (out[2 * i] != expected || out[2 * i + 1] != -expected)
        {
            exact = false;
        }
    }
    CHECK(exact);

    // Data written at the write cursor is not played by the next period.
    DWORD play;
    DWORD write;
    CHECK_EQ(buffer->GetCurrentPosition(&play, &write), DS_OK);
    CHECK_EQ(play, 100 * 4);
    CHECK_EQ(write, 301 * 4);
    CHECK_EQ(buffer->Lock(0, 4, &p1, &n1, &p2, &n2, DSBLOCK_FROMWRITECURSOR), DS_OK);
    CHECK_EQ(n1, 4);
    ((int16_t *)p1)[0] = 30000;
    ((int16_t *)p1)[1] = -30000;
    CHECK_EQ(buffer->Unlock(p1, n1, p2, n2), DS_OK);
    out = mix(200);
    CHECK(all_zero(out));
    out = mix(200);
    CHECK_EQ(out[2], 30000);
    CHECK_EQ(out[3], -30000);
    buffer->Release();
}

// Notifications at the last byte of each chunk, the way CStreamingSound
// sets them up, one event per chunk to see which fire.
static void test_notifications(IDirectSound8 *ds)
{
    const DWORD chunk = 100 * 4;
    IDirectSoundBuffer *buffer =
        make_buffer(ds, DSBCAPS_CTRLVOLUME | DSBCAPS_CTRLPOSITIONNOTIFY | DSBCAPS_GETCURRENTPOSITION2, 2, 44100,
                    16, chunk * 8);
    TestEvent events[8];
    DSBPOSITIONNOTIFY positions[8];
    for (int i = 0; i < 8; i++)
    {
        positions[i].dwOffset = chunk * i + chunk - 1;
        positions[i].hEventNotify = &events[i];
    }
    IDirectSoundNotify *notify = NULL;
    CHECK_EQ(buffer->QueryInterface(IID_IDirectSoundNotify, (void **)&notify), DS_OK);
    DSBPOSITIONNOTIFY outside = {chunk * 8, &events[0]};
    CHECK_EQ(notify->SetNotificationPositions(1, &outside), DSERR_INVALIDPARAM);
    CHECK_EQ(notify->SetNotificationPositions(8, positions), DS_OK);

    auto counts = [&events]() {
        std::string s;
        for (TestEvent &e : events)
        {
            s += (char)('0' + e.take_count());
        }
        return s;
    };

    CHECK_EQ(buffer->Play(0, 0, DSBPLAY_LOOPING), DS_OK);
    CHECK_EQ(notify->SetNotificationPositions(8, positions), DSERR_INVALIDCALL);
    mix(99);
    CHECK(counts() == "00000000");
    mix(1);
    CHECK(counts() == "10000000");
    mix(150);
    CHECK(counts() == "01000000");
    mix(50);
    CHECK(counts() == "00100000");
    mix(500);
    CHECK(counts() == "00011111");
    mix(100);
    CHECK(counts() == "10000000");
    // More than the whole buffer in one call: each fires once.
    mix(2000);
    CHECK(counts() == "11111111");
    CHECK_EQ(buffer->Stop(), DS_OK);
    mix(400);
    CHECK(counts() == "00000000");
    notify->Release();
    buffer->Release();
}

// Duplicates share the memory but not the playback state.
static void test_duplicate(IDirectSound8 *ds)
{
    std::vector<int16_t> data(100, 8000);
    IDirectSoundBuffer *original = make_buffer(ds, DSBCAPS_CTRLVOLUME | DSBCAPS_CTRLPAN, 1, 44100, 16, 200);
    fill(original, data.data(), 200);
    IDirectSoundBuffer *copy = NULL;
    CHECK_EQ(ds->DuplicateSoundBuffer(original, &copy), DS_OK);
    CHECK(copy != NULL && copy != original);
    CHECK_EQ(original->Play(0, 0, DSBPLAY_LOOPING), DS_OK);
    CHECK_EQ(copy->Play(0, 0, DSBPLAY_LOOPING), DS_OK);
    std::vector<int16_t> out = mix(4);
    CHECK_EQ(out[0], 16000);
    CHECK_EQ(copy->SetVolume(-600), DS_OK);
    out = mix(4);
    CHECK(abs(out[0] - 12010) <= 1);
    CHECK_EQ(copy->Stop(), DS_OK);
    DWORD status = 0;
    CHECK_EQ(original->GetStatus(&status), DS_OK);
    CHECK(status & DSBSTATUS_PLAYING);
    out = mix(4);
    CHECK_EQ(out[0], 8000);

    // Writing through the original changes what the copy plays.
    std::vector<int16_t> other(100, 1000);
    fill(original, other.data(), 200);
    original->Release();
    CHECK_EQ(copy->SetVolume(0), DS_OK);
    CHECK_EQ(copy->Play(0, 0, DSBPLAY_LOOPING), DS_OK);
    out = mix(4);
    CHECK_EQ(out[0], 1000);
    copy->Release();
    CHECK(all_zero(mix(4)));
}

static void test_clamp(IDirectSound8 *ds)
{
    std::vector<int16_t> loud(200);
    for (int i = 0; i < 100; i++)
    {
        loud[2 * i] = 30000;
        loud[2 * i + 1] = -30000;
    }
    IDirectSoundBuffer *a = make_buffer(ds, 0, 2, 44100, 16, 400);
    IDirectSoundBuffer *b = make_buffer(ds, 0, 2, 44100, 16, 400);
    fill(a, loud.data(), 400);
    fill(b, loud.data(), 400);
    a->Play(0, 0, DSBPLAY_LOOPING);
    b->Play(0, 0, DSBPLAY_LOOPING);
    std::vector<int16_t> out = mix(4);
    CHECK_EQ(out[0], 32767);
    CHECK_EQ(out[1], -32768);
    a->Release();
    b->Release();
}

// ---------------------------------------------------------------------------
// Game data

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

// SoundManager.cpp's get_wav_chunk.
static uint8_t *wav_chunk(uint8_t *data, const char *tag, int32_t *chunk_size, uint32_t size)
{
    while (size != 0)
    {
        *chunk_size = *(int32_t *)(data + 4);
        if (strncmp((char *)data, tag, 4) == 0)
        {
            return data + 8;
        }
        size -= *chunk_size + 8;
        data += *chunk_size + 8;
    }
    return NULL;
}

// Loads a .wav the way SoundBufferEntry::load does, plays it the way
// SoundBufferEntry::play does (SE volume 100%), and compares the output with
// the samples.
static void test_sound_effect(IDirectSound8 *ds, const std::string &dir, const char *name, LONG volume)
{
    std::vector<uint8_t> file = read_file(dir + "/" + name);
    if (file.empty())
    {
        fprintf(stderr, "skipping %s (not in %s)\n", name, dir.c_str());
        return;
    }
    CHECK(memcmp(file.data(), "RIFF", 4) == 0);
    int32_t riff_size = *(int32_t *)(file.data() + 4);
    uint8_t *chunks = file.data() + 12;
    int32_t chunk_size = 0;
    WAVEFORMATEX wfx = *(WAVEFORMATEX *)wav_chunk(chunks, "fmt ", &chunk_size, riff_size - 12);
    uint8_t *samples = wav_chunk(chunks, "data", &chunk_size, riff_size - 12);
    CHECK(samples != NULL);

    DSBUFFERDESC desc;
    memset(&desc, 0, sizeof(desc));
    desc.dwSize = sizeof(desc);
    desc.dwFlags = DSBCAPS_GLOBALFOCUS | DSBCAPS_CTRLVOLUME | DSBCAPS_CTRLPAN | DSBCAPS_LOCSOFTWARE;
    desc.dwBufferBytes = chunk_size;
    desc.lpwfxFormat = &wfx;
    IDirectSoundBuffer *buffer = NULL;
    CHECK_EQ(ds->CreateSoundBuffer(&desc, &buffer, NULL), DS_OK);
    void *p1;
    void *p2;
    DWORD n1;
    DWORD n2;
    CHECK_EQ(buffer->Lock(0, chunk_size, &p1, &n1, &p2, &n2, 0), DS_OK);
    memcpy(p1, samples, n1);
    if (n2 != 0)
    {
        memcpy(p2, samples + n1, n2);
    }
    buffer->Unlock(p1, n1, p2, n2);

    // A second effect on the same file shares it (SoundBufferEntry::load).
    IDirectSoundBuffer *dup = NULL;
    CHECK_EQ(ds->DuplicateSoundBuffer(buffer, &dup), DS_OK);
    buffer->Release();
    buffer = dup;

    buffer->Stop();
    buffer->SetCurrentPosition(0);
    buffer->SetPan(0);
    float x = 100 / 100.0f;
    float t = (1.0f - x) * (1.0f - x) * (1.0f - x);
    float u = 1.0f - t;
    buffer->SetVolume((LONG)(u * (volume + 5000)) - 5000);
    CHECK_EQ(buffer->Play(0, 0, 0), DS_OK);

    // 22.05 kHz: every other output frame is a sample of the file.
    uint32_t block = wfx.nChannels * wfx.wBitsPerSample / 8;
    uint32_t frames = chunk_size / block;
    uint32_t ratio = 44100 / wfx.nSamplesPerSec;
    std::vector<int16_t> out = mix(frames * ratio + 1000);
    float gain = db_gain(volume);
    uint32_t mismatches = 0;
    double energy = 0;
    for (uint32_t i = 0; i < frames; i++)
    {
        float left;
        float right;
        const uint8_t *p = samples + i * block;
        if (wfx.wBitsPerSample == 8)
        {
            left = (p[0] - 128) * 256.0f;
            right = wfx.nChannels == 2 ? (p[1] - 128) * 256.0f : left;
        }
        else
        {
            left = *(int16_t *)p;
            right = wfx.nChannels == 2 ? *(int16_t *)(p + 2) : left;
        }
        int16_t l = out[2 * i * ratio];
        int16_t r = out[2 * i * ratio + 1];
        if (l != (int16_t)lrintf(left * gain) || r != (int16_t)lrintf(right * gain))
        {
            mismatches++;
        }
        energy += (double)l * l + (double)r * r;
    }
    std::vector<int16_t> tail(out.begin() + 2 * frames * ratio, out.end());
    DWORD status = 1;
    buffer->GetStatus(&status);
    printf("  %-16s %5u Hz %2u-bit %u ch, %6u frames, rms %7.1f, %u mismatches, stopped %s\n", name,
           (unsigned)wfx.nSamplesPerSec, wfx.wBitsPerSample, wfx.nChannels, frames, sqrt(energy / (2.0 * frames)),
           mismatches, status == 0 ? "yes" : "no");
    CHECK_EQ(mismatches, 0);
    CHECK(energy > 0);
    CHECK(all_zero(tail));
    CHECK_EQ(status, 0);
    buffer->Release();
}

// One track of thbgm.fmt (SoundManager.h ThBgmFormat).
struct Track
{
    std::string name;
    uint32_t start;
    uint32_t intro;
    uint32_t total;
    WAVEFORMATEX format;
};

static std::vector<Track> read_tracks(const std::string &path)
{
    std::vector<Track> tracks;
    std::vector<uint8_t> fmt = read_file(path);
    for (size_t p = 0; p + 0x34 <= fmt.size() && fmt[p] != 0; p += 0x34)
    {
        Track track;
        track.name = std::string((const char *)&fmt[p], strnlen((const char *)&fmt[p], 16));
        track.start = *(uint32_t *)&fmt[p + 0x10];
        track.intro = *(uint32_t *)&fmt[p + 0x18];
        track.total = *(uint32_t *)&fmt[p + 0x1c];
        memcpy(&track.format, &fmt[p + 0x20], sizeof(WAVEFORMATEX));
        tracks.push_back(track);
    }
    return tracks;
}

// The part of DSUtil.cpp's CWaveFile the stream reads through: a track of
// thbgm.dat (or of memory), read up to its end, then from the loop point.
struct WaveSource
{
    FILE *file = NULL;
    const uint8_t *memory = NULL;
    uint32_t start = 0;
    uint32_t intro = 0;
    uint32_t total = 0;
    uint32_t cursor = 0;
    uint32_t remaining = 0;

    // CWaveFile::ResetFile.
    void reset(bool loop, uint32_t offset)
    {
        if (loop && intro > 0)
        {
            cursor = intro;
            remaining = total - intro;
            return;
        }
        cursor = offset;
        remaining = total - offset;
    }
    // CWaveFile::Read.
    DWORD read(uint8_t *dest, DWORD size)
    {
        size = std::min(size, remaining);
        remaining -= size;
        if (memory != NULL)
        {
            memcpy(dest, memory + cursor, size);
        }
        else
        {
            fseek(file, start + cursor, SEEK_SET);
            size = (DWORD)fread(dest, 1, size, file);
        }
        cursor += size;
        return size;
    }
};

// DSUtil.cpp's CStreamingSound: 8 chunks of half a second, a notification
// at the end of each, refilled by HandleWaveStreamNotification.
struct Streamer
{
    IDirectSoundBuffer *buffer = NULL;
    TestEvent event;
    WaveSource source;
    DWORD notify_size = 0;
    DWORD buffer_size = 0;
    DWORD next_write = 0;
    int refills = 0;
    int skipped = 0;
    int failures = 0;
    std::atomic<bool> quit{false};
    std::thread thread;

    bool create(IDirectSound8 *ds, const WAVEFORMATEX &format)
    {
        // SoundManager::open_bgm.
        notify_size = format.nSamplesPerSec * format.nBlockAlign * 4 / 8;
        notify_size -= notify_size % format.nBlockAlign;
        buffer_size = notify_size * 8;
        // CSoundManager::CreateStreaming.
        DSBUFFERDESC desc;
        memset(&desc, 0, sizeof(desc));
        desc.dwSize = sizeof(desc);
        desc.dwFlags = DSBCAPS_CTRLPOSITIONNOTIFY | DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_GLOBALFOCUS |
                       DSBCAPS_CTRLVOLUME | DSBCAPS_LOCSOFTWARE;
        desc.dwBufferBytes = buffer_size;
        WAVEFORMATEX wfx = format;
        desc.lpwfxFormat = &wfx;
        if (FAILED(ds->CreateSoundBuffer(&desc, &buffer, NULL)))
        {
            return false;
        }
        IDirectSoundNotify *notify = NULL;
        if (FAILED(buffer->QueryInterface(IID_IDirectSoundNotify, (void **)&notify)))
        {
            return false;
        }
        DSBPOSITIONNOTIFY positions[8];
        for (DWORD i = 0; i < 8; i++)
        {
            positions[i].dwOffset = notify_size * i + notify_size - 1;
            positions[i].hEventNotify = &event;
        }
        HRESULT hr = notify->SetNotificationPositions(8, positions);
        notify->Release();
        return SUCCEEDED(hr);
    }

    // CSound::FillBufferWithSound(buffer, total != 0, 0), then Reset and
    // Play(DSBPLAY_LOOPING) at volume 0 (SoundManager's BGM command 2).
    void start()
    {
        void *p1;
        DWORD n1;
        buffer->Lock(0, buffer_size, &p1, &n1, NULL, NULL, 0);
        source.reset(false, 0);
        DWORD read = source.read((uint8_t *)p1, n1);
        while (read < n1)
        {
            source.reset(true, 0);
            read += source.read((uint8_t *)p1 + read, n1 - read);
        }
        buffer->Unlock(p1, n1, NULL, 0);
        next_write = 0;
        buffer->SetCurrentPosition(0);
        buffer->SetVolume(0);
        buffer->Play(0, 0, DSBPLAY_LOOPING);
    }

    // CStreamingSound::HandleWaveStreamNotification(TRUE).
    void handle()
    {
        DWORD play;
        DWORD write;
        buffer->GetCurrentPosition(&play, &write);
        if (next_write >= write - notify_size && next_write < write)
        {
            skipped++;
            return;
        }
        void *p1 = NULL;
        void *p2 = NULL;
        DWORD n1;
        DWORD n2;
        if (FAILED(buffer->Lock(next_write, notify_size, &p1, &n1, &p2, &n2, 0)) || p2 != NULL)
        {
            failures++;
            return;
        }
        DWORD read = source.read((uint8_t *)p1, n1);
        while (read < n1)
        {
            source.reset(true, 0);
            read += source.read((uint8_t *)p1 + read, n1 - read);
        }
        buffer->Unlock(p1, n1, NULL, 0);
        next_write = (next_write + n1) % buffer_size;
        refills++;
    }

    // SoundManager::bgm_thread_proc, with a quit flag for WM_QUIT.
    void run_thread()
    {
        thread = std::thread([this] {
            while (!quit)
            {
                if (event.wait(50))
                {
                    handle();
                    event.handled();
                }
            }
        });
    }

    void stop()
    {
        buffer->Stop();
        quit = true;
        thread.join();
        buffer->Release();
        buffer = NULL;
    }
};

// The byte of a looping track at play offset `offset` (past the end, from
// the loop point).
static uint32_t track_offset(uint64_t offset, uint32_t intro, uint32_t total)
{
    if (offset < total)
    {
        return (uint32_t)offset;
    }
    return intro + (uint32_t)((offset - total) % (total - intro));
}

// Streams a track of thbgm.dat until 10 seconds past its loop point, with
// the refilling on its own thread, and compares every output frame with
// the track.
static void test_bgm_stream(IDirectSound8 *ds, const std::string &bgm_path, const Track &track)
{
    printf("  %s: start %u, intro %u, total %u bytes (%.1f s, loop at %.1f s)\n", track.name.c_str(), track.start,
           track.intro, track.total, track.total / 176400.0, track.intro / 176400.0);
    Streamer stream;
    stream.source.file = fopen(bgm_path.c_str(), "rb");
    FILE *expected_file = fopen(bgm_path.c_str(), "rb");
    CHECK(stream.source.file != NULL && expected_file != NULL);
    if (stream.source.file == NULL || expected_file == NULL)
    {
        return;
    }
    stream.source.start = track.start;
    stream.source.intro = track.intro;
    stream.source.total = track.total;
    CHECK(stream.create(ds, track.format));
    stream.start();
    stream.run_thread();

    const uint32_t period = 512;
    const uint64_t total_frames = track.total / 4 + 10 * 44100;
    std::vector<int16_t> out(period * 2);
    std::vector<int16_t> expected(period * 2);
    uint64_t mismatch = UINT64_MAX;
    double energy = 0;
    auto start_time = std::chrono::steady_clock::now();
    for (uint64_t done = 0; done < total_frames; done += period)
    {
        port_dsound_mix(out.data(), period);
        // The expected frames, read separately from the file.
        for (uint32_t i = 0; i < period;)
        {
            uint32_t offset = track_offset((done + i) * 4, track.intro, track.total);
            uint32_t run = std::min<uint32_t>(period - i, (track.total - offset) / 4);
            fseek(expected_file, track.start + offset, SEEK_SET);
            if (fread(&expected[2 * i], 4, run, expected_file) != run)
            {
                break;
            }
            i += run;
        }
        for (uint32_t i = 0; i < period * 2; i++)
        {
            energy += (double)out[i] * out[i];
        }
        if (mismatch == UINT64_MAX && memcmp(out.data(), expected.data(), period * 4) != 0)
        {
            for (uint32_t i = 0; i < period; i++)
            {
                if (out[2 * i] != expected[2 * i] || out[2 * i + 1] != expected[2 * i + 1])
                {
                    mismatch = done + i;
                    fprintf(stderr, "  bgm: frame %llu is %d/%d, expected %d/%d\n", (unsigned long long)mismatch,
                            out[2 * i], out[2 * i + 1], expected[2 * i], expected[2 * i + 1]);
                    break;
                }
            }
        }
        // Lets the BGM thread finish refilling before mixing on, so the
        // result does not depend on scheduling (the real stream has 3.5 s
        // of slack).
        stream.event.wait_idle();
    }
    double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start_time).count();
    stream.stop();
    fclose(stream.source.file);
    fclose(expected_file);
    uint64_t expected_refills = total_frames * 4 / stream.notify_size;
    double rms = sqrt(energy / (2.0 * total_frames));
    printf("  %.1f s of audio (rms %.0f) in %.2f s: %d refills (expected about %llu), %d skipped, first mismatch "
           "%s\n",
           total_frames / 44100.0, rms, seconds, stream.refills, (unsigned long long)expected_refills,
           stream.skipped, mismatch == UINT64_MAX ? "none" : std::to_string(mismatch).c_str());
    CHECK(rms > 1000);
    CHECK(mismatch == UINT64_MAX);
    CHECK_EQ(stream.skipped, 0);
    CHECK_EQ(stream.failures, 0);
    CHECK(stream.refills >= (int)expected_refills - 1 && stream.refills <= (int)expected_refills + 1);
}

// ---------------------------------------------------------------------------
// Real-time test through SDL

// A synthetic track for the real-time stream: 1 s of intro, a 1.5 s loop,
// frames that are never silent.
static void synth_frame(uint32_t k, int16_t *left, int16_t *right)
{
    *left = (int16_t)(1 + k % 20000);
    *right = (int16_t)(-1 - (k * 3) % 20000);
}

static void test_realtime(const std::string &data_dir)
{
    // Refuse anything that could reach the speakers.
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0)
    {
        fprintf(stderr, "realtime: SDL audio init failed: %s\n", SDL_GetError());
        g_failures++;
        return;
    }
    const char *driver = SDL_GetCurrentAudioDriver();
    if (driver == NULL || (strcmp(driver, "disk") != 0 && strcmp(driver, "dummy") != 0))
    {
        fprintf(stderr, "realtime: refusing to run with SDL audio driver \"%s\"; set SDL_AUDIODRIVER=disk or dummy\n",
                driver != NULL ? driver : "(none)");
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        g_failures++;
        return;
    }
    const char *disk_file = getenv("SDL_DISKAUDIOFILE");
    bool verify = strcmp(driver, "disk") == 0 && disk_file != NULL;
    printf("  driver %s%s%s\n", driver, verify ? ", output " : "", verify ? disk_file : "");
    port_dsound_set_manual(false);

    const uint32_t intro_frames = 44100;
    const uint32_t total_frames = 44100 * 5 / 2;
    std::vector<int16_t> track(total_frames * 2);
    for (uint32_t k = 0; k < total_frames; k++)
    {
        synth_frame(k, &track[2 * k], &track[2 * k + 1]);
    }

    // Created on another thread, as the game's sound init thread does.
    IDirectSound8 *ds = NULL;
    IDirectSoundBuffer *silence = NULL;
    std::thread init([&ds, &silence] {
        CHECK_EQ(DirectSoundCreate8(NULL, &ds, NULL), DS_OK);
        if (ds == NULL)
        {
            return;
        }
        ds->SetCooperativeLevel(NULL, DSSCL_PRIORITY);
        // SoundManager::initialize's silent looping buffer.
        silence = make_buffer(ds, DSBCAPS_LOCSOFTWARE | DSBCAPS_GLOBALFOCUS, 2, 44100, 16, 0x8000);
        std::vector<uint8_t> zeros(0x8000);
        fill(silence, zeros.data(), 0x8000);
        silence->Play(0, 0, DSBPLAY_LOOPING);
    });
    init.join();
    if (ds == NULL)
    {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return;
    }

    // 5 s of the stream: past the loop point twice (2.5 s and 4 s), and
    // past the end of the 4-second buffer.
    Streamer stream;
    stream.source.memory = (const uint8_t *)track.data();
    stream.source.intro = intro_frames * 4;
    stream.source.total = total_frames * 4;
    CHECK(stream.create(ds, make_format(2, 44100, 16)));
    stream.run_thread();
    stream.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(5000));
    stream.stop();
    int refills = stream.refills;
    int skipped = stream.skipped;
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Then a looping tone at -6 dB for half a second, and an effect.
    const uint32_t tone_frames = 44100;
    std::vector<int16_t> tone(tone_frames * 2);
    for (uint32_t k = 0; k < tone_frames; k++)
    {
        tone[2 * k] = tone[2 * k + 1] = (int16_t)lrint(10000 * sin(2 * M_PI * 441 * k / 44100.0));
    }
    IDirectSoundBuffer *tone_buffer = make_buffer(ds, DSBCAPS_CTRLVOLUME, 2, 44100, 16, tone_frames * 4);
    fill(tone_buffer, tone.data(), tone_frames * 4);
    tone_buffer->SetVolume(-600);
    tone_buffer->Play(0, 0, DSBPLAY_LOOPING);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    tone_buffer->Stop();
    tone_buffer->Release();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    std::vector<uint8_t> effect = read_file(data_dir + "/se_ok00.wav");
    uint32_t effect_frames = 0;
    if (effect.size() > 44)
    {
        int32_t riff_size = *(int32_t *)(effect.data() + 4);
        int32_t chunk_size = 0;
        WAVEFORMATEX wfx = *(WAVEFORMATEX *)wav_chunk(effect.data() + 12, "fmt ", &chunk_size, riff_size - 12);
        uint8_t *samples = wav_chunk(effect.data() + 12, "data", &chunk_size, riff_size - 12);
        IDirectSoundBuffer *buffer = make_buffer(ds, DSBCAPS_CTRLVOLUME | DSBCAPS_CTRLPAN, wfx.nChannels,
                                                 wfx.nSamplesPerSec, wfx.wBitsPerSample, chunk_size);
        fill(buffer, samples, chunk_size);
        buffer->Play(0, 0, 0);
        effect_frames = chunk_size / (wfx.nChannels * wfx.wBitsPerSample / 8) * (44100 / wfx.nSamplesPerSec);
        std::this_thread::sleep_for(std::chrono::milliseconds(800));
        DWORD status = 1;
        buffer->GetStatus(&status);
        CHECK_EQ(status, 0);
        buffer->Release();
    }

    silence->Stop();
    silence->Release();
    ds->Release();
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    printf("  stream: %d refills, %d skipped\n", refills, skipped);
    CHECK(refills >= 8);
    CHECK_EQ(skipped, 0);
    if (!verify)
    {
        return;
    }

    // The file: silence, the stream sample for sample, silence, the tone,
    // silence, the effect.
    std::vector<uint8_t> raw = read_file(disk_file);
    const int16_t *pcm = (const int16_t *)raw.data();
    size_t frames = raw.size() / 4;
    size_t i = 0;
    while (i < frames && pcm[2 * i] == 0 && pcm[2 * i + 1] == 0)
    {
        i++;
    }
    size_t stream_start = i;
    size_t matched = 0;
    for (; i < frames; i++, matched++)
    {
        uint32_t k = track_offset((uint64_t)matched * 4, intro_frames * 4, total_frames * 4) / 4;
        if (pcm[2 * i] != track[2 * k] || pcm[2 * i + 1] != track[2 * k + 1])
        {
            break;
        }
    }
    bool clean_stop = i < frames && pcm[2 * i] == 0 && pcm[2 * i + 1] == 0;
    while (i < frames && pcm[2 * i] == 0 && pcm[2 * i + 1] == 0)
    {
        i++;
    }
    size_t tone_start = i;
    double energy = 0;
    size_t tone_length = 0;
    int peak = 0;
    for (; i < frames && i < tone_start + 44100 / 2 - 2048; i++, tone_length++)
    {
        energy += (double)pcm[2 * i] * pcm[2 * i];
        peak = std::max(peak, abs(pcm[2 * i]));
    }
    double tone_rms = tone_length != 0 ? sqrt(energy / tone_length) : 0;
    // Past the tone and the gap to the effect.
    size_t j = tone_start;
    while (j < frames && (pcm[2 * j] != 0 || pcm[2 * j + 1] != 0 || pcm[2 * j + 2] != 0 || pcm[2 * j + 3] != 0))
    {
        j++;
    }
    while (j < frames && pcm[2 * j] == 0 && pcm[2 * j + 1] == 0)
    {
        j++;
    }
    size_t effect_start = j;
    size_t effect_end = effect_start;
    for (size_t k = effect_start; k < frames && k < effect_start + effect_frames + 4096; k++)
    {
        if (pcm[2 * k] != 0 || pcm[2 * k + 1] != 0)
        {
            effect_end = k + 1;
        }
    }
    printf("  output %.2f s: stream from %.3f s, %zu frames (%.2f s) exact, %s; tone rms %.0f peak %d "
           "(expected %.0f / %.0f); effect %zu frames (expected about %u)\n",
           frames / 44100.0, stream_start / 44100.0, matched, matched / 44100.0,
           clean_stop ? "then silence" : "then garbage", tone_rms, peak, 10000 * db_gain(-600) / sqrt(2.0),
           10000 * db_gain(-600), effect_end - effect_start, effect_frames);
    CHECK(matched >= 44100 * 42 / 10);
    CHECK(clean_stop);
    CHECK(fabs(tone_rms - 10000 * db_gain(-600) / sqrt(2.0)) < 50);
    CHECK(abs(peak - (int)lrintf(10000 * db_gain(-600))) <= 2);
    if (effect_frames != 0)
    {
        CHECK(effect_end - effect_start + 64 >= effect_frames / 2);
        CHECK(effect_end - effect_start <= effect_frames + 2);
    }
}

int main(int argc, char **argv)
{
    std::string data_dir;
    std::string bgm_path;
    std::string track_name = "th16_01.wav";
    bool realtime = false;
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--data") == 0 && i + 1 < argc)
        {
            data_dir = argv[++i];
        }
        else if (strcmp(argv[i], "--bgm") == 0 && i + 1 < argc)
        {
            bgm_path = argv[++i];
        }
        else if (strcmp(argv[i], "--track") == 0 && i + 1 < argc)
        {
            track_name = argv[++i];
        }
        else if (strcmp(argv[i], "--realtime") == 0)
        {
            realtime = true;
        }
        else
        {
            fprintf(stderr, "usage: %s [--data DIR] [--bgm thbgm.dat] [--track NAME] [--realtime]\n", argv[0]);
            return 2;
        }
    }

    port_dsound_set_manual(true);
    IDirectSound8 *ds = NULL;
    if (DirectSoundCreate8(NULL, &ds, NULL) != DS_OK || ds == NULL)
    {
        fprintf(stderr, "DirectSoundCreate8 failed\n");
        return 1;
    }

    struct
    {
        const char *name;
        void (*run)(IDirectSound8 *);
    } tests[] = {
        {"create", test_create},
        {"looping", test_looping},
        {"one-shot", test_one_shot},
        {"volume/pan/frequency", test_volume_pan},
        {"lock", test_lock},
        {"notifications", test_notifications},
        {"duplicate", test_duplicate},
        {"clamp", test_clamp},
    };
    for (auto &test : tests)
    {
        int before = g_failures;
        test.run(ds);
        printf("%-22s %s\n", test.name, g_failures == before ? "ok" : "FAILED");
    }

    if (!data_dir.empty())
    {
        int before = g_failures;
        printf("sound effects (%s):\n", data_dir.c_str());
        const char *files[] = {"se_plst00.wav", "se_ok00.wav", "se_select00.wav", "se_power1.wav",
                               "se_item00.wav", "se_pause.wav", "se_extend.wav", "se_gun00.wav"};
        for (const char *file : files)
        {
            test_sound_effect(ds, data_dir, file, -1100);
        }
        printf("%-22s %s\n", "sound effects", g_failures == before ? "ok" : "FAILED");
    }

    if (!data_dir.empty() && !bgm_path.empty())
    {
        int before = g_failures;
        std::vector<Track> tracks = read_tracks(data_dir + "/thbgm.fmt");
        const Track *track = NULL;
        for (const Track &t : tracks)
        {
            if (t.name == track_name)
            {
                track = &t;
            }
        }
        printf("bgm stream (%s, %zu tracks in thbgm.fmt):\n", bgm_path.c_str(), tracks.size());
        CHECK(track != NULL);
        if (track != NULL)
        {
            test_bgm_stream(ds, bgm_path, *track);
        }
        printf("%-22s %s\n", "bgm stream", g_failures == before ? "ok" : "FAILED");
    }
    ds->Release();

    if (realtime)
    {
        int before = g_failures;
        printf("realtime:\n");
        test_realtime(data_dir);
        printf("%-22s %s\n", "realtime", g_failures == before ? "ok" : "FAILED");
    }

    printf("%d checks, %d failed\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
