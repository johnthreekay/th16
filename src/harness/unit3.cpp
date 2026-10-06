// Stand-in callers for unit 3 (0x411860-0x41a3f0) and for shared functions
// unit 3 needs called the way the original calls them.
#include "../UpdateFunc.h"

// create_func and register_on_* are HARNESS_CALLED so that LTCG can drop
// their unused this, as in the original. With only constant priorities in
// view it would also fold the priority into the callee; the original has
// callers with many different ones.
void harness_unit3_register_funcs(UpdateFuncCallback callback, int priority)
{
    UpdateFunc *f = g_UpdateFuncRegistry->create_func(callback);
    g_UpdateFuncRegistry->register_on_tick(f, priority);
    g_UpdateFuncRegistry->register_on_draw(f, priority);
}
