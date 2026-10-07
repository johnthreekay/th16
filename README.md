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
    pass a local's address elsewhere: with get_vm_with_id, only callers that
    call it directly get one; going through an inline helper avoids it (see
    the get_vm_with_id entry below). Check callers for new cookies whenever
    a stub becomes real code.
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

- AnmVm::run is a wrapper that saves and restores g_game_speed around an
  interpreter LTCG inlined: a `__forceinline` run_script gives the single
  epilogue.
- For `*p = f(x)`, `*p = a * call()` and `*p op= v`, MSVC evaluates the
  arguments, then the destination pointer, then the call: compute the
  value into a temporary to get it first.
- `p = cond ? f(&x) : &x;` followed by a load through p gets tail
  duplicated; `p = &x; if (cond) p = f(p);` does not. A ternary between two
  struct lvalues becomes a branchless address select; if/else branches.
- Locals with 1-byte alignment are laid out first in the frame (3-byte RGB
  structs, not ZunColor).
- An x87 call result passed as an inline function's f32 parameter goes
  through a float slot into SSE (`fstp; movss`); used directly in
  arithmetic it stays on x87.
- /INCLUDE'd accessors make callers spill xmm0 around them; HARNESS_CALLED
  restores the interprocedural register use (get_int_var and friends).

