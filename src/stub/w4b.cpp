// Placeholders for wave 4 interpreter agent 2 (AnmVm::run and friends):
// callees and callback tables that are not decompiled yet. Compiled
// without /GL, so calls stay opaque.
#include "../AnmVm.h"

// GLOBAL: TH16 0x4919e8
AnmVmFunc g_anm_on_tick_funcs[5];

// GLOBAL: TH16 0x491b1c
AnmVmSpriteFunc g_anm_sprite_mapping_funcs[4];

// GLOBAL: TH16 0x4c0f44
AnmVmFunc g_anm_on_wait_funcs[1];

// STUB: TH16 0x4632f0
void AnmVm::update_special_vertices()
{
}

// Opaque use of a local double, for harness callers that must have 8-byte
// aligned frames.
void w4b_opaque_double(double *value)
{
}
