// Placeholders for functions the second pass over 0x411860-0x42cb00 calls
// but that are not decompiled yet. Compiled without /GL, so they stay
// opaque calls with standard conventions.
#include "../AnmManager.h"
#include "../AnmVm.h"
#include "../Gui.h"
#include "../StageData.h"

// STUB: TH16 0x46e890
AnmId __stdcall AnmManager::insert_in_world_list_front(AnmVm *vm)
{
    return vm->id;
}

// STUB: TH16 0x406380
AnmId AnmLoaded::create_effect(i32 script, i32 layer, AnmVm **out)
{
    AnmId id;
    return id;
}

// STUB: TH16 0x46f130
void __stdcall AnmManager::interrupt_tree_and_run(AnmId id, i32 interrupt)
{
}

// GLOBAL: TH16 0x4a6f18
StageData *g_stage_data;


// STUB: TH16 0x428e70
i32 Gui::on_draw_2_body()
{
    return 1;
}

// STUB: TH16 0x429b20
GuiMsgVm::GuiMsgVm(void *script)
{
}

// STUB: TH16 0x427cf0
i32 Gui::on_tick_body()
{
    return 1;
}
