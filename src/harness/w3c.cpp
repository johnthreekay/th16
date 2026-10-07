// Escaping addresses of screen globals (see s6.cpp).
//
// Not ZUN's code and never run: it gives link-time code generation calls
// or address uses the decompiled code does not have, so that it compiles
// the real functions as in the original (see docs/workflow.md, "Placeholders and
// stand-in callers"). The harness files are split the way the work was;
// regrouping them changes LTCG's choices for unrelated functions.
#include "../EnemyManager.h"
#include "../Gui.h"
#include "../HelpManual.h"
#include "../Player.h"
#include "../Supervisor.h"

// Takes an address and does nothing with it, out of LTCG's view
// (src/stub/Opaque.cpp).
void w3c_stub_sink(void *p);

// Supervisor::setup_cameras (0x43cb10) reloads the screen size after every
// store.
void harness_w3c_expose_screen_globals()
{
    w3c_stub_sink(&g_resolution_x);
    w3c_stub_sink(&g_resolution_y);
    w3c_stub_sink(&g_window_flags);
}
