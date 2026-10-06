#pragma once

#include "decomp.h"
#include "types.h"

// Only the parts decompiled code needs so far.

// The BGM stream (an adapted DirectSound sample CStreamingSound). Fields
// from 0x14 on are ZUN's fade state.
struct BgmStream
{
    void *vtable;
    struct IDirectSoundBuffer **buffers;
    u8 unk_8[0x14 - 0x8];
    i32 fade_time_left;
    i32 fade_duration;
    // 1: fade out and stop, 2: fade in, 3/4: like 2/1 but quieter.
    i32 fade_mode;

    void set_volume(i32 volume);
};

enum BgmCommand
{
    BGM_PLAY_WAV = 1,
    BGM_PLAY = 2,
    BGM_STOP = 3,
    BGM_STOP_4 = 4,
    BGM_FADE_OUT = 5,
};

struct SoundManager
{
    u8 unk_0[0x5660];
    BgmStream *bgm_stream;
    u8 unk_5664[0x5698 - 0x5664];

    // Queues a command for the sound thread.
    void modify_bgm(i32 command, i32 arg, const char *name);

    // Members that do not use this; LTCG dropped it.
    static i32 update_sound_thread();
    static void tick_bgm_fade();
    // The second argument is 0 at every call site; LTCG folded it, so
    // callers push whatever register is handy.
    HARNESS_CALLED void play_sound_centered(i32 id, i32 unused);
    // 0x45e1f0. Pans by the x coordinate.
    void play_sound_at_position(i32 id, f32 x);
};

extern SoundManager g_SoundManager;

void play_sound_centered_stub(i32 id, i32 unused);
