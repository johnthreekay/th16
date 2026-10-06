// Stand-in call sites for code that is not decompiled yet. Link-time code
// generation shapes a function by how it is called, so some functions only
// match with a caller like the original's.
#include "../UpdateFunc.h"

// Like the original's teardown (around 0x45a1e4), which deletes the registry.
void harness_delete_update_func_registry()
{
    delete g_UpdateFuncRegistry;
    g_UpdateFuncRegistry = NULL;
}
