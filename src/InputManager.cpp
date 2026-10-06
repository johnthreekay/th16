#include "InputManager.h"

// FUNCTION: TH16 0x405580
void InputManager::detect_holds_and_repeats()
{
    u32 mask = 1;
    u32 bits = cur;
    repeat = 0;
    held_long = 0;
    for (i32 i = 0; i < 32; i++, bits >>= 1, mask <<= 1)
    {
        if (bits & 1)
        {
            hold_frames[i]++;
            hold_frames_total[i]++;
            if (hold_frames[i] >= 8)
            {
                held_long |= mask;
            }
            if (hold_frames[i] >= 26)
            {
                repeat |= mask;
                hold_frames[i] -= 8;
            }
        }
        else
        {
            hold_frames[i] = 0;
            hold_frames_total[i] = 0;
        }
    }
    rising_edge = (cur ^ prev) & cur;
    falling_edge = (cur ^ prev) & ~cur;
}
