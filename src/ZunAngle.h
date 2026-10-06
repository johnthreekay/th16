#pragma once

#include "decomp.h"
#include "types.h"

// A float angle kept in [-pi, pi]. The name is ours; the methods are
// inferred from a run of small functions at 0x4052e0-0x4054cc.
struct ZunAngle
{
    f32 value;

    ZunAngle()
    {
    }
    HARNESS_CALLED ZunAngle(f32 value);

    HARNESS_CALLED ZunAngle &operator=(f32 value);
    HARNESS_CALLED ZunAngle &operator+=(f32 delta);
    HARNESS_CALLED ZunAngle operator+(f32 delta) const;
    // Shortest signed difference.
    HARNESS_CALLED ZunAngle operator-(const ZunAngle &other) const;
};
