// The screen geometry globals (g_resolution_x and the rest from 0x4d9d1c on,
// GameWindow.h) are fields of g_GameWindow in the original, whose address
// the window methods take and pass around (SystemParametersInfo at 0x45a70d,
// for one). LTCG must therefore assume that a store through any pointer may
// change them, and the original reloads them after such stores. Ours are
// separate globals, so their addresses escape here and in unit1.cpp,
// w3a.cpp and w3c.cpp instead.
//
// Not ZUN's code and never run: it gives link-time code generation calls
// or address uses the decompiled code does not have, so that it compiles
// the real functions as in the original (see README.md, "Placeholders and
// stand-in callers"). The harness files are split the way the work was;
// regrouping them changes LTCG's choices for unrelated functions.
#include "../Supervisor.h"

// LaserCurveInf::on_draw (0x438750) keeps the arcade offsets' loads in order
// with its stores through pointers.
i32 *harness_arcade_offset_ptr(i32 which)
{
    return which ? &g_early_arcade_offset_y : &g_early_arcade_offset_x;
}
