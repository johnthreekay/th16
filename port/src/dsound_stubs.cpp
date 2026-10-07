// DirectSound 8: sound effects (a buffer per effect, duplicated when two
// effects share a .wav file) and the BGM stream (a looping buffer refilled from
// thbgm.dat at notification positions, DSUtil.cpp's CStreamingSound, driven
// by SoundManager's BGM thread waiting on the notification event).
//
// Stubs for now: DirectSoundCreate8 fails and the game runs silent. The
// stub classes are the skeleton for the SDL audio implementation, which
// mixes the buffers itself (16-bit PCM, volume in hundredths of a decibel,
// DSBVOLUME_MIN = -10000) and signals the notification events.
#include <dsound.h>

#include "port_stub.h"

struct StubDirectSoundNotify : public PortComObject<IDirectSoundNotify>
{
    HRESULT SetNotificationPositions(DWORD dwPositionNotifies, LPCDSBPOSITIONNOTIFY pcPositionNotifies) override
    {
        PORT_UNIMPLEMENTED();
        return DS_OK;
    }
};

struct StubDirectSoundBuffer : public PortComObject<IDirectSoundBuffer>
{
    HRESULT GetCaps(LPDSBCAPS pDSBufferCaps) override
    {
        PORT_UNIMPLEMENTED();
        return DSERR_UNSUPPORTED;
    }
    HRESULT GetCurrentPosition(LPDWORD pdwCurrentPlayCursor, LPDWORD pdwCurrentWriteCursor) override
    {
        PORT_UNIMPLEMENTED();
        if (pdwCurrentPlayCursor != NULL)
        {
            *pdwCurrentPlayCursor = 0;
        }
        if (pdwCurrentWriteCursor != NULL)
        {
            *pdwCurrentWriteCursor = 0;
        }
        return DS_OK;
    }
    HRESULT GetFormat(LPWAVEFORMATEX pwfxFormat, DWORD dwSizeAllocated, LPDWORD pdwSizeWritten) override
    {
        PORT_UNIMPLEMENTED();
        return DSERR_UNSUPPORTED;
    }
    HRESULT GetVolume(LPLONG plVolume) override
    {
        PORT_UNIMPLEMENTED();
        *plVolume = DSBVOLUME_MAX;
        return DS_OK;
    }
    HRESULT GetPan(LPLONG plPan) override
    {
        PORT_UNIMPLEMENTED();
        *plPan = DSBPAN_CENTER;
        return DS_OK;
    }
    HRESULT GetFrequency(LPDWORD pdwFrequency) override
    {
        PORT_UNIMPLEMENTED();
        return DSERR_UNSUPPORTED;
    }
    HRESULT GetStatus(LPDWORD pdwStatus) override
    {
        PORT_UNIMPLEMENTED();
        *pdwStatus = 0;
        return DS_OK;
    }
    HRESULT Lock(DWORD dwOffset, DWORD dwBytes, LPVOID *ppvAudioPtr1, LPDWORD pdwAudioBytes1, LPVOID *ppvAudioPtr2,
                 LPDWORD pdwAudioBytes2, DWORD dwFlags) override
    {
        PORT_UNIMPLEMENTED();
        return DSERR_INVALIDCALL;
    }
    HRESULT Play(DWORD dwReserved1, DWORD dwPriority, DWORD dwFlags) override
    {
        PORT_UNIMPLEMENTED();
        return DS_OK;
    }
    HRESULT SetCurrentPosition(DWORD dwNewPosition) override
    {
        PORT_UNIMPLEMENTED();
        return DS_OK;
    }
    HRESULT SetFormat(LPCWAVEFORMATEX pcfxFormat) override
    {
        PORT_UNIMPLEMENTED();
        return DS_OK;
    }
    HRESULT SetVolume(LONG lVolume) override
    {
        PORT_UNIMPLEMENTED();
        return DS_OK;
    }
    HRESULT SetPan(LONG lPan) override
    {
        PORT_UNIMPLEMENTED();
        return DS_OK;
    }
    HRESULT SetFrequency(DWORD dwFrequency) override
    {
        PORT_UNIMPLEMENTED();
        return DS_OK;
    }
    HRESULT Stop() override
    {
        PORT_UNIMPLEMENTED();
        return DS_OK;
    }
    HRESULT Unlock(LPVOID pvAudioPtr1, DWORD dwAudioBytes1, LPVOID pvAudioPtr2, DWORD dwAudioBytes2) override
    {
        PORT_UNIMPLEMENTED();
        return DSERR_INVALIDCALL;
    }
    HRESULT Restore() override
    {
        PORT_UNIMPLEMENTED();
        return DS_OK;
    }
};

struct StubDirectSound8 : public PortComObject<IDirectSound8>
{
    HRESULT CreateSoundBuffer(LPCDSBUFFERDESC pcDSBufferDesc, IDirectSoundBuffer **ppDSBuffer,
                              LPUNKNOWN pUnkOuter) override
    {
        PORT_UNIMPLEMENTED();
        *ppDSBuffer = NULL;
        return DSERR_UNSUPPORTED;
    }
    HRESULT DuplicateSoundBuffer(IDirectSoundBuffer *pDSBufferOriginal,
                                 IDirectSoundBuffer **ppDSBufferDuplicate) override
    {
        PORT_UNIMPLEMENTED();
        *ppDSBufferDuplicate = NULL;
        return DSERR_UNSUPPORTED;
    }
    HRESULT SetCooperativeLevel(HWND hwnd, DWORD dwLevel) override
    {
        PORT_UNIMPLEMENTED();
        return DS_OK;
    }
    HRESULT Compact() override
    {
        PORT_UNIMPLEMENTED();
        return DS_OK;
    }
    HRESULT Initialize(LPCGUID pcGuidDevice) override
    {
        PORT_UNIMPLEMENTED();
        return DS_OK;
    }
};

extern "C" HRESULT DirectSoundCreate8(LPCGUID pcGuidDevice, LPDIRECTSOUND8 *ppDS8, LPUNKNOWN pUnkOuter)
{
    PORT_UNIMPLEMENTED();
    *ppDS8 = NULL;
    return DSERR_NODRIVER;
}

// Keep the skeleton compiled while DirectSoundCreate8 does not use it.
IDirectSound8 *port_new_stub_dsound()
{
    return new StubDirectSound8();
}

IDirectSoundBuffer *port_new_stub_dsound_buffer()
{
    return new StubDirectSoundBuffer();
}

IDirectSoundNotify *port_new_stub_dsound_notify()
{
    return new StubDirectSoundNotify();
}
