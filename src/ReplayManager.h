#pragma once

#include <string.h>
#include <time.h>

#include "UpdateFunc.h"
#include "ZunList.h"
#include "types.h"

struct RpyFrameInput
{
    union
    {
        u8 unk_0[6];
        struct
        {
            u16 input;
            u16 input_rising;
            u16 input_falling;
        };
    };
};

// The game state saved at the start of each stage of a replay. ExpHP:
// zRpyGamestateSnapshot; only what decompiled code needs.
struct RpyGamestate
{
    i16 stage;
    i16 rng_state;
    // Frames of input recorded for the stage.
    i32 num_frames;
    u8 unk_8[0xc - 0x8];
    // The player's position (Player::inner.pos_subpixel).
    i32 player_pos_subpixel[2];
    // The first 0x228 bytes of g_Globals.
    u8 globals[0x228];
    // Player::inner.is_focused.
    i32 player_is_focused;
    // Capture times (Spellcard::time_code) of the stage's spell cards, in
    // the order they ended.
    i32 spell_time_codes[0x14];
    // Supervisor::unk_700 when the stage began.
    u32 flag_290 : 1;
    u32 flags_290_hi : 31;

    RpyGamestate()
    {
        memset(this, 0, sizeof(RpyGamestate));
    }
};

// The replay's description, shown in the replay menu (ExpHP:
// zRpyThingA0). Only what decompiled code needs. The timestamp is only
// 4-byte aligned.
#pragma pack(push, 4)
struct RpyInfo
{
    u8 unk_0[0xc];
    __time64_t timestamp;
    // The final score, divided by 10.
    u32 score;
    u8 unk_18[0x7c - 0x18];
    // Percentage of frames slowed down.
    f32 slowdown;
    u8 unk_80[0x84 - 0x80];
    i32 character;
    i32 subshot;
    i32 difficulty;
    // The stage the replay ends on; 8 and up for the extra stage.
    i32 stage;
    i32 continues_used;
    i32 spell_id;
    i32 subseason;
};
#pragma pack(pop)

// The start of a .rpy file as the replay manager builds it (ExpHP:
// zRpyRawFile; "t16r", version 2).
struct RpyHeader
{
    u8 data[0x24];
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
    RpyGamestate *gamestate_at_stage_begin;
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
    union
    {
        i32 flags_18;
        RpyInfo *info;
    };
    void *stage_gamestate_snapshots[8];
    ZunList<RpyChunk> recorded_chunks_by_stage[8];
    ZunList<RpyChunk> *currently_recording_chunk;
    i32 num_chunks_recorded;
    ReplayStageData stages[8];
    void *rpy_thing_204;
    union
    {
        i32 current_fps_during_playback;
        u8 current_fps;
    };
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

    int on_tick_record();
    int on_tick_playback();
    static int __fastcall on_tick_record_thunk(void *arg);
    static int __fastcall on_tick_playback_thunk(void *arg);
    static int __fastcall on_tick_22(void *arg);
    static int __fastcall on_draw_47(void *arg);
    static int __fastcall on_draw_47_body(void *arg);

    // 0x449030. Saves the game state at the start of a stage, or restores
    // it during playback. Every caller goes through g_ReplayManager.
    HARNESS_CALLED void start_stage();
    // 0x448eb0. Activates the replay's callbacks and records or replays
    // the player's state for the new stage. Every caller goes through
    // g_ReplayManager.
    HARNESS_CALLED void begin_stage();

    // 0x4483b0. Dates the replay and records the stage it ends on (the
    // extra stage as 8 and up). Every caller goes through g_ReplayManager.
    HARNESS_CALLED i32 set_end_stage(i32 extra_stage);
    // 0x448400. Saves g_ReplayManager's replay under the name. The third
    // argument is the same at every call site; LTCG folded it.
    HARNESS_CALLED i32 save(const char *path, const char *name, i32 unused, i32 unk_4);
};

// 0x449120. Clears the game's button state (not the hardware's).
void clear_input_state();

extern ReplayManager *g_ReplayManager;
extern char g_current_replay_filename[0x100];
