// The thcrap folder, run configuration and patch stack; file resolution the
// way thcrap does it; the game configuration (global.js, th16.js,
// th16.v1.00a.js merged over the stack); the file hook of file_read_all.
//
// Adapted from thcrap (public domain): thcrap/src/stack.cpp (resolution
// chains, stack_json_resolve, stack_file_resolve_chain), patchfile.cpp
// (fn_for_build, fn_for_game, patch_init, patchhooks), runconfig.cpp and
// init.cpp (stack_cfg_resolve), jsondata.cpp, bp_file.cpp (file_rep_*).
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "../port_platform.h"
#include "port_thcrap.h"
#include "thcrap.h"
#include "thcrap_internal.h"

namespace thcrap
{
namespace
{

bool g_active;
std::string g_thcrap_dir;
std::string g_runcfg_path;
std::vector<Patch> g_stack;
json_t *g_runconfig;
std::map<std::string, json_t *> g_jsondata;

const char GAME[] = "th16";
const char BUILD[] = "v1.00a";

bool is_dir(const std::string &path)
{
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

bool is_file(const std::string &path)
{
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

std::string slash_normalize(std::string path)
{
    for (char &c : path)
    {
        if (c == '\\')
        {
            c = '/';
        }
    }
    return path;
}

// A file name of the patch, found ignoring case like on Windows (patch
// repositories are made on Windows; names usually match exactly, which is
// tried first).
std::string find_case_insensitive(const std::string &base, const std::string &relative)
{
    std::string exact = base + relative;
    struct stat st;
    if (stat(exact.c_str(), &st) == 0)
    {
        return exact;
    }
    std::string dir = base.empty() ? "/" : base;
    size_t start = 0;
    while (start <= relative.size())
    {
        size_t slash = relative.find('/', start);
        std::string part = relative.substr(start, slash == std::string::npos ? std::string::npos : slash - start);
        if (!part.empty())
        {
            DIR *d = opendir(dir.c_str());
            if (d == NULL)
            {
                return "";
            }
            std::string found;
            while (struct dirent *e = readdir(d))
            {
                if (strcasecmp(e->d_name, part.c_str()) == 0)
                {
                    found = e->d_name;
                    break;
                }
            }
            closedir(d);
            if (found.empty())
            {
                return "";
            }
            if (dir.back() != '/')
            {
                dir += '/';
            }
            dir += found;
        }
        if (slash == std::string::npos)
        {
            break;
        }
        start = slash + 1;
    }
    return dir;
}

std::vector<std::string> string_array(json_t *array)
{
    std::vector<std::string> out;
    size_t i;
    json_t *value;
    json_array_foreach(array, i, value)
    {
        if (json_is_string(value))
        {
            out.push_back(json_string_value(value));
        }
    }
    return out;
}

// thcrap's patch_init: patch.js merged with the run configuration's entry.
bool patch_init(json_t *entry, Patch *patch)
{
    const char *archive = json_string_value(json_object_get(entry, "archive"));
    if (archive == NULL)
    {
        return false;
    }
    std::string path = slash_normalize(archive);
    if (path.empty() || path[0] != '/')
    {
        path = g_thcrap_dir + "/" + path;
    }
    if (path.back() != '/')
    {
        path += '/';
    }
    patch->archive = path;
    if (!is_dir(path))
    {
        log("patch %s not found", path.c_str());
        return false;
    }
    json_t *patch_js = json5_load_file(path + "patch.js", NULL);
    if (patch_js == NULL)
    {
        patch_js = json_object();
    }
    json_object_update_recursive(patch_js, entry);
    const char *id = json_string_value(json_object_get(patch_js, "id"));
    if (id != NULL)
    {
        patch->id = id;
    }
    else
    {
        std::string trimmed = path.substr(0, path.size() - 1);
        patch->id = trimmed.substr(trimmed.rfind('/') + 1);
    }
    patch->ignore = string_array(json_object_get(patch_js, "ignore"));
    const char *key;
    json_t *value;
    json_object_foreach(json_object_get(patch_js, "fonts"), key, value)
    {
        patch->fonts.push_back(key);
    }
    // stack_prune_patches: patches that name their games and not th16.
    json_t *games = json_object_get(patch_js, "supported_games");
    bool supported = true;
    if (json_is_array(games))
    {
        supported = false;
        for (const std::string &game : string_array(games))
        {
            supported |= game == GAME;
        }
    }
    json_decref(patch_js);
    return supported;
}

// stack_cfg_resolve (init.cpp): global.js, th16.js and th16.v1.00a.js of
// every patch, then its run configuration "config", merged in stack order.
json_t *game_config(json_t *runcfg)
{
    json_t *result = json_object();
    std::vector<std::string> files = {"global.js"};
    for (const std::string &fn : resolve_chain(std::string(GAME) + ".js"))
    {
        files.push_back(fn);
    }
    json_t *entries = json_object_get(runcfg, "patches");
    for (size_t i = 0; i < g_stack.size(); i++)
    {
        json_t *patch_json = json_object();
        for (const std::string &fn : files)
        {
            std::string path = patch_file_path(g_stack[i], fn);
            if (path.empty())
            {
                continue;
            }
            if (json_t *json = json5_load_file(path, NULL))
            {
                json_object_update_recursive(patch_json, json);
                json_decref(json);
            }
        }
        // The entry of this patch in the run configuration.
        size_t j;
        json_t *entry;
        json_array_foreach(entries, j, entry)
        {
            const char *archive = json_string_value(json_object_get(entry, "archive"));
            if (archive == NULL)
            {
                continue;
            }
            std::string path = slash_normalize(archive);
            if (path.empty() || path[0] != '/')
            {
                path = g_thcrap_dir + "/" + path;
            }
            if (path.back() != '/')
            {
                path += '/';
            }
            if (path == g_stack[i].archive)
            {
                json_object_update_recursive(patch_json, json_object_get(entry, "config"));
            }
        }
        json_object_update_recursive(result, patch_json);
        json_decref(patch_json);
    }
    return result;
}

std::string default_thcrap_dir()
{
    std::string dir;
    if (const char *data = getenv("XDG_DATA_HOME"))
    {
        if (data[0] != '\0')
        {
            dir = std::string(data) + "/thcrap";
            if (is_dir(dir))
            {
                return dir;
            }
        }
    }
    if (const char *home = getenv("HOME"))
    {
        dir = std::string(home) + "/.local/share/thcrap";
        if (is_dir(dir))
        {
            return dir;
        }
    }
    return "";
}

// The newest config/*.js that is a run configuration (has "patches");
// thcrap's own settings (config.js, games.js) are not.
std::string newest_run_config(const std::string &dir)
{
    std::string config_dir = dir + "/config";
    DIR *d = opendir(config_dir.c_str());
    if (d == NULL)
    {
        return "";
    }
    std::string best;
    time_t best_time = 0;
    while (struct dirent *e = readdir(d))
    {
        size_t len = strlen(e->d_name);
        if (len < 4 || strcasecmp(e->d_name + len - 3, ".js") != 0)
        {
            continue;
        }
        std::string path = config_dir + "/" + e->d_name;
        struct stat st;
        if (stat(path.c_str(), &st) != 0 || !S_ISREG(st.st_mode))
        {
            continue;
        }
        json_t *json = json5_load_file(path, NULL);
        bool is_run_config = json_is_array(json_object_get(json, "patches"));
        json_decref(json);
        if (is_run_config && (best.empty() || st.st_mtime > best_time))
        {
            best = path;
            best_time = st.st_mtime;
        }
    }
    closedir(d);
    return best;
}

void jsondata_add(const std::string &fn)
{
    json_t *json = stack_json_resolve(fn, NULL);
    if (json != NULL)
    {
        g_jsondata[fn] = json;
    }
}

} // namespace

void log(const char *format, ...)
{
    char message[2048];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    port_log("thcrap: %s", message);
}

bool read_host_file(const std::string &path, std::string *data)
{
    FILE *f = fopen(path.c_str(), "rb");
    if (f == NULL)
    {
        return false;
    }
    data->clear();
    char buf[65536];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
    {
        data->append(buf, n);
    }
    fclose(f);
    return true;
}

bool wildcard_match(const char *pattern, const char *name)
{
    for (;;)
    {
        char p = *pattern;
        char n = *name;
        if (p == '\0')
        {
            return n == '\0';
        }
        if (p == '*')
        {
            pattern++;
            for (const char *s = name;; s++)
            {
                if (wildcard_match(pattern, s))
                {
                    return true;
                }
                if (*s == '\0')
                {
                    return false;
                }
            }
        }
        if (n == '\0')
        {
            return false;
        }
        char pl = p == '\\' ? '/' : (char)tolower((unsigned char)p);
        char nl = n == '\\' ? '/' : (char)tolower((unsigned char)n);
        if (p != '?' && pl != nl)
        {
            return false;
        }
        pattern++;
        name++;
    }
}

const std::vector<Patch> &stack()
{
    return g_stack;
}

json_t *runconfig()
{
    return g_runconfig;
}

const char *game_id()
{
    return GAME;
}

const char *game_build()
{
    return BUILD;
}

std::vector<std::string> resolve_chain(const std::string &fn)
{
    std::vector<std::string> chain = {fn};
    // fn_for_build: the build goes before the first dot of the base name.
    size_t base = fn.find_last_of("/\\");
    base = base == std::string::npos ? 0 : base + 1;
    size_t dot = fn.find('.', base);
    if (dot != std::string::npos)
    {
        chain.push_back(fn.substr(0, dot + 1) + BUILD + fn.substr(dot));
    }
    else
    {
        chain.push_back(fn + "." + BUILD);
    }
    return chain;
}

std::vector<std::string> resolve_chain_game(const std::string &fn)
{
    return resolve_chain(std::string(GAME) + "/" + fn);
}

std::string patch_file_path(const Patch &patch, const std::string &fn)
{
    for (const std::string &pattern : patch.ignore)
    {
        if (wildcard_match(pattern.c_str(), fn.c_str()))
        {
            return "";
        }
    }
    std::string path = find_case_insensitive(patch.archive, slash_normalize(fn));
    return !path.empty() && is_file(path) ? path : "";
}

bool patch_file_exists(const Patch &patch, const std::string &fn)
{
    return !patch_file_path(patch, fn).empty();
}

bool patch_file_load(const Patch &patch, const std::string &fn, std::string *data)
{
    std::string path = patch_file_path(patch, fn);
    return !path.empty() && read_host_file(path, data);
}

json_t *stack_json_resolve(const std::string &fn, size_t *size)
{
    json_t *result = NULL;
    size_t total = 0;
    std::vector<std::string> chain = resolve_chain(fn);
    for (const Patch &patch : g_stack)
    {
        for (const std::string &name : chain)
        {
            std::string path = patch_file_path(patch, name);
            if (path.empty())
            {
                continue;
            }
            size_t file_size = 0;
            json_t *json = json5_load_file(path, &file_size);
            if (json == NULL)
            {
                continue;
            }
            total += file_size;
            if (result == NULL || json_object_update_recursive(result, json) != 0)
            {
                // Not both objects: the newer value replaces the older.
                json_decref(result);
                result = json;
            }
            else
            {
                json_decref(json);
            }
        }
    }
    if (size != NULL)
    {
        *size = total;
    }
    return result;
}

json_t *stack_game_json_resolve(const std::string &fn, size_t *size)
{
    return stack_json_resolve(std::string(GAME) + "/" + fn, size);
}

bool stack_game_file_resolve(const std::string &fn, std::string *data)
{
    // Later patches first, and the build-specific file before the generic
    // one (stack_file_resolve_chain).
    std::vector<std::string> chain = resolve_chain_game(fn);
    for (size_t i = g_stack.size(); i-- > 0;)
    {
        for (size_t j = chain.size(); j-- > 0;)
        {
            if (patch_file_load(g_stack[i], chain[j], data))
            {
                log("%s: replaced by %s%s", fn.c_str(), g_stack[i].archive.c_str(), chain[j].c_str());
                return true;
            }
        }
    }
    return false;
}

json_t *jsondata_get(const char *fn)
{
    auto found = g_jsondata.find(fn);
    return found != g_jsondata.end() ? found->second : NULL;
}

json_t *jsondata_game_get(const char *fn)
{
    return jsondata_get((std::string(GAME) + "/" + fn).c_str());
}

bool hackpoint_enabled(const char *kind, const char *name)
{
    json_t *hack = json_object_get(json_object_get(g_runconfig, kind), name);
    if (!json_is_object(hack) || json_is_true(json_object_get(hack, "ignore")))
    {
        return false;
    }
    json_t *addr = json_object_get(hack, "addr");
    return (json_is_string(addr) && json_string_length(addr) != 0) ||
           (json_is_array(addr) && json_array_size(addr) != 0);
}

} // namespace thcrap

using namespace thcrap;

bool port_thcrap_init(const char *dir_option, const char *config_option)
{
    const char *env = getenv("TH16_THCRAP");
    if (env != NULL && (strcmp(env, "0") == 0 || strcasecmp(env, "off") == 0 || strcasecmp(env, "no") == 0))
    {
        return false;
    }
    std::string dir = dir_option != NULL && dir_option[0] != '\0' ? dir_option : "";
    if (dir.empty() && getenv("TH16_THCRAP_DIR") != NULL)
    {
        dir = getenv("TH16_THCRAP_DIR");
    }
    if (dir.empty())
    {
        dir = default_thcrap_dir();
        if (dir.empty())
        {
            return false;
        }
    }
    while (dir.size() > 1 && dir.back() == '/')
    {
        dir.pop_back();
    }
    if (!is_dir(dir))
    {
        log("folder %s not found; no patches", dir.c_str());
        return false;
    }
    g_thcrap_dir = dir;

    std::string config = config_option != NULL && config_option[0] != '\0' ? config_option : "";
    if (config.empty() && getenv("TH16_THCRAP_CONFIG") != NULL)
    {
        config = getenv("TH16_THCRAP_CONFIG");
    }
    if (!config.empty())
    {
        std::string path = config;
        if (!is_file(path))
        {
            path = dir + "/config/" + config;
            if (!is_file(path))
            {
                path += ".js";
            }
        }
        if (!is_file(path))
        {
            log("run configuration %s not found in %s/config; no patches", config.c_str(), dir.c_str());
            return false;
        }
        g_runcfg_path = path;
    }
    else
    {
        g_runcfg_path = newest_run_config(dir);
        if (g_runcfg_path.empty())
        {
            log("no run configuration in %s/config; no patches", dir.c_str());
            return false;
        }
    }
    json_t *runcfg = json5_load_file(g_runcfg_path, NULL);
    if (runcfg == NULL)
    {
        return false;
    }

    size_t i;
    json_t *entry;
    json_array_foreach(json_object_get(runcfg, "patches"), i, entry)
    {
        Patch patch;
        if (patch_init(entry, &patch))
        {
            g_stack.push_back(patch);
        }
    }
    // runconfig_load(full_cfg, RUNCONFIG_NO_OVERWRITE): the run
    // configuration's own keys stay.
    g_runconfig = game_config(runcfg);
    json_object_update(g_runconfig, runcfg);
    json_decref(runcfg);

    // Does anything in the stack target th16? (stack_check_if_unneeded:
    // th16.js, th16/, th16.v1.00a.js.)
    bool has_game = false;
    for (const Patch &patch : g_stack)
    {
        has_game |= patch_file_exists(patch, std::string(GAME) + ".js") ||
                    is_dir(find_case_insensitive(patch.archive, GAME)) ||
                    patch_file_exists(patch, std::string(GAME) + "." + BUILD + ".js");
    }
    std::string ids;
    for (const Patch &patch : g_stack)
    {
        ids += (ids.empty() ? "" : ", ") + patch.id;
    }
    log("run configuration %s: %s", g_runcfg_path.c_str(), ids.empty() ? "(no patches)" : ids.c_str());
    if (!has_game)
    {
        log("no patch in the stack is for th16; running unpatched");
        g_stack.clear();
        return false;
    }

    jsondata_add("stringdefs.js");
    jsondata_add("themes.js");
    jsondata_add(std::string(GAME) + "/musiccmt.js");
    jsondata_add(std::string(GAME) + "/spells.js");
    jsondata_add(std::string(GAME) + "/spellcomments.js");
    strings_init();
    text_init();
    g_active = true;
    return true;
}

bool port_thcrap_active()
{
    return g_active;
}

bool port_thcrap_binhack(const char *name)
{
    return g_active && hackpoint_enabled("binhacks", name);
}

bool port_thcrap_breakpoint(const char *name)
{
    return g_active && hackpoint_enabled("breakpoints", name);
}

// --- Files: thcrap's file_size/file_load/file_loaded breakpoints ---

namespace
{

struct PatchHook
{
    const char *wildcard;
    PatchFunc func;
    // Whether the hook may grow the file by up to the jdiff's size (the
    // default patch_size_func); the ANM patcher keeps the size on TH16.
    bool grows;
};

// thcrap_tsa's thcrap_plugin_init (thcrap_tsa.cpp), the patterns that
// apply to TH16.
const PatchHook PATCH_HOOKS[] = {
    {"s*.msg", patch_msg_dlg, true},
    {"e*.msg", patch_msg_end, true},
    {"*.anm", patch_anm, false},
};

} // namespace

uint8_t *port_thcrap_file_replacement(const char *name, uint32_t *size)
{
    if (!g_active)
    {
        return NULL;
    }
    std::string data;
    if (!stack_game_file_resolve(name, &data) || data.empty())
    {
        return NULL;
    }
    uint8_t *buffer = (uint8_t *)malloc(data.size());
    if (buffer == NULL)
    {
        return NULL;
    }
    memcpy(buffer, data.data(), data.size());
    *size = (uint32_t)data.size();
    return buffer;
}

uint8_t *port_thcrap_patch_file(const char *name, uint8_t *data, uint32_t *size)
{
    if (!g_active || data == NULL)
    {
        return data;
    }
    std::vector<const PatchHook *> hooks;
    for (const PatchHook &hook : PATCH_HOOKS)
    {
        if (wildcard_match(hook.wildcard, name))
        {
            hooks.push_back(&hook);
        }
    }
    if (hooks.empty())
    {
        return data;
    }
    size_t diff_size = 0;
    json_t *patch = stack_game_json_resolve(std::string(name) + ".jdiff", &diff_size);
    size_t size_in = *size;
    size_t size_out = size_in;
    for (const PatchHook *hook : hooks)
    {
        size_out += hook->grows ? diff_size : 0;
    }
    if (size_out != size_in)
    {
        uint8_t *grown = (uint8_t *)realloc(data, size_out);
        if (grown == NULL)
        {
            json_decref(patch);
            return data;
        }
        memset(grown + size_in, 0, size_out - size_in);
        data = grown;
    }
    for (const PatchHook *hook : hooks)
    {
        if (hook->func(data, size_out, size_in, name, patch) > 0 && patch != NULL)
        {
            log("%s: patched", name);
        }
    }
    json_decref(patch);
    *size = (uint32_t)size_out;
    return data;
}
