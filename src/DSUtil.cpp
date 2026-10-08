// The DirectSound sample framework (DSUtil.cpp of the DirectX SDK samples),
// as ZUN adapted it to stream thbgm.dat. TH06's copy (zwave.cpp in the TH06
// decomp) is still close to the sample.
#include <dsound.h>

#include <stdlib.h>
#include <string.h>

#include "CriticalSections.h"
#include "SoundManager.h"

double LTCG_VECTORCALL get_runtime();

static_assert(sizeof(CWaveFile) == 0xa0, "CWaveFile layout");
static_assert(offsetof(CWaveFile, m_dwSize) == 0x2c, "CWaveFile layout");
static_assert(offsetof(CWaveFile, m_dwFlags) == 0x78, "CWaveFile layout");
static_assert(offsetof(CWaveFile, m_file) == 0x8c, "CWaveFile layout");
static_assert(sizeof(CSound) == 0x80, "CSound layout");
static_assert(offsetof(CSound, m_start_time) == 0x30, "CSound layout");
static_assert(offsetof(CSound, m_playing) == 0x50, "CSound layout");
static_assert(offsetof(CSound, m_manager) == 0x7c, "CSound layout");
static_assert(sizeof(CStreamingSound) == 0xa0, "CStreamingSound layout");
static_assert(offsetof(CStreamingSound, m_dwNotifySize) == 0x94, "CStreamingSound layout");
static_assert(offsetof(SoundManager, bgm_file_offset) == 0x5670, "SoundManager layout");
static_assert(sizeof(DSBUFFERDESC) == sizeof(((CSound *)0)->m_desc), "CSound layout");

#define SAFE_RELEASE(p) \
    { \
        if (p) \
        { \
            (p)->Release(); \
            (p) = NULL; \
        } \
    }
#define SAFE_DELETE(p) \
    { \
        if (p) \
        { \
            delete (p); \
            (p) = NULL; \
        } \
    }
#define SAFE_DELETE_ARRAY(p) \
    { \
        if (p) \
        { \
            delete[] (p); \
            (p) = NULL; \
        } \
    }

// dsound.h's interface id, defined here (rather than taken from dxguid.lib)
// so that it can be annotated.
// GLOBAL: TH16 0x48b80c
extern "C" const GUID IID_IDirectSoundNotify = {0xb0210783, 0x89cd, 0x11d0, {0xaf, 0x8, 0x0, 0xa0, 0xc9, 0x25, 0xcd, 0x16}};

// Debug output, empty in the release build (TH06: utils::DebugPrint2).
// Declared with no named parameter: as `(const char *fmt, ...)`, LTCG drops
// the one-argument call in CWaveFile::open_file, which the original keeps.
// FUNCTION: TH16 0x471d90
void dsutil_debug_log(...)
{
}

// FUNCTION: TH16 0x470320
HARNESS_CALLED HRESULT CSoundManager::CreateStreaming(CStreamingSound **ppStreamingSound, const char *strWaveFileName,
                                                     DWORD dwCreationFlags, GUID guid3DAlgorithm, DWORD dwNotifyCount,
                                                     DWORD dwNotifySize, HANDLE hNotifyEvent, ThBgmFormat *track)
{
    HRESULT hr;

    if (m_pDS == NULL)
    {
        return CO_E_NOTINITIALIZED;
    }

    LPDIRECTSOUNDBUFFER pDSBuffer = NULL;
    DWORD dwDSBufferSize;
    CWaveFile *pWaveFile = NULL;
    DSBPOSITIONNOTIFY *aPosNotify = NULL;
    LPDIRECTSOUNDNOTIFY pDSNotify = NULL;

    pWaveFile = new CWaveFile();
    if (pWaveFile->Open(strWaveFileName, track) != S_OK)
    {
        // SAFE_DELETE(pWaveFile) as LTCG inlined it; ours would call an
        // out-of-line scalar deleting destructor here.
        pWaveFile->~CWaveFile();
        operator delete(pWaveFile, sizeof(CWaveFile));
        return E_FAIL;
    }

    dwDSBufferSize = dwNotifySize * dwNotifyCount;

    DSBUFFERDESC dsbd;
    ZeroMemory(&dsbd, sizeof(DSBUFFERDESC));
    dsbd.dwSize = sizeof(DSBUFFERDESC);
    dsbd.dwFlags = dwCreationFlags | DSBCAPS_CTRLPOSITIONNOTIFY | DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_GLOBALFOCUS |
                   DSBCAPS_CTRLVOLUME | DSBCAPS_LOCSOFTWARE;
    dsbd.dwBufferBytes = dwDSBufferSize;
    dsbd.guid3DAlgorithm = guid3DAlgorithm;
    dsbd.lpwfxFormat = &pWaveFile->m_track->format;

    if (FAILED(hr = ((IDirectSound8 *)m_pDS)->CreateSoundBuffer(&dsbd, &pDSBuffer, NULL)))
    {
        return E_FAIL;
    }

    if (FAILED(hr = pDSBuffer->QueryInterface(IID_IDirectSoundNotify, (VOID **)&pDSNotify)))
    {
        return E_FAIL;
    }

    aPosNotify = new DSBPOSITIONNOTIFY[dwNotifyCount];
    if (aPosNotify == NULL)
    {
        return E_OUTOFMEMORY;
    }

    for (DWORD i = 0; i < dwNotifyCount; i++)
    {
        aPosNotify[i].dwOffset = (dwNotifySize * i) + dwNotifySize - 1;
        aPosNotify[i].hEventNotify = hNotifyEvent;
    }

    if (FAILED(hr = pDSNotify->SetNotificationPositions(dwNotifyCount, aPosNotify)))
    {
        SAFE_RELEASE(pDSNotify);
        SAFE_DELETE(aPosNotify);
        return E_FAIL;
    }

    SAFE_RELEASE(pDSNotify);
    SAFE_DELETE(aPosNotify);

    *ppStreamingSound = new CStreamingSound(pDSBuffer, dwDSBufferSize, pWaveFile, dwNotifySize);
    *(DSBUFFERDESC *)(*ppStreamingSound)->m_desc = dsbd;
    (*ppStreamingSound)->m_manager = this;
    (*ppStreamingSound)->m_hNotifyEvent = hNotifyEvent;
    (*ppStreamingSound)->m_refilling = FALSE;

    return S_OK;
}

