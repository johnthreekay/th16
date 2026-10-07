// Small helpers placed among the Gui code.
#include "Globals.h"
#include "Input.h"

// GLOBAL: TH16 0x4dfbd0
static char s_buffer[0x80];

// Decodes an obfuscated dialogue string into a static buffer.
// FUNCTION: TH16 0x42bbe0
const char *LTCG_FASTCALL decode_msg_string(const char *src)
{
    u8 key = 0x77;
    u8 step = 7;
    char *dst = s_buffer;
    char c;
    do
    {
        c = *src ^ key;
        key += step;
        step += 0x10;
        *dst = c;
        src++;
        dst++;
    } while (c != '\0');
    return s_buffer;
}

// Every caller asks about g_InputState, which the original names directly.
// FUNCTION: TH16 0x42c830
HARNESS_CALLED i32 InputState::get_hold_time(int button)
{
    return (g_InputState.input & (1 << button)) ? g_InputState.hold_time[button] : 0;
}

// TODO: the original divides by 10 with idiv (as if the 10 were a folded
// parameter); ours strength-reduces it.
// The point item value in hundreds, rounded down to a multiple of ten.
// FUNCTION: TH16 0x42c860
i32 get_piv_rounded()
{
    i32 base = g_Globals.piv / 100;
    return base - base % 10;
}

// Debug logging, compiled out of the release build.
// FUNCTION: TH16 0x42c9f0
void debug_log(const char *fmt, ...)
{
}
