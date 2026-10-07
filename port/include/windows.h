// Stand-in for the Windows SDK's windows.h in the portable build: the
// types, constants, structures and functions of kernel32, user32 and gdi32
// that the game uses, with the SDK's names, values and 32-bit layouts.
// The functions are implemented (as stubs for now) in port/src/win32_*.cpp
// and port/src/gdi_*.cpp.
//
// Integer types keep their Windows widths on every target: DWORD and LONG
// are 32 bits even where long is 64 (LP64).
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "port_com.h"

#ifdef __cplusplus
extern "C" {
#endif

// ---------------------------------------------------------------------------
// Base types (windef.h, winnt.h, basetsd.h)

typedef int BOOL;
typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef uint32_t DWORD;
typedef int32_t LONG;
typedef uint32_t ULONG;
typedef int INT;
typedef unsigned int UINT;
typedef short SHORT;
typedef unsigned short USHORT;
typedef char CHAR;
typedef unsigned char UCHAR;
typedef float FLOAT;
typedef int64_t LONGLONG;
typedef uint64_t ULONGLONG;
typedef wchar_t WCHAR;
typedef BYTE BOOLEAN;
#define VOID void

typedef intptr_t INT_PTR;
typedef uintptr_t UINT_PTR;
typedef intptr_t LONG_PTR;
typedef uintptr_t ULONG_PTR;
typedef uintptr_t DWORD_PTR;
typedef size_t SIZE_T;

typedef void *PVOID;
typedef void *LPVOID;
typedef const void *LPCVOID;
typedef char *LPSTR;
typedef const char *LPCSTR;
typedef WCHAR *LPWSTR;
typedef const WCHAR *LPCWSTR;
typedef DWORD *LPDWORD;
typedef DWORD *PDWORD;
typedef BYTE *LPBYTE;
typedef BYTE *PBYTE;
typedef LONG *LPLONG;
typedef LONG *PLONG;
typedef BOOL *LPBOOL;
typedef WORD *LPWORD;
typedef UINT *LPUINT;

typedef UINT_PTR WPARAM;
typedef LONG_PTR LPARAM;
typedef LONG_PTR LRESULT;
typedef WORD ATOM;
typedef DWORD COLORREF;

#define TH16_DECLARE_HANDLE(name) \
    struct name##__ { int unused; }; \
    typedef struct name##__ *name

typedef void *HANDLE;
typedef HANDLE *LPHANDLE;
TH16_DECLARE_HANDLE(HWND);
TH16_DECLARE_HANDLE(HINSTANCE);
typedef HINSTANCE HMODULE;
TH16_DECLARE_HANDLE(HDC);
TH16_DECLARE_HANDLE(HFONT);
TH16_DECLARE_HANDLE(HBITMAP);
TH16_DECLARE_HANDLE(HBRUSH);
TH16_DECLARE_HANDLE(HPEN);
TH16_DECLARE_HANDLE(HICON);
TH16_DECLARE_HANDLE(HMENU);
TH16_DECLARE_HANDLE(HKEY);
TH16_DECLARE_HANDLE(HMONITOR);
TH16_DECLARE_HANDLE(HPALETTE);
TH16_DECLARE_HANDLE(HRGN);
typedef HICON HCURSOR;
typedef void *HGDIOBJ;
typedef HANDLE HGLOBAL;
typedef HANDLE HLOCAL;

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif
#ifndef MAX_PATH
#define MAX_PATH 260
#endif

#define MAKEWORD(a, b) ((WORD)(((BYTE)((DWORD_PTR)(a) & 0xff)) | ((WORD)((BYTE)((DWORD_PTR)(b) & 0xff))) << 8))
#define MAKELONG(a, b) ((LONG)(((WORD)((DWORD_PTR)(a) & 0xffff)) | ((DWORD)((WORD)((DWORD_PTR)(b) & 0xffff))) << 16))
#define LOWORD(l) ((WORD)((DWORD_PTR)(l) & 0xffff))
#define HIWORD(l) ((WORD)((DWORD_PTR)(l) >> 16))
#define LOBYTE(w) ((BYTE)((DWORD_PTR)(w) & 0xff))
#define HIBYTE(w) ((BYTE)((DWORD_PTR)(w) >> 8))

#define RGB(r, g, b) ((COLORREF)(((BYTE)(r) | ((WORD)((BYTE)(g)) << 8)) | (((DWORD)(BYTE)(b)) << 16)))
#define GetRValue(rgb) (LOBYTE(rgb))
#define GetGValue(rgb) (LOBYTE(((WORD)(rgb)) >> 8))
#define GetBValue(rgb) (LOBYTE((rgb) >> 16))

typedef struct tagRECT
{
    LONG left;
    LONG top;
    LONG right;
    LONG bottom;
} RECT, *PRECT, *LPRECT;
typedef const RECT *LPCRECT;

typedef struct tagPOINT
{
    LONG x;
    LONG y;
} POINT, *PPOINT, *LPPOINT;

typedef struct tagSIZE
{
    LONG cx;
    LONG cy;
} SIZE, *PSIZE, *LPSIZE;

typedef union _LARGE_INTEGER {
    struct
    {
        DWORD LowPart;
        LONG HighPart;
    } u;
    LONGLONG QuadPart;
} LARGE_INTEGER, *PLARGE_INTEGER;

typedef struct _FILETIME
{
    DWORD dwLowDateTime;
    DWORD dwHighDateTime;
} FILETIME, *PFILETIME, *LPFILETIME;

typedef struct _SYSTEMTIME
{
    WORD wYear;
    WORD wMonth;
    WORD wDayOfWeek;
    WORD wDay;
    WORD wHour;
    WORD wMinute;
    WORD wSecond;
    WORD wMilliseconds;
} SYSTEMTIME, *PSYSTEMTIME, *LPSYSTEMTIME;

// ---------------------------------------------------------------------------
// Errors (winerror.h)

#define ERROR_SUCCESS 0
#define ERROR_FILE_NOT_FOUND 2
#define ERROR_PATH_NOT_FOUND 3
#define ERROR_ACCESS_DENIED 5
#define ERROR_INVALID_HANDLE 6
#define ERROR_NOT_ENOUGH_MEMORY 8
#define ERROR_NO_MORE_FILES 18
#define ERROR_WRITE_FAULT 29
#define ERROR_READ_FAULT 30
#define ERROR_SHARING_VIOLATION 32
#define ERROR_FILE_EXISTS 80
#define ERROR_INVALID_PARAMETER 87
#define ERROR_DISK_FULL 112
#define ERROR_INSUFFICIENT_BUFFER 122
#define ERROR_NEGATIVE_SEEK 131
#define ERROR_DIR_NOT_EMPTY 145
#define ERROR_ALREADY_EXISTS 183
#define ERROR_NOT_SUPPORTED 50
#define ERROR_CALL_NOT_IMPLEMENTED 120
#define NO_ERROR 0

#define MAKELANGID(p, s) ((((WORD)(s)) << 10) | (WORD)(p))
#define LANG_NEUTRAL 0x00
#define SUBLANG_DEFAULT 0x01

#define FORMAT_MESSAGE_ALLOCATE_BUFFER 0x00000100
#define FORMAT_MESSAGE_IGNORE_INSERTS 0x00000200
#define FORMAT_MESSAGE_FROM_STRING 0x00000400
#define FORMAT_MESSAGE_FROM_HMODULE 0x00000800
#define FORMAT_MESSAGE_FROM_SYSTEM 0x00001000

// ---------------------------------------------------------------------------
// Memory (winbase.h, winnt.h)

#define CopyMemory(dst, src, len) memcpy((dst), (src), (len))
#define MoveMemory(dst, src, len) memmove((dst), (src), (len))
#define FillMemory(dst, len, fill) memset((dst), (fill), (len))
#define ZeroMemory(dst, len) memset((dst), 0, (len))

HLOCAL LocalFree(HLOCAL mem);

// ---------------------------------------------------------------------------
// Files (fileapi.h)

#define INVALID_HANDLE_VALUE ((HANDLE)(LONG_PTR)-1)
#define INVALID_FILE_SIZE ((DWORD)0xffffffff)
#define INVALID_SET_FILE_POINTER ((DWORD)-1)

#define GENERIC_READ 0x80000000u
#define GENERIC_WRITE 0x40000000u
#define FILE_SHARE_READ 0x00000001
#define FILE_SHARE_WRITE 0x00000002
#define CREATE_NEW 1
#define CREATE_ALWAYS 2
#define OPEN_EXISTING 3
#define OPEN_ALWAYS 4
#define TRUNCATE_EXISTING 5
#define FILE_ATTRIBUTE_READONLY 0x00000001
#define FILE_ATTRIBUTE_DIRECTORY 0x00000010
#define FILE_ATTRIBUTE_NORMAL 0x00000080
#define FILE_FLAG_SEQUENTIAL_SCAN 0x08000000
#define FILE_BEGIN 0
#define FILE_CURRENT 1
#define FILE_END 2

typedef struct _SECURITY_ATTRIBUTES
{
    DWORD nLength;
    LPVOID lpSecurityDescriptor;
    BOOL bInheritHandle;
} SECURITY_ATTRIBUTES, *PSECURITY_ATTRIBUTES, *LPSECURITY_ATTRIBUTES;

typedef struct _OVERLAPPED OVERLAPPED, *LPOVERLAPPED;

typedef struct _WIN32_FIND_DATAA
{
    DWORD dwFileAttributes;
    FILETIME ftCreationTime;
    FILETIME ftLastAccessTime;
    FILETIME ftLastWriteTime;
    DWORD nFileSizeHigh;
    DWORD nFileSizeLow;
    DWORD dwReserved0;
    DWORD dwReserved1;
    CHAR cFileName[MAX_PATH];
    CHAR cAlternateFileName[14];
} WIN32_FIND_DATAA, *PWIN32_FIND_DATAA, *LPWIN32_FIND_DATAA;

HANDLE CreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
                   LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition,
                   DWORD dwFlagsAndAttributes, HANDLE hTemplateFile);