// FUNCTION: TH16 0x470680
HARNESS_CALLED HRESULT CSoundManager::CreateStreamingFromMemory(CStreamingSound **ppStreamingSound, BYTE *pbData,
                                                               ULONG ulDataSize, ThBgmFormat *track,
                                                               DWORD dwCreationFlags, GUID guid3DAlgorithm,
                                                               DWORD dwNotifyCount, DWORD dwNotifySize,
                                                               HANDLE hNotifyEvent)
{
    HRESULT hr;

    if (m_pDS == NULL)
    {
        return CO_E_NOTINITIALIZED;
    }

    LPDIRECTSOUNDBUFFER pDSBuffer = NULL;
    DWORD dwDSBufferSize;
    CWaveFile *pWaveFile = NULL;
    DSBPOSITIONNOTIFY *aPosNotify = NULL;
    LPDIRECTSOUNDNOTIFY pDSNotify = NULL;

    pWaveFile = new CWaveFile();
    pWaveFile->OpenFromMemory(pbData, ulDataSize, track);

    dwDSBufferSize = dwNotifySize * dwNotifyCount;

    DSBUFFERDESC dsbd;
    ZeroMemory(&dsbd, sizeof(DSBUFFERDESC));
    dsbd.dwSize = sizeof(DSBUFFERDESC);
    dsbd.dwFlags = dwCreationFlags | DSBCAPS_CTRLPOSITIONNOTIFY | DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_GLOBALFOCUS |
                   DSBCAPS_CTRLVOLUME | DSBCAPS_LOCSOFTWARE;
    dsbd.dwBufferBytes = dwDSBufferSize;
    dsbd.guid3DAlgorithm = guid3DAlgorithm;
    dsbd.lpwfxFormat = &pWaveFile->m_track->format;

    if (FAILED(hr = ((IDirectSound8 *)m_pDS)->CreateSoundBuffer(&dsbd, &pDSBuffer, NULL)))
    {
        return E_FAIL;
    }

    if (FAILED(hr = pDSBuffer->QueryInterface(IID_IDirectSoundNotify, (VOID **)&pDSNotify)))
    {
        return E_FAIL;
    }

    aPosNotify = new DSBPOSITIONNOTIFY[dwNotifyCount];
    if (aPosNotify == NULL)
    {
        return E_OUTOFMEMORY;
    }

    for (DWORD i = 0; i < dwNotifyCount; i++)
    {
        aPosNotify[i].dwOffset = (dwNotifySize * i) + dwNotifySize - 1;
        aPosNotify[i].hEventNotify = hNotifyEvent;
    }

    if (FAILED(hr = pDSNotify->SetNotificationPositions(dwNotifyCount, aPosNotify)))
    {
        SAFE_RELEASE(pDSNotify);
        SAFE_DELETE(aPosNotify);
        return E_FAIL;
    }

    SAFE_RELEASE(pDSNotify);
    SAFE_DELETE(aPosNotify);

    *ppStreamingSound = new CStreamingSound(pDSBuffer, dwDSBufferSize, pWaveFile, dwNotifySize);
    *(DSBUFFERDESC *)(*ppStreamingSound)->m_desc = dsbd;
    (*ppStreamingSound)->m_manager = this;
    (*ppStreamingSound)->m_hNotifyEvent = hNotifyEvent;
    (*ppStreamingSound)->m_refilling = FALSE;

    return S_OK;
}

// CSound::RestoreBuffer, which LTCG inlined into FillBufferWithSound and
// the constructor.
static __forceinline HRESULT restore_buffer(LPDIRECTSOUNDBUFFER pDSB, BOOL *pbWasRestored)
{
    HRESULT hr;

    if (pDSB == NULL)
    {
        return CO_E_NOTINITIALIZED;
    }
    if (pbWasRestored)
    {
        *pbWasRestored = FALSE;
    }

    DWORD dwStatus;
    if (FAILED(hr = pDSB->GetStatus(&dwStatus)))
    {
        return hr;
    }

    if (dwStatus & DSBSTATUS_BUFFERLOST)
    {
        do
        {
            hr = pDSB->Restore();
            if (hr == DSERR_BUFFERLOST)
            {
                Sleep(10);
            }
        } while (hr = pDSB->Restore());

        if (pbWasRestored != NULL)
        {
            *pbWasRestored = TRUE;
        }

        return S_OK;
    }
    else
    {
        return S_FALSE;
    }
}

