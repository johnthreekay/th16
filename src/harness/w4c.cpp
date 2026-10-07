// Stand-in callers for wave 4's interpreters, recreating call sites in code
// that is not decompiled yet.
#include "../Gui.h"

// Like Gui::on_tick_body (0x4289c4), which runs the dialogue script.
i32 harness_w4c_msg_run(Gui *gui)
{
    if (gui->msg != NULL)
    {
        return gui->msg->run();
    }
    return 0;
}
