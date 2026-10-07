#pragma once

#include <windows.h>

#include <d3d9.h>

#include "Thread.h"
#include "decomp.h"
#include "types.h"

// TH06's TextHelper: a GDI DIB section that text is drawn into before it is
// copied to a texture. TH16 has a single global one, so LTCG folded `this`
// (and the constant buffer size) out of every member.
struct FormatInfo
{
    D3DFORMAT format;
    i32 bit_count;
    u32 alpha_mask;
    u32 red_mask;
    u32 green_mask;
    u32 blue_mask;
};

struct TextHelper
{
    D3DFORMAT format;
    i32 width;
    i32 height;
    u32 image_size;
    i32 pitch;
    HDC hdc;
    HGDIOBJ old_bitmap;
    HGDIOBJ bitmap;
    void *buffer;

    TextHelper()
    {
        format = (D3DFORMAT)-1;
        width = 0;
        height = 0;
        hdc = NULL;
        bitmap = NULL;
        old_bitmap = NULL;
        buffer = NULL;
    }
    ~TextHelper()
    {
        release_buffer();
    }

    HARNESS_CALLED bool release_buffer();
    HARNESS_CALLED bool try_allocate_buffer(i32 width, i32 height, D3DFORMAT format);
    FormatInfo *get_format_info(D3DFORMAT format);
    // Every caller passes 0 for y.
    HARNESS_CALLED bool invert_alpha(i32 rows, i32 y);
    // Spreads alpha into transparent A4R4G4B4 pixels next to opaque ones.
    HARNESS_CALLED bool blur_alpha(i32 rows);
    // Gives transparent pixels the average color of their opaque
    // neighbours, so filtering does not darken the glyph edges.
    HARNESS_CALLED bool bleed_color(i32 rows);
};

extern TextHelper g_TextHelper;

// A ThreadInf subclass (RTTI name D3DThreadInf). The only instance is a
// global nothing uses besides its static constructor and destructor.
class D3DThreadInf : public ThreadInf
{
  public:
    u8 unk_1c[0x50 - 0x1c];
};

// Creates the GDI fonts draw_text uses and fills g_text_random_bytes.
void create_fonts();

HARNESS_CALLED void __stdcall draw_text(RECT *dst_rect, i32 x, i32 font_height, D3DCOLOR color,
                                         D3DCOLOR shadow_color, const char *text,
                                         IDirect3DTexture9 *texture, i32 font_id, i32 spacing,
                                         i32 outline);

extern HFONT g_font_904;
extern HFONT g_font_908;
extern HFONT g_font_90c;
extern HFONT g_font_910;
extern HFONT g_font_914;
extern HFONT g_font_918;
extern HFONT g_font_91c;
extern HFONT g_font_920;
extern HFONT g_font_924;
extern HFONT g_font_928;
