// gdi32 over FreeType: the text renderer's subset of GDI. TextHelper.cpp
// draws dialogue, spell card names and menu text into a DIB section
// (CreateDIBSection with a BITMAPV4HEADER in the texture's pixel format,
// A4R4G4B4 or A8R8G8B8, selected into a memory DC), then copies the pixels
// into a D3D texture.
//
// What the game relies on (see TextHelper.cpp draw_text): GDI writes whole
// pixels through the DIB's colour masks, so every pixel text touches gets
// its alpha bits cleared. The game sets alpha everywhere first, draws, then
// inverts alpha: drawn pixels become opaque, the rest transparent. Glyphs
// are antialiased by blending the text colour over the pixel's colour by
// coverage in 17 levels, as GDI's grayscale smoothing does.
//
// Fonts: the game asks for MS Gothic and MS Mincho (Shift-JIS face names,
// SHIFTJIS_CHARSET), or Meiryo when EnumFontFamiliesExA finds it.
// fontconfig picks the installed font: the face itself if it is there,
// else Noto Sans Mono CJK JP for Gothic and Noto Serif CJK JP for Mincho
// (then any Japanese monospace, sans or serif font); TH16_FONT_GOTHIC and
// TH16_FONT_MINCHO (a file or a fontconfig pattern) override. A
// substitute is laid out with MS Gothic's metrics, which the game's text
// positions were made for: the cell height (CreateFontA's height) is the em
// size, the ascent 220/256 of it, half-width characters (single Shift-JIS
// bytes) half an em wide and full-width ones an em, glyphs centred in their
// cells.
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <map>
#include <mutex>
#include <set>
#include <string>
#include <vector>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_SYNTHESIS_H
#include FT_TRUETYPE_TABLES_H
#include <fontconfig/fontconfig.h>

#include <windows.h>

#include "port_platform.h"
#include "port_stub.h"
#ifdef TH16_THCRAP
#include "thcrap/thcrap.h"
#endif

namespace
{

enum GdiKind
{
    GDI_DC,
    GDI_BITMAP,
    GDI_FONT,
    GDI_BRUSH,
    GDI_PEN,
};

struct GdiObject
{
    GdiKind kind;
    bool stock = false;
    explicit GdiObject(GdiKind kind) : kind(kind)
    {
    }
    virtual ~GdiObject()
    {
    }
};

struct GdiBitmap : GdiObject
{
    int width = 1;
    int height = 1;
    bool top_down = true;
    int bits_per_pixel = 1;
    int stride = 4;
    // Colour masks (BI_BITFIELDS or the BI_RGB defaults).
    uint32_t masks[3] = {0, 0, 0};
    uint8_t *bits = NULL;
    bool owns_bits = false;

    GdiBitmap() : GdiObject(GDI_BITMAP)
    {
    }
    ~GdiBitmap() override
    {
        if (owns_bits)
        {
            free(bits);
        }
    }
};

struct LoadedFace
{
    FT_Face face = NULL;
    std::string family;
    int weight = FC_WEIGHT_REGULAR;
};

struct GdiFont : GdiObject
{
    LOGFONTA logfont;
    LoadedFace *face = NULL;
    // With thcrap fonts (Latin faces): the Japanese face for characters
    // the face does not have, as GDI's font linking would give them.
    LoadedFace *fallback = NULL;
    // A real font's own advances (proportional); else MS Gothic's cells.
    bool proportional = false;
    // Pixels: the em size glyphs are rendered at, the cell's ascent and
    // height, the advance of a half-width character.
    int em = 16;
    int ascent = 14;
    int cell_height = 16;
    int half_advance = 8;
    bool synthetic_bold = false;

    GdiFont() : GdiObject(GDI_FONT)
    {
    }
};

struct GdiDC : GdiObject
{
    GdiBitmap *bitmap;
    GdiFont *font;
    GdiObject *brush;
    GdiObject *pen;
    COLORREF text_color = 0;
    COLORREF bk_color = 0xffffff;
    int bk_mode = OPAQUE;

