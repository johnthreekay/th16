#pragma once

#include <windows.h>

#include "decomp.h"

typedef unsigned(__stdcall *ThreadStart)(void *arg);

// VTABLE: TH16 0x491c34
// The name is ZUN's, from RTTI.
class ThreadInf
{
  public:
    HANDLE handle;
    unsigned id;
    // Set while join_if_running waits for the thread to notice.
    BOOL stop_requested;
    BOOL should_run;
    int unk_14;
    ThreadStart start;

    virtual ~ThreadInf();
    DECOMP_NOINLINE void join_if_running();
    void restart(ThreadStart start, void *arg);
};
