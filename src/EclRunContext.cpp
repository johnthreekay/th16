// ECL argument decoding: an argument flagged in variable_mask names a local
// (>= 0, a byte offset from the frame base), a stack entry (-1 to -100,
// counted back from the top) or a global variable of the VM (the rest).
#include <string.h>

#include "Ecl.h"

static_assert(sizeof(EclRunContext) == 0x11e8, "EclRunContext size");

// TODO: the original checks the stack range with two compares and loads the entry value before its type.
// FUNCTION: TH16 0x473c90
i32 EclRunContext::get_int_arg(int index)
{
    EclRawInstr *ins = current_instr();
    if (ins->variable_mask & (1 << index))
    {
        i32 value = ins->args[index].i;
        if (value >= 0)
        {
            return *(i32 *)((u8 *)stack.data + stack.base_offset + value);
        }
        if (value <= -1 && value >= -100)
        {
            EclStackEntry *entry = (EclStackEntry *)((u8 *)stack.data + stack.stack_offset) + value;
            EclStackItem item = entry->value;
            char type = entry->type;
            if (type == 'f')
            {
                return (i32)item.f;
            }
            else if (type == 'i')
            {
                return item.i;
            }
            return item.i;
        }
        return vm->get_int_global(value);
    }
    return ins->args[index].i;
}

// FUNCTION: TH16 0x473d40
HARNESS_CALLED f32 EclRunContext::get_float_arg(int index)
{
    EclRawInstr *ins = current_instr();
    if (ins->variable_mask & (1 << index))
    {
        f32 value = ins->args[index].f;
        if (value >= 0.0f)
        {
            return *(f32 *)((u8 *)stack.data + stack.base_offset + (i32)value);
        }
        if (value <= -1.0f && value >= -100.0f)
        {
            EclStackEntry *entry =
                (EclStackEntry *)((u8 *)stack.data + stack.stack_offset - (i32)(-value * sizeof(EclStackEntry)));
            EclStackItem item = entry->value;
            if (entry->type != 'f' && entry->type == 'i')
            {
                return (f32)item.i;
            }
            return item.f;
        }
        return vm->get_float_global((i32)value);
    }
    return ins->args[index].f;
}

// TODO: the original adds the frame base to the stack address first and loads the entry value before its type.
// FUNCTION: TH16 0x473e40
i32 EclRunContext::get_int_arg_given_value(int index, i32 value)
{
    EclRawInstr *ins = current_instr();
    if (ins->variable_mask & (1 << index))
    {
        if (value >= 0)
        {
            return *(i32 *)((u8 *)stack.data + stack.base_offset + value);
        }
        if (value <= -1 && value >= -100)
        {
            EclStackEntry *entry = (EclStackEntry *)((u8 *)stack.data + stack.stack_offset) + value;
            EclStackItem item = entry->value;
            char type = entry->type;
            if (type == 'f')
            {
                return (i32)item.f;
            }
            else if (type == 'i')
            {
                return item.i;
            }
            return item.i;
        }
        return vm->get_int_global(value);
    }
    return value;
}

// FUNCTION: TH16 0x473ef0
HARNESS_CALLED f32 EclRunContext::get_float_arg_given_value(int index, f32 value)
{
    EclRawInstr *ins = current_instr();
    if (ins->variable_mask & (1 << index))
    {
        if (value >= 0.0f)
        {
            return *(f32 *)((u8 *)stack.data + stack.base_offset + (i32)value);
        }
        if (value <= -1.0f && value >= -100.0f)
        {
            EclStackEntry *entry =
                (EclStackEntry *)((u8 *)stack.data + stack.stack_offset - (i32)(-value * sizeof(EclStackEntry)));
            EclStackItem item = entry->value;
            if (entry->type != 'f' && entry->type == 'i')
            {
                return (f32)item.i;
            }
            return item.f;
        }
        return vm->get_float_global((i32)value);
    }
    return value;
}

// TODO: register allocation and the stack range check differ (two compares in the original).
// FUNCTION: TH16 0x473fe0
HARNESS_CALLED i32 EclRunContext::pop_int_arg(int index)
{
    EclRawInstr *ins = current_instr();
    if (ins->variable_mask & (1 << index))
    {
        i32 value = ins->args[index].i;
        if (value >= 0)
        {
            return *(i32 *)((u8 *)stack.data + stack.base_offset + value);
        }
        if (value <= -1 && value >= -100)
        {
            stack.stack_offset -= 4;
            EclStackItem item = *(EclStackItem *)((u8 *)stack.data + stack.stack_offset);
            stack.stack_offset -= 4;
            char type = *((char *)stack.data + stack.stack_offset);
            if (type == 'f')
            {
                return (i32)item.f;
            }
            else if (type == 'i')
            {
                return item.i;
            }
            return item.i;
        }
        return vm->get_int_global(value);
    }
    return ins->args[index].i;
}

// TODO: register allocation differs around the popped entry.
// FUNCTION: TH16 0x474090
HARNESS_CALLED f32 EclRunContext::pop_float_arg(int index)
{
    EclRawInstr *ins = current_instr();
    if (ins->variable_mask & (1 << index))
    {
        f32 value = ins->args[index].f;
        if (value >= 0.0f)
        {
            return *(f32 *)((u8 *)stack.data + stack.base_offset + (i32)value);
        }
        if (value <= -1.0f && value >= -100.0f)
        {
            stack.stack_offset -= 4;
            EclStackItem item = *(EclStackItem *)((u8 *)stack.data + stack.stack_offset);
            stack.stack_offset -= 4;
            char type = *((char *)stack.data + stack.stack_offset);
            if (type != 'f' && type == 'i')
            {
                return (f32)item.i;
            }
            return item.f;
        }
        return vm->get_float_global((i32)value);
    }
    return ins->args[index].f;
}

