// Replay recording test (port only): `th16 --record-from SOURCE --record-to
// NAME` plays a normal game (not a replay) with the input of SOURCE, a
// replay recorded by the original game, while the game records it as
// usual, and saves the recording as NAME in the save folder's replay/
// directory. The game sources call these hooks inside #ifdef TH16_PORT;
// port/src/record_test.cpp has them and NOTES.md ("Testing") how to run
// the whole test (tests/run_record_test.sh), which then compares NAME with
// SOURCE (tools/rpy_compare.py) and plays NAME back with the replay sync
// test.
//
// The game starts from the title menu with SOURCE's character, season and
// difficulty, the way the menus start one. When the recording begins the
// replay RNG gets SOURCE's seed and the game thread SOURCE's settings, as
// playback does at the same point (ReplayManager::initialize). Each frame
// the recorder takes SOURCE's input for the same stage and frame, in place
// of the keyboard's; it runs fast-forwarded like replay playback. When the
// game goes on to the ending (or, after the extra stage, to the score
// entry), the replay is saved as the replay save menu would save it, and
// the process exits: 0 when saved, 2 if the game ended otherwise (a game
// over, the stage end menu, back to the title).
#pragma once

struct ReplayManager;

// main: SOURCE is copied into the save folder's replay/ directory, from
// where the game reads replays. Returns false if it cannot be copied.
bool port_record_test_init(const char *source, const char *name, const char *save_dir);

// Whether a recording test is running.
bool port_record_test_active();

// TitleInf::on_tick in the title menu: true when it is time to start the
// game, once (port_start_record_test in MainMenu.cpp then starts it as the
// menus would). Coming back to the title menu after that ends the test as
// a failure.
bool port_record_test_title_tick(int idle_frames);

// SOURCE, read from replay/ the first time (as the replay menu reads it).
// Ends the test if it cannot be read.
const ReplayManager *port_record_test_source();

// ReplayManager::initialize, recording, before it reads the RNG seed and
// the settings: gives them SOURCE's values.
void port_record_test_initialize(ReplayManager *replay);

// ReplayManager::on_tick_record: SOURCE's input for the frame the recorder
// is about to write (the stage and frame it is at), in place of the
// keyboard's.
unsigned short port_record_test_input(const ReplayManager *replay);

// ReplayManager::on_tick_fast_forward while recording: whether to
// fast-forward.
bool port_record_test_fast_forward();

// ReplayManager::begin_stage, recording, once the stage's snapshot is
// complete: compares it with SOURCE's for the same stage and logs the
// differences (the final check is tools/rpy_compare.py on the saved file).
void port_record_test_stage_begin(const ReplayManager *replay);

// GameThread's destructor (the game ends or goes on to the next stage):
// saves the replay and exits if the game was cleared, or exits with 2 if
// it ended otherwise.
void port_record_test_game_thread_end(int next_gamemode);

// The game stopped early (a game over, the stage end menu).
void port_record_test_abort(const char *why);
