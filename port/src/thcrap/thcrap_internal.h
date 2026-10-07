// What the parts of the port's thcrap support share (port/src/thcrap/).
// The names follow thcrap's own (thcrap/src: stack.cpp, patchfile.cpp,
// runconfig.cpp, strings.cpp, jsondata.cpp) so that the code reads
// alongside it; thcrap is public domain (UNLICENSE), and what is adapted
// from it says so where it is.
#pragma once

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include <jansson.h>

namespace thcrap
{

// "[th16-port] thcrap: ..." on stderr.
void log(const char *format, ...) __attribute__((format(printf, 1, 2)));

// --- JSON (json5.cpp) ---

// Parses JSON5 text (thcrap's patch file syntax). NULL on errors, with a
// message in *error.
json_t *json5_loadb(const char *text, size_t length, std::string *error);
// Loads a JSON5 file (UTF-8, or UTF-16LE as thcrap allows). NULL when the
// file is missing or invalid (logged). *size: the file's size.
json_t *json5_load_file(const std::string &path, size_t *size);

bool read_host_file(const std::string &path, std::string *data);

// --- The patch stack and run configuration (stack.cpp) ---

struct Patch
{
    // Absolute folder, ending in '/'.
    std::string archive;
    std::string id;
    // Wildcards of files the run configuration blacklists for this patch.
    std::vector<std::string> ignore;
    // Font files (patch.js "fonts") to register.
    std::vector<std::string> fonts;
};

const std::vector<Patch> &stack();
// The merged game configuration: every patch's global.js, th16.js and
// th16.v1.00a.js (and the patch's run configuration "config"), in stack
// order, under the run configuration file's own keys.
json_t *runconfig();
const char *game_id();  // "th16"
const char *game_build(); // "v1.00a"

// thcrap's resolution chains: [fn, fn with ".v1.00a" before the first dot
// of its base name] and the same under "th16/".
std::vector<std::string> resolve_chain(const std::string &fn);
std::vector<std::string> resolve_chain_game(const std::string &fn);
// A patch's file (honouring its ignore list); false if absent.
bool patch_file_load(const Patch &patch, const std::string &fn, std::string *data);
bool patch_file_exists(const Patch &patch, const std::string &fn);
std::string patch_file_path(const Patch &patch, const std::string &fn);
// JSON merged over the whole stack (later patches and build-specific files
// win, objects merge recursively). New reference or NULL. *size: the total
// size of the files merged.
json_t *stack_json_resolve(const std::string &fn, size_t *size);
json_t *stack_game_json_resolve(const std::string &fn, size_t *size);
// The last patch's file in the chain for "th16/fn" (build-specific first).
bool stack_game_file_resolve(const std::string &fn, std::string *data);

// Files resolved once at startup, as thcrap's jsondata keeps them:
// "stringdefs.js", "themes.js", "th16/musiccmt.js", "th16/spells.js".
json_t *jsondata_get(const char *fn);
json_t *jsondata_game_get(const char *fn);

// Whether runconfig()'s "binhacks"/"breakpoints" enable `name` (it has an
// address and is not ignored).
bool hackpoint_enabled(const char *kind, const char *name);

// --- Strings (strings.cpp) ---

void strings_init();
// The stringdefs translation of a string the game uses (by content: the
// stringlocs address names a string of the original th16.exe, and
// th16_strings.inc gives each such address's text), or `in`.
const char *strings_lookup(const char *in);
const json_t *strings_get(const char *id);
// strings_vsprintf: the format and every %s argument looked up, then
// formatted.
std::string strings_vsprintf(const char *format, va_list args);

// --- Format patchers (msg.cpp, anm.cpp); thcrap's func_patch_t ---

typedef int (*PatchFunc)(void *file_inout, size_t size_out, size_t size_in, const char *fn, json_t *patch);
int patch_msg_dlg(void *file_inout, size_t size_out, size_t size_in, const char *fn, json_t *patch);
int patch_msg_end(void *file_inout, size_t size_out, size_t size_in, const char *fn, json_t *patch);
int patch_anm(void *file_inout, size_t size_out, size_t size_in, const char *fn, json_t *patch);

// --- Text (text.cpp) ---

void text_init();
// Width in pixels of `str` with font `font_id` of the game's font block
// (TextHelper.cpp: g_text_font_0 ...; tsa_font_block), layout markup
// applied: thcrap's text_extent_full_for_font.
int text_extent_for_font_id(const char *str, int font_id);

// thcrap's PathMatchSpec subset: '*' and '?', case-insensitive, '/' and
// '\\' alike.
bool wildcard_match(const char *pattern, const char *name);

} // namespace thcrap