BOOL ReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, LPDWORD lpNumberOfBytesRead,
              LPOVERLAPPED lpOverlapped);
BOOL WriteFile(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite, LPDWORD lpNumberOfBytesWritten,
               LPOVERLAPPED lpOverlapped);
DWORD SetFilePointer(HANDLE hFile, LONG lDistanceToMove, PLONG lpDistanceToMoveHigh, DWORD dwMoveMethod);
DWORD GetFileSize(HANDLE hFile, LPDWORD lpFileSizeHigh);
BOOL DeleteFileA(LPCSTR lpFileName);
HANDLE FindFirstFileA(LPCSTR lpFileName, LPWIN32_FIND_DATAA lpFindFileData);
BOOL FindNextFileA(HANDLE hFindFile, LPWIN32_FIND_DATAA lpFindFileData);
BOOL FindClose(HANDLE hFindFile);
BOOL CloseHandle(HANDLE hObject);

// ---------------------------------------------------------------------------
// Processes, modules, errors (processthreadsapi.h, libloaderapi.h, ...)

typedef INT_PTR(WINAPI *FARPROC)();

typedef struct _STARTUPINFOA
{
    DWORD cb;
    LPSTR lpReserved;
    LPSTR lpDesktop;
    LPSTR lpTitle;
    DWORD dwX;
    DWORD dwY;
    DWORD dwXSize;
    DWORD dwYSize;
    DWORD dwXCountChars;
    DWORD dwYCountChars;
    DWORD dwFillAttribute;
    DWORD dwFlags;
    WORD wShowWindow;
    WORD cbReserved2;
    LPBYTE lpReserved2;
    HANDLE hStdInput;
    HANDLE hStdOutput;
    HANDLE hStdError;
} STARTUPINFOA, *LPSTARTUPINFOA;

