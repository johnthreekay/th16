// Game globals that the matching build defines in src/stub/ (which the
// portable build leaves out): there they are placeholders, zero-filled so
// that MSVC's link-time code generation cannot read their contents. The
// original has real contents for most of them (.rdata/.data tables of
// callbacks, the stage table, the spell card difficulties); this file gives
// them those contents, read from th16.exe 1.00a at the listed addresses.
//
// The zero-initialised ones are weak, so a definition moved into src/ takes
// over silently. The tables are strong: when one moves into src/ (with or
// without its contents), the link fails on the duplicate, and the entry here
// should be deleted once src/ has the contents.
#include "AnmVm.h"
#include "BulletManager.h"
#include "EffectManager.h"
#include "Fog.h"
#include "Gui.h"
#include "Laser.h"
#include "Player.h"
#include "Scorefile.h"
#include "Spellcard.h"
#include "StageData.h"
#include "Supervisor.h"
#include "ZunTimer.h"

#define PORT_WEAK __attribute__((weak))

// The callbacks the tables point at (AnmVmCallbacks.cpp, AnmRender.cpp),
// which no header declares.
int __fastcall anm_effect_1_init(AnmVm *vm, D3DXVECTOR3 *pos);
int __fastcall anm_effect_2_init(AnmVm *vm, i32 arg);
int __fastcall anm_effect_3_init(AnmVm *vm, D3DXVECTOR3 *pos);
int __fastcall anm_effect_3b_init(AnmVm *vm, D3DXVECTOR3 *pos);
int __fastcall anm_effect_1_on_switch(AnmVm *vm, i32 n);
int __fastcall anm_effect_2_on_switch(AnmVm *vm, i32 n);
int __fastcall anm_effect_3_on_switch(AnmVm *vm, i32 n);
int __fastcall anm_effect_1_on_destroy(AnmVm *vm);
int __fastcall anm_effect_2_on_destroy(AnmVm *vm);
int __fastcall anm_effect_3_on_destroy(AnmVm *vm);
int __fastcall anm_effect_2_on_copy_2(AnmVm *vm, const AnmVm *other, i32 mode);
int __fastcall anm_effect_2_on_copy_1(AnmVm *vm, u8 *buffer, i32 *size, i32 mode);
int __fastcall anm_effect_1_on_tick(AnmVm *vm);
int __fastcall anm_effect_2_on_tick(AnmVm *vm);
int __fastcall anm_effect_3_on_tick(AnmVm *vm);
i32 __fastcall anm_on_tick_fan(AnmVm *vm);
i32 __fastcall anm_on_draw_masked(AnmVm *vm);
int __fastcall anm_effect_2_on_draw(AnmVm *vm);
int __fastcall anm_effect_3_on_draw(AnmVm *vm);
i32 __fastcall anm_on_draw_fan(AnmVm *vm);

// ---------------------------------------------------------------------------
// Tables with contents (src/stub/ has them zeroed)

// 0x491b0c (src/stub/unit3.cpp)
AnmVmSwitchFunc g_anm_on_switch_funcs[4] = {
    NULL,
    anm_effect_1_on_switch, // 0x407900
    anm_effect_2_on_switch, // 0x405f20
    anm_effect_3_on_switch, // 0x406920
};

// 0x491b58 (src/stub/unit8b.cpp)
AnmVmFunc g_anm_on_destroy_funcs[4] = {
    NULL,
    anm_effect_1_on_destroy, // 0x4078f0
    anm_effect_2_on_destroy, // 0x405ed0
    anm_effect_3_on_destroy, // 0x406910
};

// 0x491b50 (src/stub/unit8b.cpp)
AnmVmCopyFunc g_anm_on_copy_funcs[2] = {
    NULL,
    anm_effect_2_on_copy_2, // 0x405fa0
};

// 0x491b48 (src/stub/w3f.cpp). The callback takes u8 * where the table
// type says void *.
AnmVmSerializeFunc g_anm_serialize_funcs[2] = {
    NULL,
    (AnmVmSerializeFunc)anm_effect_2_on_copy_1, // 0x406040
};

// 0x4919e8 (src/stub/w4b.cpp)
AnmVmFunc g_anm_on_tick_funcs[5] = {
    NULL,
    anm_effect_1_on_tick, // 0x407330
    anm_effect_2_on_tick, // 0x405700
    anm_effect_3_on_tick, // 0x406690
    anm_on_tick_fan,      // 0x46a0b0
};

// 0x491b1c (src/stub/w4b.cpp)
AnmVmSpriteFunc g_anm_sprite_mapping_funcs[4] = {
    NULL,
    bullet_map_sprite,            // 0x417140
    LaserLineInf::on_sprite_set,  // 0x431fa0
    LaserCurveInf::on_sprite_set, // 0x43a840
};

// 0x491b2c (src/stub/w5e.cpp)
AnmVmFunc g_anm_on_draw_funcs[7] = {
    NULL,
    anm_on_draw_masked,   // 0x4073a0
    anm_effect_2_on_draw, // 0x405ec0
    anm_effect_3_on_draw, // 0x406860
    anm_effect_4_on_draw, // 0x418c60
    Gui::textbox_on_draw, // 0x42b5f0
    anm_on_draw_fan,      // 0x46a330
};

// 0x4a2250 (src/stub/w3a.cpp). Effect kind 2's init takes an i32 where the
// table type says D3DXVECTOR3 *, as in the original.
EffectData g_effect_table[4] = {
    {0, 0, anm_effect_1_init, 1, 1, 1, 1, 0, 0},                                         // 0x4071a0
    {0, 0, (i32(__fastcall *)(AnmVm *, D3DXVECTOR3 *))anm_effect_2_init, 2, 2, 2, 2, 1, 1}, // 0x405670
    {0, 0, anm_effect_3_init, 3, 3, 3, 3, 0, 0},                                         // 0x406510
    {0, 0, anm_effect_3b_init, 3, 3, 3, 3, 0, 0},                                        // 0x406930
};

// 0x491700 (src/stub/unit3.cpp): each spell card's difficulty (0-3 Easy to
// Lunatic, 4 Extra).
i8 g_spell_difficulty[0x78] = {
    0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3,
    0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1,
    2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3,
    0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 0,
};

// 0x4a22d0 (src/stub/w3c.cpp): per stage, its files, music and bosses.
StageData g_stage_table[8] = {
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

// 0x490eb0 (src/stub/unit5.cpp, which has the same contents).
f32 *const g_timer_speed_ptrs[] = {&g_game_speed, NULL};

// ---------------------------------------------------------------------------
// Zero in the original too (.bss)

PORT_WEAK Player *g_Player;              // 0x4a6ef8 (src/stub/unit2.cpp)
PORT_WEAK StageData *g_stage_data;       // 0x4a6f18 (src/stub/unit34b.cpp)
PORT_WEAK Scorefile *g_Scorefile;        // 0x4a6f0c (src/stub/unit6_own.cpp)
PORT_WEAK i32 g_unk_4c0f40;              // 0x4c0f40 (src/stub/unit3.cpp)
PORT_WEAK AnmVmFunc g_anm_on_wait_funcs[1]; // 0x4c0f44 (src/stub/w4b.cpp)
PORT_WEAK Float3 g_zero_vec;             // 0x4d9dc4 (src/stub/w3a.cpp)
PORT_WEAK D3DXVECTOR2 g_zero_vec2;       // 0x4c10c8 (src/stub/w4a.cpp)
PORT_WEAK void *g_ecl_unknown_634_funcs[1]; // 0x4a6dc4 (src/stub/w4a.cpp)
