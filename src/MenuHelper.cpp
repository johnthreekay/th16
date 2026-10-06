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

// FUNCTION: TH16 0x402de0
void MenuHelper::push()
{
    stack_selection[stack_depth] = next_selection;
    stack_num_choices[stack_depth] = num_choices;
    stack_depth++;
    num_disabled = 0;
    if (stack_depth >= 0x10)
    {
        stack_depth = 0xf;
    }
}

// FUNCTION: TH16 0x402e20
void MenuHelper::pop()
{
    stack_depth--;
    if (stack_depth < 0)
    {
        stack_depth = 0;
        next_selection = 0;
        num_disabled = 0;
        return;
    }
    next_selection = stack_selection[stack_depth];
    num_choices = stack_num_choices[stack_depth];
    num_disabled = 0;
}

// FUNCTION: TH16 0x440c00
void MenuHelper::disable(i32 choice)
{
    disabled[num_disabled] = choice;
    num_disabled++;
again:
    for (i32 i = 0; i < num_disabled; i++)
    {
        if (disabled[i] == next_selection)
        {
            next_selection++;
            if (next_selection >= num_choices)
            {
                next_selection = 0;
            }
            goto again;
        }
    }
}
