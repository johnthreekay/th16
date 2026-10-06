// Placeholders compiled with /GL for functions that wave 3 range B
// (0x4190b0-0x42b480) calls, where LTCG has to see a body (see
// src/placeholder/unit2.cpp). Each forwards to an opaque stub.
#include <stdarg.h>
#include <stdio.h>

#include "../AnmManager.h"

int w3b_placeholder_sink(void *object, int value);

// Formats into a stack buffer like the real one; its callers realign their
// frames (and esp, -8) because of it.
// STUB: TH16 0x46d990
void AnmManager::draw_text(AnmVm *vm, D3DCOLOR color, i32 unk_10, i32 font, i32 x, i32 y, const char *fmt, ...)
{
    char buf[0x80];
    va_list args;
    va_start(args, fmt);
    vsprintf(buf, fmt, args);
    va_end(args);
    volatile double d = x;
    w3b_placeholder_sink(vm, w3b_placeholder_sink(buf, color + unk_10 + font + (i32)d + y));
}
