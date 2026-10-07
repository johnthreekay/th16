// Stand-ins for other units' functions that unit 12b's callers need LTCG
// to see (custom conventions, registers they leave alone). Compiled with
// /GL and not forced alive.
#include "../AnmManager.h"
#include "../BulletManager.h"
#include "../Player.h"

void placeholder_sink(int a, float b);

// STUB: TH16 0x444b20
HARNESS_CALLED i32 Player::create_rect_damage_source(D3DXVECTOR3 *pos, f32 width, f32 height, f32 angle, i32 unk_2, i32 damage)
{
    placeholder_sink(unk_2 + damage + (int)this, width + height + angle + pos->x);
    return unk_2;
}
