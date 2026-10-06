// Placeholders for functions the second pass over 0x411860-0x42cb00 calls
// but that are not decompiled yet. Compiled without /GL, so they stay
// opaque calls with standard conventions.
#include "../AnmManager.h"
#include "../AnmVm.h"

// STUB: TH16 0x46e890
AnmId __stdcall AnmManager::insert_in_world_list_front(AnmVm *vm)
{
    return vm->id;
}