- `pos[1]` (D3DXVECTOR3's `operator FLOAT*`) makes MSVC reload the field
  after unrelated stores, unlike `pos.y` (the bullet wall bounces). A static
  helper that stores through a pointer and is inlined forces reloads too,
  and lookups inside it are not merged across calls.
- `D3DXVec2Length((D3DXVECTOR2 *)&v)` matches ZUN's speed recomputation;
  `sqrt(x*x + y*y)` does not.
- `if (c) f(4); else f(3);` is one call with branching pushes;
  `f(c ? 4 : 3)` becomes setne/add. Advancing a pointer parameter in place
  gives `add`, a separate cursor `lea`.
- Open: in an inlined ZunTimer::tick the original adds the speed into
  current_f's register (xmm0); ours adds into the speed's (xmm1), and folds
  the decrement's `* 1.0f`. No source variant found yet; it holds back
  several bullet ex steps, InterpFloat3::step and Spellcard::on_tick_body.

- LTCG knows which memory a visible callee does not write: a local `pos`
  kept across AsciiInf::create_stringf calls stays in its stack slot; write
  `pos.y += 15` on the struct (separate x/y locals become immediates), and
  mind the field store order.
- MSVC does not connect a store through a struct pointer with a later int
  load of the same address: store `*(EnemyRef *)&b->unk_90`, then test
  `b->unk_90`, to get the original's reload.
- `memcpy(buf, ...)` lets the compiler drop a later `buf != NULL` test;
  `memcpy(buf + size, ...)` with `size = 0` keeps it.
- An 8-byte field (`__time64_t date`) stored as one 8-byte zero changes the
  register allocation of an unrolled init loop.
- Calling both CStreamingSound::get_play_time and play_sound_centered from
  one function makes LTCG stop folding play_sound_centered's `this`
  program-wide (12 matches lost). The cause is reading a field of
  g_SoundManager (bgm_stream, or bgm_name's address) in a function that
  also calls play_sound_centered; see the wave 6 notes below.
- Open: our build adds a /GS cookie to functions with a memory-resident
  D3DXVECTOR3 copy that is never passed anywhere (Player::update_options,
  sht_on_tick, AnmVm::world_pos).
- The score file's real layout puts the character sections 8 bytes into
  Scorefile, after two buffer pointers: ScorefileData/ScorefileChara/
  ScorefileStatus are the true view, ScorefileCharacter the shifted one.

- An EH frame in a function that does not realign its frame stops LTCG from
  inlining the UCRT math (cosf, sinf, fabsf, floorf, atan2f) into everything
  it calls, several calls down (PosVel::step). One tiny EH function is
  enough; an EH caller that realigns through the ebx frame does not do it.
  The earlier zun_sinf/zun_cosf "call count" and zun_atan2f observations
  are probably this effect.
- That rule, made precise (wave 5 merge, Player::on_tick_body):
  - It is LTCG's double stack alignment pass: linking with
    `/d2:-NoDoubleStackAlign` inlines cosf/floorf everywhere (diagnostic
    only; `/d2:-inlinelog3` lists every inline, `/d2:-dumpCallGraph:<file>`
    writes the call graph LTCG uses as DGML).
  - The source is a function with C++ EH state at /GL time and no own
    realignment. The EH state counts even when codegen removes the frame:
    `new T` whose constructor is not known nothrow leaves only the dead
    `push ecx; mov [ebp-4], p` (Player::create).
  - It flows down direct call edges and through taking a function's
    address (Player::initialize registering on_tick_callback counts as a
    call). Indirect calls do not carry it, and plain unaligned or /GS
    callers are not sources.
  - Each reached function that would inline a UCRT math helper calls an
    out-of-line copy instead, and only the reached ones: an EH caller of
    zun_cosf costs zun_cosf alone, one of PosVel::step costs all three.
  - It stops at a function that realigns for a double of its own (plain
    `and esp,-8`). An ebx-form realignment that comes from an aligned
    caller does not stop it, and extra unaligned callers do not undo it.
  - Spelling the wrappers out as `(f32)cos((double)x)` dodges the inliner
    but not the alignment: the copies then skip their realignment and other
    frames shift (update_final_pos).
  - Solved (wave 6): 0x405510, 0x4054f0 and 0x405260 are not ZUN
    wrappers but LTCG's out-of-line copies of the UCRT inlines, annotated as
    `_sinf`, `_cosf`, `_floorf` (CrtInline.cpp). What the pass reaches is a
    call graph node, and an inline helper is a node of its own: ZUN's call
    sites go through small inline helpers (`zun_sinf`, `zun_cosf`,
    `zun_floorf` in ZunMath.h, plain `inline`). The pass reaches the helper
    nodes, so LTCG keeps the UCRT body out of line for them, then inlines
    the helpers, which leaves each caller calling the copy: our callers of
    the copies are now the same set as the original's (PosVel::step, the
    laser and collision rotations, check_hit_rotated_rect, ecl_run,
    run_std, ...). A large function that calls sinf directly is not
    affected the same way: it inlines it and realigns its own frame
    (PosVel::step then realigns, update_final_pos loses its padded frame and
    the floorf copy loses its 64-byte realignment). The earlier noinline
    wrappers were reached nodes too and became `push ecx; call; pop ecx`
    thunks to separate copies. A DECOMP_NOINLINE redeclaration keeps floorf
    out of line program-wide but has no effect on sinf and cosf.
  - `push ecx; call f; pop ecx; ret` (instead of `jmp f`) in a thunk is
    stack alignment padding: LTCG knows the thunk is entered 8-aligned.
    Player::on_tick_callback gets it once Player::create is HARNESS_CALLED
    and its stand-in caller realigns its frame like GameThread::thread_start
    (0x42cb60, `and esp, -8`), the original's only caller; taking the
    callback's address in Player::initialize passes the alignment on.
    lose_life matches with it, and on_tick_body gets the original's ebx
    frame.
- A caller that realigns its frame gives its callees known alignment: they
  get padded frames and lose edi shrink-wrapping, even with other callers.
  Whether a function realigns is decided over the whole function, not per
  local. ecl_run_over_300 keeps five cases in noinline helpers so it stays
  below the threshold until its `new Fog` EH frame can be restored.

- Float arguments in an LTCG custom convention go to xmm registers by
  position (counting `this`): the float at position 3 lands in xmm3, others
  stay on the stack. Parameter order matters (compute_damage_to_enemy).
- `Float3 p; p = a + b;` gives the original's unpcklps temp and movq copy;
  `Float3 p = a + b;` stores field by field.
- Early `return count` paths converge on one reload exit when the tail is
  nested under `if (count != 0)`. Identical switch cases written out
  separately keep the jump table.
- DECOMP_NOINLINE must be on a virtual's in-class declaration to stop
  speculative inlining; on the out-of-class definition it does nothing.
  A callee whose only caller is newly decompiled may need DECOMP_NOINLINE.
- quickdiff's percentages are unreliable for functions with jump tables;
  use compare.py there.

- The input words g_hardware_input_pressed/_repeat are fields of the
  address-taken input struct at 0x4a50b0 in the original; ours must be
  address-exposed or menu code keeps them in registers. They are now macros
  for fields of `g_input` (src/Input.h), whose address InputState::update
  and WinMain take, which is enough. input_pressed_or_repeating inlines as
  an if/return pair: `(p & m) || (r & m)` merges into `(p | r) & m`, a
  forceinline helper does not.
- Shared tail blocks come from duplicated statements the compiler
  tail-merged, not from goto: write the statements twice.
- AnmId::find_or_clear is called at all 25 original sites (DECOMP_NOINLINE).
- get_vm_with_id (0x46efa0) is real code now (HARNESS_CALLED). With its
  body visible, a function that calls it directly from its own body and
  passes a local's address elsewhere (a D3DXVECTOR3 to create_stringf,
  world_pos or create_vm) gets a /GS cookie the original does not have;
  the same lookup through an inline helper node (get_vm_or_clear, the new
  get_vm, find_child_of) does not. Seven functions gained cookies that way;
  PauseMenu::on_draw (get_vm_or_clear then find_child_of, three lookups),
  Spellcard::on_draw_body (get_vm_or_clear) and BombAyaSubInf::on_tick
  (get_vm for the second VM) match again written with the helpers, which
  also replaces the old hand-written "g_AnmManager read once" forms. Fog's
  destructor, interrupt_child and update_options_sprites match since. Open:
  EffectManager::next_index and anm_effect_2_on_copy_2 matched against the
  opaque stub and now allocate registers differently (this and the saved
  index trade ebx and a stack slot; a loop counter is spilled); no source
  form found yet. The original has 552 direct calls to get_vm_with_id.
