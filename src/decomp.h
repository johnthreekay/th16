#pragma once

// Link-time code generation decides inlining with the whole program in view.
// Until the callers of a function are decompiled too, our build sees far
// fewer call sites than ZUN's did and inlines things the original calls.
// DECOMP_NOINLINE marks functions the original keeps out of line, so their
// callers match. It is a stand-in for missing context: remove it once enough
// of the call graph exists for LTCG to reach the same decision on its own.
#define DECOMP_NOINLINE __declspec(noinline)
