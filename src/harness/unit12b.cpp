// Stand-in callers for unit 12b (0x401000-0x411860, second pass) functions
// whose shape depends on code not decompiled yet.
#include "../Input.h"

// The menus (0x45108b, 0x452801) read the keyboard into global buffers.
i32 harness_get_keyboard_state(u8 *keys)
{
    return get_keyboard_state(keys);
}
