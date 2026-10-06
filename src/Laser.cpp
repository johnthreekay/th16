#include <string.h>

#include "Laser.h"

// FUNCTION: TH16 0x411860
LaserInfiniteInner::LaserInfiniteInner()
{
    memset(this, 0, sizeof(LaserInfiniteInner));
    spd1 = 8.0f;
}
