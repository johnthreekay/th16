# Portable build (Linux/macOS, GCC/Clang)

The game sources in `src/` compile and link with GCC and Clang, 32-bit
(`-m32`) and 64-bit, against stand-in Windows/DirectX headers. Audio
(DirectSound over SDL2, see "Audio") and graphics (Direct3D 9 over OpenGL,
see "Graphics") are done; files, threads, the window and input are still
stubs, so the binary starts, runs WinMain's setup until loading th16.cfg
fails (CreateFileA is a stub), logs that and exits cleanly, in all four
builds. With the test shim (`-DTH16_TEST_SHIM=ON`, see "Graphics") it
plays: title screen, menus and stage 1 render. The matching MSVC build is
unaffected (see "Keeping the matching build").

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

The build needs SDL2 (found with `find_package(SDL2 CONFIG)`, target
`SDL2::SDL2`; sdl2-compat 2.32 works), and the 32-bit SDL2 (lib32-sdl2 or
lib32-sdl2-compat) for `-m32`. `TH16_M32` puts `-m32` into
`CMAKE_CXX_FLAGS_INIT`, so that CMake sees a 32-bit compiler and finds the
lib32 SDL2: configure 32-bit build directories made before that change
again from scratch. The build also makes the audio tests
(`th16_dsound_test`, `th16_dsound_game_test`, see "Audio"); `ctest` in a
build directory runs the part that needs no game data.

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
  - `d3d9_gl.cpp`: Direct3D 9 (IDirect3D9, the device, textures,
    surfaces, vertex buffers) on OpenGL 3.3 through SDL2, complete for what
    the game uses (see "Graphics"). `d3d9_gl.h` is its interface for the
    window code, `d3d9_gl_internal.h` what it shares with d3dx9_tex.cpp,
    `d3d9_gl_funcs.h` the GL functions it loads.
  - `d3dx9_math.cpp`: complete (matrix, vector, projection functions).
  - `d3dx9_tex.cpp`: the D3DX texture helpers: complete except image file
    decoding, which the game data never needs (see "Graphics").
  - `dinput_stubs.cpp`: stub classes for IDirectInput8A and
    IDirectInputDevice8A, the data formats and DIPROP ids.
    DirectInput8Create fails (the game falls back to GetKeyboardState and
    joyGetPosEx).
  - `dsound_sdl.cpp`, `dsound_sdl.h`: DirectSound 8 as a software mixer on
    one SDL audio device, complete for what the game uses (see "Audio").
  - `game_tables.cpp`: the game globals the matching build defines in
    `src/stub/` (see "Game data in src/stub/").
  - `layout_checks.cpp`: compile-time layout checks that stay on in the
    64-bit build (`TH16_PORT_CHECK`, defined in port_prelude.h).
- `tests/`: the audio tests (see "Audio"), `th16dat.py`, a Python copy
  of the game's th16.dat reader that lists and extracts files for tests,
  and `platform_shim.cpp`, the test-only platform layer (see "Graphics").

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
  waitable (WaitForSingleObject with timeouts) and closable. SetEvent is
  also called by the audio mixer from SDL's audio thread (see "Audio").
- Text: every string in the game is Shift-JIS bytes (literals are written
  as `\x` escapes; MSG/ending scripts, spell card names and menus come from
  the data files the same way). Keep them as bytes everywhere; convert
  Shift-JIS to UTF-8/UTF-32 only where text reaches the host: the GDI text
  renderer (TextHelper.cpp: `CreateFontA` with `SHIFTJIS_CHARSET` and MS
  Gothic/Mincho/Meiryo face names given in Shift-JIS, `TextOutA` into a DIB
  section), `MessageBoxA`, the log. File names are ASCII.
- Input: DIK_* scan codes (DirectInput keyboard, 256-byte state) and VK_*
  codes (GetKeyboardState fallback) are both used.

## Audio

`port/src/dsound_sdl.cpp` implements DirectSound 8 as a software mixer
feeding one SDL device (44.1 kHz, signed 16-bit stereo, 512-frame periods),
opened by the first `DirectSoundCreate8` and closed with the last
`IDirectSound8`. If SDL cannot open a device, `DirectSoundCreate8` fails
with `DSERR_NODRIVER` and the game runs silent, as on Windows without a
sound card.

What the game uses, and so what is implemented (SoundManager.cpp,
DSUtil.cpp):

- The primary buffer, only for `SetFormat(44.1 kHz, 16-bit, stereo)`. The
  format is recorded (GetFormat returns it) but the mix format is fixed.
  Its volume scales the mix.
