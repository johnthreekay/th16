#include <string.h>

#include <d3dx9.h>

#include "Rng.h"
#include "TextHelper.h"

// GLOBAL: TH16 0x4a2980
FormatInfo g_format_info[7] = {
    {D3DFMT_X8R8G8B8, 32, 0x00000000, 0x00ff0000, 0x0000ff00, 0x000000ff},
    {D3DFMT_A8R8G8B8, 32, 0xff000000, 0x00ff0000, 0x0000ff00, 0x000000ff},
    {D3DFMT_X1R5G5B5, 16, 0x00000000, 0x00007c00, 0x000003e0, 0x0000001f},
    {D3DFMT_R5G6B5, 16, 0x00000000, 0x0000f800, 0x000007e0, 0x0000001f},
    {D3DFMT_A1R5G5B5, 16, 0x00008000, 0x00007c00, 0x000003e0, 0x0000001f},
    {D3DFMT_A4R4G4B4, 16, 0x0000f000, 0x00000f00, 0x000000f0, 0x0000000f},
    {(D3DFORMAT)-1, 0, 0, 0, 0, 0},
};

// GLOBAL: TH16 0x4a5c10
D3DThreadInf g_D3DThreadInf;
// SYNTHETIC: TH16 0x401140
// ??__Eg_D3DThreadInf@@YAXXZ
// SYNTHETIC: TH16 0x48ac80
// ??__Fg_D3DThreadInf@@YAXXZ

// SYNTHETIC: TH16 0x4584f0
// D3DThreadInf::`scalar deleting destructor'

// Filled with random values by create_fonts.
// GLOBAL: TH16 0x4a5c60
u8 g_text_random_bytes[256];

// GLOBAL: TH16 0x4a5d60
TextHelper g_TextHelper;
// SYNTHETIC: TH16 0x401150
// ??__Eg_TextHelper@@YAXXZ
// SYNTHETIC: TH16 0x48aca0
// ??__Fg_TextHelper@@YAXXZ

// GLOBAL: TH16 0x4a5d90
ThreadInf g_thread_4a5d90;
// SYNTHETIC: TH16 0x401160
// ??__Eg_thread_4a5d90@@YAXXZ
// SYNTHETIC: TH16 0x48acb0
// ??__Fg_thread_4a5d90@@YAXXZ

// Set when the system has Meiryo; the fonts then use it instead of MS Gothic.
// GLOBAL: TH16 0x4a6f2c
i32 g_meiryo_available;

// GLOBAL: TH16 0x4df904
HFONT g_font_904;
// GLOBAL: TH16 0x4df908
HFONT g_font_908;
// GLOBAL: TH16 0x4df90c
HFONT g_font_90c;
// GLOBAL: TH16 0x4df910
HFONT g_font_910;
// GLOBAL: TH16 0x4df914
HFONT g_font_914;
// GLOBAL: TH16 0x4df918
HFONT g_font_918;
// GLOBAL: TH16 0x4df91c
HFONT g_font_91c;
// GLOBAL: TH16 0x4df920
HFONT g_font_920;
// GLOBAL: TH16 0x4df924
HFONT g_font_924;
// GLOBAL: TH16 0x4df928
HFONT g_font_928;

// FUNCTION: TH16 0x458520
HARNESS_CALLED bool TextHelper::release_buffer()
{
    if (hdc)
    {
        SelectObject(hdc, old_bitmap);
        DeleteDC(hdc);
        DeleteObject(bitmap);
        format = (D3DFORMAT)-1;
        width = 0;
        height = 0;
        hdc = NULL;
        bitmap = NULL;
        old_bitmap = NULL;
        buffer = NULL;
        return true;
    }
    else
    {
        return false;
    }
}

FormatInfo *TextHelper::get_format_info(D3DFORMAT format)
{
    i32 i;
    for (i = 0; g_format_info[i].format != -1 && g_format_info[i].format != format; i++)
    {
    }
    if (format == -1)
    {
        return NULL;
    }
    return &g_format_info[i];
}

