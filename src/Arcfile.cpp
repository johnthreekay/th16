#include <stdlib.h>
#include <string.h>

#include "Arcfile.h"
#include "Crypt.h"
#include "Lzss.h"

// th16.dat, opened at startup.
// GLOBAL: TH16 0x4c10b8
Arcfile g_Arcfile;
// SYNTHETIC: TH16 0x401000
// ??__Eg_Arcfile@@YAXXZ
// SYNTHETIC: TH16 0x48ac10
// ??__Fg_Arcfile@@YAXXZ
// Twenty more archives that nothing in TH16 opens; only their constructors
// and destructors run.
// GLOBAL: TH16 0x4d7ba0
Arcfile g_arcfiles[0x14];
// Its dynamic initializer and atexit destructor:
// SYNTHETIC: TH16 0x401110
// ??__Eg_arcfiles@@YAXXZ
// SYNTHETIC: TH16 0x48ac60
// ??__Fg_arcfiles@@YAXXZ

// Decryption parameters for an entry (zun_decrypt's arguments), picked by
// the sum of the entry name's bytes.
struct ArcfileKey
{
    u8 key;
    u8 step;
    // Never read.
    u8 unk_2;
    i32 block;
    i32 limit;
};

// GLOBAL: TH16 0x49f278
ArcfileKey g_arcfile_keys[8] = {
    {0x1b, 0x73, 0xaa, 0x100, 0x3800}, {0x12, 0x43, 0xff, 0x200, 0x3e00}, {0x35, 0x79, 0x11, 0x400, 0x3c00},
    {0x03, 0x91, 0xdd, 0x80, 0x6400},  {0xab, 0xdc, 0xee, 0x80, 0x7000},  {0x51, 0x9e, 0xbb, 0x100, 0x4000},
    {0xc1, 0x15, 0xcc, 0x400, 0x2c00}, {0x99, 0x7d, 0x77, 0x80, 0x4400},
};

// FUNCTION: TH16 0x457690
ArcfileEntry::ArcfileEntry()
{
    name = NULL;
}

// FUNCTION: TH16 0x457670
ArcfileEntry::~ArcfileEntry()
{
    if (name != NULL)
    {
        free(name);
        name = NULL;
    }
}

// FUNCTION: TH16 0x4576a0
void arcfile_log(const char *fmt, ...)
{
}

// FUNCTION: TH16 0x456fb0
Arcfile::Arcfile()
{
    entries = NULL;
    entry_count = 0;
    path = NULL;
    file = NULL;
}

// FUNCTION: TH16 0x456fd0
Arcfile::~Arcfile()
{
    close();
}

// Reads the archive's directory and keeps the file open for read_file.
// FUNCTION: TH16 0x456fe0
HARNESS_CALLED bool Arcfile::open(const char *path)
{
    close();
    file = new Pbg::File;
    if (read_directory(path))
    {
        this->path = dup_string(path);
        if (this->path != NULL)
        {
            file->open(this->path, "r");
            return true;
        }
    }
    close();
    return false;
}

// Frees the directory and closes the file.
// FUNCTION: TH16 0x457060
void Arcfile::close()
{
    if (path != NULL)
    {
        arcfile_log("info : %s close arcfile\r\n", path);
        if (path != NULL)
        {
            free(path);
            path = NULL;
        }
    }
    path = NULL;
    delete[] entries;
    entries = NULL;
    if (file != NULL)
    {
        delete file;
    }
    file = NULL;
    entry_count = 0;
}

// FUNCTION: TH16 0x457120
u8 *Arcfile::read_file(const char *name, u8 *dest)
{
    u8 *buf = NULL;
    if (file == NULL)
    {
        return NULL;
    }
    ArcfileEntry *entry = find_entry(name);
    if (entry != NULL)
    {
        u32 packed_size = entry[1].offset - entry->offset;
        u32 size = entry->size;
        if (packed_size == size && dest != NULL)
        {
            buf = dest;
        }
        else
        {
            buf = (u8 *)malloc(packed_size);
            if (buf == NULL)
            {
                goto fail;
            }
        }
        if (file->seek(entry->offset, FILE_BEGIN) && file->read(buf, packed_size) != 0)
        {
            const char *p = entry->name;
            i32 len = strlen(p);
            u8 sum = 0;
            for (i32 i = len; i != 0; i--)
            {
                sum += *p++;
            }
            ArcfileKey *key = &g_arcfile_keys[sum & 7];
            zun_decrypt(buf, packed_size, key->key, key->step, key->block, key->limit);
            u8 *out;
            if (packed_size != size)
            {
                out = lzss_decompress(buf, packed_size, dest, size);
            }
            else
            {
                out = buf;
            }
            if (buf != dest && buf != NULL)
            {
                free(buf);
            }
            return out;
        }
    }
fail:
    arcfile_log("info : %s error\r\n", path);
    if (buf != NULL)
    {
        free(buf);
    }
    return NULL;
}

