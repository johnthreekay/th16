#pragma once

#include <stdlib.h>
#include <string.h>

#include "Interp.h"
#include "decomp.h"
#include "types.h"

class SptInf;

// Where an ECL script is executing (ExpHP: zEclLocation).
struct EclLocation
{
    i32 subroutine_index;
    i32 offset_from_first_instruction;
};

union EclStackItem
{
    i32 i;
    f32 f;
};

// What ECL code pushes: a type tag ('i' or 'f') and the value. Locals
// below the frame base are plain values without a tag.
struct EclStackEntry
{
    char type;
    u8 unk_1[3];
    EclStackItem value;
};

// One instruction (ExpHP: zEclRawInstructionHeader), arguments following.
struct EclRawInstr
{
    i32 time;
    u16 opcode;
    u16 total_size;
    // Bit n set: argument n names a variable rather than a constant.
    u16 variable_mask;
    u8 rank_mask;
    u8 param_count;
    u8 num_stack_refs;
    u8 unk_d[3];
    EclStackItem args[1];
};

// One extra argument of the call instructions: the type of the value as
// written ('f'/'g' float, else int), the type the callee wants ('f' float,
// else int), and the value.
struct EclCallArg
{
    char type_from;
    char type_to;
    u8 unk_2[2];
    EclStackItem value;
};

// ExpHP: zEclStack.
struct EclStack
{
    EclStackItem data[0x400];
    i32 stack_offset;
    i32 base_offset;

    EclStack()
    {
        stack_offset = 0;
        base_offset = 0;
    }

    // 0x474810. Opens a call frame with size bytes of locals; -1 when the
    // stack is full.
    HARNESS_CALLED i32 enter(i32 size);
    // 0x474860. Closes the frame enter opened.
    HARNESS_CALLED i32 ecl_return();

    // Typed pushes and pops of the expression stack, inlined into
    // EclRunContext::ecl_run.
    __forceinline i32 pop_int()
    {
        stack_offset -= 4;
        EclStackItem item = *(EclStackItem *)((u8 *)data + stack_offset);
        stack_offset -= 4;
        char type = *((char *)data + stack_offset);
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
    __forceinline f32 pop_float()
    {
        stack_offset -= 4;
        EclStackItem item = *(EclStackItem *)((u8 *)data + stack_offset);
        stack_offset -= 4;
        char type = *((char *)data + stack_offset);
        if (type != 'f' && type == 'i')
        {
            return (f32)item.i;
        }
        return item.f;
    }
    __forceinline void push_int(i32 value)
    {
        *((char *)data + stack_offset) = 'i';
        stack_offset += 4;
        *(i32 *)((char *)data + stack_offset) = value;
        stack_offset += 4;
    }
    __forceinline void push_float(f32 value)
    {
        EclStackItem item;
        item.f = value;
        *((char *)data + stack_offset) = 'f';
        stack_offset += 4;
        *(i32 *)((u8 *)data + stack_offset) = item.i;
        stack_offset += 4;
    }
    // Pops the saved value of a call frame.
    __forceinline i32 pop_raw()
    {
        stack_offset -= 4;
        return *(i32 *)((u8 *)data + stack_offset);
    }
};

// One thread of ECL execution (ExpHP: zEclRunContext).
struct EclRunContext
{
    f32 time;
    EclLocation cur_location;
    EclStack stack;
    i32 async_id;
    SptInf *vm;
    i32 unk_101c;
    u8 difficulty_mask;
    u8 unk_1021[3];
    InterpFloat float_i[8];
    EclLocation float_i_locs[8];
    // Bit 0 set by instructions 18/19.
    u32 flags_11e4;

    EclRunContext();
    i32 get_int_arg(int index);
    i32 *get_int_arg_ptr(int index);
    f32 get_float_arg(int index);
    f32 *get_float_arg_ptr(int index);

    // The same for a value already read from the instruction.
    i32 get_int_arg_given_value(int index, i32 value);
    HARNESS_CALLED f32 get_float_arg_given_value(int index, f32 value);
    // Like the getters above, but a stack reference pops the entry.
    HARNESS_CALLED i32 pop_int_arg(int index);
    HARNESS_CALLED f32 pop_float_arg(int index);
    i32 pop_int_arg_given_value(int index, i32 value);
    HARNESS_CALLED f32 pop_float_arg_given_value(int index, f32 value);

    // 0x471db0. Starts dest at the subroutine named by the current
    // instruction's string argument, passing it the arguments after
    // argument index start; -1 when there is no such subroutine.
    HARNESS_CALLED i32 call_sub(EclRunContext *dest, i32 start, i32 unused);
    // 0x472030. Runs this context's instructions for one frame at the
    // given speed; nonzero once it has finished.
    HARNESS_CALLED i32 ecl_run(f32 speed);

