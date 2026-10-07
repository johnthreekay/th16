// Stand-ins compiled with /GL for functions that wave 5 range B
// (0x420000-0x440000) calls but are not decompiled, where LTCG has to see a
// body (custom conventions). Each forwards to an opaque stub.
#include <math.h>

#include "../Laser.h"
#include "../Player.h"

int w5b_placeholder_sink(void *object, int value);

// STUB: TH16 0x404220
HARNESS_CALLED i32 __stdcall line_intersection(f32 *out_x, f32 *out_y, f32 x1, f32 y1, f32 angle1, f32 x2, f32 y2,
                                               f32 angle2)
{
    *out_x = x1 + x2 * angle1;
    *out_y = y1 + y2 * angle2;
    return w5b_placeholder_sink(out_x, (i32)*out_y);
}
