// Stand-in callers for unit 7 (title screen, archive reader, window,
// screen effects), shaped like the original call sites.
#include "../Arcfile.h"

// Like Supervisor's startup (0x43b494), the only caller.
bool harness_arcfile_open()
{
    return g_Arcfile.open("th16.dat");
}
