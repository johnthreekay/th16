// DirectSound 8 as a software mixer feeding one SDL audio device.
//
// What the game uses (SoundManager.cpp, DSUtil.cpp):
// - DirectSoundCreate8, SetCooperativeLevel(DSSCL_PRIORITY), and the primary
//   buffer, only to SetFormat it to 44.1 kHz 16-bit stereo.
// - A secondary buffer per sound effect: the data chunk of a .wav file from
//   th16.dat (22.05 kHz, 8 or 16 bits, mono or stereo), with
//   DSBCAPS_CTRLVOLUME | DSBCAPS_CTRLPAN, filled once through Lock/Unlock.
//   Effects sharing a .wav file get DuplicateSoundBuffer copies. Before each
//   play the game calls Stop, SetCurrentPosition(0), SetPan, SetVolume and
//   Play (one-shot, or DSBPLAY_LOOPING for a few); GetStatus tells which are
//   playing when the game pauses.
// - A silent 0x8000-byte buffer that loops for the whole session.
// - The BGM stream (DSUtil.cpp's CStreamingSound): a looping buffer of 8
//   chunks of half a second, DSBCAPS_CTRLPOSITIONNOTIFY | DSBCAPS_CTRLVOLUME,
//   with a notification at the last byte of each chunk, all on one
//   auto-reset event. The BGM thread waits on that event, then
//   (HandleWaveStreamNotification) reads the play and write cursors, Locks
//   the next chunk and refills it from thbgm.dat, looping at the track's
//   loop point. Fades go through SetVolume; seeking and track switches
//   release the buffer and create it again.
//
// Every secondary buffer is mixed into one stereo float accumulator at 44.1
// kHz (linear interpolation for other rates; a 44.1 kHz buffer at full
// volume comes out bit-exact), clamped to 16 bits and handed to SDL. Volume
// and pan are DirectSound's: hundredths of a decibel of attenuation,
// DSBVOLUME_MIN is silence, a positive pan attenuates the left channel and a
// negative one the right.
//
// Threads: one mutex guards the buffer list and every buffer's state and
// data. The SDL callback holds it while mixing; the game's threads (main,
// sound init, BGM) hold it inside each method call. Lock hands out a copy of
// the region and Unlock writes it back under the mutex, so the game never
// writes memory the mixer is reading. Notifications are signalled with the
// Win32 layer's SetEvent from the SDL thread while the mutex is held, so a
// buffer that was stopped or released never signals afterwards (the game
// closes the event right after stopping the stream).
#include <SDL.h>

#include <math.h>

#include <algorithm>
#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

#include <dsound.h>

#include "dsound_sdl.h"
#include "port_stub.h"

