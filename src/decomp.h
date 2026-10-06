#pragma once

// Link-time code generation decides inlining with the whole program in view.
// Until the callers of a function are decompiled too, our build sees far
// fewer call sites than ZUN's did and inlines things the original calls.
// DECOMP_NOINLINE marks functions the original keeps out of line, so their
// callers match. It is a stand-in for missing context: remove it once enough
// of the call graph exists for LTCG to reach the same decision on its own.
#define DECOMP_NOINLINE __declspec(noinline)

// Functions whose callers are all known get a custom calling convention from
// link-time code generation; for a plain cdecl function that is __fastcall
// (first two arguments in ecx/edx, callee pops the rest). Our build keeps
// annotated functions alive with /INCLUDE, which makes them externally
// visible and so blocks the conversion. LTCG_FASTCALL states the result
// directly. ZUN's source most likely had no calling convention here.
#define LTCG_FASTCALL __fastcall

// The same conversion for functions taking or returning floats: LTCG passes
// them in xmm registers and returns in xmm0, which __vectorcall reproduces.
#define LTCG_VECTORCALL __vectorcall

// Marks a function kept alive by stand-in callers in src/harness/ rather
// than by /INCLUDE. Only then does LTCG see every caller, and with every
// caller known it picks a custom calling convention the same way it did in
// ZUN's build (for example `this` in ecx and a float argument in xmm1, which
// no declarable convention produces). The function must stay out of line
// for that, hence noinline.
#define HARNESS_CALLED __declspec(noinline)