// FUNCTION: TH16 0x4585a0
HARNESS_CALLED bool TextHelper::try_allocate_buffer(i32 width, i32 height, D3DFORMAT format)
{
    HGDIOBJ old;
    void *bits;
    HBITMAP bmp;
    FormatInfo *info;
    BITMAPV4HEADER header;
    HDC dc;
    i32 pitch;

    release_buffer();
    memset(&header, 0, sizeof(BITMAPV4HEADER));
    info = get_format_info(format);
    if (info == NULL)
    {
        return false;
    }
    pitch = ((width / 8 * info->bit_count + 3) / 4) * 4;
    header.bV4Size = sizeof(BITMAPV4HEADER);
    header.bV4Width = width;
    header.bV4Height = -(height + 1);
    header.bV4Planes = 1;
    header.bV4BitCount = info->bit_count;
    header.bV4SizeImage = height * pitch;
    if (format != D3DFMT_X1R5G5B5 && format != D3DFMT_X8R8G8B8)
    {
        header.bV4V4Compression = 3;
        header.bV4RedMask = info->red_mask;
        header.bV4GreenMask = info->green_mask;
        header.bV4BlueMask = info->blue_mask;
        header.bV4AlphaMask = info->alpha_mask;
    }
    bmp = CreateDIBSection(NULL, (BITMAPINFO *)&header, 0, &bits, NULL, 0);
    if (bmp == NULL)
    {
        return false;
    }
    memset(bits, 0, header.bV4SizeImage);
    dc = CreateCompatibleDC(NULL);
    old = SelectObject(dc, bmp);
    this->hdc = dc;
    this->bitmap = bmp;
    this->buffer = bits;
    this->image_size = header.bV4SizeImage;
    this->old_bitmap = old;
    this->width = width;
    this->height = height;
    this->format = format;
    this->pitch = pitch;
    return true;
}

struct A1R5G5B5
{
    u16 blue : 5;
    u16 green : 5;
    u16 red : 5;
    u16 alpha : 1;
};

struct A4R4G4B4
{
    u16 blue : 4;
    u16 green : 4;
    u16 red : 4;
    u16 alpha : 4;
};

