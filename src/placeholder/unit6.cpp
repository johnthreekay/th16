// Placeholders compiled with /GL, unlike src/stub/. Not ZUN's code: each
// forwards to an opaque stub, but because link-time code generation sees
// every caller, it shapes the calls the way it did in the original (for
// example folding an argument that is constant at every call site, so that
// callers push a junk register instead). Replace with the real function.
#include <string.h>

#include "../AnmManager.h"
#include "../SoundManager.h"

// STUB: TH16 0x45e150
HARNESS_CALLED void SoundManager::play_sound_centered(i32 id, i32 unused)
{
    play_sound_centered_stub(id, unused);
}

// Callers rely on LTCG knowing this cannot throw (no EH frame around
// new PopupManager), so an opaque stub will not do. The real constructor
// also clears the interpolators' timer flags before the memset.
// STUB: TH16 0x4093f0
DECOMP_NOINLINE AnmVm::AnmVm()
{
    memset(this, 0, sizeof(AnmVm));
    sprite_id = -1;
    instr_offset = -1;
}
