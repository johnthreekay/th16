// ECL argument decoding: an argument flagged in variable_mask names a local
// (>= 0, a byte offset from the frame base), a stack entry (-1 to -100,
// counted back from the top) or a global variable of the VM (the rest).
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "Ecl.h"
#include "Rng.h"
#include "ZunMath.h"

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

// TODO: ours hoists (i32)value above the sign test (both paths convert it); the original
// converts in each path and adds the frame base before the stack address.
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
HARNESS_CALLED EclRawInstr *EclRunContext::get_subroutine_ptr()
{
    return current_instr();
}

// TODO: the original stores stack_offset before loading base_offset (not
// changed by a new_offset local, an inline push helper or a union store).
// FUNCTION: TH16 0x474810
HARNESS_CALLED i32 EclStack::enter(i32 size)
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
HARNESS_CALLED i32 EclStack::ecl_return()
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

// This file's copy of ZunMath.h's sincosmul.
// FUNCTION: TH16 0x474510
static void __fastcall ecl_sincosmul(Float3 *dst, f32 angle, f32 radius)
{
    __asm {
        mov eax, dst
        fld angle
        fsincos
        fmul radius
        fstp [eax]
        fmul radius
        fstp [eax+4]
    }
}

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

// The pointer argument getter for an instruction other than the current
// one (the interpolation's own instruction).
__forceinline f32 *EclRunContext::float_arg_ptr_at(EclLocation *loc, int index)
{
    SptInf *vm = this->vm;
    EclRawInstr *ins = (EclRawInstr *)((u8 *)vm->file_manager->subroutines[loc->subroutine_index].bytecode + 0x10 +
                                       loc->offset_from_first_instruction);
    if (ins->variable_mask & (1 << index))
    {
        f32 value = ins->args[index].f;
        if (value >= 0.0f)
        {
            return (f32 *)((u8 *)stack.data + (stack.base_offset + (i32)value));
        }
        return vm->get_float_global_ptr((i32)value);
    }
    return NULL;
}

