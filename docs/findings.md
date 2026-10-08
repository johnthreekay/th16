# Matching findings

What we learned getting MSVC 19.10 with `/GL` and `/LTCG` to reproduce
`th16.exe`: the source shapes that decide code generation, the
whole-program effects that make one function depend on others, and the
differences still open (marked "Open"). Each unmatched function's
`// TODO:` comment in `src/` says what still differs in it; these notes
explain the patterns behind those comments.

The general notes are in the order they were found, and later entries
refine earlier ones ("made precise"). The sweep notes after them come from
passes over the remaining TODOs, grouped by game area. The workflow they
assume is in [workflow.md](workflow.md).

## General

- Function call shapes produced by LTCG are listed in
  [workflow.md](workflow.md#whole-program-optimization). One more: when a
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
  the callee is visible to LTCG and not known nothrow (while callees were
  still stubs, an `LTCG_NOTHROW` macro declared them nothrow).
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
  load of the same address: store `*(EnemyRef *)&b->target_enemy_id`, then test
  `b->target_enemy_id`, to get the original's reload.
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
  item_angle_to_player, draw_percentage) until it is understood. (Since
  2026-10-08 the seek is written inline in GameThread::on_tick_body and
  create_number is called through a function pointer; see "Second pass
  over GUI, stage and GameThread" below.) Moving
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
- The TH06 decomp's `Chain` code (`src/Global.cpp` there) is a close ancestor
  of TH16's `UpdateFuncRegistry`: same callback result codes, same case
  order in the switch, same search-then-cut structure in `unregister`.

## Sweep notes

### Wave 6: lasers

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

### TODO sweep: lasers and system code

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

### TODO sweep: enemies, ECL, bullets

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

### TODO sweep: Gui, stage, game thread, HUD text

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
- (Solved in sweep round 2, see below.) GameThread::thread_start does
  not realign its frame like the original.
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

### TODO sweep: menus, replays, sound

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
  helper. This may be the missing piece for the "early vs late"
  entry under General (callers that realign without handing alignment down): keep the
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

### TODO sweep: ANM

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

### TODO sweep: player and bombs

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
  well (sht_on_tick_sideways and BombMarisaAInf::on_tick lose their cookie
  when the calls are removed). An inline wrapper node around it does not
  help, and neither does defining g_anm_on_switch_funcs (the table its
  inlined AnmVm::interrupt calls through) with its real entries in /GL
  code. get_vm instead of a direct get_vm_with_id call removed the cookie
  from BombAyaAInf::begin and BombReimuAOrb::update.
- do_shooting's ebx-form realignment does not come from
  AnmLoaded::create_effect: making that realign (a volatile double)
  changes nothing in do_shooting or tick_shooting_state.
- Player::on_tick_body has two `time_in_state.current % 3`: the original
  divides in the blue flash (`mov ecx, 3; idiv`) and multiplies in the
  scale check. A local declared before the if (`i32 time = ...;`) gets
  the multiply but is loaded ahead of the `&&`'s first test; assigning it
  in the condition (`(time = inner.time_in_state.current) % 3 == 0`)
  loads it where the original does. A free inline helper taking the
  member keeps idiv.
- D3DXVECTOR3 `a + b` with both operands in memory loads b and adds a
  from memory (`pos + halfsize` loads halfsize); swap the operands for
  the other order. With a product on one side (`pos + halfsize * scale`),
  and for `v * f` against `f * v` with v a member, the operand order
  changes nothing. With both operands in registers it does:
  `player_scale * (halfsize * 0.5f)` copies the scale (movaps) before each
  multiply like the original, `halfsize * 0.5f * player_scale` multiplies
  in place.
- on_tick_body's scaled boxes: `f32 scale = player_scale;` puts the scale
  in xmm7 and 0.5f in xmm6; reading the member in every line still loads
  it once, into the original's xmm6 (0.5f in xmm5). The remaining
  differences there are scheduling and whether pos.x/halfsize.x are folded
  into addss/mulss. Removing the state switch or the blue flash code
  changes that scheduling; the code after the block no longer does.
- `(~(inner.flags >> 2) & 1) && !(inner.flags & 0x10)` gives the original's
  `mov eax, ecx; shr eax, 2; not eax; test al, 1` and a separate
  `test cl, 0x10`. `!(flags & 4)`, `!((flags >> 2) & 1)`, `== 0` and a
  bitfield view all merge both tests into `test byte ptr [...], 0x14`.
  sht_on_tick_sideways (0x4470f0, bit 0 of the enemy flags) and 0x428a9f
  have the same `not; test al, 1` shape, which may be the same fix.
- An empty switch case that keeps its compare (`cmp eax, 4; je` to the
  end) comes back with a one-argument debug_log call in the case: LTCG
  drops the call after the switch is lowered (on_tick_body, state 3).
- `x->angle = wrap_angle(x->angle + x->angular_speed)` loads angle first
  in either operand order, also through add_normalize_angle or a local;
  `x->angle += x->angular_speed;` loads angular_speed first like the
  original but keeps a store of the unwrapped angle.
- The `*speed * 1.0f` the original keeps in decrement (TODO at 0x40d490)
  is not from late inlining: an out-of-line decrement(f32) in
  ZunTimer.cpp, inlined by LTCG, folds it too.

### Sweep round 2, list B

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

### Sweep round 2, list A

- GameThread::thread_start realigns like the original with a double
  local (`double zero = 0.0;` for the FpsCounter stores); a dead double
  in a HARNESS_CALLED callee (Stage::create) did the same. That gave 11
  matches below it (Stage::create, load_data, load_std, on_draw_06,
  Gui::initialize, load_stage_files, LaserManager::initialize,
  render_layer, the padded Gui::on_tick_callback and
  BulletManager::on_draw_callback thunks).
- The original does not hand that alignment to every callback the
  aligned code registers: GameThread's, Stage's and BulletManager's
  on_tick_callback jump to bodies that realign themselves, and
  Gui::on_draw_2_callback's body realigns, so AsciiInf::create_number and
  Stage::start_std_vms stay unpadded. Taking those addresses in an inline
  helper node (`static inline UpdateFuncCallback f() { return cb; }`)
  keeps LTCG from passing the alignment on; taking the address directly
  still passes it (Gui::on_tick_callback, on_draw_06_callback).
- A dead double in a HARNESS_CALLED set_vm_script made it realign itself,
  not its callers (the original pads set_vm_script and realigns all of
  its laser callers); the set_substate trick does not always push up.
- seek_bgm_to_stage_time as a plain inline helper (not DECOMP_NOINLINE)
  gives GameThread::on_tick_body the original's late `and esp, -8`
  without padding sub_42dc50's callees; written out, it realigns early.
  (Superseded: the seek is now written inline and begin_stage reaches
  start_std_vms through a member pointer; see the 2026-10-08 notes.)
- `test byte ptr [flags], 0x40` followed by a fresh dword load for the
  game_mode bitfield: a `*(u8 *)` cast and the bitfield view both share
  one load (`test al`); reading game_mode through
  `((volatile Globals *)&g_Globals)` keeps them apart.
- A store through D3DXVECTOR2's operator FLOAT* (`uv[1] = ...`) may alias
  uv.x, so the following `uv.x < 0` test stays after it
  (EnemyData::update_fog).
- InterpFloat::step matches with the plain ZunTimer::tick (current stored
  before current_f) and method 17 copying initial to current as an
  integer (`*(i32 *)&current = *(i32 *)&initial`) after the bezier_2
  update: the original copies it with mov eax/mov and reloads current
  for the return, where a float assignment forwards xmm0.
- A class without its `// VTABLE:` annotation shows the vftable store in
  its constructor and destructor as a raw address in reccmp (AsciiInf).
- The frames of create_vm and create_vm_front (4 unused bytes) are padded
  for known alignment: every original caller calls them aligned.
- Open, scheduling only: AnmVm::wipe's flags_hi and/or and pops, and
  EnemyInf's memset pushes, come a few stores later in the original; the
  bitfield view, a local, one expression and other statement positions
  do not move them.

### Overnight round (2026-10-08)

Lasers and MainMenuStates:
- A function the original realigns (`and esp, -8`, ebx frame) and ours
  does not: a dead `double unused = 0.0; (void)unused;` in that function
  makes it realign itself (LaserLineInf::initialize and
  LaserInfiniteInf::initialize matched, nothing else moved). The same
  double in a callee (AnmVm::run) cost nine matches.
- Early realignment that comes from calling a callee which realigns itself
  (TitleInf::draw_spell_card_page calling AnmManager::draw_text_centered)
  goes away when the calls go through small `static inline` helpers, each
  its own call-graph node.
- Rotations: `Float3 d = *pos - position;` and computing y before x
  (`y = d.x*s + d.y*c; x = d.x*c - d.y*s`) give the original's registers
  and stack slots (all three touches_circle). The same form helped the
  line and infinite sum_rect_damage and LaserLineInf::cancel_as_bomb_rectangle
  but made LaserCurveInf::sum_rect_damage and the infinite
  cancel_as_bomb_rectangle worse.
- `double whole = floor(t);` with `(f32)whole` at each use reproduces
  `fstp qword; movsd; cvtpd2ps`; `(f32)floor(t)` gives `fstp dword; movss`
  (LaserCurveNode::step_back).
- Indexing `segs[i]` / `segs[i - 1]` from a base pointer loaded before the
  loop gives the original's induction pointer biased by -8; a `segment++`
  loop does not. The bias follows the first address the body reads.
- Storing a computed value through an `f32 *` local
  (`*node_angle = wrap_angle(angle)`) makes MSVC reload the source member
  for its next use. Reading the member back instead of the local it was
  just stored from put the local in xmm0 like the original (step_ex_angle).
- `position += vel * g_game_speed` (or a temp plus D3DXVec3Add) loads
  g_game_speed once like the original; the field-wise form reloads it after
  each store. Writing `length * g_game_speed` in each branch instead of a
  local before the compare gives the original's compare-then-multiply.
- Switch case layout: `if (a >= b) { ...fall through } else { ...; break; }`
  instead of `if (a < b) { ...; break; }` (LaserInfiniteInf::on_tick).
- Writing `vm_950.field` directly instead of through `AnmVm *vm = &vm_950`
  keeps `this` in ebx; setting the blend mode through such a pointer gives
  the `inc [ex_index]` and reload in LaserInfiniteInf::run_ex.
- `i32 i = 0` declared before `memset(hit, 0, ...)` makes the memset reuse
  i's zero register (`xor ebx, ebx; push ebx`); `step.z = 0` before the
  sincosmul call matches the original's order (bomb cancels).
- reccmp finds functions by PDB file and line: rebuild before compare.py
  after any edit that shifts lines, or functions drop out of the compare.
- Dead ends: LaserCurveInf::on_draw's z add order (every field-wise
  permutation, D3DXVec3Add both ways, `+=` on a Float3 view, pointer
  locals); the timer tick that adds current_f into the speed register
  (every tick variant and a hand-written local); `*speed * 1.0f` in
  decrement (int argument, `fenv_access(on)`, `float_control(except)`, a
  double multiply: all still folded); cancel_in_rectangle/cancel_in_radius's
  8-byte frame (dead double, 8-aligned harness caller); allocate_new_laser's
  `push ecx` (`new T` vs `new T()`, typed locals, NULL init);
  draw_spell_card_page's `idiv` by a register 10 (ours multiplies for
  `id % 10`; tens first, locals, unsigned); on_draw__replay's setne vs
  neg/sbb/and (ternary, `!!`, bool/u8 locals all identical); do_music_room's
  /GS cookie comes from its direct AnmManager::interrupt_tree calls.

