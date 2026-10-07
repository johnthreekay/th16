// ECL argument decoding: an argument flagged in variable_mask names a local
// (>= 0, a byte offset from the frame base), a stack entry (-1 to -100,
// counted back from the top) or a global variable of the VM (the rest).
#include <stdlib.h>
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

// Adds an .ecl file: merges its subroutines into the table, kept sorted by
// name, and loads its includes. Returns the file's index, -1 if it is not
// an ECL file.
// TODO: this and name swap registers (edi/ebx), and the original keeps the NULL check before free.
// FUNCTION: TH16 0x474530
int SptResourceInf::load_ecl_data(void *data)
{
    file_data_pointers[file_count] = data;
    EclRawFile *file = (EclRawFile *)file_data_pointers[file_count];
    if (file->magic != 'TPCS')
    {
        file_data_pointers[file_count] = NULL;
        return -1;
    }
    if (file->version != 1)
    {
        file_data_pointers[file_count] = NULL;
        return -1;
    }
    u32 *offsets = (u32 *)((u8 *)file + sizeof(EclRawFile) + file->include_length);
    char *name = (char *)(offsets + file->sub_count);
    subroutine_count += file->sub_count;
    EclSubroutinePtrs *old = subroutines;
    subroutines = (EclSubroutinePtrs *)malloc(subroutine_count * sizeof(EclSubroutinePtrs));
    if (old == NULL)
    {
        for (i32 i = 0; i < subroutine_count; i++)
        {
            subroutines[i].bytecode = (u8 *)file_data_pointers[file_count] + *offsets;
            subroutines[i].name = name;
            name += strlen(name) + 1;
            offsets++;
        }
    }
    else
    {
        i32 count = subroutine_count - ((EclRawFile *)file_data_pointers[file_count])->sub_count;
        memcpy(subroutines, old, count * sizeof(EclSubroutinePtrs));
        if (old != NULL)
        {
            free(old);
        }
        for (i32 i = 0; i < ((EclRawFile *)file_data_pointers[file_count])->sub_count; i++)
        {
            i32 j;
            for (j = 0; j < count; j++)
            {
                if (strcmp(name, subroutines[j].name) <= 0)
                {
                    break;
                }
            }
            for (i32 k = subroutine_count - 1; k > j; k--)
            {
                subroutines[k] = subroutines[k - 1];
            }
            subroutines[j].bytecode = (u8 *)file_data_pointers[file_count] + *offsets;
            subroutines[j].name = name;
            name += strlen(name) + 1;
            count++;
            offsets++;
        }
    }
    i32 index = file_count++;
    file = (EclRawFile *)file_data_pointers[index];
    if (file->include_length != 0)
    {
        load_includes((u8 *)file + sizeof(EclRawFile));
    }
    return index;
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

// TODO: the original aligns its frame to 8 bytes; otherwise only block placement differs.
// FUNCTION: TH16 0x473bc0
HARNESS_CALLED i32 SptInf::run_ecl(f32 speed)
{
    i32 is_primary = 1;
    EclRunContextList *node = &async_list_head;
    while (node != NULL)
    {
        EclRunContextList *next = node->next;
        context.current_context = node->entry;
        if (is_primary)
        {
            if (context.current_context->ecl_run(speed) != 0)
            {
                return -1;
            }
            is_primary = 0;
        }
        else if (context.current_context->ecl_run(speed) != 0)
        {
            delete context.current_context;
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
            delete node;
        }
        node = next;
    }
    context.current_context = &context.primary_context;
    return 0;
}

// FUNCTION: TH16 0x471da0
HARNESS_CALLED void ecl_log(const char *fmt, ...)
{
}

// TODO: the original walks the call arguments with a byte offset into args and spills differently.
// FUNCTION: TH16 0x471db0
HARNESS_CALLED i32 EclRunContext::call_sub(EclRunContext *dest, i32 start, i32 unused)
{
    EclRawInstr *ins = current_instr();
    i32 name_len = ins->args[0].i;
    i32 old_top = dest->stack.stack_offset;
    // The callee's locals start after the saved frame (four values).
    i32 locals = old_top + 0x10;
    if (old_top == 0)
    {
        locals = 0x14;
        *(i32 *)((u8 *)dest->stack.data + old_top) = old_top;
        dest->stack.stack_offset += 4;
    }
    EclCallArg *arg = (EclCallArg *)((u8 *)&ins->args[start + 1] + name_len);
    i32 *local = (i32 *)((u8 *)dest->stack.data + locals);
    for (i32 i = start + 1; i < ins->param_count; i++, arg++, local++)
    {
        if (arg->type_from == 'f' || arg->type_from == 'g')
        {
            f32 value = pop_float_arg_given_value(i, arg->value.f);
            if (arg->type_to == 'f')
            {
                *(f32 *)local = value;
            }
            else
            {
                *local = (i32)value;
            }
        }
        else
        {
            i32 value = pop_int_arg_given_value(i, arg->value.i);
            if (arg->type_to == 'f')
            {
                *(f32 *)local = (f32)value;
            }
            else
            {
                *local = value;
            }
        }
    }
    i32 top = dest->stack.stack_offset;
    if (old_top != 0)
    {
        dest->stack.stack_offset -= 4;
        i32 value = *(i32 *)((u8 *)dest->stack.data + dest->stack.stack_offset);
        dest->stack.stack_offset = old_top;
        *(i32 *)((u8 *)dest->stack.data + old_top - 4) = value;
    }
    else
    {
        dest->stack.stack_offset = 4;
    }
    *(i32 *)((u8 *)dest->stack.data + dest->stack.stack_offset) = top;
    dest->stack.stack_offset += 4;
    // Where to return to.
    if (old_top != 0)
    {
        *(f32 *)((u8 *)dest->stack.data + dest->stack.stack_offset) = time;
        dest->stack.stack_offset += 4;
        *(i32 *)((u8 *)dest->stack.data + dest->stack.stack_offset) = cur_location.offset_from_first_instruction;
        dest->stack.stack_offset += 4;
        *(i32 *)((u8 *)dest->stack.data + dest->stack.stack_offset) = cur_location.subroutine_index;
    }
    else
    {
        *(i32 *)((u8 *)dest->stack.data + dest->stack.stack_offset) = -1;
        dest->stack.stack_offset += 4;
        *(i32 *)((u8 *)dest->stack.data + dest->stack.stack_offset) = -1;
        dest->stack.stack_offset += 4;
        *(i32 *)((u8 *)dest->stack.data + dest->stack.stack_offset) = -1;
    }
    dest->stack.stack_offset += 4;
    const char *name = (const char *)&ins->args[1];
    EclRunContext *saved = vm->context.current_context;
    vm->context.current_context = dest;
    vm->load_sub_by_name(name);
    if (vm->context.current_context->current_instr() == NULL)
    {
        ecl_log(" error : \x96\xa2\x92\xe8\x8b`\x82\xcc\x8a\xd6\x90\x94\x96\xbc %s\n", name);
        cur_location.offset_from_first_instruction = -1;
        cur_location.subroutine_index = -1;
        return -1;
    }
    vm->context.current_context = saved;
    return 0;
}

// FUNCTION: TH16 0x474430
i32 SptInf::create_async(i32 id, i32 start)
{
    EclRunContext *ctx = new EclRunContext;
    EclRunContextList *node = new EclRunContextList;
    ctx->async_id = id;
    ctx->vm = this;
    ctx->time = 0.0f;
    ctx->cur_location.offset_from_first_instruction = -1;
    ctx->cur_location.subroutine_index = -1;
    ctx->difficulty_mask = context.current_context->difficulty_mask;
    node->entry = ctx;
    node->next = NULL;
    node->prev = NULL;
    node->unk_c = NULL;
    EclRunContextList *head = &async_list_head;
    if (head->next != NULL)
    {
        node->next = head->next;
        head->next->prev = node;
    }
    if (head->unk_c != NULL)
    {
        head->unk_c = node;
    }
    head->next = node;
    node->prev = head;
    return context.current_context->call_sub(ctx, start, 0);
}
