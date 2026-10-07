// Opaque placeholders for the second pass over units 5 and 6. Compiled
// without /GL, so calls into them stay opaque.
#include "../Input.h"
#include "../Player.h"

// STUB: TH16 0x442560
i32 Player::on_tick_body()
{
    return 1;
}

// STUB: TH16 0x446260
i32 __fastcall sht_on_tick_446260(PlayerBullet *bullet)
{
    return 0;
}

// STUB: TH16 0x4470f0
i32 __fastcall sht_on_tick_4470f0(PlayerBullet *bullet)
{
    return 0;
}

// STUB: TH16 0x446870
i32 __fastcall sht_on_hit_446870(PlayerBullet *bullet, i32 unk, i32 enemy, f32 x, f32 y)
{
    return 0;
}


// STUB: TH16 0x418650
void InputState::update()
{
}

