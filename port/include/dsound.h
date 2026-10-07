// Stand-in for the DirectX SDK's dsound.h in the portable build: the
// buffer description structures, constants with the SDK's values, and the
// interfaces the game and its copy of the SDK's DSUtil sample use, as
// abstract classes. port/src/dsound_*.cpp implements them.
#pragma once

#include <windows.h>
#include <mmreg.h>

#define _FACDS 0x878
#define MAKE_DSHRESULT(code) MAKE_HRESULT(1, _FACDS, code)

#define DS_OK S_OK
#define DSERR_ALLOCATED MAKE_DSHRESULT(10)
#define DSERR_CONTROLUNAVAIL MAKE_DSHRESULT(30)
#define DSERR_INVALIDPARAM E_INVALIDARG
#define DSERR_INVALIDCALL MAKE_DSHRESULT(50)
#define DSERR_GENERIC E_FAIL
#define DSERR_PRIOLEVELNEEDED MAKE_DSHRESULT(70)
#define DSERR_OUTOFMEMORY E_OUTOFMEMORY
#define DSERR_BADFORMAT MAKE_DSHRESULT(100)
#define DSERR_UNSUPPORTED E_NOTIMPL
#define DSERR_NODRIVER MAKE_DSHRESULT(120)
#define DSERR_BUFFERLOST MAKE_DSHRESULT(150)

#define DSSCL_NORMAL 0x00000001
#define DSSCL_PRIORITY 0x00000002
#define DSSCL_EXCLUSIVE 0x00000003
#define DSSCL_WRITEPRIMARY 0x00000004

#define DSBCAPS_PRIMARYBUFFER 0x00000001
#define DSBCAPS_STATIC 0x00000002
#define DSBCAPS_LOCHARDWARE 0x00000004
#define DSBCAPS_LOCSOFTWARE 0x00000008
#define DSBCAPS_CTRL3D 0x00000010
#define DSBCAPS_CTRLFREQUENCY 0x00000020
#define DSBCAPS_CTRLPAN 0x00000040
#define DSBCAPS_CTRLVOLUME 0x00000080
#define DSBCAPS_CTRLPOSITIONNOTIFY 0x00000100
#define DSBCAPS_CTRLFX 0x00000200
#define DSBCAPS_STICKYFOCUS 0x00004000
#define DSBCAPS_GLOBALFOCUS 0x00008000
#define DSBCAPS_GETCURRENTPOSITION2 0x00010000
#define DSBCAPS_MUTE3DATMAXDISTANCE 0x00020000
#define DSBCAPS_LOCDEFER 0x00040000

#define DSBPLAY_LOOPING 0x00000001

#define DSBSTATUS_PLAYING 0x00000001
#define DSBSTATUS_BUFFERLOST 0x00000002
#define DSBSTATUS_LOOPING 0x00000004
#define DSBSTATUS_LOCHARDWARE 0x00000008
#define DSBSTATUS_LOCSOFTWARE 0x00000010
#define DSBSTATUS_TERMINATED 0x00000020

#define DSBLOCK_FROMWRITECURSOR 0x00000001
#define DSBLOCK_ENTIREBUFFER 0x00000002

#define DSBVOLUME_MIN -10000
#define DSBVOLUME_MAX 0
#define DSBPAN_LEFT -10000
#define DSBPAN_CENTER 0
#define DSBPAN_RIGHT 10000
#define DSBFREQUENCY_ORIGINAL 0
#define DSBFREQUENCY_MIN 100
#define DSBFREQUENCY_MAX 200000
#define DSBSIZE_MIN 4
#define DSBSIZE_MAX 0x0FFFFFFF

#define DSBPN_OFFSETSTOP 0xFFFFFFFF

// 0x24 bytes on x86 (40 where pointers are 8 bytes).
typedef struct _DSBUFFERDESC
{
    DWORD dwSize;
    DWORD dwFlags;
    DWORD dwBufferBytes;
    DWORD dwReserved;
    LPWAVEFORMATEX lpwfxFormat;
    GUID guid3DAlgorithm;
} DSBUFFERDESC, *LPDSBUFFERDESC;
typedef const DSBUFFERDESC *LPCDSBUFFERDESC;

