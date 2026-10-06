#pragma once

// Minimal declarations of other units' classes and globals that unit 4's
// code touches. Only the fields used here are named; offsets follow
// ExpHP's th-re-data. To be replaced by the real headers when merged.

#include <d3dx9math.h>

#include "../decomp.h"
#include "../types.h"

// ExpHP: zGameThread.
struct GameThread
{
    u8 unk_0[0x88];
    u32 flag_0 : 1;
    u32 flag_1 : 1;
    u32 flag_2 : 1;
    u32 flag_3 : 1;
    u32 flag_4 : 1;
    u32 flag_5 : 1;
    u32 flag_6 : 1;
    u32 flag_7 : 1;
    u32 flag_8 : 1;
    u32 flag_9 : 1;
    u32 flag_10 : 1;
    u32 unk_flags_11 : 21;
    u8 unk_8c[0xb4 - 0x8c];
};

// ExpHP: zAsciiManager.
struct AsciiManager
{
    u8 unk_0[0x1920c];
    D3DCOLOR color;
    u8 unk_19210[0x19254 - 0x19210];

    void drawf_debug(D3DXVECTOR3 *pos, const char *fmt, ...);
};

// ExpHP: zPlayer.
struct Player
{
    u8 unk_0[0x1664c];
    u32 flags_1664c;
    u8 unk_16650[0x2c7cc - 0x16650];
    f32 damage_multiplier;
    u8 unk_2c7d0[0x2c828 - 0x2c7d0];
};

// ExpHP: zAnmLoaded.
struct AnmLoaded
{
    u8 unk_0[0x13c];

    ~AnmLoaded();
};

// ExpHP: zAnmManager.
struct AnmManager
{
    u8 unk_0[0x184f4f0];
    AnmLoaded *loaded[0x1f];

    // Reaches the manager through g_AnmManager (LTCG dropped this).
    static AnmLoaded *__stdcall preload_anm(int slot, const char *filename);

    void unload_anm(int slot)
    {
        if (slot < 0 || slot >= sizeof(loaded) / sizeof(loaded[0]))
        {
            return;
        }
        if (loaded[slot] != NULL)
        {
            // A plain delete reads loaded[slot] once; the original reads it
            // again for operator delete.
            loaded[slot]->~AnmLoaded();
            operator delete(loaded[slot], sizeof(AnmLoaded));
            loaded[slot] = NULL;
        }
    }
};

// ExpHP: zBulletManager.
struct BulletManager
{
    u8 unk_0[0x1403b24];
    AnmLoaded *bullet_anm;
};

// ExpHP: zEffectManager.
struct EffectManager
{
    u8 unk_0[0xc];
    AnmLoaded *effect_anm;
};

// Reads a whole file (from the archive if present) into a new allocation.
u8 *LTCG_FASTCALL file_read_all(const char *path, i32 *size_out, i32 flag);

// Button state block around ExpHP's INPUT (0x4a52c8).
struct InputState
{
    // Frames each button has been held.
    i32 hold_time[0x21];
    u32 input;
    u32 input_prev;

    HARNESS_CALLED i32 get_hold_time(int button);
};

extern InputState g_InputState;
// ExpHP: CURRENT_PIV.
extern i32 g_current_piv;
extern GameThread *g_GameThread;
extern AsciiManager *g_AsciiManager;
extern Player *g_Player;
extern AnmManager *g_AnmManager;
extern BulletManager *g_BulletManager;
extern EffectManager *g_EffectManager;
