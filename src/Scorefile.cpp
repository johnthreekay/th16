// scoreth16.dat: an 0x18-byte header, then LZSS-compressed and encrypted
// sections, one per character ('CR') and one status section ('ST').
#include <direct.h>
#include <stdlib.h>
#include <string.h>

#include "Scorefile.h"

#include "CriticalSections.h"
#include "Crypt.h"
#include "FileSystem.h"
#include "GameErrorContext.h"
#include "GameWindow.h"
#include "Lzss.h"
#include "Rng.h"
#include "Spellcard.h"

// GLOBAL: TH16 0x4a6f0c
Scorefile *g_Scorefile;

// A new character section: a default top ten on every difficulty and the
// spell cards' ids and difficulties.
// FUNCTION: TH16 0x4493c0
void ScorefileChara::init()
{
    memset(this, 0, sizeof(ScorefileChara));
    header.magic = SCOREFILE_SECTION_CHARA;
    header.version = SCOREFILE_SECTION_VERSION;
    header.size = sizeof(ScorefileChara);
    for (i32 d = 0; d < 6; d++)
    {
        for (i32 i = 0; i < 10; i++)
        {
            scores[d][i].score = 100000 - i * 10000;
            scores[d][i].stage = 1;
            strcpy(scores[d][i].name, "--------");
            scores[d][i].date = 0;
            scores[d][i].continues = 0;
            scores[d][i].subseason = 0;
            scores[d][i].slowdown = 0.0f;
        }
    }
    for (i32 i = 0; i < 0x77; i++)
    {
        ScorefileSpell *spell = &spells[i];
        spell->id = i;
        spell->difficulty = g_spell_difficulty[i];
    }
}

// TODO: the original keeps EnterCriticalSection's address in ebx and does not align the loop.
// A new status section: no name, and random filler.
// FUNCTION: TH16 0x449720
void ScorefileStatus::init()
{
    memset(this, 0, sizeof(ScorefileStatus));
    header.magic = SCOREFILE_SECTION_STATUS;
    header.version = SCOREFILE_SECTION_VERSION;
    header.size = sizeof(ScorefileStatus);
    strcpy(name, "        ");
    for (i32 i = 0; i < 0x1ee; i++)
    {
        random[i] = g_replay_safe_rng.rand_u16();
    }
}

// FUNCTION: TH16 0x4497e0
Scorefile::Scorefile()
{
    i32 size;

    ScorefileData *sf = (ScorefileData *)this;
    memset(sf, 0, sizeof(Scorefile));
    _chdir(g_GameWindow.save_dir);
    sf->file = (ScorefileHeader *)file_read_all("scoreth16.dat", &size, 1);
    _chdir(g_GameWindow.exe_dir);
    sf->status.init();
    for (ScorefileChara *chara = sf->charas; (u32)(chara - sf->charas) < 5; chara++)
    {
        chara->init();
    }
    load_sections();
}

// FUNCTION: TH16 0x449880
i32 Scorefile::load_sections()
{
    ScorefileData *sf = (ScorefileData *)this;
    if (sf->file != NULL)
    {
        if (sf->file->magic == SCOREFILE_MAGIC && sf->file->version == SCOREFILE_VERSION)
        {
            zun_decrypt((u8 *)(sf->file + 1), sf->file->compressed_size, 0xac, 0x35, 0x10, sf->file->compressed_size);
            u8 *data = (u8 *)(sf->file + 1);
            sf->sections = (u8 *)malloc(sf->file->size << 2);
            lzss_decompress(data, sf->file->compressed_size, sf->sections, sf->file->size);
            u8 *p = sf->sections;
            i32 remaining = sf->file->size;
            while (remaining > 0)
            {
                ScorefileSection *section = (ScorefileSection *)p;
                if (section->magic == SCOREFILE_SECTION_CHARA)
                {
                    if (section->version == SCOREFILE_SECTION_VERSION &&
                        section->compute_checksum(sizeof(ScorefileChara)) == section->checksum &&
                        section->size == sizeof(ScorefileChara))
                    {
                        memcpy(&sf->charas[((ScorefileChara *)section)->character], section, sizeof(ScorefileChara));
                    }
                }
                else if (section->magic == SCOREFILE_SECTION_STATUS)
                {
                    if (section->version == SCOREFILE_SECTION_VERSION &&
                        section->compute_checksum(sizeof(ScorefileStatus)) == section->checksum &&
                        section->size == sizeof(ScorefileStatus))
                    {
                        sf->status = *(ScorefileStatus *)section;
                    }
                }
                else
                {
                    break;
                }
                remaining -= section->size;
                if (remaining < 0)
                {
                    break;
                }
                p += section->size;
            }
            return 0;
        }
        free(sf->file);
        sf->file = NULL;
    }
    sf->file = (ScorefileHeader *)malloc(sizeof(ScorefileHeader));
    memset(sf->file, 0, sizeof(ScorefileHeader));
    sf->file->magic = SCOREFILE_MAGIC;
    sf->file->version = SCOREFILE_VERSION;
    sf->file->unk_c = 0x100;
    return 0;
}

