// Stand-in for the Windows SDK's mmreg.h: WAVEFORMATEX.
#pragma once

#include <windows.h>

#ifndef WAVE_FORMAT_PCM
#define WAVE_FORMAT_PCM 1
#endif

// 1-byte packed in the SDK (18 bytes); thbgm.fmt stores it as is.
#pragma pack(push, 1)
typedef struct tWAVEFORMATEX
{
    WORD wFormatTag;
    WORD nChannels;
    DWORD nSamplesPerSec;
    DWORD nAvgBytesPerSec;
    WORD nBlockAlign;
    WORD wBitsPerSample;
    WORD cbSize;
} WAVEFORMATEX, *PWAVEFORMATEX, *LPWAVEFORMATEX;
#pragma pack(pop)
typedef const WAVEFORMATEX *LPCWAVEFORMATEX;