DWORD GetLastError(void);
void SetLastError(DWORD dwErrCode);
DWORD FormatMessageA(DWORD dwFlags, LPCVOID lpSource, DWORD dwMessageId, DWORD dwLanguageId, LPSTR lpBuffer,
                     DWORD nSize, va_list *Arguments);
DWORD GetModuleFileNameA(HMODULE hModule, LPSTR lpFilename, DWORD nSize);
HMODULE GetModuleHandleA(LPCSTR lpModuleName);
HMODULE LoadLibraryA(LPCSTR lpLibFileName);
FARPROC GetProcAddress(HMODULE hModule, LPCSTR lpProcName);
BOOL FreeLibrary(HMODULE hLibModule);
// The game's entry point (WinMain.cpp), called by port/src/main.cpp.
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd);

void GetStartupInfoA(LPSTARTUPINFOA lpStartupInfo);
DWORD GetEnvironmentVariableA(LPCSTR lpName, LPSTR lpBuffer, DWORD nSize);
DWORD GetConsoleTitleA(LPSTR lpConsoleTitle, DWORD nSize);
DWORD GetCurrentDirectoryA(DWORD nBufferLength, LPSTR lpBuffer);
BOOL SetCurrentDirectoryA(LPCSTR lpPathName);
BOOL CreateDirectoryA(LPCSTR lpPathName, LPSECURITY_ATTRIBUTES lpSecurityAttributes);
void ExitProcess(UINT uExitCode);

// ---------------------------------------------------------------------------
// Threads and synchronisation (synchapi.h, processthreadsapi.h)

#define INFINITE 0xffffffffu
#define WAIT_OBJECT_0 0x00000000
#define WAIT_ABANDONED 0x00000080
#define WAIT_TIMEOUT 258
#define WAIT_FAILED ((DWORD)0xffffffff)
#define STILL_ACTIVE 259

#define THREAD_PRIORITY_NORMAL 0
#define THREAD_PRIORITY_ABOVE_NORMAL 1
#define THREAD_PRIORITY_HIGHEST 2
#define THREAD_PRIORITY_BELOW_NORMAL (-1)
#define THREAD_PRIORITY_LOWEST (-2)

typedef DWORD(WINAPI *PTHREAD_START_ROUTINE)(LPVOID lpThreadParameter);
typedef PTHREAD_START_ROUTINE LPTHREAD_START_ROUTINE;

// The SDK's layout on x86 (24 bytes); the game keeps an array of these in
// CriticalSections. An implementation that needs more room (a pthread
// mutex is larger) allocates it and keeps the pointer in LockSemaphore.
typedef struct _RTL_CRITICAL_SECTION
{
    void *DebugInfo;
    LONG LockCount;
    LONG RecursionCount;
    HANDLE OwningThread;
    HANDLE LockSemaphore;
    ULONG_PTR SpinCount;
} CRITICAL_SECTION, *LPCRITICAL_SECTION, *PCRITICAL_SECTION;

