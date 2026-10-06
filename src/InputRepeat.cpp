#include "types.h"

// Hardware button state (ExpHP: HARDWARE_INPUT and the fields after it).
extern u32 g_hardware_input_repeat;
extern u32 g_hardware_input_pressed;

// Menus move their cursor on a fresh press or on key repeat.
// TODO: our compiler turns the second test into a branchless setcc.
// FUNCTION: TH16 0x4186f0
i32 __stdcall input_pressed_or_repeating(u32 mask)
{
    if (g_hardware_input_pressed & mask)
    {
        return 1;
    }
    if (g_hardware_input_repeat & mask)
    {
        return 1;
    }
    return 0;
}
