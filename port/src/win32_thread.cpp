// kernel32: threads, synchronisation, thread message queues and time.
//
// Threads run on std::thread. Thread, event and mutex handles are waitable
// (port_wait): every waitable state changes under g_port_kernel_lock and
// wakes g_port_kernel_cond, which waiters sleep on. Each thread the game
// starts, and the main thread, has a message queue for PostThreadMessageA
// (the BGM thread quits on a posted WM_QUIT, waking from
// MsgWaitForMultipleObjects).
#include <errno.h>
#include <string.h>
#include <sys/file.h>
#include <sys/time.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>

#include <atomic>
#include <functional>
#include <vector>
#include <chrono>
#include <map>
#include <mutex>
#include <string>
#include <thread>

#include <mmsystem.h>
#include <process.h>
#include <windows.h>

#include "port_kernel.h"
#include "port_platform.h"
#include "port_stub.h"

std::mutex g_port_kernel_lock;
std::condition_variable g_port_kernel_cond;

namespace
{

// ---------------------------------------------------------------------------
// Thread ids and message queues

std::atomic<DWORD> g_next_thread_id{0x100};
thread_local DWORD t_thread_id;
DWORD g_main_thread_id;

struct MessageQueue
{
    std::deque<MSG> messages;
};

// Under g_port_kernel_lock.
std::map<DWORD, MessageQueue> g_queues;

void ensure_queue_locked(DWORD id)
{
    g_queues[id];
}

// ---------------------------------------------------------------------------
// Objects

struct PortThread : PortObject
{
    DWORD id;
    bool finished = false;
    DWORD exit_code = STILL_ACTIVE;

    PortThread() : PortObject(PORT_OBJECT_THREAD), id(g_next_thread_id++)
    {
    }
    bool is_signaled(DWORD thread_id) override
    {
        return finished;
    }
};

struct PortEvent : PortObject
{
    bool manual_reset;
    bool signaled;

    PortEvent(bool manual_reset, bool signaled)
        : PortObject(PORT_OBJECT_EVENT), manual_reset(manual_reset), signaled(signaled)
    {
    }
    bool is_signaled(DWORD thread_id) override
    {
        return signaled;
    }
    void on_wait_satisfied(DWORD thread_id) override
    {
        if (!manual_reset)
        {
            signaled = false;
        }
    }
};

struct PortMutex : PortObject
{
    DWORD owner = 0;
    int count = 0;
    // The lock file of a named mutex that guards a single instance.
    int lock_fd = -1;

    PortMutex() : PortObject(PORT_OBJECT_MUTEX)
    {
    }
    ~PortMutex() override
    {
        if (lock_fd >= 0)
        {
            close(lock_fd);
        }
    }
    bool is_signaled(DWORD thread_id) override
    {
        return owner == 0 || owner == thread_id;
    }
    void on_wait_satisfied(DWORD thread_id) override
    {
        owner = thread_id;
        count++;
    }
};

// Runs a thread's start routine and marks it finished (which signals its
// handle). close_handle: _beginthread's handle closes itself at the end.
void run_thread(std::shared_ptr<PortThread> thread, std::function<DWORD()> body, HANDLE close_handle)
{
    t_thread_id = thread->id;
    DWORD exit_code = body();
    {
        std::lock_guard<std::mutex> guard(g_port_kernel_lock);
        thread->finished = true;
        thread->exit_code = exit_code;
        g_queues.erase(thread->id);
    }
    g_port_kernel_cond.notify_all();
    if (close_handle != NULL)
    {
        port_handle_close(close_handle);
    }
}

HANDLE start_thread(std::function<DWORD()> body, LPDWORD thread_id, bool self_closing)
{
    std::shared_ptr<PortThread> thread = std::make_shared<PortThread>();
    {
        std::lock_guard<std::mutex> guard(g_port_kernel_lock);
        ensure_queue_locked(thread->id);
    }
    HANDLE handle = port_handle_create(thread);
    if (thread_id != NULL)
    {
        *thread_id = thread->id;
    }
    try
    {
        std::thread(run_thread, thread, std::move(body), self_closing ? handle : (HANDLE)NULL).detach();
    }
    catch (...)
    {
        port_handle_close(handle);
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return NULL;
    }
    return handle;
}

int64_t now_ns()
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

} // namespace

DWORD port_current_thread_id(void)
{
    if (t_thread_id == 0)
    {
        t_thread_id = g_next_thread_id++;
    }
    return t_thread_id;
}

DWORD port_main_thread_id(void)
{
    return g_main_thread_id;
}

// The main thread gets its id and queue before anything else runs.
static struct MainThreadInit
{
    MainThreadInit()
    {
        g_main_thread_id = port_current_thread_id();
        g_queues[g_main_thread_id];
    }
} g_main_thread_init;

bool port_post_thread_message(DWORD thread_id, const MSG &msg)
{
    {
        std::lock_guard<std::mutex> guard(g_port_kernel_lock);
        auto queue = g_queues.find(thread_id);
        if (queue == g_queues.end())
        {
            return false;
        }
        queue->second.messages.push_back(msg);
    }
    g_port_kernel_cond.notify_all();
    return true;
}

