// kernel32: threads, synchronisation and time.
//
// Critical sections and the clocks are implemented (the game takes its
// locks and reads the time from the first frame, and both map directly onto
// the C++ library). Threads, events and mutexes are stubs: the game's
// loading thread, BGM streaming thread and screenshot writer need HANDLE
// objects that can be waited on, which the platform layer provides next.
#include <chrono>
#include <mutex>
#include <thread>

#include <mmsystem.h>
#include <windows.h>

#include "port_stub.h"

// ---------------------------------------------------------------------------
// Critical sections. CRITICAL_SECTION keeps its 24-byte x86 layout (the game
// embeds an array of them in CriticalSections); the mutex lives on the heap
// with its pointer in LockSemaphore.

static std::recursive_mutex *mutex_of(LPCRITICAL_SECTION cs)
{
    return (std::recursive_mutex *)cs->LockSemaphore;
}

extern "C" {

void InitializeCriticalSection(LPCRITICAL_SECTION lpCriticalSection)
{
    memset(lpCriticalSection, 0, sizeof(*lpCriticalSection));
    lpCriticalSection->LockCount = -1;
    lpCriticalSection->LockSemaphore = (HANDLE) new std::recursive_mutex();
}

void EnterCriticalSection(LPCRITICAL_SECTION lpCriticalSection)
{
    mutex_of(lpCriticalSection)->lock();
    lpCriticalSection->RecursionCount++;
}

void LeaveCriticalSection(LPCRITICAL_SECTION lpCriticalSection)
{
    lpCriticalSection->RecursionCount--;
    mutex_of(lpCriticalSection)->unlock();
}

void DeleteCriticalSection(LPCRITICAL_SECTION lpCriticalSection)
{
    delete mutex_of(lpCriticalSection);
    lpCriticalSection->LockSemaphore = NULL;
}

// ---------------------------------------------------------------------------
// Time

void Sleep(DWORD dwMilliseconds)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(dwMilliseconds));
}

// One clock for every timer, in nanoseconds since an arbitrary start.
static int64_t now_ns()
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

BOOL QueryPerformanceCounter(LARGE_INTEGER *lpPerformanceCount)
{
    lpPerformanceCount->QuadPart = now_ns();
    return TRUE;
}

BOOL QueryPerformanceFrequency(LARGE_INTEGER *lpFrequency)
{
    lpFrequency->QuadPart = 1000000000;
    return TRUE;
}

DWORD GetTickCount(void)
{
    return (DWORD)(now_ns() / 1000000);
}

DWORD timeGetTime(void)
{
    return (DWORD)(now_ns() / 1000000);
}

MMRESULT timeBeginPeriod(UINT uPeriod)
{
    return TIMERR_NOERROR;
}

MMRESULT timeEndPeriod(UINT uPeriod)
{
    return TIMERR_NOERROR;
}

void GetLocalTime(LPSYSTEMTIME lpSystemTime)
{
    PORT_UNIMPLEMENTED();
    memset(lpSystemTime, 0, sizeof(*lpSystemTime));
}

// ---------------------------------------------------------------------------
// Threads, events, mutexes (stubs)

HANDLE CreateThread(LPSECURITY_ATTRIBUTES lpThreadAttributes, SIZE_T dwStackSize,
                    LPTHREAD_START_ROUTINE lpStartAddress, LPVOID lpParameter, DWORD dwCreationFlags,
                    LPDWORD lpThreadId)
{
    PORT_UNIMPLEMENTED();
    return NULL;
}

BOOL SetThreadPriority(HANDLE hThread, int nPriority)
{
    PORT_UNIMPLEMENTED();
    return FALSE;
}

BOOL GetExitCodeThread(HANDLE hThread, LPDWORD lpExitCode)
{
    PORT_UNIMPLEMENTED();
    return FALSE;
}

BOOL TerminateThread(HANDLE hThread, DWORD dwExitCode)
{
    PORT_UNIMPLEMENTED();
    return FALSE;
}

DWORD GetCurrentThreadId(void)
{
    PORT_UNIMPLEMENTED();
    return 0;
}

HANDLE GetCurrentThread(void)
{
    PORT_UNIMPLEMENTED();
    return NULL;
}

DWORD WaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds)
{
    PORT_UNIMPLEMENTED();
    return WAIT_FAILED;
}

DWORD WaitForMultipleObjects(DWORD nCount, const HANDLE *lpHandles, BOOL bWaitAll, DWORD dwMilliseconds)
{
    PORT_UNIMPLEMENTED();
    return WAIT_FAILED;
}

HANDLE CreateEventA(LPSECURITY_ATTRIBUTES lpEventAttributes, BOOL bManualReset, BOOL bInitialState, LPCSTR lpName)
{
    PORT_UNIMPLEMENTED();
    return NULL;
}

BOOL SetEvent(HANDLE hEvent)
{
    PORT_UNIMPLEMENTED();
    return FALSE;
}

BOOL ResetEvent(HANDLE hEvent)
{
    PORT_UNIMPLEMENTED();
    return FALSE;
}

// WinMain's single-instance check: a named mutex that already exists means
// another copy is running (GetLastError() == ERROR_ALREADY_EXISTS).
// The game quits when this returns NULL, so the stub hands out a dummy
// handle and never reports another instance.
HANDLE CreateMutexA(LPSECURITY_ATTRIBUTES lpMutexAttributes, BOOL bInitialOwner, LPCSTR lpName)
{
    static int dummy_mutex;
    PORT_UNIMPLEMENTED();
    SetLastError(ERROR_SUCCESS);
    return &dummy_mutex;
}

BOOL ReleaseMutex(HANDLE hMutex)
{
    PORT_UNIMPLEMENTED();
    return FALSE;
}

} // extern "C"