// TODO: the original never uses ebx and keeps r, g, b, n in stack slots; ours allocates ebx.
// FUNCTION: TH16 0x458730
HARNESS_CALLED bool TextHelper::bleed_color(i32 rows)
{
    u32 y;
    u32 x;
    u32 r;
    u32 g;
    u32 b;
    u32 n;
    switch (g_TextHelper.format)
    {
    case D3DFMT_A4R4G4B4: {
        A4R4G4B4 *p = (A4R4G4B4 *)g_TextHelper.buffer;
        for (y = 0; y < (u32)rows; y++)
        {
            for (x = 0; x < (u32)g_TextHelper.width; x++, p++)
            {
                if (*(u16 *)p >= 0x1000)
                {
                    continue;
                }
                r = 0;
                g = 0;
                b = 0;
                n = 0;
                if (x > 0 && *(u16 *)&p[-1] >= 0x1000)
                {
                    r += p[-1].red;
                    g += p[-1].green;
                    b += p[-1].blue;
                    n++;
                }
                if (x < (u32)g_TextHelper.width - 1 && *(u16 *)&p[1] >= 0x1000)
                {
                    r += p[1].red;
                    g += p[1].green;
                    b += p[1].blue;
                    n++;
                }
                if (y > 0)
                {
                    A4R4G4B4 *q = p - g_TextHelper.pitch / 2;
                    if (*(u16 *)q >= 0x1000)
                    {
                        r += q->red;
                        g += q->green;
                        b += q->blue;
                        n++;
                    }
                }
                if (y < (u32)g_TextHelper.height - 1)
                {
                    A4R4G4B4 *q = p + g_TextHelper.pitch / 2;
                    if (*(u16 *)q >= 0x1000)
                    {
                        r += q->red;
                        g += q->green;
                        b += q->blue;
                        n++;
                    }
                }
                if (n > 1)
                {
                    r /= n;
                    g /= n;
                    b /= n;
                }
                p->red = r / 2;
                p->green = g / 2;
                p->blue = b / 2;
            }
        }
        break;
    }
    case D3DFMT_A8R8G8B8: {
        u8 *p = (u8 *)g_TextHelper.buffer;
        for (y = 0; y < (u32)rows; y++)
        {
            for (x = 0; x < (u32)g_TextHelper.width; x++, p += 4)
            {
                if (p[3] != 0)
                {
                    continue;
                }
                r = 0;
                g = 0;
                b = 0;
                n = 0;
                if (x > 0 && p[-1] != 0)
                {
                    r += p[-2];
                    g += p[-3];
                    b += p[-4];
                    n++;
                }
                if (x < (u32)g_TextHelper.width - 1 && p[7] != 0)
                {
                    r += p[6];
                    g += p[5];
                    b += p[4];
                    n++;
                }
                if (y > 0)
                {
                    u8 *q = p - g_TextHelper.pitch / 4 * 4;
                    if (q[3] != 0)
                    {
                        r += q[2];
                        g += q[1];
                        b += q[0];
                        n++;
                    }
                }
                if (y < (u32)g_TextHelper.height - 1)
                {
                    u8 *q = p + g_TextHelper.pitch / 4 * 4;
                    if (q[3] != 0)
                    {
                        r += q[2];
                        g += q[1];
                        b += q[0];
                        n++;
                    }
                }
                if (n > 1)
                {
                    r /= n;
                    g /= n;
                    b /= n;
                }
                p[2] = r;
                p[1] = g;
                p[0] = b;
            }
        }
        break;
    }
    }
    return true;
}

// TODO: the original indexes src with the precomputed -w and keeps ebx free; ours uses ebx.
// FUNCTION: TH16 0x458af0
HARNESS_CALLED bool TextHelper::blur_alpha(i32 rows)
{
    u16 *dst = (u16 *)g_TextHelper.buffer;
    if (g_TextHelper.format != D3DFMT_A4R4G4B4)
    {
        return true;
    }
    u16 *copy = (u16 *)malloc(g_TextHelper.width * rows * 2 + 1);
    memcpy(copy, dst, g_TextHelper.width * rows * 2);
    i32 w = g_TextHelper.width;
    i32 up = -w;
    i32 up_right = 1 - w;
    i32 up_left = -w - 1;
    i32 down_right = w + 1;
    i32 down_left = w - 1;
    u16 *src = copy + w;
    dst += w;
    for (i32 i = 0; i < (rows - 2) * g_TextHelper.width; i++, src++, dst++)
    {
        if (*src >= 0x1000)
        {
            continue;
        }
        if (i % g_TextHelper.width == 0)
        {
            continue;
        }
        u32 a = (src[up] >> 12) + (src[w] >> 12) + (src[-1] >> 12) + (src[1] >> 12);
        a = a * 2 + (src[down_left] >> 12) + (src[down_right] >> 12) + (src[up_left] >> 12) + (src[up_right] >> 12);
        *dst = (*dst & 0xfff) | ((a / 14) << 12);
    }
    if (copy != NULL)
    {
        free(copy);
    }
    return true;
}

