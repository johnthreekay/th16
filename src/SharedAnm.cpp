// Loading and unloading of the ANMs every game mode shares. The names are
// ours; the loading thread calls the first and teardown the second.
#include <stddef.h>

#include "AnmManager.h"
#include "EffectManager.h"
#include "GameErrorContext.h"

// FUNCTION: TH16 0x43ad20
i32 load_shared_anms()
{
    if (AnmManager::preload_anm(5, "front.anm") == NULL)
    {
        // "The data is corrupted."
        g_GameErrorContext.log("\x83" "f\x81[\x83^\x82\xaa\x89\xf3\x82\xea\x82\xc4\x82\xa2\x82\xdc\x82\xb7\r\n");
        return -1;
    }
    if (preload_bullet_and_effect_anm() != 0)
    {
        return -1;
    }
    if (EffectManager::create() == NULL)
    {
        return -1;
    }
    return 0;
}

// FUNCTION: TH16 0x43ad60
i32 unload_shared_anms()
{
    AnmManager *anm = g_AnmManager;
    if (anm->loaded_anms[5] != NULL)
    {
        anm->loaded_anms[5]->release();
        delete anm->loaded_anms[5];
        anm->loaded_anms[5] = NULL;
    }
    if (g_EffectManager != NULL)
    {
        delete g_EffectManager;
    }
    g_EffectManager = NULL;
    return 0;
}