bool port_peek_thread_message(MSG *msg, bool remove)
{
    std::lock_guard<std::mutex> guard(g_port_kernel_lock);
    MessageQueue &queue = g_queues[port_current_thread_id()];
    if (queue.messages.empty())
    {
        return false;
    }
    *msg = queue.messages.front();
    if (remove)
    {
        queue.messages.pop_front();
    }
    return true;
}

DWORD port_wait(DWORD count, const HANDLE *handles, BOOL wait_all, DWORD milliseconds, bool wake_on_message)
{
    std::vector<std::shared_ptr<PortObject>> objects;
    for (DWORD i = 0; i < count; i++)
    {
        std::shared_ptr<PortObject> object = port_handle_get(handles[i]);
        if (object == NULL)
        {
            SetLastError(ERROR_INVALID_HANDLE);
            return WAIT_FAILED;
        }
        objects.push_back(object);
    }
    DWORD self = port_current_thread_id();
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(milliseconds);
    std::unique_lock<std::mutex> lock(g_port_kernel_lock);
    for (;;)
    {
        if (wait_all && count > 0)
        {
            bool all = true;
            for (const auto &object : objects)
            {
                all = all && object->is_signaled(self);
            }
            if (all)
            {
                for (const auto &object : objects)
                {
                    object->on_wait_satisfied(self);
                }
                return WAIT_OBJECT_0;
            }
        }
        else
        {
            for (DWORD i = 0; i < count; i++)
            {
                if (objects[i]->is_signaled(self))
                {
                    objects[i]->on_wait_satisfied(self);
                    return WAIT_OBJECT_0 + i;
                }
            }
        }
        if (wake_on_message && !g_queues[self].messages.empty())
        {
            return WAIT_OBJECT_0 + count;
        }
        if (milliseconds == 0)
        {
            return WAIT_TIMEOUT;
        }
        if (milliseconds == INFINITE)
        {
            g_port_kernel_cond.wait(lock);
        }
        else if (g_port_kernel_cond.wait_until(lock, deadline) == std::cv_status::timeout &&
                 std::chrono::steady_clock::now() >= deadline)
        {
            // One last look before giving up.
            milliseconds = 0;
        }
    }
}