extern HANDLE g_file;

// Writes to the file file_create opened; on a short write, closes it.
static inline void scorefile_write_chunk(const void *data, DWORD size)
{
    if (g_file != INVALID_HANDLE_VALUE)
    {
        DWORD written;
        WriteFile(g_file, data, size, &written, NULL);
        if (size != written)
        {
            CloseHandle(g_file);
            LEAVE_CS(CS_FILE);
        }
    }
}

// Builds the file (header, character sections with fresh checksums, the
// status section), compresses and encrypts it and writes scoreth16.dat.
// FUNCTION: TH16 0x449a00
i32 scorefile_save()
{
    ScorefileData *sf = (ScorefileData *)g_Scorefile;
    if (sf->file == NULL)
    {
        return -1;
    }
    u8 *buf = (u8 *)malloc(0x200000);
    i32 size = 0;
    memcpy(buf + size, sf->file, sizeof(ScorefileHeader));
    size += sizeof(ScorefileHeader);
    for (u32 i = 0; i < 5; i++)
    {
        ScorefileChara *chara = &sf->charas[i];
        if (chara->header.magic == SCOREFILE_SECTION_CHARA)
        {
            chara->character = i;
            chara->header.checksum = chara->header.compute_checksum(sizeof(ScorefileChara));
            memcpy(buf + size, chara, sizeof(ScorefileChara));
            size += sizeof(ScorefileChara);
        }
    }
    sf->status.header.checksum = sf->status.header.compute_checksum(sizeof(ScorefileStatus));
    *(ScorefileStatus *)(buf + size) = sf->status;
    size += sizeof(ScorefileStatus);
    sf->file->size = size - sizeof(ScorefileHeader);
    u8 *compressed = lzss_compress(buf + sizeof(ScorefileHeader), sf->file->size, (i32 *)&sf->file->compressed_size);
    sf->file->file_size = sf->file->compressed_size + sizeof(ScorefileHeader);
    zun_encrypt(compressed, sf->file->compressed_size, 0xac, 0x35, 0x10, sf->file->compressed_size);
    _chdir(g_GameWindow.save_dir);
    if (file_create("scoreth16.dat") != 0)
    {
        // "Cannot write the score file"
        g_GameErrorContext.fatal("error : \x83X\x83R\x83" "A\x83t\x83@\x83" "C\x83\x8b\x82\xaa\x8f\x91\x82\xab\x8d\x9e\x82\xdf\x82\xc8\x82\xa2\n");
        if (compressed != NULL)
        {
            free(compressed);
        }
        if (buf != NULL)
        {
            free(buf);
        }
        _chdir(g_GameWindow.exe_dir);
        return -1;
    }
    scorefile_write_chunk(sf->file, sizeof(ScorefileHeader));
    scorefile_write_chunk(compressed, sf->file->compressed_size);
    file_close();
    if (compressed != NULL)
    {
        free(compressed);
    }
    _chdir(g_GameWindow.exe_dir);
    if (buf != NULL)
    {
        free(buf);
    }
    return 0;
}
