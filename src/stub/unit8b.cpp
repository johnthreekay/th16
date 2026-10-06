// Placeholders for functions unit 8b (0x469000-0x474c00) calls but other
// units own. Compiled without /GL, so calls into them stay opaque.
#include "../AnmManager.h"
#include "../Camera.h"

// STUB: TH16 0x406380
AnmId AnmLoaded::create_effect(i32 script, i32 layer, AnmVm **out_vm)
{
    return AnmId();
}

// STUB: TH16 0x43c780
void __stdcall camera_update_43c780(Camera *camera)
{
}
