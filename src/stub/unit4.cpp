// Placeholders for functions unit 4's code calls but that are not
// decompiled yet. Compiled without /GL, so they stay opaque.
#include "../Enemy.h"
#include "unit4_extern.h"

// STUB: TH16 0x41ba10
EnemyInf::~EnemyInf()
{
}

// STUB: TH16 0x474740
int SptResourceInf::find_sub_by_name(const char *name) throw()
{
    return 0;
}

// STUB: TH16 0x41d1e0
int EnemyInf::on_tick()
{
    return 0;
}

// GLOBAL: TH16 0x4a5788
f32 g_game_speed;

// GLOBAL: TH16 0x490eb0
f32 *g_timer_speeds[1] = {&g_game_speed};

// GLOBAL: TH16 0x4a6dd4
GameThread *g_GameThread;

// GLOBAL: TH16 0x4a6ef8
Player *g_Player;

// STUB: TH16 0x473c90
i32 EclRunContext::get_int_arg(int index)
{
    return 0;
}

// STUB: TH16 0x474330
i32 *EclRunContext::get_int_arg_ptr(int index)
{
    return NULL;
}

// STUB: TH16 0x473d40
f32 EclRunContext::get_float_arg(int index)
{
    return 0.0f;
}

// STUB: TH16 0x4743a0
f32 *EclRunContext::get_float_arg_ptr(int index)
{
    return NULL;
}

// GLOBAL: TH16 0x4c0f48
AnmManager *g_AnmManager;

// GLOBAL: TH16 0x4a6dac
BulletManager *g_BulletManager;

// GLOBAL: TH16 0x4a6db8
EffectManager *g_EffectManager;

// STUB: TH16 0x46d020
AnmLoaded *__stdcall AnmManager::preload_anm(int slot, const char *filename)
{
    return NULL;
}

// STUB: TH16 0x46d770
AnmLoaded::~AnmLoaded()
{
}

// STUB: TH16 0x402440
u8 *LTCG_FASTCALL file_read_all(const char *path, i32 *size_out, i32 flag)
{
    return NULL;
}

// STUB: TH16 0x474530
int SptResourceInf::load_ecl_data(void *data)
{
    return 0;
}

// STUB: TH16 0x4185b0
void EnemyManager::destroy_all()
{
}

// GLOBAL: TH16 0x4a6d98
AsciiManager *g_AsciiManager;

// Not a game function: lets placeholders in src/harness/ look opaque.
void unit4_opaque(void *p)
{
}

// STUB: TH16 0x45b130
double LTCG_VECTORCALL get_runtime()
{
    return 0.0;
}
