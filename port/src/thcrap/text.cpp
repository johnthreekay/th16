// Text: thcrap's GDI text layout (markup such as "<r$text$width>" for
// right-aligned text, "<c$...$>" centred, "<i$...>" italic), the
// translation of strings at TextOutA, font replacement ("font",
// "fontrules", the patches' own font files), text widths in the game's
// fonts, and the dialogue's ruby offsets and speech bubble widths.
//
// Adapted from thcrap (public domain): thcrap_tsa/src/layout.cpp
// (layout_match, layout_tokenize, layout_parse_tabs, layout_process,
// layout_TextOutU, text_extent_full, GetTextExtentForFontID,
// ruby_offset_half, BP_ruby_offset, font_block_get) and
// thcrap/src/textdisp.cpp (font rules, textdisp_CreateFontA,
// patch_fonts_load); the th15_textbox_size, spell_align and
// result_spell_align binary hacks of base_tsa's th16.v1.00a.js and
// global.js, written out in C.
#include <stdlib.h>
#include <string.h>

#include <mutex>
#include <string>
#include <vector>

#include <windows.h>

#include "TextHelper.h"
#include "../port_platform.h"
#include "port_thcrap.h"
#include "thcrap.h"
#include "thcrap_internal.h"

namespace thcrap
{
namespace
{

std::recursive_mutex g_text_lock;
// Absolute tab stops ("<t$...>" defines them); Layout_Tabs.
std::vector<int> g_tabs;
// The DC text widths are measured with (layout.cpp's text_dc).
HDC g_text_dc;
// layout.cpp's ruby_offset_actual: the real furigana offset, passed to
// TextOutA through a dummy x (RUBY_OFFSET_DUMMY_HALF).
int g_ruby_offset_actual;
const int RUBY_OFFSET_DUMMY_HALF = 32767;
const int RUBY_OFFSET_DUMMY_FULL = RUBY_OFFSET_DUMMY_HALF * 2;

// One piece of a string: plain text, or a layout command
// ["cmds", "text", params...].
struct Token
{
    bool command = false;
    std::vector<std::string> params;
    std::string text;
};

// layout_match: the layout markup at str (str[0] == '<'), split at '$' at
// nesting level 0, up to the closing '>'.
std::vector<std::string> layout_match(size_t &match_len, const char *str, size_t len)
{
    std::vector<std::string> ret;
    if (str == NULL || len == 0 || str[0] != '<')
    {
        return ret;
    }
    size_t i = 1;
    const char *s = str + i;
    const char *p = s;
    int n = 0;
    for (; i < len && n >= 0; i++, p++)
    {
        n += *p == '<';
        n -= *p == '>';
        if ((n == 0 && *p == '$') || (n == -1 && *p == '>'))
        {
            ret.push_back(std::string(s, p - s));
            s = p + 1;
        }
    }
    match_len = s - str;
    return ret;
}

std::vector<Token> layout_tokenize(const char *str, size_t len)
{
    std::vector<Token> ret;
    size_t i = 0;
    while (i < len)
    {
        const char *cur_str = str + i;
        size_t cur_len = len - i;
        std::vector<std::string> match = layout_match(cur_len, cur_str, cur_len);
        // At least two parameters (one '$'), so that "<text>" stays text.
        if (match.size() > 1)
        {
            Token token;
            token.command = true;
            token.params = match;
            ret.push_back(token);
        }
        else
        {
            const char *cmd_start = cur_len > 1 ? (const char *)memchr(cur_str + 1, '<', cur_len - 1) : NULL;
            if (cmd_start != NULL)
            {
                cur_len = cmd_start - cur_str;
            }
            if (cur_str[0] != '\0')
            {
                Token token;
                token.text.assign(cur_str, cur_len);
                ret.push_back(token);
            }
        }
        if (cur_len == 0)
        {
            break;
        }
        i += cur_len;
    }
    return ret;
}

struct Layout
{
    HDC hdc;
    int orig_x;
    int orig_y;
    int bitmap_width = 0;
    std::vector<Token> tokens;
    size_t cur_tab = 0;
    int cur_x = 0;
    size_t token_id = 0;
    const std::string *draw_str = NULL;
    int cur_w = 0;
};

int extent(HDC hdc, const std::string &s)
{
    return port_gdi_text_width(hdc, s.data(), (int)s.size());
}

// layout_parse_font: b(old), i(talic), u(nderline).
bool layout_parse_font(LOGFONTA &lf, const Token &token)
{
    bool ret = false;
    for (char c : token.params[0])
    {
        if (c == 'b')
        {
            lf.lfWeight *= 2;
            ret = true;
        }
        else if (c == 'i')
        {
            lf.lfItalic = TRUE;
            ret = true;
        }
        else if (c == 'u')
        {
            lf.lfUnderline = TRUE;
            ret = true;
        }
    }
    return ret;
}

// layout_parse_tabs: s(kip), t(ab stop), l(eft), c(entre), r(ight).
void layout_parse_tabs(Layout *lay, const Token &token)
{
    size_t tabs_count = g_tabs.size();
    int tab_end;
    const std::string &cmd = token.params[0];
    size_t ret = cmd.size();
    if (token.params.size() > 2)
    {
        const std::string &p2 = token.params[2];
        // An empty second parameter: the whole bitmap.
        tab_end = p2.empty() ? lay->bitmap_width : lay->cur_x + extent(lay->hdc, p2);
    }
    else if (lay->cur_tab < tabs_count)
    {
        tab_end = g_tabs[lay->cur_tab];
    }
    else if (tabs_count > 0 && lay->token_id == lay->tokens.size() - 1)
    {
        tab_end = lay->bitmap_width;
    }
    else
    {
        tab_end = lay->cur_x + lay->cur_w;
    }
    for (char c : cmd)
    {
        switch (c)
        {
        case 's':
            tab_end = lay->cur_x;
            lay->draw_str = NULL;
            break;
        case 't':
            tab_end = lay->cur_w;
            for (size_t j = 2; j < token.params.size(); j++)
            {
                int w = extent(lay->hdc, token.params[j]);
                tab_end = w > tab_end ? w : tab_end;
            }
            tab_end += lay->cur_x;
            if (g_tabs.size() <= lay->cur_tab)
            {
                g_tabs.resize(lay->cur_tab + 1, tab_end);
            }
            g_tabs[lay->cur_tab] = tab_end;
            break;
        case 'l':
            break;
        case 'c':
            lay->cur_x += ((tab_end - lay->cur_x) / 2) - (lay->cur_w / 2);
            break;
        case 'r':
            lay->cur_x = tab_end - lay->cur_w;
            break;
        default:
            ret--;
            break;
        }
    }
    if (ret != 0)
    {
        lay->cur_tab++;
        lay->cur_w = tab_end - lay->cur_x;
    }
}

// tlnote_find: a TL note (U+0014 inline, U+0012 indexed) ends the regular
// text. TL notes are not shown in the port; the text before them is.
size_t without_tlnote(const char *str, size_t len)
{
    for (size_t i = 0; i < len; i++)
    {
        if (str[i] == 0x14 || str[i] == 0x12)
        {
            return i;
        }
    }
    return len;
}

// layout_process: lays out `str` and, with `draw`, draws each run.
// Returns the full width (lay->cur_x).
int layout_process(Layout *lay, bool draw, const char *str, size_t len)
{
    std::lock_guard<std::recursive_mutex> guard(g_text_lock);
    if (lay->hdc == NULL || str == NULL || len == 0)
    {
        return 0;
    }
    if (len >= strlen(str))
    {
        str = strings_lookup(str);
        len = strlen(str);
    }
    len = without_tlnote(str, len);
    lay->bitmap_width = port_gdi_bitmap_width(lay->hdc);
    HFONT font_orig_handle = port_gdi_current_font(lay->hdc);
    LOGFONTA font_orig;
    bool have_font = port_gdi_font_logfont(font_orig_handle, &font_orig);
    lay->tokens = layout_tokenize(str, len);
    for (lay->token_id = 0; lay->token_id < lay->tokens.size(); lay->token_id++)
    {
        const Token &token = lay->tokens[lay->token_id];
        HFONT font_new = NULL;
        if (token.command)
        {
            lay->draw_str = &token.params[1];
            LOGFONTA lf = font_orig;
            if (have_font && layout_parse_font(lf, token))
            {
                font_new = CreateFontIndirectA(&lf);
                SelectObject(lay->hdc, font_new);
            }
            lay->cur_w = extent(lay->hdc, *lay->draw_str);
            layout_parse_tabs(lay, token);
        }
        else
        {
            lay->draw_str = &token.text;
            lay->cur_w = extent(lay->hdc, token.text);
        }
        if (lay->draw_str != NULL && draw)
        {
            port_gdi_text_out_raw(lay->hdc, lay->orig_x + lay->cur_x, lay->orig_y, lay->draw_str->data(),
                                  (int)lay->draw_str->size());
        }
        if (font_new != NULL)
        {
            SelectObject(lay->hdc, font_orig_handle);
            DeleteObject(font_new);
        }
        lay->cur_x += lay->cur_w;
    }
    return lay->cur_x;
}

// font_block_get: base_tsa's tsa_font_block for TH16 is the game's font
// table from g_text_font_0 (ids 0-7, as draw_text numbers its fonts).
HFONT font_block_get(int id)
{
    switch (id)
    {
    case 0:
        return g_text_font_0;
    case 1:
        return g_text_font_1;
    case 2:
        return g_text_font_2;
    case 3:
        return g_text_font_3;
    case 4:
        return g_text_font_4;
    case 5:
        return g_text_font_5;
    case 6:
        return g_text_font_6;
    case 7:
        return g_text_font_7;
    case 8:
        return g_text_font_8;
    }
    return NULL;
}

int font_block_id(json_t *object, const char *key, int fallback)
{
    json_t *value = json_object_get(object, key);
    return json_is_integer(value) ? (int)json_integer_value(value) : fallback;
}

// text_extent_full_for_font.
int text_extent_full_for_font(const char *str, HFONT font)
{
    std::lock_guard<std::recursive_mutex> guard(g_text_lock);
    if (g_text_dc == NULL)
    {
        g_text_dc = CreateCompatibleDC(NULL);
    }
    HGDIOBJ previous = SelectObject(g_text_dc, font);
    Layout lay;
    lay.hdc = g_text_dc;
    lay.orig_x = 0;
    lay.orig_y = 0;
    int width = layout_process(&lay, false, str, strlen(str));
    if (previous != NULL)
    {
        SelectObject(g_text_dc, previous);
    }
    return width;
}

// --- Fonts (textdisp.cpp) ---

const BYTE UNSPECIFIED_QUALITY = 0xff;

const char *const QUALITY_STRINGS[] = {
    "DEFAULT_QUALITY",        "DRAFT_QUALITY",     "PROOF_QUALITY",           "NONANTIALIASED_QUALITY",
    "ANTIALIASED_QUALITY",    "CLEARTYPE_QUALITY", "CLEARTYPE_NATURAL_QUALITY",
};

BYTE string_to_quality(const char *str)
{
    if (str == NULL || str[0] == '\0')
    {
        return UNSPECIFIED_QUALITY;
    }
    for (BYTE i = 0; i < sizeof(QUALITY_STRINGS) / sizeof(*QUALITY_STRINGS); i++)
    {
        if (strcmp(str, QUALITY_STRINGS[i]) == 0)
        {
            return i;
        }
    }
    return UNSPECIFIED_QUALITY;
}

const char *fontrule_arg(char *arg, size_t arg_len, const char *str)
{
    size_t i = 0;
    char delim = ' ';
    if (*str == '\'')
    {
        delim = '\'';
        str++;
    }
    while (i < arg_len - 1 && *str && *str != delim)
    {
        arg[i++] = *str++;
    }
    arg[i] = '\0';
    while (*str && *str != ' ')
    {
        str++;
    }
    while (*str && *str == ' ')
    {
        str++;
    }
    return str;
}

int fontrule_arg_int(const char **str, int *score)
{
    char arg[LF_FACESIZE] = {0};
    *str = fontrule_arg(arg, sizeof(arg), *str);
    int ret = atoi(arg);
    *score += ret != 0;
    return ret;
}

int fontrule_parse(LOGFONTA *lf, const char *str)
{
    int score = 0;
    if (*str == '\'')
    {
        str = fontrule_arg(lf->lfFaceName, LF_FACESIZE, str);
        score++;
    }
    else
    {
        lf->lfPitchAndFamily = (BYTE)fontrule_arg_int(&str, &score);
        lf->lfFaceName[0] = '\0';
    }
    lf->lfHeight = fontrule_arg_int(&str, &score);
    lf->lfWidth = fontrule_arg_int(&str, &score);
    lf->lfWeight = fontrule_arg_int(&str, &score);
    char arg[LF_FACESIZE] = {0};
    str = fontrule_arg(arg, sizeof(arg), str);
    lf->lfQuality = string_to_quality(arg);
    score += lf->lfQuality != UNSPECIFIED_QUALITY;
    return score;
}

bool fontrule_match(const LOGFONTA *rule, const LOGFONTA *dst)
{
    if (rule->lfFaceName[0] && strncmp(rule->lfFaceName, dst->lfFaceName, LF_FACESIZE) != 0)
    {
        return false;
    }
    return !(rule->lfPitchAndFamily != 0 && rule->lfPitchAndFamily != dst->lfPitchAndFamily) &&
           !(rule->lfHeight != 0 && rule->lfHeight != dst->lfHeight) &&
           !(rule->lfWidth != 0 && rule->lfWidth != dst->lfWidth) &&
           !(rule->lfWeight != 0 && rule->lfWeight != dst->lfWeight) &&
           !(rule->lfQuality != UNSPECIFIED_QUALITY && rule->lfQuality != dst->lfQuality);
}

void fontrule_apply(LOGFONTA *dst, const LOGFONTA *rep, bool priority)
{
    if (rep->lfFaceName[0] && (priority || !dst->lfFaceName[0]))
    {
        strncpy(dst->lfFaceName, rep->lfFaceName, LF_FACESIZE);
    }
    if (rep->lfPitchAndFamily != 0 && (priority || !dst->lfPitchAndFamily))
    {
        dst->lfPitchAndFamily = rep->lfPitchAndFamily;
    }
    if (rep->lfHeight != 0 && (priority || !dst->lfHeight))
    {
        dst->lfHeight = rep->lfHeight;
    }
    if (rep->lfWidth != 0 && (priority || !dst->lfWidth))
    {
        dst->lfWidth = rep->lfWidth;
    }
    if (rep->lfWeight != 0 && (priority || !dst->lfWeight))
    {
        dst->lfWeight = rep->lfWeight;
    }
    if (rep->lfQuality != UNSPECIFIED_QUALITY && (priority || !dst->lfQuality))
    {
        dst->lfQuality = rep->lfQuality;
    }
}

} // namespace

void text_init()
{
    port_gdi_set_utf8(true);
    // patch_fonts_load: the patches' font files.
    for (const Patch &patch : stack())
    {
        for (const std::string &font : patch.fonts)
        {
            std::string path = patch_file_path(patch, font);
            if (!path.empty() && port_gdi_add_font_file(path.c_str()))
            {
                log("font %s%s", patch.archive.c_str(), font.c_str());
            }
        }
    }
}

int text_extent_for_font_id(const char *str, int font_id)
{
    HFONT font = font_block_get(font_id);
    return font != NULL ? text_extent_full_for_font(str, font) : 0;
}

} // namespace thcrap

