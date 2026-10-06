#include "GameErrorContext.h"
#include "Input.h"

// GLOBAL: TH16 0x4df508
JOYCAPSA g_joypad_caps;

// GLOBAL: TH16 0x4a50b0
u32 g_hardware_input;

// FUNCTION: TH16 0x401850
i32 get_joypad_capabilities()
{
    JOYINFOEX info;

    info.dwSize = sizeof(JOYINFOEX);
    info.dwFlags = JOY_RETURNALL;
    if (joyGetPosEx(JOYSTICKID1, &info) != JOYERR_NOERROR && joyGetPosEx(JOYSTICKID2, &info) != JOYERR_NOERROR)
    {
        // 使えるパッドが存在しないようです、残念
        g_GameErrorContext.log("\x8eg\x82\xa6\x82\xe9\x83p\x83" "b\x83h\x82\xaa\x91\xb6\x8d\xdd\x82\xb5\x82\xc8\x82\xa2"
                               "\x82\xe6\x82\xa4\x82\xc5\x82\xb7\x81" "A\x8e" "c\x94O\r\n");
        return 1;
    }
    joyGetDevCapsA(JOYSTICKID1, &g_joypad_caps, sizeof(g_joypad_caps));
    return 0;
}

// FUNCTION: TH16 0x4021a0
void clear_all_keydown_states()
{
    BYTE keys[256];

    GetKeyboardState(keys);
    for (i32 i = 0; i < 256; i++)
    {
        keys[i] &= 0x7f;
    }
    SetKeyboardState(keys);
}
