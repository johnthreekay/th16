// The original's contents of the game tables that main's src/ defines
// zero-filled (AnmVmCallbacks.cpp, EffectManager.cpp, Spellcard.cpp,
// Stage.cpp: the matching build only needs their addresses), read from
// th16.exe 1.00a at the listed addresses, and copied into them before
// main() runs. Plus g_zero_vec2, which only src/stub/ (not built here)
// defines.
//
// Main is filling those tables with their contents; once it has, delete the
// copies here (they then only repeat what src/ has, and fail to compile if
// the tables become const).
#include "AnmVm.h"
#include "BulletManager.h"
#include "EffectManager.h"
#include "Fog.h"
#include "Gui.h"
#include "Interp.h"
#include "Laser.h"
#include "Spellcard.h"
#include "StageData.h"

// The callbacks the tables point at (AnmVmCallbacks.cpp, AnmRender.cpp),
// which no header declares.
int __fastcall anm_masked_effect_init(AnmVm *vm, D3DXVECTOR3 *pos);
int __fastcall anm_gather_effect_init(AnmVm *vm, i32 arg);
int __fastcall anm_jagged_line_blue_init(AnmVm *vm, D3DXVECTOR3 *pos);
int __fastcall anm_jagged_line_gray_init(AnmVm *vm, D3DXVECTOR3 *pos);
int __fastcall anm_masked_effect_on_switch(AnmVm *vm, i32 n);
int __fastcall anm_gather_effect_on_switch(AnmVm *vm, i32 n);
int __fastcall anm_jagged_line_on_switch(AnmVm *vm, i32 n);
int __fastcall anm_masked_effect_on_destroy(AnmVm *vm);
int __fastcall anm_gather_effect_on_destroy(AnmVm *vm);
int __fastcall anm_jagged_line_on_destroy(AnmVm *vm);
int __fastcall anm_gather_effect_on_copy(AnmVm *vm, const AnmVm *other, i32 mode);
int __fastcall anm_gather_effect_on_serialize(AnmVm *vm, u8 *buffer, i32 *size, i32 mode);
int __fastcall anm_masked_effect_on_tick(AnmVm *vm);
int __fastcall anm_gather_effect_on_tick(AnmVm *vm);
int __fastcall anm_jagged_line_on_tick(AnmVm *vm);
i32 __fastcall anm_on_tick_fan(AnmVm *vm);
i32 __fastcall anm_on_draw_masked(AnmVm *vm);
int __fastcall anm_gather_effect_on_draw(AnmVm *vm);
int __fastcall anm_jagged_line_on_draw(AnmVm *vm);
i32 __fastcall anm_on_draw_fan(AnmVm *vm);

