# th16-decomp

A matching decompilation of 東方天空璋 ～ Hidden Star in Four Seasons (TH16) v1.00a:
C++ source that compiles back to the same bytes as `th16.exe`.

This repository contains no game code or data. You need your own copy of the
game; `th16.exe` must have SHA-256
`c11776019f083978e66027e7394dafb1fb9543afca986f28049a49417e341929`.

## Toolchain

Everything about ZUN's build that we could recover from the executable:

| | | Evidence |
|---|---|---|
| Compiler | MSVC 19.10.25017 (Visual Studio 2017 15.0), x86 host | Rich header: 71 objects tagged `Utc1900_LTCG_CPP` build 25017 |
| Linker | 14.10.25017, full `/LTCG`, `/OPT:NOICF`, no `/DEBUG` | POGO debug entry with `LTCG` signature; no ILTCG, CodeView or VC_FEATURE entries; byte-identical functions left unfolded (twelve copies of one `fsincos` helper) |
| Platform toolset | `v141_xp` (Windows 7.1A SDK headers and import libs) | Subsystem and OS version 5.01 |
| C runtime | static (`/MT`): VS2017 15.0 vcruntime/libcmt, UCRT 10.0.10240 | `scripts/check_toolchain.py` finds every CRT function of a test build in `th16.exe` |
| DirectX | DirectX SDK (June 2010) | imports `d3dx9_43.dll` |
| Codegen flags | Release defaults plus `/Oy-` | `/GS` cookies, C++ EH, SSE2 scalar math, RTTI (class names such as `EnemyInf`, `BombReimuAInf` survive), frame pointers kept |

All of it is downloaded from Microsoft by `scripts/setup.py`. The MSVC packages
are the original VS2017 15.0 payloads, which Microsoft still serves at the URLs
listed in the 15.0 (26228.4) layout catalog. That catalog survives in an
archived 2017 offline layout; only its URLs and hashes are used, and every file
is pinned by SHA-256 in `scripts/toolchain.json`. The Windows tools run under Wine in a private prefix
(`prefix/wine`).

## Setup (Linux)

