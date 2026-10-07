// kernel32: the handle table, files, directory searches, errors, modules
// and the environment. Paths go through port_vfs (the game folder under the
// save folder, see port_vfs.h); file HANDLEs are host file descriptors.
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <windows.h>

#include "port_kernel.h"
#include "port_platform.h"
#include "port_stub.h"
#include "port_vfs.h"

static thread_local DWORD g_last_error;

// ---------------------------------------------------------------------------
// Handle table

namespace
{

std::mutex g_handle_lock;
std::vector<std::shared_ptr<PortObject>> g_handles;

struct PortFile : PortObject
{
    int fd;
    bool can_read;
    bool can_write;

    PortFile(int fd, bool can_read, bool can_write)
        : PortObject(PORT_OBJECT_FILE), fd(fd), can_read(can_read), can_write(can_write)
    {
    }
    ~PortFile() override
    {
        close(fd);
    }
};

struct PortFind : PortObject
{
    std::vector<PortVfsEntry> entries;
    size_t next = 0;

    PortFind() : PortObject(PORT_OBJECT_FIND)
    {
    }
};

std::shared_ptr<PortFile> get_file(HANDLE handle)
{
    std::shared_ptr<PortObject> object = port_handle_get(handle, PORT_OBJECT_FILE);
    if (object == NULL)
    {
        SetLastError(ERROR_INVALID_HANDLE);
    }
    return std::static_pointer_cast<PortFile>(object);
}

DWORD error_from_errno(int error)
{
    switch (error)
    {
    case ENOENT:
        return ERROR_FILE_NOT_FOUND;
    case ENOTDIR:
        return ERROR_PATH_NOT_FOUND;
    case EACCES:
    case EPERM:
    case EROFS:
    case EISDIR:
        return ERROR_ACCESS_DENIED;
    case EEXIST:
        return ERROR_ALREADY_EXISTS;
    case ENOSPC:
        return ERROR_DISK_FULL;
    case ENOMEM:
        return ERROR_NOT_ENOUGH_MEMORY;
    case ENOTEMPTY:
        return ERROR_DIR_NOT_EMPTY;
    default:
        return ERROR_ACCESS_DENIED;
    }
}

// Seconds since 1970 to 100 ns units since 1601.
void to_filetime(time_t t, long nsec, FILETIME *out)
{
    uint64_t value = ((uint64_t)t + 11644473600ull) * 10000000ull + (uint64_t)nsec / 100;
    out->dwLowDateTime = (DWORD)value;
    out->dwHighDateTime = (DWORD)(value >> 32);
}

void fill_find_data(const PortVfsEntry &entry, LPWIN32_FIND_DATAA data)
{
    memset(data, 0, sizeof(*data));
    struct stat st;
    if (stat(entry.host_path.c_str(), &st) == 0)
    {
        to_filetime(st.st_ctime, 0, &data->ftCreationTime);
        to_filetime(st.st_atime, 0, &data->ftLastAccessTime);
        to_filetime(st.st_mtime, 0, &data->ftLastWriteTime);
        if (!entry.directory)
        {
            data->nFileSizeLow = (DWORD)st.st_size;
            data->nFileSizeHigh = (DWORD)((uint64_t)st.st_size >> 32);
        }
    }
    // FILE_ATTRIBUTE_ARCHIVE for files, as NTFS reports new files.
    data->dwFileAttributes = entry.directory ? FILE_ATTRIBUTE_DIRECTORY : 0x20;
    strncpy(data->cFileName, entry.name.c_str(), MAX_PATH - 1);
}

} // namespace

HANDLE port_handle_create(std::shared_ptr<PortObject> object)
{
    std::lock_guard<std::mutex> guard(g_handle_lock);
    size_t index = 0;
    while (index < g_handles.size() && g_handles[index] != NULL)
    {
        index++;
    }
    if (index == g_handles.size())
    {
        g_handles.push_back(object);
    }
    else
    {
        g_handles[index] = object;
    }
    return (HANDLE)(uintptr_t)((index + 1) * 4);
}

static bool handle_index(HANDLE handle, size_t *index)
{
    uintptr_t value = (uintptr_t)handle;
    if (value == 0 || value % 4 != 0 || handle == INVALID_HANDLE_VALUE)
    {
        return false;
    }
    *index = value / 4 - 1;
    return true;
}

