#pragma once

#include "CriticalSections.h"
#include "UpdateFunc.h"

inline void UpdateFuncRegistry::unregister_locked(UpdateFunc *f)
{
    if (f != NULL)
    {
        ENTER_CS(CS_UPDATE_FUNC_REGISTRY);
        unregister(f);
        LEAVE_CS(CS_UPDATE_FUNC_REGISTRY);
    }
}
