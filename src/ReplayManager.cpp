#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <direct.h>

#include "GameThread.h"
#include "Lzss.h"
#include "GameWindow.h"
#include "FileSystem.h"
#include "Crypt.h"
#include "Globals.h"
#include "Input.h"
#include "Player.h"
#include "ReplayManager.h"
#include "Rng.h"
#include "Scorefile.h"
#include "FpsCounter.h"
#include "Supervisor.h"
#include "AsciiManager.h"
#include "CriticalSections.h"
#include "MainMenu.h"

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
    if (g_GameThread != NULL && !g_GameThread->flags.loading && replay->mode == REPLAY_PLAYBACK &&
        (g_hardware_input & 0x201) && replay->current_tick_num_in_stage % 8 != 0)
    {
        return UPDATE_FUNC_RESTART_FROM_FIRST;
    }
    return UPDATE_FUNC_CONTINUE;
}

// FUNCTION: TH16 0x448e90
int __fastcall ReplayManager::on_draw_47(void *arg)
{
    if (g_GameThread != NULL && g_GameThread->flags.loading)
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
static_assert(offsetof(RpyInfo, score) == 0x14, "RpyInfo::score");
static_assert(offsetof(RpyInfo, slowdown) == 0x7c, "RpyInfo::slowdown");
static_assert(offsetof(RpyInfo, num_stages) == 0x80, "RpyInfo::num_stages");
static_assert(offsetof(RpyInfo, character) == 0x84, "RpyInfo::character");
static_assert(sizeof(RpyInfo) == 0xa0, "RpyInfo");

// FUNCTION: TH16 0x448c10
int ReplayManager::read_replay_file(const char *filename)
{
    char path[0x1000];
    u8 *data;
    i32 size;

    strcpy(this->filename, filename);
    if (!(g_Globals.flags_hi_45c & 1))
    {
        _chdir(g_GameWindow.save_dir);
        sprintf(path, "replay/%s", filename);
        if (!file_exists(path) || file_open(path) != 0)
        {
            goto fail;
        }
        rpy_file = file_read(sizeof(RpyHeader));
        if (((RpyFileHeader *)rpy_file)->magic != 0x72363174)
        {
            file_close();
        fail:
            _chdir(g_GameWindow.exe_dir);
            return -1;
        }
        if (((RpyFileHeader *)rpy_file)->version != 2)
        {
            file_close();
            goto fail;
        }
        data = file_read(((RpyFileHeader *)rpy_file)->compressed_size);
        file_close();
    }
    else
    {
        rpy_file = file_read_all(filename, &size, 0);
        data = (u8 *)rpy_file + sizeof(RpyHeader);
    }
    rpy_thing_204 = malloc(((RpyFileHeader *)rpy_file)->size);
    zun_decrypt(data, ((RpyFileHeader *)rpy_file)->compressed_size, 0x5c, 0xe1, 0x400,
                ((RpyFileHeader *)rpy_file)->compressed_size);
    zun_decrypt(data, ((RpyFileHeader *)rpy_file)->compressed_size, 0x7d, 0x3a, 0x100,
                ((RpyFileHeader *)rpy_file)->compressed_size);
    lzss_decompress(data, ((RpyFileHeader *)rpy_file)->compressed_size, (u8 *)rpy_thing_204,
                    ((RpyFileHeader *)rpy_file)->size);
    info = (RpyInfo *)rpy_thing_204;
    RpyGamestate *gamestate = (RpyGamestate *)((u8 *)rpy_thing_204 + 0xa0);
    for (i32 i = 0; i < (info->num_stages >= 8 ? 6 : info->num_stages); i++)
    {
        stages[gamestate->stage].gamestate_at_stage_begin = gamestate;
        stages[gamestate->stage].input_begin = (RpyFrameInput *)(gamestate + 1);
        stages[gamestate->stage].fps_counts_begin =
            (u8 *)(stages[gamestate->stage].input_begin + gamestate->num_frames);
        gamestate = (RpyGamestate *)((u8 *)(gamestate + 1) + gamestate->data_size);
    }
    if (!(g_Globals.flags_hi_45c & 1) && data != NULL)
    {
        free(data);
    }
    _chdir(g_GameWindow.exe_dir);
    return 0;
}

// FUNCTION: TH16 0x4483b0
HARNESS_CALLED i32 ReplayManager::set_end_stage(i32 extra_stage)
{
    _time64(&info->timestamp);
    info->stage = extra_stage != 0 ? extra_stage + 7 : g_Globals.stage_num;
    return 0;
}

static_assert(sizeof(RpyGamestate) == 0x294, "RpyGamestate");
static_assert(offsetof(RpyGamestate, globals) == 0x14, "RpyGamestate::globals");
static_assert(offsetof(RpyGamestate, spell_time_codes) == 0x240, "RpyGamestate::spell_time_codes");

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

// TODO: the original realigns its frame through ebx (and esp, -8), most
// likely for repopulate_options; the body matches.
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
        stage_num = g_Globals.stage_num;
        current_tick_num_in_stage = 0;
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

// TODO: register allocation in the unregister_locked blocks (the original
// keeps each func in ebx and loads the registry inside the null check) and
// our loops get alignment padding the original lacks.
// FUNCTION: TH16 0x447c80
ReplayManager::~ReplayManager()
{
    delete (RpyHeader *)rpy_file;
    for (i32 i = 0; i < 8; i++)
    {
        free_chunks(i);
    }
    delete info;
    info = NULL;
    for (i32 i = 0; i < 8; i++)
    {
        delete (RpyGamestate *)stage_gamestate_snapshots[i];
        stage_gamestate_snapshots[i] = NULL;
    }
    g_UpdateFuncRegistry->unregister_locked(on_tick_func);
    g_UpdateFuncRegistry->unregister_locked(on_tick_22_func);
    g_UpdateFuncRegistry->unregister_locked(on_draw_func);
    if (g_ReplayManager == this)
    {
        g_ReplayManager = NULL;
    }
}

// Shows the frame rate recorded in the replay while it plays back.
// FUNCTION: TH16 0x4482f0
int __fastcall ReplayManager::on_draw_47_body(void *arg)
{
    ReplayManager *replay = (ReplayManager *)arg;
    if (g_GameThread != NULL && replay->mode != REPLAY_RECORDING && replay->mode == REPLAY_PLAYBACK)
    {
        D3DXVECTOR3 pos(383.0f, 450.0f, 0.0f);
        f32 fps = replay->current_fps;
        g_AsciiManager->color.d3d = fps < 30.0f ? 0xff5050ff : fps < 50.0f ? 0xffa0a0ff : 0xffffffff;
        g_AsciiManager->create_stringf(&pos, "%3d", replay->current_fps);
        g_AsciiManager->color.d3d = 0xffffffff;
    }
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

// The replay's on_tick/on_draw functions, as LTCG inlined create_func.
static __forceinline UpdateFunc *new_replay_func(UpdateFuncCallback function, ReplayManager *replay)
{
    UpdateFunc *f = new UpdateFunc;
    f->flags |= UPDATE_FUNC_HEAP_ALLOCATED;
    f->function = function;
    f->on_registration = NULL;
    f->on_cleanup = NULL;
    f->arg = replay;
    f->flags &= ~UPDATE_FUNC_ACTIVE;
    return f;
}

// FUNCTION: TH16 0x447760
int ReplayManager::initialize(i32 mode, const char *filename)
{
    this->mode = mode;
    if (mode == REPLAY_RECORDING)
    {
        g_ReplayManager = this;
        free_chunks(g_Globals.stage_num);
        currently_recording_chunk = new_chunk(g_Globals.stage_num);
        RpyFileHeader *header = (RpyFileHeader *)new RpyHeader;
        header->magic = 0x72363174;
        header->version = 2;
        header->unk_10 = 0x100;
        rpy_file = header;
        info = new RpyInfo;
        stage_gamestate_snapshots[g_Globals.stage_num] = new RpyGamestate;
        RpyGamestate *gamestate = (RpyGamestate *)stage_gamestate_snapshots[g_Globals.stage_num];
        info->character = g_Globals.character;
        info->subshot = g_Globals.subshot;
        info->subseason = g_Globals.subseason;
        info->difficulty = g_Globals.difficulty;
        info->flag_practice = g_Globals.game_mode;
        info->flag_spell_practice = g_Globals.game_mode == 2 ? 1 : 0;
        info->spell_id = g_Globals.spell_id;
        if (g_GameThread != NULL)
        {
            info->config = g_GameThread->config;
        }
        gamestate->stage = g_Globals.stage_num;
        gamestate->rng_state = g_replay_safe_rng.seed;
        g_replay_safe_rng.generation_count = 0;
        gamestate->flag_290 = g_Supervisor.unk_700;
        if (g_Supervisor.unk_700)
        {
            gamestate->player_pos_subpixel[0] = 0;
            gamestate->player_pos_subpixel[1] = 0;
        }
        memcpy(gamestate->globals, &g_Globals, sizeof(gamestate->globals));
        for (i32 i = 0; i < 0x14; i++)
        {
            gamestate->spell_time_codes[i] = i * 0xdeaddead;
        }
        info->continues_used = g_Globals.continues_used;
        UpdateFunc *f = new_replay_func(on_tick_record_thunk, this);
        g_UpdateFuncRegistry->register_on_tick(f, 0x10);
        on_tick_func = f;
        f = new_replay_func(on_tick_22, this);
        g_UpdateFuncRegistry->register_on_tick(f, 0x22);
        on_tick_22_func = f;
        f = new_replay_func(on_draw_47, this);
        g_UpdateFuncRegistry->register_on_draw(f, 0x47);
        on_draw_func = f;
        stage_num = g_Globals.stage_num;
        current_tick_num_in_stage = -1;
    }
    else if (mode == REPLAY_PLAYBACK)
    {
        g_ReplayManager = this;
        if (read_replay_file(filename) != 0)
        {
            return -1;
        }
        g_GameThread->config = info->config;
        ReplayStageData *stage = &stages[g_Globals.stage_num];
        stage->input_current = stage->input_begin;
        stage->fps_counts_current = stage->fps_counts_begin;
        stage->frame_current = -1;
        RpyGamestate *gamestate = stage->gamestate_at_stage_begin;
        g_Globals.character = info->character;
        g_Globals.subshot = info->subshot;
        g_Globals.subseason = info->subseason;
        g_Globals.difficulty = info->difficulty;
        g_replay_safe_rng.seed = gamestate->rng_state;
        g_replay_safe_rng.generation_count = 0;
        memcpy(&g_Globals, gamestate->globals, sizeof(gamestate->globals));
        if (g_Globals.spell_id >= 0)
        {
            g_Globals.set_game_mode(2);
        }
        else
        {
            g_Globals.set_game_mode(0);
        }
        UpdateFunc *f = g_UpdateFuncRegistry->create_func(on_tick_playback_thunk);
        f->flags &= ~UPDATE_FUNC_ACTIVE;
        f->arg = this;
        g_UpdateFuncRegistry->register_on_tick(f, 0x10);
        on_tick_func = f;
        f = new_replay_func(on_tick_22, this);
        g_UpdateFuncRegistry->register_on_tick(f, 0x22);
        on_tick_22_func = f;
        f = g_UpdateFuncRegistry->create_func(on_draw_47);
        f->flags &= ~UPDATE_FUNC_ACTIVE;
        f->arg = this;
        g_UpdateFuncRegistry->register_on_draw(f, 0x47);
        on_draw_func = f;
        stage_num = -1;
    }
    else if (mode == REPLAY_LOADED && read_replay_file(filename) != 0)
    {
        return -1;
    }
    return 0;
}

extern const char *g_chara_names_short[4];

extern HANDLE g_file;

// file_close as LTCG inlined it.
static __forceinline void file_close_inline()
{
    if (g_file != INVALID_HANDLE_VALUE)
    {
        CloseHandle(g_file);
        LEAVE_CS(CS_FILE);
    }
}

// Writes to the file file_create opened, giving the file up on a short
// write.
static __forceinline void write_to_file(const void *data, DWORD size)
{
    DWORD written;
    if (g_file != INVALID_HANDLE_VALUE)
    {
        WriteFile(g_file, data, size, &written, NULL);
        if (size != written)
        {
            CloseHandle(g_file);
            LEAVE_CS(CS_FILE);
        }
    }
}

// Pads a USER section to a multiple of 4 bytes and stores its size.
static __forceinline i32 finish_user_section(u8 *section, char *end)
{
    if ((end - (char *)section) % 4 != 0)
    {
        end += 4 - (end - (char *)section) % 4;
    }
    i32 size = end - (char *)section;
    *(i32 *)(section + 4) = size;
    return size;
}

// TODO: ours realigns its frame to 64 bytes (alignment spreading up from a callee) and allocates registers differently.
// FUNCTION: TH16 0x448400
HARNESS_CALLED i32 ReplayManager::save(const char *path, const char *name, i32 unused, i32 unk_4)
{
    ReplayManager *replay = g_ReplayManager;
    i32 first_stage = 0;
    i32 last_stage = 0;
    strcpy(replay->info->name, name);
    for (i32 i = strlen(name); i < 8; i++)
    {
        replay->info->name[i] = ' ';
    }
    if (!(replay->unk_218 & 1) && unk_4 != 0)
    {
        RpyChunk *chunk = replay->currently_recording_chunk->entry;
        chunk->next_input_write_pos->input = 0xffff;
        chunk->next_input_write_pos->input_rising = 0xffff;
        chunk->next_input_write_pos->input_falling = 0xffff;
        chunk->next_input_write_pos++;
        if ((u8 *)chunk->next_input_write_pos - (u8 *)chunk >= (i32)sizeof(chunk->input))
        {
            replay->currently_recording_chunk = replay->new_chunk(replay->stage_num);
        }
    }
    char full_path[0x100];
    sprintf(full_path, "replay/%s", path);
    i32 num_stages = 0;
    i32 size = sizeof(RpyInfo);
    for (i32 i = 0; i < 8; i++)
    {
        RpyGamestate *gamestate = (RpyGamestate *)replay->stage_gamestate_snapshots[i];
        if (gamestate == NULL)
        {
            continue;
        }
        if (first_stage == 0)
        {
            first_stage = i;
        }
        last_stage = i;
        if (!(replay->unk_218 & 1))
        {
            gamestate->data_size = 0;
        }
        size += sizeof(RpyGamestate);
        for (ZunList<RpyChunk> *node = replay->recorded_chunks_by_stage[i].next; node != NULL; node = node->next)
        {
            RpyChunk *chunk = node->entry;
            size += ((u8 *)chunk->next_input_write_pos - (u8 *)chunk) / 6 * 6 + chunk->next_fps_count_write_pos -
                    chunk->fps_counts;
            if (!(replay->unk_218 & 1))
            {
                gamestate->data_size += ((u8 *)chunk->next_input_write_pos - (u8 *)chunk) / 6 * 6 +
                                        chunk->next_fps_count_write_pos - chunk->fps_counts;
                gamestate->num_frames += ((u8 *)node->entry->next_input_write_pos - (u8 *)node->entry) / 6;
            }
        }
        num_stages++;
    }
    replay->info->num_stages = num_stages;
    replay->info->score = g_Globals.score;
    replay->info->slowdown = 100.0f - (f32)(g_FpsCounter->total_actual / g_FpsCounter->total_expected) * 100.0f;
    u8 *data = (u8 *)malloc(size);
    memcpy(data, replay->info, sizeof(RpyInfo));
    i32 offset = sizeof(RpyInfo);
    for (i32 i = 0; i < 8; i++)
    {
        RpyGamestate *gamestate = (RpyGamestate *)replay->stage_gamestate_snapshots[i];
        if (gamestate == NULL)
        {
            continue;
        }
        memcpy(data + offset, gamestate, sizeof(RpyGamestate));
        offset += sizeof(RpyGamestate);
        ZunList<RpyChunk> *node;
        for (node = replay->recorded_chunks_by_stage[i].next; node != NULL; node = node->next)
        {
            RpyChunk *chunk = node->entry;
            memcpy(data + offset, chunk, ((u8 *)chunk->next_input_write_pos - (u8 *)chunk) / 6 * 6);
            offset += ((u8 *)chunk->next_input_write_pos - (u8 *)chunk) / 6 * 6;
        }
        for (node = replay->recorded_chunks_by_stage[i].next; node != NULL; node = node->next)
        {
            RpyChunk *chunk = node->entry;
            memcpy(data + offset, chunk->fps_counts, chunk->next_fps_count_write_pos - chunk->fps_counts);
            offset += chunk->next_fps_count_write_pos - chunk->fps_counts;
        }
    }
    i32 compressed_size;
    u8 *compressed = lzss_compress(data, offset, &compressed_size);
    free(data);
    zun_encrypt(compressed, compressed_size, 0x7d, 0x3a, 0x100, compressed_size);
    zun_encrypt(compressed, compressed_size, 0x5c, 0xe1, 0x400, compressed_size);
    RpyFileHeader *header = (RpyFileHeader *)replay->rpy_file;
    header->size = offset;
    header->compressed_size = compressed_size;
    header->file_size = header->compressed_size + sizeof(RpyHeader);
    _chdir(g_GameWindow.save_dir);
    file_create(full_path);
    write_to_file(replay->rpy_file, sizeof(RpyHeader));
    write_to_file(compressed, compressed_size);
    if (compressed != NULL)
    {
        free(compressed);
    }
    u8 *user = (u8 *)malloc(0xffff);
    memset(user, 0, 0xffff);
    *(u32 *)user = 0x52455355;
    user[8] = 0;
    char *text = (char *)user + 0xc;
    text += sprintf(text, "%s \x83\x8a\x83v\x83\x8c\x83" "C\x83t\x83@\x83" "C\x83\x8b\x8f\xee\x95\xf1\r\n",
                    "\x93\x8c\x95\xfb\x93V\x8b\xf3\xe0\xf6");
    text += sprintf(text, "Version %s\r\n", "1.00a");
    text += sprintf(text, "Name %s\r\n", replay->info->name);
    struct tm *date = _localtime64(&replay->info->timestamp);
    text += sprintf(text, "Date %.2d/%.2d/%.2d %.2d:%.2d\r\n", date->tm_year % 100, date->tm_mon + 1, date->tm_mday,
                    date->tm_hour, date->tm_min);
    text += sprintf(text, "Chara %s\r\n", g_chara_names_short[replay->info->character + replay->info->subshot]);
    text += sprintf(text, "Rank %s\r\n", g_difficulty_names[replay->info->difficulty]);
    if (replay->info->stage > 7)
    {
        if (first_stage == 7)
        {
            text += sprintf(text, "Extra Stage Clear\r\n");
        }
        else
        {
            text += sprintf(text, "Stage All Clear\r\n");
        }
    }
    else if (first_stage == last_stage)
    {
        if (first_stage == 7)
        {
            text += sprintf(text, "Extra Stage\r\n");
        }
        else
        {
            text += sprintf(text, "Stage %d\r\n", first_stage);
        }
    }
    else
    {
        text += sprintf(text, "Stage %d \x81` %d\r\n", first_stage, last_stage);
    }
    text += sprintf(text, "Score %d\r\n", replay->info->score);
    text += sprintf(text, "Slow Rate %2.2f\r\n", replay->info->slowdown) + 1;
    i32 user_size = finish_user_section(user, text);
    write_to_file(user, user_size);
    memset(user, 0, 0xffff);
    *(u32 *)user = 0x52455355;
    user[8] = 1;
    text = (char *)user + 0xc;
    text += sprintf(text, "\x83R\x83\x81\x83\x93\x83g\x82\xf0\x8f\x91\x82\xaf\x82\xdc\x82\xb7") + 1;
    user_size = finish_user_section(user, text);
    write_to_file(user, user_size);
    free(user);
    file_close_inline();
    _chdir(g_GameWindow.exe_dir);
    replay->unk_218 |= 1;
    return 0;
}