    // The instruction at cur_location, NULL when there is none.
    EclRawInstr *current_instr();
    // 0x4747d0. current_instr, out of line (ExpHP: get_subroutine_ptr).
    HARNESS_CALLED EclRawInstr *get_subroutine_ptr();
    // get_float_arg_ptr for the instruction at loc.
    f32 *float_arg_ptr_at(EclLocation *loc, int index);
};

// Intrusive list of run contexts (ExpHP: zEclRunContextList).
struct EclRunContextList
{
    EclRunContext *entry;
    EclRunContextList *next;
    EclRunContextList *prev;
    EclRunContextList *unk_c;
};

// ExpHP: zEclRunContextHolder.
struct EclRunContextHolder
{
    EclRunContext *current_context;
    EclRunContext primary_context;
};

// The header of an .ecl file: the include list (ANIM/ECLI) follows it, then
// the subroutine offsets and the subroutine names.
struct EclRawFile
{
    u32 magic;
    u16 version;
    u16 include_length;
    u32 include_offset;
    u32 unk_c;
    u16 sub_count;
    u16 unk_12;
    u32 unk_14[4];
};

struct EclSubroutinePtrs
{
    const char *name;
    void *bytecode;
};

// Bit of zero-initialized state at the end of SptResourceInf.
struct SptResourceTail
{
    i32 unk_0;
    i32 unk_4;

    SptResourceTail()
    {
        unk_0 = 0;
        unk_4 = 0;
    }
};

// VTABLE: TH16 0x4921e0
// Loaded ECL files and their subroutine table (ExpHP: zEclFileManager).
// The name is ZUN's, from RTTI.
class SptResourceInf
{
  public:
    i32 file_count;
    i32 subroutine_count;
    void *file_data_pointers[0x20];
    EclSubroutinePtrs *subroutines;
    u8 unk_90[0x1090 - 0x90];
    SptResourceTail tail;

    SptResourceInf()
    {
        memset(this, 0, sizeof(*this));
    }
    ~SptResourceInf()
    {
        if (subroutines != NULL)
        {
            free(subroutines);
            subroutines = NULL;
        }
    }

    int find_sub_by_name(const char *name) throw();

    virtual int load_ecl_data(void *data);
    virtual int load_includes(void *data);
};

// VTABLE: TH16 0x492198
// The enemy ECL file manager owned by EnemyManager (ExpHP:
// VTABLE_ENEMY_MANAGER). The name is ZUN's, from RTTI.
class EclResourceInf : public SptResourceInf
{
  public:
    virtual int load_includes(void *data);
    virtual int load_file(const char *filename);
};

// Scratch buffer for building ECL file paths.
extern char g_ecl_path[0x104];

// VTABLE: TH16 0x4921c4
// ECL virtual machine base class (ExpHP: zEclVm). The name is ZUN's, from
// RTTI.
class SptInf
{
  public:
    void *unk_4;
    void *unk_8;
    EclRunContextHolder context;
    SptResourceInf *file_manager;
    // entry points at context.primary_context; asyncs follow.
    EclRunContextList async_list_head;

    DECOMP_NOINLINE SptInf();
    void free_all_async();
    void reset_run_context();
    // 0x474430. Starts a new async context running the subroutine named by
    // the current instruction.
    i32 create_async(i32 id, i32 start);
    // 0x473bc0 (ExpHP: Enemy::ecl_run). Runs the main context and every
    // async for one frame, freeing asyncs that have finished; -1 once the
    // main context has finished.
    HARNESS_CALLED i32 run_ecl(f32 speed);
    // 0x4744e0. The async with the given id, NULL when there is none.
    EclRunContextList *lookup_async(i32 id);
    // 0x474890 (ExpHP: Enemy::load_sub_by_name). Restarts the current
    // context at the start of the named subroutine.
    int load_sub_by_name(const char *name);

    virtual int run_over_300();
    virtual int get_int_global(int var);
    virtual int *get_int_global_ptr(int var);
    virtual f32 get_float_global(int var);
    virtual f32 *get_float_global_ptr(int var);
    virtual ~SptInf();
};

inline EclRawInstr *EclRunContext::current_instr()
{
    if (cur_location.offset_from_first_instruction == -1 || cur_location.subroutine_index == -1)
    {
        return NULL;
    }
    // Instructions start after the subroutine's 0x10 byte header.
    return (EclRawInstr *)((u8 *)vm->file_manager->subroutines[cur_location.subroutine_index].bytecode + 0x10 +
                           cur_location.offset_from_first_instruction);
}
