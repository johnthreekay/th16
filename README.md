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
- Wave 6 (lasers):
  - normalize_angle as HARNESS_CALLED instead of DECOMP_NOINLINE (which
    /INCLUDEs it) lets callers keep values in xmm3 across the call;
    LaserInfiniteInf::check_graze_or_kill matched and nothing was lost.
    Worth trying for other small /INCLUDE'd helpers with many callers.
  - A constructor called at most sites but inlined at one (LaserLineInf:
    ten calls, inlined in clone) gets DECOMP_NOINLINE plus a
    `__forceinline` tag constructor (`LaserLineInf(InlineCtor)`) for the
    inlined copy.
  - allocate_new_laser is inlined into the wall bounce and the bomb
    cancels (with LaserCurveInf's constructor inlined there):
    `allocate_*_laser_inline` helpers. Inside allocate_new_laser, linking
    the laser in every case (tail-merged) matches the register use.
  - `tip += position` (D3DXVECTOR3::operator+=) and field-wise adds compile
    differently: only the former stopped LaserLineInf::method_50 from
    caching position in xmm registers (it matched).
  - The et_ex switches compare the type unsigned (`ja`, a 0x80000000 case):
    `switch ((u32)ex->type)` restores the jump table for 1..16.
    EnemyBulletShooter::aim_type is written as a word.
  - Loads the original hoists in front of an inner loop (the previous
    segment's speed and angle, the out pointers) come out the same when
    written as locals before the loop (curvy laser segment placement).
  - A struct copy done member by member in the original (movq per Float3,
    dwords for the rest) needs a field-wise operator= (LaserCurveNode).
  - A search loop whose normal exit returns: write `goto found` in the
    loop and `return` after it; `break` plus `if (i >= n)` re-tests n.
  - The laser velocity steps add their vector with D3DXVec3Add
    (`v = vel * g_game_speed; D3DXVec3Add(&unk_60, &unk_60, &v)`): its
    stores through pointers give the original's add, store, add, store
    order and reloads; `unk_60 += v` does not. Both method_3c matched.
  - Open: the original loads g_game_speed once for three position
    components (`position.x = v.x * g_game_speed + position.x` and so on);
    ours reloads it after each store. Defining g_timer_speed_ptrs in a /GL
    file instead of the stub did not change that, and neither did
    D3DXVec3Scale/D3DXVec3Add (those load it once but add in a different
    order).
  - Operand order of a commutative add or multiply often does not follow
    the source: swapping `a + b`, `a += b` or the timer tick's operands
    gave identical code in several places (LaserCurveInf::initialize,
    method_44, check_graze_or_kill); look for a different construct.
- TODO sweep (lasers and system code):
  - A value stored to a global and then tested: testing the global
    (`if (!g_Supervisor.present_params.Windowed)`) keeps the boolean in a
    register, because LTCG forwards the store across the visible
    set_resolution_from_config call; testing the local lets the compiler
    re-derive the condition from its inputs (create_game_window).
  - Stores the original merged into `movaps` to fields of a big struct
    global that code addresses as its own global (GameWindow's pacing table
    at 0x4d9d90) need a separate global struct the compiler knows is
    16-byte aligned: `DECOMP_ALIGN16` (decomp.h). Spelling
    `__declspec(align(16))` on the annotated line makes reccmp's GLOBAL
    parser take `__declspec` for the variable's name.
  - `if (a) X; else if (b) X; else Y;` (the two X tail merged) lays X out
    first; `if (a || b) X; else Y;` puts Y first (read_keyboard_input's
    Acquire branch).
  - `D3DXVec3Add(&out, &a, &b)` and `+=` give different per-component
    operand orders; one of them often matches where the other does not
    (LaserLineInf::check_graze_or_kill: `D3DXVec3Add(&start, &start,
    &position)`). Neither fixes every component everywhere
    (LaserCurveInf::on_draw).
  - The inlined collision_test_circle_rect in the method_1c variants
    recomputes `h * 0.5f` and `fabsf(y)` in each test, as code that does
    not dominate the later tests would: the original writes them inline
    in the first two tests, with only half_w and fabsf(x) as variables.
    Open: it also squares each corner difference again in every test
    where ours reuses the squares.
  - Address-taken neighbours: g_early_arcade_offset_x/_y belong to the
    address-exposed screen block too; taking their address (harness s6)
    put LaserCurveInf::on_draw's `pos.z = 0` store after their load.
  - Padded frames (`push ecx` with no local, esi saved on entry) in
    LaserManager::initialize come from known alignment down the call
    chain: GameThread::thread_start realigns in the original but not in
    ours, so its callees (LaserManager::create, initialize) stay unpadded.

- TODO sweep (enemies, ECL, bullets):
  - quickdiff counts unknown data addresses as equal, so a float constant
    one ulp off still shows MATCH; reccmp prints it as `<OFFSETn>`. The
    cancel item spread is `ZUN_PI / 180.0f * 10.0f` (0x3e32b8c2), not
    `ZUN_PI / 18.0f` (0x3e32b8c3). Run compare.py on quickdiff MATCHes
    that use float literals. Conversely quickdiff misreads
    EnemyManager::initialize (73%), which reccmp reports as 100%.
  - ZunTimer::tick_mixed (the int frame in a local, current_f updated in
    each branch) gives the unscaled path its own xmm0 register: it matches
    Bullet::step_ex_03 and ScreenEffect::on_tick_pulse and helps a dozen
    other inlined ticks. As tick()
    itself it costs four matches, so it is chosen per call site. Still
    open: whether the scaled path loads current_f and adds the speed or
    folds current_f into the speed's register differs between near
    identical callers (kill_all and kill_all_no_set_death come out
    exactly the other way round from the original).
  - Declaration order of locals decides evaluation order where the
    expression order does not: bullet_in_circle computes dy first only
    with `dx` declared before `dy`, whichever way the sum is written.
  - Contiguous int and float arrays copied with one memcpy give the
    original's three movups (ecl_enm_create); two loops do not.
  - Bullet::cancel matches with `D3DXVECTOR3 delta = ...; pos += delta;`
    (per-field `pos.x = pos.x + delta.x` and `pos += velocity * ...` each
    fold one component differently) and the ANM file loaded into a local
    before create_vm, so it is read before the arguments are pushed.
  - interp_common_methods: the two-branch curves assign x in both
    branches and multiply or return once after the if/else, which the
    original's per-case result registers show (69% to 83% in reccmp).
  - BulletManager::on_draw_callback's push ecx/pop ecx padding comes with
    a harness standing in for thread_start's aligned call of
    BulletManager::create (create and initialize HARNESS_CALLED), but then
    on_tick_callback pads its tail call too: in the original on_tick_body
    realigns itself (and esp,-8), so on_tick_callback can jump to it.

- TODO sweep, group 3 (Gui, stage, game thread, HUD text):
  - Inlined timer ticks: ZunTimer::tick_split (current_f stored in each
    branch) matches EndingChildF0::run and ScreenEffect::on_tick_pulse,
    where tick gave the other xmm register. Still open: in on_tick_flash,
    on_tick_hold and on_tick_shake the original adds current_f from memory
    into the speed's xmm1; every tick form and operand order loads it into
    xmm0 instead.
  - `delete_vm_inline_and_clear(id)` (an inline helper) keeps the id clear
    before the next lookup's push, like the original; spelling it out with
    a cached `anm` lets the push move ahead (Gui and GuiMsgVm destructors).
  - `strcat(path, ".wav")` in play_bgm_wav and start_dialogue is strlen
    plus byte stores that combine into one immediate dword, the terminator
    taken from the zero the strlen loop ended on (append_wav_extension in
    Supervisor.h). strcat, or strcpy/memcpy of the literal, copy it from
    memory; indexing the local array (`path[len + 4] = 0`) adds a /GS range
    check, so the stores go through a pointer.
  - `if (x == 4 || x == 16)` put that body after the next else-if's;
    separate branches with the same body (tail merged) give the original's
    layout (GameThread::~GameThread).
  - The order of stores in the source decides register assignment even
    where the result is scheduled the same: GameThread::create matches with
    `g_GameThread = thread; thread->replay_mode = ...; flags.paused = 1`.
  - `p->x += (__int64)d` evaluated the pointer after the conversion call;
    taking the field's address into a local first loads it before the call
    (GameThread::update_play_time, whose conversion is to unsigned
    __int64: __dtoul3, which quickdiff does not tell from __dtol3).
  - A fade_out_bgm inlined as `modify_bgm(..., c ? 3.0f / speed : 3.0f, ...)`
    keeps the hoisted constant in the register the original uses; the
    if/assign form copied it from another (EndingChildF0::run).
  - AnmVm::search_children is HARNESS_CALLED now that all its callers are
    real: LTCG then keeps xmm values and g_AnmManager in registers across
    find_child_of (set_textbox, set_textbox_width; TitleInf's
    update_options_cursor went to 99.9%). `w + 16.0f` written at each store
    keeps the add after the first lookup; adding into the parameter first
    hoists it.
  - GameThread::thread_start does not realign its frame like the original.
    Forcing it (a volatile double in it, harness_w3d_player's removed)
    matched 12 more functions it reaches (Stage::create, load_data,
    load_std, update_std_vms, on_draw_06, Gui::initialize,
    LaserManager::initialize, render_layer, ...) and lost
    Stage::start_std_vms and AsciiInf::create_number (early alignment, see
    above), and GameThread::on_tick_callback got the aligned
    `push ecx; call; pop ecx` thunk where the original jumps (its on_tick_body
    realigns itself). Nothing tried makes LTCG realign it: HARNESS_CALLED on
    its callees that realign in the original (AnmVm::run, get_runtime,
    PlayerInner::repopulate_options) or on the managers' create functions,
    nor four extra harness functions spilling doubles. thread_start itself
    is only reached through thread_start_callback (`jmp`). Our
    repopulate_options realigns through ebx with a /GS cookie where the
    original realigns plainly without one; that may be the missing source.
  - Gui::create_vm_110 realigns in the original for its zero D3DXVECTOR3
    temporary (movq copy, z through a stack slot); ours emits the same code
    without realigning, HARNESS_CALLED or not.

- Group 4 sweep (menus, replays, sound):
  - The menu states realign (and esp, -8) because they call
    TitleInf::set_substate: every caller of set_substate realigns in the
    original and only those (do_manual and do_spell_practice_character
    call set_state and create_effect but not set_substate, and do not).
    A dead `double` local in set_substate (HARNESS_CALLED) reproduces it:
    LTCG's double stack alignment pass counts IL-level double locals even
    when the optimizer deletes them, and a callee that wants alignment
    and sees all its callers makes them realign. Their callees then get
    padded frames (update_key_config_cursor's 12-byte frame).
  - The reverse: double math inside an inline helper belongs to the
    helper's call graph node, so the function that inlines it does not
    realign early. CStreamingSound::get_play_time realigns like the
    original only with the body spelled out instead of the play_time
    helper. This may be the missing piece for the README's "early vs late"
    item (callers that realign without handing alignment down): keep the
    double math in a plain inline helper, not a DECOMP_NOINLINE one.
  - `x % 13` on a member (`menu.next_selection % 13`) or a global
    (`(g_demo_replay_index + 1) % 3`) compiles to `mov reg, n; idiv`; the
    same through a local copy (`i32 selection = menu.next_selection;`)
    gets the magic multiply like the original. The demo index is still
    open: the original divides by -3 (imul 0x55555555; sub; sar 1), and
    `% -3` compiles like `% 3` here.
  - An empty log function declared `(const char *fmt, ...)` loses its
    one-argument calls (LTCG drops them); declared `(...)` it keeps them
    (dsutil_debug_log, CWaveFile::open_file).
  - `get_vm_or_clear(id) == NULL` before `id = create_effect(...)` keeps
    the dead `id = 0` store; the original calls get_vm_with_id directly
    there.
  - Menus that save and compare the selection through the menu pointer
    (`[esi + 4]`) use the menu_save_selection/menu_selection_moved
    helpers; written on the member they address `[this + offset]`.
  - Open, shared by about eight menu functions: for
    `anm_id_73c = g_AsciiManager->ascii_anm->create_effect(0x13, -1, NULL)`
    the original loads g_AsciiManager into eax and the result slot into
    ecx (pushed before ascii_anm is loaded); ours loads g_AsciiManager
    into ecx. Not a named local, a pointer local, an inline helper or
    making create_effect HARNESS_CALLED.
  - Open: `stage = x + 1; stage_num = stage; weird_stage_num = stage;
    g_stage_data = &g_stage_table[stage];` keeps stage in ecx with the
    imul hoisted above the stores in the original; ours coalesces it into
    eax (do_replay_menu and two spell practice states).

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

- Source shapes found in the player/bomb TODO sweep:
  - A local pointer to a member changes which register addresses it:
    `AnmVm *vm = &this->vm;` before copy_vm and the field stores gives the
    original's stores through the VM pointer (Player::initialize).
  - `AnmLoaded *anm = g_Player->anm_file;` before a struct-returning
    create_vm/create_effect call loads the object first, which decides the
    registers around the call (BombReimuAOrb::start). With one such local
    per branch the script argument is no longer hoisted above the branch
    (PlayerBullet::create).
  - `vm->rotation.z = x;` before `vm->flags_lo |= ...` gives the original's
    load of x ahead of the `or` (PlayerBullet::create, Marisa's bomb).
  - `v += *p` with `Float3 *p = &member` and `v += member` give different
    addss operand orders (BombCirnoAInf::on_tick).
  - The same loop through an inline helper and written in place compile
    differently: Globals::season_level's loop tests its counter in
    BombInf::activate, the loop written out there tests the pointer like
    the original.
  - Player::do_graze: `Player *player = g_Player;` first decides esi/edi;
    reading graze_in_chapter into a temp before graze gives the
    interleaved cmov clamps; `(player->inner.pos + *pos) * 0.5f` gives the
    midpoint's scheduling; atan2 spelled out (as in angle_to_player) keeps
    it inline, which realigns the frame and makes the spawn_item call fold
    unk_3 and unk_6 like the original.
  - A union in BombReimuAOrb puts a PosVel member over pos (pos is its
    first field), so update addresses it through `this` instead of a
    second pointer register.
- /GS: direct calls to AnmManager::interrupt_tree are a cookie trigger as
  well (sht_on_tick_4470f0 and BombMarisaAInf::on_tick lose their cookie
  when the calls are removed). An inline wrapper node around it does not
  help, and neither does defining g_anm_on_switch_funcs (the table its
  inlined AnmVm::interrupt calls through) with its real entries in /GL
  code. get_vm instead of a direct get_vm_with_id call removed the cookie
  from BombAyaAInf::begin and BombReimuAOrb::update.
- do_shooting's ebx-form realignment does not come from
  AnmLoaded::create_effect: making that realign (a volatile double)
  changes nothing in do_shooting or tick_shooting_state.
- Sweep round 2, list B:
  - Padded frames (`push ecx`, or a `sub esp` 4 bytes bigger than the
    locals need) in a function whose callers are aligned come from a callee
    that wants 8-byte alignment. A dead double in that callee (marked
    HARNESS_CALLED, all callers real) reproduces it: InterpAngle::step
    matched with one in ZunAngle::operator*, its only callee nobody else
    calls. The callee must not itself show signs of known alignment in the
    original: a dead double in GuiMsgVm::show matched leave_state_1 but
    cost show its edi shrink-wrapping.
  - AnmVm::run is that callee for interrupt_child_and_run, set_vm_script
    and the LaserLineInf/LaserInfiniteInf initializes (which realign in the
    original): its double math sits in the `__forceinline` run_script, a
    helper node of its own, so ours never counts as wanting alignment. A
    dead double in a HARNESS_CALLED run matches both initializes but loses
    nine others (start_std_vms, the Gui interrupt_spell_vms_2/3, Spellcard::end,
    Item::init_anm, the TitleInf cursor updates, ...), whose callers would
    need known alignment as well. Making run, set_vm_script, get_runtime or
    leave_state_1 HARNESS_CALLED alone changes nothing.
  - AnmVm::step_interpolators matched once HARNESS_CALLED (with the
    InterpAngle, InterpInt3 and InterpFloat2 steps): /INCLUDE'd, it
    realigned for InterpFloat2::step's D3DXVECTOR2.
  - quickdiff MATCHes that reccmp reports below 100% can be real
    differences hidden by "call targets count as equal": the player data
    screen called `__alldiv` where the original calls `__aulldiv` (the play
    time is an `unsigned __int64`). sigscan.py adds a CRT helper to lib.csv
    only once our build calls it.
  - Library data (dinput8.lib's c_dfDIKeyboard and c_dfDIJoystick2) can
    carry a `// GLOBAL:` annotation on an `extern "C"` redeclaration;
    reccmp then names both sides the same.
  - Data from one object file is not laid out in definition order (both
    orders put the anchor tables before g_sound_effect_table), so the
    original's adjacency of g_anchor_corners_x after the sound table could
    not be reproduced for SoundManager::initialize's end pointer.
  - A struct local copy-initialized inside a block (`ZunAngle tmp =
    initial;` in a branch) gets a stack slot that function-scope locals do
    not; the original's InterpAngle::step code needs the block form.

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
- build.py's incremental build does not record headers that files in
  src/harness/ (and the other subdirectories) include as `"../X.h"`: only
  headers reached through other headers end up in the `.dep` list. After
  changing such a header, delete `build/obj/src/harness/*.dep` and
  `build/sym/src/harness/*.dep` (or all `.dep` files), or the link can
  fail on a stale object or quietly use one.
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