// FUNCTION: TH16 0x458c80
HARNESS_CALLED bool TextHelper::invert_alpha(i32 rows, i32 y)
{
    u8 *p = (u8 *)g_TextHelper.buffer + y * g_TextHelper.pitch;
    i32 end = g_TextHelper.pitch * rows;
    i32 i;
    switch (g_TextHelper.format)
    {
    case D3DFMT_A8R8G8B8:
        for (i = 3; i < end; i += 4)
        {
            p[i] = p[i] ^ 0xff;
        }
        break;
    case D3DFMT_A1R5G5B5: {
        A1R5G5B5 *c;
        for (c = (A1R5G5B5 *)p, i = 0; i < end; i += 2, c++)
        {
            c->alpha ^= 1;
            if (!c->alpha)
            {
                c->red = 0;
                c->green = 0;
                c->blue = 0;
            }
        }
        break;
    }
    case D3DFMT_A4R4G4B4:
        for (i = 1; i < end; i += 2)
        {
            p[i] = p[i] ^ 0xf0;
            u8 a = p[i] & 0xf0;
            if (a != 0 && a != 0xf0)
            {
                p[i] |= 0xf0;
            }
        }
        break;
    default:
        return false;
    }
    return true;
}

// FUNCTION: TH16 0x458d60
static int CALLBACK find_meiryo(const LOGFONTA *font, const TEXTMETRICA *metric, DWORD type, LPARAM param)
{
    if (strcmp((const char *)((ENUMLOGFONTEXA *)font)->elfFullName, "\x83\x81\x83" "C\x83\x8a\x83I") == 0)
    {
        g_meiryo_available = 1;
        return 0;
    }
    return 1;
}

#define GOTHIC "\x82l\x82r \x83S\x83V\x83" "b\x83N"
#define MINCHO "\x82l\x82r \x96\xbe\x92\xa9"
#define MEIRYO "\x83\x81\x83" "C\x83\x8a\x83I"

