// Replay recording test: see port/include/port_record_test.h.
#include <errno.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <string>

#include "port_platform.h"
#include "port_record_test.h"

#include "GameThread.h"
#include "Globals.h"
#include "Player.h"
#include "ReplayManager.h"
#include "Rng.h"
#include "Supervisor.h"

// What ReplayManager::save writes is these structs' bytes, so a replay the
// port saves only reads in the original if they keep the original's
// layout in a 64-bit build too (ReplayManager.cpp checks RpyInfo's and
// RpyGamestate's fields).
static_assert(sizeof(RpyFrameInput) == 6, "RpyFrameInput");
static_assert(offsetof(RpyChunk, input) == 0, "save copies a chunk's input from the chunk's start");
static_assert(sizeof(RpyHeader) == 0x24, "RpyHeader");
static_assert(offsetof(RpyFileHeader, user_offset) == 0xc, "RpyFileHeader::user_offset");
static_assert(offsetof(RpyFileHeader, unk_10) == 0x10, "RpyFileHeader::unk_10");
static_assert(offsetof(RpyFileHeader, compressed_size) == 0x1c, "RpyFileHeader::compressed_size");
static_assert(offsetof(RpyFileHeader, size) == 0x20, "RpyFileHeader::size");
static_assert(sizeof(RpyInfo) == 0xa0, "RpyInfo");
static_assert(offsetof(RpyInfo, config) == 0x18 && sizeof(ConfigData) == 0x64, "RpyInfo::config");
static_assert(offsetof(RpyInfo, subseason) == 0x9c, "RpyInfo::subseason");
static_assert(sizeof(RpyGamestate) == 0x294, "RpyGamestate");
static_assert(offsetof(RpyGamestate, player_pos_subpixel) == 0xc, "RpyGamestate::player_pos_subpixel");
static_assert(offsetof(RpyGamestate, player_is_focused) == 0x23c, "RpyGamestate::player_is_focused");

namespace
{

// Idle frames in the title menu before the game starts (as the replay test
// waits for the title screen to fade in).
constexpr int START_AFTER_IDLE_FRAMES = 60;

// Written into the saved replay's info, padded to 8 like a name entered in
// the menu.
const char RECORDED_NAME[] = "PORTTEST";

enum State
{
    STATE_OFF,
    // In the title menu, until the game starts.
    STATE_WAITING,
    // The game was started.
    STATE_STARTED,
};

State g_state = STATE_OFF;
std::string g_source_name;
std::string g_output_name;
// SOURCE as the replay menu reads it (ReplayManager::create_from_file).
ReplayManager *g_source = NULL;
bool g_fast_forward = true;
int g_differences = 0;
int g_stages_begun = 0;

void report(const char *format, ...) __attribute__((format(printf, 1, 2)));
void report(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    fputs("[record-test] ", stdout);
    vfprintf(stdout, format, args);
    fputc('\n', stdout);
    va_end(args);
    fflush(stdout);
}

[[noreturn]] void finish(int code)
{
    report("result: %s (%d stages recorded, %d snapshot differences)",
           code == 0 ? "SAVED" : code == 1 ? "SAVED WITH DIFFERENCES" : "DID NOT FINISH", g_stages_begun,
           g_differences);
    fflush(stdout);
    fflush(stderr);
    _exit(code);
}

// The score as shown: the game keeps it divided by 10, and good runs pass
// 2^32 points.
unsigned long long points(u32 score)
{
    return score * 10ULL;
}

const RpyGamestate *source_stage(int stage)
{
    if (g_source == NULL || stage < 0 || stage >= 8)
    {
        return NULL;
    }
    return g_source->stages[stage].gamestate_at_stage_begin;
}

// The first stage's snapshot is taken before the stage starts
// (ReplayManager::initialize), so its per-chapter values are what the
// previous game in the same process left: a new game does not reset them,
// the stage does. The port's process is fresh, so they are zero here.
bool leftover_from_previous_game(size_t offset)
{
    auto in = [offset](size_t begin, size_t size) { return offset >= begin && offset < begin + size; };
    return in(offsetof(Globals, chapter), 3 * sizeof(i32)) ||
           in(offsetof(Globals, graze_in_chapter), sizeof(i32)) || in(offsetof(Globals, unk_dc), sizeof(i32)) ||
           in(offsetof(Globals, last_collect_pos), sizeof(Float3)) ||
           in(offsetof(Globals, enemies_spawned_in_chapter), 2 * sizeof(i32)) ||
           offset >= offsetof(Globals, music_filename);
}

// Of those, the counters nothing resets at all (they only ever add up,
// across games): in later stages they differ from the source by the first
// stage's difference.
const size_t g_carried_offsets[] = {offsetof(Globals, graze_in_chapter), offsetof(Globals, enemies_spawned_in_chapter),
                                    offsetof(Globals, enemies_destroyed_in_chapter)};
i32 g_carried_differences[3];

i32 *carried_difference(size_t offset)
{
    for (int i = 0; i < 3; i++)
    {
        if (g_carried_offsets[i] == offset)
        {
            return &g_carried_differences[i];
        }
    }
    return NULL;
}

void differ(const char *what, int stage, long long source, long long recorded)
{
    g_differences++;
    report("  DIFFERS stage %d: %s: source %lld, recorded %lld", stage, what, source, recorded);
}

} // namespace