void InitializeCriticalSection(LPCRITICAL_SECTION lpCriticalSection);
void EnterCriticalSection(LPCRITICAL_SECTION lpCriticalSection);
void LeaveCriticalSection(LPCRITICAL_SECTION lpCriticalSection);
void DeleteCriticalSection(LPCRITICAL_SECTION lpCriticalSection);

HANDLE CreateThread(LPSECURITY_ATTRIBUTES lpThreadAttributes, SIZE_T dwStackSize,
                    LPTHREAD_START_ROUTINE lpStartAddress, LPVOID lpParameter, DWORD dwCreationFlags,
                    LPDWORD lpThreadId);
BOOL SetThreadPriority(HANDLE hThread, int nPriority);
BOOL GetExitCodeThread(HANDLE hThread, LPDWORD lpExitCode);
BOOL TerminateThread(HANDLE hThread, DWORD dwExitCode);
DWORD GetCurrentThreadId(void);
HANDLE GetCurrentThread(void);
DWORD WaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds);
DWORD WaitForMultipleObjects(DWORD nCount, const HANDLE *lpHandles, BOOL bWaitAll, DWORD dwMilliseconds);
HANDLE CreateEventA(LPSECURITY_ATTRIBUTES lpEventAttributes, BOOL bManualReset, BOOL bInitialState,
                    LPCSTR lpName);
BOOL SetEvent(HANDLE hEvent);
BOOL ResetEvent(HANDLE hEvent);
HANDLE CreateMutexA(LPSECURITY_ATTRIBUTES lpMutexAttributes, BOOL bInitialOwner, LPCSTR lpName);
BOOL ReleaseMutex(HANDLE hMutex);
void Sleep(DWORD dwMilliseconds);

// ---------------------------------------------------------------------------
// Time (profileapi.h, sysinfoapi.h)

BOOL QueryPerformanceCounter(LARGE_INTEGER *lpPerformanceCount);
BOOL QueryPerformanceFrequency(LARGE_INTEGER *lpFrequency);
DWORD GetTickCount(void);
void GetLocalTime(LPSYSTEMTIME lpSystemTime);

// ---------------------------------------------------------------------------
// Code pages (stringapiset.h)

#define CP_ACP 0
#define CP_OEMCP 1
#define CP_UTF8 65001

int MultiByteToWideChar(UINT CodePage, DWORD dwFlags, LPCSTR lpMultiByteStr, int cbMultiByte,
                        LPWSTR lpWideCharStr, int cchWideChar);
int WideCharToMultiByte(UINT CodePage, DWORD dwFlags, LPCWSTR lpWideCharStr, int cchWideChar,
                        LPSTR lpMultiByteStr, int cbMultiByte, LPCSTR lpDefaultChar, LPBOOL lpUsedDefaultChar);

// ---------------------------------------------------------------------------
// Windows and messages (winuser.h)

typedef LRESULT(CALLBACK *WNDPROC)(HWND, UINT, WPARAM, LPARAM);
typedef INT_PTR(CALLBACK *DLGPROC)(HWND, UINT, WPARAM, LPARAM);
typedef void(CALLBACK *TIMERPROC)(HWND, UINT, UINT_PTR, DWORD);

#define MAKEINTRESOURCEA(i) ((LPSTR)((ULONG_PTR)((WORD)(i))))
#define MAKEINTRESOURCE MAKEINTRESOURCEA

typedef struct tagMSG
{
    HWND hwnd;
    UINT message;
    WPARAM wParam;
    LPARAM lParam;
    DWORD time;
    POINT pt;
} MSG, *PMSG, *LPMSG;

typedef struct tagWNDCLASSA
{
    UINT style;
    WNDPROC lpfnWndProc;
    int cbClsExtra;
    int cbWndExtra;
    HINSTANCE hInstance;
    HICON hIcon;
    HCURSOR hCursor;
    HBRUSH hbrBackground;
    LPCSTR lpszMenuName;
    LPCSTR lpszClassName;
} WNDCLASSA, *PWNDCLASSA, *LPWNDCLASSA;

#define WM_NULL 0x0000
#define WM_CREATE 0x0001
#define WM_DESTROY 0x0002
#define WM_MOVE 0x0003
#define WM_SIZE 0x0005
#define WM_ACTIVATE 0x0006
#define WM_SETFOCUS 0x0007
#define WM_KILLFOCUS 0x0008
#define WM_PAINT 0x000f
#define WM_CLOSE 0x0010
#define WM_QUIT 0x0012
#define WM_ERASEBKGND 0x0014
#define WM_ACTIVATEAPP 0x001c
#define WM_SETCURSOR 0x0020
#define WM_KEYDOWN 0x0100
#define WM_KEYUP 0x0101
#define WM_CHAR 0x0102
#define WM_SYSKEYDOWN 0x0104
#define WM_SYSKEYUP 0x0105
#define WM_INITDIALOG 0x0110
#define WM_COMMAND 0x0111
#define WM_SYSCOMMAND 0x0112
#define WM_TIMER 0x0113
#define WM_MOUSEMOVE 0x0200
#define WM_LBUTTONDOWN 0x0201
#define WM_LBUTTONUP 0x0202
#define WM_USER 0x0400

