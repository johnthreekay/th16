#pragma once

#include "types.h"

// The current stage's entry of the stage table: file names and music.
// Layout from ExpHP's th-re-data (zTableStageData).
struct StageData
{
    i32 stage_num;
    const char *std_filename;
    const char *ecl_filename;
    // Stage theme, then boss theme.
    const char *music_names[2];
    // One dialogue file per character.
    const char *msg_files[4];
    const char *logo_anm_filename;
    i32 music_ids[2];
    u8 unk_30[0xd4 - 0x30];
};

extern StageData *g_stage_data;