- An 8-byte struct local (a D3DXVECTOR2, a D3DLOCKED_RECT) makes LTCG
  want an 8-aligned frame: the function realigns and so does every
  visible caller, up the call graph. ZUN's draw_vm keeps width and height
  in separate floats; with a Float2 three callers of draw_vm lost their
  match. convert_texture (0x46c0d0) made its caller load_texture_from_file
  realign until that caller stopped being /INCLUDE'd (wave 6 notes below);
  it now has a plain D3DLOCKED_RECT.
- LTCG's call graph is built before inlining, and an inline helper is its
  own node there. Taking a function's address counts as a call: in a
  function that realigns its frame (WinMain), it gives the target known
  alignment, so SoundManager::thread_init got a padded frame and
  SoundManager::initialize realigned. And a caller that also stores to a
  global's fields does not get the global folded into a callee's `this`
  (release, called right after `g_SoundManager.thread_state = ...`).
  WinMain reaches both through inline helpers (start_sound,
  stop_sound_threads), which restores all three; check WinMain's other
  direct `g_X.method()` calls the same way.
- The implicit constructors of the three vertex structs (0x46a370,
  0x46a380, 0x46a390) stay out of line, called from the dynamic
  initializers of g_quad_vertices_4df4a8, g_sprite_temp_buffer and
  g_quad_vertices_4df8a0 (`// SYNTHETIC:` with `X::X`).
- Real callers replacing stand-ins settle matches:
  collision_line_intersection (0x404220) matched once the laser graze
  checks called it instead of a harness, and StageInner::draw_vms once
  AnmManager::draw_vm had a real body (draw_vms then realigns through ebx
  like the original).
- More callers can stop LTCG inlining a UCRT inline in one place: with
  Bullet::run_ex's calls, Player::angle_to_player had to spell out
  atan2f's body to keep it inline.

- An /INCLUDE'd function never gets known stack alignment from its
  callers: if it or a callee wants 8-byte alignment, it realigns itself.
  load_texture_from_file realigned as soon as convert_texture had a body;
  kept alive by its real caller (setup_entry) instead, it inherits the
  alignment load_next_entry provides and matches. setup_entry stopped
  realigning the same way.
- Without /INCLUDE, a static (`__stdcall`) function gets LTCG register
  arguments (ecx, edx), while a member that ignores `this` keeps its
  stack arguments and only loses `this` (load_texture_from_file,
  reload_texture, draw_replay_entry). Use the member form when the
  original keeps `ret N` with every caller visible.
