#include "GameThread.h"

// FUNCTION: TH16 0x418420
void GameThread::enable_update_funcs()
{
    if (on_tick != NULL)
    {
        on_tick->flags |= UPDATE_FUNC_ACTIVE;
    }
    if (on_draw != NULL)
    {
        on_draw->flags |= UPDATE_FUNC_ACTIVE;
    }
}