typedef struct _DSBPOSITIONNOTIFY
{
    DWORD dwOffset;
    HANDLE hEventNotify;
} DSBPOSITIONNOTIFY, *LPDSBPOSITIONNOTIFY;
typedef const DSBPOSITIONNOTIFY *LPCDSBPOSITIONNOTIFY;

typedef struct _DSBCAPS
{
    DWORD dwSize;
    DWORD dwFlags;
    DWORD dwBufferBytes;
    DWORD dwUnlockTransferRate;
    DWORD dwPlayCpuOverhead;
} DSBCAPS, *LPDSBCAPS;

struct IDirectSoundBuffer : public IUnknown
{
    virtual HRESULT GetCaps(LPDSBCAPS pDSBufferCaps) = 0;
    virtual HRESULT GetCurrentPosition(LPDWORD pdwCurrentPlayCursor, LPDWORD pdwCurrentWriteCursor) = 0;
    virtual HRESULT GetFormat(LPWAVEFORMATEX pwfxFormat, DWORD dwSizeAllocated, LPDWORD pdwSizeWritten) = 0;
    virtual HRESULT GetVolume(LPLONG plVolume) = 0;
    virtual HRESULT GetPan(LPLONG plPan) = 0;
    virtual HRESULT GetFrequency(LPDWORD pdwFrequency) = 0;
    virtual HRESULT GetStatus(LPDWORD pdwStatus) = 0;
    virtual HRESULT Lock(DWORD dwOffset, DWORD dwBytes, LPVOID *ppvAudioPtr1, LPDWORD pdwAudioBytes1,
                         LPVOID *ppvAudioPtr2, LPDWORD pdwAudioBytes2, DWORD dwFlags) = 0;
    virtual HRESULT Play(DWORD dwReserved1, DWORD dwPriority, DWORD dwFlags) = 0;
    virtual HRESULT SetCurrentPosition(DWORD dwNewPosition) = 0;
    virtual HRESULT SetFormat(LPCWAVEFORMATEX pcfxFormat) = 0;
    virtual HRESULT SetVolume(LONG lVolume) = 0;
    virtual HRESULT SetPan(LONG lPan) = 0;
    virtual HRESULT SetFrequency(DWORD dwFrequency) = 0;
    virtual HRESULT Stop() = 0;
    virtual HRESULT Unlock(LPVOID pvAudioPtr1, DWORD dwAudioBytes1, LPVOID pvAudioPtr2, DWORD dwAudioBytes2) = 0;
    virtual HRESULT Restore() = 0;
};

struct IDirectSoundNotify : public IUnknown
{
    virtual HRESULT SetNotificationPositions(DWORD dwPositionNotifies, LPCDSBPOSITIONNOTIFY pcPositionNotifies) = 0;
};

struct IDirectSound8 : public IUnknown
{
    virtual HRESULT CreateSoundBuffer(LPCDSBUFFERDESC pcDSBufferDesc, IDirectSoundBuffer **ppDSBuffer,
                                      LPUNKNOWN pUnkOuter) = 0;
    virtual HRESULT DuplicateSoundBuffer(IDirectSoundBuffer *pDSBufferOriginal,
                                         IDirectSoundBuffer **ppDSBufferDuplicate) = 0;
    virtual HRESULT SetCooperativeLevel(HWND hwnd, DWORD dwLevel) = 0;
    virtual HRESULT Compact() = 0;
    virtual HRESULT Initialize(LPCGUID pcGuidDevice) = 0;
};

typedef IDirectSound8 IDirectSound;
typedef IDirectSound8 *LPDIRECTSOUND8, *LPDIRECTSOUND;
typedef IDirectSoundBuffer *LPDIRECTSOUNDBUFFER;
typedef IDirectSoundBuffer IDirectSoundBuffer8;
typedef IDirectSoundBuffer *LPDIRECTSOUNDBUFFER8;
typedef IDirectSoundNotify *LPDIRECTSOUNDNOTIFY, *LPDIRECTSOUNDNOTIFY8;

// DSUtil.cpp defines this one itself.
DEFINE_GUID(IID_IDirectSoundNotify, 0xb0210783, 0x89cd, 0x11d0, 0xaf, 0x8, 0x0, 0xa0, 0xc9, 0x25, 0xcd, 0x16);

extern "C" HRESULT DirectSoundCreate8(LPCGUID pcGuidDevice, LPDIRECTSOUND8 *ppDS8, LPUNKNOWN pUnkOuter);
