#include <windows.h>

#include "CriticalSections.h"
#include "UpdateFunc.h"

// GLOBAL: TH16 0x4a6d94
UpdateFuncRegistry *g_UpdateFuncRegistry;

// FUNCTION: TH16 0x401280
UpdateFuncRegistry::UpdateFuncRegistry()
{
    is_cleaning_up = 0;
}

// FUNCTION: TH16 0x401300
int UpdateFuncRegistry::register_on_tick(UpdateFunc *f, int priority)
{
    UpdateFuncRegistry *registry = g_UpdateFuncRegistry;
    int result = 0;

    if (f->on_registration != NULL)
    {
        result = f->on_registration(f->arg);
        f->on_registration = NULL;
    }

    ENTER_CS(CS_UPDATE_FUNC_REGISTRY);
    f->priority = priority;

    UpdateFuncList *node = &registry->on_tick_head.list_node;
    while (node->next != NULL && node->next->entry->priority < priority)
    {
        node = node->next;
    }

    UpdateFuncList *new_node = &f->list_node;
    if (node->next != NULL)
    {
        new_node->next = node->next;
        node->next->prev = new_node;
    }
    if (node->unk_c != NULL)
    {
        node->unk_c = new_node;
    }
    node->next = new_node;
    new_node->prev = node;
    LEAVE_CS(CS_UPDATE_FUNC_REGISTRY);

    return result;
}

// FUNCTION: TH16 0x401730
UpdateFunc *UpdateFuncRegistry::create_func(UpdateFuncCallback function)
{
    UpdateFunc *f = new UpdateFunc();
    f->run = 1;
    f->function = function;
    f->on_registration = NULL;
    f->on_cleanup = NULL;
    return f;
}