// TODO: the original adds the frame base to the stack address first; registers differ around the pops.
// FUNCTION: TH16 0x474180
i32 EclRunContext::pop_int_arg_given_value(int index, i32 value)
{
    EclRawInstr *ins = current_instr();
    if (ins->variable_mask & (1 << index))
    {
        if (value >= 0)
        {
            return *(i32 *)((u8 *)stack.data + stack.base_offset + value);
        }
        if (value <= -1 && value >= -100)
        {
            stack.stack_offset -= 4;
            EclStackItem item = *(EclStackItem *)((u8 *)stack.data + stack.stack_offset);
            stack.stack_offset -= 4;
            char type = *((char *)stack.data + stack.stack_offset);
            if (type == 'f')
            {
                return (i32)item.f;
            }
            else if (type == 'i')
            {
                return item.i;
            }
            return item.i;
        }
        return vm->get_int_global(value);
    }
    return value;
}

// TODO: eax and ecx swapped around the popped entry.
// FUNCTION: TH16 0x474240
HARNESS_CALLED f32 EclRunContext::pop_float_arg_given_value(int index, f32 value)
{
    EclRawInstr *ins = current_instr();
    if (ins->variable_mask & (1 << index))
    {
        if (value >= 0.0f)
        {
            return *(f32 *)((u8 *)stack.data + stack.base_offset + (i32)value);
        }
        if (value <= -1.0f && value >= -100.0f)
        {
            stack.stack_offset -= 4;
            EclStackItem item = *(EclStackItem *)((u8 *)stack.data + stack.stack_offset);
            stack.stack_offset -= 4;
            char type = *((char *)stack.data + stack.stack_offset);
            if (type != 'f' && type == 'i')
            {
                return (f32)item.i;
            }
            return item.f;
        }
        return vm->get_float_global((i32)value);
    }
    return value;
}

// TODO: the original adds the stack base address last (reccmp: effective match).
// FUNCTION: TH16 0x474330
HARNESS_CALLED i32 *EclRunContext::get_int_arg_ptr(int index)
{
    EclRawInstr *ins = current_instr();
    if (ins->variable_mask & (1 << index))
    {
        i32 value = ins->args[index].i;
        if (value >= 0)
        {
            return (i32 *)((u8 *)stack.data + (stack.base_offset + value));
        }
        return vm->get_int_global_ptr(value);
    }
    return NULL;
}

// TODO: register allocation differs.
// FUNCTION: TH16 0x4743a0
f32 *EclRunContext::get_float_arg_ptr(int index)
{
    EclRawInstr *ins = current_instr();
    if (ins->variable_mask & (1 << index))
    {
        f32 value = ins->args[index].f;
        if (value >= 0.0f)
        {
            return (f32 *)((u8 *)stack.data + stack.base_offset + (i32)value);
        }
        return vm->get_float_global_ptr((i32)value);
    }
    return NULL;
}

// FUNCTION: TH16 0x474740
int SptResourceInf::find_sub_by_name(const char *name) throw()
{
    // The subroutine table is sorted by name.
    i32 lo = 0;
    i32 hi = subroutine_count - 1;
    while (lo <= hi)
    {
        i32 mid = lo + (hi - lo) / 2;
        i32 cmp = strcmp(name, subroutines[mid].name);
        if (cmp == 0)
        {
            return mid;
        }
        if (cmp < 0)
        {
            hi = mid - 1;
        }
        else
        {
            lo = mid + 1;
        }
    }
    return -1;
}

// FUNCTION: TH16 0x4747d0
EclRawInstr *EclRunContext::get_subroutine_ptr()
{
    return current_instr();
}

// TODO: the original stores stack_offset before loading base_offset.
// FUNCTION: TH16 0x474810
i32 EclStack::enter(i32 size)
{
    i32 old_offset = stack_offset;
    if (size + stack_offset >= 0x1000)
    {
        return -1;
    }
    stack_offset += size;
    *(i32 *)((u8 *)data + stack_offset) = base_offset;
    stack_offset += 4;
    base_offset = old_offset;
    return 0;
}

// FUNCTION: TH16 0x474860
i32 EclStack::ecl_return()
{
    stack_offset -= 4;
    i32 old_base = base_offset;
    base_offset = *(i32 *)((u8 *)data + stack_offset);
    stack_offset = old_base;
    return 0;
}

// FUNCTION: TH16 0x4744e0
EclRunContextList *SptInf::lookup_async(i32 id)
{
    for (EclRunContextList *node = &async_list_head; node != NULL; node = node->next)
    {
        if (node->entry->async_id == id)
        {
            return node;
        }
    }
    return NULL;
}

// FUNCTION: TH16 0x474890
int SptInf::load_sub_by_name(const char *name)
{
    context.current_context->cur_location.subroutine_index = file_manager->find_sub_by_name(name);
    context.current_context->cur_location.offset_from_first_instruction = 0;
    context.current_context->time = 0.0f;
    return 0;
}
