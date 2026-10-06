#pragma once

#include "types.h"

// Partial: only what unit 2 (Stage, Bomb) uses so far.
struct SoundManager
{
    // 0x45e150
    void play_sound_centered(i32 id, i32 unused);
    // 0x45e1f0. Pans by the x coordinate.
    void play_sound_at_position(i32 id, f32 x);
};

extern SoundManager g_SoundManager;