// FUNCTION: TH16 0x458db0
void create_fonts()
{
    HDC dc = GetDC(NULL);
    LOGFONTA font = {0, 0, 0, 0, 0, 0, 0, 0, DEFAULT_CHARSET, 0, 0, 0, 0, MEIRYO};
    EnumFontFamiliesExA(dc, &font, find_meiryo, 0, 0);
    ReleaseDC(NULL, dc);
    if (!g_TextHelper.try_allocate_buffer(1024, 128, D3DFMT_A4R4G4B4))
    {
        g_TextHelper.try_allocate_buffer(1024, 128, D3DFMT_A8R8G8B8);
    }
    for (u32 i = 0; i < 256; i++)
    {
        g_text_random_bytes[i] = g_replay_unsafe_rng.rand_u16() >> 9;
    }
    if (!g_meiryo_available)
    {
        g_font_904 = CreateFontA(24, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, SHIFTJIS_CHARSET, OUT_DEFAULT_PRECIS,
                                 CLIP_DEFAULT_PRECIS, PROOF_QUALITY, FF_ROMAN | FIXED_PITCH, GOTHIC);
        g_font_908 = CreateFontA(28, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, SHIFTJIS_CHARSET, OUT_DEFAULT_PRECIS,
                                 CLIP_DEFAULT_PRECIS, PROOF_QUALITY, FF_ROMAN | FIXED_PITCH, GOTHIC);
        g_font_90c = CreateFontA(32, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, SHIFTJIS_CHARSET, OUT_DEFAULT_PRECIS,
                                 CLIP_DEFAULT_PRECIS, PROOF_QUALITY, FF_ROMAN | FIXED_PITCH, GOTHIC);
        g_font_91c = CreateFontA(40, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, SHIFTJIS_CHARSET, OUT_DEFAULT_PRECIS,
                                 CLIP_DEFAULT_PRECIS, PROOF_QUALITY, FF_ROMAN | FIXED_PITCH, GOTHIC);
        g_font_910 = CreateFontA(32, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, SHIFTJIS_CHARSET, OUT_DEFAULT_PRECIS,
                                 CLIP_DEFAULT_PRECIS, PROOF_QUALITY, FF_ROMAN | FIXED_PITCH, MINCHO);
        g_font_920 = CreateFontA(40, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, SHIFTJIS_CHARSET, OUT_DEFAULT_PRECIS,
                                 CLIP_DEFAULT_PRECIS, PROOF_QUALITY, FF_ROMAN | FIXED_PITCH, MINCHO);
        g_font_914 = CreateFontA(15, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, SHIFTJIS_CHARSET, OUT_DEFAULT_PRECIS,
                                 CLIP_DEFAULT_PRECIS, PROOF_QUALITY, FF_ROMAN | FIXED_PITCH, GOTHIC);
        g_font_918 = CreateFontA(15, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, SHIFTJIS_CHARSET, OUT_DEFAULT_PRECIS,
                                 CLIP_DEFAULT_PRECIS, PROOF_QUALITY, FF_ROMAN | FIXED_PITCH, MINCHO);
        g_font_924 = CreateFontA(15, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, SHIFTJIS_CHARSET, OUT_DEFAULT_PRECIS,
                                 CLIP_DEFAULT_PRECIS, PROOF_QUALITY, FF_ROMAN | FIXED_PITCH, GOTHIC);
        g_font_928 = CreateFontA(15, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, SHIFTJIS_CHARSET, OUT_DEFAULT_PRECIS,
                                 CLIP_DEFAULT_PRECIS, PROOF_QUALITY, FF_ROMAN | FIXED_PITCH, MINCHO);
    }
    else
    {
        g_font_904 = CreateFontA(36, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, SHIFTJIS_CHARSET, OUT_DEFAULT_PRECIS,
                                 CLIP_DEFAULT_PRECIS, PROOF_QUALITY, FIXED_PITCH, MEIRYO);
        g_font_908 = CreateFontA(42, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, SHIFTJIS_CHARSET, OUT_DEFAULT_PRECIS,
                                 CLIP_DEFAULT_PRECIS, PROOF_QUALITY, FIXED_PITCH, MEIRYO);
        g_font_90c = CreateFontA(48, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, SHIFTJIS_CHARSET, OUT_DEFAULT_PRECIS,
                                 CLIP_DEFAULT_PRECIS, PROOF_QUALITY, FIXED_PITCH, MEIRYO);
        g_font_91c = CreateFontA(60, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, SHIFTJIS_CHARSET, OUT_DEFAULT_PRECIS,
                                 CLIP_DEFAULT_PRECIS, PROOF_QUALITY, FIXED_PITCH, MEIRYO);
        g_font_910 = CreateFontA(32, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, SHIFTJIS_CHARSET, OUT_DEFAULT_PRECIS,
                                 CLIP_DEFAULT_PRECIS, PROOF_QUALITY, FF_ROMAN | FIXED_PITCH, MINCHO);
        g_font_920 = CreateFontA(40, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, SHIFTJIS_CHARSET, OUT_DEFAULT_PRECIS,
                                 CLIP_DEFAULT_PRECIS, PROOF_QUALITY, FF_ROMAN | FIXED_PITCH, MINCHO);
        g_font_914 = CreateFontA(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, SHIFTJIS_CHARSET, OUT_DEFAULT_PRECIS,
                                 CLIP_DEFAULT_PRECIS, PROOF_QUALITY, FIXED_PITCH, MEIRYO);
        g_font_918 = CreateFontA(15, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, SHIFTJIS_CHARSET, OUT_DEFAULT_PRECIS,
                                 CLIP_DEFAULT_PRECIS, PROOF_QUALITY, FF_ROMAN | FIXED_PITCH, MINCHO);
        g_font_924 = CreateFontA(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, SHIFTJIS_CHARSET, OUT_DEFAULT_PRECIS,
                                 CLIP_DEFAULT_PRECIS, PROOF_QUALITY, FIXED_PITCH, MEIRYO);
        g_font_928 = CreateFontA(15, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, SHIFTJIS_CHARSET, OUT_DEFAULT_PRECIS,
                                 CLIP_DEFAULT_PRECIS, PROOF_QUALITY, FF_ROMAN | FIXED_PITCH, MINCHO);
    }
}