System files and MainMenu:
- Moving a function's double math into a `static __forceinline` helper
  makes it realign through ebx with ebp-relative locals like the original;
  written in place, the same body realigns with `and esp, -8` and
  esp-relative locals. get_runtime's helper also gave its callers their
  padded frames (PauseMenu::leave_paused matched, CSound::Unpause's frame
  fixed); get_controller_state matched with its whole body in a helper.
- A dead `double unused = 0.0;` matched Globals::add_to_score (ebx-form
  realignment), in the body or in a helper around the show_notice call. It
  is not universal: in ReplayManager::begin_stage it gives `and esp, -8`
  instead; in on_tick_record and present (inside an if) it does nothing;
  in the /INCLUDE'd add_power it only moves esi's push into the prologue.
- A function inlined at some call sites and called at others
  (get_score_extend_quota: inlined by the HUD, called by add_to_score,
  which keeps the score in edx across the call): an inline definition in
  the header plus a HARNESS_CALLED out-of-line copy
  (score_extend_quota_out_of_line, 0x43ddd0). Marking the one function
  HARNESS_CALLED instead cost Gui::update_lives its match.
- Shared return/failure blocks: the original keeps the first copy inline
  and later returns jump back into it; which copy ours keeps follows the
  loop structure. read_joypad's Acquire retry loop written like
  get_controller_state's (`while (hr == LOST) { hr = Acquire(); if (++n >=
  400) return; }`) makes the joyGetPosEx failure the shared copy.
- Switches on a step counter became jump tables where the original
  compares (SoundManager::update_sound_thread); if/else-if chains gave
  +11%. In the same function a `for` pan-sum loop was vectorized (unrolled
  by 2 with no_vector); `do { } while (--j)` inside `if (count > 0)` gives
  the plain loop. SoundBufferEntry::play and CSound::SetVolume need
  DECOMP_NOINLINE (the original calls them at every site).
- Computing a call's result into a local first keeps the destination
  pointer reload after the call (`DWORD pos = SetFilePointer(...);
  wave->x = pos - ...` matched CSound::Pause).
- `if (!(p & m) && !(r & m)) return 0; return 1;` avoids the setcc that
  two `if (...) return 1;` give (input_pressed_or_repeating). Nested
  `if (!reset) { if (dev) ... }` loads the device before the test where
  `&&` does not (create_d3d_device). A cursor copy of a pointer parameter
  frees its argument slot for the loop counter (Arcfile::parse_directory).
  `for (i = N; i != 0; i--)` lets WinMain reuse the final 0 in edi.
  `(x & 0xffff)` gives `movzx; test`, a `(u16)` cast gives
  `cmp word [mem], 0` (TitleInf::on_tick).
- volatile locals for values the original spills (WinMain's result,
  on_tick_record's input) help partly; the original also keeps a register
  copy.
- Dead ends: the demo index's divide by -3 (`% 3`, `% -3`,
  `x - x / -3 * -3`, an inline helper: all fold to /3 or idiv); register
  choices in CSound::SetVolume, SoundBufferEntry::play, preload_bgm,
  file_read_all, ScorefileStatus::init and lzss_decompress; get_runtime's
  spill slot ([ebp-8] vs [ebp-0x10]); loop-alignment nops ours adds at
  some loop heads (read_line, ScorefileStatus::init, the ReplayManager
  destructor, WinMain); SoundManager::initialize (99.53%) differs only in
  data layout.
- Open, cross-file: an early-aligned GameThread::on_tick_body (seek
  written inline, or a dead double in begin_stage) matches
  ReplayManager::begin_stage and gives finish_stage_transition and
  begin_stage their frames, but Stage::start_std_vms loses its match (it
  loses edi shrink-wrapping). EnemyManager::allocate_new_enemy's dead
  parameter (junk third argument in begin_stage) needs it HARNESS_CALLED
  plus a harness caller with a second object.

GUI, stage, effects, pause and help menus:
- The open timer item (the original adds current_f from memory into the
  speed's register) is solved by `ZunTimer::tick_goto`:
  `if (speed == NULL) goto whole_frame; if (*speed > 0.99f && *speed <
  1.01f) { whole_frame: cur++; current_f = current_f + 1.0f; } else {
  current_f = *speed + current_f; cur = (i32)current_f; } current = cur;`
  gives `addss xmm1, [current_f]`, a store in each branch and 1.01f kept in
  a register (ScreenEffect::on_tick_flash, on_tick_hold and on_tick_shake
  matched). Candidates elsewhere: the bullet ex steps, InterpFloat3::step,
  Spellcard::on_tick_body, kill_all.
- /GS cookies from struct temporaries: a plain `static inline` helper that
  owns the Float3 temporary (LTCG inlines it) removes the cookie
  (set_float3 in the GuiMsgVm constructor, set_entity_pos_xyz in
  setup_stage_hud); `__forceinline` keeps it whenever the local stays in
  memory, and LTCG does not inline plain helpers whose local's address
  goes to a call. Even a dead Float3 local or a POD D3DVECTOR triggers the
  cookie. `__declspec(safebuffers)` on the declaration removes it in
  GuiMsgVm::run (a documented workaround) but not in InterpCameraSky::step.
  Unexplained: the original's InterpCameraSky::step passes temporaries by
  address to operator+ and has no cookie.
- Padded functions in the original (create_number_with_digit,
  create_stringf) keep the cookie at [ebp-8] with 4 free bytes above it:
  LTCG knows esp mod 8 at each call site and aligns the 0x104-byte buffer.
  Ours, when it knows the alignment, adds 8 bytes and keeps the cookie at
  [ebp-4].
- Dead doubles (the lasers technique) matched Stage::on_tick,
  Stage::update_std_vms, Gui::show_lights_out and
  Gui::show_stage_clear_bonus, but cost InterpCameraSky::step 3 points.
  On other functions they had side effects: on_draw_2_body +13% but
  create_number loses its match; HelpManual::on_tick_body +5% but
  AnmManager::reload_texture -2.4; update_callout worse. Routing a callee
  through a `static inline` helper did not stop the padding
  (reload_texture, update_season_gauge's run calls).
- A dead double in a HARNESS_CALLED AnmVm::run plus HARNESS_CALLED
  update_std_vms (not committed): 8 matched (Stage::on_tick, update_std_vms,
  anm_masked_effect_on_tick, collect_full_power, both laser initializers,
  ReplayManager::begin_stage, interrupt_tree_and_run), 7 lost
  (PosVel::step_from_center, start_std_vms, cancel_rectangle_as_bomb,
  boss_timer_on_spell_start, Spellcard::end, start_dialogue,
  Item::init_anm; they gain padded frames).
- `char buf[3] = {0, 0, 0}` instead of three stores puts buf in another
  variable's slot (draw_text). Indexing `anm_ids[last_used_index]` afresh
  in each test instead of an `AnmId &` local puts `this` in ebx and spills
  the index (EffectManager::next_index). In blur_alpha, offsets from the
  global width keep `up = -width` its own variable; up_left must be
  `-w - 1` to get `not`.
- Operand order: `f32 r = rand(); ... r * PI / 40 + PI / 80 + wave_angle_b`
  adds the member last (step_fog); `D3DXVECTOR3 *pos = &vertex->pos;
  pos->y = pos->y + d.y` loads pos.y first. Each corner component written
  as its own `center.x + size.x * 0.5f` (CSE merges them) schedules closer
  than named bounds (StdObject::is_culled).
- `(~(flags >> 5) & 1) && (~flags & 1)` gives Gui::on_tick_body's
  `shr; not; test al, 1` sequence (same as Player::on_tick_body's).
- Small inline helpers taking a `MenuHelper *` give HelpManual's
  `[edi+4]` accesses.
- build.py cannot parse `__declspec(...)` before an annotated definition;
  put it on the declaration.
- Dead ends: on_draw_03's pop and spill placement; show_boss_marker's
  redundant `test` (about 12 forms); begin_score_entry's character offset
  (always folded into the index); AsciiInf::tick shrink-wrap; PopupManager
  loop base field; blur_alpha/bleed_color never using ebx (empty `__asm {}`
  does nothing); Ending::initialize's stack slots; the CameraSky cookie
  (non-array color struct, user constructor); a form of
  Stage::start_std_vms that keeps its shrink-wrap whatever the caller's
  alignment.

Bullets, enemies, ECL and spell cards:
- An inline helper returning `c ? x->entry : NULL` lets MSVC jump straight
  out of the caller's loop on the NULL branch; `T *b = c ? ... : NULL;
  return b;` keeps the original's join and second test
  (BulletManager::iter_first/iter_advance; matched the three cancel
  functions).
- `v <= -1 && v >= -100` on one variable merges into `lea; cmp; ja`;
  reading the first bound from memory again (`ins->args[i].i <= -1 &&
  value >= -100`) keeps the original's two signed compares (get_int_arg,
  pop_int_arg). The given_value variants merge in the original too.
- One result variable assigned on every path, then `return result`,
  gives the original's type load into a register and a shared epilogue
  (get_int_arg, pop_int_arg, pop_int_arg_given_value).
- Reading a popped entry as `i32` and returning `*(f32 *)&item` fixes the
  register swap in pop_float_arg and pop_float_arg_given_value (in
  EclStack::pop_float it gave ecl_run +11%; the same form in pop_int made
  ecl_run worse).
- An inline `EclStack::local_ptr` that computes `data + base_offset` first
  gives the original's add order and stops `(i32)value` being hoisted above
  the sign test.
- Reading fields through a pointer local of their own (`Float3 *to =
  &...`) stops MSVC reusing earlier loads (EnemyInf::die reloads the
  positions for atan2, Spellcard::on_tick_body reloads boss_pos).
- 0x3d23d70b is `0.2f * 0.2f`, one ulp off `0.04f` (EnemyInf::die and the
  die instruction in ecl_run_over_300).
- `i32 fps = 60; x / fps; x % fps` keeps one `idiv` for both; two ternaries
  give the clamp's cmovs; `break` instead of `return NULL` shares the NULL
  exit (check_time_interrupts).
- `u32 f = flags; if (f & A) { if (!((u8)f & B)) ... }` keeps
  `test eax, A` and `test al, B` on one load (step_interpolators); the
  bitfield and dword forms merge into and/cmp.
- `node.init(this)` instead of four field stores moved the memset pushes
  and matched EnemyInf::EnemyInf (the open "EnemyInf's memset pushes" item).
- tick_goto can also flip LTCG to "load current_f into xmm0, add the
  speed": it matched Bullet::step_ex_00, step_ex_04 and
  EnemyManager::kill_all and fixed the tick in step_ex_17,
  kill_all_in_group and EnemyManager::update. Try it on every tick
  mismatch, whichever way it differs.
- /GS cookies: a `__forceinline` helper owning the Float3 temporary removed
  EnemyData::on_tick's cookie (the plain static inline one was not
  inlined there); `__declspec(safebuffers)` removed step_logic's.
- AnmId::find_or_clear HARNESS_CALLED instead of DECOMP_NOINLINE: with
  every caller visible LTCG knows it leaves xmm1-xmm3 alone, so
  EnemyData::on_tick keeps the summed position in registers across the
  call like the original (matched, nothing else moved).
- Smaller: Bullet::on_tick writes `release(); return -1;` at each site
  instead of a goto (the original keeps the first copy inline at the top);
  half steps as one expression `pos += velocity * g_game_speed * 0.5f` in
  clear_all's inlined copy while Bullet::cancel keeps its delta local
  (picked per copy by a constant flag on a forceinline helper); an `if`
  instead of a ternary for `dealt` (step_logic); assigning `offset.y`
  before `offset.x` (eject_extra_drops); a separate `return 1` on
  Spellcard::on_tick_body's early-bomb path; ecl_run_over_300 can inline
  new Fog, the four laser instructions and angleToPlayer again (+11%).
- Dead ends: kill_all_no_set_death's tick (every tick form and goto
  variant gives "load"; the original adds into the speed register);
  Spellcard::on_tick_body's tick (every form gives "fold" where the
  original loads); step_ex_08's realignment from its 8-byte D3DXVECTOR2
  corner (a Float3 adds a cookie, a 2-float struct still realigns,
  HARNESS_CALLED moves it into Bullet::on_tick); get_int_arg_given_value's
  `+4` always folded into the displacement; EnemyManager::destroy_all
  always counts down; BulletManager::destroy_all's padded ebp frame needs
  known alignment from GameThread's callers; kill_all_in_group's `value`
  stays in its slot; callee-saved permutations in load_ecl_data,
  ecl_anm_vm_instr and Spellcard::start; call_sub's byte-offset argument
  loop gives the original's instructions with permuted registers (net
  -0.06); an out-of-line LaserInfiniteInner constructor costs
  LaserInfiniteInf's constructor its match.

ANM VM, loader, drawing and interpolation:
- Resetting list nodes with `ZunList::init(this)` instead of four field
  stores matched AnmVm::wipe and wipe_suffix (closes the open "AnmVm::wipe
  scheduling" item; LaserBeamInf::initialize matched with it).
- `char buf[0x10c]` left 8 unused bytes under the /GS cookie; MAX_PATH
  (0x104; 0x108 also works) gives the original frame
  (AnmLoaded::load_entry).
- The three-argument `Int3(b, g, r)` constructor loads x, z, y; assigning
  the fields y, z, x matched AnmVm::set_rgb1_time and set_rgb2_time.
- Locals hoisted before a switch take callee-saved registers: draw_vm's
  anchor_x/anchor_y went to ebx; reading them at each call matches.
- Ending the RECT_ROT_GRAD case with `break` (returning 0 at the end) keeps
  the later copy of the shared draw_rect_bordered tail like the original:
  the cross-jumping item noted open for PauseMenu.
- One epilogue per branch: `return pos;` in each branch of
  transform_coords; one return after the if/else shares the epilogue.
- A two-case switch on the resolution mode was laid out the other way
  round; an if/else-if chain fixed both sprite corner functions. Where the
  original has a jump table (draw_billboard_fog) it needs explicit
  `case 2: case 3:` instead of `default:`.
- HARNESS_CALLED on a function whose only caller already realigns stops it
  realigning itself (InterpFloat2::step_radial_dist, called only by
  EnemyData::step_interpolators).
- `*(Float3 *)&vertex->pos += pos` keeps the original's z = 0 store and
  reload; per field, the z sum is folded (update_special_vertices).
  `D3DXVec3Add(p, p, &delta)` through a pointer local gives AnmVm::run's
  per-component operand order for the camera add.
- tick_goto matched InterpInt3::step (effective); it does nothing in
  InterpFloat2 or InterpFloat3.
- A function-local `static const f32` moved to file scope with a GLOBAL
  annotation lets reccmp name it. The "cannot name" TODO in
  SoundManager.cpp is outdated.
- Operand order of `a + b` mattered in InterpStrange1 and InterpInt3
  (`tmp + goal`, `goal + bezier_2`) though it is canonicalized elsewhere.
- A dead double in AnmVm::run (not HARNESS_CALLED): 7 matched, 7 lost
  (same lists as the HARNESS_CALLED experiment above); most losses trace
  to ecl_run_over_300 and ItemManager::on_tick_body having different frames
  from the original. The AnmLoaded callers (create_vm, create_effect,
  set_vm_script) do not respond; they may be in run's call-graph cycle
  (run -> EffectManager::create_effect -> AnmLoaded::create_effect -> run).
- 16-byte realignment (draw_3d, draw_3d_vertex_strip,
  write_sprite_corners__with_z_rot, build_world_matrix): a D3DXMATRIX
  local passed to D3DXMatrixRotationX makes it realign to 8, and the
  `world = vm->world_matrix` copy makes that 16. HARNESS_CALLED, or member
  functions with forwarders, only gave the ebx form.
- interp_common_methods cannot reach 100% in reccmp: reccmp names only the
  float constants some x87 instruction references, so the SSE-only
  ease-back divisors always count as differences.
- quickdiff stops at the first `ret` in functions with jump tables
  (interp_common_methods showed MATCH there at 83% in reccmp).
- Dead ends: create_vm_front (blocked by callers: ecl_run_over_300's
  helpers, repopulate_options); insert_in_* (manager in ecx instead of
  edx); restore_snapshot (CS flag cached in bl); InterpFloat2's bezier
  scheduling; preload_anm's 8 unused frame bytes; setup_entry,
  AnmLoaded::load, reload_texture, load_texture_from_data (register
  allocation, unmoved by statement order, locals, helpers, HARNESS_CALLED).

ANM manager, callbacks, rendering and loaded ANMs:
- How much dead double math it takes: in anm_jagged_line_on_draw one or
  two `double unused = 0.0;` changed the `entity_pos.x + pos.x` operand
  order but did not realign; three or more realigned but flipped the order
  back; one double with three `unused = unused * 2.0;` did both and
  matched. A dead double inside a loop counts for more than one outside.
  anm_masked_effect_on_tick matched with one.
- A callee the original realigns, whose callers do not: put the dead
  double in an out-of-line body and route callers through a `static
  inline` wrapper in the header (interrupt_tree_and_run, now
  interrupt_tree_and_run_out_of_line at the same address, matched; without
  the wrapper the same double cost 11 functions and 3 matches). This does
  not work for a function returning a struct (create_effect): callers then
  read the AnmId back from the return slot instead of eax (-30 functions).
- ENTER_CS/LEAVE_CS with the flag read once into a bool, reloaded after
  EnterCriticalSection and tested in LEAVE, keeps it in bl across the call
  (restore_snapshot, effective match). The macros' two reads do not.
- `src++` with a `const AnmVm *vm = src;` copy for the field reads gives
  load_from's stack slots (copy in a local, extra-data pointer in src's
  argument slot).
- `*(Float3 *)&vertex->pos += a + b` computes all three sums before the
  stores like the original; field-wise adds reload after each store
  (anm_fan_init, anm_on_tick_fan).
- `*(radius + 33) = speed` instead of `radius[33] = speed` puts the store
  between the multiply and the add of the x87 expression (anm_fan_init).
  Walking with a saved first pointer and `*vertex = *first` gives
  on_tick_fan's loop. Reusing `offset = mid - start;` as the normalize
  input gives the original's CSE (anm_gather_effect_on_tick 55.7 -> 68.7).
- Open, x/y vs z operand order: in the original x and y often use one
  operand order and z the other (anm_fan_init, anm_on_tick_fan,
  gather_on_tick's `start_center += offset`, jagged's sums; compare
  LaserCurveInf::on_draw's z adds and Player::on_tick_body's scaled boxes).
  Our D3DX operators always give all three the same order; swapping
  operands flips all three, field-wise temporaries in any order change
  nothing, only IL numbering changes (dead doubles) move one component.
- Alignment cluster (show_notice, add_power, the collect_* functions,
  do_shooting): it does come from AnmLoaded::create_effect wanting
  alignment (contradicting the do_shooting note above): a dead double with
  three multiplies in create_effect matches it and Player::do_shooting,
  tick_shooting_state and shoot_one_bullet, but costs 21 functions
  (add_to_score, PlayerBullet::create, Gui::on_tick_callback lose their
  matches; EffectManager::create_effect 100* -> 79.7; show_notice
  realigns itself where the original pads it). The same math in a
  `__forceinline` helper inside create_effect pads instead of realigning:
  the three Player matches stay, 8 functions go down (add_power realigns
  and is no longer inlined into collect_*; EffectManager::create_effect,
  replace_with_effect and PlayerBullet::create get padded frames). What
  blocks a commit is the show_notice/add_power/collect_* arrangement and
  the direct callers that become padded (Gui, Globals, Item, Effect,
  PlayerBullet).
- Dead ends: draw_triangle_fan's known-alignment padding (dead doubles
  anywhere only make it realign through ebx); ANM rendering functions that
  never use ebx in ours (anm_on_draw_masked, draw_vertex_strip,
  draw_triangle_fan, draw_circle_outline, draw_ring, reload_texture,
  create_d3d_textures, build_world_matrix); `and esp, -16` from the movaps
  matrix row (write_sprite_corners__with_z_rot, draw_3d,
  draw_3d_vertex_strip, build_world_matrix: memcpy, loop copy, `m.m[3][]`,
  a float pointer, a g_AnmManager destination); world_pos's /GS cookie
  (removing it loses the root_vm reload); gather_on_copy's spilled counter;
  copy_screen_to_sprite; insert_in_* (edx/ecx swap); draw_text's height
  lookup; convert_texture (`row[x]` is worse). create_effect calls load
  g_EffectManager into ecx where the original uses eax (same open item as
  the menu functions).

Collision, second pass over lasers, menus and system:
- Collision rotations: writing `sinf`/`cosf` inside the 4-point rotation
  loop keeps it rolled like the original (`mov eax, 4` counter, pointer
  walk), the array in memory and the original's /GS cookie; with the calls
  before the loop MSVC unrolls and scalarizes it (whether rotate_points is
  forceinline, inline or takes a count). collision_test_points_in_rect
  28.75 -> 97.50.
- The original squares each corner offset again in every test. That comes
  back when the squared length is read through a pointer:
  `offset_length_sq(const Float3 *d)` returning `d->y*d->y + d->x*d->x`
  (y term first gives the original's x-term accumulator). It must be a
  Float3: an 8-byte Float2 local made LTCG realign the callers' frames
  (cancel_rectangle_as_bomb lost its match). The plain expression and a
  by-value helper get the squares shared. The helper in Collision.h also
  helped the inlined copy in the three sum_rect_damage functions.
- `fabsf(y) <= h * 0.5f` evaluates fabsf before the multiply like the
  original; `h * 0.5f >= fabsf(y)` does not.
- tick_goto matched ZunTimer::operator-= (the unscaled branch gets its own
  load of current_f) and both step_ex_angle functions (effective).
- Component pointers decide add operand order: `f32 *px = &position.x;
  *px += offset.x;` loads position and adds the offset from memory
  (LaserCurveInf::initialize). LaserCurveInf::on_draw matched with x and y
  as `D3DXVec2Add` plus a block-scoped `f32 *segment_z =
  &segment->pos.z; vertex->pos.z = *segment_z + vertex->pos.z;` (one
  pointer shared by both vertices was worse). `&((T *)segments)[i - 1]`
  instead of `&segment[-1]` loads segments before scaling i.
- Menu states: writing `g_stage_data = &g_stage_table[stage]` before the
  stage_num/weird_stage_num stores keeps `stage + 1` in ecx with the imul
  hoisted (solves the open item; do_spell_practice_subseason and
  do_spell_practice_difficulty matched). A clamp in place on the member
  (`cfg.x += 5; if (cfg.x > 100) cfg.x = 100;`) gave do_options' al/ecx.
- ~ReplayManager: the three unregister blocks written out instead of an
  inline helper (+4.5). The original hoists EnterCriticalSection's address
  into a register in some loops (ScorefileStatus::init, ~ReplayManager);
  not reproduced.
- Dead ends: draw_circle/draw_circle_outline/draw_ring's
  `movaps xmm0, step; addss angle, xmm0` copy that pushes the angle add
  past the stores (every loop and add form; the original's own
  update_special_vertices has the same copy); draw_rect's cos/sin slot
  order; ZunAngle::operator-'s PI/b register swap; the `* 1.0f` in
  decrement (an out-of-line HARNESS_CALLED function with an i32 parameter
  gets `ret 4` but folds; an f32 parameter goes in xmm1; a const reference,
  an extern const, a never-written global all fold); the rotation loops'
  store order; collision_segment_intersection's canonicalized operand
  orders; do_key_config's byte load widened to a dword; the shared ascii
  create_effect push order; do_spell_practice_row's base/index order;
  CSound::Unpause's add order; SoundBufferEntry::play's merged SetVolume
  calls; a dead double around interrupt_child_and_run's run() (cost two
  cursor functions their matches); camera_update_2d calling zun_tanf
  (zun_tanf becomes a jmp thunk and loses its match); lzss setup order.

Second pass over GUI, stage and GameThread:
- Cutting a call-graph edge with a member function pointer: write the call
  as `(obj->*f())()`, with `f` a small `static inline` helper returning
  `&Class::method`. The optimizer turns it back into a direct call, but
  LTCG's call graph has no edge, so an aligned caller's alignment does not
  reach the callee. This resolved the GameThread trade-off (the bgm seek
  inline in on_tick_body realigns it early; begin_stage calls
  start_std_vms through the pointer, which keeps its shrink-wrapped edi):
  GameThread::on_tick_body and ReplayManager::begin_stage matched.
  Gui::on_draw_2_body realigns early (`and esp, -64` like the original)
  while AsciiInf::create_number, called through the pointer, stays
  unpadded; update_season_gauge stopped realigning once its AnmVm::run
  calls went through it. An address-taken member keeps `this` in ecx, so
  callees whose callers never set ecx must be `static` (`__stdcall static`
  when the original ends in `ret N`).
- /GS cookies come from `__forceinline` helpers: a forceinline helper
  brings its own buffer check into the caller even when the caller is
  `__declspec(safebuffers)` (documented MSVC behaviour). InterpCameraSky::step
  lost its cookie once its sky_step_* helpers were
  `static __declspec(safebuffers) __forceinline` (its CameraSky operators
  need safebuffers too, or they are not inlined into them). safebuffers on
  the declaration alone removed the extra cookies in StageInner::run_std
  (+9) and HelpManual::on_tick_body (matched). Candidates with a cookie the
  original lacks: AnmVm::world_pos, BombMarisaAInf::on_tick,
  TitleInf::do_music_room, Player::update_options,
  PlayerInner::repopulate_options, sht_on_tick_sideways, sht_on_tick_laser.
- A dead double in a callee whose callers are aligned gives it known
  alignment: Fog::Fog saves its registers up front with `this` in ebx, no
  shrink-wrap, like the original (+43). It did nothing in
  create_ui_effect, create_ui_vm, update_score or update_callout.
- `Stage *stage = g_Stage2; if (stage != NULL && ...) delete g_Stage2;`
  keeps the delete's own null test. Three separate `if (flag) return 3;`
  keep three `test al, n` and the first return-3 epilogue at the top where
  one `||` merges them into `test al, 0x70` (GameThread::on_tick_body). An
  early `return` from find_child_id_inline_search for a missing parent
  keeps the child search inline (setup_stage_hud +7).
- run_std's entry loop test: `i32 now = time_in_stage.current; if
  (ins->time > now) goto ticked; do {...} while (ins->time <= current);`
  gives `mov eax, [cur]; cmp [esi], eax`.
- Fog::Fog HARNESS_CALLED folds its two unused constant arguments, and the
  callers push junk into those slots like the original.
- `D3DXVec3Add(&pos, &a, &b); pos = pos + c;` adds every component in the
  original's operand order where the operator+ chain swapped y and z
  (GuiMsgVm::update_callout).
- Loading `g_ending_files[i]` into a local before `strcpy(path, "");
  strcat(path, file)` puts the path clear after the load
  (Ending::initialize). A forceinline helper returning the AnmId makes the
  caller read the id back from its stack slot (Fog::Fog).
- PopupManager::on_tick wants the plain `tick()` in both loops; tick_goto
  did not help run_std's or PauseMenu's ticks.
- Cross-file, measured but not committed: a dead double plus
  HARNESS_CALLED on AnmVm::run gained 4 matches and lost 9 (the
  function-pointer trick at the losing call sites might keep them); a dead
  double in Gui::show_notice gained collect_full_power and
  Globals::add_power but lost Spellcard::end, Item::init_anm and
  Globals::add_to_score. begin_stage and finish_stage_transition are now
  blocked only by allocate_new_enemy's folded third argument.
- Dead ends: Fog::set_rect's divss/cvt scheduling; begin_score_entry's
  folded character offset; PauseMenu::on_tick's tick combinations;
  show_boss_marker (the join gives cmov, `> -1` gives cmp/jle);
  update_score and update_callout do not realign with a dead double;
  open_game_over_menu realigns instead of padding; PopupManager::on_draw's
  esi/edi swap; Gui::on_tick_body's `this` in esi not edi; AsciiInf::tick's
  shrink-wrap is not from caller alignment; take_snapshot's dead
  `test eax, eax` after the D3DX call; create_stringf and
  create_number_with_digit put the padding above the cookie in the
  original, between cookie and buffer in ours.

Vector operand order (research; the x/y vs z item above):
- Method: a standalone snippet compiled with build.py's CFLAGS reproduced
  the tree's code for anm_fan_init exactly, z mismatch included, so most of
  this ran on snippets (tools in the overnight scratchpad were not kept).
- For `a + b` with both operands in memory MSVC does not follow source
  order: `p->f[k] + p->e[k]` and `p->e[k] + p->f[k]` compile the same. In
  straight-line code with one base pointer, the field at the higher offset
  is loaded and the lower one folded into addss; with two pointer
  parameters the earlier parameter's load comes first. Globals differ:
  `a + b` loads b, inside a loop a.
- A component read at offset 0 through a pointer (x of every D3DX operand:
  operator+, +=, D3DXVec3Add's `this` and `v`, a local `D3DXVECTOR3 *`) is
  ordered by the function's count of named variables and parameters, with
  period 8: every named variable counts 1, whatever its type, scope or
  position. Only that component flips (with `float *p = &v.y`, p[0] = y
  flips instead). Dead stores, identity inline helpers and CSE'd duplicates
  change nothing; field-wise `vm->pos.x + vm->entity_pos.x` never flips.
  Each D3DX operation has its own flip window, so one count change can fix
  one and break another. Three dead named locals matched
  LaserInfiniteInf::cancel (the "only x differs" case in straight-line
  code).
- Inside a loop body (base pointer not an induction variable) the choice
  follows a period-4 pattern over the loop's statements; its phase moves
  with the memory statements before the loop and with the loop form:
  top-tested counted loops (for, while, guarded do) alike, bottom-tested
  loops (do/while, goto, for(;;) with break) one phase earlier,
  end-pointer loops (`p < end`) one phase later. Dead locals, field offsets
  and source operand order do not move it. This is where the mixed x/y vs
  z orders in loops come from: a do/while counting up matched
  anm_fan_init, and the same idea LaserCurveInf::on_draw.
- Ruled out: the struct-copy pairing (unpcklps/movq; the patterns appear
  without any copy); the D3DX header (five other operator definitions and
  a Float3 built on a Float2 only shift the patterns, and field-wise code
  shows the same effects); /d2SSAOptimizer-, /d2newcolor-, /d2linscan,
  /d2Loop0-2 (no change except Loop0 on vectorized loops).
- A local declared at function scope instead of an inner block changes
  stack slot reuse: AnmVm::load_from's `i32 read` at the top gave
  &index_of_on_serialize its own slot like the original.
- Python's subprocess with capture_output makes every build wait for
  mspdbsrv.exe to exit (minutes); redirect to a file instead.
- Dead ends: anm_on_tick_fan (about 3000 snippet combinations of loop
  forms, dead locals 0-7, helper parameters, statement orders);
  anm_gather_effect_on_tick's `start_center += offset` (y never flips);
  draw_text's height lookup; load_from's `read` slot; gather_on_copy's
  spilled counter; insert_in_* (dead locals 1-7 included). A loop-form
  scan over 17 functions found no further matches.

Cross-file alignment (create_effect, show_notice, the item and shooting
functions):
- The wish for an aligned stack is weighted and adds up across call edges.
  A plain dead double in AnmLoaded::create_effect, or one with one or two
  multiplies, makes its direct callers realign while create_effect itself
  only loses shrink-wrapping; with three multiplies it realigns too. One
  direct call to a wishing callee is fine, two make the caller realign
  (TitleInf::on_tick, the GuiMsgVm constructor, PlayerBullet::create,
  show_notice); calls in loops count more. Dead doubles add up: Fog's own
  plus the wish create_fog_vm passes on made Fog::Fog realign, while the
  same double in a plain `static inline` helper (its own call-graph node)
  kept the known alignment without realigning and matched Fog.
- The fix (commit 5431169): create_effect and show_notice get dead
  doubles, and the callers that must not realign reach them through
  member function pointers (gui_show_notice_func(), the
  create_effect_via_pointer forceinline wrapper), which cuts the call-graph
  edge. PlayerBullet::create keeps one direct call, which passes the wish
  on so do_shooting realigns through ebx and tick_shooting_state is padded
  like the original. ItemManager::on_tick_body calls init_anm through a
  member pointer and realigns early like the original. Matched
  create_effect, Fog::Fog, Item::collect_full_power, Globals::add_power,
  Player::shoot_one_bullet, do_shooting and tick_shooting_state
  (collect_big_power effective); 16 up, 0 down.
- A member-pointer call inside a `static __forceinline` wrapper loads the
  object pointer first (g_AsciiManager in eax, the result slot in ecx):
  that solves the open "ascii create_effect" menu item (TitleInf::do_manual
  matched). The raw member-pointer call in the caller leaves the order
  alone; a forceinline wrapper with a plain call is worse.
- Member-pointer calls lose return-slot forwarding (create_fog_vm, which
  returns create_effect's result, fell to 56% that way), and taking the
  address of a function whose `this` LTCG had dropped brings `this` back.
- GuiMsgVm::update_callout needed two dead multiplies to realign (a plain
  dead double does nothing); writing the scale out at each use
  (`pos.x *= 2.0f / scale` twice) fixed the multiply order and the x add
  order followed. It matched.
- The ebx form appears when the wish comes from a callee or a helper node;
  a strong dead double in the body gives the plain `and esp, -8` form.
- Dead ends: a dead double in AnmVm::run on top of this (15 down, 8 lost);
  Gui::update_score and ReplayManager::on_tick_record realigning through
  ebx (a strong double gives the plain form; helpers and HARNESS_CALLED do
  nothing); the missing 4 frame bytes in create_vm, create_vm_front,
  create_ui_vm and create_ui_effect; show_notice's loop registers; the
  forceinline wrapper in Ending.cpp (EndingScriptVm::run loses its match),
  in EffectManager::create_effect (68%) and at Player.cpp's two calls
  (Player::move stops realigning).

Second pass over bullets, enemies, ECL and spell cards:
- `#pragma fenv_access(on)` around one function stops MSVC treating CRT
  math calls as pure: members are reloaded after a floor call instead of
  kept across it (Spellcard::measure_real_time matched). A double local
  passed to floor gives the original's load into xmm0 and move to the x87
  stack; `#pragma function(floor)` also forces the reload but passes the
  argument as `movsd [esp]`.
