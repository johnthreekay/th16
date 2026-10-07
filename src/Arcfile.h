#pragma once

#include <windows.h>

#include "decomp.h"
#include "types.h"

// File access behind an interface, as in TH06's FileAbstraction (whose
// method order is the same apart from the byte-wise helpers). The names are
// ZUN's, from RTTI.
namespace Pbg
{

// VTABLE: TH16 0x493790
class IFile
{
  public:
    virtual bool open(const char *path, const char *mode) = 0;
    virtual void close() = 0;
    // Returns the number of bytes read.
    virtual u32 read(void *buf, u32 size) = 0;
    virtual bool write(const void *buf, u32 size) = 0;
    virtual u32 tell() = 0;
    virtual u32 get_size() = 0;
    virtual bool seek(u32 offset, u32 origin) = 0;
    virtual ~IFile()
    {
    }
};

// VTABLE: TH16 0x493768
class File : public IFile
{
  public:
    HANDLE handle;
    DWORD access;

    File()
    {
        handle = INVALID_HANDLE_VALUE;
        access = 0;
    }
    virtual bool open(const char *path, const char *mode);
    virtual void close();
    virtual u32 read(void *buf, u32 size);
    virtual bool write(const void *buf, u32 size);
    virtual u32 tell();
    virtual u32 get_size();
    virtual bool seek(u32 offset, u32 origin);
    virtual ~File()
    {
        close();
    }
    // Reads the whole file into a new allocation, or returns NULL if it is
    // larger than max_size.
    virtual void *read_whole(u32 max_size);
};

} // namespace Pbg

// One file in the archive's directory. The directory has an extra entry at
// the end whose offset is where the directory starts, so the packed size of
// entry i is entries[i + 1].offset - entries[i].offset.
struct ArcfileEntry
{
    char *name;
    u32 offset;
    // Size after decompression; equal to the packed size if stored as is.
    u32 size;
    // Read from the directory but never used.
    u32 unk_c;

    ArcfileEntry();
    ~ArcfileEntry();
};

// A THA1 archive (th16.dat): an encrypted, LZSS-compressed directory at the
// end of the file, followed by encrypted and compressed entries.
struct Arcfile
{
    ArcfileEntry *entries;
    i32 entry_count;
    char *path;
    Pbg::IFile *file;

    Arcfile();
    ~Arcfile();

    HARNESS_CALLED bool open(const char *path);
    void close();
    // Reads, decrypts and decompresses an entry into dest, or into a new
    // allocation if dest is NULL.
    u8 *read_file(const char *name, u8 *dest);
    ArcfileEntry *find_entry(const char *name);
    // find_entry as file_read_all has it inline.
    __forceinline ArcfileEntry *find_entry_inline(const char *name)
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
    // These reach the archive through g_Arcfile or not at all; LTCG drops
    // the unused this.
    HARNESS_CALLED bool read_directory(const char *path);
    HARNESS_CALLED ArcfileEntry *parse_directory(u8 *data, i32 count, u32 end_offset);
    // parse_directory has the same code inline.
    DECOMP_NOINLINE static char *__stdcall dup_string(const char *s);
};

extern Arcfile g_Arcfile;
extern Arcfile g_arcfiles[0x14];

// The archive code's own debug log; empty like zun_log, but a separate
// function.
void arcfile_log(const char *fmt, ...);
