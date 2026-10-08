#include "Input.h"
#include "types.h"

// Menus move their cursor on a fresh press or on key repeat. Written as the
// test for neither: two ifs returning 1 turn the second test into a
// branchless setcc.
// FUNCTION: TH16 0x4186f0
i32 __stdcall input_pressed_or_repeating(u32 mask)
{
    if (!(g_hardware_input_pressed & mask) && !(g_hardware_input_repeat & mask))
    {
        return 0;
    }
    return 1;
}