// Instructions below 300; the VM handles the rest (run_over_300). Each
// instruction runs once its time has come; time counts up by speed per
// frame and jumps set it. Case bodies follow ZUN's order in the binary.
// TODO: the original computes each operator's result before the inlined
// push (ours sinks it into the push), so sub/mul and the comparisons do not
// tail-merge into add's and eq's push; stack slots and base/index register
// order in the stack addressing differ; the float_i loop keeps vm in esi.
// FUNCTION: TH16 0x472030
HARNESS_CALLED i32 EclRunContext::ecl_run(f32 speed)
{
    if (cur_location.offset_from_first_instruction == -1 || cur_location.subroutine_index == -1)
    {
        return -1;
    }
    EclRawInstr *ins = (EclRawInstr *)((u8 *)vm->file_manager->subroutines[cur_location.subroutine_index].bytecode +
                                       0x10 + cur_location.offset_from_first_instruction);
    while (time >= (f32)ins->time)
    {
        if (ins->rank_mask & difficulty_mask)
        {
            switch ((i16)ins->opcode)
            {
            case 0:
                break;
            // return
            case 10:
                stack.ecl_return();
                if (stack.stack_offset != 0)
                {
                    cur_location.subroutine_index = stack.pop_raw();
                    cur_location.offset_from_first_instruction = stack.pop_raw();
                    *(i32 *)&time = stack.pop_raw();
                    stack.stack_offset = stack.pop_raw();
                    ins = get_subroutine_ptr();
                    if (cur_location.offset_from_first_instruction >= 0)
                    {
                        break;
                    }
                }
            // delete
            case 1:
                cur_location.offset_from_first_instruction = -1;
                cur_location.subroutine_index = -1;
                return -1;
            // callAsync
            case 15:
                vm->create_async(-1, 0);
                goto next_instr;
            // killAllAsync
            case 21:
            {
                EclRunContextList *node = vm->async_list_head.next;
                while (node != NULL)
                {
                    EclRunContextList *next = node->next;
                    node->entry->cur_location.offset_from_first_instruction = -1;
                    node->entry->cur_location.subroutine_index = -1;
                    node = next;
                }
                goto next_instr;
            }
            // callAsyncId: the id follows the subroutine name.
            case 16:
                vm->create_async(
                    pop_int_arg_given_value(1, ins->args[(ins->args[0].i + sizeof(i32)) / sizeof(EclStackItem)].i),
                    1);
                goto next_instr;
            // killAsync
            case 17:
            {
                EclRunContextList *node = vm->lookup_async(get_int_arg(0));
                if (node != NULL)
                {
                    node->entry->cur_location.offset_from_first_instruction = -1;
                }
                break;
            }
            case 18:
            {
                EclRunContextList *node = vm->lookup_async(get_int_arg(0));
                if (node != NULL)
                {
                    node->entry->flags_11e4 |= 1;
                }
                break;
            }
            case 19:
            {
                EclRunContextList *node = vm->lookup_async(get_int_arg(0));
                if (node != NULL)
                {
                    node->entry->flags_11e4 &= ~1;
                }
                break;
            }
            case 20:
            {
                EclRunContextList *node = vm->lookup_async(get_int_arg(0));
                if (node != NULL)
                {
                    EclRunContext *ctx = node->entry;
                    ctx->unk_101c = get_int_arg(1);
                }
                break;
            }
            // call: the callee pops the stack arguments itself.
            case 11:
                ins->num_stack_refs = 0;
                if (call_sub(this, 0, 0) != 0)
                {
                    cur_location.offset_from_first_instruction = -1;
                    cur_location.subroutine_index = -1;
                    return -1;
                }
                ins = get_subroutine_ptr();
                continue;
            // jmpNeq (jump if nonzero)
            case 14:
                if (stack.pop_int() != 0)
                {
                    goto jump;
                }
                break;
            // jmpEq (jump if zero)
            case 13:
                if (stack.pop_int() != 0)
                {
                    break;
                }
            // jmp: offset, new time
            case 12:
            jump:
                time = ins->args[1].i;
                cur_location.offset_from_first_instruction += ins->args[0].i;
                ins = (EclRawInstr *)((u8 *)ins + ins->args[0].i);
                continue;
            // wait
            case 23:
                time -= get_int_arg(0);
                break;
            case 24:
                time -= get_float_arg(0);
                break;
            // stackAlloc
            case 40:
                stack.enter(get_int_arg(0));
                break;
            case 41:
                stack.ecl_return();
                break;
            // push
            case 42:
                stack.push_int(pop_int_arg(0));
                goto next_instr;
            case 44:
                stack.push_float(pop_float_arg(0));
                goto next_instr;
            // set: stores the popped value, then converts it in place.
            case 43:
            {
                i32 *dst = get_int_arg_ptr(0);
                *dst = stack.pop_raw();
                stack.stack_offset -= 4;
                if (*((char *)stack.data + stack.stack_offset) == 'f')
                {
                    *dst = (i32) * (f32 *)dst;
                }
                goto next_instr;
            }
            case 45:
            {
                f32 *dst = get_float_arg_ptr(0);
                *(i32 *)dst = stack.pop_raw();
                stack.stack_offset -= 4;
                char type = *((char *)stack.data + stack.stack_offset);
                if (type != 'f' && type == 'i')
                {
                    *dst = (f32) * (i32 *)dst;
                }
                goto next_instr;
            }
            // Integer arithmetic.
            case 50:
            {
                i32 b = stack.pop_int();
                i32 a = stack.pop_int();
                a += b;
                stack.push_int(a);
                ins->num_stack_refs = 0;
                goto next_instr;
            }
            case 52:
            {
                i32 b = stack.pop_int();
                i32 a = stack.pop_int();
                stack.push_int(a - b);
                ins->num_stack_refs = 0;
                goto next_instr;
            }
            case 54:
            {
                i32 b = stack.pop_int();
                i32 a = stack.pop_int();
                stack.push_int(a * b);
                ins->num_stack_refs = 0;
                goto next_instr;
            }
            case 56:
            {
                i32 b = stack.pop_int();
                i32 a = stack.pop_int();
                stack.push_int(a / b);
                ins->num_stack_refs = 0;
                goto next_instr;
            }
            case 58:
            {
                i32 b = stack.pop_int();
                i32 a = stack.pop_int();
                stack.push_int(a % b);
                ins->num_stack_refs = 0;
                goto next_instr;
            }
            // Float arithmetic.
            case 51:
            {
                f32 b = stack.pop_float();
                f32 a = stack.pop_float();
                stack.push_float(a + b);
                ins->num_stack_refs = 0;
                goto next_instr;
            }
            case 53:
            {
                f32 b = stack.pop_float();
                f32 a = stack.pop_float();
                stack.push_float(a - b);
                ins->num_stack_refs = 0;
                goto next_instr;
            }
            case 55:
            {
                f32 b = stack.pop_float();
                f32 a = stack.pop_float();
                stack.push_float(a * b);
                ins->num_stack_refs = 0;
                goto next_instr;
            }
            case 57:
            {
                f32 b = stack.pop_float();
                f32 a = stack.pop_float();
                stack.push_float(a / b);
                ins->num_stack_refs = 0;
                goto next_instr;
            }
            // Integer comparisons.
            case 59:
            {
                i32 b = stack.pop_int();
                i32 a = stack.pop_int();
                stack.push_int(a == b);
                goto next_instr;
            }
            case 61:
            {
                i32 b = stack.pop_int();
                i32 a = stack.pop_int();
                stack.push_int(a != b);
                goto next_instr;
            }
            case 63:
            {
                i32 b = stack.pop_int();
                i32 a = stack.pop_int();
                stack.push_int(a < b);
                goto next_instr;
            }
            case 65:
            {
                i32 b = stack.pop_int();
                i32 a = stack.pop_int();
                stack.push_int(a <= b);
                goto next_instr;
            }
            case 67:
            {
                i32 b = stack.pop_int();
                i32 a = stack.pop_int();
                stack.push_int(a > b);
                goto next_instr;
            }
            case 69:
            {
                i32 b = stack.pop_int();
                i32 a = stack.pop_int();
                stack.push_int(a >= b);
                goto next_instr;
            }
            case 71:
                stack.push_int(!stack.pop_int());
                goto next_instr;
            // Float comparisons.
            case 60:
            {
                f32 b = stack.pop_float();
                f32 a = stack.pop_float();
                stack.push_int(a == b);
                goto next_instr;
            }
            case 62:
            {
                f32 b = stack.pop_float();
                f32 a = stack.pop_float();
                stack.push_int(a != b);
                goto next_instr;
            }
            case 64:
            {
                f32 b = stack.pop_float();
                f32 a = stack.pop_float();
                stack.push_int(a < b);
                goto next_instr;
            }
            case 66:
            {
                f32 b = stack.pop_float();
                f32 a = stack.pop_float();
                stack.push_int(a <= b);
                goto next_instr;
            }
            case 68:
            {
                f32 b = stack.pop_float();
                f32 a = stack.pop_float();
                stack.push_int(a > b);
                goto next_instr;
            }
            case 70:
            {
                f32 b = stack.pop_float();
                f32 a = stack.pop_float();
                stack.push_int(a >= b);
                goto next_instr;
            }
            case 72:
                stack.push_int(stack.pop_float() == 0.0f);
                goto next_instr;
            // Logic and bit operations.
            case 73:
            {
                i32 b = stack.pop_int();
                i32 a = stack.pop_int();
                stack.push_int(a || b);
                goto next_instr;
            }
            case 74:
            {
                i32 b = stack.pop_int();
                i32 a = stack.pop_int();
                stack.push_int(a && b);
                goto next_instr;
            }
            case 75:
            {
                i32 b = stack.pop_int();
                i32 a = stack.pop_int();
                stack.push_int(a ^ b);
                goto next_instr;
            }
            case 76:
            {
                i32 b = stack.pop_int();
                i32 a = stack.pop_int();
                stack.push_int(a | b);
                goto next_instr;
            }
            case 77:
            {
                i32 b = stack.pop_int();
                i32 a = stack.pop_int();
                stack.push_int(a & b);
                goto next_instr;
            }
            // Negation.
            case 83:
                stack.push_int(-stack.pop_int());
                goto next_instr;
            case 84:
                stack.push_float(-stack.pop_float());
                goto next_instr;
            // Decrement a variable, pushing its old value.
            case 78:
            {
                i32 value = get_int_arg(0);
                *get_int_arg_ptr(0) = value - 1;
                stack.push_int(value);
                goto next_instr;
            }
            case 79:
                stack.push_float((f32)sin(stack.pop_float()));
                goto next_instr;
            case 88:
                stack.push_float(sqrtf(stack.pop_float()));
                goto next_instr;
            case 80:
                stack.push_float((f32)cos(stack.pop_float()));
                goto next_instr;
            // x, y = radius at angle
            case 81:
            {
                f32 angle = normalize_angle(get_float_arg(2));
                Float3 pos;
                ecl_sincosmul(&pos, angle, get_float_arg(3));
                *get_float_arg_ptr(0) = pos.x;
                *get_float_arg_ptr(1) = pos.y;
                break;
            }
            // Squared length, length.
            case 85:
            {
                f32 x = get_float_arg(1);
                f32 y = get_float_arg(2);
                *get_float_arg_ptr(0) = x * x + y * y;
                break;
            }
            case 86:
            {
                f32 x = get_float_arg(1);
                f32 y = get_float_arg(2);
                *get_float_arg_ptr(0) = sqrtf(x * x + y * y);
                break;
            }
            case 82:
            {
                f32 angle = normalize_angle(get_float_arg(0));
                *get_float_arg_ptr(0) = angle;
                break;
            }
            // Angle from (x1, y1) to (x2, y2).
            case 87:
            {
                f32 x1 = get_float_arg(1);
                f32 y1 = get_float_arg(2);
                f32 x2 = get_float_arg(3);
                f32 y2 = get_float_arg(4);
                f32 angle = atan2f(y2 - y1, x2 - x1);
                *get_float_arg_ptr(0) = angle;
                break;
            }
            // Signed difference of two angles, wrapped once.
            case 89:
            {
                f32 a = get_float_arg(1);
                f32 b = get_float_arg(2);
                f32 diff;
                if (b - a > ZUN_PI)
                {
                    diff = b - (a + ZUN_2PI);
                }
                else if (a - b > ZUN_PI)
                {
                    diff = b - (a - ZUN_2PI);
                }
                else
                {
                    diff = b - a;
                }
                *get_float_arg_ptr(0) = diff;
                break;
            }
            // Rotate (x, y) by an angle.
            case 90:
            {
                f32 x = get_float_arg(2);
                f32 y = get_float_arg(3);
                f32 angle = normalize_angle(get_float_arg(4));
                f32 s = zun_sinf(angle);
                f32 c = zun_cosf(angle);
                f32 rx = x * c - y * s;
                f32 ry = y * c + x * s;
                *get_float_arg_ptr(0) = rx;
                *get_float_arg_ptr(1) = ry;
                break;
            }
            // floatTime: interpolate a variable from initial to goal.
            case 91:
            {
                i32 i = get_int_arg(0);
                float_i_locs[i] = cur_location;
                float_i[i].step();
                float_i[i].end_time = get_int_arg(2);
                float_i[i].method = get_int_arg(3);
                f32 initial = get_float_arg(4);
                f32 goal = get_float_arg(5);
                float_i[i].initial = initial;
                float_i[i].goal = goal;
                *get_float_arg_ptr(1) = initial;
                float_i[i].bezier_1 = 0.0f;
                float_i[i].bezier_2 = 0.0f;
                float_i[i].reset();
                break;
            }
            // The same with bezier control values.
            case 92:
            {
                i32 i = get_int_arg(0);
                float_i_locs[i] = cur_location;
                float_i[i].step();
                float_i[i].end_time = get_int_arg(2);
                float_i[i].method = get_int_arg(3);
                f32 initial = get_float_arg(4);
                f32 goal = get_float_arg(5);
                float_i[i].initial = initial;
                float_i[i].goal = goal;
                *get_float_arg_ptr(1) = initial;
                f32 bezier_1 = get_float_arg(6);
                f32 bezier_2 = get_float_arg(7);
                float_i[i].bezier_1 = bezier_1;
                float_i[i].bezier_2 = bezier_2;
                float_i[i].reset();
                break;
            }
            // Random point in a ring.
            case 93:
            {
                f32 r1 = get_float_arg(2);
                f32 r2 = get_float_arg(3);
                f32 angle = g_replay_safe_rng.randf_neg_to(ZUN_PI);
                f32 radius = g_replay_safe_rng.randf_neg_to(r2 - r1) + r1;
                Float3 pos;
                ecl_sincosmul(&pos, angle, radius);
                *get_float_arg_ptr(0) = pos.x;
                *get_float_arg_ptr(1) = pos.y;
                break;
            }
            // Debug instructions, empty in release builds.
            case 22:
            case 30:
            case 31:
                break;
            default:
            {
                i32 result = vm->run_over_300();
                if (result == -1)
                {
                    goto step_float_i;
                }
                else if (result == 0)
                {
                    break;
                }
                else if (result == 1)
                {
                    continue;
                }
                break;
            }
            }
            if (ins->num_stack_refs)
            {
                stack.stack_offset -= ins->num_stack_refs;
            }
        }
    next_instr:
        cur_location.offset_from_first_instruction += ins->total_size;
        ins = (EclRawInstr *)((u8 *)ins + ins->total_size);
    }
    time += speed;
step_float_i:
    for (i32 i = 0; i < 8; i++)
    {
        if (float_i[i].end_time != 0)
        {
            f32 *dst = float_arg_ptr_at(&float_i_locs[i], 1);
            *dst = float_i[i].step();
        }
    }
    return 0;
}
