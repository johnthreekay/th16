// Placeholders for unit 6 functions that are not decompiled yet, and for
// sound code other units own.
#include "../Scorefile.h"
#include "../SoundManager.h"
#include "../Supervisor.h"

// GLOBAL: TH16 0x4d9e10
SoundManager g_SoundManager;

// GLOBAL: TH16 0x4a6f0c
Scorefile *g_Scorefile;


void play_sound_centered_stub(i32 id, i32 unused)
{
}

#include "../PauseMenu.h"
#include "../ReplayManager.h"

// STUB: TH16 0x43f980
void PauseMenu::tick_open()
{
}

#include "../AsciiManager.h"
#include "../PopupManager.h"

// STUB: TH16 0x44a000
int PopupManager::on_draw()
{
    return 1;
}

#include "../LoadingThread.h"

// STUB: TH16 0x447760
int ReplayManager::initialize(i32 mode, const char *filename)
{
    return 0;
}