// ResetFile, inlined into the constructor through fill_buffer_inline.
__forceinline HRESULT CWaveFile::reset_file_inline(bool loop, DWORD offset)
{
    if (m_bIsReadingFromMemory)
    {
        m_pbDataCur = m_pbData;
        if (m_track->total_size > 0)
        {
            m_ulDataSize = m_track->total_size;
        }
        if (loop && m_track->intro_size > 0)
        {
            m_pbDataCur = m_pbData + m_track->intro_size;
        }
    }
    else
    {
        if (m_file == INVALID_HANDLE_VALUE || m_file == NULL)
        {
            return CO_E_NOTINITIALIZED;
        }

        if (loop && m_track->intro_size > 0)
        {
            SetFilePointer(m_file, m_track->start_offset + m_track->intro_size + g_SoundManager.bgm_file_offset,
                           NULL, FILE_BEGIN);
            m_ck.cksize = m_track->total_size - m_track->intro_size;
            return S_OK;
        }

        if (offset >= m_track->total_size)
        {
            offset += m_track->intro_size - m_track->total_size;
        }
        SetFilePointer(m_file, m_track->start_offset + g_SoundManager.bgm_file_offset + offset, NULL, FILE_BEGIN);
        m_ck.cksize = m_track->total_size - offset;
    }
    return S_OK;
}

// FillBufferWithSound as inlined into the constructor.
__forceinline HRESULT CSound::fill_buffer_inline(LPDIRECTSOUNDBUFFER pDSB, BOOL bRepeatWavIfBufferLarger, DWORD offset)
{
    HRESULT hr;
    VOID *pDSLockedBuffer = NULL;
    DWORD dwDSLockedBufferSize = 0;
    DWORD dwWavDataRead = 0;

    if (pDSB == NULL)
    {
        return CO_E_NOTINITIALIZED;
    }

    if (FAILED(hr = restore_buffer(pDSB, NULL)))
    {
        return hr;
    }

    if (FAILED(hr = pDSB->Lock(0, m_dwDSBufferSize, &pDSLockedBuffer, &dwDSLockedBufferSize, NULL, NULL, 0L)))
    {
        return hr;
    }

    m_pWaveFile->reset_file_inline(false, offset);

    if (FAILED(hr = m_pWaveFile->Read((BYTE *)pDSLockedBuffer, dwDSLockedBufferSize, &dwWavDataRead)))
    {
        return hr;
    }

    if (dwWavDataRead == 0)
    {
        FillMemory((BYTE *)pDSLockedBuffer, dwDSLockedBufferSize,
                   (BYTE)(m_pWaveFile->m_track->format.wBitsPerSample == 8 ? 128 : 0));
    }
    else if (dwWavDataRead < dwDSLockedBufferSize)
    {
        if (bRepeatWavIfBufferLarger)
        {
            DWORD dwReadSoFar = dwWavDataRead;
            while (dwReadSoFar < dwDSLockedBufferSize)
            {
                if (FAILED(hr = m_pWaveFile->reset_file_inline(true, 0)))
                {
                    return hr;
                }

                hr = m_pWaveFile->Read((BYTE *)pDSLockedBuffer + dwReadSoFar, dwDSLockedBufferSize - dwReadSoFar,
                                       &dwWavDataRead);
                if (FAILED(hr))
                {
                    return hr;
                }

                dwReadSoFar += dwWavDataRead;
            }
        }
        else
        {
            FillMemory((BYTE *)pDSLockedBuffer + dwWavDataRead, dwDSLockedBufferSize - dwWavDataRead,
                       (BYTE)(m_pWaveFile->m_track->format.wBitsPerSample == 8 ? 128 : 0));
        }
    }

    pDSB->Unlock(pDSLockedBuffer, dwDSLockedBufferSize, NULL, 0);

    return S_OK;
}

// FUNCTION: TH16 0x470250
HARNESS_CALLED HRESULT CSoundManager::SetPrimaryBufferFormat(DWORD dwPrimaryChannels, DWORD dwPrimaryFreq,
                                                            DWORD dwPrimaryBitRate)
{
    HRESULT hr;
    LPDIRECTSOUNDBUFFER pDSBPrimary = NULL;

    if (m_pDS == NULL)
    {
        return CO_E_NOTINITIALIZED;
    }

    DSBUFFERDESC dsbd;
    ZeroMemory(&dsbd, sizeof(DSBUFFERDESC));
    dsbd.dwSize = sizeof(DSBUFFERDESC);
    dsbd.dwFlags = DSBCAPS_PRIMARYBUFFER;
    dsbd.dwBufferBytes = 0;
    dsbd.lpwfxFormat = NULL;

    if (FAILED(hr = m_pDS->CreateSoundBuffer(&dsbd, &pDSBPrimary, NULL)))
    {
        return hr;
    }

    WAVEFORMATEX wfx;
    ZeroMemory(&wfx, sizeof(WAVEFORMATEX));
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = (WORD)dwPrimaryChannels;
    wfx.nSamplesPerSec = dwPrimaryFreq;
    wfx.wBitsPerSample = (WORD)dwPrimaryBitRate;
    wfx.nBlockAlign = wfx.wBitsPerSample / 8 * wfx.nChannels;
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;

    if (FAILED(hr = pDSBPrimary->SetFormat(&wfx)))
    {
        return hr;
    }

    SAFE_RELEASE(pDSBPrimary);

    return S_OK;
}