    GdiDC() : GdiObject(GDI_DC)
    {
    }
};

// Every live object; rendering runs under the same lock (FreeType faces are
// not thread-safe).
std::recursive_mutex g_lock;
std::set<GdiObject *> g_objects;

FT_Library g_freetype;
bool g_fontconfig_ready;
std::map<std::string, LoadedFace *> g_faces;

GdiBitmap *stock_bitmap()
{
    static GdiBitmap bitmap;
    bitmap.stock = true;
    return &bitmap;
}

GdiFont *stock_font();

GdiObject *stock_object(int index)
{
    static GdiObject brushes[6] = {GdiObject(GDI_BRUSH), GdiObject(GDI_BRUSH), GdiObject(GDI_BRUSH),
                                   GdiObject(GDI_BRUSH), GdiObject(GDI_BRUSH), GdiObject(GDI_BRUSH)};
    static GdiObject pens[3] = {GdiObject(GDI_PEN), GdiObject(GDI_PEN), GdiObject(GDI_PEN)};
    if (index >= WHITE_BRUSH && index <= NULL_BRUSH)
    {
        brushes[index].stock = true;
        return &brushes[index];
    }
    if (index >= 6 && index <= 8) // WHITE_PEN, BLACK_PEN, NULL_PEN
    {
        pens[index - 6].stock = true;
        return &pens[index - 6];
    }
    if (index >= 10 && index <= 17) // the stock fonts
    {
        return stock_font();
    }
    return NULL;
}

template <typename T> T *as(HGDIOBJ handle, GdiKind kind)
{
    GdiObject *object = (GdiObject *)handle;
    if (object == NULL)
    {
        return NULL;
    }
    if (!object->stock && g_objects.count(object) == 0)
    {
        return NULL;
    }
    return object->kind == kind ? (T *)object : NULL;
}

GdiDC *new_dc()
{
    GdiDC *dc = new GdiDC();
    dc->bitmap = stock_bitmap();
    dc->font = stock_font();
    dc->brush = stock_object(WHITE_BRUSH);
    dc->pen = stock_object(7);
    g_objects.insert(dc);
    return dc;
}

// ---------------------------------------------------------------------------
// Fonts

bool init_freetype()
{
    if (g_freetype == NULL && FT_Init_FreeType(&g_freetype) != 0)
    {
        port_log("FreeType failed to start; no text");
        return false;
    }
    if (!g_fontconfig_ready)
    {
        g_fontconfig_ready = FcInit();
    }
    return true;
}

LoadedFace *load_face(const std::string &file, int index, const std::string &family, int weight)
{
    std::string key = file + "#" + std::to_string(index);
    auto found = g_faces.find(key);
    if (found != g_faces.end())
    {
        return found->second;
    }
    FT_Face face;
    if (FT_New_Face(g_freetype, file.c_str(), index, &face) != 0)
    {
        return NULL;
    }
    LoadedFace *loaded = new LoadedFace();
    loaded->face = face;
    loaded->family = family;
    loaded->weight = weight;
    g_faces[key] = loaded;
    return loaded;
}

// Matches a fontconfig pattern; with `exact_family`, only a font of that
// family counts. The font must have Japanese kana and kanji unless
// `any_script` (thcrap's Latin fonts).
LoadedFace *match_font(const std::string &pattern_text, int weight, bool exact_family, bool any_script = false)
{
    FcPattern *pattern = FcNameParse((const FcChar8 *)pattern_text.c_str());
    if (pattern == NULL)
    {
        return NULL;
    }
    FcPatternDel(pattern, FC_WEIGHT);
    FcPatternAddInteger(pattern, FC_WEIGHT, weight);
    FcConfigSubstitute(NULL, pattern, FcMatchPattern);
    FcDefaultSubstitute(pattern);
    FcResult result;
    FcPattern *match = FcFontMatch(NULL, pattern, &result);
    LoadedFace *face = NULL;
    if (match != NULL)
    {
        FcChar8 *file = NULL;
        FcChar8 *family = NULL;
        int index = 0;
        int matched_weight = FC_WEIGHT_REGULAR;
        FcCharSet *charset = NULL;
        FcPatternGetString(match, FC_FILE, 0, &file);
        FcPatternGetString(match, FC_FAMILY, 0, &family);
        FcPatternGetInteger(match, FC_INDEX, 0, &index);
        FcPatternGetInteger(match, FC_WEIGHT, 0, &matched_weight);
        FcPatternGetCharSet(match, FC_CHARSET, 0, &charset);
        bool japanese = charset != NULL && FcCharSetHasChar(charset, 0x3042) && FcCharSetHasChar(charset, 0x6f22);
        bool family_ok = true;
        if (exact_family)
        {
            // The family named by the pattern (up to ':').
            std::string wanted = pattern_text.substr(0, pattern_text.find(':'));
            family_ok = false;
            for (int i = 0; FcPatternGetString(match, FC_FAMILY, i, &family) == FcResultMatch; i++)
            {
                if (strcasecmp((const char *)family, wanted.c_str()) == 0)
                {
                    family_ok = true;
                    break;
                }
            }
            FcPatternGetString(match, FC_FAMILY, 0, &family);
        }
        if (file != NULL && (japanese || any_script) && family_ok)
        {
            face = load_face((const char *)file, index, family != NULL ? (const char *)family : "", matched_weight);
        }
        FcPatternDestroy(match);
    }
    FcPatternDestroy(pattern);
    return face;
}

// Whether the system has this face itself (a registered thcrap font counts:
// port_gdi_add_font_file). Japanese faces only, unless `any_script`.
LoadedFace *find_real_face(const std::string &name_utf8, int weight, bool any_script = false)
{
    if (!init_freetype() || !g_fontconfig_ready || name_utf8.empty())
    {
        return NULL;
    }
    return match_font(name_utf8, weight, true, any_script);
}

bool contains(const std::string &text, const char *part)
{
    return strcasestr(text.c_str(), part) != NULL;
}

// Whether strings are taken as UTF-8 when they are valid UTF-8 (thcrap's
// patches are UTF-8; the game's own strings Shift-JIS), as thcrap's
// win32_utf8 does. Off without a thcrap stack.
bool g_utf8_text;

// Whether `text` is valid UTF-8 (shortest forms, no surrogates).
bool valid_utf8(const uint8_t *s, int length)
{
    for (int i = 0; i < length;)
    {
        uint8_t c = s[i];
        int n = c < 0x80 ? 1 : (c & 0xe0) == 0xc0 ? 2 : (c & 0xf0) == 0xe0 ? 3 : (c & 0xf8) == 0xf0 ? 4 : 0;
        if (n == 0 || i + n > length || (n == 2 && c < 0xc2))
        {
            return false;
        }
        uint32_t cp = n == 1 ? c : c & (0x7f >> n);
        for (int k = 1; k < n; k++)
        {
            if ((s[i + k] & 0xc0) != 0x80)
            {
                return false;
            }
            cp = (cp << 6) | (s[i + k] & 0x3f);
        }
        if ((n == 3 && cp < 0x800) || (n == 4 && (cp < 0x10000 || cp > 0x10ffff)) || (cp >= 0xd800 && cp < 0xe000))
        {
            return false;
        }
        i += n;
    }
    return true;
}

// A face name: UTF-8 (thcrap's "font") or Shift-JIS (the game's).
std::string face_name_utf8(const char *name)
{
    size_t length = strnlen(name, LF_FACESIZE);
    if (g_utf8_text && valid_utf8((const uint8_t *)name, (int)length))
    {
        return std::string(name, length);
    }
    return port_sjis_to_utf8(name, (int)length);
}

// TH16_FONT_GOTHIC or TH16_FONT_MINCHO (a file or a fontconfig pattern),
// if set and found.
LoadedFace *override_face(bool mincho, int weight)
{
    LoadedFace *face = NULL;
    const char *override_name = getenv(mincho ? "TH16_FONT_MINCHO" : "TH16_FONT_GOTHIC");
    if (override_name != NULL && override_name[0] != '\0')
    {
        if (override_name[0] == '/')
        {
            face = load_face(override_name, 0, override_name, weight);
        }
        else if (g_fontconfig_ready)
        {
            face = match_font(override_name, weight, false);
        }
        if (face == NULL)
        {
            port_log("font %s not found", override_name);
        }
    }
    return face;
}

// The first substitute for MS Gothic or MS Mincho the system has.
LoadedFace *substitute_face(bool mincho, int weight)
{
    LoadedFace *face = NULL;
    if (g_fontconfig_ready)
    {
        static const char *const gothic[] = {"MS Gothic", "Noto Sans Mono CJK JP", "Source Han Code JP",
                                             "Noto Sans CJK JP", "Source Han Sans JP", "IPAGothic",
                                             "monospace:lang=ja", "sans-serif:lang=ja"};
        static const char *const serif[] = {"MS Mincho", "Noto Serif CJK JP", "Source Han Serif JP", "IPAMincho",
                                            "serif:lang=ja", "sans-serif:lang=ja"};
        const char *const *names = mincho ? serif : gothic;
        size_t count = mincho ? sizeof(serif) / sizeof(*serif) : sizeof(gothic) / sizeof(*gothic);
        for (size_t i = 0; i < count && face == NULL; i++)
        {
            face = match_font(names[i], weight, strchr(names[i], ':') == NULL);
        }
    }
    return face;
}

void setup_font(GdiFont *font)
{
    std::lock_guard<std::recursive_mutex> guard(g_lock);
    const LOGFONTA &lf = font->logfont;
    std::string face_name = face_name_utf8(lf.lfFaceName);
    int weight = FcWeightFromOpenType(lf.lfWeight == FW_DONTCARE ? FW_NORMAL : lf.lfWeight);
    bool mincho = contains(face_name, "\xe6\x98\x8e\xe6\x9c\x9d") || contains(face_name, "mincho") ||
                  contains(face_name, "serif");
    if (!init_freetype())
    {
        return;
    }
    LoadedFace *face = override_face(mincho, weight);
    bool emulate_metrics = true;
    if (face == NULL && !face_name.empty())
    {
        face = find_real_face(face_name, weight);
        emulate_metrics = face == NULL;
        if (face == NULL && g_utf8_text)
        {
            // A thcrap font without Japanese (Touhou Biolinum): its own
            // metrics, Japanese characters from the substitute.
            face = find_real_face(face_name, weight, true);
            if (face != NULL)
            {
                emulate_metrics = false;
                LoadedFace *japanese = override_face(mincho, weight);
                font->fallback = japanese != NULL ? japanese : substitute_face(mincho, weight);
            }
        }
    }
    if (face == NULL)
    {
        face = substitute_face(mincho, weight);
    }
    font->face = face;
    if (face == NULL)
    {
        port_log("no Japanese font for \"%s\"; text stays empty", face_name.c_str());
        return;
    }
    font->proportional = !emulate_metrics && g_utf8_text;
    // CreateFontA's height: > 0 the cell height, < 0 the em size.
    int height = lf.lfHeight == 0 ? 16 : lf.lfHeight;
    TT_OS2 *os2 = (TT_OS2 *)FT_Get_Sfnt_Table(face->face, FT_SFNT_OS2);
    if (emulate_metrics || os2 == NULL || os2->usWinAscent + os2->usWinDescent == 0)
    {
        // MS Gothic: the cell is the em square.
        font->em = height > 0 ? height : -height;
        font->cell_height = font->em;
        font->ascent = (font->em * 220 + 128) / 256;
    }
    else
    {
        double cell_units = os2->usWinAscent + os2->usWinDescent;
        double em_units = face->face->units_per_EM;
        font->em = height > 0 ? (int)(height * em_units / cell_units + 0.5) : -height;
        font->cell_height = height > 0 ? height : (int)(-height * cell_units / em_units + 0.5);
        font->ascent = (int)(font->cell_height * os2->usWinAscent / cell_units + 0.5);
    }
    font->half_advance = (font->em + 1) / 2;
    font->synthetic_bold = lf.lfWeight >= FW_SEMIBOLD && face->weight < FC_WEIGHT_DEMIBOLD;
    static std::set<std::string> logged;
    std::string description = face_name + " -> " + face->family;
    if (font->fallback != NULL)
    {
        description += " (Japanese from " + font->fallback->family + ")";
    }
    if (logged.insert(description).second)
    {
        port_log("font %s%s", description.c_str(), emulate_metrics ? " (MS Gothic metrics)" : "");
    }
}

GdiFont *stock_font()
{
    static GdiFont *font;
    if (font == NULL)
    {
        font = new GdiFont();
        font->stock = true;
        memset(&font->logfont, 0, sizeof(font->logfont));
        font->logfont.lfHeight = 16;
        font->logfont.lfWeight = FW_NORMAL;
        setup_font(font);
    }
    return font;
}

// Shift-JIS characters: lead bytes 0x81-0x9f and 0xe0-0xfc start two-byte
// (full-width) characters.
bool is_lead_byte(uint8_t c)
{
    return (c >= 0x81 && c <= 0x9f) || (c >= 0xe0 && c <= 0xfc);
}

uint32_t code_point(const uint8_t *s, int length)
{
    static std::map<uint32_t, uint32_t> cache;
    uint32_t key = length == 2 ? (s[0] << 8) | s[1] : s[0];
    auto found = cache.find(key);
    if (found != cache.end())
    {
        return found->second;
    }
    std::string utf8 = port_sjis_to_utf8((const char *)s, length);
    const uint8_t *u = (const uint8_t *)utf8.data();
    uint32_t c = 0xfffd;
    if (utf8.size() >= 1 && u[0] < 0x80)
    {
        c = u[0];
    }
    else if (utf8.size() >= 2 && (u[0] & 0xe0) == 0xc0)
    {
        c = ((u[0] & 0x1f) << 6) | (u[1] & 0x3f);
    }
    else if (utf8.size() >= 3 && (u[0] & 0xf0) == 0xe0)
    {
        c = ((u[0] & 0x0f) << 12) | ((u[1] & 0x3f) << 6) | (u[2] & 0x3f);
    }
    else if (utf8.size() >= 4 && (u[0] & 0xf8) == 0xf0)
    {
        c = ((u[0] & 0x07) << 18) | ((u[1] & 0x3f) << 12) | ((u[2] & 0x3f) << 6) | (u[3] & 0x3f);
    }
    cache[key] = c;
    return c;
}

// The characters of a string: its code point, and whether it takes a full
// (em-wide) cell in MS Gothic's metrics. Shift-JIS, or UTF-8 when the
// string is valid UTF-8 and g_utf8_text is on.
template <typename F> void for_each_char(const char *text, int length, F f)
{
    const uint8_t *s = (const uint8_t *)text;
    if (g_utf8_text && valid_utf8(s, length))
    {
        for (int i = 0; i < length;)
        {
            uint8_t c = s[i];
            int n = c < 0x80 ? 1 : (c & 0xe0) == 0xc0 ? 2 : (c & 0xf0) == 0xe0 ? 3 : 4;
            uint32_t cp = n == 1 ? c : c & (0x7f >> n);
            for (int k = 1; k < n; k++)
            {
                cp = (cp << 6) | (s[i + k] & 0x3f);
            }
            // Half-width: what Shift-JIS has as single bytes (ASCII,
            // half-width katakana) and Latin-1, as MS Gothic draws them.
            f(cp, !(cp < 0x100 || (cp >= 0xff61 && cp <= 0xff9f)));
            i += n;
        }
        return;
    }
    for (int i = 0; i < length;)
    {
        int n = is_lead_byte(s[i]) && i + 1 < length ? 2 : 1;
        f(code_point(s + i, n), n == 2);
        i += n;
    }
}

// The face that draws `c`: the font's own, or its Japanese fallback.
LoadedFace *face_for(GdiFont *font, uint32_t c)
{
    if (font->fallback != NULL && FT_Get_Char_Index(font->face->face, c) == 0 &&
        FT_Get_Char_Index(font->fallback->face, c) != 0)
    {
        return font->fallback;
    }
    return font->face;
}

// Loads `c` at the font's size into the face's glyph slot, with the
// synthetic styles; false if the face has no such glyph.
bool load_glyph(GdiFont *font, LoadedFace *face, uint32_t c)
{
    FT_Face ft = face->face;
    FT_Set_Pixel_Sizes(ft, 0, font->em);
    FT_UInt glyph = FT_Get_Char_Index(ft, c);
    if (glyph == 0)
    {
        return false;
    }
    bool mono = font->logfont.lfQuality == NONANTIALIASED_QUALITY;
    if (FT_Load_Glyph(ft, glyph, FT_LOAD_DEFAULT | (mono ? FT_LOAD_TARGET_MONO : FT_LOAD_TARGET_LIGHT)) != 0)
    {
        return false;
    }
    if (font->logfont.lfItalic && !(ft->style_flags & FT_STYLE_FLAG_ITALIC))
    {
        FT_GlyphSlot_Oblique(ft->glyph);
    }
    if (font->synthetic_bold || (face != font->face && font->logfont.lfWeight >= FW_SEMIBOLD &&
                                 face->weight < FC_WEIGHT_DEMIBOLD))
    {
        FT_GlyphSlot_Embolden(ft->glyph);
    }
    return true;
}

// How far a character moves the pen: its cell in MS Gothic's metrics, or
// the glyph's own (rounded) advance for a real font.
int char_advance(GdiFont *font, uint32_t c, bool full_width)
{
    if (!font->proportional)
    {
        return full_width ? font->em : font->half_advance;
    }
    static std::map<std::string, int> cache;
    LoadedFace *face = face_for(font, c);
    char key[96];
    snprintf(key, sizeof(key), "%p/%d/%d/%d/%d/%u", (void *)face, font->em, font->logfont.lfItalic,
             font->logfont.lfWeight, font->logfont.lfQuality, c);
    auto found = cache.find(key);
    if (found != cache.end())
    {
        return found->second;
    }
    int advance = full_width ? font->em : font->half_advance;
    if (load_glyph(font, face, c))
    {
        advance = (int)((face->face->glyph->advance.x + 32) >> 6);
    }
    cache[key] = advance;
    return advance;
}

int text_width(GdiFont *font, const char *text, int length)
{
    int width = 0;
    for_each_char(text, length, [&](uint32_t c, bool full) { width += char_advance(font, c, full); });
    return width;
}

// ---------------------------------------------------------------------------
// Pixels

int mask_shift(uint32_t mask)
{
    int shift = 0;
    while (mask != 0 && !(mask & 1))
    {
        mask >>= 1;
        shift++;
    }
    return shift;
}

int mask_bits(uint32_t mask)
{
    int bits = 0;
    for (mask >>= mask_shift(mask); mask & 1; mask >>= 1)
    {
        bits++;
    }
    return bits;
}

uint32_t read_pixel(const GdiBitmap *bitmap, int x, int y)
{
    int row = bitmap->top_down ? y : bitmap->height - 1 - y;
    const uint8_t *p = bitmap->bits + (size_t)row * bitmap->stride;
    switch (bitmap->bits_per_pixel)
    {
    case 16:
        return ((const uint16_t *)p)[x];
    case 24:
        p += x * 3;
        return p[0] | (p[1] << 8) | (p[2] << 16);
    case 32:
        return ((const uint32_t *)p)[x];
    }
    return 0;
}

void write_pixel(GdiBitmap *bitmap, int x, int y, uint32_t value)
{
    int row = bitmap->top_down ? y : bitmap->height - 1 - y;
    uint8_t *p = bitmap->bits + (size_t)row * bitmap->stride;
    switch (bitmap->bits_per_pixel)
    {
    case 16:
        ((uint16_t *)p)[x] = (uint16_t)value;
        break;
    case 24:
        p += x * 3;
        p[0] = (uint8_t)value;
        p[1] = (uint8_t)(value >> 8);
        p[2] = (uint8_t)(value >> 16);
        break;
    case 32:
        ((uint32_t *)p)[x] = value;
        break;
    }
}

// A channel (8 bits) from and to a pixel through its mask.
int get_channel(uint32_t pixel, uint32_t mask)
{
    int bits = mask_bits(mask);
    if (bits == 0)
    {
        return 0;
    }
    uint32_t value = (pixel & mask) >> mask_shift(mask);
    return bits >= 8 ? (int)(value >> (bits - 8)) : (int)((value * 255 + ((1u << bits) - 1) / 2) / ((1u << bits) - 1));
}

uint32_t put_channel(int value, uint32_t mask)
{
    int bits = mask_bits(mask);
    if (bits == 0)
    {
        return 0;
    }
    uint32_t scaled = bits >= 8 ? (uint32_t)value << (bits - 8) : (uint32_t)value >> (8 - bits);
    return (scaled << mask_shift(mask)) & mask;
}

// Blends `color` (COLORREF) over a pixel by coverage (0-255). The pixel is
// rewritten from its colour channels only, so bits outside the masks (the
// alpha of A4R4G4B4 and A8R8G8B8) become 0, as with GDI.
void blend_pixel(GdiBitmap *bitmap, int x, int y, COLORREF color, int coverage)
{
    if (x < 0 || y < 0 || x >= bitmap->width || y >= bitmap->height || coverage <= 0)
    {
        return;
    }
    const int source[3] = {(int)(color & 0xff), (int)((color >> 8) & 0xff), (int)((color >> 16) & 0xff)};
    uint32_t pixel = read_pixel(bitmap, x, y);
    uint32_t out = 0;
    for (int i = 0; i < 3; i++)
    {
        int dst = get_channel(pixel, bitmap->masks[i]);
        int value = coverage >= 255 ? source[i] : dst + ((source[i] - dst) * coverage + 127) / 255;
        out |= put_channel(value, bitmap->masks[i]);
    }
    write_pixel(bitmap, x, y, out);
}

bool drawable(GdiDC *dc)
{
    GdiBitmap *bitmap = dc->bitmap;
    return bitmap != NULL && bitmap->bits != NULL &&
           (bitmap->bits_per_pixel == 16 || bitmap->bits_per_pixel == 24 || bitmap->bits_per_pixel == 32);
}

void draw_glyph(GdiDC *dc, GdiFont *font, uint32_t c, int cell_x, int cell_width, int baseline)
{
    LoadedFace *loaded = face_for(font, c);
    if (!load_glyph(font, loaded, c))
    {
        return;
    }
    FT_Face face = loaded->face;
    bool mono = font->logfont.lfQuality == NONANTIALIASED_QUALITY;
    if (face->glyph->format != FT_GLYPH_FORMAT_BITMAP &&
        FT_Render_Glyph(face->glyph, mono ? FT_RENDER_MODE_MONO : FT_RENDER_MODE_NORMAL) != 0)
    {
        return;
    }
    const FT_Bitmap &bitmap = face->glyph->bitmap;
    int advance = (int)(face->glyph->advance.x >> 6);
    // A real font's glyph starts at the pen; a substitute's is centred in
    // MS Gothic's cell.
    int x0 = font->proportional ? cell_x + face->glyph->bitmap_left
                                : cell_x + (cell_width - advance) / 2 + face->glyph->bitmap_left;
    int y0 = baseline - face->glyph->bitmap_top;
    for (unsigned row = 0; row < bitmap.rows; row++)
    {
        const uint8_t *src = bitmap.buffer + (ptrdiff_t)row * bitmap.pitch;
        for (unsigned col = 0; col < bitmap.width; col++)
        {
            int coverage;
            if (bitmap.pixel_mode == FT_PIXEL_MODE_MONO)
            {
                coverage = (src[col >> 3] & (0x80 >> (col & 7))) ? 255 : 0;
            }
            else if (bitmap.pixel_mode == FT_PIXEL_MODE_GRAY)
            {
                // GDI's grayscale smoothing has 17 levels (GGO_GRAY4): the
                // faintest edges round to nothing rather than to opaque
                // pixels after the game's alpha trick.
                coverage = (src[col] * 16 + 127) / 255 * 255 / 16;
            }
            else
            {
                continue;
            }
            blend_pixel(dc->bitmap, x0 + (int)col, y0 + (int)row, dc->text_color, coverage);
        }
    }
}

} // namespace

