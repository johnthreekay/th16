// Spell card names (spells.js) and the music room (themes.js,
// musiccmt.js): what thcrap's spell_id, spell_name, music_title and
// music_cmt breakpoints do at the places base_tsa's th16.v1.00a.js puts
// them (EnemyData::ecl_run_over_300, Spellcard::start,
// TitleInf::draw_spell_card_page, TitleInf::load_spell_list,
// TitleInf::do_music_room).
//
// Adapted from thcrap (public domain): thcrap_tsa/src/spells.cpp
// (BP_spell_id, BP_spell_name) and music.cpp (music_title_get,
// music_title_print, BP_music_title, BP_music_cmt).
#include <stdio.h>
#include <string.h>

#include <mutex>
#include <string>

#include "port_thcrap.h"
#include "thcrap_internal.h"

namespace thcrap
{
namespace
{

std::mutex g_hooks_lock;
// The ECL spell instruction's id, before the difficulty is added.
int32_t g_spell_id;
bool g_spell_id_set;
// Formatted music titles, kept for the game (strings_sprintf's slot 0).
std::string g_music_title_storage[2];

// Spell ids from real_id down to base_id, the first one spells.js has.
const char *spell_lookup(int32_t real_id, int32_t base_id)
{
    json_t *spells = jsondata_game_get("spells.js");
    if (spells == NULL)
    {
        return NULL;
    }
    for (int32_t id = real_id; id >= base_id; id--)
    {
        char key[16];
        snprintf(key, sizeof(key), "%d", id);
        if (const char *name = json_string_value(json_object_get(spells, key)))
        {
            return name;
        }
    }
    return NULL;
}

// music_title_get: themes.js "th16_NN" (1-based).
const char *music_title_get(int32_t track)
{
    char key[32];
    snprintf(key, sizeof(key), "%s_%02d", game_id(), track);
    return json_string_value(json_object_get(jsondata_get("themes.js"), key));
}

// music_title_print: the title with the stringdefs format ("Music Room
// Numbered Title" for the list, "Music Room Note Title" in the comment),
// or the bare title.
const char *music_title_print(const char *str, const char *format_id, int32_t track, int slot)
{
    const char *format = json_string_value(strings_get(format_id));
    const char *title = music_title_get(track);
    if (title == NULL)
    {
        return str;
    }
    if (format == NULL)
    {
        return title;
    }
    char buf[1024];
    snprintf(buf, sizeof(buf), format, track, title);
    std::lock_guard<std::mutex> guard(g_hooks_lock);
    g_music_title_storage[slot] = buf;
    return g_music_title_storage[slot].c_str();
}

const char *breakpoint_string(const char *name, const char *key)
{
    return json_string_value(json_object_get(json_object_get(json_object_get(runconfig(), "breakpoints"), name), key));
}

} // namespace
} // namespace thcrap

using namespace thcrap;

void port_thcrap_spell_id(int32_t spell_id)
{
    std::lock_guard<std::mutex> guard(g_hooks_lock);
    g_spell_id = spell_id;
    g_spell_id_set = true;
}

const char *port_thcrap_spell_name(int32_t real_id, const char *name)
{
    if (!port_thcrap_breakpoint("spell_name"))
    {
        return name;
    }
    int32_t base_id;
    {
        std::lock_guard<std::mutex> guard(g_hooks_lock);
        base_id = g_spell_id_set && g_spell_id <= real_id ? g_spell_id : real_id;
    }
    const char *translated = spell_lookup(real_id, base_id);
    return translated != NULL ? translated : name;
}

const char *port_thcrap_spell_name_ranked(int32_t real_id, int32_t rank, const char *name)
{
    if (!port_thcrap_active())
    {
        return name;
    }
    const char *translated = spell_lookup(real_id, real_id - rank);
    return translated != NULL ? translated : name;
}

const char *port_thcrap_music_title(int32_t track, const char *title)
{
    if (!port_thcrap_breakpoint("music_title"))
    {
        return title;
    }
    const char *format_id = breakpoint_string("music_title", "format_id");
    return music_title_print(title, format_id != NULL ? format_id : "", track + 1, 0);
}

const char *port_thcrap_music_comment(int32_t track, int32_t line, const char *comment)
{
    if (!port_thcrap_breakpoint("music_cmt") || comment == NULL)
    {
        return comment;
    }
    char key[16];
    snprintf(key, sizeof(key), "%d", track + 1);
    json_t *cmt = json_object_get(jsondata_game_get("musiccmt.js"), key);
    if (!json_is_array(cmt))
    {
        return comment;
    }
    const char *line_text = json_string_value(json_array_get(cmt, line));
    if (line_text == NULL)
    {
        line_text = "";
    }
    // "@": the title, formatted for the comment.
    if (strcmp(line_text, "@") == 0)
    {
        const char *format_id = breakpoint_string("music_cmt", "format_id");
        return music_title_print(comment, format_id != NULL ? format_id : "", track + 1, 1);
    }
    return line_text;
}