// FUNCTION: TH16 0x4709b0
HARNESS_CALLED CSound::CSound(LPDIRECTSOUNDBUFFER *apDSBuffer, DWORD dwDSBufferSize, DWORD dwNumBuffers,
                              CWaveFile *pWaveFile)
{
    DWORD i;

    m_apDSBuffer = new LPDIRECTSOUNDBUFFER[dwNumBuffers];
    for (i = 0; i < dwNumBuffers; i++)
    {
        m_apDSBuffer[i] = apDSBuffer[i];
    }

    m_dwDSBufferSize = dwDSBufferSize;
    m_dwNumBuffers = dwNumBuffers;
    m_pWaveFile = pWaveFile;

    fill_buffer_inline(m_apDSBuffer[0], FALSE, 0);

    for (i = 0; i < dwNumBuffers; i++)
    {
        m_apDSBuffer[i]->SetCurrentPosition(0);
    }
    m_playing = FALSE;
    m_paused = FALSE;
}

// SYNTHETIC: TH16 0x470b80
// CSound::`scalar deleting destructor'

// FUNCTION: TH16 0x470e10
CSound::~CSound()
{
    for (DWORD i = 0; i < m_dwNumBuffers; i++)
    {
        SAFE_RELEASE(m_apDSBuffer[i]);
    }

    SAFE_DELETE_ARRAY(m_apDSBuffer);
    SAFE_DELETE(m_pWaveFile);
}

// FUNCTION: TH16 0x470ed0
HRESULT CSound::FillBufferWithSound(LPDIRECTSOUNDBUFFER pDSB, BOOL bRepeatWavIfBufferLarger, DWORD offset)
{
    HRESULT hr;
    VOID *pDSLockedBuffer = NULL;
    DWORD dwDSLockedBufferSize = 0;
    DWORD dwWavDataRead = 0;

    if (pDSB == NULL)
    {
        return CO_E_NOTINITIALIZED;
    }

    if (FAILED(hr = restore_buffer(pDSB, NULL)))
    {
        return hr;
    }

    if (FAILED(hr = pDSB->Lock(0, m_dwDSBufferSize, &pDSLockedBuffer, &dwDSLockedBufferSize, NULL, NULL, 0L)))
    {
        return hr;
    }

    m_pWaveFile->ResetFile(false, offset);

    if (FAILED(hr = m_pWaveFile->Read((BYTE *)pDSLockedBuffer, dwDSLockedBufferSize, &dwWavDataRead)))
    {
        return hr;
    }

    if (dwWavDataRead == 0)
    {
        FillMemory((BYTE *)pDSLockedBuffer, dwDSLockedBufferSize,
                   (BYTE)(m_pWaveFile->m_track->format.wBitsPerSample == 8 ? 128 : 0));
    }
    else if (dwWavDataRead < dwDSLockedBufferSize)
    {
        if (bRepeatWavIfBufferLarger)
        {
            DWORD dwReadSoFar = dwWavDataRead;
            while (dwReadSoFar < dwDSLockedBufferSize)
            {
                if (FAILED(hr = m_pWaveFile->ResetFile(true, 0)))
                {
                    return hr;
                }

                hr = m_pWaveFile->Read((BYTE *)pDSLockedBuffer + dwReadSoFar, dwDSLockedBufferSize - dwReadSoFar,
                                       &dwWavDataRead);
                if (FAILED(hr))
                {
                    return hr;
                }

                dwReadSoFar += dwWavDataRead;
            }
        }
        else
        {
            FillMemory((BYTE *)pDSLockedBuffer + dwWavDataRead, dwDSLockedBufferSize - dwWavDataRead,
                       (BYTE)(m_pWaveFile->m_track->format.wBitsPerSample == 8 ? 128 : 0));
        }
    }

    pDSB->Unlock(pDSLockedBuffer, dwDSLockedBufferSize, NULL, 0);

    return S_OK;
}

// The part of CSound::RestoreBuffer that LTCG kept out of line. Its callers
// test pDSB for NULL themselves and pass it in ecx, the way a member function
// of the buffer would get it; this stand-in type reproduces that.
struct DSoundBufferRestore
{
    HRESULT restore(BOOL *pbWasRestored);
};

// FUNCTION: TH16 0x471040
HARNESS_CALLED HRESULT DSoundBufferRestore::restore(BOOL *pbWasRestored)
{
    HRESULT hr;
    LPDIRECTSOUNDBUFFER pDSB = (LPDIRECTSOUNDBUFFER)this;

    if (pbWasRestored)
    {
        *pbWasRestored = FALSE;
    }

    DWORD dwStatus;
    if (FAILED(hr = pDSB->GetStatus(&dwStatus)))
    {
        return hr;
    }

    if (dwStatus & DSBSTATUS_BUFFERLOST)
    {
        do
        {
            hr = pDSB->Restore();
            if (hr == DSERR_BUFFERLOST)
            {
                Sleep(10);
            }
        } while (hr = pDSB->Restore());

        if (pbWasRestored != NULL)
        {
            *pbWasRestored = TRUE;
        }

        return S_OK;
    }
    else
    {
        return S_FALSE;
    }
}

