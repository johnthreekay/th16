#pragma once

#include <windows.h>

#include "decomp.h"

typedef unsigned(__stdcall *ThreadStart)(void *arg);

// A worker thread owned by a game object (Supervisor, AnmManager, Ending,
// ...). The thread function polls stop_requested and should_run to know
// when to end.
// The name is ZUN's, from RTTI.
// VTABLE: TH16 0x491c34
class ThreadInf
{
  public:
    HANDLE handle;
    unsigned id;
    // Set while join_if_running waits for the thread to notice.
    BOOL stop_requested;
    BOOL should_run;
    // Never used.
    int unk_14;
    ThreadStart start;

    ThreadInf()
    {
        handle = NULL;
        id = 0;
        stop_requested = FALSE;
        should_run = FALSE;
    }
    virtual ~ThreadInf();
    // Asks the thread to stop and waits for it to end, then closes it.
    DECOMP_NOINLINE void join_if_running();
    // Stops the running thread, if any, and starts start(arg) in a new one.
    void restart(ThreadStart start, void *arg);
};