#define SIZE_RESTORED 0
#define SIZE_MINIMIZED 1
#define SIZE_MAXIMIZED 2
#define SIZE_MAXSHOW 3
#define SIZE_MAXHIDE 4

#define SC_SIZE 0xf000
#define SC_MOVE 0xf010
#define SC_MINIMIZE 0xf020
#define SC_MAXIMIZE 0xf030
#define SC_CLOSE 0xf060
#define SC_MOUSEMENU 0xf090
#define SC_KEYMENU 0xf100
#define SC_RESTORE 0xf120
#define SC_SCREENSAVE 0xf140
#define SC_MONITORPOWER 0xf170

#define WS_OVERLAPPED 0x00000000
#define WS_POPUP 0x80000000
#define WS_CHILD 0x40000000
#define WS_MINIMIZE 0x20000000
#define WS_VISIBLE 0x10000000
#define WS_DISABLED 0x08000000
#define WS_CLIPSIBLINGS 0x04000000
#define WS_CLIPCHILDREN 0x02000000
#define WS_MAXIMIZE 0x01000000
#define WS_CAPTION 0x00C00000
#define WS_BORDER 0x00800000
#define WS_DLGFRAME 0x00400000
#define WS_VSCROLL 0x00200000
#define WS_HSCROLL 0x00100000
#define WS_SYSMENU 0x00080000
#define WS_THICKFRAME 0x00040000
#define WS_MINIMIZEBOX 0x00020000
#define WS_MAXIMIZEBOX 0x00010000
#define WS_OVERLAPPEDWINDOW \
    (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX)
#define WS_EX_TOPMOST 0x00000008
#define WS_EX_APPWINDOW 0x00040000

#define CS_VREDRAW 0x0001
#define CS_HREDRAW 0x0002
#define CS_OWNDC 0x0020

#define CW_USEDEFAULT ((int)0x80000000)

#define SW_HIDE 0
#define SW_SHOWNORMAL 1
#define SW_NORMAL 1
#define SW_SHOWMINIMIZED 2
#define SW_SHOWMAXIMIZED 3
#define SW_SHOW 5
#define SW_MINIMIZE 6
#define SW_RESTORE 9

#define GWL_STYLE (-16)
#define GWL_EXSTYLE (-20)

#define HWND_TOP ((HWND)0)
#define HWND_BOTTOM ((HWND)1)
#define HWND_TOPMOST ((HWND)-1)
#define HWND_NOTOPMOST ((HWND)-2)

#define SWP_NOSIZE 0x0001
#define SWP_NOMOVE 0x0002
#define SWP_NOZORDER 0x0004
#define SWP_NOREDRAW 0x0008
#define SWP_NOACTIVATE 0x0010
#define SWP_FRAMECHANGED 0x0020
#define SWP_SHOWWINDOW 0x0040
#define SWP_HIDEWINDOW 0x0080

#define SM_CXSCREEN 0
#define SM_CYSCREEN 1
#define SM_CYCAPTION 4
#define SM_CXBORDER 5
#define SM_CYBORDER 6
#define SM_CXDLGFRAME 7
#define SM_CYDLGFRAME 8
#define SM_CXFRAME 32
#define SM_CYFRAME 33

#define PM_NOREMOVE 0x0000
#define PM_REMOVE 0x0001

#define QS_KEY 0x0001
#define QS_MOUSEMOVE 0x0002
#define QS_MOUSEBUTTON 0x0004
#define QS_POSTMESSAGE 0x0008
#define QS_TIMER 0x0010
#define QS_PAINT 0x0020
#define QS_SENDMESSAGE 0x0040
#define QS_HOTKEY 0x0080
#define QS_ALLPOSTMESSAGE 0x0100
#define QS_MOUSE (QS_MOUSEMOVE | QS_MOUSEBUTTON)
#define QS_INPUT (QS_MOUSE | QS_KEY)
#define QS_ALLEVENTS (QS_INPUT | QS_POSTMESSAGE | QS_TIMER | QS_PAINT | QS_HOTKEY)

#define MB_OK 0x00000000
#define MB_OKCANCEL 0x00000001
#define MB_YESNO 0x00000004
#define MB_ICONHAND 0x00000010
#define MB_ICONQUESTION 0x00000020
#define MB_ICONEXCLAMATION 0x00000030
#define MB_ICONASTERISK 0x00000040
#define MB_ICONSTOP MB_ICONHAND
#define MB_ICONERROR MB_ICONHAND
#define IDOK 1
#define IDCANCEL 2
#define IDYES 6
#define IDNO 7

#define BM_GETCHECK 0x00F0
#define BM_SETCHECK 0x00F1
#define BST_UNCHECKED 0x0000
#define BST_CHECKED 0x0001

#define IDC_ARROW MAKEINTRESOURCEA(32512)
#define IDI_APPLICATION MAKEINTRESOURCEA(32512)

