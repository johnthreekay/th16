#include <stddef.h>

#include "GameThread.h"
#include "Globals.h"
#include "Input.h"
#include "Player.h"
#include "ReplayManager.h"
#include "Rng.h"
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

static_assert(offsetof(RpyInfo, timestamp) == 0xc, "RpyInfo::timestamp");
static_assert(offsetof(RpyInfo, stage) == 0x90, "RpyInfo::stage");
static_assert(sizeof(RpyInfo) == 0xa0, "RpyInfo");

// FUNCTION: TH16 0x4483b0
HARNESS_CALLED i32 ReplayManager::set_end_stage(i32 extra_stage)
{
    _time64(&info->timestamp);
    info->stage = extra_stage != 0 ? extra_stage + 7 : g_Globals.stage_num;
    return 0;
}

static_assert(sizeof(RpyGamestate) == 0x294, "RpyGamestate");
static_assert(offsetof(RpyGamestate, globals) == 0x14, "RpyGamestate::globals");

// FUNCTION: TH16 0x449030
HARNESS_CALLED void ReplayManager::start_stage()
{
    if (mode == REPLAY_RECORDING)
    {
        stage_gamestate_snapshots[g_Globals.stage_num] = new RpyGamestate;
        RpyGamestate *gamestate = (RpyGamestate *)stage_gamestate_snapshots[g_Globals.stage_num];
        gamestate->rng_state = g_replay_safe_rng.seed;
        g_replay_unsafe_rng.seed = gamestate->rng_state;
        g_replay_safe_rng.generation_count = 0;
        gamestate->stage = g_Globals.stage_num;
        gamestate->flag_290 = g_Supervisor.unk_700;
    }
    else if (mode == REPLAY_PLAYBACK)
    {
        ReplayStageData *stage = &stages[g_Globals.stage_num];
        RpyGamestate *gamestate = stage->gamestate_at_stage_begin;
        stage->input_current = stage->input_begin;
        stage->fps_counts_current = stage->fps_counts_begin;
        stage->frame_current = -1;
        g_replay_safe_rng.seed = gamestate->rng_state;
        g_replay_unsafe_rng.seed = gamestate->rng_state;
        g_replay_safe_rng.generation_count = 0;
        memcpy(&g_Globals, gamestate->globals, sizeof(gamestate->globals));
    }
}

// TODO: the original realigns its frame (and esp, -8; most likely for a
// callee such as repopulate_options) and reads the stage before clearing
// current_tick_num_in_stage in the recording branch.
// FUNCTION: TH16 0x448eb0
HARNESS_CALLED void ReplayManager::begin_stage()
{
    if (on_tick_func != NULL)
    {
        on_tick_func->flags |= UPDATE_FUNC_ACTIVE;
    }
    if (on_tick_22_func != NULL)
    {
        on_tick_22_func->flags |= UPDATE_FUNC_ACTIVE;
    }
    if (on_draw_func != NULL)
    {
        on_draw_func->flags |= UPDATE_FUNC_ACTIVE;
    }
    clear_input_state();
    if (mode == REPLAY_RECORDING)
    {
        RpyGamestate *gamestate = (RpyGamestate *)stage_gamestate_snapshots[g_Globals.stage_num];
        free_chunks(g_Globals.stage_num);
        currently_recording_chunk = new_chunk(g_Globals.stage_num);
        if (g_Supervisor.unk_700 == 0)
        {
            memcpy(gamestate->globals, &g_Globals, sizeof(gamestate->globals));
        }
        gamestate->player_pos_subpixel[0] = g_Player->inner.pos_subpixel.x;
        gamestate->player_pos_subpixel[1] = g_Player->inner.pos_subpixel.y;
        current_tick_num_in_stage = 0;
        stage_num = g_Globals.stage_num;
        ((RpyGamestate *)stage_gamestate_snapshots[stage_num])->player_is_focused = g_Player->inner.is_focused;
    }
    else if (mode == REPLAY_PLAYBACK)
    {
        RpyGamestate *gamestate = stages[g_Globals.stage_num].gamestate_at_stage_begin;
        stage_num = g_Globals.stage_num;
        g_Player->set_position_subpixel((Int2 *)gamestate->player_pos_subpixel);
        Player *player = g_Player;
        player->inner.is_focused = gamestate->player_is_focused;
        if (g_Globals.stage_num == 3)
        {
            unk_10 = 1;
        }
        ReplayStageData *stage = &stages[g_Globals.stage_num];
        stage->frame_current = 0;
        stage->input_current = stage->input_begin;
        stage->fps_counts_current = stage->fps_counts_begin;
        player->inner.repopulate_options();
    }
    current_tick_num_in_stage = 0;
}

// GLOBAL: TH16 0x4a5144
i32 g_input_repeat_time[0x20];

// FUNCTION: TH16 0x449120
void clear_input_state()
{
    memset(g_input_repeat_time, 0, sizeof(g_input_repeat_time));
    memset(g_InputState.hold_time, 0, 0x80);
    g_InputState.hold_time[0x20] = 0;
    g_InputState.input = 0;
    g_InputState.input_prev = 0;
    g_InputState.unk_8c = 0;
    g_InputState.input_rising = 0;
    g_InputState.input_falling = 0;
    g_InputState.unk_9c = 0;
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