inline HRESULT CSound::RestoreBuffer(LPDIRECTSOUNDBUFFER pDSB, BOOL *pbWasRestored)
{
    if (pDSB == NULL)
    {
        return CO_E_NOTINITIALIZED;
    }
    return ((DSoundBufferRestore *)pDSB)->restore(pbWasRestored);
}

// FUNCTION: TH16 0x4710b0
LPDIRECTSOUNDBUFFER CSound::GetFreeBuffer()
{
    if (m_apDSBuffer == NULL)
    {
        return NULL;
    }

    DWORD i;
    for (i = 0; i < m_dwNumBuffers; i++)
    {
        if (m_apDSBuffer[i])
        {
            DWORD dwStatus = 0;
            m_apDSBuffer[i]->GetStatus(&dwStatus);
            if ((dwStatus & DSBSTATUS_PLAYING) == 0)
            {
                break;
            }
        }
    }

    if (i != m_dwNumBuffers)
    {
        return m_apDSBuffer[i];
    }
    else
    {
        return m_apDSBuffer[rand() % m_dwNumBuffers];
    }
}

// FUNCTION: TH16 0x471120
HRESULT CSound::Play(DWORD dwPriority, DWORD dwFlags, DWORD offset)
{
    HRESULT hr;
    BOOL bRestored;

    if (m_apDSBuffer == NULL)
    {
        return CO_E_NOTINITIALIZED;
    }

    LPDIRECTSOUNDBUFFER pDSB = GetFreeBuffer();

    if (pDSB == NULL)
    {
        return E_FAIL;
    }

    if (FAILED(hr = RestoreBuffer(pDSB, &bRestored)))
    {
        return hr;
    }

    if (bRestored)
    {
        if (FAILED(hr = FillBufferWithSound(pDSB, FALSE, offset)))
        {
            return hr;
        }

        Reset();
    }

    m_fade_mode = 0;
    m_fade_time_left = 0;
    m_fade_duration = 0;
    SetVolume(0);
    m_playing = TRUE;
    m_play_priority = dwPriority;
    m_play_flags = dwFlags;
    unk_2c = 0;
    m_start_time = get_runtime();
    m_paused_total = 0.0;
    unk_48 = 0.0;
    m_pause_time = 0.0;

    return pDSB->Play(0, dwPriority, dwFlags);
}

// TODO: the original has a call in each branch, pushing -10000 directly and loading the buffer early in the first; written that way ours hoists the buffer load above the test.
// FUNCTION: TH16 0x4711f0
HRESULT CSound::SetVolume(i32 volume)
{
    if (g_SoundManager.bgm_volume != 0)
    {
        f32 x = g_SoundManager.bgm_volume / 100.0f;
        f32 t = (1.0f - x) * (1.0f - x);
        f32 u = 1.0f - t;
        volume = (i32)(u * (volume + 5000)) - 5000;
    }
    else
    {
        volume = -10000;
    }
    return m_apDSBuffer[0]->SetVolume(volume);
}

// FUNCTION: TH16 0x471270
HRESULT CSound::Stop(BOOL close_file)
{
    if (m_apDSBuffer == NULL)
    {
        return CO_E_NOTINITIALIZED;
    }

    HRESULT hr = 0;

    m_playing = FALSE;
    m_paused = FALSE;
    for (DWORD i = 0; i < m_dwNumBuffers; i++)
    {
        hr |= m_apDSBuffer[i]->Stop();
        hr |= m_apDSBuffer[i]->SetCurrentPosition(0);
    }

    m_fade_mode = 0;
    if (close_file)
    {
        m_pWaveFile->Close();
    }

    return hr;
}

// TODO: the original reloads m_pWaveFile after SetFilePointer (alias analysis differs) and has a 4-byte frame.
// FUNCTION: TH16 0x4712f0
HRESULT CSound::Pause()
{
    if (m_apDSBuffer == NULL)
    {
        return CO_E_NOTINITIALIZED;
    }
    if (!m_playing)
    {
        m_paused = FALSE;
        return CO_E_NOTINITIALIZED;
    }

    m_playing = FALSE;
    m_paused = TRUE;
    HRESULT hr = m_apDSBuffer[0]->Stop();
    m_pause_time = get_runtime();
    m_pWaveFile->m_paused_position =
        SetFilePointer(m_pWaveFile->m_file, 0, NULL, FILE_CURRENT) - m_pWaveFile->m_track->start_offset;
    m_pWaveFile->Close();
    return hr;
}

// TODO: the original adds m_paused_total from memory (addsd xmm0, [m_paused_total]); ours loads it into xmm1.
// FUNCTION: TH16 0x471380
HRESULT CSound::Unpause()
{
    if (m_apDSBuffer == NULL)
    {
        return CO_E_NOTINITIALIZED;
    }
    if (!m_paused)
    {
        return CO_E_NOTINITIALIZED;
    }

    m_paused = FALSE;
    m_pWaveFile->open_bgm(m_pWaveFile->m_track, m_pWaveFile->m_paused_position);
    LPDIRECTSOUNDBUFFER pDSB = m_apDSBuffer[0];
    m_playing = TRUE;
    m_paused_total = m_paused_total + (get_runtime() - m_pause_time);
    return pDSB->Play(0, m_play_priority, m_play_flags);
}

