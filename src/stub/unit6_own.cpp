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

#include "../PauseMenu.h"
#include "../ReplayManager.h"

// STUB: TH16 0x43f980
void PauseMenu::tick_open()
{
}

#include "../AsciiManager.h"
#include "../PopupManager.h"


#include "../LoadingThread.h"


