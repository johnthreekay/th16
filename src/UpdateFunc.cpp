#include <windows.h>

#include "CriticalSections.h"
#include "Supervisor.h"
#include "UpdateFunc.h"

// GLOBAL: TH16 0x4a6d94
UpdateFuncRegistry *g_UpdateFuncRegistry;

UpdateFunc::~UpdateFunc()
{
    function = NULL;
    on_registration = NULL;
    on_cleanup = NULL;
}

// Both chains start empty (the heads' constructors clear them).
// FUNCTION: TH16 0x401280
UpdateFuncRegistry::UpdateFuncRegistry()
{
    is_cleaning_up = 0;
}

// Stops the supervisor's thread, gives every active on_tick function its
// on_cleanup (is_cleaning_up makes run_all_on_tick do that instead of
// running them) and unregisters everything. Only ever inlined into the
// scalar deleting destructor below.
UpdateFuncRegistry::~UpdateFuncRegistry()
{
    g_Supervisor.thread.join_if_running();
    is_cleaning_up = 1;
    run_all_on_tick();
    unregister_all_in_list(&on_tick_head);
    unregister_all_in_list(&on_draw_head);
}

// SYNTHETIC: TH16 0x45a3a0
// UpdateFuncRegistry::`scalar deleting destructor'

// Calls f's on_registration once, then inserts f into the on_tick chain
// after every function with a lower or equal priority (lower priorities run
// first). Returns what on_registration returned, or 0.
// FUNCTION: TH16 0x401300
HARNESS_CALLED int UpdateFuncRegistry::register_on_tick(UpdateFunc *f, int priority)
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

// register_on_tick for the on_draw chain.
// FUNCTION: TH16 0x4013b0
HARNESS_CALLED int UpdateFuncRegistry::register_on_draw(UpdateFunc *f, int priority)
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

    UpdateFuncList *node = &registry->on_draw_head.list_node;
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

// Runs every active on_tick function in priority order and acts on each
// result (UpdateFuncResult). Returns how many registered functions it
// visited, or 0, 1 or -1 when one ends the frame (EXIT_SUCCESS, BREAK, EXIT_ERROR). The lock is released
// around each call, and iter_next keeps the loop valid when a function
// unregisters others.
// FUNCTION: TH16 0x401460
int UpdateFuncRegistry::run_all_on_tick()
{
    UpdateFuncList *node;
    UpdateFunc *f;
    int count;
    int result;

    ENTER_CS(CS_UPDATE_FUNC_REGISTRY);
restart_from_first:
    count = 0;
    for (node = on_tick_head.list_node.next; node != NULL; node = iter_next)
    {
        iter_next = node->next;
        f = node->entry;
        if (f->function == NULL)
        {
            continue;
        }
        while (f->flags & UPDATE_FUNC_ACTIVE)
        {
            if (is_cleaning_up)
            {
                goto cleanup;
            }
            LEAVE_CS(CS_UPDATE_FUNC_REGISTRY);
            result = f->function(f->arg);
            ENTER_CS(CS_UPDATE_FUNC_REGISTRY);
            switch (result)
            {
            case UPDATE_FUNC_REMOVE:
                unregister(f);
                break;
            case UPDATE_FUNC_EXECUTE_AGAIN:
                continue;
            case UPDATE_FUNC_EXIT_SUCCESS:
            case UPDATE_FUNC_EXIT_SUCCESS_8:
                count = 0;
                goto done;
            case UPDATE_FUNC_BREAK:
                count = 1;
                goto done;
            case UPDATE_FUNC_EXIT_ERROR:
                count = -1;
                goto done;
            case UPDATE_FUNC_RESTART_FROM_FIRST:
                goto restart_from_first;
            case UPDATE_FUNC_CLEANUP:
            cleanup:
                if (f->on_cleanup != NULL)
                {
                    f->on_cleanup(f->arg);
                }
                break;
            }
            break;
        }
        count++;
    }
done:
    LEAVE_CS(CS_UPDATE_FUNC_REGISTRY);
    return count;
}

