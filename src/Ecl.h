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

    virtual int run_over_300();
    virtual int get_int_global(int var);
    virtual int *get_int_global_ptr(int var);
    virtual f32 get_float_global(int var);
    virtual f32 *get_float_global_ptr(int var);
    virtual ~SptInf();
};