// FUNCTION: TH16 0x4713f0
HRESULT CSound::Reset()
{
    if (m_apDSBuffer == NULL)
    {
        return CO_E_NOTINITIALIZED;
    }

    HRESULT hr = 0;

    for (DWORD i = 0; i < m_dwNumBuffers; i++)
    {
        hr |= m_apDSBuffer[i]->SetCurrentPosition(0);
    }

    return hr;
}

// FUNCTION: TH16 0x471430
CStreamingSound::CStreamingSound(LPDIRECTSOUNDBUFFER pDSBuffer, DWORD dwDSBufferSize, CWaveFile *pWaveFile,
                                 DWORD dwNotifySize)
    : CSound(&pDSBuffer, dwDSBufferSize, 1, pWaveFile)
{
    m_dwLastPlayPos = 0;
    m_dwPlayProgress = 0;
    m_dwNotifySize = dwNotifySize;
    m_dwNextWriteOffset = 0;
    m_bFillNextNotificationWithSilence = FALSE;
}

// SYNTHETIC: TH16 0x471490
// CStreamingSound::`scalar deleting destructor'

CStreamingSound::~CStreamingSound()
{
}

// FUNCTION: TH16 0x4714c0
HARNESS_CALLED HRESULT CStreamingSound::HandleWaveStreamNotification(BOOL bLoopedPlay)
{
    HRESULT hr;
    DWORD dwCurrentPlayPos;
    DWORD dwPlayDelta;
    DWORD dwBytesWrittenToBuffer;
    VOID *pDSLockedBuffer;
    VOID *pDSLockedBuffer2;
    DWORD dwDSLockedBufferSize;
    DWORD dwDSLockedBufferSize2;

    if (m_apDSBuffer == NULL || m_pWaveFile == NULL)
    {
        return CO_E_NOTINITIALIZED;
    }

    ENTER_CS(CS_BGM_STREAM);

    // Leave the part the play cursor is about to reach alone.
    DWORD dwPlayCursor;
    DWORD dwWriteCursor;
    m_apDSBuffer[0]->GetCurrentPosition(&dwPlayCursor, &dwWriteCursor);
    if (m_dwNextWriteOffset >= dwWriteCursor - m_dwNotifySize && m_dwNextWriteOffset < dwWriteCursor)
    {
        hr = CO_E_NOTINITIALIZED;
        goto end;
    }

    BOOL bRestored;
    if (FAILED(hr = RestoreBuffer(m_apDSBuffer[0], &bRestored)))
    {
        goto end;
    }

    if (bRestored)
    {
        hr = FillBufferWithSound(m_apDSBuffer[0], FALSE, 0);
        goto end;
    }

    pDSLockedBuffer = NULL;
    pDSLockedBuffer2 = NULL;
    if (FAILED(hr = m_apDSBuffer[0]->Lock(m_dwNextWriteOffset, m_dwNotifySize, &pDSLockedBuffer,
                                          &dwDSLockedBufferSize, &pDSLockedBuffer2, &dwDSLockedBufferSize2, 0L)))
    {
        goto end;
    }

    if (pDSLockedBuffer2 != NULL)
    {
        return E_UNEXPECTED;
    }

    if (!m_bFillNextNotificationWithSilence)
    {
        if (FAILED(hr = m_pWaveFile->Read((BYTE *)pDSLockedBuffer, dwDSLockedBufferSize, &dwBytesWrittenToBuffer)))
        {
            goto end;
        }
    }
    else
    {
        FillMemory(pDSLockedBuffer, dwDSLockedBufferSize,
                   (BYTE)(m_pWaveFile->m_track->format.wBitsPerSample == 8 ? 128 : 0));
        dwBytesWrittenToBuffer = dwDSLockedBufferSize;
    }

    if (dwBytesWrittenToBuffer < dwDSLockedBufferSize)
    {
        if (!bLoopedPlay)
        {
            FillMemory((BYTE *)pDSLockedBuffer + dwBytesWrittenToBuffer, dwDSLockedBufferSize - dwBytesWrittenToBuffer,
                       (BYTE)(m_pWaveFile->m_track->format.wBitsPerSample == 8 ? 128 : 0));
            m_bFillNextNotificationWithSilence = TRUE;
        }
        else
        {
            m_pWaveFile->m_looped = TRUE;
            DWORD dwReadSoFar = dwBytesWrittenToBuffer;
            while (dwReadSoFar < dwDSLockedBufferSize)
            {
                if (FAILED(hr = m_pWaveFile->ResetFile(true, 0)))
                {
                    goto end;
                }

                if (FAILED(hr = m_pWaveFile->Read((BYTE *)pDSLockedBuffer + dwReadSoFar,
                                                  dwDSLockedBufferSize - dwReadSoFar, &dwBytesWrittenToBuffer)))
                {
                    goto end;
                }

                dwReadSoFar += dwBytesWrittenToBuffer;
            }
        }
    }

    m_apDSBuffer[0]->Unlock(pDSLockedBuffer, dwDSLockedBufferSize, NULL, 0);

    if (FAILED(hr = m_apDSBuffer[0]->GetCurrentPosition(&dwCurrentPlayPos, NULL)))
    {
        goto end;
    }

    if (dwCurrentPlayPos < m_dwLastPlayPos)
    {
        dwPlayDelta = (m_dwDSBufferSize - m_dwLastPlayPos) + dwCurrentPlayPos;
    }
    else
    {
        dwPlayDelta = dwCurrentPlayPos - m_dwLastPlayPos;
    }

    m_dwPlayProgress += dwPlayDelta;
    m_dwLastPlayPos = dwCurrentPlayPos;

    m_dwNextWriteOffset += dwDSLockedBufferSize;
    m_dwNextWriteOffset %= m_dwDSBufferSize;
    hr = S_OK;

end:
    LEAVE_CS(CS_BGM_STREAM);
    return hr;
}

