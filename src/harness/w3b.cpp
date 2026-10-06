// Stand-in callers for wave 3 range B (0x4190b0-0x42b480), for functions
// whose shape depends on callers that are not decompiled yet.
#include "../AsciiManager.h"

// GameThread's destructor and the menus show and hide the "now loading"
// animation; the original passes the coordinates in xmm1 and xmm2.
void harness_w3b_now_loading(f32 x, f32 y, i32 hide)
{
    if (hide)
    {
        g_AsciiManager->hide_now_loading();
    }
    else
    {
        g_AsciiManager->show_now_loading(x, y);
    }
}
