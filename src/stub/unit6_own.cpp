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

void play_sound_centered_stub(i32 id, i32 unused)
{
}

// STUB: TH16 0x4711f0
void BgmStream::set_volume(i32 volume)
{
}

// STUB: TH16 0x43ce10
int Supervisor::switch_gamemodes()
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

#include "../PauseMenu.h"
#include "../ReplayManager.h"

// STUB: TH16 0x447c80
ReplayManager::~ReplayManager()
{
}

// STUB: TH16 0x43e5f0
int PauseMenu::on_tick()
{
    return 1;
}

// STUB: TH16 0x43edc0
int PauseMenu::on_draw()
{
    return 1;
}

#include "../AsciiManager.h"
#include "../PopupManager.h"

// STUB: TH16 0x44a000
int PopupManager::on_draw()
{
    return 1;
}

#include "../LoadingThread.h"

// STUB: TH16 0x43d970
void Supervisor::setup_special_anms()
{
}

// STUB: TH16 0x43afe0
LoadingThread::~LoadingThread()
{
}

// STUB: TH16 0x447760
int ReplayManager::initialize(i32 mode, const char *filename)
{
    return 0;
}

// STUB: TH16 0x448c10
int ReplayManager::read_replay_file(const char *filename)
{
    return 0;
}

// STUB: TH16 0x4482f0
int __fastcall ReplayManager::on_draw_47_body(void *arg)
{
    return 1;
}
