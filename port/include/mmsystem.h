// Stand-in for the Windows SDK's mmsystem.h: the multimedia timer and the
// mmio structures the DirectSound sample code keeps (DSUtil.cpp).
#pragma once

#include <windows.h>
#include <mmreg.h>

typedef UINT MMRESULT;
typedef DWORD FOURCC;
typedef HANDLE HTASK;
typedef struct HMMIO__ *HMMIO;
typedef LRESULT(CALLBACK MMIOPROC)(LPSTR lpmmioinfo, UINT uMsg, LPARAM lParam1, LPARAM lParam2);
typedef MMIOPROC *LPMMIOPROC;

#define MMSYSERR_NOERROR 0
#define TIMERR_NOERROR 0

#define mmioFOURCC(ch0, ch1, ch2, ch3) \
    ((DWORD)(BYTE)(ch0) | ((DWORD)(BYTE)(ch1) << 8) | ((DWORD)(BYTE)(ch2) << 16) | ((DWORD)(BYTE)(ch3) << 24))

typedef struct _MMCKINFO
{
    FOURCC ckid;
    DWORD cksize;
    FOURCC fccType;
    DWORD dwDataOffset;
    DWORD dwFlags;
} MMCKINFO, *PMMCKINFO, *LPMMCKINFO;

// 72 bytes on x86. CWaveFile embeds one but never uses it.
typedef struct _MMIOINFO
{
    DWORD dwFlags;
    FOURCC fccIOProc;
    LPMMIOPROC pIOProc;
    UINT wErrorRet;
    HTASK htask;
    LONG cchBuffer;
    char *pchBuffer;
    char *pchNext;
    char *pchEndRead;
    char *pchEndWrite;
    LONG lBufOffset;
    LONG lDiskOffset;
    DWORD adwInfo[3];
    DWORD dwReserved1;
    DWORD dwReserved2;
    HMMIO hmmio;
} MMIOINFO, *PMMIOINFO, *LPMMIOINFO;

// The winmm joystick API (the game's fallback when DirectInput finds no pad).
#define MAXPNAMELEN 32
#define MAX_JOYSTICKOEMVXDNAME 260
#define JOYSTICKID1 0
#define JOYSTICKID2 1
#define JOYERR_NOERROR 0
#define JOYERR_PARMS 165
#define JOYERR_NOCANDO 166
#define JOYERR_UNPLUGGED 167
#define JOY_RETURNX 0x00000001
#define JOY_RETURNY 0x00000002
#define JOY_RETURNZ 0x00000004
#define JOY_RETURNR 0x00000008
#define JOY_RETURNU 0x00000010
#define JOY_RETURNV 0x00000020
#define JOY_RETURNPOV 0x00000040
#define JOY_RETURNBUTTONS 0x00000080
#define JOY_RETURNALL                                                                                    \
    (JOY_RETURNX | JOY_RETURNY | JOY_RETURNZ | JOY_RETURNR | JOY_RETURNU | JOY_RETURNV | JOY_RETURNPOV | \
     JOY_RETURNBUTTONS)
#define JOY_POVCENTERED (WORD)-1
#define JOY_POVFORWARD 0
#define JOY_POVRIGHT 9000
#define JOY_POVBACKWARD 18000
#define JOY_POVLEFT 27000

typedef struct tagJOYCAPSA
{
    WORD wMid;
    WORD wPid;
    CHAR szPname[MAXPNAMELEN];
    UINT wXmin;
    UINT wXmax;
    UINT wYmin;
    UINT wYmax;
    UINT wZmin;
    UINT wZmax;
    UINT wNumButtons;
    UINT wPeriodMin;
    UINT wPeriodMax;
    UINT wRmin;
    UINT wRmax;
    UINT wUmin;
    UINT wUmax;
    UINT wVmin;
    UINT wVmax;
    UINT wCaps;
    UINT wMaxAxes;
    UINT wNumAxes;
    UINT wMaxButtons;
    CHAR szRegKey[MAXPNAMELEN];
    CHAR szOEMVxD[MAX_JOYSTICKOEMVXDNAME];
} JOYCAPSA, *PJOYCAPSA, *LPJOYCAPSA;

typedef struct joyinfoex_tag
{
    DWORD dwSize;
    DWORD dwFlags;
    DWORD dwXpos;
    DWORD dwYpos;
    DWORD dwZpos;
    DWORD dwRpos;
    DWORD dwUpos;
    DWORD dwVpos;
    DWORD dwButtons;
    DWORD dwButtonNumber;
    DWORD dwPOV;
    DWORD dwReserved1;
    DWORD dwReserved2;
} JOYINFOEX, *PJOYINFOEX, *LPJOYINFOEX;

extern "C" {
MMRESULT joyGetPosEx(UINT uJoyID, LPJOYINFOEX pji);
MMRESULT joyGetDevCapsA(UINT_PTR uJoyID, LPJOYCAPSA pjc, UINT cbjc);
DWORD timeGetTime(void);
MMRESULT timeBeginPeriod(UINT uPeriod);
MMRESULT timeEndPeriod(UINT uPeriod);
}
