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
    // User-declared, which makes copies of structs holding a ZunAngle
    // (PosVel) memberwise, as in the original, instead of block copies.
    ZunAngle &operator=(const ZunAngle &other)
    {
        value = other.value;
        return *this;
    }
    HARNESS_CALLED ZunAngle &operator+=(f32 delta);
    HARNESS_CALLED ZunAngle operator+(f32 delta) const;
    // Shortest signed difference.
    HARNESS_CALLED ZunAngle operator-(const ZunAngle &other) const;
    // 0x4475f0 and 0x447650, used by InterpAngle::step.
    HARNESS_CALLED ZunAngle operator*(f32 factor) const;
    HARNESS_CALLED ZunAngle operator+(const ZunAngle &other) const;
};