namespace
{

// Interface ids, compared by value (DSUtil.cpp defines IID_IDirectSoundNotify
// for the game; this file does not depend on it).
const GUID kIidUnknown = {0x00000000, 0x0000, 0x0000, {0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
const GUID kIidDirectSound = {0x279afa83, 0x4981, 0x11ce, {0xa5, 0x21, 0x00, 0x20, 0xaf, 0x0b, 0xe5, 0x60}};
const GUID kIidDirectSound8 = {0xc50a7e93, 0xf395, 0x4834, {0x9e, 0xf6, 0x7f, 0xa9, 0x9d, 0xe5, 0x09, 0x66}};
const GUID kIidDirectSoundBuffer = {0x279afa85, 0x4981, 0x11ce, {0xa5, 0x21, 0x00, 0x20, 0xaf, 0x0b, 0xe5, 0x60}};
const GUID kIidDirectSoundBuffer8 = {0x6825a449, 0x7524, 0x4d82, {0x92, 0x0f, 0x50, 0xe3, 0x6a, 0xb3, 0xab, 0x1e}};
const GUID kIidDirectSoundNotify = {0xb0210783, 0x89cd, 0x11d0, {0xaf, 0x08, 0x00, 0xa0, 0xc9, 0x25, 0xcd, 0x16}};

// The DX7 DSBUFFERDESC (no guid3DAlgorithm), which DirectSound also accepts.
const DWORD kBufferDescSizeDx7 = offsetof(DSBUFFERDESC, guid3DAlgorithm);

// IUnknown's reference counting; objects can be released from any thread.
template <typename Interface> struct ComObject : public Interface
{
    std::atomic<uint32_t> ref_count{1};

    uint32_t AddRef() override
    {
        return ++ref_count;
    }
    uint32_t Release() override
    {
        uint32_t count = --ref_count;
        if (count == 0)
        {
            delete this;
        }
        return count;
    }
};

class SecondaryBuffer;

struct Mixer
{
    // Guards the buffer list and the state and data of every buffer.
    std::mutex lock;
    std::vector<SecondaryBuffer *> buffers;
    std::vector<float> accum;
    // Frames per mix call: how far past the play cursor the next call may
    // read, which is the write cursor's lead.
    uint32_t period = 512;
    // The primary buffer's format (recorded; the mix format stays
    // PORT_DSOUND_RATE 16-bit stereo) and volume.
    WAVEFORMATEX primary_format = {WAVE_FORMAT_PCM, 2, PORT_DSOUND_RATE, PORT_DSOUND_RATE * 4, 4, 16, 0};
    float primary_gain = 1.0f;

    // The SDL device, opened by the first IDirectSound8 and closed with the
    // last. Guarded by device_lock, which the SDL callback never takes.
    std::mutex device_lock;
    uint32_t device_users = 0;
    SDL_AudioDeviceID device = 0;
    bool manual = false;
};

// Never destroyed: the SDL callback may still run while static destructors
// do if the game exits without releasing DirectSound.
Mixer &mixer()
{
    static Mixer *instance = new Mixer();
    return *instance;
}

// DirectSound attenuation in hundredths of a decibel as a linear gain.
float attenuation_gain(LONG hundredths_db)
{
    if (hundredths_db <= DSBVOLUME_MIN)
    {
        return 0.0f;
    }
    if (hundredths_db >= 0)
    {
        return 1.0f;
    }
    return powf(10.0f, hundredths_db / 2000.0f);
}

bool supported_format(const WAVEFORMATEX *format)
{
    return format->wFormatTag == WAVE_FORMAT_PCM && (format->nChannels == 1 || format->nChannels == 2) &&
           (format->wBitsPerSample == 8 || format->wBitsPerSample == 16) &&
           format->nSamplesPerSec >= DSBFREQUENCY_MIN && format->nSamplesPerSec <= DSBFREQUENCY_MAX;
}

// One frame of a buffer as floats on the 16-bit scale. 8-bit PCM is
// unsigned, 16-bit is signed little-endian; mono plays on both channels.
template <int Bits, int Channels> inline void read_frame(const uint8_t *data, uint32_t frame, float *left, float *right)
{
    if (Bits == 8)
    {
        const uint8_t *p = data + (size_t)frame * Channels;
        *left = (p[0] - 128) * 256.0f;
        *right = Channels == 2 ? (p[1] - 128) * 256.0f : *left;
    }
    else
    {
        const uint8_t *p = data + (size_t)frame * 2 * Channels;
        *left = (float)(int16_t)(p[0] | p[1] << 8);
        *right = Channels == 2 ? (float)(int16_t)(p[2] | p[3] << 8) : *left;
    }
}

class SecondaryBuffer : public ComObject<IDirectSoundBuffer>
{
  public:
    // Registers the buffer with the mixer.
    SecondaryBuffer(DWORD flags, const WAVEFORMATEX &format, std::shared_ptr<std::vector<uint8_t>> data)
        : flags_(flags), format_(format), data_(std::move(data))
    {
        format_.cbSize = 0;
        block_align_ = format_.nChannels * format_.wBitsPerSample / 8;
        format_.nBlockAlign = (WORD)block_align_;
        format_.nAvgBytesPerSec = format_.nSamplesPerSec * block_align_;
        frame_count_ = (uint32_t)(data_->size() / block_align_);
        set_frequency_locked(format_.nSamplesPerSec);
        Mixer &m = mixer();
        std::lock_guard<std::mutex> guard(m.lock);
        m.buffers.push_back(this);
    }

    ~SecondaryBuffer() override
    {
        Mixer &m = mixer();
        std::lock_guard<std::mutex> guard(m.lock);
        m.buffers.erase(std::remove(m.buffers.begin(), m.buffers.end(), this), m.buffers.end());
        for (PendingLock &lock : locks_)
        {
            delete[] lock.staging;
        }
    }

    // A buffer sharing this one's memory (DuplicateSoundBuffer): stopped at
    // position 0, with this one's volume, pan and frequency.
    SecondaryBuffer *duplicate()
    {
        SecondaryBuffer *copy = new SecondaryBuffer(flags_, format_, data_);
        std::lock_guard<std::mutex> guard(mixer().lock);
        copy->volume_ = volume_;
        copy->pan_ = pan_;
        copy->update_gains_locked();
        copy->set_frequency_locked(frequency_);
        return copy;
    }

    HRESULT QueryInterface(REFIID riid, void **ppvObject) override;

    HRESULT GetCaps(LPDSBCAPS caps) override
    {
        if (caps == NULL || caps->dwSize < sizeof(DSBCAPS))
        {
            return DSERR_INVALIDPARAM;
        }
        caps->dwFlags = (flags_ & ~DSBCAPS_LOCDEFER) | DSBCAPS_LOCSOFTWARE;
        caps->dwBufferBytes = (DWORD)data_->size();
        caps->dwUnlockTransferRate = 0;
        caps->dwPlayCpuOverhead = 0;
        return DS_OK;
    }

    HRESULT GetCurrentPosition(LPDWORD play_cursor, LPDWORD write_cursor) override
    {
        std::lock_guard<std::mutex> guard(mixer().lock);
        if (play_cursor != NULL)
        {
            *play_cursor = position_ * block_align_;
        }
        if (write_cursor != NULL)
        {
            *write_cursor = write_cursor_locked();
        }
        return DS_OK;
    }

    HRESULT GetFormat(LPWAVEFORMATEX format, DWORD size_allocated, LPDWORD size_written) override
    {
        if (format == NULL && size_written == NULL)
        {
            return DSERR_INVALIDPARAM;
        }
        if (format == NULL)
        {
            *size_written = sizeof(WAVEFORMATEX);
            return DS_OK;
        }
        DWORD size = std::min<DWORD>(size_allocated, sizeof(WAVEFORMATEX));
        memcpy(format, &format_, size);
        if (size_written != NULL)
        {
            *size_written = size;
        }
        return DS_OK;
    }

    HRESULT GetVolume(LPLONG volume) override
    {
        if (volume == NULL)
        {
            return DSERR_INVALIDPARAM;
        }
        if (!(flags_ & DSBCAPS_CTRLVOLUME))
        {
            return DSERR_CONTROLUNAVAIL;
        }
        std::lock_guard<std::mutex> guard(mixer().lock);
        *volume = volume_;
        return DS_OK;
    }

    HRESULT GetPan(LPLONG pan) override
    {
        if (pan == NULL)
        {
            return DSERR_INVALIDPARAM;
        }
        if (!(flags_ & DSBCAPS_CTRLPAN))
        {
            return DSERR_CONTROLUNAVAIL;
        }
        std::lock_guard<std::mutex> guard(mixer().lock);
        *pan = pan_;
        return DS_OK;
    }

    HRESULT GetFrequency(LPDWORD frequency) override
    {
        if (frequency == NULL)
        {
            return DSERR_INVALIDPARAM;
        }
        if (!(flags_ & DSBCAPS_CTRLFREQUENCY))
        {
            return DSERR_CONTROLUNAVAIL;
        }
        std::lock_guard<std::mutex> guard(mixer().lock);
        *frequency = frequency_;
        return DS_OK;
    }

    HRESULT GetStatus(LPDWORD status) override
    {
        if (status == NULL)
        {
            return DSERR_INVALIDPARAM;
        }
        std::lock_guard<std::mutex> guard(mixer().lock);
        *status = 0;
        if (playing_)
        {
            *status = DSBSTATUS_PLAYING | (looping_ ? DSBSTATUS_LOOPING : 0);
        }
        return DS_OK;
    }

    // Hands out a copy of the region (two parts when it wraps past the end
    // and the caller takes the second part); Unlock writes it back.
    HRESULT Lock(DWORD offset, DWORD bytes, LPVOID *ptr1, LPDWORD bytes1, LPVOID *ptr2, LPDWORD bytes2,
                 DWORD flags) override
    {
        if (ptr1 == NULL || bytes1 == NULL)
        {
            return DSERR_INVALIDPARAM;
        }
        std::lock_guard<std::mutex> guard(mixer().lock);
        DWORD size = (DWORD)data_->size();
        if (flags & DSBLOCK_FROMWRITECURSOR)
        {
            offset = write_cursor_locked();
        }
        if (flags & DSBLOCK_ENTIREBUFFER)
        {
            bytes = size;
        }
        if (offset >= size || bytes == 0 || bytes > size)
        {
            return DSERR_INVALIDPARAM;
        }
        PendingLock lock;
        lock.offset = offset;
        lock.bytes1 = std::min(bytes, size - offset);
        lock.bytes2 = ptr2 != NULL ? bytes - lock.bytes1 : 0;
        lock.staging = new uint8_t[lock.bytes1 + lock.bytes2];
        memcpy(lock.staging, data_->data() + offset, lock.bytes1);
        memcpy(lock.staging + lock.bytes1, data_->data(), lock.bytes2);
        locks_.push_back(lock);
        *ptr1 = lock.staging;
        *bytes1 = lock.bytes1;
        if (ptr2 != NULL)
        {
            *ptr2 = lock.bytes2 != 0 ? lock.staging + lock.bytes1 : NULL;
        }
        if (bytes2 != NULL)
        {
            *bytes2 = lock.bytes2;
        }
        return DS_OK;
    }

    HRESULT Play(DWORD reserved, DWORD priority, DWORD flags) override
    {
        if (reserved != 0)
        {
            return DSERR_INVALIDPARAM;
        }
        std::lock_guard<std::mutex> guard(mixer().lock);
        looping_ = (flags & DSBPLAY_LOOPING) != 0;
        playing_ = true;
        return DS_OK;
    }

    HRESULT SetCurrentPosition(DWORD position) override
    {
        if (position >= data_->size())
        {
            return DSERR_INVALIDPARAM;
        }
        std::lock_guard<std::mutex> guard(mixer().lock);
        position_ = position / block_align_;
        if (position_ >= frame_count_)
        {
            position_ = 0;
        }
        fraction_ = 0;
        return DS_OK;
    }

    HRESULT SetFormat(LPCWAVEFORMATEX format) override
    {
        return DSERR_INVALIDCALL;
    }

    HRESULT SetVolume(LONG volume) override
    {
        if (!(flags_ & DSBCAPS_CTRLVOLUME))
        {
            return DSERR_CONTROLUNAVAIL;
        }
        if (volume < DSBVOLUME_MIN || volume > DSBVOLUME_MAX)
        {
            return DSERR_INVALIDPARAM;
        }
        std::lock_guard<std::mutex> guard(mixer().lock);
        volume_ = volume;
        update_gains_locked();
        return DS_OK;
    }

    HRESULT SetPan(LONG pan) override
    {
        if (!(flags_ & DSBCAPS_CTRLPAN))
        {
            return DSERR_CONTROLUNAVAIL;
        }
        if (pan < DSBPAN_LEFT || pan > DSBPAN_RIGHT)
        {
            return DSERR_INVALIDPARAM;
        }
        std::lock_guard<std::mutex> guard(mixer().lock);
        pan_ = pan;
        update_gains_locked();
        return DS_OK;
    }

    HRESULT SetFrequency(DWORD frequency) override
    {
        if (!(flags_ & DSBCAPS_CTRLFREQUENCY))
        {
            return DSERR_CONTROLUNAVAIL;
        }
        if (frequency == DSBFREQUENCY_ORIGINAL)
        {
            frequency = format_.nSamplesPerSec;
        }
        if (frequency < DSBFREQUENCY_MIN || frequency > DSBFREQUENCY_MAX)
        {
            return DSERR_INVALIDPARAM;
        }
        std::lock_guard<std::mutex> guard(mixer().lock);
        set_frequency_locked(frequency);
        return DS_OK;
    }

    HRESULT Stop() override
    {
        std::lock_guard<std::mutex> guard(mixer().lock);
        if (playing_)
        {
            playing_ = false;
            signal_stop_locked();
        }
        return DS_OK;
    }

    HRESULT Unlock(LPVOID ptr1, DWORD bytes1, LPVOID ptr2, DWORD bytes2) override
    {
        std::lock_guard<std::mutex> guard(mixer().lock);
        auto it = std::find_if(locks_.begin(), locks_.end(),
                               [ptr1](const PendingLock &lock) { return lock.staging == ptr1; });
        if (ptr1 == NULL || it == locks_.end())
        {
            return DSERR_INVALIDPARAM;
        }
        memcpy(data_->data() + it->offset, it->staging, std::min(bytes1, it->bytes1));
        if (ptr2 != NULL)
        {
            memcpy(data_->data(), it->staging + it->bytes1, std::min(bytes2, it->bytes2));
        }
        delete[] it->staging;
        locks_.erase(it);
        return DS_OK;
    }

    HRESULT Restore() override
    {
        // Software buffers are never lost.
        return DS_OK;
    }

    // IDirectSoundNotify::SetNotificationPositions on this buffer.
    HRESULT set_notifications(DWORD count, LPCDSBPOSITIONNOTIFY positions)
    {
        if (count != 0 && positions == NULL)
        {
            return DSERR_INVALIDPARAM;
        }
        std::lock_guard<std::mutex> guard(mixer().lock);
        if (playing_)
        {
            return DSERR_INVALIDCALL;
        }
        for (DWORD i = 0; i < count; i++)
        {
            if (positions[i].dwOffset != DSBPN_OFFSETSTOP && positions[i].dwOffset >= data_->size())
            {
                return DSERR_INVALIDPARAM;
            }
        }
        notifies_.assign(positions, positions + count);
        return DS_OK;
    }

    // Adds the next `frames` output frames of this buffer to accum and
    // advances it. Called with the mixer lock held.
    void mix_locked(float *accum, uint32_t frames)
    {
        if (!playing_ || frame_count_ == 0)
        {
            return;
        }
        uint32_t start = position_;
        bool ended = false;
        uint64_t consumed;
        switch (format_.wBitsPerSample * 10 + format_.nChannels)
        {
        case 81:
            consumed = mix_frames<8, 1>(accum, frames, &ended);
            break;
        case 82:
            consumed = mix_frames<8, 2>(accum, frames, &ended);
            break;
        case 161:
            consumed = mix_frames<16, 1>(accum, frames, &ended);
            break;
        default:
            consumed = mix_frames<16, 2>(accum, frames, &ended);
            break;
        }
        signal_passed_locked(start, consumed);
        if (ended)
        {
            // A one-shot buffer stops at its end and rewinds.
            playing_ = false;
            position_ = 0;
            fraction_ = 0;
            signal_stop_locked();
        }
    }

  private:
    struct PendingLock
    {
        uint8_t *staging;
        DWORD offset;
        DWORD bytes1;
        DWORD bytes2;
    };

    template <int Bits, int Channels> uint64_t mix_frames(float *accum, uint32_t frames, bool *ended)
    {
        const uint8_t *data = data_->data();
        const uint32_t step_whole = (uint32_t)(step_ >> 32);
        const uint32_t step_fraction = (uint32_t)step_;
        const float gain_left = gain_left_;
        const float gain_right = gain_right_;
        uint64_t consumed = 0;
        for (uint32_t i = 0; i < frames; i++)
        {
            float left;
            float right;
            read_frame<Bits, Channels>(data, position_, &left, &right);
            if (fraction_ != 0)
            {
                uint32_t next = position_ + 1;
                if (next >= frame_count_)
                {
                    next = looping_ ? 0 : position_;
                }
                float next_left;
                float next_right;
                read_frame<Bits, Channels>(data, next, &next_left, &next_right);
                float t = fraction_ * (1.0f / 4294967296.0f);
                left += (next_left - left) * t;
                right += (next_right - right) * t;
            }
            accum[2 * i] += left * gain_left;
            accum[2 * i + 1] += right * gain_right;

            uint64_t fraction = (uint64_t)fraction_ + step_fraction;
            fraction_ = (uint32_t)fraction;
            uint32_t advance = step_whole + (uint32_t)(fraction >> 32);
            consumed += advance;
            position_ += advance;
            if (position_ >= frame_count_)
            {
                if (!looping_)
                {
                    consumed -= position_ - frame_count_;
                    *ended = true;
                    break;
                }
                position_ %= frame_count_;
            }
        }
        return consumed;
    }

    DWORD write_cursor_locked()
    {
        if (!playing_)
        {
            return position_ * block_align_;
        }
        // The next mix call reads up to a period ahead (plus one frame for
        // interpolation); only data past that is safe to write.
        uint64_t lead = (((uint64_t)mixer().period * step_) >> 32) + 1;
        return (DWORD)((position_ + lead) % frame_count_) * block_align_;
    }

    void set_frequency_locked(DWORD frequency)
    {
        frequency_ = frequency;
        step_ = ((uint64_t)frequency << 32) / PORT_DSOUND_RATE;
    }

    void update_gains_locked()
    {
        float volume = attenuation_gain(volume_);
        gain_left_ = volume * (pan_ > 0 ? attenuation_gain(-pan_) : 1.0f);
        gain_right_ = volume * (pan_ < 0 ? attenuation_gain(pan_) : 1.0f);
    }

    // Signals the notifications whose byte the mixer just consumed, in
    // `consumed` frames from frame `start` on.
    void signal_passed_locked(uint32_t start, uint64_t consumed)
    {
        if (consumed == 0)
        {
            return;
        }
        for (const DSBPOSITIONNOTIFY &notify : notifies_)
        {
            if (notify.dwOffset == DSBPN_OFFSETSTOP)
            {
                continue;
            }
            uint32_t frame = std::min(notify.dwOffset / block_align_, frame_count_ - 1);
            uint32_t ahead = frame >= start ? frame - start : frame + frame_count_ - start;
            if (ahead < consumed)
            {
                SetEvent(notify.hEventNotify);
            }
        }
    }

    void signal_stop_locked()
    {
        for (const DSBPOSITIONNOTIFY &notify : notifies_)
        {
            if (notify.dwOffset == DSBPN_OFFSETSTOP)
            {
                SetEvent(notify.hEventNotify);
            }
        }
    }

    const DWORD flags_;
    WAVEFORMATEX format_;
    uint32_t block_align_;
    uint32_t frame_count_;
    // Shared with duplicates.
    const std::shared_ptr<std::vector<uint8_t>> data_;

    // Everything below is guarded by the mixer lock.
    bool playing_ = false;
    bool looping_ = false;
    // Play cursor in frames, and the fraction of a frame (1/2^32 units).
    uint32_t position_ = 0;
    uint32_t fraction_ = 0;
    // Buffer frames per output frame, 32.32 fixed point.
    uint64_t step_ = 0;
    DWORD frequency_ = 0;
    LONG volume_ = DSBVOLUME_MAX;
    LONG pan_ = DSBPAN_CENTER;
    float gain_left_ = 1.0f;
    float gain_right_ = 1.0f;
    std::vector<DSBPOSITIONNOTIFY> notifies_;
    std::vector<PendingLock> locks_;
};

// The notification interface of a secondary buffer (QueryInterface with
// IID_IDirectSoundNotify). Holds a reference to the buffer.
class Notify : public ComObject<IDirectSoundNotify>
{
  public:
    explicit Notify(SecondaryBuffer *buffer) : buffer_(buffer)
    {
        buffer_->AddRef();
    }
    ~Notify() override
    {
        buffer_->Release();
    }

    HRESULT QueryInterface(REFIID riid, void **ppvObject) override
    {
        if (ppvObject == NULL)
        {
            return E_POINTER;
        }
        if (riid == kIidUnknown || riid == kIidDirectSoundNotify)
        {
            AddRef();
            *ppvObject = static_cast<IDirectSoundNotify *>(this);
            return S_OK;
        }
        return buffer_->QueryInterface(riid, ppvObject);
    }

    HRESULT SetNotificationPositions(DWORD count, LPCDSBPOSITIONNOTIFY positions) override
    {
        return buffer_->set_notifications(count, positions);
    }

  private:
    SecondaryBuffer *buffer_;
};

HRESULT SecondaryBuffer::QueryInterface(REFIID riid, void **ppvObject)
{
    if (ppvObject == NULL)
    {
        return E_POINTER;
    }
    if (riid == kIidUnknown || riid == kIidDirectSoundBuffer || riid == kIidDirectSoundBuffer8)
    {
        AddRef();
        *ppvObject = static_cast<IDirectSoundBuffer *>(this);
        return S_OK;
    }
    if (riid == kIidDirectSoundNotify && (flags_ & DSBCAPS_CTRLPOSITIONNOTIFY))
    {
        *ppvObject = static_cast<IDirectSoundNotify *>(new Notify(this));
        return S_OK;
    }
    *ppvObject = NULL;
    return E_NOINTERFACE;
}

// The primary buffer. The mix runs at a fixed format, so SetFormat only
// records the format (the game asks for exactly the mix format) and the
// volume scales the whole mix.
class PrimaryBuffer : public ComObject<IDirectSoundBuffer>
{
  public:
    explicit PrimaryBuffer(DWORD flags) : flags_(flags)
    {
    }

    HRESULT QueryInterface(REFIID riid, void **ppvObject) override
    {
        if (ppvObject == NULL)
        {
            return E_POINTER;
        }
        if (riid == kIidUnknown || riid == kIidDirectSoundBuffer)
        {
            AddRef();
            *ppvObject = static_cast<IDirectSoundBuffer *>(this);
            return S_OK;
        }
        *ppvObject = NULL;
        return E_NOINTERFACE;
    }

    HRESULT GetCaps(LPDSBCAPS caps) override
    {
        if (caps == NULL || caps->dwSize < sizeof(DSBCAPS))
        {
            return DSERR_INVALIDPARAM;
        }
        std::lock_guard<std::mutex> guard(mixer().lock);
        caps->dwFlags = flags_ | DSBCAPS_LOCSOFTWARE;
        caps->dwBufferBytes = mixer().period * 4;
        caps->dwUnlockTransferRate = 0;
        caps->dwPlayCpuOverhead = 0;
        return DS_OK;
    }

    HRESULT GetCurrentPosition(LPDWORD play_cursor, LPDWORD write_cursor) override
    {
        if (play_cursor != NULL)
        {
            *play_cursor = 0;
        }
        if (write_cursor != NULL)
        {
            *write_cursor = 0;
        }
        return DS_OK;
    }

    HRESULT GetFormat(LPWAVEFORMATEX format, DWORD size_allocated, LPDWORD size_written) override
    {
        if (format == NULL && size_written == NULL)
        {
            return DSERR_INVALIDPARAM;
        }
        if (format == NULL)
        {
            *size_written = sizeof(WAVEFORMATEX);
            return DS_OK;
        }
        std::lock_guard<std::mutex> guard(mixer().lock);
        DWORD size = std::min<DWORD>(size_allocated, sizeof(WAVEFORMATEX));
        memcpy(format, &mixer().primary_format, size);
        if (size_written != NULL)
        {
            *size_written = size;
        }
        return DS_OK;
    }

    HRESULT GetVolume(LPLONG volume) override
    {
        if (volume == NULL)
        {
            return DSERR_INVALIDPARAM;
        }
        if (!(flags_ & DSBCAPS_CTRLVOLUME))
        {
            return DSERR_CONTROLUNAVAIL;
        }
        *volume = volume_;
        return DS_OK;
    }

    HRESULT GetPan(LPLONG pan) override
    {
        return DSERR_CONTROLUNAVAIL;
    }

    HRESULT GetFrequency(LPDWORD frequency) override
    {
        return DSERR_CONTROLUNAVAIL;
    }

    HRESULT GetStatus(LPDWORD status) override
    {
        if (status == NULL)
        {
            return DSERR_INVALIDPARAM;
        }
        *status = DSBSTATUS_PLAYING | DSBSTATUS_LOOPING;
        return DS_OK;
    }

    HRESULT Lock(DWORD offset, DWORD bytes, LPVOID *ptr1, LPDWORD bytes1, LPVOID *ptr2, LPDWORD bytes2,
                 DWORD flags) override
    {
        // Writing the primary buffer needs DSSCL_WRITEPRIMARY, which the
        // game does not use.
        return DSERR_PRIOLEVELNEEDED;
    }

    HRESULT Play(DWORD reserved, DWORD priority, DWORD flags) override
    {
        return (flags & DSBPLAY_LOOPING) ? DS_OK : DSERR_INVALIDPARAM;
    }

    HRESULT SetCurrentPosition(DWORD position) override
    {
        return DSERR_INVALIDCALL;
    }

    HRESULT SetFormat(LPCWAVEFORMATEX format) override
    {
        if (format == NULL)
        {
            return DSERR_INVALIDPARAM;
        }
        if (!supported_format(format))
        {
            return DSERR_BADFORMAT;
        }
        if (format->nSamplesPerSec != PORT_DSOUND_RATE || format->nChannels != 2 || format->wBitsPerSample != 16)
        {
            fprintf(stderr, "[th16-port] audio: primary format %u Hz %u-bit %u ch requested, mixing at %d Hz 16-bit "
                            "stereo\n",
                    (unsigned)format->nSamplesPerSec, format->wBitsPerSample, format->nChannels, PORT_DSOUND_RATE);
        }
        std::lock_guard<std::mutex> guard(mixer().lock);
        mixer().primary_format = *format;
        mixer().primary_format.cbSize = 0;
        return DS_OK;
    }

    HRESULT SetVolume(LONG volume) override
    {
        if (!(flags_ & DSBCAPS_CTRLVOLUME))
        {
            return DSERR_CONTROLUNAVAIL;
        }
        if (volume < DSBVOLUME_MIN || volume > DSBVOLUME_MAX)
        {
            return DSERR_INVALIDPARAM;
        }
        volume_ = volume;
        std::lock_guard<std::mutex> guard(mixer().lock);
        mixer().primary_gain = attenuation_gain(volume);
        return DS_OK;
    }

    HRESULT SetPan(LONG pan) override
    {
        return DSERR_CONTROLUNAVAIL;
    }

    HRESULT SetFrequency(DWORD frequency) override
    {
        return DSERR_CONTROLUNAVAIL;
    }

    HRESULT Stop() override
    {
        return DS_OK;
    }

    HRESULT Unlock(LPVOID ptr1, DWORD bytes1, LPVOID ptr2, DWORD bytes2) override
    {
        return DSERR_INVALIDCALL;
    }

    HRESULT Restore() override
    {
        return DS_OK;
    }

  private:
    const DWORD flags_;
    LONG volume_ = DSBVOLUME_MAX;
};

void SDLCALL audio_callback(void *userdata, Uint8 *stream, int len)
{
    port_dsound_mix((int16_t *)stream, (uint32_t)len / 4);
}

// Opens the SDL device for the first IDirectSound8 (none in manual mode).
HRESULT acquire_device()
{
    Mixer &m = mixer();
    std::lock_guard<std::mutex> guard(m.device_lock);
    if (m.device_users > 0 || m.manual)
    {
        m.device_users++;
        return DS_OK;
    }
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0)
    {
        fprintf(stderr, "[th16-port] audio: SDL_InitSubSystem failed: %s\n", SDL_GetError());
        return DSERR_NODRIVER;
    }
    SDL_AudioSpec want;
    SDL_AudioSpec have;
    memset(&want, 0, sizeof(want));
    memset(&have, 0, sizeof(have));
    want.freq = PORT_DSOUND_RATE;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 512;
    want.callback = audio_callback;
    // No allowed changes: SDL converts to whatever the hardware takes.
    m.device = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (m.device == 0)
    {
        fprintf(stderr, "[th16-port] audio: SDL_OpenAudioDevice failed: %s\n", SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return DSERR_NODRIVER;
    }
    {
        std::lock_guard<std::mutex> mix_guard(m.lock);
        m.period = have.samples != 0 ? have.samples : 512;
    }
    fprintf(stderr, "[th16-port] audio: SDL driver %s, %d Hz, %u-frame periods\n", SDL_GetCurrentAudioDriver(),
            have.freq, (unsigned)have.samples);
    m.device_users++;
    SDL_PauseAudioDevice(m.device, 0);
    return DS_OK;
}

void release_device()
{
    Mixer &m = mixer();
    std::lock_guard<std::mutex> guard(m.device_lock);
    if (--m.device_users > 0 || m.device == 0)
    {
        return;
    }
    // Waits for the callback to finish; the mixer lock is not held here.
    SDL_CloseAudioDevice(m.device);
    m.device = 0;
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

class Device : public ComObject<IDirectSound8>
{
  public:
    ~Device() override
    {
        release_device();
    }

    HRESULT QueryInterface(REFIID riid, void **ppvObject) override
    {
        if (ppvObject == NULL)
        {
            return E_POINTER;
        }
        if (riid == kIidUnknown || riid == kIidDirectSound || riid == kIidDirectSound8)
        {
            AddRef();
            *ppvObject = static_cast<IDirectSound8 *>(this);
            return S_OK;
        }
        *ppvObject = NULL;
        return E_NOINTERFACE;
    }

    HRESULT CreateSoundBuffer(LPCDSBUFFERDESC desc, IDirectSoundBuffer **buffer, LPUNKNOWN outer) override
    {
        if (buffer == NULL)
        {
            return DSERR_INVALIDPARAM;
        }
        *buffer = NULL;
        if (desc == NULL || outer != NULL || (desc->dwSize != sizeof(DSBUFFERDESC) && desc->dwSize != kBufferDescSizeDx7))
        {
            return DSERR_INVALIDPARAM;
        }
        if (desc->dwFlags & DSBCAPS_PRIMARYBUFFER)
        {
            if (desc->dwBufferBytes != 0 || desc->lpwfxFormat != NULL)
            {
                return DSERR_INVALIDPARAM;
            }
            *buffer = new PrimaryBuffer(desc->dwFlags);
            return DS_OK;
        }
        if (desc->lpwfxFormat == NULL)
        {
            return DSERR_INVALIDPARAM;
        }
        if (!supported_format(desc->lpwfxFormat))
        {
            return DSERR_BADFORMAT;
        }
        if (desc->dwFlags & (DSBCAPS_CTRL3D | DSBCAPS_CTRLFX))
        {
            // Neither is used by the game.
            PORT_UNIMPLEMENTED();
            return DSERR_CONTROLUNAVAIL;
        }
        DWORD block_align = desc->lpwfxFormat->nChannels * desc->lpwfxFormat->wBitsPerSample / 8;
        if (desc->dwBufferBytes < DSBSIZE_MIN || desc->dwBufferBytes > DSBSIZE_MAX ||
            desc->dwBufferBytes < block_align)
        {
            return DSERR_INVALIDPARAM;
        }
        // Starts out silent.
        uint8_t silence = desc->lpwfxFormat->wBitsPerSample == 8 ? 0x80 : 0;
        auto data = std::make_shared<std::vector<uint8_t>>(desc->dwBufferBytes, silence);
        *buffer = new SecondaryBuffer(desc->dwFlags, *desc->lpwfxFormat, std::move(data));
        return DS_OK;
    }

    HRESULT DuplicateSoundBuffer(IDirectSoundBuffer *original, IDirectSoundBuffer **duplicate) override
    {
        if (duplicate == NULL)
        {
            return DSERR_INVALIDPARAM;
        }
        *duplicate = NULL;
        if (original == NULL)
        {
            return DSERR_INVALIDPARAM;
        }
        SecondaryBuffer *source = NULL;
        {
            Mixer &m = mixer();
            std::lock_guard<std::mutex> guard(m.lock);
            for (SecondaryBuffer *candidate : m.buffers)
            {
                if (static_cast<IDirectSoundBuffer *>(candidate) == original)
                {
                    source = candidate;
                    break;
                }
            }
        }
        if (source == NULL)
        {
            // The primary buffer cannot be duplicated.
            return DSERR_INVALIDCALL;
        }
        *duplicate = source->duplicate();
        return DS_OK;
    }

    HRESULT SetCooperativeLevel(HWND hwnd, DWORD level) override
    {
        return DS_OK;
    }

    HRESULT Compact() override
    {
        return DS_OK;
    }

    HRESULT Initialize(LPCGUID device) override
    {
        return DS_OK;
    }
};

} // namespace

void port_dsound_mix(int16_t *out, uint32_t frames)
{
    Mixer &m = mixer();
    std::lock_guard<std::mutex> guard(m.lock);
    m.period = frames;
    m.accum.assign((size_t)frames * 2, 0.0f);
    for (SecondaryBuffer *buffer : m.buffers)
    {
        buffer->mix_locked(m.accum.data(), frames);
    }
    const float gain = m.primary_gain;
    for (size_t i = 0; i < (size_t)frames * 2; i++)
    {
        float value = m.accum[i] * gain;
        if (value > 32767.0f)
        {
            value = 32767.0f;
        }
        else if (value < -32768.0f)
        {
            value = -32768.0f;
        }
        out[i] = (int16_t)lrintf(value);
    }
}

void port_dsound_set_manual(bool manual)
{
    Mixer &m = mixer();
    std::lock_guard<std::mutex> guard(m.device_lock);
    m.manual = manual;
}

extern "C" HRESULT DirectSoundCreate8(LPCGUID pcGuidDevice, LPDIRECTSOUND8 *ppDS8, LPUNKNOWN pUnkOuter)
{
    if (ppDS8 == NULL)
    {
        return DSERR_INVALIDPARAM;
    }
    *ppDS8 = NULL;
    if (pUnkOuter != NULL)
    {
        return DSERR_INVALIDPARAM;
    }
    HRESULT hr = acquire_device();
    if (FAILED(hr))
    {
        return hr;
    }
    *ppDS8 = new Device();
    return DS_OK;
}
