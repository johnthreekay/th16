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

// STUB: TH16 0x46d1c0
AnmLoaded *__stdcall AnmManager::load_next_entry(AnmLoaded *anm)
{
    return anm;
}

// Opaque work for the /GL placeholders in src/placeholder/unit8b.cpp.
int unit8b_placeholder_sink(void *object, int value)
{
    return value;
}

// GLOBAL: TH16 0x491b50
AnmVmCopyFunc g_anm_on_copy_funcs[2];

// STUB: TH16 0x425d80
template <> __declspec(noinline) void ZunList<void>::append(ZunList<void> *node)
{
    insert_after(node);
}

// MSVC treats the specialization as inline (the template defines append in
// the class) and only emits it where it is used.
void unit8b_use_append(ZunList<void> *list, ZunList<void> *node)
{
    list->append(node);
}