using namespace thcrap;

void port_thcrap_font_face(char face[LF_FACESIZE])
{
    if (!port_thcrap_active())
    {
        return;
    }
    // Hardcoded string translation first, then the run configuration's
    // "font" (textdisp_CreateFontA).
    char original[LF_FACESIZE + 1] = {0};
    memcpy(original, face, LF_FACESIZE);
    const char *name = strings_lookup(original);
    if (name == original)
    {
        name = json_string_value(json_object_get(runconfig(), "font"));
    }
    if (name != NULL && name != original)
    {
        memset(face, 0, LF_FACESIZE);
        strncpy(face, name, LF_FACESIZE - 1);
    }
}

void port_thcrap_font_rules(LOGFONTA *lf)
{
    if (!port_thcrap_active())
    {
        return;
    }
    json_t *fontrules = json_object_get(runconfig(), "fontrules");
    LOGFONTA rep_full;
    memset(&rep_full, 0, sizeof(rep_full));
    rep_full.lfQuality = UNSPECIFIED_QUALITY;
    int rep_score = 0;
    const char *key;
    json_t *value;
    json_object_foreach(fontrules, key, value)
    {
        LOGFONTA rule;
        memset(&rule, 0, sizeof(rule));
        int rule_score = fontrule_parse(&rule, key);
        bool priority = rule_score >= rep_score;
        const char *rep_str = json_string_value(value);
        if (rep_str == NULL || !fontrule_match(&rule, lf))
        {
            continue;
        }
        fontrule_parse(&rule, rep_str);
        fontrule_apply(&rep_full, &rule, priority);
        rep_score = rule_score > rep_score ? rule_score : rep_score;
    }
    fontrule_apply(lf, &rep_full, true);
    if (lf->lfFaceName[0])
    {
        lf->lfCharSet = DEFAULT_CHARSET;
    }
}