- Sound effects: one secondary buffer per effect, filled from the data chunk
  of a `.wav` in th16.dat (22.05 kHz, 8-bit mono, 8-bit stereo, 16-bit mono
  or 16-bit stereo), `DSBCAPS_CTRLVOLUME | DSBCAPS_CTRLPAN`, and
  `DuplicateSoundBuffer` for effects sharing a file (shared memory, own
  position, volume, pan). The game parses the RIFF chunks itself
  (`get_wav_chunk`) and never calls `mmio*`: th16.exe imports none, and
  ZUN's `CWaveFile` reads thbgm.dat with `CreateFileA`/`ReadFile`
  (`MMCKINFO`/`MMIOINFO` only remain as fields). So there is no mmio layer.
- A silent 0x8000-byte buffer that loops all session.
- The BGM: `CStreamingSound`, a looping buffer of 8 half-second chunks with
  a notification at the last byte of each, all on one auto-reset event. The
  BGM thread refills the chunk that has just played from thbgm.dat
  (`HandleWaveStreamNotification`), checking it against the play and write
  cursors first. Fades are `SetVolume`. Seeks and track switches release the
  buffer and create a new one.

Semantics: `Lock` returns a copy of the region (two parts when it wraps
and the caller asks for the second), `Unlock` writes it back, so the game
never writes memory the mixer reads. The play cursor is where the mixer has
read up to. The write cursor of a playing buffer is a mix period plus one
frame ahead of it (what the next mix may read), and equal to it when
stopped. A notification fires once the mixer has consumed its byte.
`DSBPN_OFFSETSTOP` fires on `Stop` of a playing buffer and when a one-shot
buffer ends (it then rewinds to 0, as DirectSound does). Volume and pan are
hundredths of a decibel (`DSBVOLUME_MIN` is silence, a positive pan
attenuates the left channel). Mono plays at full level on both channels.
Other rates are resampled with linear interpolation, and a 44.1 kHz 16-bit
stereo buffer at volume 0 comes out bit-exact. `SetFrequency`, `GetCaps`,
`GetFormat` and the controls' `DSERR_CONTROLUNAVAIL` /
`DSERR_INVALIDPARAM` checks are DirectSound's. Not implemented (unused):
3D and effects buffers (`DSBCAPS_CTRL3D`, `DSBCAPS_CTRLFX` fail),
writing the primary buffer, voice management (`DSBCAPS_LOCDEFER`
priorities). Buffers are never lost.

Threads: one mutex guards the buffer list and every buffer's state and
data. The SDL callback mixes under it, and every DirectSound method takes
it, so the game's threads (main, sound init, BGM) can call in at any time.

Interface with the Win32 layer: the mixer signals notification events
with plain `SetEvent(HANDLE)` on the handles the game created with
`CreateEventA` (the BGM event is auto-reset). It calls it from SDL's audio
thread while holding the mixer mutex (so that a stopped or released buffer
never signals afterwards: the game closes the event right after stopping
the stream). So `SetEvent` must be thread-safe and must not call back into
DirectSound. The BGM thread then needs `MsgWaitForMultipleObjects(1,
&event, FALSE, INFINITE, QS_ALLEVENTS)` to return `WAIT_OBJECT_0` for the
event and `WAIT_OBJECT_0 + 1` for a message posted with
`PostThreadMessageA` (`WM_QUIT` ends it). Streaming also uses `CreateFileA`,
`ReadFile`, `SetFilePointer` (`FILE_BEGIN` up to 388 MB into thbgm.dat, and
`FILE_CURRENT` with 0 to read the position when pausing) and `CloseHandle`
on `INVALID_HANDLE_VALUE` (CWaveFile closes twice). `DirectSoundCreate8`
runs on the sound init thread, so `SDL_InitSubSystem(SDL_INIT_AUDIO)` does
too. That works on Linux.

Tests (never play to a real device: they use SDL's `dummy` driver, or the
`disk` driver writing to a file under the build directory):

```
port/tests/run_dsound_tests.sh build-port/clang64 "<game dir>"
```

- `th16_dsound_test`: unit tests in manual mode (`port_dsound_set_manual`:
  no device, the test calls `port_dsound_mix` and compares every output
  frame): looping, one-shot with interpolation and rewind, cursors, `Lock`
  wrap-around and `DSBLOCK_*`, notifications, volume, pan, frequency,
  duplicates, clamping. With `--data`: eight se_*.wav files of every format
  loaded and played the way SoundManager does. With `--bgm`: a copy of
  CStreamingSound refilling from thbgm.dat on its own thread. The output
  must equal the track sample for sample until 10 s past the loop point.
  `--realtime`: through SDL (refuses any driver but `disk` and `dummy`),
  DirectSound created on another thread as in the game: 5 s of a stream
  with two loop points, a tone at -6 dB and an effect, checked in the file
  `SDL_DISKAUDIOFILE` names.
