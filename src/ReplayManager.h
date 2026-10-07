#pragma once

#include <string.h>
#include <time.h>

#include "GameThread.h"
#include "UpdateFunc.h"
#include "ZunList.h"
#include "types.h"

// .rpy files: an RpyFileHeader, then the encrypted, LZSS-compressed replay
// (RpyInfo, then per stage an RpyGamestate followed by its frames of input
// and its frame rate samples), then two USER sections (the info text and a
// comment) that the game writes but never reads.

// "t16r"
constexpr u32 RPY_MAGIC = 0x72363174;
constexpr u16 RPY_VERSION = 2;
// "USER"
constexpr u32 RPY_USER_MAGIC = 0x52455355;

// One frame of recorded input (the game's button bits). A frame of all
// 0xffff marks where the replay ends.
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
    // Bytes of input and fps data after the snapshot.
    i32 data_size;
    // The player's position (Player::inner.pos_subpixel).
    i32 player_pos_subpixel[2];
    // The first 0x228 bytes of g_Globals.
    u8 globals[0x228];
    // Player::inner.is_focused.
    i32 player_is_focused;
    // Capture times (Spellcard::time_code) of the stage's spell cards, in
    // the order they ended.
    i32 spell_time_codes[0x14];
    // Supervisor::new_game_started when the stage began.
    u32 new_game_started : 1;
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
    char name[0xa];
    union
    {
        // 2: spell practice.
        u16 flags_a;
        struct
        {
            // The low bit of Globals::game_mode.
            u8 flag_practice : 1;
            u8 flag_spell_practice : 1;
            u8 flags_a_2 : 6;
            u8 flags_a_hi;
        };
    };
    __time64_t timestamp;
    // The final score, divided by 10.
    u32 score;
    // The game thread's settings when the replay was recorded.
    ConfigData config;
    // Percentage of frames slowed down.
    f32 slowdown;
    // Stage snapshots stored after this.
    i32 num_stages;
    i32 character;
    i32 subshot;
    i32 difficulty;
    // The stage the replay ends on; 8 and up for the extra stage.
    i32 stage;
    i32 continues_used;
    i32 spell_id;
    i32 subseason;

    RpyInfo()
    {
        memset(this, 0, sizeof(RpyInfo));
    }
};
#pragma pack(pop)

// The start of a .rpy file as the replay manager allocates it (ExpHP:
// zRpyRawFile); RpyFileHeader has its fields.
struct RpyHeader
{
    u8 data[0x24];

    RpyHeader()
    {
        memset(this, 0, sizeof(RpyHeader));
    }
};

// RpyHeader's fields.
struct RpyFileHeader
{
    // RPY_MAGIC and RPY_VERSION.
    u32 magic;
    u16 version;
    u8 unk_6[0xc - 0x6];
    // Where the USER sections start: the header plus the compressed data.
    u32 user_offset;
    // 0x100 in every file the game writes.
    u32 unk_10;
    u8 unk_14[0x1c - 0x14];
    // Of the encrypted, compressed data after the header.
    u32 compressed_size;
    u32 size;
};

// A block of recorded input, 900 frames long, with a frame rate sample for
// every 30 frames. ExpHP: zRpyChunk.
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
    // Only read, for the replay menus.
    REPLAY_LOADED = 2,
};

// Records the game's input into a replay and saves it, plays one back, or
// just reads one for the menus. Layout from ExpHP's th-re-data
// (zReplayManager).
struct ReplayManager
{
    u32 flags;
    UpdateFunc *on_tick_func;
    UpdateFunc *on_draw_func;
    // A ReplayManagerMode.
    i32 mode;
    // Set to 1 when playback reaches stage 3; never read.
    i32 unk_10;
    // The file header (an RpyHeader).
    void *rpy_file;
    union
    {
        i32 flags_18;
        RpyInfo *info;
    };
    // While recording: each stage's RpyGamestate.
    void *stage_gamestate_snapshots[8];
    ZunList<RpyChunk> recorded_chunks_by_stage[8];
    ZunList<RpyChunk> *currently_recording_chunk;
    i32 num_chunks_recorded;
    // While playing back: where each stage's data is in replay_data.
    ReplayStageData stages[8];
    // The decompressed replay (RpyInfo first) read from the file.
    void *replay_data;
    // The frame rate recorded for the current 30 frames of playback.
    union
    {
        i32 current_fps_during_playback;
        u8 current_fps;
    };
    i32 current_tick_num_in_stage;
    UpdateFunc *fast_forward_func;
    i32 stage_num;
    // Bit 0: the replay was saved already, so saving again does not add
    // the end marker or count the stages' frames again.
    i32 save_flags;
    char filename[0x100];

    ReplayManager()
    {
        memset(this, 0, sizeof(ReplayManager));
    }
    ~ReplayManager();
    // 0x447760. Sets up recording (the file header, the info and the
    // first stage's snapshot) or playback, with their callbacks.
    int initialize(i32 mode, const char *filename);
    // 0x448c10. Reads, decrypts and decompresses a .rpy file (from the
    // replay directory, or the game's data for a demo) and finds each
    // stage's data in it.
    int read_replay_file(const char *filename);
    HARNESS_CALLED static ReplayManager *create(i32 mode);
    HARNESS_CALLED static ReplayManager *create_from_file(const char *filename);
    HARNESS_CALLED static void destroy(ReplayManager *replay);
    // 0x4491c0. Appends an empty chunk to the stage's recording.
    ZunList<RpyChunk> *new_chunk(i32 stage);
    // 0x449270. Frees the stage's recorded chunks.
    void free_chunks(i32 stage);

    // 0x447fd0. Records the frame's input (and every 30 frames the frame
    // rate). Priority 0x10, before the game reads input.
    int on_tick_record();
    // 0x448130. Replaces the frame's input with the recorded one; at the
    // end marker, opens the replay end menu.
    int on_tick_playback();
    static int __fastcall on_tick_record_thunk(void *arg);
    static int __fastcall on_tick_playback_thunk(void *arg);
    // 0x448e40. Fast-forwards playback while shot or skip is held: the
    // tick list runs 8 times per frame (priority 0x22).
    static int __fastcall on_tick_fast_forward(void *arg);
    // 0x448e90 and 0x4482f0. Shows the recorded frame rate during playback
    // (priority 0x47), unless the game is paused.
    static int __fastcall on_draw_fps(void *arg);
    static int __fastcall draw_fps(void *arg);

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
    // 0x448400. Saves g_ReplayManager's replay under the name, adding the
    // end marker the first time if add_end_marker is set. The third
    // argument is the same at every call site; LTCG folded it.
    HARNESS_CALLED i32 save(const char *path, const char *name, i32 unused, i32 add_end_marker);
};

// 0x449120. Clears the game's button state (not the hardware's).
void clear_input_state();

extern ReplayManager *g_ReplayManager;
extern char g_current_replay_filename[0x100];
