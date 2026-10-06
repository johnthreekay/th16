#pragma once

#include "types.h"

// Collects log lines; on exit the game shows them if fatal() was called.
// TH06 equivalent: GameErrorContext (with a 0x800-byte buffer).
struct GameErrorContext
{
    char buffer[0x2000];
    char *buffer_end;
    i8 show_message_box;

    // Variadic member functions are __cdecl with this as the first stack
    // argument.
    const char *log(const char *fmt, ...);
    const char *fatal(const char *fmt, ...);
};
