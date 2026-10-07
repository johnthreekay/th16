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
Wine, so it is the tool for trying source variants quickly. Data addresses
compare as symbol+offset when the original's address of the symbol is known
(`// GLOBAL:` annotations, `build/lib.csv`), so wrong offsets into global
objects show up; unknown data addresses and call targets still count as
equal, so confirm with `compare.py`. Large structs should carry
`static_assert(offsetof(...))` checks for their known offsets.

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
  through a `volatile UpdateFunc *`. The constructor does that through a
  volatile copy of `this`, so stores made after it (in `create_func` and
  where LTCG inlines it, such as `EffectManager::initialize`) still merge.
  Making a struct field `volatile` instead fixes `create_func` but breaks the
  scalar deleting destructor's scheduling.
- Register allocation follows source shape closely. `unregister_all_in_list`
  only matches when the loop loads `node->next` before `node->entry`, and the
  `run_all_*` loops only match as `while (f->active) { ... switch ... break; }`
  with `continue` for "execute again".
- UCRT stdio and math helpers (`vsprintf`, `cosf`, `fabsf`, ...) are inline
  functions in the SDK headers, so they are compiled with `/GL` as part of
  the game and get LTCG conventions like ZUN's code. The out-of-line
  function at 0x405540 is `_vsnprintf_l` (vsprintf's callee in every UCRT
  version checked: 10240, 14393, 15063) with the count folded away; the
  `vsprintf` callers still have a 4-byte stack slot ours lack.
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
- Whether a function realigns its frame (`and esp, -8`) for a spilled
  double, such as the argument of an inlined `atan2f`, depends on the whole
  program: LTCG only does it once enough code spills doubles. The original
  is past that point; our build sits near it, so removing double math
  anywhere can flip `angle_to_player` and `zun_atan2f` back.
  `harness_homing_angle` (src/harness/unit56b.cpp) stands in for the
  undecompiled atan2f users.

- Whole-program effects show up in single functions:
  - Giving a callee a visible /GL body can add a /GS cookie to callers that
    pass a local's address elsewhere (get_vm_with_id stays an opaque stub
    for that reason; its matching code is kept under `#if 0`). Check callers
    for new cookies whenever a stub becomes real code.
  - LTCG realigns frames (`and esp,-8`) for spilled doubles only once enough
    of the program uses them; a harness function with a few inlined
    `atan2f` calls keeps that threshold (harness_homing_angle).
  - As the call graph grows, LTCG starts or stops inlining small helpers;
    after a merge look for new out-of-line copies in build/th16.map and use
    `__forceinline` or DECOMP_NOINLINE.
  - A function that is inlined at some call sites and called at others
    needs an explicit inline copy (`delete_vm_inline`, `create_inline`).
- Speculative devirtualization (`cmp [obj], vftable; jne call [edx]`) appears
  when LTCG cannot see the overrides; /GL placeholders for them remove it.
- Large structs carry `static_assert(offsetof(...))` for known offsets; a
  shifted layout once matched in the old quickdiff.
- `D3DXMATRIX.m[i][j]` and `._ij` compile differently (the former reloads
  the other floats); chained assignments store right to left.

- HARNESS_CALLED also changes alias analysis inside the callee: with every
  caller visible, LTCG knows pointer arguments only point at callers'
  locals and keeps loads cached across stores and calls. An /INCLUDE'd
  function loses that (CWaveFile::Read, save_vm_tree). So does a callee
  whose caller also passes the same local's address to an /INCLUDE'd
  function: save_vm_tree only kept caching `*size` once load_vm_tree, which
  anm_effect_2_on_copy_1 hands the same `&child_size`, became
  HARNESS_CALLED too.
