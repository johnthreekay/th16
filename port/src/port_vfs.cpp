// The Windows file system the game sees: see port_vfs.h.
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <mutex>
#include <string>
#include <vector>

#include <windows.h>

#include "port_platform.h"
#include "port_vfs.h"

namespace
{

std::string g_game_dir;
std::string g_save_dir;

std::mutex g_cwd_lock;
std::string g_cwd = PORT_VFS_EXE_DIR;

bool equals_ignoring_case(const std::string &a, const char *b)
{
    return strcasecmp(a.c_str(), b) == 0;
}

// A component's name on the host: Shift-JIS names become UTF-8.
std::string host_name(const std::string &name)
{
    return port_sjis_to_utf8(name.c_str(), (int)name.size());
}

// The host name of a directory entry as the game sees it (Shift-JIS);
// false for names that have no Shift-JIS form.
bool game_name(const char *name, std::string *out)
{
    for (const char *p = name; *p != '\0'; p++)
    {
        if ((unsigned char)*p >= 0x80)
        {
            return port_utf8_to_sjis(name, out);
        }
    }
    *out = name;
    return true;
}

PortVfsKind kind_of(const std::string &host_path)
{
    struct stat st;
    if (stat(host_path.c_str(), &st) != 0)
    {
        return PORT_VFS_NONE;
    }
    return S_ISDIR(st.st_mode) ? PORT_VFS_DIRECTORY : PORT_VFS_FILE;
}

// The entry of directory `dir` called `name` ignoring case: the exact name
// if it exists, else the first entry that matches.
bool find_entry(const std::string &dir, const std::string &name, std::string *out)
{
    std::string exact = dir + "/" + name;
    struct stat st;
    if (lstat(exact.c_str(), &st) == 0)
    {
        *out = exact;
        return true;
    }
    DIR *d = opendir(dir.c_str());
    if (d == NULL)
    {
        return false;
    }
    bool found = false;
    while (struct dirent *entry = readdir(d))
    {
        if (strcasecmp(entry->d_name, name.c_str()) == 0)
        {
            *out = dir + "/" + entry->d_name;
            found = true;
            break;
        }
    }
    closedir(d);
    return found;
}

// Looks `path` (its first `count` components) up in one folder.
PortVfsKind lookup_in(const std::string &root, const PortVfsPath &path, size_t count, std::string *out)
{
    std::string current = root;
    for (size_t i = 0; i < count; i++)
    {
        std::string next;
        if (!find_entry(current, host_name(path[i]), &next))
        {
            return PORT_VFS_NONE;
        }
        if (i + 1 < count && kind_of(next) != PORT_VFS_DIRECTORY)
        {
            return PORT_VFS_NONE;
        }
        current = next;
    }
    PortVfsKind kind = kind_of(current);
    if (kind != PORT_VFS_NONE && out != NULL)
    {
        *out = current;
    }
    return kind;
}

bool copy_file(const std::string &from, const std::string &to)
{
    int in = open(from.c_str(), O_RDONLY | O_CLOEXEC);
    if (in < 0)
    {
        return false;
    }
    int out = open(to.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    if (out < 0)
    {
        close(in);
        return false;
    }
    char buffer[65536];
    bool ok = true;
    for (;;)
    {
        ssize_t n = read(in, buffer, sizeof(buffer));
        if (n < 0 && errno == EINTR)
        {
            continue;
        }
        if (n <= 0)
        {
            ok = n == 0;
            break;
        }
        for (ssize_t done = 0; done < n;)
        {
            ssize_t w = write(out, buffer + done, n - done);
            if (w < 0 && errno == EINTR)
            {
                continue;
            }
            if (w <= 0)
            {
                ok = false;
                break;
            }
            done += w;
        }
        if (!ok)
        {
            break;
        }
    }
    close(in);
    close(out);
    return ok;
}

bool mkdir_p(const std::string &path)
{
    if (path.empty())
    {
        return true;
    }
    struct stat st;
    if (stat(path.c_str(), &st) == 0)
    {
        return S_ISDIR(st.st_mode);
    }
    size_t slash = path.rfind('/');
    if (slash != std::string::npos && slash > 0 && !mkdir_p(path.substr(0, slash)))
    {
        return false;
    }
    return mkdir(path.c_str(), 0755) == 0 || errno == EEXIST;
}

// Splits a Windows path into its drive letter ('\0' if none), whether it is
// rooted, and its components with "." and ".." resolved.
void split_path(const char *path, char *drive, bool *rooted, std::vector<std::string> *parts)
{
    *drive = '\0';
    if (isalpha((unsigned char)path[0]) && path[1] == ':')
    {
        *drive = (char)toupper((unsigned char)path[0]);
        path += 2;
    }
    *rooted = path[0] == '\\' || path[0] == '/';
    std::string part;
    for (const char *p = path;; p++)
    {
        if (*p == '\\' || *p == '/' || *p == '\0')
        {
            if (part == "..")
            {
                if (!parts->empty())
                {
                    parts->pop_back();
                }
            }
            else if (!part.empty() && part != ".")
            {
                parts->push_back(part);
            }
            part.clear();
            if (*p == '\0')
            {
                break;
            }
        }
        else
        {
            part.push_back(*p);
        }
    }
}

// An absolute, normalised Windows path on drive C: its components.
bool absolute_components(const char *windows_path, std::vector<std::string> *parts)
{
    char drive;
    bool rooted;
    std::vector<std::string> own;
    split_path(windows_path, &drive, &rooted, &own);
    if (drive != '\0' && drive != 'C')
    {
        return false;
    }
    if (drive == '\0' && !rooted)
    {
        // Relative to the current directory.
        std::string cwd = port_vfs_get_cwd();
        char cwd_drive;
        bool cwd_rooted;
        split_path(cwd.c_str(), &cwd_drive, &cwd_rooted, parts);
        std::string joined;
        for (const std::string &part : *parts)
        {
            joined += "\\" + part;
        }
        joined += "\\";
        joined += windows_path;
        parts->clear();
        split_path(joined.c_str(), &drive, &rooted, parts);
        return true;
    }
    *parts = own;
    return true;
}

} // namespace

bool port_vfs_init(const std::string &game_dir, const std::string &save_dir)
{
    g_game_dir = game_dir;
    g_save_dir = save_dir;
    while (g_game_dir.size() > 1 && g_game_dir.back() == '/')
    {
        g_game_dir.pop_back();
    }
    while (g_save_dir.size() > 1 && g_save_dir.back() == '/')
    {
        g_save_dir.pop_back();
    }
    std::lock_guard<std::mutex> guard(g_cwd_lock);
    g_cwd = PORT_VFS_EXE_DIR;
    if (!mkdir_p(g_save_dir))
    {
        port_log("cannot create the save folder %s: %s", g_save_dir.c_str(), strerror(errno));
        return false;
    }
    return true;
}

const char *port_game_dir(void)
{
    return g_game_dir.c_str();
}

const char *port_save_dir(void)
{
    return g_save_dir.c_str();
}

bool port_vfs_map(const char *windows_path, PortVfsPath *path)
{
    std::vector<std::string> parts;
    path->clear();
    if (windows_path == NULL || !absolute_components(windows_path, &parts))
    {
        SetLastError(ERROR_PATH_NOT_FOUND);
        return false;
    }
    size_t skip = 0;
    if (parts.empty())
    {
        // C:\ itself.
    }
    else if (equals_ignoring_case(parts[0], "th16"))
    {
        skip = 1;
    }
    else if (equals_ignoring_case(parts[0], "AppData"))
    {
        // %APPDATA%, %APPDATA%\ShanghaiAlice and the save directory below
        // them are all the root.
        skip = 1;
        if (parts.size() > 1 && equals_ignoring_case(parts[1], "ShanghaiAlice"))
        {
            skip = 2;
            if (parts.size() > 2 && equals_ignoring_case(parts[2], "th16"))
            {
                skip = 3;
            }
        }
    }
    else
    {
        SetLastError(ERROR_PATH_NOT_FOUND);
        return false;
    }
    path->assign(parts.begin() + skip, parts.end());
    return true;
}

PortVfsKind port_vfs_lookup(const PortVfsPath &path, std::string *host_path)
{
    PortVfsKind kind = lookup_in(g_save_dir, path, path.size(), host_path);
    if (kind != PORT_VFS_NONE)
    {
        return kind;
    }
    return lookup_in(g_game_dir, path, path.size(), host_path);
}

PortVfsKind port_vfs_lookup_writable(const PortVfsPath &path, std::string *host_path)
{
    return lookup_in(g_save_dir, path, path.size(), host_path);
}

std::string port_vfs_writable(const PortVfsPath &path, bool copy_existing)
{
    if (path.empty())
    {
        SetLastError(ERROR_ACCESS_DENIED);
        return "";
    }
    std::string current = g_save_dir;
    for (size_t i = 0; i + 1 < path.size(); i++)
    {
        std::string next;
        if (find_entry(current, host_name(path[i]), &next))
        {
            if (kind_of(next) != PORT_VFS_DIRECTORY)
            {
                SetLastError(ERROR_PATH_NOT_FOUND);
                return "";
            }
            current = next;
            continue;
        }
        // Only in the game folder: create it here, with that spelling.
        std::string lower;
        if (lookup_in(g_game_dir, path, i + 1, &lower) != PORT_VFS_DIRECTORY)
        {
            SetLastError(ERROR_PATH_NOT_FOUND);
            return "";
        }
        next = current + lower.substr(lower.rfind('/'));
        if (mkdir(next.c_str(), 0755) != 0 && errno != EEXIST)
        {
            SetLastError(ERROR_ACCESS_DENIED);
            return "";
        }
        current = next;
    }
    std::string target;
    if (find_entry(current, host_name(path.back()), &target))
    {
        return target;
    }
    target = current + "/" + host_name(path.back());
    std::string lower;
    if (copy_existing && lookup_in(g_game_dir, path, path.size(), &lower) == PORT_VFS_FILE)
    {
        target = current + lower.substr(lower.rfind('/'));
        if (!copy_file(lower, target))
        {
            SetLastError(ERROR_ACCESS_DENIED);
            return "";
        }
    }
    return target;
}

bool port_vfs_list(const PortVfsPath &path, std::vector<PortVfsEntry> *entries)
{
    entries->clear();
    bool exists = false;
    const std::string *roots[2] = {&g_save_dir, &g_game_dir};
    for (const std::string *root : roots)
    {
        std::string dir;
        if (lookup_in(*root, path, path.size(), &dir) != PORT_VFS_DIRECTORY)
        {
            continue;
        }
        exists = true;
        DIR *d = opendir(dir.c_str());
        if (d == NULL)
        {
            continue;
        }
        while (struct dirent *entry = readdir(d))
        {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            {
                continue;
            }
            PortVfsEntry item;
            if (!game_name(entry->d_name, &item.name))
            {
                continue;
            }
            bool duplicate = false;
            for (const PortVfsEntry &other : *entries)
            {
                if (strcasecmp(other.name.c_str(), item.name.c_str()) == 0)
                {
                    duplicate = true;
                    break;
                }
            }
            if (duplicate)
            {
                continue;
            }
            item.host_path = dir + "/" + entry->d_name;
            item.directory = kind_of(item.host_path) == PORT_VFS_DIRECTORY;
            entries->push_back(item);
        }
        closedir(d);
    }
    std::sort(entries->begin(), entries->end(), [](const PortVfsEntry &a, const PortVfsEntry &b) {
        return strcasecmp(a.name.c_str(), b.name.c_str()) < 0;
    });
    return exists;
}

std::string port_vfs_get_cwd()
{
    std::lock_guard<std::mutex> guard(g_cwd_lock);
    return g_cwd;
}

bool port_vfs_set_cwd(const char *windows_path)
{
    std::vector<std::string> parts;
    PortVfsPath path;
    if (windows_path == NULL || windows_path[0] == '\0' || !absolute_components(windows_path, &parts) ||
        !port_vfs_map(windows_path, &path))
    {
        SetLastError(ERROR_PATH_NOT_FOUND);
        return false;
    }
    if (!path.empty() && port_vfs_lookup(path, NULL) != PORT_VFS_DIRECTORY)
    {
        SetLastError(ERROR_PATH_NOT_FOUND);
        return false;
    }
    std::string cwd = "C:";
    for (const std::string &part : parts)
    {
        cwd += "\\" + part;
    }
    if (parts.empty())
    {
        cwd += "\\";
    }
    std::lock_guard<std::mutex> guard(g_cwd_lock);
    g_cwd = cwd;
    return true;
}

bool port_vfs_match(const char *pattern, const char *name)
{
    // "*.*" matches names without a dot too, as on Windows.
    if (strcmp(pattern, "*.*") == 0)
    {
        return true;
    }
    const char *star = NULL;
    const char *star_name = NULL;
    while (*name != '\0')
    {
        if (*pattern == '*')
        {
            star = pattern++;
            star_name = name;
        }
        else if (*pattern == '?' || tolower((unsigned char)*pattern) == tolower((unsigned char)*name))
        {
            pattern++;
            name++;
        }
        else if (star != NULL)
        {
            pattern = star + 1;
            name = ++star_name;
        }
        else
        {
            return false;
        }
    }
    while (*pattern == '*')
    {
        pattern++;
    }
    return *pattern == '\0';
}

std::string port_host_path(const char *windows_path)
{
    PortVfsPath path;
    std::string host;
    if (!port_vfs_map(windows_path, &path) || port_vfs_lookup(path, &host) == PORT_VFS_NONE)
    {
        return "";
    }
    return host;
}
