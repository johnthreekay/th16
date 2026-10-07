// Helpers of the title screen's music room and spell practice menus.
#include <string.h>

#include <windows.h>

#include "Scorefile.h"
#include "types.h"

// The spell cards of each stage (Extra last) in spell practice: one row per
// boss attack, one id per difficulty, -1 after the last.
// GLOBAL: TH16 0x490ee0
const i32 g_spell_practice_ids[7][13][5] = {
    {{0, 1, 2, 3, -1}, {4, 5, 6, 7, -1}},
    {{8, 9, 10, 11, -1}, {12, 13, 14, 15, -1}, {16, 17, 18, 19, -1}},
    {{20, 21, -1}, {22, 23, 24, 25, -1}, {26, 27, 28, 29, -1}, {30, 31, 32, 33, -1}},
    {{34, 35, 36, 37, -1}, {38, 39, 40, 41, -1}, {42, 43, 44, 45, -1}},
    {{46, 47, 48, 49, -1},
     {50, 51, 52, 53, -1},
     {54, 55, 56, 57, -1},
     {58, 59, 60, 61, -1},
     {62, 63, 64, 65, -1},
     {66, 67, 68, 69, -1}},
    {{70, 71, 72, 73, -1},
     {74, 75, 76, 77, -1},
     {78, 79, 80, 81, -1},
     {82, 83, 84, 85, -1},
     {86, 87, 88, 89, -1},
     {90, 91, 92, 93, -1},
     {94, 95, 96, 97, -1},
     {98, 99, 100, 101, -1},
     {102, 103, 104, 105, -1}},
    {{106, -1},
     {107, -1},
     {108, -1},
     {109, -1},
     {110, -1},
     {111, -1},
     {112, -1},
     {113, -1},
     {114, -1},
     {115, -1},
     {116, -1},
     {117, -1},
     {118, -1}},
};

// Skips to the end of the line and past the line breaks, counting the
// bytes left down; stops when none are left.
// FUNCTION: TH16 0x455330
char *__fastcall skip_line(char *p, i32 *remaining)
{
    while (*p != '\n' && *p != '\r')
    {
        if (*remaining == 0)
        {
            return p;
        }
        p++;
        (*remaining)--;
    }
    if (*remaining == 0)
    {
        return p;
    }
    while (*p == '\n' || *p == '\r')
    {
        if (*remaining == 0)
        {
            return p;
        }
        p++;
        (*remaining)--;
    }
    return p;
}

// Copies the line starting at src into dst and returns the start of the
// next line.
// TODO: the second loop reloads *remaining each time; the original keeps the count in ecx and stores it at the loop top.
// FUNCTION: TH16 0x455370
char *__fastcall read_line(char *dst, char *src, i32 *remaining)
{
    char *p = src;
    while (*p != '\n' && *p != '\r')
    {
        if (*remaining == 0)
        {
            return p;
        }
        p++;
        (*remaining)--;
    }
    i32 left = *remaining;
    if (left == 0)
    {
        return p;
    }
    *p = '\0';
    strcpy(dst, src);
    p++;
    *remaining = left - 1;
    while (*p == '\n' || *p == '\r')
    {
        if (*remaining == 0)
        {
            return p;
        }
        p++;
        (*remaining)--;
    }
    return p;
}

// Whether the main game has seen any spell card of a spell practice row,
// which makes the row selectable.
// FUNCTION: TH16 0x456060
BOOL __stdcall spell_practice_row_seen(i32 stage, i32 row)
{
    for (i32 i = 0; i < 5; i++)
    {
        i32 id = g_spell_practice_ids[stage][row][i];
        if (id < 0)
        {
            break;
        }
        if (g_Scorefile->characters[4].spells[id].attempts[0] != 0)
        {
            return TRUE;
        }
    }
    return FALSE;
}