#define SPI_GETSCREENSAVEACTIVE 0x0010
#define SPI_SETSCREENSAVEACTIVE 0x0011
#define SPI_GETLOWPOWERACTIVE 0x0053
#define SPI_GETPOWEROFFACTIVE 0x0054
#define SPI_SETLOWPOWERACTIVE 0x0055
#define SPI_SETPOWEROFFACTIVE 0x0056
#define SPIF_UPDATEINIFILE 0x0001
#define SPIF_SENDCHANGE 0x0002

#define VK_LBUTTON 0x01
#define VK_RBUTTON 0x02
#define VK_BACK 0x08
#define VK_TAB 0x09
#define VK_RETURN 0x0D
#define VK_SHIFT 0x10
#define VK_CONTROL 0x11
#define VK_MENU 0x12
#define VK_PAUSE 0x13
#define VK_ESCAPE 0x1B
#define VK_SPACE 0x20
#define VK_PRIOR 0x21
#define VK_NEXT 0x22
#define VK_END 0x23
#define VK_HOME 0x24
#define VK_LEFT 0x25
#define VK_UP 0x26
#define VK_RIGHT 0x27
#define VK_DOWN 0x28
#define VK_INSERT 0x2D
#define VK_DELETE 0x2E
#define VK_NUMPAD0 0x60
#define VK_NUMPAD1 0x61
#define VK_NUMPAD2 0x62
#define VK_NUMPAD3 0x63
#define VK_NUMPAD4 0x64
#define VK_NUMPAD5 0x65
#define VK_NUMPAD6 0x66
#define VK_NUMPAD7 0x67
#define VK_NUMPAD8 0x68
#define VK_NUMPAD9 0x69
#define VK_F1 0x70
#define VK_F2 0x71
#define VK_F3 0x72
#define VK_F4 0x73
#define VK_F5 0x74
#define VK_F6 0x75
#define VK_F7 0x76
#define VK_F8 0x77
#define VK_F9 0x78
#define VK_F10 0x79
#define VK_F11 0x7A
#define VK_F12 0x7B

ATOM RegisterClassA(const WNDCLASSA *lpWndClass);
BOOL UnregisterClassA(LPCSTR lpClassName, HINSTANCE hInstance);
HWND CreateWindowExA(DWORD dwExStyle, LPCSTR lpClassName, LPCSTR lpWindowName, DWORD dwStyle, int X, int Y,
                     int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam);
BOOL DestroyWindow(HWND hWnd);
BOOL ShowWindow(HWND hWnd, int nCmdShow);
BOOL UpdateWindow(HWND hWnd);
BOOL MoveWindow(HWND hWnd, int X, int Y, int nWidth, int nHeight, BOOL bRepaint);
BOOL SetWindowPos(HWND hWnd, HWND hWndInsertAfter, int X, int Y, int cx, int cy, UINT uFlags);
LONG SetWindowLongA(HWND hWnd, int nIndex, LONG dwNewLong);
LONG GetWindowLongA(HWND hWnd, int nIndex);
BOOL GetWindowRect(HWND hWnd, LPRECT lpRect);
BOOL GetClientRect(HWND hWnd, LPRECT lpRect);
BOOL AdjustWindowRect(LPRECT lpRect, DWORD dwStyle, BOOL bMenu);
BOOL SetForegroundWindow(HWND hWnd);
HWND GetForegroundWindow(void);
HWND SetFocus(HWND hWnd);
BOOL SetWindowTextA(HWND hWnd, LPCSTR lpString);
LRESULT DefWindowProcA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
LRESULT SendMessageA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
BOOL PostMessageA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
BOOL PostThreadMessageA(DWORD idThread, UINT Msg, WPARAM wParam, LPARAM lParam);
void PostQuitMessage(int nExitCode);
BOOL PeekMessageA(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax, UINT wRemoveMsg);
BOOL GetMessageA(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax);
BOOL TranslateMessage(const MSG *lpMsg);
LRESULT DispatchMessageA(const MSG *lpMsg);
DWORD MsgWaitForMultipleObjects(DWORD nCount, const HANDLE *pHandles, BOOL fWaitAll, DWORD dwMilliseconds,
                                DWORD dwWakeMask);
UINT_PTR SetTimer(HWND hWnd, UINT_PTR nIDEvent, UINT uElapse, TIMERPROC lpTimerFunc);
BOOL KillTimer(HWND hWnd, UINT_PTR uIDEvent);
int MessageBoxA(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption, UINT uType);

HWND CreateDialogParamA(HINSTANCE hInstance, LPCSTR lpTemplateName, HWND hWndParent, DLGPROC lpDialogFunc,
                        LPARAM dwInitParam);
INT_PTR DialogBoxParamA(HINSTANCE hInstance, LPCSTR lpTemplateName, HWND hWndParent, DLGPROC lpDialogFunc,
                        LPARAM dwInitParam);
