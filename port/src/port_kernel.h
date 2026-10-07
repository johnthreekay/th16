// The kernel objects behind HANDLE values (files, find searches, threads,
// events, mutexes) and the per-thread message queues. Internal to the
// platform layer: win32_kernel.cpp (handle table, files), win32_thread.cpp
// (threads, synchronisation, thread messages) and win32_user.cpp (window
// messages).
//
// A HANDLE is an index into a table ((index + 1) * 4, like the small values
// Windows hands out), never a pointer, so stale or foreign values (the game
// closes NULL handles, and handles twice) are recognised and rejected.
#pragma once

#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>

#include <windows.h>

enum PortObjectType
{
    PORT_OBJECT_FILE,
    PORT_OBJECT_FIND,
    PORT_OBJECT_THREAD,
    PORT_OBJECT_EVENT,
    PORT_OBJECT_MUTEX,
};

struct PortObject
{
    const PortObjectType type;

    explicit PortObject(PortObjectType type) : type(type)
    {
    }
    virtual ~PortObject()
    {
    }

    // Waitable objects: whether a wait by thread_id would succeed now, and
    // what a successful wait does (an auto-reset event resets, a mutex is
    // taken). Both run with g_port_kernel_lock held.
    virtual bool is_signaled(DWORD thread_id)
    {
        return true;
    }
    virtual void on_wait_satisfied(DWORD thread_id)
    {
    }
};

// Every waitable state (signal flags, thread exit, message queues) changes
// under this lock, and every change notifies this condition variable.
extern std::mutex g_port_kernel_lock;
extern std::condition_variable g_port_kernel_cond;

HANDLE port_handle_create(std::shared_ptr<PortObject> object);
// The object behind a handle, or NULL if the handle is not open (or not of
// the given type; any type when type < 0).
std::shared_ptr<PortObject> port_handle_get(HANDLE handle, int type = -1);
// Removes the handle; the object lives on while something else holds it
// (a running thread holds its own object).
bool port_handle_close(HANDLE handle);

// The calling thread's id (GetCurrentThreadId), assigned on first use.
DWORD port_current_thread_id(void);
DWORD port_main_thread_id(void);

// Thread message queues (PostThreadMessageA, PostMessageA, PeekMessageA).
// Every thread started through CreateThread/_beginthread(ex) and the main
// thread have one from the start.
bool port_post_thread_message(DWORD thread_id, const MSG &msg);
// Takes (remove) or copies the first message of the calling thread's queue.
bool port_peek_thread_message(MSG *msg, bool remove);
// Waits on handles and, with wake_on_message, on the calling thread's queue
// (WAIT_OBJECT_0 + count when a message is there). WaitForMultipleObjects
// semantics otherwise.
DWORD port_wait(DWORD count, const HANDLE *handles, BOOL wait_all, DWORD milliseconds, bool wake_on_message);
