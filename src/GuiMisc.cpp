// Small helpers placed among the Gui code.
#include "Globals.h"
#include "Input.h"

// decode_msg_string's result.
// GLOBAL: TH16 0x4dfbd0
static char s_buffer[0x80];

// Decodes an obfuscated dialogue or ending string into a static buffer:
// each byte is XORed with a key that starts at 0x77 and grows by a step
// that starts at 7 and itself grows by 0x10 per byte.
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

// The point item value in hundreds, rounded down to a multiple of ten.
// Written on the global: through a local, % 10 becomes a multiply by the
// reciprocal, where the original divides (mov reg, 10; idiv).
// FUNCTION: TH16 0x42c860
i32 get_piv_rounded()
{
    return g_Globals.piv / 100 - g_Globals.piv / 100 % 10;
}

// Debug logging, compiled out of the release build.
// FUNCTION: TH16 0x42c9f0
void debug_log(const char *fmt, ...)
{
}
