#include <string.h>

#include "AsciiManager.h"
#include "PopupManager.h"

// GLOBAL: TH16 0x4a6f10
PopupManager *g_PopupManager;

// FUNCTION: TH16 0x449c70
PopupManager::PopupManager()
{
    memset(this, 0, sizeof(PopupManager));
    g_PopupManager = this;
    flags |= 2;
}

// FUNCTION: TH16 0x449cd0
int PopupManager::initialize()
{
    UpdateFunc *f;

    ascii_anm = g_AsciiManager->ascii_anm;

    f = g_UpdateFuncRegistry->create_func(on_tick);
    f->flags &= ~UPDATE_FUNC_ACTIVE;
    f->arg = this;
    g_UpdateFuncRegistry->register_on_tick(f, 0x14);
    on_tick_func = f;

    f = g_UpdateFuncRegistry->create_func(on_draw);
    f->flags &= ~UPDATE_FUNC_ACTIVE;
    f->arg = this;
    g_UpdateFuncRegistry->register_on_draw(f, 0x2f);
    on_draw_func = f;

    ascii_anm->init_vm_with_sprite(&vm, 0x103);
    vm.flags_hi = (vm.flags_hi & ~0x440000) | 0x380000;
    return 0;
}

// FUNCTION: TH16 0x449d60
PopupManager::~PopupManager()
{
    g_UpdateFuncRegistry->unregister_locked(on_tick_func);
    g_UpdateFuncRegistry->unregister_locked(on_draw_func);
    g_PopupManager = NULL;
}

// FUNCTION: TH16 0x449e50
PopupManager *PopupManager::create()
{
    PopupManager *manager = new PopupManager();
    if (manager->initialize() != 0)
    {
        delete manager;
        return NULL;
    }
    return manager;
}
