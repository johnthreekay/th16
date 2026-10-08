// Replay sync test (port only): `th16 --replay FILE` plays a replay recorded
// by the original game from the title screen, fast-forwarded, and checks
// that the port stays in sync with it. The game sources call these hooks
// inside #ifdef TH16_PORT; port/src/replay_test.cpp has the checks and
// NOTES.md ("Testing") how to run them.
//
// A replay stores, for each stage, the state the original had when the
// stage began (the first 0x228 bytes of g_Globals, the replay RNG's seed,
// the player's position) and how many frames of input the stage took.
// Playback restores that state at every stage, so each stage is checked on
// its own: when the next stage starts, the state the port reached is
// compared with the one the original recorded, before playback overwrites
// it. When the replay ends, the score is compared with the recorded final
// score.
//
// The process exits when the replay ends: 0 if every check passed, 1 if
// one failed, 2 if the replay could not be played to its end (it could not
// be read, the player got a game over, or the game left playback early).
#pragma once

struct ReplayManager;
struct RpyGamestate;

// main: FILE is copied into the save folder's replay/ directory, from where
// the game reads replays. Returns false if it cannot be copied.
bool port_replay_test_init(const char *file, const char *save_dir);

// Whether a replay test is running.
bool port_replay_test_active();

// TitleInf::on_tick in the title menu: returns the replay's file name (in
// replay/) when it is time to start it, once; NULL otherwise. Coming back
// to the title menu after that ends the test as a failure (the replay
// stopped before its end).
const char *port_replay_test_title_tick(int idle_frames);

// ReplayManager::on_tick_fast_forward: whether to fast-forward (the game
// does it while the shot or skip key is held).
bool port_replay_test_fast_forward();

// port_start_test_replay (MainMenu.cpp): the replay was read; lists its
// stages.
void port_replay_test_describe(const ReplayManager *replay);

// ReplayManager::on_tick_playback, after a frame of input was replayed:
// with TH16_REPLAY_TEST_TRACE=N, logs the game state every N frames.
void port_replay_test_frame(const ReplayManager *replay);

// ReplayManager::start_stage during playback, before it restores the
// recorded state of the stage that begins (`recorded`).
void port_replay_test_stage_start(const ReplayManager *replay, const RpyGamestate *recorded);

// open_replay_end_menu: the replay reached its end.
void port_replay_test_replay_end();

// The replay stopped early (a game over, a stage end menu, an unreadable
// file).
void port_replay_test_abort(const char *why);
