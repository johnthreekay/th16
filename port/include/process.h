// Stand-in for the MSVC CRT's process.h: _beginthread and _beginthreadex.
#pragma once

#include <stdint.h>

extern "C" {
uintptr_t _beginthread(void(__cdecl *start_address)(void *), unsigned stack_size, void *arglist);
uintptr_t _beginthreadex(void *security, unsigned stack_size, unsigned(__stdcall *start_address)(void *),
                         void *arglist, unsigned initflag, unsigned *thrdaddr);
void _endthread(void);
void _endthreadex(unsigned retval);
}