std::shared_ptr<PortObject> port_handle_get(HANDLE handle, int type)
{
    size_t index;
    if (!handle_index(handle, &index))
    {
        return NULL;
    }
    std::lock_guard<std::mutex> guard(g_handle_lock);
    if (index >= g_handles.size() || g_handles[index] == NULL)
    {
        return NULL;
    }
    if (type >= 0 && g_handles[index]->type != type)
    {
        return NULL;
    }
    return g_handles[index];
}

bool port_handle_close(HANDLE handle)
{
    size_t index;
    std::shared_ptr<PortObject> object;
    if (!handle_index(handle, &index))
    {
        return false;
    }
    {
        std::lock_guard<std::mutex> guard(g_handle_lock);
        if (index >= g_handles.size() || g_handles[index] == NULL)
        {
            return false;
        }
        object.swap(g_handles[index]);
    }
    // The object (a file closes its descriptor) goes away here, outside
    // the table lock, unless something else still holds it.
    object.reset();
    return true;
}

extern "C" {

DWORD GetLastError(void)
{
    return g_last_error;
}

void SetLastError(DWORD dwErrCode)
{
    g_last_error = dwErrCode;
}

// The game only asks for the text of GetLastError() codes, with
// FORMAT_MESSAGE_ALLOCATE_BUFFER, and logs the result without checking
// for failure, so this always produces a message.
DWORD FormatMessageA(DWORD dwFlags, LPCVOID lpSource, DWORD dwMessageId, DWORD dwLanguageId, LPSTR lpBuffer,
                     DWORD nSize, va_list *Arguments)
{
    const char *meaning;
    switch (dwMessageId)
    {
    case ERROR_FILE_NOT_FOUND:
        meaning = "The system cannot find the file specified.";
        break;
    case ERROR_PATH_NOT_FOUND:
        meaning = "The system cannot find the path specified.";
        break;
    case ERROR_ACCESS_DENIED:
        meaning = "Access is denied.";
        break;
    case ERROR_DISK_FULL:
        meaning = "There is not enough space on the disk.";
        break;
    default:
        meaning = "Error.";
        break;
    }
    char text[128];
    snprintf(text, sizeof(text), "%s (%u)\r\n", meaning, (unsigned)dwMessageId);
    size_t length = strlen(text);
    if (dwFlags & FORMAT_MESSAGE_ALLOCATE_BUFFER)
    {
        char *buffer = (char *)malloc(length + 1);
        memcpy(buffer, text, length + 1);
        *(LPSTR *)lpBuffer = buffer;
    }
    else
    {
        if (nSize <= length)
        {
            return 0;
        }
        memcpy(lpBuffer, text, length + 1);
    }
    return (DWORD)length;
}

// Frees FormatMessageA's FORMAT_MESSAGE_ALLOCATE_BUFFER results.
HLOCAL LocalFree(HLOCAL mem)
{
    free(mem);
    return NULL;
}

// ---------------------------------------------------------------------------
// Files

HANDLE CreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
                   LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition,
                   DWORD dwFlagsAndAttributes, HANDLE hTemplateFile)
{
    PortVfsPath path;
    if (!port_vfs_map(lpFileName, &path))
    {
        return INVALID_HANDLE_VALUE;
    }
    bool want_read = (dwDesiredAccess & GENERIC_READ) != 0;
    bool want_write = (dwDesiredAccess & GENERIC_WRITE) != 0;
    std::string host;
    PortVfsKind kind = port_vfs_lookup(path, &host);
    if (kind == PORT_VFS_DIRECTORY || path.empty())
    {
        SetLastError(ERROR_ACCESS_DENIED);
        return INVALID_HANDLE_VALUE;
    }
    bool exists = kind == PORT_VFS_FILE;
    int flags = O_CLOEXEC;
    switch (dwCreationDisposition)
    {
    case CREATE_NEW:
        if (exists)
        {
            SetLastError(ERROR_FILE_EXISTS);
            return INVALID_HANDLE_VALUE;
        }
        flags |= O_CREAT | O_EXCL;
        break;
    case CREATE_ALWAYS:
        flags |= O_CREAT | O_TRUNC;
        break;
    case OPEN_EXISTING:
        if (!exists)
        {
            // Not found: ERROR_PATH_NOT_FOUND if the directory is missing.
            PortVfsPath parent(path.begin(), path.end() - 1);
            SetLastError(parent.empty() || port_vfs_lookup(parent, NULL) == PORT_VFS_DIRECTORY
                             ? ERROR_FILE_NOT_FOUND
                             : ERROR_PATH_NOT_FOUND);
            return INVALID_HANDLE_VALUE;
        }
        break;
    case OPEN_ALWAYS:
        if (!exists)
        {
            flags |= O_CREAT;
        }
        break;
    case TRUNCATE_EXISTING:
        if (!exists)
        {
            SetLastError(ERROR_FILE_NOT_FOUND);
            return INVALID_HANDLE_VALUE;
        }
        flags |= O_TRUNC;
        want_write = true;
        break;
    default:
        SetLastError(ERROR_INVALID_PARAMETER);
        return INVALID_HANDLE_VALUE;
    }
    bool creates = (flags & O_CREAT) != 0;
    if (want_write || creates)
    {
        // Writes only ever reach the save folder.
        bool keep_contents = exists && !(flags & O_TRUNC);
        host = port_vfs_writable(path, keep_contents);
        if (host.empty())
        {
            return INVALID_HANDLE_VALUE;
        }
    }
    flags |= want_write ? (want_read ? O_RDWR : O_WRONLY) : O_RDONLY;
    int fd = open(host.c_str(), flags, 0644);
    if (fd < 0)
    {
        SetLastError(error_from_errno(errno));
        return INVALID_HANDLE_VALUE;
    }
    HANDLE handle = port_handle_create(std::make_shared<PortFile>(fd, want_read, want_write));
    // CREATE_ALWAYS and OPEN_ALWAYS report whether the file was there.
    SetLastError((exists && (dwCreationDisposition == CREATE_ALWAYS || dwCreationDisposition == OPEN_ALWAYS))
                     ? ERROR_ALREADY_EXISTS
                     : ERROR_SUCCESS);
    return handle;
}