- `th16_dsound_game_test <data dir> <thbgm.dat> [track]`: the game's own
  DSUtil.cpp on the mixer (CreateStreaming as in open_bgm, BGM command 2's
  steps, a thread like `bgm_thread_proc`): the title theme (th16_01.wav)
  comes out bit-exact through its loop point, across `Pause`/`Unpause` and
  after `seek`. The test provides the few Win32 calls DSUtil.cpp makes.

All pass in the four builds, and under ThreadSanitizer (clang64 with
`-fsanitize=thread`). The game itself does not reach DirectSound yet (it
stops at th16.cfg). Once files, threads and events work, the title BGM
should stream as in `th16_dsound_game_test`.

## Graphics

`port/src/d3d9_gl.cpp` implements Direct3D 9 on an OpenGL 3.3 core context
through SDL2 (no libGL link: the GL functions in `d3d9_gl_funcs.h` are
loaded with `SDL_GL_GetProcAddress`), `port/src/d3dx9_tex.cpp` the D3DX
texture helpers on top of it.

### Interface for the window code (`port/src/d3d9_gl.h`)

The renderer does not own the window. The window code (win32_user*.cpp):

1. after `SDL_Init(SDL_INIT_VIDEO)`, calls `port_gl_prepare_window()` (GL
   attributes), then creates the window with `port_gl_window_flags()`
   (`SDL_WINDOW_OPENGL`) added to its own flags;
2. calls `port_gl_attach_window(window)` before the game's `CreateDevice`
   (CreateWindowExA is the natural place); the HWND the game passes to
   `CreateDevice` is not looked at;
3. calls `port_gl_detach_window(window)` before destroying it;
4. maps mouse positions with `port_gl_window_to_back_buffer` if it needs
   them (Present letterboxes the back buffer into the window).

If nothing is attached, `CreateDevice` initializes SDL video itself and
opens a plain window of its own (`port_gl_window()` returns whichever is
in use), so the renderer works with the stub window code. The renderer
never pumps events; full-screen modes (`Windowed = FALSE`) become
`SDL_WINDOW_FULLSCREEN_DESKTOP` on the window in CreateDevice/Reset.
GL is only called from the thread that created the device (the game's main
thread).

### How it works

- One shader emulates the fixed-function state the game uses:
  pre-transformed (`XYZRHW`) and world-space (`XYZ`) vertices with diffuse
  color and one texture coordinate; the world, view and projection
  matrices; the texture matrix (`D3DTTFF_COUNT2`, input `(u, v, 1, 0)`, not
  applied to pre-transformed vertices, as in D3D); texture stage 0's color
  and alpha operations and arguments (every D3DTOP the game could set,
  `D3DTA_COMPLEMENT`/`ALPHAREPLICATE`, the texture factor; without a
  texture, an operation that reads it selects the current color, as D3D
  drivers do); alpha test (8-bit compare); vertex fog (linear, exp, exp2,
  range fog). Blending (separate alpha, every blend op), depth test and
  write, culling, color write mask and the viewport (with MinZ/MaxZ) are
  GL state. Uniforms are only sent when they change; vertices go through
  one streaming buffer (mapped unsynchronized, orphaned when full).
- Coordinates: every render target, the back buffer included, is an RGBA8
  texture with an FBO, stored with D3D's row 0 (the top) first. The shader
  flips y so that GL's window row 0 is D3D's row 0, moves vertices by half
  a pixel (D3D9 pixel centers are on integers), and remaps clip-space z
  from [0, w] to [-w, w]. Pre-transformed vertices are mapped through the
  viewport, so they are clipped to it as on D3D9 hardware, with depth
  clamping instead of near/far clipping, and 1/rhw as w. Present blits the
  back buffer to the window, flipped and scaled to fit (aspect kept).
