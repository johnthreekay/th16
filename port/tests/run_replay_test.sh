#!/bin/sh
# Replay sync test (NOTES.md, "Testing"): plays REPLAY in the port, headless
# and fast-forwarded, and checks it against the state the original game
# recorded in it (port/include/port_replay_test.h).
#
#   run_replay_test.sh TH16_BINARY REPLAY WORK_DIR
#
# The game folder is $TH16_DATA_DIR. Without it (or without th16.dat in it)
# the test is skipped: exit code 77, which CMake's SKIP_RETURN_CODE maps to
# "skipped". Otherwise the exit code is th16's: 0 in sync, 1 desync, 2 the
# replay did not play to its end. The log is WORK_DIR/<replay>.log.
set -u
th16=$1
replay=$2
work=$3
if [ -z "${TH16_DATA_DIR:-}" ] || [ ! -f "$TH16_DATA_DIR/th16.dat" ]; then
    echo "TH16_DATA_DIR is not set to a folder with th16.dat; skipping"
    exit 77
fi
name=$(basename "$replay" .rpy)
save="$work/$name"
rm -rf "$save"
mkdir -p "$save"
log="$work/$name.log"
SDL_VIDEODRIVER=${SDL_VIDEODRIVER:-offscreen} SDL_AUDIODRIVER=dummy SDL_JOYSTICK_HIDAPI=0 TH16_NO_DIALOGS=1 \
    "$th16" --save-dir "$save" --replay "$replay" --game-dir "$TH16_DATA_DIR" > "$log" 2>&1
code=$?
grep '^\[replay-test\]' "$log"
exit $code