BOOL EndDialog(HWND hDlg, INT_PTR nResult);
BOOL IsDialogMessageA(HWND hDlg, LPMSG lpMsg);
HWND GetDlgItem(HWND hDlg, int nIDDlgItem);
UINT IsDlgButtonChecked(HWND hDlg, int nIDButton);
BOOL CheckDlgButton(HWND hDlg, int nIDButton, UINT uCheck);
LRESULT SendDlgItemMessageA(HWND hDlg, int nIDDlgItem, UINT Msg, WPARAM wParam, LPARAM lParam);

int GetSystemMetrics(int nIndex);
BOOL SystemParametersInfoA(UINT uiAction, UINT uiParam, PVOID pvParam, UINT fWinIni);
int ShowCursor(BOOL bShow);
HCURSOR SetCursor(HCURSOR hCursor);
HCURSOR LoadCursorA(HINSTANCE hInstance, LPCSTR lpCursorName);
HICON LoadIconA(HINSTANCE hInstance, LPCSTR lpIconName);
BOOL GetKeyboardState(PBYTE lpKeyState);
BOOL SetKeyboardState(LPBYTE lpKeyState);
short GetAsyncKeyState(int vKey);
short GetKeyState(int nVirtKey);
HDC GetDC(HWND hWnd);
int ReleaseDC(HWND hWnd, HDC hDC);

// ---------------------------------------------------------------------------
// GDI (wingdi.h). Bitmap headers are 2-byte packed in the SDK.

#pragma pack(push, 2)
typedef struct tagBITMAPFILEHEADER
{
    WORD bfType;
    DWORD bfSize;
    WORD bfReserved1;
    WORD bfReserved2;
    DWORD bfOffBits;
} BITMAPFILEHEADER, *LPBITMAPFILEHEADER, *PBITMAPFILEHEADER;
#pragma pack(pop)

typedef struct tagBITMAPINFOHEADER
{
    DWORD biSize;
    LONG biWidth;
    LONG biHeight;
    WORD biPlanes;
    WORD biBitCount;
    DWORD biCompression;
    DWORD biSizeImage;
    LONG biXPelsPerMeter;
    LONG biYPelsPerMeter;
    DWORD biClrUsed;
    DWORD biClrImportant;
} BITMAPINFOHEADER, *LPBITMAPINFOHEADER, *PBITMAPINFOHEADER;

typedef LONG FXPT2DOT30;

typedef struct tagCIEXYZ
{
    FXPT2DOT30 ciexyzX;
    FXPT2DOT30 ciexyzY;
    FXPT2DOT30 ciexyzZ;
} CIEXYZ;

typedef struct tagCIEXYZTRIPLE
{
    CIEXYZ ciexyzRed;
    CIEXYZ ciexyzGreen;
    CIEXYZ ciexyzBlue;
} CIEXYZTRIPLE;

typedef struct
{
    DWORD bV4Size;
    LONG bV4Width;
    LONG bV4Height;
    WORD bV4Planes;
    WORD bV4BitCount;
    DWORD bV4V4Compression;
    DWORD bV4SizeImage;
    LONG bV4XPelsPerMeter;
    LONG bV4YPelsPerMeter;
    DWORD bV4ClrUsed;
    DWORD bV4ClrImportant;
    DWORD bV4RedMask;
    DWORD bV4GreenMask;
    DWORD bV4BlueMask;
    DWORD bV4AlphaMask;
    DWORD bV4CSType;
    CIEXYZTRIPLE bV4Endpoints;
    DWORD bV4GammaRed;
    DWORD bV4GammaGreen;
    DWORD bV4GammaBlue;
} BITMAPV4HEADER, *LPBITMAPV4HEADER, *PBITMAPV4HEADER;

typedef struct tagRGBQUAD
{
    BYTE rgbBlue;
    BYTE rgbGreen;
    BYTE rgbRed;
    BYTE rgbReserved;
} RGBQUAD;

typedef struct tagBITMAPINFO
{
    BITMAPINFOHEADER bmiHeader;
    RGBQUAD bmiColors[1];
} BITMAPINFO, *LPBITMAPINFO, *PBITMAPINFO;

typedef struct tagPALETTEENTRY
{
    BYTE peRed;
    BYTE peGreen;
    BYTE peBlue;
    BYTE peFlags;
} PALETTEENTRY, *PPALETTEENTRY, *LPPALETTEENTRY;

#define BI_RGB 0
#define BI_BITFIELDS 3
#define DIB_RGB_COLORS 0
#define DIB_PAL_COLORS 1

#define LF_FACESIZE 32

typedef struct tagLOGFONTA
{
    LONG lfHeight;
    LONG lfWidth;
    LONG lfEscapement;
    LONG lfOrientation;
    LONG lfWeight;
    BYTE lfItalic;
    BYTE lfUnderline;
    BYTE lfStrikeOut;
    BYTE lfCharSet;
    BYTE lfOutPrecision;
    BYTE lfClipPrecision;
    BYTE lfQuality;
    BYTE lfPitchAndFamily;
    CHAR lfFaceName[LF_FACESIZE];
} LOGFONTA, *PLOGFONTA, *LPLOGFONTA;

#define LF_FULLFACESIZE 64

