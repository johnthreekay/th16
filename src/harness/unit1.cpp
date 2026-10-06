// Stand-in callers for unit 1 (0x402e70-0x409490).
#include "../ZunMath.h"

// ECL and the per-object updates write g_GameSpeed all the time. Without a
// write LTCG would treat it as the constant 1.0.
void harness_set_game_speed(f32 speed)
{
    g_GameSpeed = speed;
}
