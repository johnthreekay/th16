#pragma once

#include "decomp.h"
#include "types.h"

#define ZUN_PI ((f32)(3.14159265358979323846))
#define ZUN_2PI ((f32)(ZUN_PI * 2.0f))

// Wrap an angle into [-pi, pi], giving up after 32 turns either way.
// TH06 equivalent: utils::AddNormalizeAngle
f32 LTCG_VECTORCALL add_normalize_angle(f32 a, f32 b);
f32 LTCG_VECTORCALL normalize_angle(f32 a);