// FUNCTION: TH16 0x471720
HRESULT CStreamingSound::Reset(DWORD offset)
{
    HRESULT hr;

    if (m_apDSBuffer[0] == NULL || m_pWaveFile == NULL)
    {
        return CO_E_NOTINITIALIZED;
    }

    m_dwLastPlayPos = 0;
    m_dwPlayProgress = 0;
    m_dwNextWriteOffset = 0;
    unk_8c = 0;
    m_bFillNextNotificationWithSilence = FALSE;

    BOOL bRestored;
    if (FAILED(hr = RestoreBuffer(m_apDSBuffer[0], &bRestored)))
    {
        return hr;
    }

    if (bRestored)
    {
        if (FAILED(hr = FillBufferWithSound(m_apDSBuffer[0], FALSE, offset)))
        {
            return hr;
        }
    }

    m_pWaveFile->ResetFile(m_pWaveFile->m_looped == TRUE, offset);

    return m_apDSBuffer[0]->SetCurrentPosition(0L);
}

// get_play_time's body as LTCG inlined it into switch_track.
static __forceinline double play_time(CStreamingSound *sound)
{
    double time = get_runtime() - (sound->m_start_time + sound->m_paused_total);
    ThBgmFormat *track = sound->m_pWaveFile->m_track;
    double end = track->total_size / (track->format.nSamplesPerSec / 8.0) / track->format.wBitsPerSample /
                 track->format.nChannels;
    double loop = (track->total_size - track->intro_size) / (double)track->format.nSamplesPerSec /
                  (track->format.wBitsPerSample / 8.0) / track->format.nChannels;
    while (time >= end)
    {
        time -= loop;
    }
    return time;
}

// FUNCTION: TH16 0x470bb0
HRESULT CStreamingSound::recreate_buffers(ThBgmFormat *track)
{
    DWORD i;

    m_playing = FALSE;
    for (i = 0; i < m_dwNumBuffers; i++)
    {
        SAFE_RELEASE(m_apDSBuffer[i]);
    }
    SAFE_DELETE_ARRAY(m_apDSBuffer);

    LPDIRECTSOUNDNOTIFY pDSNotify = NULL;
    m_apDSBuffer = new LPDIRECTSOUNDBUFFER[m_dwNumBuffers];
    DSBUFFERDESC dsbd = *(DSBUFFERDESC *)m_desc;
    dsbd.lpwfxFormat = &track->format;
    for (i = 0; i < m_dwNumBuffers; i++)
    {
        if (FAILED(((IDirectSound8 *)m_manager->m_pDS)->CreateSoundBuffer(&dsbd, &m_apDSBuffer[i], NULL)))
        {
            return E_FAIL;
        }
        if (FAILED(m_apDSBuffer[i]->QueryInterface(IID_IDirectSoundNotify, (VOID **)&pDSNotify)))
        {
            return E_FAIL;
        }

        DSBPOSITIONNOTIFY *aPosNotify = new DSBPOSITIONNOTIFY[8];
        if (aPosNotify == NULL)
        {
            return E_OUTOFMEMORY;
        }
        for (DWORD j = 0; j < 8; j++)
        {
            aPosNotify[j].dwOffset = (m_dwNotifySize * j) + m_dwNotifySize - 1;
            aPosNotify[j].hEventNotify = m_hNotifyEvent;
        }

        if (FAILED(pDSNotify->SetNotificationPositions(8, aPosNotify)))
        {
            SAFE_RELEASE(pDSNotify);
            SAFE_DELETE_ARRAY(aPosNotify);
            return E_FAIL;
        }
        SAFE_RELEASE(pDSNotify);
        SAFE_DELETE_ARRAY(aPosNotify);
    }
    return S_OK;
}

// FUNCTION: TH16 0x471b00
HRESULT CStreamingSound::switch_track(ThBgmFormat *track)
{
    double time = play_time(this);
    m_pWaveFile->m_track = track;
    seek(time);
    return S_OK;
}

// The body is spelled out: through the inline play_time helper its double
// math belongs to the helper's call graph node, and the frame loses the
// original's realignment (and esp, -8).
// FUNCTION: TH16 0x471bd0
HARNESS_CALLED double CStreamingSound::get_play_time()
{
    double time = get_runtime() - (m_start_time + m_paused_total);
    ThBgmFormat *track = m_pWaveFile->m_track;
    double end = track->total_size / (track->format.nSamplesPerSec / 8.0) / track->format.wBitsPerSample /
                 track->format.nChannels;
    double loop = (track->total_size - track->intro_size) / (double)track->format.nSamplesPerSec /
                  (track->format.wBitsPerSample / 8.0) / track->format.nChannels;
    while (time >= end)
    {
        time -= loop;
    }
    return time;
}

