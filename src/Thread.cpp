#include <process.h>

#include "Thread.h"

// GLOBAL: TH16 0x4c1a68
ThreadInf g_Thread_4c1a68;

ThreadInf::~ThreadInf()
{
    join_if_running();
}

// SYNTHETIC: TH16 0x402ef0
// ThreadInf::`scalar deleting destructor'

// FUNCTION: TH16 0x402f30
void ThreadInf::join_if_running()
{
    if (handle == NULL)
    {
        return;
    }
    stop_requested = TRUE;
    should_run = FALSE;
    while (WaitForSingleObject(handle, 200) == WAIT_TIMEOUT)
    {
        stop_requested = TRUE;
        should_run = FALSE;
        Sleep(1);
    }
    CloseHandle(handle);
    handle = NULL;
    start = NULL;
}

// FUNCTION: TH16 0x402fb0
void ThreadInf::restart(ThreadStart start, void *arg)
{
    join_if_running();
    this->start = start;
    should_run = TRUE;
    stop_requested = FALSE;
    handle = (HANDLE)_beginthreadex(NULL, 0, start, arg, 0, &id);
}
