// Opaque placeholders for wave 3 range D (0x43dc30-0x44f710) and the
// callees it needs from other ranges. Compiled without /GL.
#include "../MainMenu.h"
#include "../Player.h"

// STUB: TH16 0x44af80
i32 TitleInf::on_tick()
{
    return 1;
}

// STUB: TH16 0x451d50
i32 TitleInf::on_draw__replay()
{
    return 1;
}

// STUB: TH16 0x440fb0
i32 Player::initialize()
{
    return 0;
}

// STUB: TH16 0x444e10
i32 PlayerBullet::create(i32 shooter_ref, i32 time, PlayerInner *inner)
{
    return 0;
}
