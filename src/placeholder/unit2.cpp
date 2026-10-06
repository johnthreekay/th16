// Placeholders for functions other units own whose call sites only take
// their original shape under LTCG's custom conventions (float arguments in
// xmm registers, constant arguments folded into the callee). Unlike
// src/stub/, this file is compiled with /GL and nothing here is forced
// alive with /INCLUDE, so LTCG sees every caller and converts these the way
// it converted the real functions. Each body hands its arguments to an
// opaque stub so the calls cannot be optimized away.
#include "../AnmManager.h"
#include "../SoundManager.h"
#include "../Timer.h"

void placeholder_sink(int a, float b);

// GLOBAL: TH16 0x4d9e10
SoundManager g_SoundManager;

// STUB: TH16 0x406190
HARNESS_CALLED void Timer::operator++(int)
{
    tick();
}

// STUB: TH16 0x45e150
HARNESS_CALLED void SoundManager::play_sound_centered(i32 id, i32 unused)
{
    placeholder_sink(id, 0.0f);
}

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
