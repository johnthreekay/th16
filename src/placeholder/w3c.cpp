// Stand-ins for callees from other ranges whose shape LTCG must see for
// wave 3, range C (0x42b480-0x43dc30). Compiled with /GL, not forced alive.
#include "../SoundManager.h"

int w3c_placeholder_sink(void *a, const void *b);

// LTCG has to see that the only caller passes a constant to fold it.
// STUB: TH16 0x45db10
DECOMP_NOINLINE i32 SoundManager::open_bgm_dat(const char *name)
{
    return w3c_placeholder_sink(this, name);
}
