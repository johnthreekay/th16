// Definitions that link-time code generation must not see into. build.py
// compiles src/stub/ without /GL, so LTCG treats everything here as
// external: it cannot read a global's value or inline a function's body.
// Not ZUN's code apart from the one global.
#include <d3dx9math.h>

// A zero vector that nothing writes (Interp.h). The original loads it at
// every use; with the definition in view LTCG would see that it is never
// written and fold the loads into constant zeros (in the ECL radial and
// ellipse interpolations).
// GLOBAL: TH16 0x4c10c8
D3DXVECTOR2 g_zero_vec2;

// Takes the address of a value without doing anything with it. The harness
// stand-ins in src/harness/ use it to make LTCG assume that a global's
// address escapes (harness_w3c_expose_screen_globals).
void w3c_stub_sink(void *p)
{
}

// Takes the address of a local double, which makes a caller's frame 8-byte
// aligned (harness_gui_spell_vms, harness_w5e_transformed_pos).
void w4b_opaque_double(double *value)
{
}
