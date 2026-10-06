#pragma once

#include <d3dx9math.h>

#include "AnmManager.h"
#include "AnmVm.h"
#include "types.h"

// Draws text with the ascii.anm fonts. Layout from ExpHP (zAsciiManager);
// only the fields unit 3 needs.
struct AsciiManager
{
    u8 unk_0[0x1920c];
    // Applies to strings added from now on.
    ZunColor color;
    u8 unk_19210[0x19224 - 0x19210];
    u32 font_id;
    u32 group;
    u8 unk_1922c[0x19240 - 0x1922c];
    AnmLoaded *ascii_anm;
    AnmId unk_anm_id;
    AnmId now_loading_anm_id;
    u8 unk_1924c[0x19254 - 0x1924c];

    // TH06 equivalent: AsciiManager::AddFormatText. Variadic member
    // functions are __cdecl with this as the first stack argument.
    void add_formatted_string(const D3DXVECTOR3 *pos, const char *fmt, ...);
};

extern AsciiManager *g_AsciiManager;
