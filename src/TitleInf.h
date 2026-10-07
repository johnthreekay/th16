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
};

extern TitleInf *g_TitleInf;
