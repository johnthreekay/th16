// Stand-in callers and readers for unit 6 (Supervisor, Player, menus,
// replays). Link-time code generation drops stores to globals nothing reads,
// so globals whose readers are not decompiled yet are read here.
#include "../Supervisor.h"

int harness_unit6_read_globals()
{
    return g_unk_4a6ef0 + g_arcade_width + g_arcade_height;
}
