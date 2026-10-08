#!/bin/sh
# Replay recording test (NOTES.md, "Testing"): plays a game in the port
# with the input of SOURCE (a replay the original game recorded) while the
# game records it, then checks the replay the port saved: against SOURCE
# (tools/rpy_compare.py), and by playing it back with the replay sync test.
#
#   run_record_test.sh TH16_BINARY SOURCE WORK_DIR
#
# The game folder is $TH16_DATA_DIR; without it the test is skipped (exit
# code 77). Otherwise: 0 if the recording matches SOURCE and plays back in
# sync, 1 if not, 2 if the game or the playback did not get to the end. The
# port's replay is WORK_DIR/<source>_recorded.rpy, the logs
# WORK_DIR/<source>.record.log and WORK_DIR/<source>_recorded.log.
set -u
th16=$1
source=$2
work=$3
here=$(dirname "$0")
if [ -z "${TH16_DATA_DIR:-}" ] || [ ! -f "$TH16_DATA_DIR/th16.dat" ]; then
    echo "TH16_DATA_DIR is not set to a folder with th16.dat; skipping"
    exit 77
fi
name=$(basename "$source" .rpy)
save="$work/$name.record"
recorded="$work/${name}_recorded.rpy"
rm -rf "$save" "$recorded"
mkdir -p "$save"
log="$work/$name.record.log"
SDL_VIDEODRIVER=${SDL_VIDEODRIVER:-offscreen} SDL_AUDIODRIVER=dummy SDL_JOYSTICK_HIDAPI=0 TH16_NO_DIALOGS=1 \
    "$th16" --save-dir "$save" --record-from "$source" --record-to recorded.rpy --game-dir "$TH16_DATA_DIR" \
    > "$log" 2>&1
code=$?
grep '^\[record-test\]' "$log"
if [ ! -f "$save/replay/recorded.rpy" ]; then
    echo "no replay was saved (exit code $code)"
    exit 2
fi
cp "$save/replay/recorded.rpy" "$recorded"
echo "comparing with the source:"
python3 "$here/../tools/rpy_compare.py" compare "$source" "$recorded"
compare=$?
echo "playing back:"
"$here/run_replay_test.sh" "$th16" "$recorded" "$work"
playback=$?
if [ $playback -eq 2 ]; then
    exit 2
fi
if [ $code -ne 0 ] || [ $compare -ne 0 ] || [ $playback -ne 0 ]; then
    exit 1
fi
exit 0
