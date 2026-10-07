// The DirectSound mixer's own entry points (dsound_sdl.cpp), besides the
// DirectSound API itself. The game never calls these; the SDL audio
// callback and the tests in port/tests/ do.
#pragma once

#include <stdint.h>

// The mixer's output: interleaved signed 16-bit stereo at this rate, the
// primary buffer format the game asks for (SoundManager::initialize).
#define PORT_DSOUND_RATE 44100

// Mixes the next `frames` frames of every playing secondary buffer into out
// (2 int16_t per frame), advances their play cursors and signals the
// position notifications they pass. The SDL audio callback calls this; the
// tests call it directly in manual mode.
void port_dsound_mix(int16_t *out, uint32_t frames);

// Manual mode, set before DirectSoundCreate8: no SDL device is opened and
// buffers only move when port_dsound_mix is called.
void port_dsound_set_manual(bool manual);