- `this->member % n` in an inline member keeps its `idiv` even when LTCG
  inlines it with a constant n (BulletManager's `*_multiple_of`).
- Pass-through members (every caller goes through `g_X->`): write the body
  with `this`; naming the global explicitly drops the `push ecx` slot.
- Taking a global's address anywhere (even in a harness) keeps stores to it
  ordered with pointer stores.
- LTCG can split a function: an early check stays inlined in the callers and
  the rest is out of line (RestoreBuffer). A stand-in struct whose member
  gets the remaining argument as `this` reproduces it.
- Nested inlining can differ per call site (the CSound ctor inlines
  FillBufferWithSound and ResetFile; the out-of-line FillBufferWithSound
  calls ResetFile). Each shape needs its own `__forceinline` copy.
- A folded constant argument can move into the callee (CreateStreaming
  pushes the "thbgm.dat" literal itself).
- `delete p` goes through a `??_G` helper LTCG may not inline;
  `p->~T(); operator delete(p, sizeof(T));` gives the inlined form.
- A class with a vtable and double members needs `#pragma pack(4)`, or the
  vfptr is padded to 8 bytes (CSound).
- An array of a class with a constructor nested in an array element stops
  LTCG from unrolling the construction loop (PlayerOption::unk_4).
- How a struct is split into members picks `rep stosd` vs `movups` for its
  zeroing (TitleInf ctor: `AnmId; AnmId[0x24]; AnmId[9]`).
- dxguid IIDs can be defined in source to carry a GLOBAL annotation.

- Frame realignment (`and esp,-8`) spreads up from callees: if a callee
  needs 8-byte alignment, LTCG realigns its callers too. The draw_text
  placeholder holds a volatile double so its callers get it.
- The /GS decision cuts both ways: an opaque stub that receives a local's
  address gives the caller a cookie, which a /GL placeholder body removes
  (set_pos_time, the Interp steps); a visible body can also add one (see
  get_vm_with_id above).
- Some of ZUN's flag fields really are bitfields (EnemyData::flags_low,
  AnmVm blend mode): assignments compile to xor/and/xor. EnemyFlagsLow and
  AnmVmFlagsLoBits are bitfield views of them.
- Parameter folding and dropping `this` only happen once a function is
  HARNESS_CALLED with harness callers (spawn_item, create_vm_front,
  get_boss, LaserManager::find_by_id).
- Loop form decides store/load order: delete_vm_inline walks children with
  `node = &list; while ((node = node->next) != NULL)` so the flags store
  comes first.

- Globals next to address-taken ones (probably one ZUN struct, such as the
  screen block near 0x4d9d10) need their address exposed too (a harness
  taking `&g_x` is enough); otherwise LTCG keeps them in registers across
  pointer stores and turns conditional stores into cmov.
- A jump table without a bounds check comes from `default: __assume(0);`.
- Byte/word store combining follows statement order.
- `this->member % n` inside an inline member function keeps
  `cdq; idiv` even when n is constant after inlining; a free inline
  function (or n as the dividend) gets the magic multiply.
- A struct-returning call whose result is read back from its stack slot
  (not through eax) was inside an inlined helper returning the struct.
- A dropped unused `this` keeps the parameter slot (callers `push ecx`); a
  `__stdcall` static loses it entirely (`ret` instead of `ret 4`).
- LTCG may stop folding a constant `this` once callers in several objects
  pass it (CriticalSections::leave now reads g_CriticalSections directly);
  when a constant caller still keeps `this`, the harness can pass an opaque
  pointer (Supervisor::load_game_config).
- Arguments of inlined helpers are evaluated right to left: load the object
  pointer into a local first. `fabsf` and `fabs` compile differently.
- Doubles at 4-byte offsets in plain structs need `#pragma pack(4)` too
  (Spellcard, Scorefile).
- Not reproduced yet: the original pops each `malloc` argument right after
  the call at most sites; ours merges the pops (Fog::Fog, AnmLoaded::load).
- Some matches depend on unrelated code existing: 0x43c940 stops matching
  as soon as write_screenshot is defined anywhere in the program.
- dxguid.lib is a single object: one unresolved reference pulls in all of
  it, which then clashes with any GUID defined in source for annotation.
  So every GUID the game takes from it is defined in source (DSUtil.cpp,
  SupervisorSetup.cpp), including the axis, POV and key ids that
  dinput8.lib's `c_dfDIJoystick2` and `c_dfDIKeyboard` refer to.
- Where a struct global overlaps globals that are annotated on their own
  (g_GameWindow's flags is g_unk_4d9d1c, its resolution fields are
  g_resolution_x and the rest), code that addresses the field as a global
  must name the separate global; quickdiff and reccmp name the original's
  address after the exact annotation, so `g_GameWindow.flags` shows up as a
  difference even with identical code.