namespace
{

// 0x491b0c
const AnmVmSwitchFunc k_anm_on_switch_funcs[4] = {
    NULL,
    anm_masked_effect_on_switch, // 0x407900
    anm_gather_effect_on_switch, // 0x405f20
    anm_jagged_line_on_switch,   // 0x406920
};

// 0x491b58
const AnmVmFunc k_anm_on_destroy_funcs[4] = {
    NULL,
    anm_masked_effect_on_destroy, // 0x4078f0
    anm_gather_effect_on_destroy, // 0x405ed0
    anm_jagged_line_on_destroy,   // 0x406910
};

// 0x491b50
const AnmVmCopyFunc k_anm_on_copy_funcs[2] = {
    NULL,
    anm_gather_effect_on_copy, // 0x405fa0
};

// 0x491b48. The callback takes u8 * where the table type says void *.
const AnmVmSerializeFunc k_anm_serialize_funcs[2] = {
    NULL,
    (AnmVmSerializeFunc)anm_gather_effect_on_serialize, // 0x406040
};

// 0x4919e8
const AnmVmFunc k_anm_on_tick_funcs[5] = {
    NULL,
    anm_masked_effect_on_tick, // 0x407330
    anm_gather_effect_on_tick, // 0x405700
    anm_jagged_line_on_tick,   // 0x406690
    anm_on_tick_fan,           // 0x46a0b0
};

// 0x491b1c
const AnmVmSpriteFunc k_anm_sprite_mapping_funcs[4] = {
    NULL,
    bullet_map_sprite,            // 0x417140
    LaserLineInf::on_sprite_set,  // 0x431fa0
    LaserCurveInf::on_sprite_set, // 0x43a840
};

// 0x491b2c
const AnmVmFunc k_anm_on_draw_funcs[7] = {
    NULL,
    anm_on_draw_masked,        // 0x4073a0
    anm_gather_effect_on_draw, // 0x405ec0
    anm_jagged_line_on_draw,   // 0x406860
    anm_effect_4_on_draw,      // 0x418c60
    Gui::textbox_on_draw,      // 0x42b5f0
    anm_on_draw_fan,           // 0x46a330
};

// 0x4a2250. The gather effect's init takes an i32 where the table type says
// D3DXVECTOR3 *, as in the original.
const EffectData k_effect_table[4] = {
    {0, 0, anm_masked_effect_init, 1, 1, 1, 1, 0, 0},                                         // 0x4071a0
    {0, 0, (i32(__fastcall *)(AnmVm *, D3DXVECTOR3 *))anm_gather_effect_init, 2, 2, 2, 2, 1, 1}, // 0x405670
    {0, 0, anm_jagged_line_blue_init, 3, 3, 3, 3, 0, 0},                                      // 0x406510
    {0, 0, anm_jagged_line_gray_init, 3, 3, 3, 3, 0, 0},                                      // 0x406930
};

// 0x491700: each spell card's difficulty (0-3 Easy to Lunatic, 4 Extra).
const i8 k_spell_difficulty[0x78] = {
    0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3,
    0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1,
    2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3,
    0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 0,
};

// 0x4a22d0: per stage, its files, music and bosses.
const StageData k_stage_table[8] = {
    {0, "st01.std", "st00.ecl", {"th16_02", "th16_03"}, {"st00a.msg", "st01b.msg", "st01c.msg", "st01d.msg"},
     "st01logo.anm", {1, 2}, 0,
     {{3, 9, 0, 3, 17, 3, 14, 3, 8, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
      {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}}},
    {1, "st01.std", "st01.ecl", {"th16_02", "th16_03"}, {"st01a.msg", "st01b.msg", "st01c.msg", "st01d.msg"},
     "st01logo.anm", {1, 2}, 0,
     {{3, 9, 0, 3, 17, 3, 14, 3, 8, 0}, {3, 9, 0, 3, 17, -1, -1, -1, -1, -1}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
      {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}}},
    {2, "st02.std", "st02.ecl", {"th16_04", "th16_05"}, {"st02a.msg", "st02b.msg", "st02c.msg", "st02d.msg"},
     "st02logo.anm", {3, 4}, 0,
     {{3, 11, 0, 3, 19, 3, 16, 3, 10, 1}, {-1, -1, 0, -1, -1, -1, -1, -1, -1, -1}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
      {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}}},
    {3, "st03.std", "st03.ecl", {"th16_06", "th16_07"}, {"st03a.msg", "st03b.msg", "st03c.msg", "st03d.msg"},
     "st03logo.anm", {5, 6}, 0,
     {{3, 11, 0, 3, 20, 3, 17, 3, 10, 3}, {3, 11, 0, -1, -1, -1, -1, -1, -1, 2}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
      {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}}},
    {4, "st04.std", "st04.ecl", {"th16_08", "th16_09"}, {"st04a.msg", "st04b.msg", "st04c.msg", "st04d.msg"},
     "st04logo.anm", {7, 8}, 0,
     {{3, 12, 0, 3, 19, 3, 16, 3, 11, 4}, {4, 12, 0, 4, 19, 4, 16, 4, 11, -1}, {4, 12, 0, 4, 19, 4, 16, 4, 11, -1},
      {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}}},
    {5, "st05.std", "st05.ecl", {"th16_10", "th16_11"}, {"st05a.msg", "st05b.msg", "st05c.msg", "st05d.msg"},
     "st05logo.anm", {9, 10}, 0,
     {{3, 12, 0, 3, 19, 3, 16, 3, 11, 5}, {-1, -1, 0, 3, 20, -1, -1, 4, 8, -1}, {-1, -1, 0, 3, 21, -1, -1, 4, 8, -1},
      {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}}},
    {6, "st06.std", "st06.ecl", {"th16_13", "th16_12"}, {"st06a.msg", "st06b.msg", "st06c.msg", "st06d.msg"},
     "st06logo.anm", {11, 12}, 0,
     {{3, 10, 0, 3, 17, 3, 14, 3, 9, 6}, {-1, -1, 0, -1, -1, -1, -1, -1, -1, -1}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
      {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}}},
    {7, "st07.std", "st07.ecl", {"th16_16", "th16_17"}, {"st07a.msg", "st07b.msg", "st07c.msg", "st07d.msg"},
     "st07logo.anm", {13, 14}, 0,
     {{3, 13, 0, 3, 20, 3, 17, 3, 12, 6}, {4, 9, 1, 4, 13, -1, -1, 4, 8, 5}, {-1, -1, 0, -1, -1, -1, -1, 5, 8, -1},
      {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}}},
};

// Copies element by element, so that a table main makes const (or resizes)
// stops the build here instead of being overwritten.
template <class T, size_t N> void fill(T (&dst)[N], const T (&src)[N])
{
    for (size_t i = 0; i < N; i++)
    {
        dst[i] = src[i];
    }
}

__attribute__((constructor)) void port_fill_game_tables()
{
    fill(g_anm_on_switch_funcs, k_anm_on_switch_funcs);
    fill(g_anm_on_destroy_funcs, k_anm_on_destroy_funcs);
    fill(g_anm_on_copy_funcs, k_anm_on_copy_funcs);
    fill(g_anm_serialize_funcs, k_anm_serialize_funcs);
    fill(g_anm_on_tick_funcs, k_anm_on_tick_funcs);
    fill(g_anm_sprite_mapping_funcs, k_anm_sprite_mapping_funcs);
    fill(g_anm_on_draw_funcs, k_anm_on_draw_funcs);
    fill(g_effect_table, k_effect_table);
    fill(g_spell_difficulty, k_spell_difficulty);
    fill(g_stage_table, k_stage_table);
}

} // namespace

// 0x4c10c8: zero in the original too (.bss). Defined in src/stub/Opaque.cpp,
// out of LTCG's sight; weak, so that a definition moved into src/ takes over.
__attribute__((weak)) D3DXVECTOR2 g_zero_vec2;
