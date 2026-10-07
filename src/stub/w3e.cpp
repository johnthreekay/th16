// Opaque stubs for wave 3 range E (0x44f710-0x4630f0): bodies LTCG must not
// see, behind the placeholders in src/placeholder/w3e.cpp, and callees from
// other ranges.
#include "../Spellcard.h"
#include "../Supervisor.h"

int w3e_placeholder_sink(void *object, int value)
{
    return 0;
}

// STUB: TH16 0x417bc0
void Spellcard::sub_417bc0()
{
}

// STUB: TH16 0x43bbd0
void __stdcall Supervisor::save_screenshot(const char *path)
{
}

// STUB: TH16 0x43dcc0
void Supervisor::release_dinput()
{
}

// STUB: TH16 0x45ba80
void Supervisor::reset_render_state()
{
}

void w3e_opaque_double(double *value)
{
}
