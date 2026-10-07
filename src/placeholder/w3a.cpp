// Stand-ins for other ranges' functions that wave 3, range A's callers
// need LTCG to see (folded arguments, dropped this). Compiled with /GL and
// not forced alive.
#include "../AnmManager.h"
#include "../Fog.h"

void placeholder_sink(int a, float b);

// STUB: TH16 0x469890
HARNESS_CALLED void AnmManager::draw_triangle_fan(i32 count, Float3 *center, Float2 *offsets, ZunColor *colors)
{
    placeholder_sink(count + colors[0].d3d + g_AnmManager->unk_c0, center->x + offsets[0].x);
}
