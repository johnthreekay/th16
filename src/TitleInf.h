#pragma once

#include "AsciiManager.h"
#include "types.h"

// The title screen and main menu (ExpHP: zMainMenu). The name is ZUN's, from
// RTTI. Only what other code needs so far.
class TitleInf : public TaskInf
{
  public:
    u8 unk_4[0x5e00 - 0x4];

    // 0x44ad20. Not virtual: Supervisor::destroy_game_objects calls it
    // directly.
    ~TitleInf();
    virtual u32 get_size();
    // 0x44aee0 (ExpHP: MainMenu::operator new) and 0x44af50.
    static TitleInf *create();
    static void destroy();
};

extern TitleInf *g_TitleInf;
