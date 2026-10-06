// Placeholders for functions other units own whose call sites only take
// their original shape under LTCG's custom conventions (float arguments in
// xmm registers, constant arguments folded into the callee). Unlike
// src/stub/, this file is compiled with /GL and nothing here is forced
// alive with /INCLUDE, so LTCG sees every caller and converts these the way
// it converted the real functions. Each body hands its arguments to an
// opaque stub so the calls cannot be optimized away.
#include "../AnmManager.h"
#include "../BulletManager.h"
#include "../Player.h"
#include "../SoundManager.h"
#include "../ZunTimer.h"

void placeholder_sink(int a, float b);

// STUB: TH16 0x45e1f0
HARNESS_CALLED void SoundManager::play_sound_at_position(i32 id, f32 x)
{
    placeholder_sink(id, x);
}

// STUB: TH16 0x46f1c0
HARNESS_CALLED void AnmManager::delete_vm(AnmId id)
{
    placeholder_sink(id.id, 0.0f);
}

// STUB: TH16 0x416d20
HARNESS_CALLED void BulletManager::cancel_radius_as_bomb(D3DXVECTOR3 *pos, f32 radius, i32 mode)
{
    placeholder_sink(mode, radius + pos->x);
}

