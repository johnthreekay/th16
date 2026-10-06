// Opaque placeholders for wave 3 range D (0x43dc30-0x44f710) and the
// callees it needs from other ranges. Compiled without /GL.
#include "../MainMenu.h"
#include "../Player.h"

// STUB: TH16 0x44af80
i32 TitleInf::on_tick()
{
    return 1;
}

// STUB: TH16 0x4513c0
i32 TitleInf::on_draw__practice_stage_select()
{
    return 1;
}

// STUB: TH16 0x451d50
i32 TitleInf::on_draw__replay()
{
    return 1;
}

// STUB: TH16 0x453030
i32 TitleInf::on_draw__player_data()
{
    return 1;
}

// STUB: TH16 0x4538b0
i32 TitleInf::on_draw__4538b0()
{
    return 1;
}

// STUB: TH16 0x4541b0
i32 TitleInf::on_draw__4541b0()
{
    return 1;
}

// STUB: TH16 0x456d50
i32 TitleInf::on_draw__spell_practice_histories()
{
    return 1;
}

// STUB: TH16 0x440fb0
i32 Player::initialize()
{
    return 0;
}
