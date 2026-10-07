// Stand-in callers for the second pass over 0x411860-0x42cb00, for
// functions whose shape depends on how they are called.
#include "../AnmVm.h"
#include "../Gui.h"
#include "../PosVel.h"
#include "../Spellcard.h"

// Like the ECL anm scale instruction at 0x42354a.
void harness_anm_vm_scale_to(AnmVm *vm, i32 time, i32 method, f32 x, f32 y)
{
    vm->scale_to(time, method, x, y);
}

// Like the ECL movement instruction at 0x41fb58.
void harness_posvel_set_ellipse_angle(PosVel *pv, f32 angle)
{
    if (angle > -999999.0)
    {
        pv->set_ellipse_angle(angle);
    }
}

// Like the stage restart code at 0x42d4d1.
void harness_gui_release_msg()
{
    g_Gui->release_msg();
}

// Like the dialogue script's textbox instructions at 0x42a63a and 0x42a833.
void harness_gui_msg_textbox(GuiMsgVm *msg, i32 kind, f32 x, f32 y, f32 width)
{
    msg->set_textbox(x, y, width, kind);
    msg->set_textbox_width(width, kind);
    msg->set_textbox(y, x, width * 2.0f, kind + 1);
    msg->set_textbox_width(x, kind + 2);
}

// Like the HUD code at 0x42dc6c.
void harness_gui_sub_42c1b0()
{
    g_Gui->sub_42c1b0();
}