// TODO: code matches; buf shares the stack slot of a different spill (frame 0x38 vs 0x34).
// FUNCTION: TH16 0x459240
HARNESS_CALLED void __stdcall draw_text(RECT *dst_rect, i32 x, i32 font_height, D3DCOLOR color,
                                         D3DCOLOR shadow_color, const char *text,
                                         IDirect3DTexture9 *texture, i32 font_id, i32 spacing,
                                         i32 outline)
{
    HFONT font;
    switch (font_id)
    {
    case 8:
        font = g_font_908;
        break;
    case 0:
        font = g_font_90c;
        break;
    case 2:
        font = g_font_914;
        break;
    case 1:
        font = g_font_910;
        break;
    case 3:
        font = g_font_918;
        break;
    case 4:
        font = g_font_91c;
        break;
    case 6:
        font = g_font_924;
        break;
    case 5:
        font = g_font_920;
        break;
    case 7:
        font = g_font_928;
        break;
    default:
        font = g_font_904;
        break;
    }
    memset(g_TextHelper.buffer, 0, g_TextHelper.image_size);
    HDC hdc = g_TextHelper.hdc;
    HGDIOBJ old_font = SelectObject(hdc, font);
    i32 rows = (font_height >= 17 ? font_height : 17) * 2 + 12;
    g_TextHelper.invert_alpha(rows, 0);
    SetBkMode(hdc, TRANSPARENT);
    i32 len = strlen(text);
    if (spacing == 0)
    {
        x += 2;
        if (outline)
        {
            SetTextColor(hdc, shadow_color);
            TextOutA(hdc, x + 1, 1, text, len);
            TextOutA(hdc, x - 1, 1, text, len);
            TextOutA(hdc, x + 1, 3, text, len);
            TextOutA(hdc, x - 1, 3, text, len);
            TextOutA(hdc, x + 2, 2, text, len);
            TextOutA(hdc, x - 2, 2, text, len);
            TextOutA(hdc, x, 0, text, len);
            TextOutA(hdc, x, 4, text, len);
        }
        SetTextColor(hdc, color);
        TextOutA(hdc, x, 2, text, len);
    }
    else
    {
        char buf[3];
        buf[0] = 0;
        buf[1] = 0;
        buf[2] = 0;
        for (i32 i = 0; i < len; i += 2)
        {
            buf[0] = text[i];
            buf[1] = text[i + 1];
            if (outline)
            {
                SetTextColor(hdc, shadow_color);
                TextOutA(hdc, x + 2, 4, buf, 2);
                TextOutA(hdc, x - 2, 4, buf, 2);
                TextOutA(hdc, x + 2, 0, buf, 2);
                TextOutA(hdc, x - 2, 0, buf, 2);
            }
            SetTextColor(hdc, color);
            TextOutA(hdc, x, 2, buf, 2);
            x += spacing;
        }
    }
    SelectObject(hdc, old_font);
    g_TextHelper.invert_alpha(rows, 0);
    g_TextHelper.blur_alpha(rows);
    g_TextHelper.bleed_color(rows);
    SelectObject(hdc, old_font);
    RECT src_rect;
    src_rect.left = 0;
    src_rect.top = 0;
    src_rect.right = dst_rect->right - dst_rect->left;
    src_rect.bottom = dst_rect->bottom - dst_rect->top;
    if (src_rect.right > 1024)
    {
        src_rect.right = 1024;
    }
    if (g_meiryo_available && font_id != 6 && font_id != 2)
    {
        src_rect.top = 6;
        src_rect.bottom += 6;
    }
    {
        IDirect3DSurface9 *surface;
        texture->GetSurfaceLevel(0, &surface);
        D3DXLoadSurfaceFromMemory(surface, NULL, dst_rect, g_TextHelper.buffer, g_TextHelper.format,
                                  g_TextHelper.pitch, NULL, &src_rect, D3DX_FILTER_NONE, 0);
        if (surface != NULL)
        {
            surface->Release();
        }
    }
}
