#pragma once

#include <d3dx9math.h>

#include "types.h"

// Partial: only what unit 2 (Stage, Bomb) uses so far. Layout from ExpHP.
struct AsciiManager
{
    u8 unk_0[0x1920c];
    union {
        u32 color;
        u8 color_bytes[4];
    };
    u8 unk_19210[0x19224 - 0x19210];
    i32 font_id;
    i32 group;
    i32 duration;
    i32 align_h;
    i32 align_v;

    // 0x408260. Variadic, so __cdecl with this pushed first.
    void sprintf(D3DXVECTOR3 *pos, const char *fmt, ...);
};

extern AsciiManager *g_AsciiManager;
