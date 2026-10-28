#pragma once
#include "types.h"

// Corner offsets of a sprite in units of its size, by anchoring (center,
// left/top, right/bottom), for the corners in g_sprite_temp_buffer order.
struct AnmAnchorCorners
{
    f32 corner[4];
};
