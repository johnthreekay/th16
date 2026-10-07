// Placeholders for wave 5, range E (0x458000-0x4748e0). Compiled without
// /GL, so LTCG treats them as opaque.
#include "../AnmManager.h"

// GLOBAL: TH16 0x491b2c
AnmVmFunc g_anm_on_draw_funcs[7];

// The decompiled body is parked in src/AnmTexels.cpp (see there).
// STUB: TH16 0x46c0d0
void __stdcall AnmManager::convert_texture(IDirect3DTexture9 *texture)
{
}
