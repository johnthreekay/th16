// ECL argument decoding: an argument flagged in variable_mask names a local
// (>= 0, a byte offset from the frame base), a stack entry (-1 to -100,
// counted back from the top) or a global variable of the VM (the rest).
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
