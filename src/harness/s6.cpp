// Stand-in callers for the group 6 TODO sweep (lasers and system code).
#include "../Supervisor.h"

// The early arcade offsets belong to the screen block whose other fields
// have their addresses taken (harness_screen_metric_ptr): stores through
// pointers stay ordered with loads of them (LaserCurveInf::on_draw).
i32 *harness_arcade_offset_ptr(i32 which)
{
    return which ? &g_early_arcade_offset_y : &g_early_arcade_offset_x;
}
