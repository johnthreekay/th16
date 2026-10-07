// Tests of the thcrap support (port/src/thcrap/) without the game or its
// data: JSON5, the patch stack from a thcrap folder the test writes
// (./thcrap_test_dir), the game configuration, string translation and the
// translating printf, spell names, the music room, file replacement, the
// .msg patcher, layout markup, ruby offsets and bubble widths. GDI is a
// stand-in here: every byte of text is 10 pixels wide, and TextOutA
// records what it draws.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <string>
#include <vector>

#include <windows.h>

#include "port_thcrap.h"
#include "../src/thcrap/thcrap.h"
#include "../src/thcrap/thcrap_internal.h"

// --- Stand-ins for what the module uses from the game and the platform ---

HFONT g_text_font_default = (HFONT)0x100;
HFONT g_text_font_8 = (HFONT)0x108;
HFONT g_text_font_0 = (HFONT)0x110;
HFONT g_text_font_1 = (HFONT)0x111;
HFONT g_text_font_2 = (HFONT)0x112;
HFONT g_text_font_3 = (HFONT)0x113;
HFONT g_text_font_4 = (HFONT)0x114;
HFONT g_text_font_5 = (HFONT)0x115;
HFONT g_text_font_6 = (HFONT)0x116;
HFONT g_text_font_7 = (HFONT)0x117;

namespace
{
HFONT g_selected = (HFONT)0x110;
int g_bitmap_width = 1000;
struct Drawn
{
    int x;
    std::string text;
    bool italic;
};
std::vector<Drawn> g_drawn;
bool g_italic;
} // namespace

void port_log(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    fprintf(stderr, "[log] ");
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    va_end(args);
}

extern "C" {
BOOL port_gdi_text_out_raw(HDC, int x, int, const char *text, int length)
{
    g_drawn.push_back({x, std::string(text, length), g_italic});
    return TRUE;
}
int port_gdi_text_width(HDC, const char *, int length)
{
    // Font 2 (the ruby font) is half as wide.
    return length * (g_selected == g_text_font_2 ? 5 : 10);
}
int port_gdi_bitmap_width(HDC)
{
    return g_bitmap_width;
}
HFONT port_gdi_current_font(HDC)
{
    return g_selected;
}
bool port_gdi_font_logfont(HFONT, LOGFONTA *lf)
{
    memset(lf, 0, sizeof(*lf));
    lf->lfHeight = 32;
    lf->lfWeight = 400;
    lf->lfItalic = g_italic;
    return true;
}
void port_gdi_set_utf8(bool)
{
}
bool port_gdi_add_font_file(const char *)
{
    return true;
}
HFONT CreateFontIndirectA(const LOGFONTA *lf)
{
    return lf->lfItalic ? (HFONT)0x200 : (HFONT)0x201;
}
HGDIOBJ SelectObject(HDC, HGDIOBJ h)
{
    HGDIOBJ previous = g_selected;
    g_selected = (HFONT)h;
    g_italic = h == (HGDIOBJ)0x200;
    return previous;
}
BOOL DeleteObject(HGDIOBJ)
{
    return TRUE;
}
HDC CreateCompatibleDC(HDC)
{
    return (HDC)0x300;
}
}

