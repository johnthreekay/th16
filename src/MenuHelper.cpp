#include "MenuHelper.h"

// FUNCTION: TH16 0x402e70
i32 MenuHelper::move_cursor(i32 delta)
{
    if (num_choices > 0)
    {
    again:
        next_selection += delta;
        while (next_selection >= num_choices)
        {
            if (wraps)
            {
                next_selection -= num_choices;
            }
            else
            {
                next_selection = num_choices - 1;
            }
        }
        while (next_selection < 0)
        {
            if (wraps)
            {
                next_selection += num_choices;
            }
            else
            {
                next_selection = 0;
            }
        }
        for (i32 i = 0; i < num_disabled; i++)
        {
            if (disabled[i] == next_selection)
            {
                goto again;
            }
        }
    }
    return next_selection;
}
