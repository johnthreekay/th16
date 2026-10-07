// Placeholders for functions wave 3, range C (0x42b480-0x43dc30) calls but
// that are not decompiled yet. Compiled without /GL, so they stay opaque
// calls with standard conventions.
#include "../Scorefile.h"

// Opaque work for the stand-ins in src/harness/w3c.cpp.
void w3c_stub_sink(void *p)
{
}

// Opaque work for the stand-ins in src/placeholder/w3c.cpp.
int w3c_placeholder_sink(void *a, const void *b)
{
    return 0;
}

// STUB: TH16 0x46b900
void anm_manager_46b900()
{
}

// STUB: TH16 0x458db0
void supervisor_458db0()
{
}

// STUB: TH16 0x4497e0
Scorefile::Scorefile()
{
}

// STUB: TH16 0x449a00
void scorefile_save_449a00()
{
}

#include "../TitleInf.h"

// GLOBAL: TH16 0x4a6f20
TitleInf *g_TitleInf;

// STUB: TH16 0x44ad20
TitleInf::~TitleInf()
{
}

u32 TitleInf::get_size()
{
    return sizeof(TitleInf);
}

// STUB: TH16 0x458520
bool supervisor_458520()
{
    return false;
}

#include <windows.h>

// Fonts 0x458db0 creates (only 1 is never deleted).
// GLOBAL: TH16 0x4df904
HFONT g_fonts_4df904[10];