- The "this folding" break above, made precise: a function that reads a
  field of g_SoundManager itself (a load of bgm_stream, or taking
  bgm_name's address) and also calls g_SoundManager.play_sound_centered
  stops the folding everywhere. Through an inline SoundManager member
  (bgm_play_time, seek_bgm, get_bgm_name) the access belongs to another
  call graph node and the folding stays; an inline BgmStream member does
  not help, because the caller still reads g_SoundManager.bgm_stream.
- PauseMenu::tick_open: the jump table puts cases 12/15 before case 11,
  and two shared tails are gotos (the score tail reached from cases 2-5,
  the Esc tail reached from case 6); written twice, the compiler kept
  the first copy instead. Where only a suffix is shared (the Q key path,
  `set_cursor(1); set_unk_1f4(16)`), cross-jumping keeps the earlier copy
  in ours and the later one in the original; no source form found.
- Open: `menu.next_selection % 13` compiles to `mov reg, 13; idiv` in
  tick_open, do_replay_save and do_score_name_entry, where the original
  multiplies by the reciprocal; draw_keyboard's `i % 13` does multiply.
  Not the expression form (a free inline helper, fewer uses of 13) and
  not the frame realignment.
- When two variables have the same use counts, the order of their first
  assignment decides which gets the callee-saved register: in
  convert_texture's 32-bit loop clearing b first puts b in ebx and the
  count in esi like the original.
- Early vs late realignment (wave 6, GameThread/Gui/Item ticks): a caller
  whose realignment LTCG knows before its callees are compiled (IL-level
  double math such as a `(double)` printf argument or an inlined atan2, or
  a direct call to a function that needs alignment, even one that realigns
  itself like Player::angle_to_player or CStreamingSound::seek) hands
  known alignment to its callees: they get padded frames and lose
  shrink-wrapping (Stage::start_std_vms, Item::init_anm,
  AsciiInf::create_number all lost their matches this way). The original
  has the same callers realigning without that effect, so its decision
  came later. Not reproduced: the affected callers keep that double math
  in small DECOMP_NOINLINE helpers (seek_bgm_to_stage_time,
  item_angle_to_player, draw_percentage) until it is understood. Moving
  them out also costs matches that depend on the early alignment
  (Item::collect_full_power, the GameThread stage restart helpers).
  Bisecting by commenting out parts of the caller finds the culprit in a
  few builds.
- A call to zun_fabsf from Gui::on_tick_body (no EH frame, no cookie, no
  realignment) is enough to make LTCG stop inlining fabs into zun_fabsf;
  the HUD inlines fabsf for now.
- A memory-resident D3DXVECTOR3 local built from a non-constant value
  gets a /GS cookie (and the caller then realigns through ebx); one built
  from all zeros does not. Separate float locals avoid it (Gui::on_tick_body),
  but Gui::sub_426d70 keeps the cookie where the original copies the
  struct from memory.

- ANM TODO sweep (group 5):
  - A device/vtable register swap around a D3D call is often fixed by
    `IDirect3DDevice9 *device = g_Supervisor.d3d_device; device->X(...)`
    (setup_vertex_buffer, flush_sprites). Not every swap: mode_9/mode_11's
    SetTexture did not move.
  - Commutative operands of two memory values: the operand created later
    in the IL is loaded into the destination register. `a + b` loads b
    first, also through D3DXVECTOR3's operator+: ZUN wrote
    `entity_pos + pos + pos_2` (get_own_transformed_pos, world_pos). For a
    temporary's fields read later, the order of the field assignments
    decides: draw_vm__mode_7 matches D3DXVec3Length's accumulator only with
    `diff.y` assigned before `diff.x`. Writing the operands of `x * x + y * y`
    the other way round changes nothing.
  - A load that the original does before an aliasing store: compute it into
    a local first (`root = parent->parent ? ... : parent;` before the
    `unk_5b0` store in restore_snapshot_vm).
  - ZunTimer::tick_split's shape (speed pointer loaded before `previous` is
    stored, `*speed + current_f`) is the original's in anm_effect_3_on_tick
    and anm_effect_2_on_tick; tick_in_place stores `previous` first. Other
    tick_in_place users (StageInner::step_fog, Gui) may want tick_split.
  - A function that passes a local's address to itself (world_pos's
    recursion) gets a /GS cookie the original does not have. With the body
    in a `__forceinline` helper the cookie goes, but alias analysis then
    keeps `parent` cached across the result stores; not solved.
  - Several "ours never uses ebx" functions (anm_on_draw_masked,
    draw_triangle_fan, draw_circle_outline, draw_ring) differ only in that;
    draw_triangle_fan's original frame is consistent with known 8-byte
    alignment from its only caller, anm_effect_3_on_draw, which realigns in
    the original and not in ours.
  - AnmVm::run case 2: `instr_offset = -1; return 0;` written separately
    gives the original's jump to the final `xor eax, eax`, but then case 1's
    `return 1` merges with the on_wait `return 1`; the fallthrough form keeps
    case 1 right and case 2 wrong.

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
- The `// GLOBAL:` parser does not take a constructor call with
  arguments (`ScreenEffect g_screen_effect(...);`); write it as copy
  initialization (`= ScreenEffect(...)`), which compiles the same.
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
- `// FUNCTION:` only finds C++ symbols; WinMain (C linkage) is annotated
  by its linker symbol: `// SYNTHETIC: TH16 0x459830 SYMBOL`, then
  `// _WinMain@16`.
- The TH06 decomp's `Chain` code (`src/Global.cpp` there) is a close ancestor
  of TH16's `UpdateFuncRegistry`: same callback result codes, same case
  order in the switch, same search-then-cut structure in `unregister`.

## Credits

Names and structure layouts follow ExpHP's
[th-re-data](https://github.com/exphp-share/th-re-data) where possible. The
[TH06 decomp](https://github.com/happyhavoc/th06) is a useful reference for
how ZUN's engine looked in its first Windows incarnation.