bool port_record_test_init(const char *source, const char *name, const char *save_dir)
{
    std::string path = source;
    size_t slash = path.find_last_of('/');
    g_source_name = slash == std::string::npos ? path : path.substr(slash + 1);
    g_output_name = name;
    if (g_output_name.empty() || g_output_name.find('/') != std::string::npos || g_output_name == g_source_name)
    {
        fprintf(stderr, "[record-test] --record-to wants a file name (in replay/) other than the source's\n");
        return false;
    }
    std::string dir = std::string(save_dir) + "/replay";
    mkdir(save_dir, 0755);
    mkdir(dir.c_str(), 0755);
    std::string target = dir + "/" + g_source_name;
    FILE *in = fopen(path.c_str(), "rb");
    if (in == NULL)
    {
        fprintf(stderr, "[record-test] cannot open %s: %s\n", path.c_str(), strerror(errno));
        return false;
    }
    FILE *out = fopen(target.c_str(), "wb");
    if (out == NULL)
    {
        fprintf(stderr, "[record-test] cannot write %s: %s\n", target.c_str(), strerror(errno));
        fclose(in);
        return false;
    }
    char buffer[65536];
    size_t n;
    while ((n = fread(buffer, 1, sizeof(buffer), in)) > 0)
    {
        fwrite(buffer, 1, n, out);
    }
    fclose(in);
    fclose(out);
    remove((dir + "/" + g_output_name).c_str());
    const char *speed = getenv("TH16_RECORD_TEST_FAST");
    g_fast_forward = speed == NULL || strcmp(speed, "0") != 0;
    g_state = STATE_WAITING;
    report("recording %s with the input of %s", g_output_name.c_str(), g_source_name.c_str());
    return true;
}

bool port_record_test_active()
{
    return g_state != STATE_OFF;
}

bool port_record_test_title_tick(int idle_frames)
{
    if (g_state == STATE_STARTED)
    {
        port_record_test_abort("the game returned to the title screen");
    }
    if (g_state != STATE_WAITING || idle_frames < START_AFTER_IDLE_FRAMES)
    {
        return false;
    }
    g_state = STATE_STARTED;
    return true;
}

const ReplayManager *port_record_test_source()
{
    if (g_source == NULL)
    {
        g_source = ReplayManager::create_from_file(g_source_name.c_str());
        if (g_source == NULL)
        {
            port_record_test_abort("the source replay could not be read");
        }
        const RpyInfo *info = g_source->info;
        report("source: character %d, subseason %d, difficulty %d, %d stage(s), ends on stage %d, score %llu",
               info->character, info->subseason, info->difficulty, info->num_stages, info->stage, points(info->score));
    }
    return g_source;
}

void port_record_test_initialize(ReplayManager *replay)
{
    (void)replay;
    if (g_state != STATE_STARTED)
    {
        return;
    }
    const RpyGamestate *first = NULL;
    for (int i = 0; i < 8 && first == NULL; i++)
    {
        first = source_stage(i);
    }
    if (first == NULL)
    {
        port_record_test_abort("the source replay has no stage");
    }
    // What playback sets at this point (ReplayManager::initialize): the
    // recording then starts from the state the original's did.
    g_replay_safe_rng.seed = first->rng_state;
    if (g_GameThread != NULL)
    {
        g_GameThread->config = g_source->info->config;
    }
    // The recorded input already has the focus that this setting adds
    // while shot is held.
    g_Supervisor.config.flags &= ~CONFIG_SHOT_HOLD_FOCUS;
}

unsigned short port_record_test_input(const ReplayManager *replay)
{
    int tick = replay->current_tick_num_in_stage;
    const RpyGamestate *gamestate = source_stage(replay->stage_num);
    if (gamestate == NULL || tick < 0 || tick >= gamestate->num_frames)
    {
        return 0;
    }
    const RpyFrameInput *frame = &g_source->stages[replay->stage_num].input_begin[tick];
    if (frame->input == 0xffff && frame->input_rising == 0xffff && frame->input_falling == 0xffff)
    {
        return 0;
    }
    return frame->input;
}

