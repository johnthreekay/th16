# Portable build (Linux/macOS, GCC/Clang)

The game sources in `src/` compile and link with GCC and Clang, 32-bit
(`-m32`) and 64-bit, against stand-in Windows/DirectX headers. The platform
layer is stubbed: the binary starts, runs WinMain's setup until loading
th16.cfg fails (CreateFileA is a stub), logs that and exits cleanly, in all
four builds. The matching MSVC build is unaffected (see "Keeping the
matching build").

## Building

```
cmake -S port -B build-port/clang64 -G Ninja -DCMAKE_CXX_COMPILER=clang++
cmake -S port -B build-port/gcc64   -G Ninja -DCMAKE_CXX_COMPILER=g++
cmake -S port -B build-port/clang32 -G Ninja -DCMAKE_CXX_COMPILER=clang++ -DTH16_M32=ON
cmake -S port -B build-port/gcc32   -G Ninja -DCMAKE_CXX_COMPILER=g++ -DTH16_M32=ON
cmake --build build-port/clang64
```

`-DTH16_M32=ON` needs the 32-bit multilib (gcc-multilib / lib32 glibc and
libstdc++). Build output stays under `build-port/` (ignored by git). Tested
with Clang 23 and GCC 16 on Arch Linux; not yet built on macOS.

The CMake build compiles every `src/*.cpp` (the glob is not recursive, so
`src/stub/`, `src/placeholder/` and `src/harness/` stay out) and `port/src/*.cpp`,
with `-DTH16_PORT`, C++17 (gnu++17) and `-include port/include/port_prelude.h`.

Flags that matter for behaviour (CMakeLists.txt): `-fno-strict-aliasing`,
`-fwrapv`, `-fno-delete-null-pointer-checks`, `-fsigned-char`,
`-ffp-contract=off`, GCC's
`-fno-aggressive-loop-optimizations` (the game indexes past arrays, e.g.
`Scorefile::unlock_all` writes `clears[5]` of an `i32[5]`, MainMenu.cpp:782),
and for `-m32` `-msse2 -mfpmath=sse` (the original does its float math in SSE;
x87 would round differently).

Warnings left in a clean build (Clang 41/32, GCC 72/63 for 64/32-bit; no
errors): 31 `-Wdelete-non-virtual-dtor` (the game's
classes have virtual methods and non-virtual destructors, as in the
original), 24 GCC `-Wuninitialized` (constructors that clear one bit of an
uninitialised field: UpdateFunc.h:67, ZunTimer.h:33, written that way to
match), 9 `-Wint-to-pointer-cast` on 64-bit (LaserXInf::method_1c, never
called, see below), 5 `-Wmismatched-new-delete` (DSUtil.cpp's SAFE_DELETE on arrays,
SupervisorSetup.cpp:166 `delete` of a `new[]`), 2 GCC `-Waddress`
(Stage.cpp:610, 631: comparing `&sprites[i]` with NULL), 1 GCC
`-Wformat-overflow` (GameWindow.cpp:415, a 4 KiB save path into a 256-byte
buffer, as in the original).

## Layout of port/

- `CMakeLists.txt`: the build (see above).
- `include/`: what the game includes from the SDKs.
  - `port_prelude.h`: force-included first. Calling convention keywords and
    `WINAPI`/`CALLBACK` are empty, `__forceinline` is `inline`, `__int64` is
    `long long`, `__assume` is a no-op, and on 64-bit `static_assert` is
    redefined to pass (the game's asserts describe the 32-bit layout; they
    stay in force for `-m32`). Includes `port_crt.h` and `port_fpu.h`.
  - `port_crt.h`: MSVC CRT extensions (`_stricmp`, `_vsnprintf_l`,
    `_vsprintf_l`, `__time64_t`, `_time64`, `_localtime64`, and a
    `localtime(const __time64_t *)` overload since MSVC's `time_t` is 64-bit).
  - `port_fpu.h`: the x87 inline assembly as functions (see below).
  - `port_com.h`: HRESULT, GUID, IUnknown, `DEFINE_GUID` (declares only),
    CoInitialize/CoCreateInstance.
  - `windows.h`, `mmsystem.h`, `mmreg.h`, `winnls32.h`, `shlobj.h`,
    `process.h`, `direct.h`: kernel32/user32/gdi32/winmm/shell/CRT subsets.
  - `d3d9.h`, `d3dx9.h`, `d3dx9math.h`, `d3dx9tex.h`, `dinput.h`, `dsound.h`.
- `src/`: the platform layer, one file per subsystem. `PORT_UNIMPLEMENTED()`
  (`port_stub.h`) marks every stub and logs its name once to stderr;
  `grep -rn PORT_UNIMPLEMENTED port/src` lists what is left.
  - `main.cpp`: `main()` builds the command line and calls `WinMain`.
  - `crt_compat.cpp`: complete except `_beginthread(ex)` (stubs).
  - `win32_kernel.cpp`: files, find, modules, environment: stubs.
    Get/SetLastError, FormatMessageA (generic text, the game logs it
    unchecked), LocalFree, GetModuleHandleA(NULL): done.
  - `win32_thread.cpp`: critical sections (heap `std::recursive_mutex`, see
    below), Sleep, QueryPerformanceCounter/Frequency, timeGetTime,
    GetTickCount: done. Threads, events, waits: stubs. CreateMutexA returns
    a dummy handle (the game quits on NULL).
  - `win32_user.cpp`: window, messages, dialog, cursor, keyboard state:
    stubs.
  - `win32_misc.cpp`: winmm joystick (stubs), COM and shell link (fail, which
    the game handles), WINNLSEnableIME (no-op).
  - `gdi_stubs.cpp`: the text renderer's GDI calls: stubs.
  - `d3d9_stubs.cpp`: stub classes for IDirect3D9, IDirect3DDevice9,
    IDirect3DTexture9, IDirect3DSurface9, IDirect3DVertexBuffer9 with every
    method. Direct3DCreate9 returns the stub IDirect3D9, whose CreateDevice
    fails.
  - `d3dx9_math.cpp`: complete (matrix, vector, projection functions).
  - `d3dx9_tex.cpp`: texture creation and surface loading: stubs.
  - `dinput_stubs.cpp`: stub classes for IDirectInput8A and
    IDirectInputDevice8A, the data formats and DIPROP ids.
    DirectInput8Create fails (the game falls back to GetKeyboardState and
    joyGetPosEx).
  - `dsound_stubs.cpp`: stub classes for IDirectSound8, IDirectSoundBuffer,
    IDirectSoundNotify. DirectSoundCreate8 fails (the game runs silent).
  - `game_tables.cpp`: the game globals the matching build defines in
    `src/stub/` (see "Game data in src/stub/").
  - `layout_checks.cpp`: compile-time layout checks that stay on in the
    64-bit build (`TH16_PORT_CHECK`, defined in port_prelude.h).

The interfaces in `include/` are C++ abstract classes with only the methods
the game calls (plus a few obvious companions), in an order of our own: the
vtable layout is not COM's, which nothing here needs. `IUnknown` has a
virtual destructor. To implement one, derive from it (or from
`PortComObject<I>` in `port_stub.h` for the IUnknown part) and replace the
stub class's methods; constants, enum values and structure layouts are the
SDK's, so code and data that use them keep their meaning.

## How the MSVC-specific code is handled

- `src/decomp.h`: a `#ifdef TH16_PORT` branch defines `DECOMP_NOINLINE` as
  `__attribute__((noinline))`, `DECOMP_ALIGN16` as
  `__attribute__((aligned(16)))`, `HARNESS_CALLED`, `LTCG_FASTCALL`,
  `LTCG_VECTORCALL` as nothing and `LTCG_NOTHROW` as `noexcept`.
- `src/types.h`: `iptr`/`uptr`, integers that hold pointers (`i32`/`u32`
  for MSVC, `intptr_t`/`uintptr_t` in the port).
- Inline assembly: every `__asm` block has a `#ifdef TH16_PORT` branch that
  calls a `port_fpu.h` helper with the same arguments: `port_sincosmul`,
  `port_sincosmul2` (the fsincos multiply copies, 20 of them),
  `port_sincos` (fld/fsincos/fstp pairs), `port_frndint_sub` (AnmDraw.cpp's
  pixel snapping) and `port_finit` (`__asm finit`, a no-op). On x86 and
  x86-64 the helpers run `fsincos` itself through GNU inline asm on
  `long double`, so results are bit-identical to the original (which runs
  them after `finit`, in 64-bit precision). Elsewhere (ARM macOS) they fall
  back to `sinl`/`cosl`, which can differ in the last bit. Main's readability
  pass plans to move these blocks behind helpers in `src/ZunAsm.h`; when that
  merges, the `#ifdef TH16_PORT` branches belong inside those helpers.
- MSVC extensions in the code itself:
  - taking the address of a temporary vector (`&(a - b)`,
    `&Float3(x, y, z)`): `D3DXVECTOR2/3/4` and `D3DXMATRIX` have a member
    `operator&`, which is legal on temporaries;
  - binding `AnmId &` to a temporary (`get_vm_or_clear(find_child_id(...))`):
    an rvalue overload in AnmManager.h under `TH16_PORT`;
  - two `goto`s jumping past initialisations: BulletManager.cpp (the angle
    in case 4 moved into its own block) and PauseMenu.cpp (`choice`
    declared, then assigned). Both unguarded; MSVC output unchanged.
- Calling conventions are irrelevant in the port (every caller and callee is
  ours), so they expand to nothing, including in function pointer types.

## Things the platform layer has to provide faithfully

- Floating point. The original does float math in SSE (no extended
  precision) except the fsincos blocks above; keep `-ffp-contract=off` and
  `-mfpmath=sse` for `-m32`. The game creates its D3D9 device without
  `D3DCREATE_FPU_PRESERVE`, which puts the x87 unit in 24-bit precision, but
  it also runs `finit` in several places (the Rng float functions,
  GameThread.cpp, Spellcard.cpp, Gui.cpp), after which the unit is in 64-bit
  precision; the fsincos helpers use 64-bit precision throughout.
  D3DX math (`d3dx9_math.cpp`) uses the documented formulas, while the real
  D3DX used SSE code paths: results can differ in the last bit. The game
  uses D3DX mostly for drawing, but D3DXVec2Normalize is also used by
  bullet code (BulletManager.cpp:1937-1952), which matters for replay
  compatibility.
- `CRITICAL_SECTION` keeps its x86 layout (24 bytes, an array of them sits in
  `CriticalSections`). The implementation keeps a heap mutex pointer in
  `LockSemaphore`.
- Files: `CreateFileA` and friends get Windows paths with backslashes
  (`"replay\\th16_01.rpy"`, `save_dir` built from `APPDATA` +
  `\\ShanghaiAlice\\th16\\`) and names written for a case-insensitive file
  system. Map separators, look names up case-insensitively, and give
  `GetEnvironmentVariableA("APPDATA")` a host directory (XDG data dir).
- Threads: the loading thread (Thread.cpp, `_beginthreadex`), the sound
  init/load threads and the BGM thread (CreateThread; the BGM thread waits
  on an auto-reset event with MsgWaitForMultipleObjects and quits on a
  posted WM_QUIT), the screenshot writer (`_beginthread`). HANDLEs must be
  waitable (WaitForSingleObject with timeouts) and closable.
- Text: every string in the game is Shift-JIS bytes (literals are written
  as `\x` escapes; MSG/ending scripts, spell card names and menus come from
  the data files the same way). Keep them as bytes everywhere; convert
  Shift-JIS to UTF-8/UTF-32 only where text reaches the host: the GDI text
  renderer (TextHelper.cpp: `CreateFontA` with `SHIFTJIS_CHARSET` and MS
  Gothic/Mincho/Meiryo face names given in Shift-JIS, `TextOutA` into a DIB
  section), `MessageBoxA`, the log. File names are ASCII.
- Input: DIK_* scan codes (DirectInput keyboard, 256-byte state) and VK_*
  codes (GetKeyboardState fallback) are both used.

## Game data in src/stub/

`src/stub/`, `src/placeholder/` and `src/harness/` hold no game functions
(no `FUNCTION` annotations; the placeholders and harness callers only
shape MSVC's output, and the link succeeds without them). But `src/stub/`
defines 19 real game globals (with `GLOBAL` annotations), zero-filled so that
LTCG cannot see their contents. In the original 10 of them have contents:
the ANM callback tables `g_anm_on_switch_funcs`, `g_anm_on_destroy_funcs`,
`g_anm_on_copy_funcs`, `g_anm_serialize_funcs`, `g_anm_on_tick_funcs`,
`g_anm_sprite_mapping_funcs`, `g_anm_on_draw_funcs`, the effect table
`g_effect_table`, the stage table `g_stage_table` and
`g_spell_difficulty`. `port/src/game_tables.cpp` defines all 19 with the
original's contents (read from th16.exe 1.00a). When main moves them into
`src/` (its readability plan), delete the entries here: the zero-filled
ones are weak and give way silently, the tables are strong and the link
reports the duplicate.

## Keeping the matching build

- Edits to game files are either inside `#ifdef TH16_PORT` or produce the
  same code for MSVC (types that are typedefs of the same type, sizes
  written as `sizeof` of what they measure, scoping, a declaration split
  from its assignment). After each change:
  `.venv/bin/python scripts/build.py`, `scripts/check_unchanged.py` (all
  1214 functions unchanged) and `scripts/quickdiff.py | grep -c MATCH`
  (842 at the start of this branch and after it). `check_unchanged` always
  reports `.rdata` as different: the linker writes a new timestamp and PDB
  id each time, even for identical builds.
- Edited game files (expect merge conflicts with main's renames on these
  lines): decomp.h, types.h, AnmManager.h, Supervisor.h (`unk_0`),
  Scorefile.h, SoundManager.h, Player.h, Player.cpp, PlayerShot.cpp,
  AnmDraw.cpp, AnmManagerVms.cpp, BombMain.cpp, Stage.cpp,
  BulletManager.cpp, PauseMenu.cpp, and the 19 files with `__asm` blocks.
- The game's `static_assert`s are off in the 64-bit build. Layout facts that
  must hold there too go in `port/src/layout_checks.cpp` as
  `TH16_PORT_CHECK(...)` (file structures, the Scorefile and BgmStream
  views, CSound::m_desc).

## 64-bit problems

Found by compiling (pointer/int casts are errors in GCC and Clang), by size
probes at both pointer sizes and by reading the loaders. ANM, ECL, MSG and
ending scripts keep 4-byte offsets in the file and pointers in separate
arrays or locals, so they are fine; so are replays (`RpyInfo`,
`RpyGamestate`), th16.cfg (`Config`), thbgm.fmt (`ThBgmFormat`), the archive
directory and the score file sections, which hold no pointers.

### Fixed (MSVC output unchanged)

- Pointers passed or stored as `i32`/`u32`, now `iptr`/`uptr` (types.h):
  the sht hit callbacks' position and size arguments (Player.h:210-211
  typedefs, PlayerShot.cpp, Player.cpp:891 call; sht_on_hit_446870 casts
  them back to `Float3 *`), `AnmManager::render_cache_184fbc0` (a sprite
  pointer used as a cache key, AnmManager.h:316, AnmDraw.cpp:657-660),
  `BombInf::method_c` arguments (Player.cpp:823), the null `AnmId` returns in
  AnmManagerVms.cpp:970,1017.
- `Supervisor::unk_0` holds the HINSTANCE (WinMain.cpp:445 writes it through
  `*(HINSTANCE *)`); was `u8[4]`, which overwrote `d3d` on 64-bit and
  crashed at shutdown. Now `u8[sizeof(void *)]`.
- STD (Stage.cpp `load_std`): the `StdHeader::objects` table of 4-byte
  offsets was rewritten into pointers in place. The port builds its own
  table (freed in `~Stage`).
- SHT (Player.cpp `read_sht_file`): `ShtFile::shooter_arrays` holds offsets
  and `ShtShooter`'s four callbacks hold table indices, both 4 bytes in the
  file; with 8-byte pointers `ShtShooter` is 0x68 bytes, not 0x58, and the
  shooters start at 0x1e0, not 0x1b8. The port converts the file to the
  in-memory layout first (`port_convert_sht_file`), then the original loop
  resolves offsets and indices.
- Scorefile (Scorefile.h): `ScorefileData` (the real layout) starts with
  two pointers, `Scorefile` (the view most code uses) assumes 8 bytes for
  them. The view gets 8 bytes of padding on 64-bit and `ScorefileData` is
  packed to 4, so both views and `sizeof` agree.
- `CSound::m_desc` (SoundManager.h:212) was `u8[0x24]`; DSUtil.cpp:140,215
  copy a whole `DSBUFFERDESC` (0x28 bytes on 64-bit) into it, clobbering
  `m_manager`. Now `u8[0x20 + sizeof(void *)]`.
- `BgmStream` (SoundManager.h:111), a hand-laid-out view of
  `CStreamingSound`: packed to 4 like `CSound` and its gap before
  `refilling` widened on 64-bit.
- Reimu's bomb orbs (BombMain.cpp:321) were allocated as 0x6c0 bytes; they
  hold an `EnemyInf *` each and need 0x700 on 64-bit. Now
  `sizeof(BombReimuAOrbs)`.

### Open

- `LaserXInf::method_1c(i32 a, i32 b, i32 c, i32 d, i32 e, i32 f)`
  (Laser.h:70,167,235,350,413; Laser.cpp:1110-1115, 1197-1202, 1271-1276;
  LaserBeam.cpp:22) takes pointers in a, b and f and casts them back
  (the 9 `-Wint-to-pointer-cast` warnings). Nothing calls it; if something
  does, make those three `iptr`.
- Function pointer types that do not match the function (also in the
  original, harmless on x86-64 and ARM64 since the mismatched argument is
  unused or of the same size class): `g_effect_table[1].init` is
  `anm_effect_2_init(AnmVm *, i32)` called as `(AnmVm *, D3DXVECTOR3 *)`;
  `g_anm_serialize_funcs[1]` takes `u8 *` for `void *`;
  `SoundManager::thread_init`/`thread_load_sound_files` return void but are
  started as `LPTHREAD_START_ROUTINE` (WinMain.cpp:401, Supervisor.cpp:994;
  only the exit code is garbage).
- `#pragma pack(4)` structs hold 8-byte pointers at 4-aligned offsets
  (Supervisor, GameWindow, CSound, PauseMenu, Spellcard, Scorefile,
  RpyInfo): fine for plain loads and stores on x86-64 and ARM64, wrong for
  atomics.
- AnmVm snapshots (AnmManagerVms.cpp:788-878, AnmVmCallbacks.cpp:269-310)
  copy pointer-holding `AnmVm` records by `sizeof`. Consistent within one
  build; they stay in memory (not in replays), so nothing to do.

### Not 64-bit, but noticed

- Fog.cpp:27 allocates one byte too few
  (`sizeof(AnmVm *) * FOG_STRIP_COUNT - 1`): the original's bug, in both
  builds.
- MainMenu.cpp:782 writes `clears[5]` of an `i32[5]` (into the next field,
  as in the original); GCC needs `-fno-aggressive-loop-optimizations`.
- GameWindow.cpp:415 formats a 4 KiB `save_dir` into a 256-byte path
  buffer; keep the host save path short or the original overflow happens.
- `delete` on `new[]` memory (DSUtil.cpp SAFE_DELETE, SupervisorSetup.cpp:166).
