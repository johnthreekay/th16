#pragma once

// Macros for reproducing what ZUN's whole-program build (/GL + /LTCG) did to
// individual functions. None of them is ZUN's; see README.md, "Whole-program
// optimization".

#ifdef TH16_PORT
// The portable build (port/CMakeLists.txt) compiles with GCC or Clang. The
// macros below are hints for MSVC's link-time code generation; here they
// reduce to their plain meaning. The calling convention keywords they
// expand to are defined away by port/include/port_prelude.h.
#define DECOMP_NOINLINE __attribute__((noinline))
#define DECOMP_ALIGN16 __attribute__((aligned(16)))
#define LTCG_FASTCALL
#define LTCG_VECTORCALL
#define HARNESS_CALLED
#else

// Link-time code generation decides inlining with the whole program in view,
// and our build does not always reach the original's decision (callers
// whose shape differs, functions kept alive with /INCLUDE). DECOMP_NOINLINE
// marks functions the original keeps out of line where ours would inline
// them, so that their callers match.
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

// Marks a function kept alive by its callers (and, where those are not
// enough, by stand-in callers in src/harness/) rather than by /INCLUDE. Only
// then does LTCG see every caller, and with every caller known it picks a
// custom calling convention the same way it did in ZUN's build (for example
// `this` in ecx and a float argument in xmm1, which no declarable convention
// produces). The function must stay out of line for that, hence noinline.
#define HARNESS_CALLED __declspec(noinline)

#endif // TH16_PORT