// FUNCTION: TH16 0x471c90
HARNESS_CALLED void CStreamingSound::seek(double seconds)
{
    ENTER_CS(CS_BGM_STREAM);
    m_apDSBuffer[0]->Stop();
    ThBgmFormat *track = m_pWaveFile->m_track;
    i32 offset = (i32)(track->format.nSamplesPerSec * seconds) * (track->format.wBitsPerSample / 8) *
                     track->format.nChannels -
                 track->format.nBlockAlign;
    if (offset < 0)
    {
        offset = 0;
    }
    recreate_buffers(track);
    Reset(offset);
    m_pWaveFile->m_dwSize = m_pWaveFile->m_ck.cksize;
    FillBufferWithSound(m_apDSBuffer[0], m_pWaveFile->m_track->total_size != 0, offset);
    Play(m_play_priority, m_play_flags, offset);
    m_start_time -= seconds;
    LEAVE_CS(CS_BGM_STREAM);
}

// FUNCTION: TH16 0x4717e0
HRESULT CWaveFile::open_file(const char *filename, ThBgmFormat *track)
{
    if (filename == NULL)
    {
        return E_INVALIDARG;
    }

    dsutil_debug_log("Streaming File Open %s\r\n", filename);
    m_file = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                         FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (m_file == INVALID_HANDLE_VALUE)
    {
        LPSTR message;
        FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                       NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&message, 0, NULL);
        dsutil_debug_log(message);
        LocalFree(message);
        return E_FAIL;
    }

    m_track = track;
    m_filename = filename;
    ResetFile(false, 0);
    m_dwSize = m_ck.cksize;
    m_looped = FALSE;
    return S_OK;
}

// FUNCTION: TH16 0x4718a0
HRESULT CWaveFile::open_bgm(ThBgmFormat *track, i32 offset)
{
    if (m_bIsReadingFromMemory)
    {
        return E_FAIL;
    }

    if (m_file == INVALID_HANDLE_VALUE)
    {
        Open(m_filename, track);
        if (m_file == INVALID_HANDLE_VALUE)
        {
            return E_FAIL;
        }
    }

    m_looped = FALSE;
    m_track = track;
    dsutil_debug_log("%s %d %d\n", track->name, track->intro_size, track->total_size);
    ResetFile(false, offset);
    m_dwSize = m_ck.cksize;
    return S_OK;
}

// FUNCTION: TH16 0x471930
HARNESS_CALLED HRESULT CWaveFile::ResetFile(bool loop, DWORD offset)
{
    if (m_bIsReadingFromMemory)
    {
        m_pbDataCur = m_pbData;
        if (m_track->total_size > 0)
        {
            m_ulDataSize = m_track->total_size;
        }
        if (loop && m_track->intro_size > 0)
        {
            m_pbDataCur = m_pbData + m_track->intro_size;
        }
    }
    else
    {
        if (m_file == INVALID_HANDLE_VALUE || m_file == NULL)
        {
            return CO_E_NOTINITIALIZED;
        }

        if (loop && m_track->intro_size > 0)
        {
            SetFilePointer(m_file, m_track->start_offset + m_track->intro_size + g_SoundManager.bgm_file_offset,
                           NULL, FILE_BEGIN);
            m_ck.cksize = m_track->total_size - m_track->intro_size;
            return S_OK;
        }

        if (offset >= m_track->total_size)
        {
            offset += m_track->intro_size - m_track->total_size;
        }
        SetFilePointer(m_file, m_track->start_offset + g_SoundManager.bgm_file_offset + offset, NULL, FILE_BEGIN);
        m_ck.cksize = m_track->total_size - offset;
    }
    return S_OK;
}

// FUNCTION: TH16 0x471a30
HARNESS_CALLED HRESULT CWaveFile::Read(BYTE *pBuffer, DWORD dwSizeToRead, DWORD *pdwSizeRead)
{
    if (m_bIsReadingFromMemory)
    {
        if (m_pbDataCur == NULL)
        {
            return CO_E_NOTINITIALIZED;
        }
        if (pdwSizeRead != NULL)
        {
            *pdwSizeRead = 0;
        }

        if ((BYTE *)(m_pbDataCur + dwSizeToRead) > (BYTE *)(m_pbData + m_ulDataSize))
        {
            dwSizeToRead = m_ulDataSize - (DWORD)(m_pbDataCur - m_pbData);
        }

        memcpy(pBuffer, m_pbDataCur, dwSizeToRead);

        m_pbDataCur += dwSizeToRead;

        if (pdwSizeRead != NULL)
        {
            *pdwSizeRead = dwSizeToRead;
        }

        return S_OK;
    }

    HANDLE file = m_file;
    if (file == NULL)
    {
        return CO_E_NOTINITIALIZED;
    }
    if (pBuffer == NULL || pdwSizeRead == NULL)
    {
        return E_INVALIDARG;
    }

    DWORD cbDataIn = dwSizeToRead;
    if (cbDataIn > m_ck.cksize)
    {
        cbDataIn = m_ck.cksize;
    }
    m_ck.cksize -= cbDataIn;

    DWORD dwRead;
    ReadFile(file, pBuffer, cbDataIn, &dwRead, NULL);
    *pdwSizeRead = dwRead;
    return S_OK;
}
