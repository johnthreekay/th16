#pragma once

#include "types.h"

// Collects log lines in one buffer; on exit the game shows them in a
// message box if fatal() was called. TH06 equivalent: GameErrorContext
// (with a 0x800-byte buffer).
struct GameErrorContext
{
    char buffer[0x2000];
    char *buffer_end;
    // Set by fatal().
    i8 show_message_box;

    // Simple enough to be evaluated at compile time; only the empty
    // destructor is registered at startup (0x401010).
    GameErrorContext()
    {
        buffer_end = buffer;
        buffer[0] = '\0';
        show_message_box = false;
    }
    ~GameErrorContext()
    {
    }

    // Variadic member functions are __cdecl with this as the first stack
    // argument.
    const char *log(const char *fmt, ...);
    const char *fatal(const char *fmt, ...);
};

extern GameErrorContext g_GameErrorContext;