namespace
{

int g_failures;

#define CHECK(cond)                                                                                                  \
    do                                                                                                               \
    {                                                                                                                \
        if (!(cond))                                                                                                 \
        {                                                                                                            \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                                         \
            g_failures++;                                                                                            \
        }                                                                                                            \
    } while (0)

#define CHECK_STR(a, b)                                                                                              \
    do                                                                                                               \
    {                                                                                                                \
        std::string a_ = (a), b_ = (b);                                                                              \
        if (a_ != b_)                                                                                                \
        {                                                                                                            \
            fprintf(stderr, "FAIL %s:%d: \"%s\" != \"%s\"\n", __FILE__, __LINE__, a_.c_str(), b_.c_str());           \
            g_failures++;                                                                                            \
        }                                                                                                            \
    } while (0)

void write_file(const std::string &path, const std::string &data)
{
    std::string dir;
    for (size_t i = 0; i < path.size(); i++)
    {
        if (path[i] == '/' && i > 0)
        {
            mkdir(path.substr(0, i).c_str(), 0755);
        }
    }
    FILE *f = fopen(path.c_str(), "wb");
    fwrite(data.data(), 1, data.size(), f);
    fclose(f);
}

std::string dump(json_t *json)
{
    char *s = json_dumps(json, JSON_COMPACT | JSON_SORT_KEYS);
    std::string out = s != NULL ? s : "(null)";
    free(s);
    json_decref(json);
    return out;
}

void test_json5()
{
    std::string error;
    const char *text = "// comment\n{ a: 1, 'b': 'x\\'y', /* c */ \"c\": [0x10, +2, .5, 3.,], d: { e: null, }, }";
    CHECK_STR(dump(thcrap::json5_loadb(text, strlen(text), &error)),
              "{\"a\":1,\"b\":\"x'y\",\"c\":[16,2,0.5,3.0],\"d\":{\"e\":null}}");
    const char *bad = "{a: }";
    CHECK(thcrap::json5_loadb(bad, strlen(bad), &error) == NULL);
    const char *utf8 = "{\"s\": \"\xe6\x9d\xb1\\u3000\"}";
    CHECK_STR(dump(thcrap::json5_loadb(utf8, strlen(utf8), &error)), "{\"s\":\"\xe6\x9d\xb1\xe3\x80\x80\"}");
}

void test_wildcards()
{
    CHECK(thcrap::wildcard_match("s*.msg", "st01a.msg"));
    CHECK(thcrap::wildcard_match("e*.msg", "E01.MSG"));
    CHECK(!thcrap::wildcard_match("s*.msg", "e01.msg"));
    CHECK(thcrap::wildcard_match("*.anm", "front.anm"));
    CHECK(thcrap::wildcard_match("th16/?ront.*", "th16\\front.anm"));
}

// A .msg file with one entry: a line at time 34, the box's end at 40.
std::string make_msg(const char *line)
{
    std::string msg;
    auto u32 = [&](uint32_t v) { msg.append((const char *)&v, 4); };
    auto instr = [&](uint16_t time, uint8_t type, const std::string &data) {
        msg.append((const char *)&time, 2);
        msg += (char)type;
        msg += (char)data.size();
        msg += data;
    };
    u32(1);
    u32(12); // entry 0 at offset 12
    u32(0);
    std::string text(line);
    text += '\0';
    for (size_t i = 0; i < text.size(); i++)
    {
        const size_t ip = i - 1;
        text[i] ^= (char)(0x77 + i * 7 + (ip * ip + ip) / 2 * 16);
    }
    instr(34, 17, text);
    instr(40, 11, "");
    instr(0, 0, "");
    return msg;
}

std::string decrypt(const uint8_t *data, size_t len)
{
    std::string text((const char *)data, len);
    for (size_t i = 0; i < text.size(); i++)
    {
        const size_t ip = i - 1;
        text[i] ^= (char)(0x77 + i * 7 + (ip * ip + ip) / 2 * 16);
    }
    return std::string(text.c_str());
}

void test_stack()
{
    const std::string root = "thcrap_test_dir";
    write_file(root + "/config/config.js", "{\"console\": false}");
    write_file(root + "/config/test.js", "{\"patches\": [{\"archive\": \"repos/t/base/\"}, "
                                         "{\"archive\": \"repos/t/lang/\"}, {\"archive\": \"repos/t/other/\"}], "
                                         "\"binhacks\": {\"score_force_visual_update\": {\"ignore\": true}}}");
    write_file(root + "/repos/t/base/patch.js", "{\"id\": \"base\"}");
    write_file(root + "/repos/t/base/global.js",
               "{\"binhacks\": {\"spell_align\": {\"code\": \"90\"}}, \"breakpoints\": {\"ruby_offset\": "
               "{\"font_dialog\": 0, \"font_ruby\": 2}}}");
    write_file(root + "/repos/t/base/th16.js",
               "{\"title\": \"Title\", \"breakpoints\": {\"music_title\": {\"format_id\": \"Numbered\"}, "
               "\"music_cmt\": {\"format_id\": \"Note\"}}}");
    write_file(root + "/repos/t/base/th16.v1.00a.js",
               "{\"binhacks\": {\"th15_textbox_size\": {\"addr\": [\"Rx2a5d0\", \"Rx2a7c6\"],},"
               " \"fix_satono_1\": {\"addr\": \"Rx21596\", \"ignore\": true}, \"spell_align\": {\"addr\": "
               "\"Rx6db40\"}, \"score_force_visual_update\": {\"addr\": \"Rx2d7b3\"}}, \"breakpoints\": {\"spell_name\": {\"addr\": \"Rx180d6\"}, \"music_title\": "
               "{\"addr\": \"Rx54af3\"}, \"music_cmt\": {\"addr\": [\"Rx54d59\"]}, \"ruby_offset\": "
               "{\"addr\": \"Rx2a53a\"}}}");
    // "Stage 1" (0x49290c) and the full-width digit 0 (0x493330).
    write_file(root + "/repos/t/base/th16/stringlocs.v1.00a.js",
               "{\"Rx9290c\": \"stage_1\", \"Rx93330\": \"digit_0\"}");
    write_file(root + "/repos/t/lang/patch.js", "{\"id\": \"lang\"}");
    write_file(root + "/repos/t/lang/stringdefs.js",
               "{\"stage_1\": \"Etapa 1\", \"digit_0\": \"0\", \"Numbered\": \"No.%2d %s\", \"Note\": \"<ls$%d>*%s\"}");
    write_file(root + "/repos/t/lang/themes.js", "{\"th16_01\": \"Theme One\"}");
    write_file(root + "/repos/t/lang/th16/musiccmt.js", "{\"1\": [\"@\", \"\", \"Comment\"]}");
    write_file(root + "/repos/t/lang/th16/spells.js", "{\"0\": \"Sign \\\"Zero\\\"\", \"4\": \"Sign \\\"Four\\\"\"}");
    write_file(root + "/repos/t/lang/th16/data.bin", "replaced");
    write_file(root + "/repos/t/lang/th16/st01a.msg.jdiff",
               "{\"0\": {\"34_0\": {\"lines\": [\"Hello there,\", \"second <i$line$>\"]}}}");
    // A patch for another game only: its files must not count.
    write_file(root + "/repos/t/other/patch.js", "{\"id\": \"other\", \"supported_games\": [\"th06\"]}");
    write_file(root + "/repos/t/other/th16/data.bin", "wrong");

    CHECK(port_thcrap_init(root.c_str(), "test"));
    CHECK(port_thcrap_active());
    CHECK(thcrap::stack().size() == 2);
    CHECK(port_thcrap_binhack("th15_textbox_size"));
    CHECK(port_thcrap_binhack("spell_align"));
    CHECK(!port_thcrap_binhack("fix_satono_1"));
    CHECK(!port_thcrap_binhack("meiryo_disable"));
    // Switched off by the run configuration.
    CHECK(!port_thcrap_binhack("score_force_visual_update"));
    CHECK(port_thcrap_breakpoint("spell_name"));

    // Strings, by content.
    CHECK_STR(port_thcrap_string("Stage 1"), "Etapa 1");
    CHECK_STR(port_thcrap_string("Stage 2"), "Stage 2");
    char buf[64];
    port_thcrap_snprintf(buf, sizeof(buf), "Stage 1");
    CHECK_STR(buf, "Etapa 1");
    port_thcrap_snprintf(buf, sizeof(buf), "[%s|%3d|%9ld|%.1f|%%|%-4s]", "\x82O", 7, 12345, 2.25, "ab");
    CHECK_STR(buf, "[0|  7|    12345|2.2|%|ab  ]");
    port_thcrap_snprintf(buf, 6, "%s", "Stage 1");
    CHECK_STR(buf, "Etapa");

    // Spell names: down from the real id to the ECL's (or by rank).
    port_thcrap_spell_id(4);
    CHECK_STR(port_thcrap_spell_name(5, "orig"), "Sign \"Four\"");
    port_thcrap_spell_id(2);
    CHECK_STR(port_thcrap_spell_name(3, "orig"), "orig");
    CHECK_STR(port_thcrap_spell_name_ranked(1, 1, "orig"), "Sign \"Zero\"");
    CHECK_STR(port_thcrap_spell_name_ranked(1, 0, "orig"), "orig");

    // Music room.
    CHECK_STR(port_thcrap_music_title(0, "orig"), "No. 1 Theme One");
    CHECK_STR(port_thcrap_music_title(1, "orig"), "orig");
    CHECK_STR(port_thcrap_music_comment(0, 0, "orig"), "<ls$1>*Theme One");
    CHECK_STR(port_thcrap_music_comment(0, 2, "orig"), "Comment");
    CHECK_STR(port_thcrap_music_comment(0, 5, "orig"), "");
    CHECK_STR(port_thcrap_music_comment(1, 0, "orig"), "orig");

    // Files: replacement from the last patch with it.
    uint32_t size = 0;
    uint8_t *data = port_thcrap_file_replacement("data.bin", &size);
    CHECK(data != NULL && size == 8 && memcmp(data, "replaced", 8) == 0);
    free(data);
    CHECK(port_thcrap_file_replacement("missing.bin", &size) == NULL);

    // .msg: the line replaced, the extra one inserted before the box ends.
    std::string msg = make_msg("\x82\xa0");
    size = (uint32_t)msg.size();
    data = (uint8_t *)malloc(size);
    memcpy(data, msg.data(), size);
    data = port_thcrap_patch_file("st01a.msg", data, &size);
    CHECK(size > msg.size());
    const uint8_t *p = data + 12;
    CHECK(p[0] == 34 && p[2] == 17);
    CHECK_STR(decrypt(p + 4, p[3]), "Hello there,");
    p += 4 + p[3];
    CHECK(p[0] == 34 && p[2] == 17);
    CHECK_STR(decrypt(p + 4, p[3]), "second <i$line$>");
    p += 4 + p[3];
    CHECK(p[0] == 40 && p[2] == 11);
    p += 4 + p[3];
    CHECK(p[0] == 0 && p[2] == 0);
    free(data);

    // Layout: an italic run, right alignment to a width, centring in the
    // bitmap.
    g_drawn.clear();
    port_thcrap_text_out((HDC)0x400, 2, 2, "ab<i$cd$>ef", 11);
    CHECK(g_drawn.size() == 3);
    if (g_drawn.size() == 3)
    {
        CHECK(g_drawn[0].x == 2 && g_drawn[0].text == "ab" && !g_drawn[0].italic);
        CHECK(g_drawn[1].x == 22 && g_drawn[1].text == "cd" && g_drawn[1].italic);
        CHECK(g_drawn[2].x == 42 && g_drawn[2].text == "ef" && !g_drawn[2].italic);
    }
    g_drawn.clear();
    port_thcrap_text_out((HDC)0x400, 0, 0, "No.<r$7$999>", 12);
    CHECK(g_drawn.size() == 2 && g_drawn[1].x == 30 + 30 - 10);
    g_drawn.clear();
    port_thcrap_text_out((HDC)0x400, 0, 0, "<c$abcd$>", 9);
    CHECK(g_drawn.size() == 1 && g_drawn[0].x == 500 - 20);
    g_drawn.clear();
    port_thcrap_text_out((HDC)0x400, 0, 0, "Stage 1", 7);
    CHECK(g_drawn.size() == 1 && g_drawn[0].text == "Etapa 1");

    // Bubble width: (text width / 2 - 28) * 2, at least 0.
    CHECK(port_thcrap_textbox_width("0123456789", 123.0f) == (50 - 28) * 2.0f);
    CHECK(port_thcrap_textbox_width("ab", 123.0f) == 0.0f);
    // spell_align: sprite width - (width / 2 + 8) * 2.
    CHECK(port_thcrap_text_right_x("abcd", 0, 500.0f, 7) == 500 - (20 + 8) * 2);
    CHECK(port_thcrap_text_centered_x(77) == 77);

    // Ruby: "|\tbefore\t,\tbase\t,ruby"; the offset goes through TextOutA.
    const char *line = "|\tA \t,\tyamanba\t,mountain hag";
    const char *params = line + 1;
    int x = port_thcrap_ruby_offset(&params, 0);
    CHECK(x == 32767);
    const char *rest = strchr(params, ',') + 1;
    CHECK(atoi(rest) == 0);
    rest = strchr(rest, ',');
    CHECK_STR(rest + 1, "mountain hag");
    g_drawn.clear();
    port_thcrap_text_out((HDC)0x400, x * 2 + 2, 0, rest + 1, (int)strlen(rest + 1));
    // (2 * 20 + 70 - 60 + 1) / 2 + 4 = 29, plus draw_text's 2.
    CHECK(g_drawn.size() == 1 && g_drawn[0].x == 2 + 29);

    // The window title.
    std::string title = "original";
    port_thcrap_window_title("\x93\x8c\x95\xfb", &title);
    CHECK_STR(title, "Title v1.00a");
}

} // namespace

int main()
{
    test_json5();
    test_wildcards();
    test_stack();
    if (g_failures != 0)
    {
        fprintf(stderr, "%d failures\n", g_failures);
        return 1;
    }
    printf("thcrap: all tests passed\n");
    return 0;
}