// Whether `bits` lies in a DIB section's memory (the null renderer's text
// dump uses it to tell text from other uploads).
bool port_gdi_is_dib_memory(const void *bits)
{
    std::lock_guard<std::recursive_mutex> guard(g_lock);
    for (GdiObject *object : g_objects)
    {
        if (object->kind != GDI_BITMAP)
        {
            continue;
        }
        GdiBitmap *bitmap = (GdiBitmap *)object;
        const uint8_t *p = (const uint8_t *)bits;
        if (p >= bitmap->bits && p < bitmap->bits + (size_t)bitmap->stride * bitmap->height)
        {
            return true;
        }
    }
    return false;
}

extern "C" {

HFONT CreateFontIndirectA(const LOGFONTA *lplf)
{
    std::lock_guard<std::recursive_mutex> guard(g_lock);
    GdiFont *font = new GdiFont();
    font->logfont = *lplf;
#ifdef TH16_THCRAP
    port_thcrap_font_rules(&font->logfont);
#endif
    setup_font(font);
    g_objects.insert(font);
    return (HFONT)font;
}

HFONT CreateFontA(int cHeight, int cWidth, int cEscapement, int cOrientation, int cWeight, DWORD bItalic,
                  DWORD bUnderline, DWORD bStrikeOut, DWORD iCharSet, DWORD iOutPrecision, DWORD iClipPrecision,
                  DWORD iQuality, DWORD iPitchAndFamily, LPCSTR pszFaceName)
{
    LOGFONTA lf;
    memset(&lf, 0, sizeof(lf));
    lf.lfHeight = cHeight;
    lf.lfWidth = cWidth;
    lf.lfEscapement = cEscapement;
    lf.lfOrientation = cOrientation;
    lf.lfWeight = cWeight;
    lf.lfItalic = (BYTE)bItalic;
    lf.lfUnderline = (BYTE)bUnderline;
    lf.lfStrikeOut = (BYTE)bStrikeOut;
    lf.lfCharSet = (BYTE)iCharSet;
    lf.lfOutPrecision = (BYTE)iOutPrecision;
    lf.lfClipPrecision = (BYTE)iClipPrecision;
    lf.lfQuality = (BYTE)iQuality;
    lf.lfPitchAndFamily = (BYTE)iPitchAndFamily;
    if (pszFaceName != NULL)
    {
        strncpy(lf.lfFaceName, pszFaceName, LF_FACESIZE - 1);
    }
#ifdef TH16_THCRAP
    port_thcrap_font_face(lf.lfFaceName);
#endif
    return CreateFontIndirectA(&lf);
}

HGDIOBJ SelectObject(HDC hdc, HGDIOBJ h)
{
    std::lock_guard<std::recursive_mutex> guard(g_lock);
    GdiDC *dc = as<GdiDC>(hdc, GDI_DC);
    GdiObject *object = (GdiObject *)h;
    if (dc == NULL || object == NULL || (!object->stock && g_objects.count(object) == 0))
    {
        return NULL;
    }
    GdiObject *previous = NULL;
    switch (object->kind)
    {
    case GDI_BITMAP:
        previous = dc->bitmap;
        dc->bitmap = (GdiBitmap *)object;
        break;
    case GDI_FONT:
        previous = dc->font;
        dc->font = (GdiFont *)object;
        break;
    case GDI_BRUSH:
        previous = dc->brush;
        dc->brush = object;
        break;
    case GDI_PEN:
        previous = dc->pen;
        dc->pen = object;
        break;
    default:
        return NULL;
    }
    return (HGDIOBJ)previous;
}

BOOL DeleteObject(HGDIOBJ ho)
{
    std::lock_guard<std::recursive_mutex> guard(g_lock);
    GdiObject *object = (GdiObject *)ho;
    if (object == NULL || object->stock || g_objects.count(object) == 0 || object->kind == GDI_DC)
    {
        return FALSE;
    }
    g_objects.erase(object);
    delete object;
    return TRUE;
}

HGDIOBJ GetStockObject(int i)
{
    std::lock_guard<std::recursive_mutex> guard(g_lock);
    return (HGDIOBJ)stock_object(i);
}

HDC CreateCompatibleDC(HDC hdc)
{
    std::lock_guard<std::recursive_mutex> guard(g_lock);
    return (HDC)new_dc();
}

BOOL DeleteDC(HDC hdc)
{
    std::lock_guard<std::recursive_mutex> guard(g_lock);
    GdiDC *dc = as<GdiDC>(hdc, GDI_DC);
    if (dc == NULL || dc->stock)
    {
        return FALSE;
    }
    g_objects.erase(dc);
    delete dc;
    return TRUE;
}

// The screen's DC (for any window): the game reads the refresh rate from it
// and enumerates fonts with it.
HDC GetDC(HWND hWnd)
{
    std::lock_guard<std::recursive_mutex> guard(g_lock);
    static GdiDC *screen;
    if (screen == NULL)
    {
        screen = new_dc();
        g_objects.erase(screen);
        screen->stock = true;
    }
    return (HDC)screen;
}

int ReleaseDC(HWND hWnd, HDC hDC)
{
    return 1;
}

int GetDeviceCaps(HDC hdc, int index)
{
    int width, height;
    switch (index)
    {
    case VREFRESH:
        return port_display_refresh_rate();
    case BITSPIXEL:
        return 32;
    case HORZRES:
        port_display_size(&width, &height);
        return width;
    case VERTRES:
        port_display_size(&width, &height);
        return height;
    case LOGPIXELSX:
    case LOGPIXELSY:
        return 96;
    }
    return 0;
}

// DIB sections of 16, 24 and 32 bits per pixel, BI_RGB or BI_BITFIELDS
// (masks right after the 40-byte header, where both BITMAPINFO's colour
// table and BITMAPV4HEADER's mask fields are), top-down or bottom-up.
HBITMAP CreateDIBSection(HDC hdc, const BITMAPINFO *pbmi, UINT usage, void **ppvBits, HANDLE hSection, DWORD offset)
{
    std::lock_guard<std::recursive_mutex> guard(g_lock);
    if (ppvBits != NULL)
    {
        *ppvBits = NULL;
    }
    const BITMAPINFOHEADER &header = pbmi->bmiHeader;
    int bpp = header.biBitCount;
    if (header.biWidth <= 0 || header.biHeight == 0 || (bpp != 16 && bpp != 24 && bpp != 32) ||
        (header.biCompression != BI_RGB && header.biCompression != BI_BITFIELDS) || hSection != NULL)
    {
        port_log("CreateDIBSection: unsupported %dx%d, %d bits, compression %u", (int)header.biWidth,
                 (int)header.biHeight, bpp, (unsigned)header.biCompression);
        SetLastError(ERROR_INVALID_PARAMETER);
        return NULL;
    }
    GdiBitmap *bitmap = new GdiBitmap();
    bitmap->width = header.biWidth;
    bitmap->height = header.biHeight < 0 ? -header.biHeight : header.biHeight;
    bitmap->top_down = header.biHeight < 0;
    bitmap->bits_per_pixel = bpp;
    bitmap->stride = ((bitmap->width * bpp + 31) / 32) * 4;
    if (header.biCompression == BI_BITFIELDS)
    {
        const uint32_t *masks = (const uint32_t *)((const uint8_t *)pbmi + 40);
        bitmap->masks[0] = masks[0];
        bitmap->masks[1] = masks[1];
        bitmap->masks[2] = masks[2];
    }
    else if (bpp == 16)
    {
        bitmap->masks[0] = 0x7c00;
        bitmap->masks[1] = 0x03e0;
        bitmap->masks[2] = 0x001f;
    }
    else
    {
        bitmap->masks[0] = 0xff0000;
        bitmap->masks[1] = 0x00ff00;
        bitmap->masks[2] = 0x0000ff;
    }
    size_t size = (size_t)bitmap->stride * bitmap->height;
    bitmap->bits = (uint8_t *)calloc(1, size);
    bitmap->owns_bits = true;
    if (bitmap->bits == NULL)
    {
        delete bitmap;
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return NULL;
    }
    g_objects.insert(bitmap);
    if (ppvBits != NULL)
    {
        *ppvBits = bitmap->bits;
    }
    return (HBITMAP)bitmap;
}

COLORREF SetTextColor(HDC hdc, COLORREF color)
{
    std::lock_guard<std::recursive_mutex> guard(g_lock);
    GdiDC *dc = as<GdiDC>(hdc, GDI_DC);
    if (dc == NULL)
    {
        return 0xffffffff; // CLR_INVALID
    }
    COLORREF previous = dc->text_color;
    dc->text_color = color;
    return previous;
}

COLORREF SetBkColor(HDC hdc, COLORREF color)
{
    std::lock_guard<std::recursive_mutex> guard(g_lock);
    GdiDC *dc = as<GdiDC>(hdc, GDI_DC);
    if (dc == NULL)
    {
        return 0xffffffff;
    }
    COLORREF previous = dc->bk_color;
    dc->bk_color = color;
    return previous;
}

int SetBkMode(HDC hdc, int mode)
{
    std::lock_guard<std::recursive_mutex> guard(g_lock);
    GdiDC *dc = as<GdiDC>(hdc, GDI_DC);
    if (dc == NULL)
    {
        return 0;
    }
    int previous = dc->bk_mode;
    dc->bk_mode = mode;
    return previous;
}

// Draws Shift-JIS text (or UTF-8, see g_utf8_text) with its cell's
// top-left corner at (x, y) (TA_TOP | TA_LEFT, the default alignment).
// With a thcrap stack, TextOutA first goes through thcrap's layout
// (port/src/thcrap/text.cpp), which calls this for each run.
BOOL port_gdi_text_out_raw(HDC hdc, int x, int y, const char *lpString, int c)
{
    std::lock_guard<std::recursive_mutex> guard(g_lock);
    GdiDC *dc = as<GdiDC>(hdc, GDI_DC);
    if (dc == NULL)
    {
        return FALSE;
    }
    GdiFont *font = dc->font;
    if (!drawable(dc) || font == NULL || font->face == NULL || c <= 0)
    {
        return TRUE;
    }
    if (dc->bk_mode == OPAQUE)
    {
        int width = text_width(font, lpString, c);
        for (int row = y; row < y + font->cell_height; row++)
        {
            for (int col = x; col < x + width; col++)
            {
                blend_pixel(dc->bitmap, col, row, dc->bk_color, 255);
            }
        }
    }
    int pen = x;
    int baseline = y + font->ascent;
    for_each_char(lpString, c, [&](uint32_t ch, bool full) {
        int cell = char_advance(font, ch, full);
        if (ch != ' ' && ch != 0x3000 && ch != 0)
        {
            draw_glyph(dc, font, ch, pen, cell, baseline);
        }
        pen += cell;
    });
    if (font->logfont.lfUnderline)
    {
        int thickness = font->em / 14 > 1 ? font->em / 14 : 1;
        int top = baseline + (font->cell_height - font->ascent) / 3;
        for (int row = top; row < top + thickness; row++)
        {
            for (int col = x; col < pen; col++)
            {
                blend_pixel(dc->bitmap, col, row, dc->text_color, 255);
            }
        }
    }
    return TRUE;
}

BOOL TextOutA(HDC hdc, int x, int y, LPCSTR lpString, int c)
{
#ifdef TH16_THCRAP
    if (port_thcrap_text_out(hdc, x, y, lpString, c))
    {
        return TRUE;
    }
#endif
    return port_gdi_text_out_raw(hdc, x, y, lpString, c);
}

int port_gdi_text_width(HDC hdc, const char *text, int length)
{
    std::lock_guard<std::recursive_mutex> guard(g_lock);
    GdiDC *dc = as<GdiDC>(hdc, GDI_DC);
    if (dc == NULL || dc->font == NULL || dc->font->face == NULL || length <= 0)
    {
        return 0;
    }
    return text_width(dc->font, text, length);
}

BOOL GetTextExtentPoint32A(HDC hdc, LPCSTR lpString, int c, LPSIZE psizl)
{
    std::lock_guard<std::recursive_mutex> guard(g_lock);
    GdiDC *dc = as<GdiDC>(hdc, GDI_DC);
    if (dc == NULL || dc->font == NULL)
    {
        return FALSE;
    }
    psizl->cx = text_width(dc->font, lpString, c);
    psizl->cy = dc->font->cell_height;
    return TRUE;
}

int port_gdi_bitmap_width(HDC hdc)
{
    std::lock_guard<std::recursive_mutex> guard(g_lock);
    GdiDC *dc = as<GdiDC>(hdc, GDI_DC);
    return dc != NULL && dc->bitmap != NULL && dc->bitmap->bits != NULL ? dc->bitmap->width : 0;
}

HFONT port_gdi_current_font(HDC hdc)
{
    std::lock_guard<std::recursive_mutex> guard(g_lock);
    GdiDC *dc = as<GdiDC>(hdc, GDI_DC);
    return dc != NULL ? (HFONT)dc->font : NULL;
}

bool port_gdi_font_logfont(HFONT font, LOGFONTA *lf)
{
    std::lock_guard<std::recursive_mutex> guard(g_lock);
    GdiFont *f = as<GdiFont>(font, GDI_FONT);
    if (f == NULL)
    {
        return false;
    }
    *lf = f->logfont;
    return true;
}

void port_gdi_set_utf8(bool on)
{
    std::lock_guard<std::recursive_mutex> guard(g_lock);
    g_utf8_text = on;
}

bool port_gdi_add_font_file(const char *path)
{
    std::lock_guard<std::recursive_mutex> guard(g_lock);
    if (!init_freetype() || !g_fontconfig_ready)
    {
        return false;
    }
    return FcConfigAppFontAddFile(FcConfigGetCurrent(), (const FcChar8 *)path) == FcTrue;
}

BOOL GetTextMetricsA(HDC hdc, LPTEXTMETRICA lptm)
{
    std::lock_guard<std::recursive_mutex> guard(g_lock);
    GdiDC *dc = as<GdiDC>(hdc, GDI_DC);
    if (dc == NULL || dc->font == NULL)
    {
        return FALSE;
    }
    GdiFont *font = dc->font;
    memset(lptm, 0, sizeof(*lptm));
    lptm->tmHeight = font->cell_height;
    lptm->tmAscent = font->ascent;
    lptm->tmDescent = font->cell_height - font->ascent;
    lptm->tmInternalLeading = font->cell_height - font->em;
    lptm->tmAveCharWidth = font->half_advance;
    lptm->tmMaxCharWidth = font->em;
    lptm->tmWeight = font->logfont.lfWeight;
    lptm->tmDigitizedAspectX = 96;
    lptm->tmDigitizedAspectY = 96;
    lptm->tmFirstChar = 0x20;
    lptm->tmLastChar = 0xfc;
    lptm->tmDefaultChar = 0x1f;
    lptm->tmBreakChar = 0x20;
    lptm->tmPitchAndFamily = font->logfont.lfPitchAndFamily;
    lptm->tmCharSet = font->logfont.lfCharSet;
    return TRUE;
}

// Reports a face only if the system has that font itself (fontconfig knows
// its family name). The game asks whether Meiryo is there; without it, it
// uses MS Gothic and MS Mincho, which have substitutes.
int EnumFontFamiliesExA(HDC hdc, LPLOGFONTA lpLogfont, FONTENUMPROCA lpProc, LPARAM lParam, DWORD dwFlags)
{
    std::lock_guard<std::recursive_mutex> guard(g_lock);
    std::string name = port_sjis_to_utf8(lpLogfont->lfFaceName);
    if (name.empty() || find_real_face(name, FC_WEIGHT_REGULAR) == NULL)
    {
        return 1;
    }
    ENUMLOGFONTEXA font;
    TEXTMETRICA metrics;
    memset(&font, 0, sizeof(font));
    memset(&metrics, 0, sizeof(metrics));
    font.elfLogFont = *lpLogfont;
    font.elfLogFont.lfCharSet = SHIFTJIS_CHARSET;
    strncpy((char *)font.elfFullName, lpLogfont->lfFaceName, LF_FULLFACESIZE - 1);
    strcpy((char *)font.elfStyle, "Regular");
    return lpProc(&font.elfLogFont, &metrics, 4 /* TRUETYPE_FONTTYPE */, lParam);
}

BOOL GdiFlush(void)
{
    return TRUE;
}

} // extern "C"
