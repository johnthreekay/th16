// Placeholders that, unlike those in src/stub/, are compiled with /GL: LTCG
// sees their bodies, which matters to callers whose code depends on what the
// callee does (register conventions LTCG picks, registers it clobbers,
// whether a pointer argument is only read). The real functions are outside
// unit 3 (0x411860-0x41a3f0); replace these once they are decompiled.
#include <math.h>
#include <stdarg.h>
#include <stdio.h>

#include "../AnmVm.h"
#include "../AsciiManager.h"
#include "../ZunMath.h"

// STUB: TH16 0x4033f0
// Kept out of line so that LTCG gives it the original's register
// convention: method in ecx, t in xmm1, end_time in xmm2. The original has
// some thirty curves.
HARNESS_CALLED f32 interp_common_methods(i32 method, f32 t, f32 end_time)
{
    if (end_time == 0.0f)
    {
        return 1.0f;
    }
    t /= end_time;
    switch (method)
    {
    case 1:
        return t * t;
    case 2:
        return t * t * t;
    case 4:
        return 1.0f - (1.0f - t) * (1.0f - t);
    case 9:
        // Some of the real curves call into the CRT, which matters to
        // the callers' register allocation.
        return sinf(t * ZUN_PI / 2);
    }
    return t;
}

// STUB: TH16 0x408260
// Only reads *pos: with that in view LTCG drops the /GS cookie that callers
// passing the address of a local position would otherwise get. Like the
// real one it formats the string with the CRT, which clobbers registers and
// may write any global.
void AsciiManager::add_formatted_string(const D3DXVECTOR3 *pos, const char *fmt, ...)
{
    char buf[0x100];
    va_list args;
    va_start(args, fmt);
    vsprintf(buf, fmt, args);
    va_end(args);
    if (pos->x < 0.0f)
    {
        font_id = buf[0];
    }
}