bool port_thcrap_text_out(HDC hdc, int x, int y, const char *text, int length)
{
    if (!port_thcrap_active())
    {
        return false;
    }
    // A furigana drawn at the dummy offset (keeping the outline's shifts).
    {
        std::lock_guard<std::recursive_mutex> guard(g_text_lock);
        if (x > RUBY_OFFSET_DUMMY_FULL / 2)
        {
            x = (x - RUBY_OFFSET_DUMMY_FULL) + g_ruby_offset_actual;
        }
    }
    Layout lay;
    lay.hdc = hdc;
    lay.orig_x = x;
    lay.orig_y = y;
    layout_process(&lay, true, text, length);
    return true;
}

int32_t port_thcrap_ruby_offset(const char **params, int32_t x)
{
    // BP_ruby_offset: "|\tbefore\t,\tbase\t,ruby" (str = params).
    if (!port_thcrap_breakpoint("ruby_offset"))
    {
        return x;
    }
    const char *str = *params;
    if (str == NULL || str[0] != '\t')
    {
        return x;
    }
    json_t *bp_info = json_object_get(json_object_get(runconfig(), "breakpoints"), "ruby_offset");
    HFONT font_dialog = font_block_get(font_block_id(bp_info, "font_dialog", -1));
    HFONT font_ruby = font_block_get(font_block_id(bp_info, "font_ruby", -1));
    if (font_dialog == NULL || font_ruby == NULL)
    {
        return x;
    }
    const char *offset_start = str + 1;
    const char *offset_end = strchr(offset_start, '\t');
    if (offset_end == NULL || offset_end[1] != ',' || offset_end[2] != '\t')
    {
        return x;
    }
    const char *base_start = offset_end + 3;
    const char *base_end = strchr(base_start, '\t');
    if (base_end == NULL || base_end[1] != ',')
    {
        return x;
    }
    std::string before(offset_start, offset_end);
    std::string base(base_start, base_end);
    std::string ruby(base_end + 2);
    // ruby_offset_half, at twice the precision.
    int offset_double = text_extent_full_for_font(before.c_str(), font_dialog) * 2 +
                        text_extent_full_for_font(base.c_str(), font_dialog) -
                        text_extent_full_for_font(ruby.c_str(), font_ruby);
    {
        std::lock_guard<std::recursive_mutex> guard(g_text_lock);
        // The ruby sprite's on-screen shift in TH15 and TH16.
        g_ruby_offset_actual = (offset_double + 1) / 2 + 4;
    }
    // So that the game's two strchr(',') calls find the ruby text: the
    // last comma before the end of the base text.
    const char *comma = base_end;
    while (comma > str && *(--comma) != ',')
    {
    }
    *params = comma;
    return RUBY_OFFSET_DUMMY_HALF;
}

float port_thcrap_textbox_width(const char *text, float width)
{
    // th15_textbox_size: (GetTextExtentForFontID(text, 0) - 28, at least
    // 0) * 2, where the game uses (strlen / 2 * 16 - 28) * 2.
    if (!port_thcrap_binhack("th15_textbox_size"))
    {
        return width;
    }
    int half = text_extent_for_font_id(text, 0) / 2 - 28;
    return (float)(half < 0 ? 0 : half) * 2.0f;
}

int32_t port_thcrap_text_right_x(const char *text, int32_t font, float sprite_width, int32_t x)
{
    // spell_align: sprite width - (GetTextExtentForFontID + 8) * 2.
    if (!port_thcrap_binhack("spell_align"))
    {
        return x;
    }
    int half = text_extent_for_font_id(text, font) / 2;
    return (int32_t)(sprite_width - (float)((half + 8) * 2));
}

int32_t port_thcrap_text_centered_x(int32_t x)
{
    // result_spell_align: the result screen's text starts at 0 (its
    // layout markup aligns the columns).
    return port_thcrap_binhack("result_spell_align") ? 0 : x;
}
