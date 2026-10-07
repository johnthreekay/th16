// The Windows file system the game sees, on the host (port_vfs.cpp).
//
// The game runs from PORT_VFS_EXE_PATH and saves under
// PORT_VFS_APPDATA\ShanghaiAlice\th16. Both directories are the root of one
// namespace on the host: the save folder over the game folder. A file is
// looked up in the save folder, then in the game folder; files are only
// ever created, written and deleted in the save folder (a file of the game
// folder opened for writing is copied there first), and directories the
// game creates are created there. Names are matched case-insensitively,
// separators may be '\' or '/', and non-ASCII (Shift-JIS) names are
// converted to UTF-8 on the host.
//
// Paths outside these two directories (other drives, other folders under
// C:\) do not exist.
#pragma once

#include <string>
#include <vector>

#include <windows.h>

#define PORT_VFS_EXE_DIR "C:\\th16"
#define PORT_VFS_EXE_PATH "C:\\th16\\th16.exe"
#define PORT_VFS_APPDATA "C:\\AppData"

// Sets the host folders (absolute, without trailing slash; the save folder
// is created if needed) and the current directory (PORT_VFS_EXE_DIR).
bool port_vfs_init(const std::string &game_dir, const std::string &save_dir);

// A path of the namespace: its components below the root ("replay",
// "th16_01.rpy"). Empty for the root itself.
typedef std::vector<std::string> PortVfsPath;

// Maps a Windows path (absolute, or relative to the current directory) to
// a namespace path. False if it is outside the namespace (sets
// ERROR_PATH_NOT_FOUND).
bool port_vfs_map(const char *windows_path, PortVfsPath *path);

enum PortVfsKind
{
    PORT_VFS_NONE,
    PORT_VFS_FILE,
    PORT_VFS_DIRECTORY,
};

// Finds an existing file or directory: the save folder's if it has one,
// else the game folder's. Returns its host path and kind.
PortVfsKind port_vfs_lookup(const PortVfsPath &path, std::string *host_path);

// The host path for creating or replacing `path` in the save folder. The
// parent directories are created there when the namespace has them (in
// either folder); with copy_existing, a file that only exists in the game
// folder is copied over first. "" if the parent directory does not exist
// (ERROR_PATH_NOT_FOUND) or creation fails.
std::string port_vfs_writable(const PortVfsPath &path, bool copy_existing);

// Whether `path` exists in the save folder (deletable), with its host path.
PortVfsKind port_vfs_lookup_writable(const PortVfsPath &path, std::string *host_path);

struct PortVfsEntry
{
    std::string name; // Shift-JIS, as the game sees it
    std::string host_path;
    bool directory;
};

// The entries of a directory of the namespace (both folders; the save
// folder's entry wins when both have a name), sorted by name ignoring case
// as NTFS does. False if the directory does not exist.
bool port_vfs_list(const PortVfsPath &path, std::vector<PortVfsEntry> *entries);

// The current directory (a Windows path), for _chdir/_getcwd and
// GetCurrentDirectoryA/SetCurrentDirectoryA.
std::string port_vfs_get_cwd();
// Changes it; false (and nothing changes) if the directory does not exist.
bool port_vfs_set_cwd(const char *windows_path);

// Windows wildcard matching ('*' and '?'), ignoring ASCII case.
bool port_vfs_match(const char *pattern, const char *name);
