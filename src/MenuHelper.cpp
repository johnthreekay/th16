#include "MenuHelper.h"

// FUNCTION: TH16 0x402e70
i32 MenuHelper::move_cursor(i32 delta)
{
    if (num_choices > 0)
    {
    again:
        selection += delta;
        while (selection >= num_choices)
        {
            if (wraps)
            {
                selection -= num_choices;
            }
            else
            {
                selection = num_choices - 1;
            }
        }
        while (selection < 0)
        {
            if (wraps)
            {
                selection += num_choices;
            }
            else
            {
                selection = 0;
            }
        }
        for (i32 i = 0; i < num_disabled; i++)
        {
            if (disabled[i] == selection)
            {
                goto again;
            }
        }
    }
    return selection;
}