Requires Wine, `cabextract`, Python 3 and [uv](https://github.com/astral-sh/uv).

```sh
uv venv && uv pip install -r requirements.txt
.venv/bin/python scripts/setup.py --game-dir "/path/to/your/TH16 install"
.venv/bin/python scripts/check_toolchain.py
```

`setup.py` downloads about 800 MB into `prefix/downloads`, unpacks the toolchain
into `prefix/` and copies `th16.exe` into `orig/`. Neither directory is
committed.

## Workflow

```sh
.venv/bin/python scripts/build.py      # src/ -> build/th16.exe + .pdb + .map
.venv/bin/python scripts/compare.py    # per-function match report (reccmp)
.venv/bin/python scripts/compare.py -v 0x401300   # diff one function
.venv/bin/python scripts/quickdiff.py 0x401300    # rough diff in seconds
```

`quickdiff.py` skips reccmp's PDB parsing, which takes about 25 seconds under
Wine, so it is the tool for trying source variants quickly. It treats every
address as equal, so confirm with `compare.py`.

Each decompiled function carries an annotation with its address in the
original, in [reccmp](https://github.com/isledecomp/reccmp)'s format:

```cpp
// FUNCTION: TH16 0x401300
int UpdateFuncRegistry::register_on_tick(UpdateFunc *f, int priority)
```

Globals referenced by decompiled code get `// GLOBAL: TH16 0x...` so the
comparison can line up their addresses. Compiler-generated functions use
`// SYNTHETIC: TH16 0x...` with the name on the following comment line, e.g.
``// UpdateFuncRegistry::`scalar deleting destructor'``.

### Whole-program optimization

ZUN built with `/GL` + `/LTCG`, so code generation happens when the whole
program is linked. That has consequences for how we work:

- Matching is checked on the linked executable, function by function, never
  per object file.
- Most of our functions have no callers yet, so `build.py` passes
  `/INCLUDE:<symbol>` for every annotated function to stop the linker from
  discarding it. The decorated names come from a quick second compile without
  `/GL`, since `/GL` objects have no symbol table.
- LTCG rewrites the calling conventions of functions whose callers are all
  known. Two patterns show up in `th16.exe`:
  - Callers push stack arguments, never set `ecx`, and the callee ends in
    `ret N`: a member function that does not use `this` (it reaches its
    object through a global instead). LTCG dropped the dead `this`.
  - Arguments arrive in registers such as `ebx`, `ecx`, `edx` with no
    matching declaration: an ordinary function that LTCG gave a custom
    convention. Forcing it alive with `/INCLUDE` makes it externally visible
    and blocks that, so such functions have to be matched together with their
    callers.
- Library code (CRT, UCRT) is located in the original by `scripts/sigscan.py`
  after each build and fed to reccmp through `build/lib.csv`, so calls into
  it compare by name. Large functions are found by masked byte patterns;
  small ones by unique matches inside the library's address range, by which
  function their calls lead to, or through the calls of functions already
  found. Globals they reference (`__security_cookie`) are named the same way.
- `scripts/compare.py` runs reccmp with two fixes for VS2017 binaries: it
  accepts the newer C++ EH FuncInfo magic (`0x19930522`), and it moves each
  EH handler thunk's start back over the `/GS` cookie check that precedes
  `mov eax, FuncInfo; jmp __CxxFrameHandler3`. Without them every function
  with a C++ EH frame shows a spurious difference.

### Placeholders and stand-in callers

Three directories hold code that is not ZUN's, to give partially
decompiled code the surroundings it had in the original:

- `src/stub/` is compiled **without** `/GL`. It holds placeholder bodies for
  functions we call but have not decompiled (and the temporary `WinMain`).
  Link-time code generation cannot see inside them, so calls to them stay
  opaque the way calls to real, non-trivial code are: they may throw, they
  keep the standard calling convention, and nothing gets inlined.
- `src/placeholder/` is compiled **with** `/GL` and not forced alive. It holds
  stand-ins for callees whose shape LTCG must see: a custom calling
  convention, a constructor LTCG has to know cannot throw, a parameter it
  should fold, overrides that stop speculative devirtualization. Their
  bodies call an opaque stub so the calls survive, and they should clobber
  registers roughly like the real code, because LTCG's register allocation
  across calls looks inside them.
- `src/harness/` is compiled **with** `/GL`. It recreates call sites from code
  that is not decompiled yet, when a function's shape depends on how it is
  called (for example `delete g_UpdateFuncRegistry`, which is what makes the
  compiler generate and specialize the scalar deleting destructor).
- `DECOMP_NOINLINE` (`src/decomp.h`) marks functions the original keeps out of
  line but our smaller program would inline. Remove it once enough callers
  exist.
- `HARNESS_CALLED` marks functions kept alive by harness callers instead of
  `/INCLUDE`. That lets LTCG see every caller and pick the same custom
  calling convention it did in the original, including conventions no
  keyword can request (`this` in `ecx` with a float in `xmm1`). Prefer it
  over spelling out `LTCG_FASTCALL`/`LTCG_VECTORCALL`, which only cover the
  simpler cases.

### Things learned so far

- Function call shapes produced by LTCG are listed above. One more: when a
  parameter turns out constant for every caller (the `flags` argument of a
  scalar deleting destructor, always 1), LTCG folds it into the callee but
  keeps the stack slot, and callers fill it with whatever register is
  shortest to push (`push ecx`).
- `UpdateFunc`'s flags are a plain `unsigned int` updated with `&=`/`|=`,
  not bitfields. `create_func` keeps every constructor store and then
  overwrites them; the only construct found that reproduces this is writing
  through a `volatile UpdateFunc *`. Making a struct field `volatile` instead
  fixes `create_func` but breaks the scalar deleting destructor's scheduling.
- Register allocation follows source shape closely. `unregister_all_in_list`
  only matches when the loop loads `node->next` before `node->entry`, and the
  `run_all_*` loops only match as `while (f->active) { ... switch ... break; }`
  with `continue` for "execute again".
- UCRT stdio and math helpers (`vsprintf`, `cosf`, `fabsf`, ...) are inline
  functions in the SDK headers, so they are compiled with `/GL` as part of
  the game and get LTCG conventions like ZUN's code. `_vsprintf_l` stays out
  of line in the original but not yet in our build, which is why
  `GameErrorContext::log`/`fatal` do not match yet.
- Polymorphic classes must use ZUN's names: RTTI stores them in the
  executable (`ThreadInf`, `EnemyInf`, ...).
- LTCG deletes stores to globals that nothing in the program reads, so a
  harness function has to read manager pointers such as `g_PauseMenu`.
  Conversely, if every caller is `g_X->method()`, LTCG replaces `this`
  inside the method with the global (an unused `push ecx` slot plus a load
  of `g_X`): write it with `this`, mark it HARNESS_CALLED and call it as
  `g_X->method()` from the harness.
- Any parameter that is constant at every call site gets folded, including
  in our own harness. Harness callers pass varied values unless the
  original folded it too (then callers `push ecx` junk for that slot).
- `new T` and `new T()` differ: the parentheses value-initialize and add a
  memset. ZUN writes `new T`.
- Jump-table switches lay case blocks out in source order, which reveals
  ZUN's case order. For `a + b` of two globals MSVC loads `b` first.
- A UCRT inline (`sinf`, `_vsprintf_l`, ...) can be kept out of line for the
  whole program by redeclaring it `DECOMP_NOINLINE` in one source file.
- `#pragma loop(no_vector)` reproduces loops the original did not
  vectorize where ours would.
- Constructors and `new` expressions in the original sometimes keep a dead
  `push ecx; mov [ebp-4], this`: leftover EH cleanup state. It appears when
  the callee is visible to LTCG and not known nothrow; `LTCG_NOTHROW`
  (decomp.h) declares a stubbed constructor nothrow when it must not appear.
- ExpHP's Supervisor layout is 4 bytes off at the start: `d3d` is at +4,
  `d3d_device` at +8 (0x4c10d8, hundreds of uses), `dinput` at +0xc.

### Known tooling gaps

- Functions with internal linkage (per-file `static` copies such as the
  `sincosmul` fsincos helper) and template members cannot be annotated:
  build.py looks names up among external symbols, and its name parsing does
  not handle `Interp<Float3>`-style names.
- Comments must go above `// FUNCTION:`, not between it and the signature:
  reccmp then loses the function and build.py may misread the declaration.
- quickdiff misreports jump thunks and tail jumps; check those with
  compare.py.
- The TH06 decomp's `Chain` code (`src/Global.cpp` there) is a close ancestor
  of TH16's `UpdateFuncRegistry`: same callback result codes, same case
  order in the switch, same search-then-cut structure in `unregister`.

## Credits

Names and structure layouts follow ExpHP's
[th-re-data](https://github.com/exphp-share/th-re-data) where possible. The
[TH06 decomp](https://github.com/happyhavoc/th06) is a useful reference for
how ZUN's engine looked in its first Windows incarnation.
