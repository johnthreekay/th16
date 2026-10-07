// Definitions that link-time code generation must not see into. build.py
// compiles src/stub/ without /GL, so LTCG treats everything here as
// external: it cannot read a global's value or inline a function's body.
// Not ZUN's code apart from the globals (g_zero_vec2, g_stage_table).
#include <d3dx9math.h>

#include "../StageData.h"

// A zero vector that nothing writes (Interp.h). The original loads it at
// every use; with the definition in view LTCG would see that it is never
// written and fold the loads into constant zeros (in the ECL radial and
// ellipse interpolations).
// GLOBAL: TH16 0x4c10c8
D3DXVECTOR2 g_zero_vec2;

// Takes the address of a value without doing anything with it. The harness
// stand-ins in src/harness/ use it to make LTCG assume that a global's
// address escapes (harness_w3c_expose_screen_globals).
void w3c_stub_sink(void *p)
{
}

// Takes the address of a local double, which makes a caller's frame 8-byte
// aligned (harness_w5e_transformed_pos).
void w4b_opaque_double(double *value)
{
}

// The stage table: file names, music and bosses per stage number (0 is the
// test stage, 7 the Extra stage). The dialogue files are per character
// (Reimu, Cirno, Aya, Marisa). A boss row reads: spell card background
// slot and script, flag 0x200, extra spell effect slot and script, intro
// slot and script, face slot and script, marker script; the slots index
// the stage's ECL anm files, -1 for none.
// Defined here rather than in Stage.cpp because giving it its contents in
// /GL code changes code elsewhere: LTCG then orders the operands of the
// scale multiplications in AnmVm::write_sprite_corners__without_rot and
// __with_z_rot differently (even with the strings left out).
// GLOBAL: TH16 0x4a22d0
StageData g_stage_table[8] = {
    {0, "st01.std", "st00.ecl", {"th16_02", "th16_03"},
     {"st00a.msg", "st01b.msg", "st01c.msg", "st01d.msg"},
     "st01logo.anm", {1, 2}, 0,
     {
         {3, 9, 0, 3, 17, 3, 14, 3, 8, 0},
         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
     }},
    {1, "st01.std", "st01.ecl", {"th16_02", "th16_03"},
     {"st01a.msg", "st01b.msg", "st01c.msg", "st01d.msg"},
     "st01logo.anm", {1, 2}, 0,
     {
         {3, 9, 0, 3, 17, 3, 14, 3, 8, 0},
         {3, 9, 0, 3, 17, -1, -1, -1, -1, -1},
         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
     }},
    {2, "st02.std", "st02.ecl", {"th16_04", "th16_05"},
     {"st02a.msg", "st02b.msg", "st02c.msg", "st02d.msg"},
     "st02logo.anm", {3, 4}, 0,
     {
         {3, 11, 0, 3, 19, 3, 16, 3, 10, 1},
         {-1, -1, 0, -1, -1, -1, -1, -1, -1, -1},
         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
     }},
    {3, "st03.std", "st03.ecl", {"th16_06", "th16_07"},
     {"st03a.msg", "st03b.msg", "st03c.msg", "st03d.msg"},
     "st03logo.anm", {5, 6}, 0,
     {
         {3, 11, 0, 3, 20, 3, 17, 3, 10, 3},
         {3, 11, 0, -1, -1, -1, -1, -1, -1, 2},
         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
     }},
    {4, "st04.std", "st04.ecl", {"th16_08", "th16_09"},
     {"st04a.msg", "st04b.msg", "st04c.msg", "st04d.msg"},
     "st04logo.anm", {7, 8}, 0,
     {
         {3, 12, 0, 3, 19, 3, 16, 3, 11, 4},
         {4, 12, 0, 4, 19, 4, 16, 4, 11, -1},
         {4, 12, 0, 4, 19, 4, 16, 4, 11, -1},
         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
     }},
    {5, "st05.std", "st05.ecl", {"th16_10", "th16_11"},
     {"st05a.msg", "st05b.msg", "st05c.msg", "st05d.msg"},
     "st05logo.anm", {9, 10}, 0,
     {
         {3, 12, 0, 3, 19, 3, 16, 3, 11, 5},
         {-1, -1, 0, 3, 20, -1, -1, 4, 8, -1},
         {-1, -1, 0, 3, 21, -1, -1, 4, 8, -1},
         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
     }},
    {6, "st06.std", "st06.ecl", {"th16_13", "th16_12"},
     {"st06a.msg", "st06b.msg", "st06c.msg", "st06d.msg"},
     "st06logo.anm", {11, 12}, 0,
     {
         {3, 10, 0, 3, 17, 3, 14, 3, 9, 6},
         {-1, -1, 0, -1, -1, -1, -1, -1, -1, -1},
         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
     }},
    {7, "st07.std", "st07.ecl", {"th16_16", "th16_17"},
     {"st07a.msg", "st07b.msg", "st07c.msg", "st07d.msg"},
     "st07logo.anm", {13, 14}, 0,
     {
         {3, 13, 0, 3, 20, 3, 17, 3, 12, 6},
         {4, 9, 1, 4, 13, -1, -1, 4, 8, 5},
         {-1, -1, 0, -1, -1, -1, -1, 5, 8, -1},
         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
     }},
};
