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

    // Frees the DIB section and its device context.
    HARNESS_CALLED bool release_buffer();
    // Creates a DIB section of the given size and format to draw text into;
    // false if the format is not one of g_format_info's.
    HARNESS_CALLED bool try_allocate_buffer(i32 width, i32 height, D3DFORMAT format);
    FormatInfo *get_format_info(D3DFORMAT format);
    // Flips the alpha bits of the rows from y on (GDI clears alpha where it
    // draws). Every caller passes 0 for y.
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

// 0x459240. Renders text into the DIB section with a GDI font (font_id
// picks one of the fonts below), outlined in shadow_color if outline is
// set, and copies the result into dst_rect of the texture. A nonzero
// spacing draws the text two bytes (one Shift-JIS character) at a time,
// spacing pixels apart.
HARNESS_CALLED void __stdcall draw_text(RECT *dst_rect, i32 x, i32 font_height, D3DCOLOR color,
                                         D3DCOLOR shadow_color, const char *text,
                                         IDirect3DTexture9 *texture, i32 font_id, i32 spacing,
                                         i32 outline);

// draw_text's fonts by font id, as create_fonts makes them (size without
// Meiryo, with it): 0 MS Gothic 32 (Meiryo 48), 1 MS Mincho 32 semibold,
// 2 MS Gothic 15 bold (Meiryo 16), 3 MS Mincho 15 bold, 4 MS Gothic 40
// (Meiryo 60), 5 MS Mincho 40 semibold, 6 like 2, 7 like 3, 8 MS Gothic 28
// (Meiryo 42), any other MS Gothic 24 (Meiryo 36).
extern HFONT g_text_font_default;
extern HFONT g_text_font_8;
extern HFONT g_text_font_0;
extern HFONT g_text_font_1;
extern HFONT g_text_font_2;
extern HFONT g_text_font_3;
extern HFONT g_text_font_4;
extern HFONT g_text_font_5;
extern HFONT g_text_font_6;
extern HFONT g_text_font_7;
