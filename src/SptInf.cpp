#include "Ecl.h"
#include "AnmManager.h"
#include "Enemy.h"
#include "EnemyManager.h"
#include "FileSystem.h"
#include "GameErrorContext.h"

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

// FUNCTION: TH16 0x41a860
int EclResourceInf::load_file(const char *filename)
{
    strcpy(g_ecl_path, "");
    strcat(g_ecl_path, filename);
    if (SptResourceInf::load_ecl_data(file_read_all(g_ecl_path, NULL, 0)) < 0)
    {
        return -1;
    }
    return 0;
}

// FUNCTION: TH16 0x41b040
int EclResourceInf::load_includes(void *data)
{
    u32 *header = (u32 *)data;
    if (header[0] != 'MINA')
    {
        return 0;
    }
    char *name = (char *)&header[2];
    for (u32 i = 0; i < header[1]; i++)
    {
        g_EnemyManager->anim_statement_anms[2 + i] = AnmManager::preload_anm(i + 10, name);
        if (g_EnemyManager->anim_statement_anms[2 + i] == NULL)
        {
            g_GameErrorContext.log("\x83" "f\x81[\x83^\x82\xaa\x89\xf3\x82\xea\x82\xc4\x82\xa2\x82\xdc\x82\xb7\r\n");
            return -1;
        }
        name += strlen(name) + 1;
    }
    int misalign = (name - (char *)data) % 4;
    if (misalign != 0)
    {
        name += 4 - misalign;
    }
    header = (u32 *)name;
    if (header[0] == 'ILCE')
    {
        name = (char *)&header[2];
        for (u32 i = 0; i < header[1]; i++)
        {
            load_file(name);
            name += strlen(name) + 1;
        }
    }
    return 0;
}

// GLOBAL: TH16 0x4dfc50
char g_ecl_path[0x104];
