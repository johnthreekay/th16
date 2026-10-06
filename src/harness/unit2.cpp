// Stand-in callers for unit 2 (Stage, Bomb) functions whose shape depends
// on code not decompiled yet.
#include "../Timer.h"

// Callers of Timer::decrement all over the game (0x40a6d9, 0x412146, ...)
// pass 1.
void harness_timer_decrement(Timer *t)
{
    (*t)--;
}

// Timer::set_value is called with many different values (0x40dc23 passes
// 60), so LTCG must not fold its argument.
void harness_timer_set_value(Timer *t, i32 value)
{
    t->set_value(value);
}