- Textures keep their pixels in their D3D format (as the managed pool
  does); LockRect hands those out and UnlockRect marks the rectangle
  dirty; the GL texture (always RGBA8) is created and updated when a draw
  on the device thread needs it. So the loading thread can create and fill
  textures while the main thread draws, as the game does. Formats:
  A8R8G8B8, X8R8G8B8, R5G6B5, X1R5G5B5, A1R5G5B5, A4R4G4B4, X4R4G4B4,
  A8R3G3B2, R3G3B2, R8G8B8, A8 (samples as (0, 0, 0, a), as in D3D9), L8,
  A8L8; one mip level (the game draws without mipmaps). Render targets
  have no CPU copy: LockRect, D3DXLoadSurfaceFromSurface and the
  screenshot read them back with glReadPixels (device thread only). All
  render targets share one depth buffer (24-bit; D3D's was D16), grown to
  the largest target, like D3D's single auto depth surface. SetRenderTarget
  resets the viewport to the whole target and Clear is clipped to the
  viewport, as in D3D9.
- Present: the game expects Present to wait for a 60 Hz vertical blank.
  Vsync is on unless the game asks for `D3DPRESENT_INTERVAL_IMMEDIATE`; on
  a display that is not at 60 Hz (or when 60 swaps take under half a
  second: hidden windows, drivers that ignore the swap interval), Present
  also waits on a 60 Hz timer. GetAdapterDisplayMode reports 60 Hz for that
  reason, and GetRasterStatus is always in the vertical blank.
- Image files: D3DXCreateTextureFromFileInMemoryEx,
  D3DXLoadSurfaceFromFileInMemory and D3DXGetImageInfoFromFileInMemory only
  get data for ANM entries without embedded pixels that name an image
  file, read from disk next to the game (`file_read_all(..., 1)`), never
  from th16.dat. th16.dat has none: its 54 .anm files hold 417 embedded
  textures (formats 1, 3, 5 and 7 of `g_anm_d3d_formats`: A8R8G8B8 190,
  R5G6B5 52, A4R4G4B4 136, A8 39), 4 empty textures and 2 render targets
  (`tests/th16dat.py` to extract, then read the entry headers). So no image
  decoder is vendored: those three functions log the file's first bytes
  and fail. The rest of D3DX is complete: format conversion between all
  the formats above, D3DX_FILTER_NONE (no scaling, transparent black
  outside the source), POINT, and the other filters as an area average
  when shrinking (the 2:1 low-resolution textures at 640x480) or bilinear
  when growing; color keys; D3DX_DEFAULT sizes.
- Not implemented (the game never uses them): stages above 0, lighting,
  vertex and pixel shaders, index buffers, stencil, point sprites,
  specular, `Present` with rectangles, `StretchRect` from or to a
  non-render-target, MSAA. Each logs once through PORT_UNIMPLEMENTED if it
  is ever reached.

### Testing

Until the real platform layer exists, `-DTH16_TEST_SHIM=ON` links
`port/tests/platform_shim.cpp` first with `--allow-multiple-definition`, so
its files, threads, events, window (attached to the renderer) and scripted
keyboard replace the stubs. Without a sound device it also drops the
queued BGM commands, which GameThread otherwise waits for forever before
a stage starts. Never needed once the platform layer is in. Its file layer
reads missing files from the game folder (`TH16_GAME_DIR`, by default
`~/Touhou Project/(TH16) Touhou Tenkuushou ~ Hidden Star in Four Seasons`)
read-only, and writes only to the current directory (APPDATA is
`./appdata`, so th16.cfg, score and snapshots go to
`./appdata/ShanghaiAlice/th16/`).

Headless run (no window on the desktop, no sound), from a run directory
under build-port/ holding `appdata/ShanghaiAlice/th16/th16.cfg` (a
windowed configuration):

```
env -u DISPLAY -u WAYLAND_DISPLAY SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy \
    TH16_GL_DUMP_DIR=frames TH16_GL_DUMP_EVERY=300 TH16_GL_EXIT_AFTER=3600 \
    TH16_SHIM_KEYS=900:Z:5,940:Z:5,... ../shim64/th16
```

Renderer environment variables (any build): `TH16_GL_DUMP_DIR` (write
the back buffer as `frame_NNNNNN.png` there), `TH16_GL_DUMP_EVERY` (every
N presents, default 60), `TH16_GL_DUMP_FROM` (first frame to dump),
`TH16_GL_EXIT_AFTER` (exit after N presents), `TH16_GL_TRACE_FRAME` (log
the render target changes, clears and draws with their state for that
frame), `TH16_GL_PACE=0` (no 60 Hz timer: run as fast as possible),
`TH16_GL_VSYNC=0`. Shim: `TH16_SHIM_KEYS` ("frame:KEY:frames,...", frames
counted in GetKeyboardState calls; Z X C P UP DOWN LEFT RIGHT ESC SHIFT
CTRL ENTER), `TH16_GAME_DIR`.

Checked this way (clang 32- and 64-bit, NVIDIA through EGL with SDL's
offscreen driver), at 640x480 and 1280x960: the loading screen, the title
screen, difficulty, character and season selection, stage 1 (3D forest
with fog, enemies, items, HUD, the spring season release, the player's
invincibility blink), the pause menu (its blurred copy of the playfield
goes through D3DXLoadSurfaceFromSurface from a render target), and the
in-game screenshot (P: back buffer LockRect, written as a BMP by the
game's thread). Text drawn through GDI (TextHelper) stays blank until the
GDI layer exists; the textures it fills work like any other.

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
  resolves offsets and indices. A table ends at the first record with a
  negative fire rate, and the file's terminators are shorter than a record
  (table offsets are not multiples of 0x58), so the conversion copies each
  table record by record and writes a whole terminator record after it
  (needed in the 32-bit build too: it crashed at the first game start).
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
