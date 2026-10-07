// Placeholders for functions wave 3, range C (0x42b480-0x43dc30) calls but
// that are not decompiled yet. Compiled without /GL, so they stay opaque
// calls with standard conventions.
#include "../Scorefile.h"

// Opaque work for the stand-ins in src/harness/w3c.cpp.
void w3c_stub_sink(void *p)
{
}

// STUB: TH16 0x46b900
void anm_manager_46b900()
{
}

#include "../StageData.h"

// GLOBAL: TH16 0x4a22d0
StageData g_stage_table[8];

#include "../Supervisor.h"

Supervisor *w3c_stub_supervisor()
{
    return &g_Supervisor;
}