extern "C" {

// ---------------------------------------------------------------------------
// Critical sections. CRITICAL_SECTION keeps its 24-byte x86 layout (the game
// embeds an array of them in CriticalSections); the mutex lives on the heap
// with its pointer in LockSemaphore.

static std::recursive_mutex *mutex_of(LPCRITICAL_SECTION cs)
{
    return (std::recursive_mutex *)cs->LockSemaphore;
}

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
// Time. One clock (steady, nanoseconds) for every timer; SDL_GetTicks uses
// the same monotonic clock on Linux.

void Sleep(DWORD dwMilliseconds)
{
    if (dwMilliseconds == 0)
    {
        std::this_thread::yield();
        return;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(dwMilliseconds));
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
    struct timeval tv;
    gettimeofday(&tv, NULL);
    time_t seconds = tv.tv_sec;
    struct tm local;
    localtime_r(&seconds, &local);
    lpSystemTime->wYear = (WORD)(local.tm_year + 1900);
    lpSystemTime->wMonth = (WORD)(local.tm_mon + 1);
    lpSystemTime->wDayOfWeek = (WORD)local.tm_wday;
    lpSystemTime->wDay = (WORD)local.tm_mday;
    lpSystemTime->wHour = (WORD)local.tm_hour;
    lpSystemTime->wMinute = (WORD)local.tm_min;
    lpSystemTime->wSecond = (WORD)local.tm_sec;
    lpSystemTime->wMilliseconds = (WORD)(tv.tv_usec / 1000);
}

// ---------------------------------------------------------------------------
// Threads

HANDLE CreateThread(LPSECURITY_ATTRIBUTES lpThreadAttributes, SIZE_T dwStackSize,
                    LPTHREAD_START_ROUTINE lpStartAddress, LPVOID lpParameter, DWORD dwCreationFlags,
                    LPDWORD lpThreadId)
{
    if (dwCreationFlags & 4)
    {
        // CREATE_SUSPENDED: the game never asks for it.
        port_log("CreateThread: CREATE_SUSPENDED is not supported");
    }
    return start_thread([=]() { return lpStartAddress(lpParameter); }, lpThreadId, false);
}

// Priorities are left to the host scheduler.
BOOL SetThreadPriority(HANDLE hThread, int nPriority)
{
    return TRUE;
}

BOOL GetExitCodeThread(HANDLE hThread, LPDWORD lpExitCode)
{
    std::shared_ptr<PortObject> object = port_handle_get(hThread, PORT_OBJECT_THREAD);
    if (object == NULL)
    {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    std::lock_guard<std::mutex> guard(g_port_kernel_lock);
    *lpExitCode = ((PortThread *)object.get())->exit_code;
    return TRUE;
}

// A thread cannot be killed safely; the game never needs to.
BOOL TerminateThread(HANDLE hThread, DWORD dwExitCode)
{
    port_log("TerminateThread is not supported");
    SetLastError(ERROR_NOT_SUPPORTED);
    return FALSE;
}

DWORD GetCurrentThreadId(void)
{
    return port_current_thread_id();
}

// The pseudo handle for the calling thread.
HANDLE GetCurrentThread(void)
{
    return (HANDLE)(intptr_t)-2;
}

uintptr_t _beginthreadex(void *security, unsigned stack_size, unsigned(__stdcall *start_address)(void *),
                         void *arglist, unsigned initflag, unsigned *thrdaddr)
{
    DWORD id;
    HANDLE handle = start_thread([=]() { return (DWORD)start_address(arglist); }, &id, false);
    if (handle == NULL)
    {
        errno = EAGAIN;
        return 0;
    }
    if (thrdaddr != NULL)
    {
        *thrdaddr = id;
    }
    return (uintptr_t)handle;
}

// _beginthread's handle closes itself when the thread ends (the CRT
// closes it); the game only checks it against 0.
uintptr_t _beginthread(void(__cdecl *start_address)(void *), unsigned stack_size, void *arglist)
{
    HANDLE handle = start_thread(
        [=]() {
            start_address(arglist);
            return (DWORD)0;
        },
        NULL, true);
    if (handle == NULL)
    {
        errno = EAGAIN;
        return (uintptr_t)-1;
    }
    return (uintptr_t)handle;
}

// Never called by the game (its threads return); there is no portable way
// to end a std::thread from inside other than returning.
void _endthread(void)
{
    port_log("_endthread is not supported");
    abort();
}

void _endthreadex(unsigned retval)
{
    port_log("_endthreadex is not supported");
    abort();
}

// ---------------------------------------------------------------------------
// Waits, events and mutexes

DWORD WaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds)
{
    return port_wait(1, &hHandle, FALSE, dwMilliseconds, false);
}

DWORD WaitForMultipleObjects(DWORD nCount, const HANDLE *lpHandles, BOOL bWaitAll, DWORD dwMilliseconds)
{
    return port_wait(nCount, lpHandles, bWaitAll, dwMilliseconds, false);
}

HANDLE CreateEventA(LPSECURITY_ATTRIBUTES lpEventAttributes, BOOL bManualReset, BOOL bInitialState, LPCSTR lpName)
{
    if (lpName != NULL)
    {
        port_log("CreateEventA: named events are process-local");
    }
    return port_handle_create(std::make_shared<PortEvent>(bManualReset != FALSE, bInitialState != FALSE));
}

static BOOL set_event_state(HANDLE hEvent, bool signaled)
{
    std::shared_ptr<PortObject> object = port_handle_get(hEvent, PORT_OBJECT_EVENT);
    if (object == NULL)
    {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    {
        std::lock_guard<std::mutex> guard(g_port_kernel_lock);
        ((PortEvent *)object.get())->signaled = signaled;
    }
    if (signaled)
    {
        g_port_kernel_cond.notify_all();
    }
    return TRUE;
}

BOOL SetEvent(HANDLE hEvent)
{
    return set_event_state(hEvent, true);
}

BOOL ResetEvent(HANDLE hEvent)
{
    return set_event_state(hEvent, false);
}

// WinMain's single-instance check: a named mutex that already exists means
// another copy of the game is running (GetLastError() ==
// ERROR_ALREADY_EXISTS). Named mutexes are process-local, except that the
// first one holds a lock on a file in the save folder, so a second copy of
// the port using the same save folder sees the first.
HANDLE CreateMutexA(LPSECURITY_ATTRIBUTES lpMutexAttributes, BOOL bInitialOwner, LPCSTR lpName)
{
    std::shared_ptr<PortMutex> mutex = std::make_shared<PortMutex>();
    bool already_exists = false;
    if (lpName != NULL && port_save_dir()[0] != '\0')
    {
        std::string path = std::string(port_save_dir()) + "/th16-port.lock";
        mutex->lock_fd = open(path.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0644);
        if (mutex->lock_fd >= 0 && flock(mutex->lock_fd, LOCK_EX | LOCK_NB) != 0)
        {
            already_exists = true;
        }
    }
    if (bInitialOwner)
    {
        mutex->owner = port_current_thread_id();
        mutex->count = 1;
    }
    HANDLE handle = port_handle_create(mutex);
    SetLastError(already_exists ? ERROR_ALREADY_EXISTS : ERROR_SUCCESS);
    return handle;
}

BOOL ReleaseMutex(HANDLE hMutex)
{
    std::shared_ptr<PortObject> object = port_handle_get(hMutex, PORT_OBJECT_MUTEX);
    if (object == NULL)
    {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    {
        std::lock_guard<std::mutex> guard(g_port_kernel_lock);
        PortMutex *mutex = (PortMutex *)object.get();
        if (mutex->owner != port_current_thread_id())
        {
            SetLastError(288); // ERROR_NOT_OWNER
            return FALSE;
        }
        if (--mutex->count == 0)
        {
            mutex->owner = 0;
        }
    }
    g_port_kernel_cond.notify_all();
    return TRUE;
}

} // extern "C"
