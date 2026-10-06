#pragma once

#include <windows.h>

#include "decomp.h"

struct UpdateFunc;

// Intrusive doubly linked list node; entry points back at the owner.
struct UpdateFuncList
{
    UpdateFunc *entry;
    UpdateFuncList *next;
    UpdateFuncList *prev;
    UpdateFuncList *unk_c;
};

// What a callback asks the registry to do next. Values 0-6 are the same as
// TH06's ChainCallbackResult.
enum UpdateFuncResult
{
    UPDATE_FUNC_REMOVE = 0,
    UPDATE_FUNC_CONTINUE = 1,
    UPDATE_FUNC_EXECUTE_AGAIN = 2,
    UPDATE_FUNC_BREAK = 3,
    UPDATE_FUNC_EXIT_SUCCESS = 4,
    UPDATE_FUNC_EXIT_ERROR = 5,
    UPDATE_FUNC_RESTART_FROM_FIRST = 6,
    // on_tick only: run on_cleanup and carry on.
    UPDATE_FUNC_CLEANUP = 7,
    // on_tick only: same effect as UPDATE_FUNC_EXIT_SUCCESS.
    UPDATE_FUNC_EXIT_SUCCESS_8 = 8,
};

// The argument arrives in ecx (ExpHP: "ecx_arg_to_function").
typedef int(__fastcall *UpdateFuncCallback)(void *arg);

enum UpdateFuncFlags
{
    // Allocated by create_func; unregister deletes it. (TH06: isHeapAllocated)
    UPDATE_FUNC_HEAP_ALLOCATED = 1 << 0,
    // Only active functions are run. Owners set or clear this right after
    // create_func and toggle it later.
    UPDATE_FUNC_ACTIVE = 1 << 1,
};

// One per-frame callback.
struct UpdateFunc
{
    int priority;
    unsigned int flags;
    UpdateFuncCallback function;
    UpdateFuncCallback on_registration;
    UpdateFuncCallback on_cleanup;
    UpdateFuncList list_node;
    void *arg;

    UpdateFunc()
    {
        flags &= ~UPDATE_FUNC_HEAP_ALLOCATED;
        function = NULL;
        on_registration = NULL;
        on_cleanup = NULL;
        priority = 0;
        list_node.entry = this;
        list_node.next = NULL;
        list_node.prev = NULL;
        list_node.unk_c = NULL;
    }

    ~UpdateFunc();
};

// Holds the sentinel heads of the on_tick and on_draw chains. ZUN's name
// for it in older games was FuncCtrlInf, later funcChainInf (per ExpHP).
//
// create_func, register_on_* and run_all_on_draw reach the registry through
// g_UpdateFuncRegistry instead of this. Link-time code generation then drops
// the unused this: callers push only the stack arguments, never set ecx, and
// the callee still pops them (ret N).
struct UpdateFuncRegistry
{
    UpdateFunc on_tick_head;
    UpdateFunc on_draw_head;
    // The node a run_all_* loop visits next; unregister moves it along when
    // that node is removed mid-run.
    UpdateFuncList *iter_next;
    int is_cleaning_up;

    UpdateFuncRegistry();
    ~UpdateFuncRegistry();

    // TH06 equivalent: Chain::CreateElem
    DECOMP_NOINLINE UpdateFunc *create_func(UpdateFuncCallback function);
    // TH06 equivalent: Chain::AddToCalcChain
    int register_on_tick(UpdateFunc *f, int priority);
    // TH06 equivalent: Chain::AddToDrawChain
    int register_on_draw(UpdateFunc *f, int priority);
    // TH06 equivalent: Chain::RunCalcChain
    int run_all_on_tick();
    // TH06 equivalent: Chain::RunDrawChain
    int run_all_on_draw();
    void unregister_all_in_list(UpdateFunc *head);
    // TH06 equivalent: Chain::Cut
    DECOMP_NOINLINE void unregister(UpdateFunc *f);
};

extern UpdateFuncRegistry *g_UpdateFuncRegistry;
