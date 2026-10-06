#pragma once

#include "decomp.h"
#include "types.h"

struct AnmLoaded;

// Loads and runs every ANM file. Only the parts this code calls so far.
struct AnmManager
{
    // A member reaching the manager through g_AnmManager in the original,
    // with this dropped by LTCG; static __stdcall gives the same call shape.
    static AnmLoaded *__stdcall preload_anm(i32 slot, const char *name);
};

extern AnmManager *g_AnmManager;
