#include <stdlib.h>

#include "CriticalSections.h"
#include "FileSystem.h"
#include "Log.h"

// GLOBAL: TH16 0x49f270
HANDLE g_file = INVALID_HANDLE_VALUE;

// FUNCTION: TH16 0x402600
BOOL LTCG_FASTCALL file_exists(const char *path)
{
    ENTER_CS(CS_FILE);
    HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (h != INVALID_HANDLE_VALUE)
    {
        CloseHandle(h);
        LEAVE_CS(CS_FILE);
        return TRUE;
    }
    LEAVE_CS(CS_FILE);
    return FALSE;
}

// FUNCTION: TH16 0x402680
i32 LTCG_FASTCALL file_write(const char *path, const void *data, i32 size)
{
    DWORD written;

    ENTER_CS(CS_FILE);
    HANDLE h = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE)
    {
        LPSTR msg;
        FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                       NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&msg, 0, NULL);
        zun_log("error : %s write error %s\r\n", path, msg);
        LocalFree(msg);
        LEAVE_CS(CS_FILE);
        return -1;
    }
    WriteFile(h, data, size, &written, NULL);
    if (size != written)
    {
        CloseHandle(h);
        zun_log("error : %s write error\r\n", path);
        LEAVE_CS(CS_FILE);
        return -2;
    }
    CloseHandle(h);
    zun_log("%s write ...\r\n", path);
    LEAVE_CS(CS_FILE);
    return 0;
}

// FUNCTION: TH16 0x4027c0
i32 LTCG_FASTCALL file_create(const char *path)
{
    ENTER_CS(CS_FILE);
    g_file = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (g_file == INVALID_HANDLE_VALUE)
    {
        LPSTR msg;
        FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                       NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&msg, 0, NULL);
        zun_log("error : %s write error %s\r\n", path, msg);
        LocalFree(msg);
        LEAVE_CS(CS_FILE);
        return -1;
    }
    zun_log("%s open ...\r\n", path);
    return 0;
}

// FUNCTION: TH16 0x402880
i32 LTCG_FASTCALL file_open(const char *path)
{
    ENTER_CS(CS_FILE);
    g_file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                         FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (g_file == INVALID_HANDLE_VALUE)
    {
        LPSTR msg;
        FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                       NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&msg, 0, NULL);
        zun_log("error : %s write error %s\r\n", path, msg);
        LocalFree(msg);
        LEAVE_CS(CS_FILE);
        return -1;
    }
    zun_log("%s open ...\r\n", path);
    return 0;
}

// FUNCTION: TH16 0x402940
u8 *LTCG_FASTCALL file_read(i32 size)
{
    DWORD read;

    if (g_file == INVALID_HANDLE_VALUE)
    {
        return NULL;
    }
    u8 *buf = (u8 *)malloc(size);
    if (buf == NULL)
    {
        CloseHandle(g_file);
        return NULL;
    }
    ReadFile(g_file, buf, size, &read, NULL);
    return buf;
}

// FUNCTION: TH16 0x4029a0
i32 file_close()
{
    if (g_file != INVALID_HANDLE_VALUE)
    {
        CloseHandle(g_file);
        LEAVE_CS(CS_FILE);
    }
    return 0;
}
