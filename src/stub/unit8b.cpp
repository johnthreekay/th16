// Placeholders for functions unit 8b (0x469000-0x474c00) calls but other
// units own. Compiled without /GL, so calls into them stay opaque.
#include "../AnmManager.h"
#include "../Camera.h"

// The real code is in AnmManagerVms.cpp; see the TODO there.
// STUB: TH16 0x46efa0
AnmVm *AnmManager::get_vm_with_id(AnmId id)
{
    return NULL;
}

// STUB: TH16 0x43c780
void __stdcall camera_update_43c780(Camera *camera)
{
}

// GLOBAL: TH16 0x491b58
AnmVmFunc g_anm_on_destroy_funcs[4];

// Opaque work for the /GL placeholders in src/placeholder/unit8b.cpp.
int unit8b_placeholder_sink(void *object, int value)
{
    return value;
}

// GLOBAL: TH16 0x491b50
AnmVmCopyFunc g_anm_on_copy_funcs[2];

