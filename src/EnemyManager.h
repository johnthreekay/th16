#pragma once

#include <string.h>

#include <d3dx9math.h>

#include "AnmManager.h"
#include "Ecl.h"
#include "Enemy.h"
#include "UpdateFunc.h"
#include "ZunTimer.h"
#include "decomp.h"
#include "types.h"

// Layout from ExpHP's th-re-data (zEnemyManagerInner).
struct EnemyManagerInner
{
    i32 ecl_int_vars[4];
    f32 ecl_float_vars[8];
    i32 miss_count;
    // Season releases increment it (BombSub.cpp).
    i32 bomb_count;
    // Cleared by BombInf (Bomb.cpp).
    i32 can_still_capture_spell;
    // Enemy ids of the bosses (0: none). ExpHP has 4 and unknown space up
    // to 0x7c, which destroy_all clears as 16 ids.
    i32 boss_ids[16];
    u32 boss_bit : 1;
    i32 enemy_limit;
    i32 next_enemy_id;
    i32 last_enemy_id;
    ZunTimer time_in_stage;
    i32 unk_a0[2];
};

struct EnemyManager;
extern EnemyManager *g_EnemyManager;

// What a new enemy starts with (ECL's enmCreate arguments and the creating
// enemy's variables).
struct EnemyCreateParams
{
    Float3 pos;
    i32 score_reward;
    i32 item_drop;
    i32 life;
    // ENEMY_FLAG_MIRRORED.
    i32 mirrored;
    // ENEMY_FLAG_4000000; enmCreate leaves it 0.
    i32 flag_4000000;
    i32 ecl_int_vars[4];
    f32 ecl_float_vars[8];
    i32 parent_enemy_id;
};

// Owns every enemy (ExpHP: zEnemyManager).
struct EnemyManager
{
    u32 flags;
    UpdateFunc *on_tick;
    UpdateFunc *on_draw;
    EnemyManagerInner inner;
    EnemyManagerInner snapshot_inner;
    EnemyList *unk_15c;
    u8 unk_160[4];
    struct AnmLoaded *anim_statement_anms[6];
    EclResourceInf *file_manager;
    EnemyList *active_enemy_list_head;
    EnemyList *active_enemy_list_tail;
    // ExpHP: __owned_list_188__always_empty.
    EnemyList *owned_list_188;
    i32 enemy_count_real;

    EnemyManager()
    {
        memset(this, 0, sizeof(*this));
        flags |= 2;
        inner.last_enemy_id = inner.next_enemy_id;
        g_EnemyManager = this;
        if (++inner.next_enemy_id == 0)
        {
            ++inner.next_enemy_id;
        }
    }
    ~EnemyManager();
    static HARNESS_CALLED EnemyManager *create(const char *ecl_filename);
    int initialize(const char *ecl_filename);
    void destroy_all();
    // Uses g_EnemyManager; LTCG dropped this.
    HARNESS_CALLED int get_enemy_count();
    HARNESS_CALLED void set_boss_id(int index, EnemyInf *enemy);
    HARNESS_CALLED void set_boss_bit(int value);
    HARNESS_CALLED void remove_from_active_list(EnemyInf *enemy);
    DECOMP_NOINLINE int update();
    static int __fastcall on_tick_callback(EnemyManager *mgr);
    static int __fastcall on_draw_callback(EnemyManager *mgr);
    // Uses g_EnemyManager; callers pass no this.
    HARNESS_CALLED EnemyInf *get_boss(i32 i);
    // 0x41aa70. Creates an enemy running the named subroutine and adds it
    // to the active list. unused is 0 everywhere.
    EnemyInf *allocate_new_enemy(const char *sub_name, EnemyCreateParams *params, i32 unused);
    // 0x41d900. Kills every enemy; reaches the manager through
    // g_EnemyManager.
    static void kill_all();
    // 0x41da30. kill_all for the enemies whose unk_278 is value.
    static void __stdcall kill_all_with_unk_278(i32 value);
    // 0x41db70. kill_all, skipping the set_death subroutines.
    static void kill_all_no_set_death();
    // Reaches the manager through g_EnemyManager; LTCG dropped this.
    HARNESS_CALLED BOOL is_enemy_alive(int id);
    EnemyInf *find_enemy_by_id(int id);
    HARNESS_CALLED struct EnemyRef find_closest(D3DXVECTOR3 *pos, f32 max_dist);
    // 0x42ca00. Restarts the stage timer and forgets the enemy list, for
    // GameThread's thread_start. Called as g_EnemyManager->, with an
    // argument it ignores.
    HARNESS_CALLED i32 reset_for_stage(i32 unused);
};

// An enemy referred to by id; 0 means none.
struct EnemyRef
{
    i32 id;

    EnemyInf *get();
};
