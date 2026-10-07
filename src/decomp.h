#pragma once

#ifdef TH16_PORT
// The portable build (port/CMakeLists.txt) compiles with GCC or Clang. The
// matching-build annotations below are hints for MSVC's link-time code
// generation; here they reduce to their plain meaning. The calling
// convention keywords they expand to are defined away by
// port/include/port_prelude.h.
#define DECOMP_NOINLINE __attribute__((noinline))
#define DECOMP_ALIGN16 __attribute__((aligned(16)))
#define LTCG_FASTCALL
#define LTCG_VECTORCALL
#define HARNESS_CALLED
#define LTCG_NOTHROW noexcept
#else

// Link-time code generation decides inlining with the whole program in view.
// Until the callers of a function are decompiled too, our build sees far
// fewer call sites than ZUN's did and inlines things the original calls.
// DECOMP_NOINLINE marks functions the original keeps out of line, so their
// callers match. It is a stand-in for missing context: remove it once enough
// of the call graph exists for LTCG to reach the same decision on its own.
#define DECOMP_NOINLINE __declspec(noinline)

// A global the compiler must know to be 16-byte aligned, as the original's
// was as part of a larger struct (merged movaps stores). A plain name rather
// than __declspec(align(16)) on the annotated line: reccmp's GLOBAL parser
// would take __declspec for the variable's name.
#define DECOMP_ALIGN16 __declspec(align(16))

// Functions whose callers are all known get a custom calling convention from
// link-time code generation: the first arguments move into ecx/edx, while
// stack cleanup stays as declared (cdecl: caller pops; thiscall/stdcall:
// callee pops). A callee-popped function with its first two arguments in
// ecx/edx looks exactly like __fastcall; in ZUN's source it was most likely
// a member function whose unused `this` LTCG dropped. Our build keeps
// annotated functions alive with /INCLUDE, which blocks the conversion, so
// LTCG_FASTCALL states the result directly.
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

// LTCG works out which functions cannot throw and drops the unwind state
// around calls to them (and around array members built from them). A
// placeholder in src/stub/ hides the callee's body, so its declaration needs
// the promise spelled out to keep callers shaped like the original. Remove
// it once the callee is decompiled.
#define LTCG_NOTHROW throw()

#endif // TH16_PORT