// run_all_on_tick for the on_draw chain, without the cleanup and restart
// results.
// FUNCTION: TH16 0x4015a0
HARNESS_CALLED int UpdateFuncRegistry::run_all_on_draw()
{
    UpdateFuncRegistry *registry = g_UpdateFuncRegistry;
    UpdateFuncList *node;
    UpdateFunc *f;
    int count;
    int result;

    ENTER_CS(CS_UPDATE_FUNC_REGISTRY);
    count = 0;
    for (node = registry->on_draw_head.list_node.next; node != NULL; node = registry->iter_next)
    {
        registry->iter_next = node->next;
        f = node->entry;
        if (f->function == NULL)
        {
            continue;
        }
        while (f->flags & UPDATE_FUNC_ACTIVE)
        {
            LEAVE_CS(CS_UPDATE_FUNC_REGISTRY);
            result = f->function(f->arg);
            ENTER_CS(CS_UPDATE_FUNC_REGISTRY);
            switch (result)
            {
            case UPDATE_FUNC_REMOVE:
                registry->unregister(f);
                break;
            case UPDATE_FUNC_EXECUTE_AGAIN:
                continue;
            case UPDATE_FUNC_EXIT_SUCCESS:
                count = 0;
                goto done;
            case UPDATE_FUNC_BREAK:
                count = 1;
                goto done;
            case UPDATE_FUNC_EXIT_ERROR:
                count = -1;
                goto done;
            }
            break;
        }
        count++;
    }
done:
    LEAVE_CS(CS_UPDATE_FUNC_REGISTRY);
    return count;
}

// A new heap-allocated UpdateFunc running function; owners set its arg and
// flags and then register it.
// FUNCTION: TH16 0x401730
HARNESS_CALLED UpdateFunc *UpdateFuncRegistry::create_func(UpdateFuncCallback function)
{
    // The original keeps every constructor store and then overwrites them
    // (see the constructor).
    UpdateFunc *f = new UpdateFunc;
    f->flags |= UPDATE_FUNC_HEAP_ALLOCATED;
    f->function = function;
    f->on_registration = NULL;
    f->on_cleanup = NULL;
    return f;
}

// Unregisters every function in the chain that starts at head.
// FUNCTION: TH16 0x4016b0
void UpdateFuncRegistry::unregister_all_in_list(UpdateFunc *head)
{
    UpdateFuncList *node = head->list_node.next;
    while (node != NULL)
    {
        UpdateFuncList *next = node->next;
        UpdateFunc *f = node->entry;
        if (f != NULL)
        {
            ENTER_CS(CS_UPDATE_FUNC_REGISTRY);
            unregister(f);
            LEAVE_CS(CS_UPDATE_FUNC_REGISTRY);
        }
        node = next;
    }
}

// Unlinks f from whichever chain holds it and clears its function; deletes
// it if create_func made it. The caller holds the lock.
// FUNCTION: TH16 0x4017a0
void UpdateFuncRegistry::unregister(UpdateFunc *f)
{
    UpdateFuncList *node;

    if (f == NULL)
    {
        return;
    }

    for (node = &on_tick_head.list_node; node != NULL; node = node->next)
    {
        if (node->entry == f)
        {
            goto found;
        }
    }
    for (node = &on_draw_head.list_node; node != NULL; node = node->next)
    {
        if (node->entry == f)
        {
            goto found;
        }
    }
    return;

found:
    if (iter_next == node)
    {
        iter_next = node->next;
    }
    if (node->prev == NULL)
    {
        return;
    }
    if (node->next != NULL)
    {
        node->next->prev = node->prev;
    }
    if (node->prev != NULL)
    {
        node->prev->next = node->next;
    }
    node->next = NULL;
    node->prev = NULL;
    f->function = NULL;
    if (f->flags & UPDATE_FUNC_HEAP_ALLOCATED)
    {
        delete f;
    }
}