bool port_record_test_fast_forward()
{
    return g_state == STATE_STARTED && g_fast_forward;
}

void port_record_test_stage_begin(const ReplayManager *replay)
{
    if (g_state != STATE_STARTED)
    {
        return;
    }
    int stage = replay->stage_num;
    const RpyGamestate *recorded = (const RpyGamestate *)replay->stage_gamestate_snapshots[stage];
    const RpyGamestate *source = source_stage(stage);
    g_stages_begun++;
    if (source == NULL)
    {
        g_differences++;
        report("stage %d begins, which the source replay does not have", stage);
        return;
    }
    const Globals *g = (const Globals *)recorded->globals;
    report("stage %d begins: score %llu (source %llu), rng seed %u (source %u)", stage, points(g->score),
           points(((const Globals *)source->globals)->score), (u16)recorded->rng_state, (u16)source->rng_state);
    int before = g_differences;
    if (recorded->stage != source->stage)
    {
        differ("stage", stage, source->stage, recorded->stage);
    }
    if (recorded->rng_state != source->rng_state)
    {
        differ("replay RNG seed", stage, (u16)source->rng_state, (u16)recorded->rng_state);
    }
    for (size_t offset = 0; offset < sizeof(recorded->globals); offset += 4)
    {
        i32 expected;
        i32 actual;
        memcpy(&expected, source->globals + offset, 4);
        memcpy(&actual, recorded->globals + offset, 4);
        if (expected == actual)
        {
            continue;
        }
        i32 *carried = carried_difference(offset);
        if (recorded->new_game_started && leftover_from_previous_game(offset))
        {
            report("  note: g_Globals+0x%zx: source %d, recorded %d (left by the original's previous game)", offset,
                   expected, actual);
            if (carried != NULL)
            {
                *carried = expected - actual;
            }
            continue;
        }
        if (!recorded->new_game_started && carried != NULL && expected - actual == *carried)
        {
            report("  note: g_Globals+0x%zx: source %d, recorded %d (the previous game's count, carried)", offset,
                   expected, actual);
            continue;
        }
        char what[48];
        snprintf(what, sizeof(what), "g_Globals+0x%zx", offset);
        differ(what, stage, expected, actual);
    }
    for (int i = 0; i < 2; i++)
    {
        if (recorded->player_pos_subpixel[i] != source->player_pos_subpixel[i])
        {
            differ(i == 0 ? "player x (subpixels)" : "player y (subpixels)", stage, source->player_pos_subpixel[i],
                   recorded->player_pos_subpixel[i]);
        }
    }
    if (recorded->player_is_focused != source->player_is_focused)
    {
        differ("player focused", stage, source->player_is_focused, recorded->player_is_focused);
    }
    if (recorded->new_game_started != source->new_game_started)
    {
        differ("new game started", stage, source->new_game_started, recorded->new_game_started);
    }
    if (g_differences == before)
    {
        report("  snapshot matches the source's");
    }
}

void port_record_test_game_thread_end(int next_gamemode)
{
    if (g_state != STATE_STARTED || next_gamemode == GAMEMODE_NEXT_STAGE)
    {
        return;
    }
    if (next_gamemode != GAMEMODE_ENDING && next_gamemode != GAMEMODE_TITLE_SCORE_ENTRY)
    {
        char why[64];
        snprintf(why, sizeof(why), "the game ended without a clear (game mode %d next)", next_gamemode);
        port_record_test_abort(why);
    }
    // What the replay save menu does once the ending or the score entry is
    // over (MainMenuStates.cpp): the stage is "cleared", and there is no
    // end marker.
    ReplayManager *replay = g_ReplayManager;
    replay->set_end_stage(1);
    replay->save(g_output_name.c_str(), RECORDED_NAME, 0, 0);
    report("saved replay/%s: score %llu (source %llu)", g_output_name.c_str(), points(g_Globals.score),
           points(g_source->info->score));
    if (g_Globals.score != g_source->info->score)
    {
        differ("final score / 10", g_Globals.stage_num, g_source->info->score, g_Globals.score);
    }
    finish(g_differences == 0 ? 0 : 1);
}

void port_record_test_abort(const char *why)
{
    if (g_state == STATE_OFF)
    {
        return;
    }
    report("stopped in stage %d: %s (score %llu, lives %d, bombs %d, power %d)", g_Globals.stage_num, why,
           points(g_Globals.score), g_Globals.lives, g_Globals.bombs, g_Globals.power);
    finish(2);
}
