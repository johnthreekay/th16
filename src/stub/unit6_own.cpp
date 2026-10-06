// Placeholders for unit 6 functions that are not decompiled yet, and for
// sound code other units own.
#include "../Scorefile.h"
#include "../SoundManager.h"
#include "../Supervisor.h"

// GLOBAL: TH16 0x4d9e10
SoundManager g_SoundManager;

// GLOBAL: TH16 0x4a6f0c
Scorefile *g_Scorefile;

// STUB: TH16 0x45e330
i32 SoundManager::update_sound_thread()
{
    return 0;
}

// STUB: TH16 0x45ed00
void SoundManager::modify_bgm(i32 command, i32 arg, const char *name)
{
}

// STUB: TH16 0x45e150
void __stdcall SoundManager::play_sound_centered(i32 id, i32 unused)
{
}

// STUB: TH16 0x4711f0
void BgmStream::set_volume(i32 volume)
{
}

// STUB: TH16 0x401d50
void Supervisor::read_keyboard_input()
{
}

// STUB: TH16 0x43ce10
int Supervisor::switch_gamemodes()
{
    return 0;
}

// STUB: TH16 0x43b520
int __fastcall Supervisor::on_registration(void *arg)
{
    return 0;
}

// STUB: TH16 0x43d140
int __fastcall Supervisor::on_draw_01(void *arg)
{
    return 1;
}

// STUB: TH16 0x43d2f0
int __fastcall Supervisor::on_draw_0f(void *arg)
{
    return 1;
}