BOOL ReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, LPDWORD lpNumberOfBytesRead,
              LPOVERLAPPED lpOverlapped)
{
    if (lpNumberOfBytesRead != NULL)
    {
        *lpNumberOfBytesRead = 0;
    }
    std::shared_ptr<PortFile> file = get_file(hFile);
    if (file == NULL)
    {
        return FALSE;
    }
    if (!file->can_read)
    {
        SetLastError(ERROR_ACCESS_DENIED);
        return FALSE;
    }
    DWORD done = 0;
    while (done < nNumberOfBytesToRead)
    {
        ssize_t n = read(file->fd, (char *)lpBuffer + done, nNumberOfBytesToRead - done);
        if (n < 0 && errno == EINTR)
        {
            continue;
        }
        if (n < 0)
        {
            SetLastError(ERROR_READ_FAULT);
            if (lpNumberOfBytesRead != NULL)
            {
                *lpNumberOfBytesRead = done;
            }
            return FALSE;
        }
        if (n == 0)
        {
            break;
        }
        done += (DWORD)n;
    }
    if (lpNumberOfBytesRead != NULL)
    {
        *lpNumberOfBytesRead = done;
    }
    return TRUE;
}

BOOL WriteFile(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite, LPDWORD lpNumberOfBytesWritten,
               LPOVERLAPPED lpOverlapped)
{
    if (lpNumberOfBytesWritten != NULL)
    {
        *lpNumberOfBytesWritten = 0;
    }
    std::shared_ptr<PortFile> file = get_file(hFile);
    if (file == NULL)
    {
        return FALSE;
    }
    if (!file->can_write)
    {
        SetLastError(ERROR_ACCESS_DENIED);
        return FALSE;
    }
    DWORD done = 0;
    while (done < nNumberOfBytesToWrite)
    {
        ssize_t n = write(file->fd, (const char *)lpBuffer + done, nNumberOfBytesToWrite - done);
        if (n < 0 && errno == EINTR)
        {
            continue;
        }
        if (n <= 0)
        {
            SetLastError(errno == ENOSPC ? ERROR_DISK_FULL : ERROR_WRITE_FAULT);
            if (lpNumberOfBytesWritten != NULL)
            {
                *lpNumberOfBytesWritten = done;
            }
            return FALSE;
        }
        done += (DWORD)n;
    }
    if (lpNumberOfBytesWritten != NULL)
    {
        *lpNumberOfBytesWritten = done;
    }
    return TRUE;
}

DWORD SetFilePointer(HANDLE hFile, LONG lDistanceToMove, PLONG lpDistanceToMoveHigh, DWORD dwMoveMethod)
{
    std::shared_ptr<PortFile> file = get_file(hFile);
    if (file == NULL)
    {
        return INVALID_SET_FILE_POINTER;
    }
    int64_t distance = lpDistanceToMoveHigh != NULL
                           ? (int64_t)(((uint64_t)(uint32_t)*lpDistanceToMoveHigh << 32) | (uint32_t)lDistanceToMove)
                           : (int64_t)lDistanceToMove;
    int whence = dwMoveMethod == FILE_CURRENT ? SEEK_CUR : dwMoveMethod == FILE_END ? SEEK_END : SEEK_SET;
    off_t position = lseek(file->fd, (off_t)distance, whence);
    if (position < 0)
    {
        SetLastError(ERROR_NEGATIVE_SEEK);
        return INVALID_SET_FILE_POINTER;
    }
    if (lpDistanceToMoveHigh != NULL)
    {
        *lpDistanceToMoveHigh = (LONG)((uint64_t)position >> 32);
    }
    SetLastError(ERROR_SUCCESS);
    return (DWORD)position;
}

