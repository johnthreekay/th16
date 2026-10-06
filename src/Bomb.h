#pragma once

#include <d3dx9math.h>

#include "AnmManager.h"
#include "Timer.h"
#include "UpdateFunc.h"
#include "decomp.h"
#include "types.h"

// The player's bomb (main) and season release (sub). One of each exists
// while a stage runs; the classes differ only in their virtual functions.
// Layout from ExpHP's zBomb.
//
// VTABLE: TH16 0x491e58
class BombInf
{
  public:
    u32 flags;
    UpdateFunc *on_tick_func;
    UpdateFunc *on_draw_func;
    void *unk_10;
    D3DXVECTOR3 pos;
    u8 unk_20[0xc];
    f32 angle;
    i32 in_use;
    // Counts frames while in use; negative while a release cools down.
    Timer timer;
    Timer timer_48;
    AnmId anm_id;
    AnmId anm_id_60;
    AnmId anm_id_64;
    // Set when a bomb starts during a spell card that has run a second.
    i32 started_during_spell;
    i32 unk_6c;
    void *unk_70;
    u8 unk_74[0x20];
    Timer timer_94;
    Timer timer_a8;
    AnmId anm_id_bc;
    AnmId anm_id_c0;
    AnmId anm_id_c4;
    u8 unk_c8[8];
    void *unk_d0;
    i32 is_season;
    i32 season_level;
    // PIV bonus of the last release and the value on display.
    f32 release_bonus;
    f32 release_bonus_shown;
    Timer release_bonus_timer;
    // Season level of the last release; picks the bonus text color.
    i32 release_bonus_level;
    D3DXVECTOR3 release_bonus_pos;

    DECOMP_NOINLINE BombInf();
    ~BombInf();

    virtual i32 begin();
    virtual i32 on_tick();
    virtual i32 on_draw();
    virtual i32 method_c(i32 a, i32 b);
    virtual i32 method_10();
    virtual void method_14();

    i32 initialize(i32 is_season);
    i32 update();
    void draw();
    i32 activate();
    i32 can_activate();
    void start_release_cooldown();

    static int __fastcall on_tick_callback(void *arg);
    static int __fastcall on_draw_callback(void *arg);

    static BombInf *create();
    static void destroy_all();
};

// VTABLE: TH16 0x491e3c
class BombReimuAInf : public BombInf
{
};

// VTABLE: TH16 0x491e04
class BombCirnoAInf : public BombInf
{
};

// VTABLE: TH16 0x491de8
class BombAyaAInf : public BombInf
{
};

// VTABLE: TH16 0x491e20
class BombMarisaAInf : public BombInf
{
};

// Spring release.
// VTABLE: TH16 0x491dcc
class BombReimuSubInf : public BombInf
{
};

// Summer release.
// VTABLE: TH16 0x491d94
class BombCirnoSubInf : public BombInf
{
};

// Autumn release.
// VTABLE: TH16 0x491d78
class BombAyaSubInf : public BombInf
{
};

// Winter release.
// VTABLE: TH16 0x491db0
class BombMarisaSubInf : public BombInf
{
};

// Doyou (all seasons) release.
// VTABLE: TH16 0x491d5c
class BombAllSubInf : public BombInf
{
};

extern BombInf *g_MainBomb;
extern BombInf *g_SubseasonBomb;
