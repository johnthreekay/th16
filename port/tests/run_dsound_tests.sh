#!/bin/sh
# Runs the DirectSound mixer tests of one build:
#
#   port/tests/run_dsound_tests.sh <build dir> [<game dir>]
#
# <game dir> holds th16.dat and thbgm.dat (only read). With it, the sound
# effects and thbgm.fmt are extracted into <build dir>/dsound-data and the
# BGM stream tests run too (the second one through the game's own
# DSUtil.cpp). Audio never reaches a real device: the manual
# tests use SDL's dummy driver and the real-time test writes the mix to
# <build dir>/dsound-realtime.raw through the disk driver.
set -eu

build=$1
game=${2:-}
test_bin=$build/th16_dsound_test
here=$(dirname "$0")

if [ -z "$game" ]; then
    SDL_AUDIODRIVER=dummy SDL_AUDIO_DRIVER=dummy "$test_bin"
    exit
fi

data=$build/dsound-data
if [ ! -f "$data/thbgm.fmt" ]; then
    python3 -I "$here/th16dat.py" "$game/th16.dat" "$data" thbgm.fmt se_plst00.wav se_ok00.wav \
        se_select00.wav se_power1.wav se_item00.wav se_pause.wav se_extend.wav se_gun00.wav
fi

SDL_AUDIODRIVER=dummy SDL_AUDIO_DRIVER=dummy "$test_bin" --data "$data" --bgm "$game/thbgm.dat"
SDL_AUDIODRIVER=dummy SDL_AUDIO_DRIVER=dummy "$build/th16_dsound_game_test" "$data" "$game/thbgm.dat"
SDL_AUDIODRIVER=disk SDL_AUDIO_DRIVER=disk SDL_DISKAUDIOFILE="$build/dsound-realtime.raw" \
    "$test_bin" --data "$data" --realtime