- Supervisor::take_screenshot (0x43bbd0) lost its frame realignment and
  matched once it became HARNESS_CALLED with its one real caller,
  GameWindow::take_screenshot; LTCG then also dropped its unused `this`.

- LTCG folds a constant `this` into a HARNESS_CALLED callee when every
  caller passes the same global. If the original keeps `this` in ecx, also
  call it with a second object from the harness; if it addresses the global
  directly, name the global in the body (open_bgm, preload_bgm).
- A callee whose callers LTCG all sees does not realign its own frame; an
  8-aligned harness caller (an address-taken double) restores it. Remove
  stand-in harness callers once the real callers exist, or they keep
  shaping the callee (FpsCounter::update, take_screenshots). This is
  fragile: adding two unrelated DirectInput callbacks broke three matches.
- A byte test of flags next to a dword `|=` on the same flags avoids common
  subexpression reuse. Fall-through case order shows in jump table tail
  merges.

- LTCG keeps a value in ecx across a call only when it sees every caller of
  the callee: helpers called only from one interpreter (EclStack::enter,
  ecl_return, get_subroutine_ptr) are HARNESS_CALLED for that.
- Wrapper copies depend on call counts: about a dozen extra zun_sinf/zun_cosf
  call sites made LTCG keep sinf/cosf out of line and turned the wrappers
  into thunks. run_std calls sinf/cosf directly.
- Large interpreters need `__forceinline` on the small stack helpers the
  original inlined. Some constants are compared unsigned (`jae`), e.g. MSG
  hold-time checks; ZUN's MSG VM flags are real bitfields.

### Compiler-generated and CRT functions

Name-based annotations: the marker, then a comment line naming the function.

- Scalar deleting destructors: `// SYNTHETIC: TH16 0x...` then
  ``// X::`scalar deleting destructor'``.
- Dynamic initializers (0x401000-0x401250 in the original) and the atexit
  destructors they register (0x48ac10-0x48acd0): name them by symbol, e.g.
  `// SYNTHETIC: TH16 0x401110` then `// ??__Eg_arcfiles@@YAXXZ`, and
  `// ??__Fg_arcfiles@@YAXXZ` for the destructor (see Arcfile.cpp). The PDB
  knows these static functions only by that string; compare.py treats it as
  their symbol. Matching them means defining the global, with its
  constructor and destructor, the way ZUN did.
- Implicit constructors/destructors: `// X::X` or `// X::~X`, or the symbol.
- UCRT functions defined inline in the headers with C linkage:
  `// LIBRARY: TH16 0x4090d0 SYMBOL` then `// _sprintf` (src/CrtInline.cpp).
- The code after the CRT up to 0x48a3c0 and the small functions there are
  exception unwind funclets generated from the functions they belong to;
  they need no source of their own.

### Known tooling gaps

- Template members cannot be annotated: build.py's name parsing does not
  handle `Interp<Float3>`-style names. Static functions defined in a .cpp
  can be (build.py falls back to the static symbol of that object file), but
  static functions defined in a header, such as the per-file `sincosmul`
  copies, cannot, and neither can anything annotated inside a header: move
  an out-of-line copy into a .cpp instead.
- Comments must go above `// FUNCTION:`, not between it and the signature:
  reccmp then loses the function and build.py may misread the declaration.
- quickdiff misreports jump thunks and tail jumps; check those with
  compare.py.
- Overloads are told apart by the object file that defines them and then
  by parameter types (typedefs mapped through `TYPEDEFS` in build.py; add
  new ones there if an overload is reported as ambiguous).
- build.py reads `template <> __declspec(noinline) X::f` as a function
  named `__declspec`; use DECOMP_NOINLINE. An explicit specialization of an
  in-class template member also needs a user in its own .cpp to be emitted.
- The TH06 decomp's `Chain` code (`src/Global.cpp` there) is a close ancestor
  of TH16's `UpdateFuncRegistry`: same callback result codes, same case
  order in the switch, same search-then-cut structure in `unregister`.

## Credits

Names and structure layouts follow ExpHP's
[th-re-data](https://github.com/exphp-share/th-re-data) where possible. The
[TH06 decomp](https://github.com/happyhavoc/th06) is a useful reference for
how ZUN's engine looked in its first Windows incarnation.
