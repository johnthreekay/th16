// Placeholders for functions unit 12b (0x401000-0x411860, second pass)
// calls but has not decompiled, its own and other units'.
#include "../AnmManager.h"
#include "../Bomb.h"
#include "../EffectManager.h"
#include "../ScreenEffect.h"
#include "../Stage.h"

// STUB: TH16 0x409f90
i32 Stage::on_draw_03()
{
    return 1;
}

// STUB: TH16 0x40a410
void Stage::on_draw_06()
{
}

// STUB: TH16 0x40b3b0
void StageInner::run_std()
{
}

// STUB: TH16 0x40c4a0
void StageInner::step_fog()
{
}

// 0x45c630. Only referenced by ScreenEffect modes nothing here uses yet.
int __fastcall ScreenEffect::on_tick_a(void *arg)
{
    return 1;
}

// STUB: TH16 0x45c900
int __fastcall ScreenEffect::on_tick_b(void *arg)
{
    return 1;
}

// 0x45c9e0. Only referenced by ScreenEffect modes nothing here uses yet.
int __fastcall ScreenEffect::on_tick_c(void *arg)
{
    return 1;
}

// 0x45cac0. Only referenced by ScreenEffect modes nothing here uses yet.
int __fastcall ScreenEffect::on_tick_d(void *arg)
{
    return 1;
}

// 0x45cbd0. Only referenced by ScreenEffect modes nothing here uses yet.
int __fastcall ScreenEffect::on_tick_e(void *arg)
{
    return 1;
}

// STUB: TH16 0x45cd30
int __fastcall ScreenEffect::on_tick_shake(void *arg)
{
    return 1;
}

// STUB: TH16 0x45cf10
int __fastcall ScreenEffect::on_tick_f(void *arg)
{
    return 1;
}

// 0x45c860. Only referenced by ScreenEffect modes nothing here uses yet.
int __fastcall ScreenEffect::on_draw_a(void *arg)
{
    return 1;
}

// STUB: TH16 0x45c990
int __fastcall ScreenEffect::on_draw_b(void *arg)
{
    return 1;
}

// 0x45cb50. Only referenced by ScreenEffect modes nothing here uses yet.
int __fastcall ScreenEffect::on_draw_c(void *arg)
{
    return 1;
}

// 0x45cba0. Only referenced by ScreenEffect modes nothing here uses yet.
int __fastcall ScreenEffect::on_draw_d(void *arg)
{
    return 1;
}

// 0x45ccc0. Only referenced by ScreenEffect modes nothing here uses yet.
int __fastcall ScreenEffect::on_draw_e(void *arg)
{
    return 1;
}

// STUB: TH16 0x45d130
int __fastcall ScreenEffect::on_cleanup(void *arg)
{
    return 1;
}

// STUB: TH16 0x40fb00
i32 BombMarisaAInf::on_tick()
{
    return 0;
}

// STUB: TH16 0x40fe80
i32 BombMarisaAInf::method_10()
{
    return 0;
}

// STUB: TH16 0x410de0
i32 BombReimuAInf::on_tick()
{
    return 0;
}

// STUB: TH16 0x410bb0
void BombReimuAOrbs::finish_all()
{
}

// STUB: TH16 0x418af0
AnmId EffectManager::create_effect(i32 effect, D3DXVECTOR3 *pos, i32 unk)
{
    AnmId id;
    return id;
}