DWORD GetFileSize(HANDLE hFile, LPDWORD lpFileSizeHigh)
{
    std::shared_ptr<PortFile> file = get_file(hFile);
    struct stat st;
    if (file == NULL || fstat(file->fd, &st) != 0)
    {
        return INVALID_FILE_SIZE;
    }
    if (lpFileSizeHigh != NULL)
    {
        *lpFileSizeHigh = (DWORD)((uint64_t)st.st_size >> 32);
    }
    SetLastError(ERROR_SUCCESS);
    return (DWORD)st.st_size;
}

// Deletes from the save folder only; the game folder is never changed (a
// file there stays visible).
BOOL DeleteFileA(LPCSTR lpFileName)
{
    PortVfsPath path;
    std::string host;
    if (!port_vfs_map(lpFileName, &path))
    {
        return FALSE;
    }
    PortVfsKind kind = port_vfs_lookup_writable(path, &host);
    if (kind != PORT_VFS_FILE)
    {
        SetLastError(kind == PORT_VFS_DIRECTORY ? ERROR_ACCESS_DENIED : ERROR_FILE_NOT_FOUND);
        return FALSE;
    }
    if (unlink(host.c_str()) != 0)
    {
        SetLastError(error_from_errno(errno));
        return FALSE;
    }
    return TRUE;
}

HANDLE FindFirstFileA(LPCSTR lpFileName, LPWIN32_FIND_DATAA lpFindFileData)
{
    // The directory part goes through the namespace; the last component
    // is the pattern.
    std::string full = lpFileName;
    size_t slash = full.find_last_of("\\/");
    std::string directory = slash == std::string::npos ? "." : full.substr(0, slash + 1);
    std::string pattern = slash == std::string::npos ? full : full.substr(slash + 1);
    PortVfsPath path;
    if (!port_vfs_map(directory.c_str(), &path))
    {
        return INVALID_HANDLE_VALUE;
    }
    std::vector<PortVfsEntry> entries;
    if (!port_vfs_list(path, &entries))
    {
        SetLastError(ERROR_PATH_NOT_FOUND);
        return INVALID_HANDLE_VALUE;
    }
    std::shared_ptr<PortFind> find = std::make_shared<PortFind>();
    for (const PortVfsEntry &entry : entries)
    {
        if (port_vfs_match(pattern.c_str(), entry.name.c_str()))
        {
            find->entries.push_back(entry);
        }
    }
    if (find->entries.empty())
    {
        SetLastError(ERROR_FILE_NOT_FOUND);
        return INVALID_HANDLE_VALUE;
    }
    fill_find_data(find->entries[0], lpFindFileData);
    find->next = 1;
    return port_handle_create(find);
}