// The entry with the given name (case-insensitive), or NULL.
// FUNCTION: TH16 0x457290
ArcfileEntry *Arcfile::find_entry(const char *name)
{
    ArcfileEntry *entry = entries;
    if (entry == NULL)
    {
        return NULL;
    }
    for (i32 i = entry_count; i > 0; i--, entry++)
    {
        if (_stricmp(name, entry->name) == 0)
        {
            return entry;
        }
    }
    return NULL;
}

// The directory's header, encrypted. The sizes and count are stored with
// constants added.
struct ArcfileHeader
{
    u32 magic;
    u32 unpacked_size;
    u32 packed_size;
    u32 entry_count;
};

// "THA1", read as a little-endian u32.
const u32 ARCFILE_MAGIC = '1AHT';
// What the header's fields have added to them.
const u32 ARCFILE_UNPACKED_SIZE_BIAS = 123456789;
const u32 ARCFILE_PACKED_SIZE_BIAS = 987654321;
const u32 ARCFILE_ENTRY_COUNT_BIAS = 135792468;
// zun_decrypt's key and step for the header, and key, step and block size
// for the directory.
const u8 ARCFILE_HEADER_KEY = 0x1b;
const u8 ARCFILE_HEADER_STEP = 0x37;
const u8 ARCFILE_DIRECTORY_KEY = 0x3e;
const u8 ARCFILE_DIRECTORY_STEP = 0x9b;
const i32 ARCFILE_DIRECTORY_BLOCK = 0x80;

// Opens the archive at path and reads its directory into g_Arcfile: the
// header at the start, then the directory at the end of the file
// (encrypted and compressed).
// FUNCTION: TH16 0x4572e0
HARNESS_CALLED bool Arcfile::read_directory(const char *path)
{
    ArcfileHeader header;
    u8 *packed;
    u8 *unpacked = NULL;

    if (g_Arcfile.file == NULL)
    {
        return false;
    }
    if (g_Arcfile.file->open(path, "r"))
    {
        if (g_Arcfile.file->read(&header, sizeof(header)) != 0)
        {
            zun_decrypt((u8 *)&header, sizeof(header), ARCFILE_HEADER_KEY, ARCFILE_HEADER_STEP, sizeof(header),
                        sizeof(header));
            if (header.magic == ARCFILE_MAGIC)
            {
                header.unpacked_size -= ARCFILE_UNPACKED_SIZE_BIAS;
                header.packed_size -= ARCFILE_PACKED_SIZE_BIAS;
                g_Arcfile.entry_count = header.entry_count - ARCFILE_ENTRY_COUNT_BIAS;
                u32 dir_offset = g_Arcfile.file->get_size() - header.packed_size;
                g_Arcfile.file->seek(dir_offset, FILE_BEGIN);
                u32 packed_size = header.packed_size;
                packed = (u8 *)malloc(packed_size);
                if (packed != NULL)
                {
                    if (g_Arcfile.file->read(packed, packed_size) != 0)
                    {
                        zun_decrypt(packed, packed_size, ARCFILE_DIRECTORY_KEY, ARCFILE_DIRECTORY_STEP,
                                    ARCFILE_DIRECTORY_BLOCK, packed_size);
                        unpacked = lzss_decompress(packed, packed_size, NULL, header.unpacked_size);
                        if (unpacked != NULL)
                        {
                            g_Arcfile.entries = parse_directory(unpacked, g_Arcfile.entry_count, dir_offset);
                            if (g_Arcfile.entries != NULL)
                            {
                                free(packed);
                                free(unpacked);
                                return true;
                            }
                        }
                    }
                    free(packed);
                    if (unpacked != NULL)
                    {
                        free(unpacked);
                    }
                }
            }
        }
    }
    if (g_Arcfile.file != NULL)
    {
        delete g_Arcfile.file;
    }
    g_Arcfile.file = NULL;
    return false;
}

// Turns the unpacked directory (per entry: the name, zero-padded to a
// multiple of 4 bytes, then offset, size and unk_c) into count entries plus
// the end entry at end_offset. The directory is read through a copy of
// data: written on the parameter itself, the loop counter does not get
// data's argument slot and the stack slots come out swapped.
// FUNCTION: TH16 0x4574b0
HARNESS_CALLED ArcfileEntry *Arcfile::parse_directory(u8 *data, i32 count, u32 end_offset)
{
    ArcfileEntry *entries = new ArcfileEntry[count + 1];
    if (entries == NULL)
    {
        return NULL;
    }
    u8 *cursor = data;
    for (i32 i = 0; i < count; i++)
    {
        char *name = (char *)malloc(strlen((char *)cursor) + 1);
        if (name != NULL)
        {
            strcpy(name, (char *)cursor);
        }
        entries[i].name = name;
        i32 len = strlen((char *)cursor) + 1;
        if (len % 4 != 0)
        {
            len += 4 - len % 4;
        }
        cursor += len;
        entries[i].offset = ((u32 *)cursor)[0];
        entries[i].size = ((u32 *)cursor)[1];
        entries[i].unk_c = ((u32 *)cursor)[2];
        cursor += 12;
    }
    entries[count].offset = end_offset;
    entries[count].size = 0;
    return entries;
}