- Writing a tick's whole-frame step twice (one copy for a missing speed,
  one for a speed close to 1) makes MSVC load 1.0f into a register early
  and merge the copies afterwards: the new `ZunTimer::tick_nested`
  matched EnemyManager::update and fixed Bullet::on_tick's tick (in
  BulletManager::on_tick_body it hoists the constant but assigns the
  registers differently). Other functions with the original's
  1.0f-in-register tick: InterpFloat3::step, InterpCameraSky::step,
  InterpStrange1::step, InterpFloat2::step and step_radial_dist,
  InterpInt3::step, Gui::on_tick_body, PauseMenu::on_tick,
  Player::on_tick_body, PopupManager::on_tick.
- HARNESS_CALLED on both Bullet::on_tick and step_ex_08 removes both
  functions' `and esp, -8` (step_ex_08's D3DXVECTOR2 alignment then comes
  from on_tick_body's realigned frame); doing one alone moves the
  realignment around.
- Walking a loop by byte offset reproduces an "offset counter compared
  with the end" loop: `for (u32 off = offsetof(...[10]); off <
  offsetof(...[16]); off += 4)` (EnemyManager::destroy_all 78 -> 98); an
  index loop counts down beside a pointer.
- `Float3 pos(0, 0, 0); if (vm) pos = vm->pos;` gives the original's zero
  temporary copied in on the else path, better than a zero local assigned
  in an else (step_interpolators +7).
- Inline helpers and D3DXVECTOR3 constructors with float arguments
  evaluate them left to right in our build where the original reads them
  right to left (int-argument helpers already go right to left); reading
  y into a local first fixed anmScale/anmScale2.
- A member updated with `+=` is added in memory and reloaded; computing the
  new count into a local, storing it and using the local gives the
  original's register add (load_ecl_data).
- reccmp artifacts that source cannot fix: its float-constant scan only
  looks at x87 instructions, so a constant only SSE code uses shows as
  `<OFFSETn>` on the original side even when equal (step_interpolators'
  0.03f, step_ex_08's -384.0f); its latin1 string scan reads the original's
  1.9f at 0x494548 as a string, so eject_extra_drops cannot reach 100%.
- Dead ends: eject_extra_drops (operand and statement orders, pointer
  forms, f32 helpers; fenv_access made it worse); step_ex_17 (all six
  statement orders); clear_all's x/y register swap; kill_all_no_set_death's
  tick (tick_nested and tick_in_place also load); the kept `* 1.0f` in
  decrement (fenv_access, float_control(except), a volatile 1.0f, an
  out-of-line decrement with inline_depth(0): all fold or reorder);
  get_int_arg_given_value's folded +4; load_ecl_data's redundant
  `test esi, esi` before free; run_ex's ex pointer (index addressing in
  every form); check_player_collision; step_logic's cmov reuses damage's
  register; Spellcard::start's esi/edi; a volatile
  EclRunContextHolder::current_context (cost 4 functions and SptInf::run_ecl).

Second pass over ANM VM, loader, drawing and interpolation:
- `__restrict` on a pointer parameter that every caller fills with a
  local's address lets loads move above stores: AnmLoaded::load_sprite's
  struct copy then loads its last dword before the first movups store, as
  in the original (matched). HARNESS_CALLED alone gave LTCG no such alias
  knowledge.
- Reading through a local copy of a pointer parameter or of `this`
  (`AnmVm *vm = vm_param;`, `InterpFloat3 *self = this;`) changes the
  operand order of the SSE math (write_sprite_corners__without_rot and
  with_z_rot, draw_billboard_fog, InterpFloat3::step 76 -> 97,
  InterpFloat2::step and step_radial_dist 93 -> 99). Combined with dead
  named locals (one for InterpFloat3, four for InterpFloat2); without the
  `self` copy the dead locals did nothing there. tick_nested plus three
  dead locals made InterpInt3::step exact.
- AnmVm::transform_coords matches through any copy of `pos`, but that form
  gives every function passing a local through get_own_transformed_pos a
  /GS cookie (draw_vm and write_sprite_corners lose their matches), so it
  stays as it is.
- 16-byte struct locals in vectorized loops make LTCG realign to 16
  (with_z_rot's AnmAnchorCorners locals gave `and esp, -16`); `f32[4]`
  arrays filled element by element keep the original's /GS cookie and
  unaligned frame (51.6 -> 74.2). A struct copy or memcpy into the arrays
  is promoted to registers and loses the cookie.
- Behaviour bug fixed: `D3DXMatrixIdentity(&vm->sprite_matrix);
  vm->world_matrix = vm->sprite_matrix;` lost all 16 identity stores in our
  build (draw_3d_vertex_strip). Copying from the return value
  (`world_matrix = *D3DXMatrixIdentity(&sprite_matrix)`), a pointer local,
  or explicit field stores keeps them, in the original's order.
- draw_3d: HARNESS_CALLED turns its 16-byte realignment into the 8-byte
  ebx form; a dead double in draw_vm aligns draw_vm early but costs 7
  matches up the chain (StageInner::draw_vms, BulletManager's on_draw body
  and callback, ItemManager::on_draw_body, both laser on_draws,
  render_layer).
- AnmLoaded::load: declaring `i32 i = 0;` right after the null check
  (where ZUN zeroes it) gives the early `xor ebx, ebx` and keeps `this` on
  the stack. draw_billboard_fog: `case 2: case 3:` instead of `default:`
  restores the jump table (with a C4715 pragma: any return after the
  switch costs 0.6), `!(flags & mask) ? color_1 : color_2`, and the
  "declared, then assigned" vector form `D3DXVECTOR3 diff; diff = ...`.
- reccmp does not name the original's strings at 0x494210, 0x49422c and
  0x494244 (preload_anm, setup_entry); the Shift-JIS string before them
  may be sized wrong and overlap them.
- Dead ends: create_vm_front (callers in other files); InterpStrange1's
  esi/edi swap (loop forms, switch, locals, helpers, pointers, dead locals
  1-7); the InterpFloat2/3 tick ("load, add, store once" in no tick form);
  write_billboard_corners (all field orders, operator-, Vec3Subtract,
  sqrtf orders); preload_anm/load_texture_from_data as HARNESS_CALLED
  members (code identical to the static form); reload_texture,
  render_sprite_2d, update_special_vertices (anm_sincosmul `__stdcall` cost
  a match); AnmVm::run (`fenv_access(on)` took it 92 -> 79);
  draw_3d_vertex_strip's rotation matrix as D3DMATRIX or f32[16] still
  realigns to 16.

Sweep of the enemy files with the vector-research levers (first half):
- Behaviour fix: anmPosTime (ecl_anm_vm_instr) and anmPlayPos
  (ecl_run_over_300) read their y argument before x, as the original does
  (0x4236dd, 0x41dfca); get_float_arg consumes g_replay_safe_rng for the
  random variables, so the old order swapped the two random values. The
  D3DXVECTOR3 constructor evaluates float arguments left to right in our
  build, so y is read into a local first (as anmScale already did).
  ecl_anm_vm_instr dropped 65.89 -> 65.42 for it; accepted.
- The dead-local lever does not reach inside inlined helpers: dead locals
  in the caller never moved the load or operand order inside an inlined
  ZunTimer tick (kill_all_no_set_death, Spellcard::on_tick_body) or in the
  static outside_range helper (step_ex_08). The count seems to apply within
  each inlined body, so the helper itself would need them.
- The lever also moves scheduling: in Bullet::on_tick dead locals changed
  whether the half-step moves compute all three components before storing
  or store each in turn (7 dead locals measured 77.76 -> 79.94).
- quickdiff can mislead after a layout shift (Bullet::on_tick 51.2 -> 75 in
  quickdiff, 77.76 -> 77.96 in reccmp); confirm with compare.py.
- No dead-local count 1-7 or loop form moved run_ex,
  kill_all_no_set_death, BulletManager::on_tick_body,
  Spellcard::on_tick_body, step_ex_08, step_logic or shoot_one;
  check_player_collision gets worse at counts 4-6; step_interpolators'
  camera y add never flips (field-wise, pointer, D3DXVec3Add and operator+
  either way, an `f32 *` component pointer with counts 0-7).

Third pass over menu states, collision and primitives:
- ascii create_effect (the open menu item): `g_AsciiManager->get_anm()->
  create_effect(...)` with an inline `AsciiInf::get_anm()` accessor; with
  the object behind an inline call MSVC evaluates it after the arguments,
  which gives g_AsciiManager in eax and the result slot in ecx. The
  create_effect_via_pointer wrapper from the alignment work does the same;
  both are in MainMenuStates.cpp now. The accessor also lifted
  Spellcard::start and Gui::show_notice when tried there.
- Many `return 1` vs one: with `return 1` at each exit the constant (shared
  with a cmov) lived in esi for the whole function; leaving every path
  through `break` to the single final `return 1` gives the original's
  per-exit `mov edx, 1; mov eax, edx` (do_practice_stage_select matched).
  This is the "push esi but esi unused" shape.
- `script = diff == EXTRA ? 0x99 : 0x97` instead of `(diff == EXTRA) * 2 +
  0x97` gives the original's spill of script to a stack slot
  (do_subseason_select; in do_character_select it also stopped
  `&anm_ids[script]` being kept as a pointer register). The row color as an
  if/else instead of a ternary swapped edx/esi back
  (on_draw__score_name_entry matched).
- Tail statements, two opposite cases: the OK sound written in each branch
  of the SHOT block stopped MSVC reusing the pressed word for the BOMB test
  (do_score_name_entry matched); the gamemode store and `return 1` written
  once after the stage 1/stage 7 if/else get duplicated into both branches
  with immediate stores, while written in each branch MSVC hoisted them
  (do_subseason_select matched). Try both.
- `goto` into case 3's body reproduces the original's shared tail
  (do_spell_practice_stage_select matched), against the general note that
  shared tails come from duplicated statements.
- Indexing with a named variable makes `this` the base (`[edi + idx +
  disp]`): a byte-offset loop counter (do_spell_practice_row) or a local
  copy of a member index (`i32 cursor = replay_name_cursor;`,
  do_replay_save). The member itself or a strength-reduced counter puts the
  index first.
- Statement order: num_choices stored before wraps reuses the entry load
  (do_difficulty_select); g_last_replay_slot read after the num_choices
  store (do_replay_menu matched); menu.next_selection read into a local
  before create_from_file flips do_replay_save's esi/edi (69.6 -> 95.5).
- `i32 ten = 10;` as the divisor gives one `idiv` by a register for
  `id % 10` and `id / 10` (draw_spell_card_page), like `fps = 60`.
- `__declspec(safebuffers)` on do_music_room's declaration removed its
  cookie (effective match); menu_save_selection(&menu) and
  menu_selection_moved(&menu) stopped MSVC keeping &menu in a register.
- draw_rect: the left pair before the right in the x switch and the bottom
  pair first in the y switch (+6, +8). `g_Globals.subshot +
  g_Globals.character` loads character first like the original.
- Dead ends: do_player_data's vectorized OR order; on_draw__replay's setne
  vs neg/sbb/and (more forms); do_difficulty_select's reload before pop;
  ZunAngle::operator- (eight more forms); the collision rotation store
  order; collision_test_circle_rect; draw_circle's angle add; draw_ring;
  draw_rect's sin/cos slots (an f32[2] gets the order but moves to the
  bottom of the frame); draw_rect_outline's movq copy; BombInf::draw's
  `* 1.0f` (double literals keep the conversions but fold the multiply);
  load_spell_list; draw_spell_card_page (quickdiff and reccmp disagreed).

Player, shots, bombs, items and PosVel (first pass; the agent stalled
before its final report, so this comes from its commits and messages):
- Player::on_draw_callback matched with draw_vm through an inline helper;
  Player::check_hit_rotated_rect with r.y first and `half size + r`;
  sht_on_hit_burst with g_Player read at each use and get_damage_source;
  damage_source_on_hit_bullet with shooter_ref read through the bullet.
- Item::collect_point 57 -> 99 and collect_power 54 -> 75: the piv
  rounding written as an expression of the global
  (`g_Globals.piv / 100 - g_Globals.piv / 100 % 10`), which keeps
  `mov reg, 10; idiv` like the original; through a local it is
  strength-reduced (the same fix matched get_piv_rounded).
- Player::tick_bullets 54 -> 80 with a pointer loop and a negated
  off-screen test; Player::on_tick_body's damage source loop advances a
  pointer (79 -> 81), then the angle wrap, deathbomb branch and power drop
  forms (-> 85). PosVel::step 86 -> 97 with an angle temporary and the
  rotation order in the ellipse case. BombReimuAInf::on_tick 86 -> 95 with
  a goto out of the orb search and a ternary damage source lookup.
  sht_on_hit_laser 60 -> 77 with the damage source looked up in each
  branch; sht_on_tick_sideways 35 -> 43 with a separate no-hurtbox flag
  test (its `not; test al, 1` shape is the `(~flags & 1)` pattern above).
- Its alignment experiment (a dead double in show_notice) and its lead
  that the Item/Globals realignment cluster came from show_notice and
  create_effect started the cross-file alignment work above.

Enemy-file sweep, second half:
- Store before load: when the original stores a member and only then
  loads another, but ours hoists the load above the store, do the store
  through a pointer local to that member (`u32 *f = &flags; *f |= 2;`
  matched EnemyManager::create exactly; `i32 *base = &base_offset;` did
  the same for EclStack::enter). The pointer must hold the member whose
  store has to come first or whose load has to come later; it did not
  help step_ex_17's struct copy.
- Reading `g_BulletManager->bullet_anm` into an `AnmLoaded *anm` local
  before create_vm keeps the manager in eax and loads it before the pushes
  (Bullet::check_player_collision 84.6 -> 90.0, Bullet::on_tick -> 82.2,
  with its dead-local count kept the same modulo 8).
- The named-variable lever does nothing for a scalar `*speed` in an inlined
  tick (even with dead locals inside a local helper), for register swaps
  (clear_all x/y, ecl_enm_create ecx/edx, call_sub), step_ex_12's constant
  load order, step_logic's ternary or EclStack::enter. In clear_all, dead
  locals inside the inlined cancel_bullet change Bullet::cancel's copy but
  never clear_all's.
- tick_goto and tick_split raise Spellcard::on_tick_body in quickdiff but
  lower it in reccmp (85.03 -> 84.78).
- Unfinished lead: Bullet::on_tick's active-case `pos += velocity *
  g_game_speed` in a forceinline helper with two dead locals gave the best
  quickdiff (80.1 vs 76.7); not yet confirmed in reccmp.

Third pass over GUI, stage, GameThread and the ANM manager:
- Padded frames (4 unused bytes, a `push ecx` or a 4-byte-bigger
  `sub esp`) need a callee that wants an aligned stack: a dead double in a
  plain `static inline` helper of its own, called at the top, makes the
  function pad without realigning itself. Matched AnmLoaded::set_vm_script
  (with HARNESS_CALLED), create_vm, create_ui_effect, create_ui_vm,
  create_vm_front and Stage::interrupt_vms; LaserManager::cancel_in_radius
  needed two calls (85.7 -> 95.2). On Player::on_draw_callback it matched
  but cost 4 matches; on preload_anm +10 but EndingScriptVm::run lost its
  match; on open_game_over_menu +4 but two matches lost.
- allocate_new_enemy's junk third argument comes from LTCG dropping the
  unused parameter once it is HARNESS_CALLED; a stand-in caller on a second
  object (src/harness/r3b.cpp) keeps `this` in ecx
  (GameThread::finish_stage_transition effective). In the original
  Bullet::run_ex and ECL still push 0, probably from a call-graph cycle
  compiled before it.
- AnmVm::world_pos: parent_pos in a `static __declspec(safebuffers)
  __forceinline` helper removes the cookie, but lets root_vm be cached; a
  volatile re-read (`*(AnmVm *volatile *)&root_vm`) restores the reload and
  one dead named local the sums' order (effective match). A volatile load
  does not stay ordered after normal stores.
- Look an object up into a local before reading the instruction pointer:
  `vm = get_vm_or_clear(x); vm->f = instr()->args...` loads the instruction
  after the call, into ecx like the original (GuiMsgVm::run).
- `strlen / 2 * 16` compiles to shr; shl; the original's lea; and comes
  from the equivalent `len * 8 & ~15`. A VM pointer local in each branch
  gave run_std's cross-jump into the shared flag store. Passing an index as
  its own parameter to a forceinline create helper puts the add at the
  call after the spill (setup_stage_hud).
- Dead ends: do_key_config's byte load widens to a dword whenever the same
  address is read as a dword anywhere in the function;
  interrupt_child_and_run (the helper pads it but moves registers);
  read_line's alignment nop (five loop forms); Fog::set_rect;
  do_title_screen's 4-byte slot offset; EffectManager::create_ui_effect's
  eax/ecx copies; InterpCameraSky::step's loop operand order;
  update_season_gauge's ebx; Gui::on_tick_body's tick (the original lays
  out the whole-frame block first; no tick form reproduces it).

Sweep of lasers, system and GUI-side files with the new levers:
- Dead locals move register choices and stack slots, not only the x load
  (LaserLineInf::cancel_as_bomb_circle +12.9 with one, get_state +6.2
  with four). Removing a named variable did not act as "minus one".
- The D3DX operation decides which count window applies: in
  LaserCurveNode::step_back six dead locals fixed the dt copy, and only
  `D3DXVec3Add(&sum, &a, &b)` then also fixed the sum's x operands.
- The x flip can come with swapped registers: in LaserInfiniteInf::on_tick
  three dead locals fixed the x add but swapped x and y's registers;
  writing the scaled vector field by field (`Float3 v; v.x = ...;
  position += v;`) with two dead locals matches that block.
- tick_nested matched PauseMenu::on_tick (both timers). In
  PopupManager::on_tick the timer form decides the loop pointer's base.
- Of the loop forms, only the guarded do moved anything (while and
  for(;;)+break compile like for).
- Screening: insert N dead locals into every function at once and build
  once per N, then confirm each hit alone.

Sweep of collision, primitives and menu states:
- A signed pointer compare (`cmp esi, <table end>; jl`) means ZUN wrote an
  index loop (`for (i = 0; i < 4; i++) ... g_rect_edges[i][0]`) that MSVC
  strength-reduced; a pointer loop gives `jb`. This fixed the edge loops
  in collision_line_rect (78.9 -> 83.5) and both nested loops of
  collision_test_rect_rect. Worth a grep for `jl` against a table end.
- `d0 < d1` and `d1 > d0` compile to the same compare, but the side written
  first is computed first and takes the lower registers
  (collision_line_rect's distance block).
- D3DX operators make LTCG load a pointer parameter early:
  `Float3 end = *pos + d; Float3 start = *pos - d;` loads pos into a
  callee-saved register before the sinf/cosf calls like the original;
  field-wise it is loaded after. Not committed (pos lands in edi, the
  original's esi); the most promising lead for collision_line_rect.
- Dead locals changed register and slot choices without any D3DX op (six
  in collision_test_rect_rect +3.3; four in load_spell_list +0.2).
- Neither loop forms nor dead locals inside rotate_points move the
  rotation's y-first order; writing y first gets the schedule but copies
  the registers for y instead of x (97.5 -> 94.4).
- Dead locals 1-7 changed nothing in points_in_rect, circle_rect,
  segment_intersection, ZunAngle::operator-, do_player_data,
  do_difficulty_select, on_draw__replay, do_replay_save,
  do_character_select, draw_spell_card_page, BombInf::draw, draw_rect and
  draw_rect_outline, and made draw_ring and draw_circle_outline worse.

Second pass over player, shots, bombs and PosVel:
- `__declspec(safebuffers)` on the declaration removed the /GS cookie the
  original lacks in BombMarisaAInf::on_tick (which then matched with
  `D3DXVec3Add(&beam_pos, &beam_pos, &pos)` for its third add),
  sht_on_tick_laser, sht_on_tick_sideways and
  PlayerInner::repopulate_options; it made Player::update_options worse.
- Two levers at once: in PosVel::step six dead locals fixed the circle and
  wave sums but flipped `pos += velocity`; scanning the form of the broken
  op together with the count found 5 locals plus
  `D3DXVec3Add(&pos, &velocity, &pos)`, which matched.
- In sht_on_tick_laser a plain dead double did not realign; one with three
  `unused = unused * 2.0;` gave `and esp, -8` with esp-relative locals.
- sht_on_tick_sideways matched (43 -> 100): an inline `advance_enemy_iter`
  returning `EnemyInf *e = node != NULL ? node->entry : NULL; return e;`
  (the iter_advance pattern), `Float3 pos` copied before the unk_15c
  store, and `if (c) set_angle(-PI); else set_angle(0.0f);` instead of a
  ternary argument, which had hoisted `xorps xmm1` to the top.
- `do { if (orb->active) {...} orb++; } while (--i != 0);` gives
  `sub esi, 1; mov [slot], esi; jne`; `for (i = 8; i != 0; i--)` gives
  dec/mov/test (BombReimuAInf::on_tick). A member-pointer call
  `(orb->*orb_update_func())()` replaced a DECOMP_NOINLINE stand-in
  function.
- Timers: Player::tick_bullets wants tick_in_place (+14); all three of
  Player::on_tick_body's timers as tick_nested (+1.4); BombReimuAOrb::update
  stays best with tick_mixed. A named `Gui *gui = g_Gui;` local in
  check_hit_circle fixed the distance registers (+4.3).

Enemy-file sweep continuation:
- Behaviour fix: Bullet::on_tick's offscreen test used 480.0f as the
  bottom bound; the original compares with 448.0f (0x4946b0).
- EnemyData::ecl_enm_create became exact with `EclRawInstr *instr =
  full->context.current_context->current_instr();` written before
  `EnemyInf *vm = full;`.
- A separate copy of a shared inline helper per caller with a different
  body (`f32 v = *x; return v + half <= lo || v - half >= hi;`) fixed
  Bullet::on_tick's add order without touching step_ex_12; a byte-identical
  copy changed nothing. Writing the inlined sprite height lookup out in
  place gave 85 -> 89. The active-case step in a forceinline helper with
  two dead locals confirmed in reccmp (82.2 -> 83.9).
- tick_nested in BulletManager::on_tick_body raises quickdiff 78.6 -> 84.4
  but lowers reccmp 90.67 -> 86.32.
- kill_all_no_set_death and Spellcard::on_tick_body have the same tick
  difference in opposite directions with the same tick(); no form or
  count moves either. step_logic: the original hoists g_MainBomb into the
  prologue where ours hoists g_SubseasonBomb (no form found).

Research: loop-head nops and functions that never use ebx:
- MSVC 19.10 at /O2 aligns a loop head to 16 bytes when that takes at
  most 7 bytes of padding, or (up to 15 bytes) when the loop's first
  instructions would otherwise straddle the 16-byte boundary. "First
  instructions" means up to three, stopping after the first branch or call
  (two if the three come to more than 16 bytes). Loop size, nesting, calls,
  the loop form, function size and `/favor` make no difference; /O1 and /Os
  never pad. No padded head in either binary breaks the rule; 2236 of 2561
  original heads follow it exactly, and every exception is an unpadded
  backward-jump target (shared error or return tails, interpreter jumps, a
  merged head as below).
- To avoid a nop the original lacks: write the head statements once before
  the loop and again at the end of the body. The compiler merges the two
  copies and the back edge jumps to the one before the loop, which is not
  its own loop-head label and is never padded (read_line).
- Extra nops in ScorefileStatus::init and WinMain come from earlier
  code-size differences, not from alignment.
- A COM call written as `g_Supervisor.d3d_device->X(...)` (also d3d and
  back_buffer) makes LTCG leave ebx unused in the whole function, inlined
  code included. The same call through `supervisor_d3d_device()`
  (Supervisor.h) or a local copy of the pointer keeps ebx usable, with
  otherwise identical code. The original has functions of both kinds:
  using the getter at all 268 call sites gave 10 up and 4 down
  (AsciiInf::draw_group, GameWindow::do_frame, render_sprite_2d and
  draw_circle prefer the direct form), so it is chosen per function. The
  same call through another global, another Supervisor-typed global or a
  struct field defined in another file does not trigger it; stack
  alignment, HARNESS_CALLED, dead locals, `/d2:-newcolor-`,
  `/d2:-linscan`, the DirectInput enum callbacks and the `&g_Supervisor`
  registrations are ruled out. Getters for dinput, keyboard, joystick and
  the arcade surfaces gained nothing and cost get_controller_state its
  match. SoundBufferEntry::load (`g_SoundManager.dsound->...`) shows the
  same signature (untested). reload_texture lacks ebx even compiled
  without /GL, so its cause is different.
- Tooling: objdump mis-disassembles .obj files at `$LN` labels (a
  capstone-based COFF disassembler works), and linking with `/d2:-cands`
  crashes LTCG.