typedef struct tagENUMLOGFONTEXA
{
    LOGFONTA elfLogFont;
    BYTE elfFullName[LF_FULLFACESIZE];
    BYTE elfStyle[LF_FACESIZE];
    BYTE elfScript[LF_FACESIZE];
} ENUMLOGFONTEXA, *LPENUMLOGFONTEXA;

typedef struct tagTEXTMETRICA
{
    LONG tmHeight;
    LONG tmAscent;
    LONG tmDescent;
    LONG tmInternalLeading;
    LONG tmExternalLeading;
    LONG tmAveCharWidth;
    LONG tmMaxCharWidth;
    LONG tmWeight;
    LONG tmOverhang;
    LONG tmDigitizedAspectX;
    LONG tmDigitizedAspectY;
    BYTE tmFirstChar;
    BYTE tmLastChar;
    BYTE tmDefaultChar;
    BYTE tmBreakChar;
    BYTE tmItalic;
    BYTE tmUnderlined;
    BYTE tmStruckOut;
    BYTE tmPitchAndFamily;
    BYTE tmCharSet;
} TEXTMETRICA, *PTEXTMETRICA, *LPTEXTMETRICA;

typedef int(CALLBACK *FONTENUMPROCA)(const LOGFONTA *, const TEXTMETRICA *, DWORD, LPARAM);

#define FW_DONTCARE 0
#define FW_THIN 100
#define FW_LIGHT 300
#define FW_NORMAL 400
#define FW_REGULAR 400
#define FW_MEDIUM 500
#define FW_SEMIBOLD 600
#define FW_BOLD 700
#define FW_HEAVY 900

#define ANSI_CHARSET 0
#define DEFAULT_CHARSET 1
#define SHIFTJIS_CHARSET 128

#define OUT_DEFAULT_PRECIS 0
#define OUT_TT_PRECIS 4
#define CLIP_DEFAULT_PRECIS 0
#define DEFAULT_QUALITY 0
#define DRAFT_QUALITY 1
#define PROOF_QUALITY 2
#define NONANTIALIASED_QUALITY 3
#define ANTIALIASED_QUALITY 4
#define DEFAULT_PITCH 0
#define FIXED_PITCH 1
#define VARIABLE_PITCH 2
#define FF_DONTCARE (0 << 4)
#define FF_ROMAN (1 << 4)
#define FF_SWISS (2 << 4)
#define FF_MODERN (3 << 4)

#define TRANSPARENT 1
#define OPAQUE 2

#define WHITE_BRUSH 0
#define LTGRAY_BRUSH 1
#define GRAY_BRUSH 2
#define DKGRAY_BRUSH 3
#define BLACK_BRUSH 4
#define NULL_BRUSH 5

#define HORZRES 8
#define VERTRES 10
#define BITSPIXEL 12
#define LOGPIXELSX 88
#define LOGPIXELSY 90
#define VREFRESH 116

#define SRCCOPY 0x00CC0020u

HFONT CreateFontA(int cHeight, int cWidth, int cEscapement, int cOrientation, int cWeight, DWORD bItalic,
                  DWORD bUnderline, DWORD bStrikeOut, DWORD iCharSet, DWORD iOutPrecision, DWORD iClipPrecision,
                  DWORD iQuality, DWORD iPitchAndFamily, LPCSTR pszFaceName);
HFONT CreateFontIndirectA(const LOGFONTA *lplf);
HGDIOBJ SelectObject(HDC hdc, HGDIOBJ h);
BOOL DeleteObject(HGDIOBJ ho);
HGDIOBJ GetStockObject(int i);
HDC CreateCompatibleDC(HDC hdc);
BOOL DeleteDC(HDC hdc);
int GetDeviceCaps(HDC hdc, int index);
HBITMAP CreateDIBSection(HDC hdc, const BITMAPINFO *pbmi, UINT usage, void **ppvBits, HANDLE hSection,
                         DWORD offset);
COLORREF SetTextColor(HDC hdc, COLORREF color);
COLORREF SetBkColor(HDC hdc, COLORREF color);
int SetBkMode(HDC hdc, int mode);
BOOL TextOutA(HDC hdc, int x, int y, LPCSTR lpString, int c);
BOOL GetTextExtentPoint32A(HDC hdc, LPCSTR lpString, int c, LPSIZE psizl);
BOOL GetTextMetricsA(HDC hdc, LPTEXTMETRICA lptm);
int EnumFontFamiliesExA(HDC hdc, LPLOGFONTA lpLogfont, FONTENUMPROCA lpProc, LPARAM lParam, DWORD dwFlags);
BOOL GdiFlush(void);

#ifdef __cplusplus
}
#endif

// The A/W macro names used without a suffix.
#define CreateFont CreateFontA
#define TextOut TextOutA
#define SendMessage SendMessageA
#define MessageBox MessageBoxA
#define CreateFile CreateFileA
#define DeleteFile DeleteFileA
#define LoadCursor LoadCursorA
#define LOGFONT LOGFONTA
#define TEXTMETRIC TEXTMETRICA
