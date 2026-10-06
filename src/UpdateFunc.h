#pragma once

#include <windows.h>

struct UpdateFunc;

// Intrusive doubly linked list node; entry points back at the owner.
struct UpdateFuncList
{
    UpdateFunc *entry;
    UpdateFuncList *next;
    UpdateFuncList *prev;
    UpdateFuncList *unk_c;
};

// The argument arrives in ecx (ExpHP: "ecx_arg_to_function").
typedef int(__fastcall *UpdateFuncCallback)(void *arg);

// One per-frame callback.
struct UpdateFunc
{
    int priority;
    // volatile: the original keeps every store around changes to these bits,
    // which MSVC only does for volatile accesses (acquire/release under
    // /volatile:ms).
    volatile unsigned int run : 1;
    volatile unsigned int unk_4_1 : 1;
    volatile unsigned int unk_4_2 : 30;
    UpdateFuncCallback function;
    UpdateFuncCallback on_registration;
    UpdateFuncCallback on_cleanup;
    UpdateFuncList list_node;
    void *arg;

    UpdateFunc()
    {
        run = 0;
        function = NULL;
        on_registration = NULL;
        on_cleanup = NULL;
        priority = 0;
        list_node.entry = this;
        list_node.next = NULL;
        list_node.prev = NULL;
        list_node.unk_c = NULL;
    }
};

// Holds the sentinel heads of the on_tick and on_draw chains. ZUN's name
// for it in older games was FuncCtrlInf, later funcChainInf (per ExpHP).
//
// The methods are ordinary member functions that reach the registry through
// g_UpdateFuncRegistry instead of this. Link-time code generation then drops
// the unused this: callers push only the stack arguments, never set ecx, and
// the callee still pops them (ret N).
struct UpdateFuncRegistry
{
    UpdateFunc on_tick_head;
    UpdateFunc on_draw_head;
    UpdateFuncList *unk_50;
    int is_cleaning_up;

    UpdateFuncRegistry();

    // TH06 equivalent: Chain::CreateElem
    UpdateFunc *create_func(UpdateFuncCallback function);
    // TH06 equivalent: Chain::AddToCalcChain
    int register_on_tick(UpdateFunc *f, int priority);
};

extern UpdateFuncRegistry *g_UpdateFuncRegistry;
