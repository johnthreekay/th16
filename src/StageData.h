#pragma once

#include "types.h"

// ExpHP: zTableStageDataArrayItem.
struct StageBoss
{
    // The anm slot and script of the spell card background, and whether
    // the card sets Spellcard flag 0x200.
    i32 spell_bg_anm_slot;
    i32 spell_bg_script;
    i32 spell_flag_200;
    // Another effect the card starts with, by the card's last ECL
    // argument (slot -1 for none).
    i32 spell_anm_slot;
    i32 spell_script;
    // The ECL anm slot and script of the boss's intro (dialogue
    // instruction 20) and of its face (instruction 2).
    i32 intro_anm_slot;
    i32 intro_script;
    i32 face_anm_slot;
    i32 face_script;
    // front.anm script of the boss marker, minus 0xa4.
    i32 marker_script;
};

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
    i32 unk_30;
    StageBoss bosses[4];
};

extern StageData *g_stage_data;

// The stage table, indexed by stage number.
extern StageData g_stage_table[8];
