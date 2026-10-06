#include "GameThread.h"
#include "Globals.h"
#include "Input.h"
#include "ReplayManager.h"
#include "Scorefile.h"
#include "FpsCounter.h"
#include "Supervisor.h"

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
    if (g_GameThread != NULL && !g_GameThread->flags.paused && replay->mode == REPLAY_PLAYBACK &&
        (g_hardware_input & 0x201) && replay->current_tick_num_in_stage % 8 != 0)
    {
        return UPDATE_FUNC_RESTART_FROM_FIRST;
    }
    return UPDATE_FUNC_CONTINUE;
}

// FUNCTION: TH16 0x448e90
int __fastcall ReplayManager::on_draw_47(void *arg)
{
    if (g_GameThread != NULL && g_GameThread->flags.paused)
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

// Placeholder for the pause menu's end-of-replay handling (0x43f240).
void replay_ended_43f240();

// TODO: the original realigns its frame to 8 bytes and keeps the recorded
// input in a local (edi, spilled to the frame); ours rereads the global.
// FUNCTION: TH16 0x447fd0
int ReplayManager::on_tick_record()
{
    if (g_GameThread == NULL)
    {
        return 1;
    }
    g_InputState.input_prev = g_InputState.input;
    g_InputState.input = (u16)g_hardware_input;
    InputState::update();
    u32 input;
    if (g_Supervisor.config.flags_2c & 0x200)
    {
        input = g_InputState.input;
        if ((input & 1) && (u32)g_InputState.hold_time[0] >= 10)
        {
            input |= 8;
            g_InputState.input = input;
        }
    }
    else
    {
        input = g_InputState.input;
    }
    if (current_tick_num_in_stage < 0)
    {
        return 1;
    }
    if (current_tick_num_in_stage % 30 == 0)
    {
        f32 fps = g_FpsCounter->fps + 0.5f;
        u8 count = fps >= 256.0f ? 0xff : (u8)(i32)fps;
        RpyChunk *chunk = currently_recording_chunk->entry;
        *chunk->next_fps_count_write_pos = count;
        chunk->next_fps_count_write_pos++;
        input = g_InputState.input;
    }
    RpyChunk *chunk = currently_recording_chunk->entry;
    chunk->next_input_write_pos->input = input;
    chunk->next_input_write_pos->input_rising = g_InputState.input_rising;
    chunk->next_input_write_pos->input_falling = g_InputState.input_falling;
    chunk->next_input_write_pos++;
    if ((u8 *)chunk->next_input_write_pos - (u8 *)chunk >= 0x1518)
    {
        currently_recording_chunk = new_chunk(stage_num);
    }
    current_tick_num_in_stage++;
    return 1;
}

// FUNCTION: TH16 0x448130
int ReplayManager::on_tick_playback()
{
    if (g_GameThread == NULL)
    {
        return 1;
    }
    if (stages[stage_num].frame_current < 0)
    {
        g_InputState.input = 0;
        g_InputState.input_rising = 0;
        g_InputState.input_falling = 0;
        return 1;
    }
    if (stage_num >= 0)
    {
        g_InputState.input_prev = g_InputState.input;
        if (stages[stage_num].frame_current < stages[stage_num].gamestate_at_stage_begin->num_frames)
        {
            RpyFrameInput *frame = stages[stage_num].input_current;
            if (frame->input == 0xffff && frame->input_rising == 0xffff && frame->input_falling == 0xffff)
            {
                g_InputState.input = 0;
                g_InputState.input_rising = 0;
                g_InputState.input_falling = 0;
                replay_ended_43f240();
                stage_num = -1;
                return 1;
            }
            g_InputState.input = frame->input;
            g_InputState.input_rising = stages[stage_num].input_current->input_rising;
            g_InputState.input_falling = stages[stage_num].input_current->input_falling;
            InputState::update();
            current_fps = *stages[stage_num].fps_counts_current;
            stages[stage_num].input_current++;
            if (current_tick_num_in_stage % 30 == 0)
            {
                stages[stage_num].fps_counts_current++;
            }
        }
        else
        {
            g_InputState.input = 0;
            g_InputState.input_rising = 0;
            g_InputState.input_falling = 0;
        }
        stages[stage_num].frame_current++;
    }
    else
    {
        g_InputState.input = 0;
        g_InputState.input_rising = 0;
        g_InputState.input_falling = 0;
    }
    current_tick_num_in_stage++;
    return 1;
}

// FUNCTION: TH16 0x448e20
int __fastcall ReplayManager::on_tick_record_thunk(void *arg)
{
    return ((ReplayManager *)arg)->on_tick_record();
}

// FUNCTION: TH16 0x448e30
int __fastcall ReplayManager::on_tick_playback_thunk(void *arg)
{
    return ((ReplayManager *)arg)->on_tick_playback();
}
