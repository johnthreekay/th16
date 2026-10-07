// Opaque stubs for wave 5 range C (0x440000-0x44c000): callees from other
// ranges. Compiled without /GL.
#include "../MainMenu.h"

int w5c_sink(void *object, int value)
{
    return value;
}

int w5c_sink_f(float a, float b)
{
    return (int)(a * b);
}

// STUB: TH16 0x44fe20
i32 TitleInf::do_difficulty_select()
{
    return 0;
}

// STUB: TH16 0x4502c0
i32 TitleInf::do_character_select()
{
    return 0;
}

// STUB: TH16 0x452330
i32 TitleInf::do_state_452330()
{
    return 0;
}

// STUB: TH16 0x4532f0
i32 TitleInf::do_state_4532f0()
{
    return 0;
}

// STUB: TH16 0x4546f0
i32 TitleInf::do_music_room()
{
    return 0;
}
