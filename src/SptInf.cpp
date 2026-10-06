#include "Ecl.h"

// FUNCTION: TH16 0x41a3f0
int SptResourceInf::load_includes(void *data)
{
    return 0;
}

// FUNCTION: TH16 0x41a400
HARNESS_CALLED EclRunContext::EclRunContext()
{
    for (int i = 0; i < 8; i++)
    {
        float_i[i].end_time = 0;
    }
}

// FUNCTION: TH16 0x41a4a0
int SptInf::run_over_300()
{
    return 0;
}

// FUNCTION: TH16 0x41a4b0
int SptInf::get_int_global(int var)
{
    return 0;
}

// FUNCTION: TH16 0x41a4c0
int *SptInf::get_int_global_ptr(int var)
{
    return NULL;
}

// FUNCTION: TH16 0x41a4d0
f32 SptInf::get_float_global(int var)
{
    return 0.0f;
}

// FUNCTION: TH16 0x41a4e0
f32 *SptInf::get_float_global_ptr(int var)
{
    return NULL;
}

// FUNCTION: TH16 0x41a4f0
void SptInf::free_all_async()
{
    EclRunContextList *node = async_list_head.next;
    while (node != NULL)
    {
        EclRunContextList *next = node->next;
        delete node->entry;
        delete node;
        node = next;
    }
}

// FUNCTION: TH16 0x41a530
SptInf::SptInf()
{
    for (int i = 0; i < 8; i++)
    {
        context.primary_context.float_i[i].end_time = 0;
    }
}

// FUNCTION: TH16 0x41a5a0
void SptInf::reset_run_context()
{
    context.primary_context.flags_11e4 &= ~1;
    context.primary_context.time = 0.0f;
    context.primary_context.cur_location.offset_from_first_instruction = -1;
    context.primary_context.cur_location.subroutine_index = -1;
    context.primary_context.async_id = -1;
    context.primary_context.vm = this;
    context.primary_context.unk_101c = 0;
    for (int i = 0; i < 8; i++)
    {
        context.primary_context.float_i[i].end_time = 0;
    }
    context.current_context = &context.primary_context;
    context.primary_context.stack.stack_offset = 0;
    context.primary_context.stack.base_offset = 0;
    async_list_head.entry = &context.primary_context;
    async_list_head.next = NULL;
    async_list_head.prev = NULL;
    async_list_head.unk_c = NULL;
}

// SYNTHETIC: TH16 0x41a670
// SptInf::`scalar deleting destructor'

SptInf::~SptInf()
{
    free_all_async();
}
