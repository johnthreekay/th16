// Replay sync test: see port/include/port_replay_test.h.
#include <errno.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <string>

#include "port_platform.h"
#include "port_replay_test.h"

#include "Globals.h"
#include "Player.h"
#include "ReplayManager.h"
#include "Rng.h"

namespace
{

// Idle frames in the title menu before the replay starts (the title screen
// is still fading in before that).
constexpr int START_AFTER_IDLE_FRAMES = 60;

enum State
{
    STATE_OFF,
    // In the title menu, until the replay starts.
    STATE_WAITING,
    // The title menu asked for the replay; playback has not run a frame yet.
    STATE_STARTED,
    // Playback is running.
    STATE_PLAYING,
};

State g_state = STATE_OFF;
std::string g_name;
bool g_fast_forward = true;
int g_trace_every = 0;
// TH16_REPLAY_TEST_DUMP: every frame's g_Globals (the part replays store),
// replay RNG and player position, as hex, for comparing with the original.
FILE *g_dump = NULL;
int g_failures = 0;
int g_stages_checked = 0;

// One field of Globals, compared as 32-bit words. `gameplay` marks the ones
// a desync changes; the others are reported but do not fail the test (they
// may legitimately differ, for example per-stage counters the game resets
// between this check and the point the original took its snapshot).
struct GlobalsField
{
    const char *name;
    size_t offset;
    size_t count;
    bool gameplay;
};

#define FIELD(name, gameplay) {#name, offsetof(Globals, name), 1, gameplay}
#define ARRAY(name, gameplay) \
    {#name, offsetof(Globals, name), sizeof(((Globals *)0)->name) / sizeof(i32), gameplay}

const GlobalsField g_fields[] = {
    FIELD(stage_num, false),
    FIELD(weird_stage_num, false),
    FIELD(chapter, false),
    FIELD(time_in_stage, false),
    FIELD(time_in_chapter, false),
    FIELD(character, true),
    FIELD(subshot, true),
    FIELD(subseason, true),
    FIELD(score, true),
    FIELD(difficulty, true),
    FIELD(continues_used, true),
    FIELD(rank, false),
    FIELD(graze, true),
    FIELD(graze_in_chapter, false),
    FIELD(spell_id, false),
    FIELD(miss_count, true),
    FIELD(unk_40, false),
    FIELD(num_point_items_collected, true),
    FIELD(piv, true),
    FIELD(initial_piv, true),
    FIELD(max_piv, true),
    FIELD(power, true),
    FIELD(max_power, true),
    FIELD(power_per_level, false),
    FIELD(unk_60, false),
    FIELD(lives, true),
    FIELD(life_fragments, true),
    FIELD(next_score_extend_index, true),
    FIELD(bombs, true),
    FIELD(bomb_fragments, true),
    FIELD(season_power, true),
    FIELD(max_season_power, true),
    ARRAY(season_level_deltas, true),
    ARRAY(season_level_thresholds, true),
    ARRAY(unk_c8, false),
    FIELD(full_value_item_score, true),
    FIELD(unk_d4, false),
    FIELD(full_value_item_count, true),
    FIELD(unk_dc, false),
    ARRAY(last_collect_pos, false),
    FIELD(item_spawn_count, true),
    FIELD(enemies_spawned_in_chapter, false),
    FIELD(enemies_destroyed_in_chapter, false),
    ARRAY(music_filename, false),
    ARRAY(unk_1f8, false),
    FIELD(unk_200, false),
    FIELD(unk_204, false),
    FIELD(unk_208, false),
    FIELD(unk_20c, false),
    FIELD(unk_210, false),
    FIELD(unk_214, false),
    FIELD(unk_218, false),
    FIELD(unk_21c, false),
    FIELD(unk_220, false),
    FIELD(unk_224, false),
};

#undef FIELD
#undef ARRAY

static_assert(sizeof(((RpyGamestate *)0)->globals) == 0x228, "the snapshot covers Globals up to unk_224");

void report(const char *format, ...) __attribute__((format(printf, 1, 2)));
void report(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    fputs("[replay-test] ", stdout);
    vfprintf(stdout, format, args);
    fputc('\n', stdout);
    va_end(args);
    fflush(stdout);
}

[[noreturn]] void finish(int code)
{
    if (g_dump != NULL)
    {
        fclose(g_dump);
    }
    report("result: %s (%d stage transitions checked, %d failed checks)",
           code == 0 ? "IN SYNC" : code == 1 ? "DESYNC" : "DID NOT FINISH", g_stages_checked, g_failures);
    fflush(stdout);
    fflush(stderr);
    _exit(code);
}

void check(bool ok, const char *what, int stage, int frame, long long expected, long long actual, bool counts)
{
    if (ok)
    {
        return;
    }
    if (counts)
    {
        g_failures++;
    }
    report("  %s stage %d frame %d: %s expected %lld, got %lld", counts ? "MISMATCH" : "differs ", stage, frame,
           what, expected, actual);
}

} // namespace

bool port_replay_test_init(const char *file, const char *save_dir)
{
    std::string path = file;
    size_t slash = path.find_last_of('/');
    g_name = slash == std::string::npos ? path : path.substr(slash + 1);
    std::string dir = std::string(save_dir) + "/replay";
    mkdir(save_dir, 0755);
    mkdir(dir.c_str(), 0755);
    std::string target = dir + "/" + g_name;
    if (path != target)
    {
        FILE *in = fopen(path.c_str(), "rb");
        if (in == NULL)
        {
            fprintf(stderr, "[replay-test] cannot open %s: %s\n", path.c_str(), strerror(errno));
            return false;
        }
        FILE *out = fopen(target.c_str(), "wb");
        if (out == NULL)
        {
            fprintf(stderr, "[replay-test] cannot write %s: %s\n", target.c_str(), strerror(errno));
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
    }
    const char *speed = getenv("TH16_REPLAY_TEST_FAST");
    g_fast_forward = speed == NULL || strcmp(speed, "0") != 0;
    const char *trace = getenv("TH16_REPLAY_TEST_TRACE");
    g_trace_every = trace != NULL ? atoi(trace) : 0;
    const char *dump = getenv("TH16_REPLAY_TEST_DUMP");
    if (dump != NULL && dump[0] != '\0')
    {
        g_dump = fopen(dump, "w");
        if (g_dump != NULL)
        {
            // Whole lines survive a crash.
            setvbuf(g_dump, NULL, _IOLBF, 1 << 16);
        }
    }
    g_state = STATE_WAITING;
    report("replay %s", g_name.c_str());
    return true;
}

bool port_replay_test_active()
{
    return g_state != STATE_OFF;
}

const char *port_replay_test_title_tick(int idle_frames)
{
    if (g_state == STATE_PLAYING)
    {
        port_replay_test_abort("the game returned to the title screen");
    }
    if (g_state != STATE_WAITING || idle_frames < START_AFTER_IDLE_FRAMES)
    {
        return NULL;
    }
    g_state = STATE_STARTED;
    return g_name.c_str();
}

void port_replay_test_describe(const ReplayManager *replay)
{
    const RpyInfo *info = replay->info;
    report("character %d, subseason %d, difficulty %d, %d stage(s), ends on stage %d, score %u", info->character,
           info->subseason, info->difficulty, info->num_stages, info->stage, info->score * 10);
    for (int i = 0; i < 8; i++)
    {
        const RpyGamestate *gs = replay->stages[i].gamestate_at_stage_begin;
        if (gs == NULL)
        {
            continue;
        }
        const Globals *g = (const Globals *)gs->globals;
        report("  stage %d: %d frames, rng seed %u, score %u, lives %d+%d, bombs %d+%d, power %d, season %d, "
               "player (%d, %d)",
               gs->stage, gs->num_frames, (u16)gs->rng_state, g->score * 10, g->lives, g->life_fragments, g->bombs,
               g->bomb_fragments, g->power, g->season_power, gs->player_pos_subpixel[0], gs->player_pos_subpixel[1]);
    }
}

static void dump_hex(const void *data, size_t size)
{
    for (size_t i = 0; i < size; i++)
    {
        fprintf(g_dump, "%02x", ((const u8 *)data)[i]);
    }
}

void port_replay_test_frame(const ReplayManager *replay)
{
    if (replay->stage_num < 0 || replay->stage_num >= 8)
    {
        return;
    }
    int frame = replay->stages[replay->stage_num].frame_current;
    if (g_dump != NULL)
    {
        fprintf(g_dump, "%d %d ", replay->stage_num, frame);
        dump_hex(&g_Globals, sizeof(((RpyGamestate *)0)->globals));
        fputc(' ', g_dump);
        u32 rng[2] = {g_replay_safe_rng.seed, g_replay_safe_rng.generation_count};
        dump_hex(rng, sizeof(rng));
        fputc(' ', g_dump);
        i32 pos[2] = {g_Player != NULL ? g_Player->inner.pos_subpixel.x : 0,
                      g_Player != NULL ? g_Player->inner.pos_subpixel.y : 0};
        dump_hex(pos, sizeof(pos));
        fputc('\n', g_dump);
    }
    if (g_trace_every <= 0)
    {
        return;
    }
    if (frame % g_trace_every != 0)
    {
        return;
    }
    report("trace stage %d frame %d: score %u, lives %d+%d, bombs %d+%d, power %d, graze %d, season %d, "
           "rng %u/%u, player (%d, %d)",
           replay->stage_num, frame, g_Globals.score * 10, g_Globals.lives, g_Globals.life_fragments, g_Globals.bombs,
           g_Globals.bomb_fragments, g_Globals.power, g_Globals.graze, g_Globals.season_power, g_replay_safe_rng.seed,
           g_replay_safe_rng.generation_count, g_Player != NULL ? g_Player->inner.pos_subpixel.x : 0,
           g_Player != NULL ? g_Player->inner.pos_subpixel.y : 0);
}

bool port_replay_test_fast_forward()
{
    // Only called during playback.
    if (g_state == STATE_STARTED)
    {
        g_state = STATE_PLAYING;
    }
    return g_state == STATE_PLAYING && g_fast_forward;
}

void port_replay_test_stage_start(const ReplayManager *replay, const RpyGamestate *recorded)
{
    if (g_state != STATE_STARTED && g_state != STATE_PLAYING)
    {
        return;
    }
    int prev = replay->stage_num;
    int stage = recorded->stage;
    int frame = prev >= 0 && prev < 8 ? replay->stages[prev].frame_current : -1;
    int recorded_frames = prev >= 0 && prev < 8 && replay->stages[prev].gamestate_at_stage_begin != NULL
                              ? replay->stages[prev].gamestate_at_stage_begin->num_frames
                              : -1;
    g_stages_checked++;
    report("stage %d -> %d: score %u (recorded %u), stage %d took %d frames (recorded %d), rng steps %u", prev, stage,
           g_Globals.score * 10, ((const Globals *)recorded->globals)->score * 10, prev, frame, recorded_frames,
           g_replay_safe_rng.generation_count);
    check(frame == recorded_frames, "frames in the previous stage", prev, frame, recorded_frames, frame, true);
    check(g_replay_safe_rng.seed == (u16)recorded->rng_state, "replay RNG seed", stage, frame, (u16)recorded->rng_state,
          g_replay_safe_rng.seed, true);
    const u8 *live = (const u8 *)&g_Globals;
    for (const GlobalsField &field : g_fields)
    {
        for (size_t i = 0; i < field.count; i++)
        {
            i32 expected;
            i32 actual;
            memcpy(&expected, recorded->globals + field.offset + i * 4, 4);
            memcpy(&actual, live + field.offset + i * 4, 4);
            if (expected != actual)
            {
                char what[96];
                if (field.count > 1)
                {
                    snprintf(what, sizeof(what), "g_Globals.%s[%zu]", field.name, i);
                }
                else
                {
                    snprintf(what, sizeof(what), "g_Globals.%s", field.name);
                }
                check(false, what, stage, frame, expected, actual, field.gameplay);
            }
        }
    }
    if (g_Player != NULL)
    {
        check(g_Player->inner.pos_subpixel.x == recorded->player_pos_subpixel[0], "player x (subpixels)", stage,
              frame, recorded->player_pos_subpixel[0], g_Player->inner.pos_subpixel.x, false);
        check(g_Player->inner.pos_subpixel.y == recorded->player_pos_subpixel[1], "player y (subpixels)", stage,
              frame, recorded->player_pos_subpixel[1], g_Player->inner.pos_subpixel.y, false);
    }
}

void port_replay_test_replay_end()
{
    if (g_state != STATE_STARTED && g_state != STATE_PLAYING)
    {
        return;
    }
    const ReplayManager *replay = g_ReplayManager;
    int stage = g_Globals.stage_num;
    int frame = -1;
    int recorded_frames = -1;
    if (stage >= 0 && stage < 8)
    {
        frame = replay->stages[stage].frame_current;
        if (replay->stages[stage].gamestate_at_stage_begin != NULL)
        {
            recorded_frames = replay->stages[stage].gamestate_at_stage_begin->num_frames;
        }
    }
    report("end in stage %d at frame %d (stage recorded %d frames): score %u (recorded %u)", stage, frame,
           recorded_frames, g_Globals.score * 10, replay->info->score * 10);
    check(g_Globals.score == replay->info->score, "final score / 10", stage, frame, replay->info->score,
          g_Globals.score, true);
    finish(g_failures == 0 ? 0 : 1);
}

void port_replay_test_abort(const char *why)
{
    if (g_state == STATE_OFF)
    {
        return;
    }
    const ReplayManager *replay = g_ReplayManager;
    int stage = g_Globals.stage_num;
    int frame = replay != NULL && stage >= 0 && stage < 8 ? replay->stages[stage].frame_current : -1;
    report("stopped in stage %d at frame %d: %s (score %u, lives %d, bombs %d, power %d)", stage, frame, why,
           g_Globals.score * 10, g_Globals.lives, g_Globals.bombs, g_Globals.power);
    finish(2);
}
