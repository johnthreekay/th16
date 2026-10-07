#include <stdlib.h>
#include <string.h>

#include "Arcfile.h"
#include "CriticalSections.h"
#include "FileSystem.h"
#include "Log.h"

#ifdef TH16_PORT
#include "port_thcrap.h"
#endif

// The file that file_create or file_open opened, until file_close.
// GLOBAL: TH16 0x49f270
HANDLE g_file = INVALID_HANDLE_VALUE;

// TODO: register allocation differs: the original keeps path on the stack and size in [ebp-8] in the archive branch, and data in esi throughout.
// FUNCTION: TH16 0x402440
u8 *LTCG_FASTCALL file_read_all(const char *path, i32 *size, i32 not_in_archive)
{
    u8 *data;
    DWORD file_size;

    ENTER_CS(CS_FILE);
    if (!not_in_archive)
    {
        // The archive only knows file names.
        const char *name = strrchr(path, '\\');
        name = name == NULL ? path : name + 1;
        name = strrchr(name, '/');
        name = name == NULL ? path : name + 1;
        ArcfileEntry *entry = g_Arcfile.find_entry_inline(name);
        file_size = entry != NULL ? entry->size : 0;
#ifdef TH16_PORT
        // thcrap (port/include/port_thcrap.h; the file_size, file_load and
        // file_loaded breakpoints): a patch's file replaces the archive's
        // or adds one it lacks, and the format patchers run on the result.
        u32 replacement_size = 0;
        u8 *replacement = port_thcrap_file_replacement(name, &replacement_size);
        if (replacement != NULL)
        {
            file_size = replacement_size;
        }
#endif
        if (size != NULL)
        {
            *size = file_size;
        }
        if (file_size == 0)
        {
            goto fail;
        }
        zun_log("%s Decode ... \r\n", name);
#ifdef TH16_PORT
        if (replacement != NULL)
        {
            data = replacement;
        }
        else
        {
            data = (u8 *)malloc(file_size);
            if (data == NULL)
            {
                goto fail;
            }
            g_Arcfile.read_file(name, data);
        }
        data = port_thcrap_patch_file(name, data, &file_size);
        if (size != NULL)
        {
            *size = file_size;
        }
#else
        data = (u8 *)malloc(file_size);
        if (data == NULL)
        {
            goto fail;
        }
        g_Arcfile.read_file(name, data);
#endif
        g_CriticalSections.leave(CS_FILE);
        return data;
    }
    else
    {
        zun_log("%s Load ... \r\n", path);
        HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                               FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
        if (h == INVALID_HANDLE_VALUE)
        {
            zun_log("error : %s is not found.\r\n", path);
            goto fail;
        }
        file_size = GetFileSize(h, NULL);
        data = (u8 *)malloc(file_size);
        if (data == NULL)
        {
            zun_log("error : %s allocation error.\r\n", path);
            CloseHandle(h);
            goto fail;
        }
        ReadFile(h, data, file_size, &file_size, NULL);
        if (size != NULL)
        {
            *size = file_size;
        }
        CloseHandle(h);
        LEAVE_CS(CS_FILE);
        return data;
    }
fail:
    LEAVE_CS(CS_FILE);
    return NULL;
}

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

// The error message says "write error" here too.
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

// Closes g_file and leaves the file critical section, if a file is open.
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