// FUNCTION: TH16 0x457620
char *__stdcall Arcfile::dup_string(const char *s)
{
    char *copy = (char *)malloc(strlen(s) + 1);
    if (copy != NULL)
    {
        strcpy(copy, s);
    }
    return copy;
}

// Prefixes a relative path with the executable's directory.
// FUNCTION: TH16 0x457a70
HARNESS_CALLED void make_full_path(char *out, const char *path)
{
    if (strchr(path, ':') != NULL)
    {
        strcpy(out, path);
        return;
    }
    GetModuleFileNameA(NULL, out, MAX_PATH);
    char *slash = strrchr(out, '\\');
    if (slash == NULL)
    {
        *out = '\0';
    }
    slash[1] = '\0';
    strcat(out, path);
}

// Opens path (relative to the executable's directory) for reading ("r"),
// writing ("w", replacing the file) or appending ("a"); the first of those
// letters in mode wins.
// FUNCTION: TH16 0x457700
bool Pbg::File::open(const char *path, const char *mode)
{
    BOOL append = FALSE;
    DWORD disposition = 0;
    char full_path[MAX_PATH];

    close();
    const char *m;
    for (m = mode; *m != '\0'; m++)
    {
        if (*m == 'r')
        {
            access = GENERIC_READ;
            disposition = OPEN_EXISTING;
            break;
        }
        if (*m == 'w')
        {
            DeleteFileA(path);
            access = GENERIC_WRITE;
            disposition = CREATE_ALWAYS;
            break;
        }
        if (*m == 'a')
        {
            append = TRUE;
            disposition = OPEN_ALWAYS;
            access = GENERIC_WRITE;
            break;
        }
    }
    if (*m == '\0')
    {
        return false;
    }
    make_full_path(full_path, path);
    handle = CreateFileA(full_path, access, FILE_SHARE_READ, NULL, disposition,
                         FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (handle == INVALID_HANDLE_VALUE)
    {
        return false;
    }
    if (append)
    {
        SetFilePointer(handle, 0, NULL, FILE_END);
    }
    return true;
}

// FUNCTION: TH16 0x457850
void Pbg::File::close()
{
    if (handle != INVALID_HANDLE_VALUE)
    {
        CloseHandle(handle);
        handle = INVALID_HANDLE_VALUE;
        access = 0;
    }
}

// FUNCTION: TH16 0x457880
u32 Pbg::File::read(void *buf, u32 size)
{
    DWORD read = 0;
    if (access != GENERIC_READ)
    {
        return 0;
    }
    ReadFile(handle, buf, size, &read, NULL);
    return read;
}

// FUNCTION: TH16 0x4578c0
bool Pbg::File::write(const void *buf, u32 size)
{
    DWORD written = 0;
    if (access != GENERIC_WRITE)
    {
        return false;
    }
    WriteFile(handle, buf, size, &written, NULL);
    if (size != written)
    {
        return false;
    }
    return true;
}

// FUNCTION: TH16 0x457900
u32 Pbg::File::tell()
{
    if (handle == INVALID_HANDLE_VALUE)
    {
        return 0;
    }
    return SetFilePointer(handle, 0, NULL, FILE_CURRENT);
}

// FUNCTION: TH16 0x457920
u32 Pbg::File::get_size()
{
    if (handle == INVALID_HANDLE_VALUE)
    {
        return 0;
    }
    return GetFileSize(handle, NULL);
}

// FUNCTION: TH16 0x457940
bool Pbg::File::seek(u32 offset, u32 origin)
{
    if (handle == INVALID_HANDLE_VALUE)
    {
        return false;
    }
    SetFilePointer(handle, offset, NULL, origin);
    return true;
}

// FUNCTION: TH16 0x457970
void *Pbg::File::read_whole(u32 max_size)
{
    if (access != GENERIC_READ)
    {
        return NULL;
    }
    u32 size = get_size();
    if (size > max_size)
    {
        return NULL;
    }
    void *data = malloc(size);
    if (data == NULL)
    {
        return NULL;
    }
    u32 pos = tell();
    // Seeks to where it already is, like TH06's ReadWholeFile; probably
    // meant to seek to 0. The failure case leaks data.
    if (!seek(pos, FILE_BEGIN))
    {
        return NULL;
    }
    if (read(data, size) == 0)
    {
        free(data);
        return NULL;
    }
    seek(pos, FILE_BEGIN);
    return data;
}

// The destructors are inline in Arcfile.h; these are the deleting
// destructors their vtables point to.

// SYNTHETIC: TH16 0x4576b0
// Pbg::File::`scalar deleting destructor'

// SYNTHETIC: TH16 0x457af0
// Pbg::IFile::`scalar deleting destructor'