BOOL FindNextFileA(HANDLE hFindFile, LPWIN32_FIND_DATAA lpFindFileData)
{
    std::shared_ptr<PortFind> find =
        std::static_pointer_cast<PortFind>(port_handle_get(hFindFile, PORT_OBJECT_FIND));
    if (find == NULL)
    {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    if (find->next >= find->entries.size())
    {
        SetLastError(ERROR_NO_MORE_FILES);
        return FALSE;
    }
    fill_find_data(find->entries[find->next++], lpFindFileData);
    return TRUE;
}

BOOL FindClose(HANDLE hFindFile)
{
    if (port_handle_get(hFindFile, PORT_OBJECT_FIND) == NULL)
    {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    return port_handle_close(hFindFile);
}

// Every kind of handle; NULL, INVALID_HANDLE_VALUE and handles closed before
// fail with ERROR_INVALID_HANDLE (the game closes some twice).
BOOL CloseHandle(HANDLE hObject)
{
    if (!port_handle_close(hObject))
    {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    return TRUE;
}

DWORD GetCurrentDirectoryA(DWORD nBufferLength, LPSTR lpBuffer)
{
    std::string cwd = port_vfs_get_cwd();
    if (nBufferLength <= cwd.size())
    {
        return (DWORD)cwd.size() + 1;
    }
    memcpy(lpBuffer, cwd.c_str(), cwd.size() + 1);
    return (DWORD)cwd.size();
}

BOOL SetCurrentDirectoryA(LPCSTR lpPathName)
{
    return port_vfs_set_cwd(lpPathName) ? TRUE : FALSE;
}

// Creates the directory in the save folder. Fails with
// ERROR_ALREADY_EXISTS if the namespace has it (in either folder).
BOOL CreateDirectoryA(LPCSTR lpPathName, LPSECURITY_ATTRIBUTES lpSecurityAttributes)
{
    PortVfsPath path;
    if (!port_vfs_map(lpPathName, &path))
    {
        return FALSE;
    }
    if (path.empty() || port_vfs_lookup(path, NULL) != PORT_VFS_NONE)
    {
        SetLastError(ERROR_ALREADY_EXISTS);
        return FALSE;
    }
    std::string host = port_vfs_writable(path, false);
    if (host.empty())
    {
        return FALSE;
    }
    if (mkdir(host.c_str(), 0755) != 0)
    {
        SetLastError(error_from_errno(errno));
        return FALSE;
    }
    return TRUE;
}

// ---------------------------------------------------------------------------
// Modules and the environment

// The executable's path as the game sees it. Its directory is the game's
// working directory and the root of relative archive paths.
DWORD GetModuleFileNameA(HMODULE hModule, LPSTR lpFilename, DWORD nSize)
{
    if (hModule != NULL && hModule != GetModuleHandleA(NULL))
    {
        SetLastError(ERROR_INVALID_HANDLE);
        return 0;
    }
    size_t length = strlen(PORT_VFS_EXE_PATH);
    if (nSize == 0)
    {
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return 0;
    }
    if (length >= nSize)
    {
        memcpy(lpFilename, PORT_VFS_EXE_PATH, nSize - 1);
        lpFilename[nSize - 1] = '\0';
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return nSize;
    }
    memcpy(lpFilename, PORT_VFS_EXE_PATH, length + 1);
    return (DWORD)length;
}

// The executable's "module handle": any non-NULL value the game can store
// and pass back. main() passes the same one to WinMain.
HMODULE GetModuleHandleA(LPCSTR lpModuleName)
{
    static struct HINSTANCE__ module;
    if (lpModuleName == NULL)
    {
        return &module;
    }
    SetLastError(ERROR_FILE_NOT_FOUND);
    return NULL;
}

// The game loads dwmapi.dll to turn desktop composition off; there is no
// such library here, which the game handles.
HMODULE LoadLibraryA(LPCSTR lpLibFileName)
{
    SetLastError(ERROR_FILE_NOT_FOUND);
    return NULL;
}

FARPROC GetProcAddress(HMODULE hModule, LPCSTR lpProcName)
{
    return NULL;
}

BOOL FreeLibrary(HMODULE hLibModule)
{
    return FALSE;
}

// As when started from Explorer: the title is the executable's path.
void GetStartupInfoA(LPSTARTUPINFOA lpStartupInfo)
{
    static char title[] = PORT_VFS_EXE_PATH;
    DWORD cb = lpStartupInfo->cb;
    memset(lpStartupInfo, 0, sizeof(*lpStartupInfo));
    lpStartupInfo->cb = cb;
    lpStartupInfo->lpTitle = title;
    lpStartupInfo->wShowWindow = SW_SHOWNORMAL;
}

// APPDATA is the save folder's parent in the game's namespace (see
// port_vfs.h); other variables come from the host.
DWORD GetEnvironmentVariableA(LPCSTR lpName, LPSTR lpBuffer, DWORD nSize)
{
    const char *value = strcmp(lpName, "APPDATA") == 0 ? PORT_VFS_APPDATA : getenv(lpName);
    if (value == NULL)
    {
        SetLastError(203); // ERROR_ENVVAR_NOT_FOUND
        if (nSize != 0)
        {
            lpBuffer[0] = '\0';
        }
        return 0;
    }
    size_t length = strlen(value);
    if (length >= nSize)
    {
        return (DWORD)length + 1;
    }
    memcpy(lpBuffer, value, length + 1);
    return (DWORD)length;
}

// No console.
DWORD GetConsoleTitleA(LPSTR lpConsoleTitle, DWORD nSize)
{
    if (nSize != 0)
    {
        lpConsoleTitle[0] = '\0';
    }
    SetLastError(6);
    return 0;
}

void ExitProcess(UINT uExitCode)
{
    exit((int)uExitCode);
}

} // extern "C"
