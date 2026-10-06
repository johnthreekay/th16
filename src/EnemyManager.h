#pragma once

#include <d3dx9math.h>

#include "AnmVm.h"
#include "Timer.h"
#include "UpdateFunc.h"
#include "ZunList.h"
#include "types.h"

// An enemy, boss or not. Only the parts unit 3 needs; layout from ExpHP
// (zEnemy), whose vtable (zVTableEcl) ends with the deleting destructor.
// The name is ZUN's, from RTTI.
class EnemyInf
{
  public:
    virtual void run_over_300() = 0;
    virtual void get_int_global() = 0;
    virtual void get_int_global_ptr() = 0;
    virtual void get_float_global() = 0;
    virtual void get_float_global_ptr() = 0;
    virtual ~EnemyInf() = 0;

    u8 unk_4[0x1250 - 0x4];
    // Where the enemy is drawn (ExpHP: enemy.final_pos.pos).
    D3DXVECTOR3 final_pos;
    u8 unk_125c[0x5740 - 0x125c];
    i32 id;
    u8 unk_5744[0x574c - 0x5744];
};

// Owns every enemy. Layout from ExpHP (zEnemyManager).
struct EnemyManager
{
    // Stands for the vtable pointer (zVTableEnemyManager).
    void *vtable;
    UpdateFunc *on_tick;
    UpdateFunc *on_draw;
    i32 ecl_int_vars[4];
    f32 ecl_float_vars[8];
    i32 miss_count;
    i32 bomb_count;
    i32 can_still_capture_spell;
    // Enemy ids of the bosses (0: none), and room for more.
    i32 boss_ids[16];
    u32 flags;
    u8 unk_8c[0x180 - 0x8c];
    ZunList<EnemyInf> *active_enemy_list_head;
    ZunList<EnemyInf> *active_enemy_list_tail;
    ZunList<EnemyInf> *owned_list_188;
    i32 enemy_count_real;

    // Uses g_EnemyManager; callers pass no this.
    EnemyInf *get_boss(i32 i);
    void destroy_all();
};

extern EnemyManager *g_EnemyManager;
