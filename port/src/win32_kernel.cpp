// kernel32: files, errors, modules, environment and code pages.
//
// Stubs for now. The file functions are what the archive reader
// (Arcfile.cpp, FileSystem.cpp), the config, score file and replays go
// through; an implementation maps HANDLEs to host file descriptors and
// paths to the host's separators, with a case-insensitive lookup since the
// game's names were written for a case-insensitive file system.
#include <stdlib.h>

#include <windows.h>

#include "port_stub.h"

static thread_local DWORD g_last_error;

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
// for failure, so this always produces a (generic) message.
DWORD FormatMessageA(DWORD dwFlags, LPCVOID lpSource, DWORD dwMessageId, DWORD dwLanguageId, LPSTR lpBuffer,
                     DWORD nSize, va_list *Arguments)
{
    char text[64];
    snprintf(text, sizeof(text), "error %u\r\n", (unsigned)dwMessageId);
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

HANDLE CreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
                   LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition,
                   DWORD dwFlagsAndAttributes, HANDLE hTemplateFile)
{
    PORT_UNIMPLEMENTED();
    SetLastError(ERROR_FILE_NOT_FOUND);
    return INVALID_HANDLE_VALUE;
}

BOOL ReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, LPDWORD lpNumberOfBytesRead,
              LPOVERLAPPED lpOverlapped)
{
    PORT_UNIMPLEMENTED();
    if (lpNumberOfBytesRead != NULL)
    {
        *lpNumberOfBytesRead = 0;
    }
    return FALSE;
}

BOOL WriteFile(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite, LPDWORD lpNumberOfBytesWritten,
               LPOVERLAPPED lpOverlapped)
{
    PORT_UNIMPLEMENTED();
    if (lpNumberOfBytesWritten != NULL)
    {
        *lpNumberOfBytesWritten = 0;
    }
    return FALSE;
}

DWORD SetFilePointer(HANDLE hFile, LONG lDistanceToMove, PLONG lpDistanceToMoveHigh, DWORD dwMoveMethod)
{
    PORT_UNIMPLEMENTED();
    return INVALID_SET_FILE_POINTER;
}

DWORD GetFileSize(HANDLE hFile, LPDWORD lpFileSizeHigh)
{
    PORT_UNIMPLEMENTED();
    return INVALID_FILE_SIZE;
}

BOOL DeleteFileA(LPCSTR lpFileName)
{
    PORT_UNIMPLEMENTED();
    return FALSE;
}

HANDLE FindFirstFileA(LPCSTR lpFileName, LPWIN32_FIND_DATAA lpFindFileData)
{
    PORT_UNIMPLEMENTED();
    return INVALID_HANDLE_VALUE;
}

BOOL FindNextFileA(HANDLE hFindFile, LPWIN32_FIND_DATAA lpFindFileData)
{
    PORT_UNIMPLEMENTED();
    return FALSE;
}

BOOL FindClose(HANDLE hFindFile)
{
    PORT_UNIMPLEMENTED();
    return FALSE;
}

BOOL CloseHandle(HANDLE hObject)
{
    PORT_UNIMPLEMENTED();
    return FALSE;
}

DWORD GetCurrentDirectoryA(DWORD nBufferLength, LPSTR lpBuffer)
{
    PORT_UNIMPLEMENTED();
    return 0;
}

BOOL SetCurrentDirectoryA(LPCSTR lpPathName)
{
    PORT_UNIMPLEMENTED();
    return FALSE;
}

BOOL CreateDirectoryA(LPCSTR lpPathName, LPSECURITY_ATTRIBUTES lpSecurityAttributes)
{
    PORT_UNIMPLEMENTED();
    return FALSE;
}

DWORD GetModuleFileNameA(HMODULE hModule, LPSTR lpFilename, DWORD nSize)
{
    PORT_UNIMPLEMENTED();
    if (nSize != 0)
    {
        lpFilename[0] = '\0';
    }
    return 0;
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
    PORT_UNIMPLEMENTED();
    return NULL;
}

// The game loads dwmapi.dll to turn desktop composition off; there is no
// such library here, which the game handles.
HMODULE LoadLibraryA(LPCSTR lpLibFileName)
{
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

void GetStartupInfoA(LPSTARTUPINFOA lpStartupInfo)
{
    PORT_UNIMPLEMENTED();
    DWORD cb = lpStartupInfo->cb;
    memset(lpStartupInfo, 0, sizeof(*lpStartupInfo));
    lpStartupInfo->cb = cb;
}

DWORD GetEnvironmentVariableA(LPCSTR lpName, LPSTR lpBuffer, DWORD nSize)
{
    PORT_UNIMPLEMENTED();
    if (nSize != 0)
    {
        lpBuffer[0] = '\0';
    }
    return 0;
}

DWORD GetConsoleTitleA(LPSTR lpConsoleTitle, DWORD nSize)
{
    PORT_UNIMPLEMENTED();
    if (nSize != 0)
    {
        lpConsoleTitle[0] = '\0';
    }
    return 0;
}

void ExitProcess(UINT uExitCode)
{
    exit((int)uExitCode);
}

// Shift-JIS to UTF-16 in the original; only used for the .lnk path.
int MultiByteToWideChar(UINT CodePage, DWORD dwFlags, LPCSTR lpMultiByteStr, int cbMultiByte,
                        LPWSTR lpWideCharStr, int cchWideChar)
{
    PORT_UNIMPLEMENTED();
    return 0;
}

int WideCharToMultiByte(UINT CodePage, DWORD dwFlags, LPCWSTR lpWideCharStr, int cchWideChar,
                        LPSTR lpMultiByteStr, int cbMultiByte, LPCSTR lpDefaultChar, LPBOOL lpUsedDefaultChar)
{
    PORT_UNIMPLEMENTED();
    return 0;
}

} // extern "C"
