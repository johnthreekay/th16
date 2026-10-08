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

// One value on the ECL stack or in an argument.
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

// The instructions every ECL VM runs (EclRunContext::ecl_run). Names follow
// ExpHP's th-re-data labels (in parentheses where ours differ); 300 and up
// belong to the VM subclass (EnemyData::ecl_run_over_300).
enum EclOpcode
{
    ECL_OP_NOP = 0,
    // Ends the context.
    ECL_OP_DELETE = 1,
    ECL_OP_RETURN = 10,
    ECL_OP_CALL = 11,
    ECL_OP_JMP = 12,
    // Pop; jump if zero / nonzero.
    ECL_OP_JMP_EQ = 13,
    ECL_OP_JMP_NEQ = 14,
    ECL_OP_CALL_ASYNC = 15,
    ECL_OP_CALL_ASYNC_ID = 16,
    ECL_OP_KILL_ASYNC = 17,
    // Unnamed by ExpHP: set and clear bit 0 of an async's flags_11e4, and
    // set its unk_101c (neither is read anywhere).
    ECL_OP_ASYNC_FLAG_SET = 18,
    ECL_OP_ASYNC_FLAG_CLEAR = 19,
    ECL_OP_ASYNC_SET_101C = 20,
    ECL_OP_KILL_ALL_ASYNC = 21,
    // Debug instructions, empty in release builds.
    ECL_OP_DEBUG_22 = 22,
    ECL_OP_WAIT = 23,
    ECL_OP_WAITF = 24,
    ECL_OP_DEBUG_30 = 30,
    ECL_OP_DEBUG_31 = 31,
    ECL_OP_STACK_ALLOC = 40,
    // Closes the frame stackAlloc opened (unnamed by ExpHP).
    ECL_OP_STACK_DEALLOC = 41,
    ECL_OP_PUSH = 42,
    ECL_OP_SET = 43,
    ECL_OP_PUSHF = 44,
    ECL_OP_SETF = 45,
    ECL_OP_ADD = 50,
    ECL_OP_ADDF = 51,
    ECL_OP_SUB = 52,
    ECL_OP_SUBF = 53,
    ECL_OP_MUL = 54,
    ECL_OP_MULF = 55,
    ECL_OP_DIV = 56,
    ECL_OP_DIVF = 57,
    ECL_OP_MOD = 58,
    ECL_OP_EQ = 59,
    ECL_OP_EQF = 60,
    ECL_OP_NEQ = 61,
    ECL_OP_NEQF = 62,
    ECL_OP_LESS = 63,
    ECL_OP_LESSF = 64,
    ECL_OP_LEQ = 65,
    ECL_OP_LEQF = 66,
    ECL_OP_GREATER = 67,
    ECL_OP_GREATERF = 68,
    ECL_OP_GEQ = 69,
    ECL_OP_GEQF = 70,
    ECL_OP_NOT = 71,
    ECL_OP_NOTF = 72,
    ECL_OP_OR = 73,
    ECL_OP_AND = 74,
    ECL_OP_XOR = 75,
    ECL_OP_BIT_OR = 76,
    ECL_OP_BIT_AND = 77,
    // Decrements a variable, pushing its old value.
    ECL_OP_DEC = 78,
    ECL_OP_STACK_SIN = 79,
    ECL_OP_STACK_COS = 80,
    ECL_OP_MATH_CIRCLE_POS = 81,
    // Wraps an angle variable into [-pi, pi].
    ECL_OP_VALID_RAD = 82,
    ECL_OP_NEG = 83,
    ECL_OP_NEGF = 84,
    ECL_OP_NORM_SQ = 85,
    ECL_OP_NORM = 86,
    ECL_OP_MATH_ANGLE = 87,
    ECL_OP_STACK_SQRT = 88,
    // Signed difference of two angles (ExpHP: linearFunc).
    ECL_OP_ANGLE_DIFF = 89,
    ECL_OP_POINT_ROTATE = 90,
    ECL_OP_FLOAT_TIME = 91,
    // floatTime with bezier control values (ExpHP: math92).
    ECL_OP_FLOAT_TIME_BEZIER = 92,
    // Random point in a ring (ExpHP: math93).
    ECL_OP_RAND_RING_POS = 93,
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

    // The local at the given byte offset from the frame base. Callers that
    // go through it compute the frame address before adding the offset.
    i32 *local_ptr(i32 offset)
    {
        return (i32 *)((u8 *)data + base_offset + offset);
    }

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
    // pop_float reads the entry as an int and reinterprets it for the float
    // return (EclRunContext::pop_float_arg does the same).
    __forceinline f32 pop_float()
    {
        stack_offset -= 4;
        i32 item = *(i32 *)((u8 *)data + stack_offset);
        stack_offset -= 4;
        char type = *((char *)data + stack_offset);
        if (type != 'f' && type == 'i')
        {
            return (f32)item;
        }
        return *(f32 *)&item;
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
    // Set on another async by ECL 20; never read.
    i32 unk_101c;
    u8 difficulty_mask;
    u8 unk_1021[3];
    InterpFloat float_i[8];
    EclLocation float_i_locs[8];
    // Bit 0 set and cleared on another async by ECL 18 and 19; never read.
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

// One entry of the subroutine table, sorted by name.
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
