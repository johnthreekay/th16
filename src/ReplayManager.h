#pragma once

#include <string.h>

#include "UpdateFunc.h"
#include "ZunList.h"
#include "types.h"

struct RpyFrameInput
{
    u8 unk_0[6];
};

// A block of recorded input, 900 frames long. ExpHP: zRpyChunk.
struct RpyChunk
{
    RpyFrameInput input[900];
    RpyFrameInput *next_input_write_pos;
    u8 fps_counts[30];
    u8 unk_153a[0x18a0 - 0x153a];
    u8 *next_fps_count_write_pos;
    ZunList<RpyChunk> node;

    RpyChunk()
    {
        memset(this, 0, sizeof(RpyChunk));
    }
};

// One stage of a replay being played back. ExpHP: zReplayLoadedStageData.
struct ReplayStageData
{
    RpyFrameInput *input_begin;
    RpyFrameInput *input_current;
    u8 *fps_counts_begin;
    u8 *fps_counts_current;
    void *gamestate_at_stage_begin;
    i32 frame_current;
    ZunList<ReplayStageData> node;

    ReplayStageData();
    ~ReplayStageData();
};

enum ReplayManagerMode
{
    REPLAY_RECORDING = 0,
    REPLAY_PLAYBACK = 1,
    REPLAY_LOADED = 2,
};

// Records and plays back replays. Layout from ExpHP's th-re-data
// (zReplayManager).
struct ReplayManager
{
    u32 flags;
    UpdateFunc *on_tick_func;
    UpdateFunc *on_draw_func;
    i32 mode;
    i32 unk_10;
    void *rpy_file;
    i32 flags_18;
    void *stage_gamestate_snapshots[8];
    ZunList<RpyChunk> recorded_chunks_by_stage[8];
    RpyChunk *currently_recording_chunk;
    i32 num_chunks_recorded;
    ReplayStageData stages[8];
    void *rpy_thing_204;
    i32 current_fps_during_playback;
    i32 current_tick_num_in_stage;
    UpdateFunc *on_tick_22_func;
    i32 stage_num;
    i32 unk_218;
    char filename[0x100];

    ReplayManager()
    {
        memset(this, 0, sizeof(ReplayManager));
    }
    ~ReplayManager();
    int initialize(i32 mode, const char *filename);
    int read_replay_file(const char *filename);
    HARNESS_CALLED static ReplayManager *create(i32 mode);
    HARNESS_CALLED static ReplayManager *create_from_file(const char *filename);
    HARNESS_CALLED static void destroy(ReplayManager *replay);
    ZunList<RpyChunk> *new_chunk(i32 stage);
    void free_chunks(i32 stage);

    static int __fastcall on_tick_22(void *arg);
    static int __fastcall on_draw_47(void *arg);
    static int __fastcall on_draw_47_body(void *arg);
};

extern ReplayManager *g_ReplayManager;
extern char g_current_replay_filename[0x100];
