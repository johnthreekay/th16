#include "GameThread.h"
#include "Globals.h"
#include "Input.h"
#include "ReplayManager.h"
#include "Scorefile.h"

// GLOBAL: TH16 0x4a6f08
ReplayManager *g_ReplayManager;

// GLOBAL: TH16 0x4a6de0
char g_current_replay_filename[0x100];

// FUNCTION: TH16 0x447e20
HARNESS_CALLED ReplayManager *ReplayManager::create(i32 mode)
{
    ReplayManager *replay = new ReplayManager();
    if (replay->initialize(mode, g_current_replay_filename) != 0)
    {
        delete replay;
        return NULL;
    }
    return replay;
}

// FUNCTION: TH16 0x447ef0
HARNESS_CALLED ReplayManager *ReplayManager::create_from_file(const char *filename)
{
    ReplayManager *replay = new ReplayManager();
    replay->mode = REPLAY_LOADED;
    if (replay->read_replay_file(filename) != 0)
    {
        delete replay;
        return NULL;
    }
    return replay;
}

// FUNCTION: TH16 0x447fb0
HARNESS_CALLED void ReplayManager::destroy(ReplayManager *replay)
{
    delete replay;
}

// FUNCTION: TH16 0x448e40
int __fastcall ReplayManager::on_tick_22(void *arg)
{
    ReplayManager *replay = (ReplayManager *)arg;

    // Fast-forward: run the frame list again for 7 of every 8 frames.
    if (g_GameThread != NULL && !(g_GameThread->flags & 4) && replay->mode == REPLAY_PLAYBACK &&
        (g_hardware_input & 0x201) && replay->current_tick_num_in_stage % 8 != 0)
    {
        return UPDATE_FUNC_RESTART_FROM_FIRST;
    }
    return UPDATE_FUNC_CONTINUE;
}

// FUNCTION: TH16 0x448e90
int __fastcall ReplayManager::on_draw_47(void *arg)
{
    if (g_GameThread != NULL && (g_GameThread->flags & 4))
    {
        return UPDATE_FUNC_CONTINUE;
    }
    return on_draw_47_body(arg);
}

// FUNCTION: TH16 0x449190
void Globals::set_game_mode(u32 mode)
{
    if (game_mode != 2)
    {
        spell_id = -1;
    }
    game_mode = mode;
}

// FUNCTION: TH16 0x4491c0
ZunList<RpyChunk> *ReplayManager::new_chunk(i32 stage)
{
    RpyChunk *chunk = new RpyChunk();
    chunk->next_fps_count_write_pos = chunk->fps_counts;
    chunk->next_input_write_pos = chunk->input;
    ZunList<RpyChunk> *node = &chunk->node;
    node->entry = chunk;
    node->next = NULL;
    node->prev = NULL;
    node->unk_c = NULL;
    recorded_chunks_by_stage[stage].append(node);
    num_chunks_recorded++;
    return node;
}

// FUNCTION: TH16 0x449270
void ReplayManager::free_chunks(i32 stage)
{
    ZunList<RpyChunk> *node = recorded_chunks_by_stage[stage].next;
    while (node != NULL)
    {
        ZunList<RpyChunk> *next = node->next;
        RpyChunk *chunk = node->entry;
        if (chunk != NULL)
        {
            if (chunk->node.next != NULL)
            {
                chunk->node.next->prev = chunk->node.prev;
            }
            if (chunk->node.prev != NULL)
            {
                chunk->node.prev->next = chunk->node.next;
            }
            chunk->node.next = NULL;
            chunk->node.prev = NULL;
            delete chunk;
        }
        node = next;
    }
}

// FUNCTION: TH16 0x4492e0
ReplayStageData::ReplayStageData()
{
    memset(this, 0, sizeof(ReplayStageData));
    input_current = input_begin;
    fps_counts_current = fps_counts_begin;
    node.entry = this;
    node.next = NULL;
    node.prev = NULL;
    node.unk_c = NULL;
}

// FUNCTION: TH16 0x449320
ReplayStageData::~ReplayStageData()
{
    if (node.next != NULL)
    {
        node.next->prev = node.prev;
    }
    if (node.prev != NULL)
    {
        node.prev->next = node.next;
    }
    node.next = NULL;
    node.prev = NULL;
}

// FUNCTION: TH16 0x449350
u32 ScorefileSection::compute_checksum(i32 size)
{
    u8 *data = (u8 *)this + 8;
    u32 sum = 0;
    for (i32 i = 0; i < size - 8; i++)
    {
        sum += data[i];
    }
    return sum;
}
