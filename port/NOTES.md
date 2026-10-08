# Portable build (Linux/macOS, GCC/Clang)

The game sources in `src/` compile and link with GCC and Clang, 32-bit
(`-m32`) and 64-bit, against stand-in Windows/DirectX headers. The platform
layer is complete for what the game uses: files, threads, the window and
its messages, keyboard and controller input, GDI text (see "Platform
layer"), audio (DirectSound over SDL2, see "Audio") and graphics
(Direct3D 9 over OpenGL, see "Graphics"). The game boots with real files,
threads, input and text to its title menu and plays, in all four builds;
with `-DTH16_NULL_RENDERER=ON` (a Direct3D that draws nothing) it also
runs without a display. It can also load a thcrap patch stack (the
English patch, for example) from the user's thcrap folder, see "thcrap".
The matching MSVC build is unaffected (see "Keeping the matching build").


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
`SDL2::SDL2`; sdl2-compat 2.32 works), FreeType and fontconfig (through
pkg-config; on Arch freetype2 and fontconfig) and, for `-m32`, the 32-bit
versions of all three (lib32-sdl2 or lib32-sdl2-compat, lib32-freetype2,
lib32-fontconfig; CMake points pkg-config at `/usr/lib32/pkgconfig` or
`/usr/lib/i386-linux-gnu/pkgconfig` unless `PKG_CONFIG_LIBDIR` is set).
iconv comes with glibc (macOS: libiconv). At run time the text renderer
wants a Japanese font (Noto Sans Mono CJK JP and Noto Serif CJK JP are
preferred, see "Text"). `TH16_M32` puts `-m32` into
`CMAKE_CXX_FLAGS_INIT`, so that CMake sees a 32-bit compiler and finds the
lib32 SDL2: configure 32-bit build directories made before that change
again from scratch. The build also makes the audio tests
(`th16_dsound_test`, `th16_dsound_game_test`, see "Audio") and the platform
test (`th16_platform_test`, see "Platform layer"); `ctest` in a build
directory runs the parts that need no game data.

`-DTH16_THCRAP=ON` (the default when pkg-config finds jansson and libpng)
builds the thcrap support (`src/thcrap/`, see "thcrap"), with PNG decoding
in D3DX. Arch: `jansson libpng`; Debian: `libjansson-dev libpng-dev`.
There is rarely a 32-bit jansson, so the `-m32` builds usually have it off;
CMake says which at configure time ("thcrap support: on/off").

`-DTH16_NULL_RENDERER=ON` builds `src/d3d9_null.cpp` in place of every other
`src/d3d9_*.cpp` and `src/d3dx9_tex*.cpp`: a Direct3D 9 that succeeds at
everything and draws nothing (textures and surfaces are plain memory, so
locks and the text upload work; Present waits for the next 1/60 s when
the game asks for vsync). It is for running the game without a renderer.

The CMake build compiles every `src/*.cpp` (the glob is not recursive, so
`src/harness/`, the matching build's stand-in callers, stays out),
`src/stub/Opaque.cpp` by name (see "Game data in src/stub/") and
`port/src/*.cpp` (and `port/src/thcrap/*.cpp` with `TH16_THCRAP`), with
`-DTH16_PORT`, C++17 (gnu++17) and `-include port/include/port_prelude.h`.

Flags that matter for behaviour (CMakeLists.txt): `-fno-strict-aliasing`,
`-fwrapv`, `-fno-delete-null-pointer-checks`, `-fsigned-char`,
`-ffp-contract=off`, GCC's
`-fno-aggressive-loop-optimizations` (the game indexes past arrays, e.g.
`Scorefile::unlock_all` writes `clears[5]` of an `i32[5]`, MainMenu.cpp:791),
and for `-m32` `-msse2 -mfpmath=sse` (the original does its float math in SSE;
x87 would round differently).

Warnings left in a clean build (Clang 41/32, GCC 72/63 for 64/32-bit; no
errors): 31 `-Wdelete-non-virtual-dtor` (the game's
classes have virtual methods and non-virtual destructors, as in the
original), 24 GCC `-Wuninitialized` (constructors that clear one bit of an
uninitialised field: UpdateFunc.h:76, ZunTimer.h:35, written that way to
match), 9 `-Wint-to-pointer-cast` on 64-bit (`sum_rect_damage` of the
laser classes, never called, see below), 5 `-Wmismatched-new-delete`
(DSUtil.cpp's SAFE_DELETE on arrays, SupervisorSetup.cpp:170 `delete` of a
`new[]`), 2 GCC `-Waddress` (Stage.cpp:635, 656: comparing `&sprites[i]`
with NULL), 1 GCC `-Wformat-overflow` (GameWindow.cpp:415, a 4 KiB save
path into a 256-byte buffer, as in the original).

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
  - `d3dx9math.h` also has `PortAnonVec3`, a D3DXVECTOR3 without
    constructors for members of anonymous structs (see below).
  - `port_com.h`: HRESULT, GUID, IUnknown, `DEFINE_GUID` (declares only),
    CoInitialize/CoCreateInstance.
  - `port_thcrap.h`: what the game sources call (inside `#ifdef
    TH16_PORT`) where thcrap acts on th16.exe; inline no-ops without
    `TH16_THCRAP`. See "thcrap".
  - `windows.h`, `mmsystem.h`, `mmreg.h`, `winnls32.h`, `shlobj.h`,
    `process.h`, `direct.h`: kernel32/user32/gdi32/winmm/shell/CRT subsets.
  - `d3d9.h`, `d3dx9.h`, `d3dx9math.h`, `d3dx9tex.h`, `dinput.h`, `dsound.h`.
- `src/`: the platform layer, one file per subsystem. `PORT_UNIMPLEMENTED()`
  (`port_stub.h`) marks every stub and logs its name once to stderr;
  `grep -rn PORT_UNIMPLEMENTED port/src` lists what is left.
  - `port_platform.h`: what the platform layer offers the renderer and
    the sound code (the SDL window, its flags and size, the display,
    folders, Shift-JIS conversion, `port_log`). See "Platform layer".
  - `main.cpp`: arguments and folders, starts SDL, calls `WinMain`.
  - `port_vfs.cpp`, `port_vfs.h`: the Windows file namespace the game sees.
  - `port_kernel.h`: kernel objects behind HANDLEs and thread message
    queues (internal).
  - `crt_compat.cpp`: the MSVC CRT extensions; `_chdir`, `_mkdir`,
    `_getcwd` in the game's namespace.
  - `win32_kernel.cpp`: the handle table, files, find, errors, modules,
    environment.
  - `win32_thread.cpp`: critical sections (heap `std::recursive_mutex`, see
    below), time, threads (`CreateThread`, `_beginthread(ex)`), events,
    mutexes, waits, thread message queues.
  - `win32_user.cpp`: the window over SDL, messages, the resolution dialog,
    cursor, keyboard state, system metrics, message boxes.
  - `win32_misc.cpp`: winmm joystick (over `port_input.h`), COM and shell
    link (fail, which the game handles), shell folders, WINNLSEnableIME.
  - `port_input.h`, `input_sdl.cpp`: key states from SDL events (DIK_ and
    VK_ tables) and the first SDL game controller.
  - `dinput_sdl.cpp`: DirectInput 8 keyboard and joystick over
    `port_input.h`, the data formats and DIPROP ids.
  - `gdi_text.cpp`: the GDI subset of the text renderer over FreeType and
    fontconfig (DCs, DIB sections, fonts, `TextOutA`), and `GetDC`.
  - `port_text.cpp`: Shift-JIS conversion (iconv), MultiByteToWideChar,
    WideCharToMultiByte, `port_log`.
  - `d3d9_gl.cpp`: Direct3D 9 (IDirect3D9, the device, textures,
    surfaces, vertex buffers) on OpenGL 3.3 through SDL2, complete for what
    the game uses (see "Graphics"). `d3d9_gl.h` is its interface for the
    window code, `d3d9_gl_internal.h` what it shares with d3dx9_tex.cpp,
    `d3d9_gl_funcs.h` the GL functions it loads.
  - `d3d9_null.cpp`: the null Direct3D 9 (`TH16_NULL_RENDERER`).
  - `d3dx9_math.cpp`: complete (matrix, vector, projection functions).
  - `d3dx9_tex.cpp`: the D3DX texture helpers: complete except image file
    decoding, which the game data never needs (see "Graphics").
  - `dsound_sdl.cpp`, `dsound_sdl.h`: DirectSound 8 as a software mixer on
    one SDL audio device, complete for what the game uses (see "Audio").
  - `layout_checks.cpp`: compile-time layout checks that stay on in the
    64-bit build (`TH16_PORT_CHECK`, defined in port_prelude.h).
  - `thcrap/`: thcrap support (see "thcrap").
- `tests/`: the audio tests (see "Audio"), the platform test
  (`platform_test.cpp`), the thcrap test (`thcrap_test.cpp`) and
  `th16dat.py`, a Python copy of the game's th16.dat reader that lists and
  extracts files for tests.
- `tools/thcrap_strings.py`: makes the table of the game's strings at their
  original addresses (see "thcrap").

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
  `__attribute__((aligned(16)))`, and `HARNESS_CALLED`, `LTCG_FASTCALL`,
  `LTCG_VECTORCALL` as nothing.
- `src/types.h`: `iptr`/`uptr`, integers that hold pointers (`i32`/`u32`
  for MSVC, `intptr_t`/`uintptr_t` in the port).
- Inline assembly: every `__asm` block in the game goes through a macro in
  `src/ZunAsm.h`, and each macro has a `#ifdef TH16_PORT` branch that calls
  a `port_fpu.h` helper; the game sources have no per-site port code for
  asm. `ZUN_ASM_SINCOSMUL_XY`, `ZUN_ASM_SINCOSMUL` and
  `ZUN_ASM_SINCOSMUL_PTRS` (the fsincos multiply copies) call
  `port_sincosmul2`, `ZUN_ASM_SINCOS` (fld/fsincos/fstp pairs)
  `port_sincos`, `ZUN_ASM_SNAP_QUAD_TO_PIXEL_CENTERS` (AnmDraw.cpp's pixel
  snapping) `port_frndint_sub` per coordinate, and `ZUN_ASM_FINIT`
  `port_finit` (a no-op). On x86 and x86-64 the helpers run `fsincos`
  itself through GNU inline asm on `long double`, so results are
  bit-identical to the original when it runs them in 64-bit precision
  (after `finit`). Elsewhere (ARM macOS) they fall back to `sinl`/`cosl`,
  which can differ in the last bit. (ZunAsm.h notes that Direct3D puts the
  x87 unit in 24-bit precision at device creation; the game's `finit`s,
  among them those in the Rng float functions, put it back to 64 bits.)
- MSVC extensions in the code itself:
  - taking the address of a temporary vector (`&(a - b)`,
    `&Float3(x, y, z)`): `D3DXVECTOR2/3/4` and `D3DXMATRIX` have a member
    `operator&`, which is legal on temporaries;
  - binding `AnmId &` to a temporary (`get_vm_or_clear(find_child_id(...))`):
    an rvalue overload in AnmManager.h under `TH16_PORT`;
  - three `goto`s jumping past initialisations: BulletManager.cpp (the
    angle in the BULLET_EX_ACCEL case moved into its own block),
    PauseMenu.cpp (`choice` declared, then assigned) and GameThread.cpp
    (`zero` in `thread_start`, the same). All unguarded; MSVC output
    unchanged;
  - members with constructors in an anonymous struct (Bomb.h's
    `BombReimuAOrb`, whose `pos` and `start_pos` overlay its `PosVel`):
    GCC rejects them, so under `TH16_PORT` they are `PortAnonVec3`
    (d3dx9math.h), D3DXVECTOR3's layout without constructors, converting
    to and from D3DXVECTOR3 and with its arithmetic.
- Calling conventions are irrelevant in the port (every caller and callee is
  ours), so they expand to nothing, including in function pointer types.

## Things the platform layer has to provide faithfully

(What the game needs; "Platform layer" below says how the port does it.)

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
  compatibility. The CRT's sinf, cosf, tanf and atan2f (statically linked
  in th16.exe) widen to double, call the double function and round
  (0x405510, 0x4054f0, 0x43dc90, 0x4052a0); glibc's float functions can
  round the last bit differently. The replays tested stay in sync without
  emulating either difference (Testing, "Replay sync tests"), so the port
  does neither for now.
- Argument evaluation order. C++ leaves the order of a call's arguments
  unspecified; MSVC (and GCC on x86-64) evaluates them right to left,
  Clang left to right. Where two arguments draw from the replay RNGs, the
  order decides which value goes where: the season items an enemy drops
  (Enemy.cpp, both spawn_item calls) took the angle and speed the other way
  round in Clang builds, and replays recorded in the original desynced
  within seconds. Those calls and the one other such site (Item.cpp, an
  item's jitter offset) draw into locals in MSVC's order under
  `TH16_PORT`. A search for calls with two or more RNG draws or ECL stack
  pops among their arguments finds no other site; anything new of that
  shape needs the same treatment.
- `CRITICAL_SECTION` keeps its x86 layout (24 bytes, an array of them sits in
  `CriticalSections`). The implementation keeps a heap mutex pointer in
  `LockSemaphore`.
- Files: `CreateFileA` and friends get Windows paths with backslashes
  (`"replay\\th16_01.rpy"`, `save_dir` built from `APPDATA` +
  `\\ShanghaiAlice\\th16\\`) and names written for a case-insensitive file
  system. Map separators, look names up case-insensitively, and give
  `GetEnvironmentVariableA("APPDATA")` a directory that maps to the host's
  save folder.
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

## Platform layer

### Running

```
th16 [--game-dir DIR] [--save-dir DIR] [--thcrap DIR] [--thcrap-config NAME] [--no-thcrap] [DIR]
```

The game folder (th16.dat, thbgm.dat) is `DIR`/`--game-dir`, else
`$TH16_DATA_DIR`, else the current directory if it has th16.dat, else the
executable's directory. Nothing is ever written there. The save folder
(th16.cfg, scoreth16.dat, replay/, snapshot/, log.txt) is `--save-dir`, else
`$TH16_SAVE_DIR`, else SDL's preference path: `$XDG_DATA_HOME/th16-port`
(`~/.local/share/th16-port`), `~/Library/Application Support/th16-port`
on macOS; it is created if needed. `main()` starts SDL (events, video,
game controllers) before `WinMain` and quits it afterwards. Without a
display SDL video fails and the game runs windowless (with the null
renderer). The port logs to stderr as `[th16-port <seconds>] ...`; the
game's own log goes to log.txt in the save folder at exit, as on Windows.
The thcrap options are described under "thcrap".

Environment: `TH16_FONT_GOTHIC`, `TH16_FONT_MINCHO` (a font file or a
fontconfig pattern for the two faces), `TH16_NO_DIALOGS` (no message box
at exit; the text still goes to stderr), `TH16_DEBUG_EVENTS` (logs SDL
events and the DirectInput keyboard's held keys), `TH16_NULL_DUMP_TEXT=dir`
(null renderer only: writes each text image the game uploads, as PAM).

### Files (port_vfs.cpp)

The game sees a Windows file system: `GetModuleFileNameA` says
`C:\th16\th16.exe`, `GetEnvironmentVariableA("APPDATA")` (and
`SHGetFolderPathA`) `C:\AppData`, so its save directory is
`C:\AppData\ShanghaiAlice\th16\`. Both `C:\th16` and that directory (and
`C:\AppData`, `C:\AppData\ShanghaiAlice`) are the root of one namespace:
the save folder layered over the game folder. Lookups try the save folder,
then the game folder; files are created, written and deleted only in the
save folder (a game folder file opened for writing without truncation is
copied over first; a directory written into that only the game folder has
is created in the save folder with its spelling). So th16.cfg, the score
file and replays left in a game folder (for example by Wine) are read
until the game saves its own. Names match ignoring ASCII case, `\` and `/`
both separate, `.` and `..` resolve, non-ASCII names are Shift-JIS in the
game and UTF-8 on the host. Other drives and folders do not exist
(`ERROR_PATH_NOT_FOUND`). The current directory (`_chdir`,
`SetCurrentDirectoryA`) is a Windows path in this namespace, process-wide
as on Windows; since both of the game's directories are the same place,
the game's habit of switching between them around relative opens (and the
loading thread opening "thbgm.dat" meanwhile) is harmless.
`FindFirstFileA` lists both folders, sorted ignoring case as NTFS does,
without `.` and `..`.

### Kernel objects and threads (win32_kernel.cpp, win32_thread.cpp)

A HANDLE is an index into a table ((index + 1) * 4), never a pointer:
closing NULL, `INVALID_HANDLE_VALUE` or a closed handle fails with
`ERROR_INVALID_HANDLE` (CWaveFile and SoundManager close handles twice or
close NULL). Files are host file descriptors (64-bit offsets). Threads
(`CreateThread`, `_beginthreadex`, `_beginthread`, whose handle closes
itself at the end as the CRT's does) run on detached `std::thread`s;
thread handles signal when the routine returns and keep its exit code
(`GetExitCodeThread`). Events (auto and manual reset) and mutexes are
waitable; all waitable state changes under one lock and wakes one
condition variable (`port_wait` serves WaitForSingleObject,
WaitForMultipleObjects and MsgWaitForMultipleObjects). SetEvent only takes
that lock, so the audio mixer may call it from SDL's audio thread. Every
thread started this way, and the main thread, has a message queue from the
start: `PostThreadMessageA(WM_QUIT)` to the BGM thread works even before it
first waits, and `MsgWaitForMultipleObjects` returns `WAIT_OBJECT_0 + n`
while its queue has a message. The single-instance mutex ("Touhou 12 App")
also locks `th16-port.lock` in the save folder, so a second copy of the
port on the same save folder gets `ERROR_ALREADY_EXISTS` and the game's
"cannot start twice" error. Thread priorities are ignored;
TerminateThread and CREATE_SUSPENDED are not supported (the game uses
neither). Time: QueryPerformanceCounter/Frequency, timeGetTime and
GetTickCount all read SDL's performance counter, so the game's clocks agree
with SDL's. The port builds with `_FILE_OFFSET_BITS=64`.

### Window and messages (win32_user.cpp)

`CreateWindowExA` creates the SDL window (title converted from
Shift-JIS). The system metrics for frames and captions are 0, so the size
the game asks for is the client size: 640x480, 960x720 or 1280x960 from
th16.cfg's window size. `WS_POPUP` (the full screen sizes) means
`SDL_WINDOW_FULLSCREEN_DESKTOP`: the window covers the display and the
renderer scales the back buffer into it (`port_window_drawable_size`). The
game switches modes (Alt+Enter, maximising) through its device-reset path,
which calls `SetWindowLongA(GWL_STYLE)` and `SetWindowPos`; those apply the
mode. A saved window position is used when it is on a display, else the
window is centred. The minimise/restore pair the game sends right after
creating the window is reduced to raising it.

The main thread's queue holds the window messages; `PeekMessageA` and
`GetMessageA` move SDL's pending events into it when it is empty:
focus gained/lost become `WM_ACTIVATEAPP` (the game reads no input while
inactive) plus `WM_SETCURSOR`, closing the window (or SDL_QUIT: Ctrl+C,
SIGTERM) `WM_CLOSE` (the game quits through its own path, saving th16.cfg
and the score file), maximising `WM_SIZE(SIZE_MAXIMIZED)`, keys
`WM_KEYDOWN`/`WM_SYSKEYDOWN` (Alt combinations and F10 are system keys, so
Alt+Enter reaches the game's full screen switch), the left mouse button
`WM_LBUTTONDOWN`. `SetTimer` produces no messages (the window procedure
ignores WM_TIMER). Text input (IME) is off. `ShowCursor` keeps Windows'
display counter; the cursor is shown while it is >= 0 and `SetCursor`
has not set NULL.

The resolution dialog (resource 0xcb; shown when th16.cfg asks for it,
which a new configuration does, or Shift is held) has no resources here:
`CreateDialogParamA` sends `WM_INITDIALOG` (the game sets the check boxes
from the configuration) and then answers with the dialog's start button
(0xd0), so the game reads back its own choices and goes on. Message boxes
(the game's error log at exit) print to stderr and open an
`SDL_ShowSimpleMessageBox` unless there is no display (offscreen or dummy
video driver) or `TH16_NO_DIALOGS` is set. The screen saver is held off
while the game runs (its SystemParametersInfoA calls).

### Input (input_sdl.cpp, dinput_sdl.cpp)

Key states follow SDL key events (so events pushed with `SDL_PushEvent`
count). DirectInput (`DirectInput8Create` always succeeds) gives the
keyboard as 256 DIK_ states (scan codes, independent of the layout, as on
Windows; Japanese keys included) and the first SDL game controller as a
joystick: `EnumDevices(DI8DEVCLASS_GAMECTRL)` reports it, `EnumObjects`
lists its axes, POV and buttons, `SetProperty(DIPROP_RANGE)` (the game
sets -1000..1000) and `DIPROP_DEADZONE` work, `Poll` returns
`DIERR_INPUTLOST` and unacquires once the controller is unplugged (the game
re-acquires each frame). A controller SDL has a mapping for reports the
layout DirectInput gives an Xbox pad: buttons A, B, X, Y, LB, RB, Back,
Start, left stick, right stick; axes X/Y (left stick), Z (left trigger
minus right trigger), Rx/Ry (right stick); the d-pad as POV 0, and also
pushing X/Y to their ends, since the game reads only X and Y (so the
d-pad moves the player; on Windows an Xbox pad's d-pad does nothing in the
game). Other joysticks report SDL's raw axes, buttons and first hat (also
folded into X/Y). th16.cfg's button mapping (custom.exe) indexes these
buttons. `GetKeyboardState` (the game's fallback with DirectInput off in
the configuration) gives VK_ codes (letters and digits by layout, the
rest by position; VK_SHIFT/CONTROL/MENU when either side is held), and
`joyGetPosEx`/`joyGetDevCapsA` (its pad fallback) the same controller with
0..65535 axes. A controller connected later is found by the winmm path
(DirectInput enumerates once, at startup).

### Text (gdi_text.cpp, port_text.cpp)

TextHelper.cpp draws text with GDI into an A4R4G4B4 (or A8R8G8B8) DIB
section and uploads it with `D3DXLoadSurfaceFromMemory` (so the renderer
gets text as an ordinary A4R4G4B4 upload from memory; the game never calls
GetDC on a D3D surface). `CreateDIBSection` handles 16/24/32-bit BI_RGB and
BI_BITFIELDS DIBs, top-down or bottom-up; `TextOutA` writes whole pixels
through the colour masks, so every pixel text touches loses its alpha
bits, which the game's invert-alpha trick turns into opaque text. Glyphs
are antialiased (FreeType grayscale coverage in GDI's 17 levels blended
over the pixel, like GDI's standard smoothing) in the COLORREF text colour; `SetBkMode(OPAQUE)`
fills the cell. Strings are split into Shift-JIS characters (lead bytes
0x81-0x9f, 0xe0-0xfc) and converted with iconv (CP932).

Fonts come from fontconfig: the face named in the LOGFONT if the system
has that family (and it covers kana and kanji), else for MS Gothic Noto
Sans Mono CJK JP (or Source Han Code JP, Noto Sans CJK JP, Source Han Sans
JP, IPAGothic, any Japanese monospace or sans font), for MS Mincho Noto
Serif CJK JP (or Source Han Serif JP, IPAMincho, any Japanese serif or
sans font); `TH16_FONT_GOTHIC`/`TH16_FONT_MINCHO` override. Weights follow
lfWeight, with synthetic emboldening when the font has no bold. A
substitute is laid out with MS Gothic's metrics, which the game's text
positions were made for: CreateFontA's (positive) height is the em, the
ascent 220/256 of it, single-byte characters half an em wide and
double-byte ones an em, glyphs centred in their cells. A real font keeps
its own metrics (height scaled by usWinAscent + usWinDescent, as GDI
does). `EnumFontFamiliesExA` reports only faces the system really has, so
the game uses Meiryo (with its larger sizes) only if Meiryo is installed;
otherwise MS Gothic and MS Mincho, as on a Windows without Meiryo.

With a thcrap stack loaded (`port_gdi_set_utf8`), strings that are valid
UTF-8 are taken as UTF-8 (thcrap's translations are UTF-8), others as
Shift-JIS, as thcrap's win32_utf8 does; a real font (one fontconfig has,
such as thcrap's Touhou Biolinum, registered from the patch with
`port_gdi_add_font_file`) then keeps its own proportional advances, and a
font without Japanese gets the Japanese substitute for the characters it
lacks (GDI's font linking). `lfItalic` (synthetic oblique), `lfUnderline`
and `NONANTIALIASED_QUALITY` (monochrome glyphs) work; thcrap's layout
markup and font rules use them. `TextOutA` goes through thcrap's layout
first (`port/src/thcrap/text.cpp`), which calls `port_gdi_text_out_raw` for
each run. Without a stack nothing of this changes the text.

### For the renderer and the sound code (port_platform.h)

- `port_sdl_window(hwnd)`: the SDL window behind the game's HWND (NULL
  before the window exists or without a display). The window exists by
  the time the game calls `IDirect3D9::CreateDevice` (whose hFocusWindow
  is that HWND), and lives until after the device is released.
- The renderer's window functions (`d3d9_gl.h`, declared again in
  `port_platform.h`): `CreateWindowExA` calls `port_gl_prepare_window()`,
  creates the window with `port_gl_window_flags()` and calls
  `port_gl_attach_window(window)` (before the game creates its device);
  `DestroyWindow` calls `port_gl_detach_window(window)` first. If the
  window cannot be made with those flags it is made without them and not
  attached. `win32_user.cpp` has weak no-op defaults for builds without the
  OpenGL renderer.
- `port_window_drawable_size`: pixels to present into (full screen is
  desktop-sized; scale the back buffer, keeping 4:3).
- `port_display_refresh_rate` (60 if unknown) and `port_display_size`:
  what `GetDeviceCaps(VREFRESH)` and `GetSystemMetrics` report; use them in
  `GetAdapterDisplayMode` so the game's 60 Hz checks agree.
- `port_game_dir`, `port_save_dir`, `port_host_path(windows_path)`,
  `port_sjis_to_utf8`, `port_utf8_to_sjis`, `port_log`.
- SDL: video, events and game controllers are started by `main()`; audio is
  started by the mixer with `SDL_InitSubSystem` and stopped with
  `SDL_QuitSubSystem`. All window and event calls happen on the main
  thread (the game's), which is where the renderer runs too.

### Testing

`th16_platform_test` (ctest) checks the file namespace, kernel objects,
keyboard and controller input (an SDL virtual controller) and the text
renderer without game data. With game data, a null renderer build
(`-DTH16_NULL_RENDERER=ON`) runs headless with `SDL_VIDEODRIVER=offscreen
SDL_AUDIODRIVER=dummy`; under `xvfb-run` with `SDL_VIDEODRIVER=x11`,
`xdotool` can send keys (the title menu takes about 8 s to accept input;
Escape then Z quits the game, Up x4 then Z opens the music room, whose
track list and comments are GDI text: `TH16_NULL_DUMP_TEXT` shows them).
Checked this way: boot to the title menu (gamemode 4) with th16.dat read
through the namespace, the loading, sound and BGM threads, a new th16.cfg
and scoreth16.dat in the save folder, keyboard input through DirectInput,
quitting from the menu and on SIGINT/SIGTERM, screenshots (P/Home:
`_beginthread`, `_mkdir`, a 1280x960 BMP in snapshot/), Alt+Enter both
ways, Japanese text, and starting a game (Z through the menus) and letting
it run for 90 s without input. With the OpenGL renderer (normal build,
`TH16_GL_DUMP_DIR` for frames; offscreen on NVIDIA, Xvfb on Mesa
llvmpipe): the loading screen and title menu, the music room's text, and
stage 1 played holding Z (`xdotool keydown z`) through the midboss to the
boss dialogue, whose text shows in the speech bubbles. SDL's HIDAPI
controller probing (libusb) can take a second or more at startup, much
longer under gdb; `SDL_JOYSTICK_HIDAPI=0` skips it for tests.

A headless run with the OpenGL renderer and keys, as used for the checks
after merging main (no window on the desktop, no sound; the game data is
only read, saves go to the `--save-dir`), as a script run by
`xvfb-run -a -s "-screen 0 1280x1024x24" sh run.sh`:

```
export SDL_VIDEODRIVER=x11 SDL_AUDIODRIVER=dummy SDL_JOYSTICK_HIDAPI=0 TH16_NO_DIALOGS=1
export TH16_GL_DUMP_DIR=$PWD/build-port/run/frames TH16_GL_DUMP_EVERY=60 TH16_GL_EXIT_AFTER=4200
mkdir -p build-port/run/frames build-port/run/save
build-port/clang64/th16 --save-dir build-port/run/save \
    --game-dir "$HOME/Touhou Project/(TH16) Touhou Tenkuushou ~ Hidden Star in Four Seasons" &
sleep 14
for i in 1 2 3 4 5; do xdotool mousemove 200 200 keydown z sleep 0.15 keyup z; sleep 2; done
sleep 8; xdotool keydown z; wait
```

(Z five times: Game Start, difficulty, character, season, then into stage
1; then Z held. Without a window manager, keys go to the window under the
pointer, hence the `mousemove`.) Checked this way after the merge of main
72c0ffa (clang64, Mesa llvmpipe): the title menu, stage 1 with its
enemies, items, HUD and the BGM caption, enemy bullets (now from main's
`g_bullet_types`), grazing, the player dying (Normal starts with two spare
lives) and the continue menu once they run out. The straight-edged fog
area at the bottom of the playfield before the midboss (around stage
frame 2300) looks the same in the build from before the merge at the same
stage time.

#### Replay sync tests

`th16 --replay FILE` (port/src/replay_test.cpp, port_replay_test.h) plays a
replay recorded in the original game, fast-forwarded (the game's own replay
fast-forward, 8 ticks per frame), and checks it, then exits: 0 in sync, 1
desync, 2 the replay did not play to its end (a game over or the stage end
menu, which a clear replay never reaches). Replays store, for each stage,
the original's g_Globals (score, lives, bombs, power, graze, season
power, ...) and the replay RNG's seed when the stage began, and playback
restores them at every stage; the test compares the state the port reached
with them just before, and at the end the score with the recorded final
score. The player position and each stage's frame count are reported but
not checked: the recording keeps adding frames to a stage while the next
one loads, so they depend on the original's loading time (about 145
frames per stage, and 380 after a clear).

The replays in port/tests/replays are the owner's, recorded in TH16 1.00a:
two Extra clears with Reimu and an Easy all clear with
Cirno (six stages, five transitions). ctest runs them through
tests/run_replay_test.sh when `TH16_DATA_DIR` points at the game's data and
skips them otherwise; in a null renderer build they need no display:

```
cmake -S port -B build-port/null64 -G Ninja -DCMAKE_CXX_COMPILER=clang++ -DTH16_NULL_RENDERER=ON
cmake --build build-port/null64
(cd build-port/null64 && TH16_DATA_DIR=~/"Touhou Project/(TH16) ..." ctest -j3)
```

That takes about 80 s per Extra replay and 3.5 min for the Easy run. All
three play in sync with Clang and GCC builds (64-bit, checked on the merge
of main 67fb187).

For finding where a replay desyncs: `TH16_REPLAY_TEST_TRACE=N` logs the
state every N frames, and `TH16_REPLAY_TEST_DUMP=FILE` writes every
frame's g_Globals, replay RNG and player position as hex (with
`TH16_REPLAY_TEST_ITEMS=FIRST:LAST`, also every active item in those
frames). `scripts/replay_trace.py record` (main's tracer, docs/workflow.md,
"Replay check of the MSVC build") writes the same dump for the original:
it runs th16.exe under Wine (a scratch prefix given as WINEPREFIX), opens
the replay through the menus with xdotool under Xvfb, and reads the same
state from /proc/PID/mem every millisecond (g_Globals 0x4a5790,
g_replay_safe_rng 0x4a6d88, g_Player 0x4a6ef8 +0x61c, g_ReplayManager
0x4a6f08: stage_num +0x214, frame_current +0xd8 + 0x28 * stage; items from
g_ItemManager 0x4a6ddc + 0x14, 0xc78 bytes each). `replay_trace.py
compare` then prints the first frame where each field differs. With
ptrace_scope 1 the script has to be the game's ancestor: Wine's launcher
exits after starting th16.exe, so the script makes itself a child
subreaper (prctl PR_SET_CHILD_SUBREAPER) to inherit it. The two dumps are
a phase apart (the port's is taken after GameThread's priority 0xf tick,
so time_in_stage is one ahead); everything else matched frame for frame
once the evaluation order was fixed. The first difference before that was
the season items' launch angles at frame 250 of the Extra stage, then item
collection a few frames apart, and the first extra death at frame 6929.

#### Replay recording tests

The sync tests play replays the original recorded; the recording tests
check the other direction, that a replay the port records is the replay
the original would have recorded. `th16 --record-from SOURCE --record-to
NAME` (port/src/record_test.cpp, port_record_test.h) starts a normal game
(not a replay) from the title menu with SOURCE's character, season and
difficulty, as the menus would, and plays it with SOURCE's input: each
frame, ReplayManager::on_tick_record takes SOURCE's input for the stage
and frame it is about to record instead of the keyboard's. At the point
where playback sets them (ReplayManager::initialize) the replay RNG gets
SOURCE's seed and the game thread SOURCE's settings, so the game is the
one SOURCE recorded, and the recorder's own snapshot of each stage is
compared with SOURCE's as the stage begins. It runs with the replay
fast-forward (8 ticks per frame), which on_tick_fast_forward also applies
while recording in this test. When the cleared game goes on to the ending
(or after the extra stage to the score entry), the test saves the replay
as the replay save menu would after them (set_end_stage(1), then save
without an end marker, which is what the clear replays in
port/tests/replays have) and exits: 0 saved, 1 saved but a snapshot
differed, 2 a game over, the stage end menu or a return to the title.

tests/run_record_test.sh then compares the saved replay with SOURCE
(`port/tools/rpy_compare.py compare`, which decrypts and decompresses
both as read_replay_file does: the info, every stage's snapshot and every
frame's input) and plays it back with the sync test. Expected
differences, shown as notes: the spell cards' capture times (real time
from get_runtime, so about an eighth of the original's when
fast-forwarded; only how many cards ended has to match), the settings,
and what the previous game in the original's process left in g_Globals
(a fresh process, like the port's, has zeros): the first stage's
snapshot is taken (ReplayManager::initialize) before the stage starts,
and a new game does not reset the per-chapter values (chapter,
last_collect_pos, ...; the stage does), while graze_in_chapter and
enemies_spawned/destroyed_in_chapter are never reset at all, so the
previous game's counts stay in them and they differ by the same amount
at every stage of the replay (seen in replays recorded after another
game, for example a Royalflare Hard run whose player had grazed 33781
times before it). The live check during recording logs these as notes
too. ctest runs it for the Reimu Extra clear and the Cirno Easy clear
(record_*, skipped without `TH16_DATA_DIR`; about 3 and 7 minutes, and
all eight tests take 8.5 minutes with `ctest -j4`). `rpy_compare.py
show FILE` prints a replay's header, info and stage snapshots.

To check that the original accepts the port's replays, play one in
th16.exe with `scripts/replay_trace.py record` and compare its dump with the
port's (`TH16_REPLAY_TEST_DUMP` while `th16 --replay` plays the same
file). Several reference runs at once each need their own WINEPREFIX and
`--display N` (xvfb-run -a races when two start together).

Checked (clang64 null renderer, on the merge of main 6d99888): the two
ctest replays and seven 1cc replays from the Royalflare archive (Extra
clears with Aya, Cirno and Marisa; Hard Reimu Autumn, Lunatic Aya Summer,
Lunatic Marisa Autumn, Normal Cirno Autumn) record to files that match
their sources (every stage's RNG seed, g_Globals, player position and
focus, every frame's input, the final score, even each stage's frame
count) apart from the expected differences above, and play back in sync.
The original th16.exe played the port's recordings of the Reimu Extra
clear and the six-stage Cirno Easy clear to their ends, and its state
matched the port's frame for frame (36013 and 93548 frames sampled; the
only differences are the replay RNG's step counter on the last frame of
two stages, where the port's dump is a tick later and the next stage has
already reset it).

ReplayManager::save writes RpyInfo, RpyGamestate and the chunks' input
as they are in memory, so record_test.cpp also checks their layout
(static_asserts: the 64-bit build keeps the original's 0xa0, 0x294 and
6-byte records; RpyInfo is pack(4) for its 8-byte timestamp at 0xc).

### Not done

- Controller hot-plugging through DirectInput (only the winmm fallback
  sees a controller connected after startup), force feedback (unused).
- Exclusive full screen modes (full screen is desktop-sized); high-DPI
  windows (a renderer can ask for `SDL_WINDOW_ALLOW_HIGHDPI`).
- Named events and mutexes are process-local (only the single-instance
  check crosses processes, through the lock file).
- Not built or run on macOS yet (iconv, fontconfig and SDL paths are
  handled; the fonts may need `TH16_FONT_*`).

## thcrap

thcrap (Touhou Community Reliant Automatic Patcher) patches the Windows
th16.exe in memory: it hooks the Win32 file and text functions and puts
breakpoints and binary hacks at fixed addresses of v1.00a. None of that can
work on the port, so `port/src/thcrap/` reads the user's thcrap folder and
patch stack itself and does, at the same places in the decompiled game,
what thcrap's TH16 support does. The user's English patch (thpatch's
lang_en over nmlgc's base_tsa, script_latin and western_name_order) then
works: dialogue, endings, menus and other images, spell card names, the
music room, the help manual, the hardcoded strings and the fonts.

### Using it

The port looks for a thcrap folder at `--thcrap DIR`, else
`$TH16_THCRAP_DIR`, else `$XDG_DATA_HOME/thcrap` or
`~/.local/share/thcrap` if it exists, and takes its run configuration
from `--thcrap-config NAME` (a path, or a name in the folder's `config/`
with or without `.js`), else `$TH16_THCRAP_CONFIG`, else the newest
`config/*.js` that has a `"patches"` list (thcrap's own `config.js` has
none). `--no-thcrap` or `TH16_THCRAP=0` turns it off. The folder is only
read: a thcrap installation made on Windows (or under Wine) works as it is,
with its patches already downloaded by thcrap (the port does not update
them). The log says what was loaded:

```
[th16-port] thcrap: run configuration .../config/en.js: base_tsa, base_tasofro, script_latin, ...
[th16-port] thcrap: 49 hardcoded strings can be translated
[th16-port] thcrap: font .../script_latin/THBiolinum.otf
[th16-port] thcrap: st01a.msg: patched
```

A stack whose patches have nothing for th16 (no `th16.js`, `th16/` or
`th16.v1.00a.js`) leaves the game unpatched.

### The patch stack (stack.cpp)

As in thcrap (stack.cpp, patchfile.cpp, init.cpp): each run configuration
entry's `archive` (relative to the thcrap folder) with its `patch.js`
(id, `ignore` wildcards, `fonts`, `supported_games`; patches naming other
games only are dropped). Files resolve through thcrap's chains: `fn` and
`fn` with `.v1.00a` before the first dot of the base name, under `th16/`
for game files. A replacement file is the last patch's, build-specific
first; JSON files merge over the whole stack in order (objects
recursively). The game configuration is every patch's `global.js`,
`th16.js` and `th16.v1.00a.js` (and the run configuration's `config`)
merged in stack order, under the run configuration's own keys: that is
where the breakpoints and binary hacks below are switched on (one with an
`addr` and not `"ignore": true`), and where `font`, `fontrules` and
`tsa_font_block` come from. Patch files are JSON5 (json5.cpp turns them
into JSON for jansson; base_tsa's th16.v1.00a.js has trailing commas).
File names are matched ignoring case, since patches are made on Windows.

### Hooks in the game

`port/include/port_thcrap.h` is what the game calls, always inside
`#ifdef TH16_PORT` (the MSVC build never sees it), and each call returns
its input unchanged without a stack. The thcrap hackpoints of base_tsa's
`th16.js`/`th16.v1.00a.js` and script_latin's `th16.v1.00a.js`, and their
places in the port:

| thcrap (address) | game function | port |
|---|---|---|
| file_size, file_load (0x4024cc, 0x402504), file_loaded (0x45724b) | `file_read_all`, `Arcfile::read_file` | `port_thcrap_file_replacement` (a patch file replaces an archive file or adds one), `port_thcrap_patch_file` (format patchers) in `file_read_all`'s archive branch |
| sprintf_* hacks (strings_sprintf) | `AnmManager::draw_text`, `draw_text_right`, `draw_text_centered`, `AsciiInf::create_stringf`, `PauseMenu::tick_open`, `TitleInf::do_replay_save`, `load_replay_list` | `port_thcrap_vsnprintf`/`snprintf` (format and `%s` arguments translated; draw_text's buffers are 0x400 bytes in the port) |
| spell_id (0x4217c9) | `EnemyData::ecl_run_over_300`, spell instructions | `port_thcrap_spell_id` |
| spell_id#real, spell_name (0x417f4a, 0x4180d6) | `Spellcard::start` | `port_thcrap_spell_name` (the shown name only; the score file keeps the original) |
| spell_id#result, spell_name#result (0x452d8d, 0x452ed5) | `TitleInf::draw_spell_card_page` | `port_thcrap_spell_name_ranked` (rank: the card's difficulty) |
| spell_name#practice (0x456622) | `TitleInf::load_spell_list` | `port_thcrap_spell_name_ranked` (rank: the row) |
| music_title_prepare, music_title (0x454aef) | `TitleInf::do_music_room` | `port_thcrap_music_title` |
| music_cmt#line, music_cmt (0x454d46, 0x454e0f) | its comment lines | `port_thcrap_music_comment` |
| ruby_offset (0x42a53a, 0x42a736) | `GuiMsgVm::run` | `port_thcrap_ruby_offset` |
| th15_textbox_size (0x42a5d0, 0x42a7c6) | `GuiMsgVm::run` | `port_thcrap_textbox_width` |
| spell_align (0x46db40) | `AnmManager::draw_text_right` | `port_thcrap_text_right_x` |
| result_spell_align (0x46dd11) | `AnmManager::draw_text_centered` | `port_thcrap_text_centered_x` |
| meiryo_disable (0x458e22, script_latin) | `create_fonts` | `port_thcrap_binhack("meiryo_disable")` skips the Meiryo search |
| score_force_visual_update (0x42d7b3) | `GameThread::on_tick_body` | calls `Gui::update_score` while the game-over menu is open |
| fix_satono_1/2 (0x421596) | `EnemyData::ecl_run_over_300`, setNext in spell practice | the second boss ends its card in `BossDeadB` |
| steamstub/steamdrm cracks | (Steam's DRM) | not needed |

The last three are switched by the stack like the rest: base_tsa enables
them (so they are on with the English stack); a stack without them, or
with `"ignore": true` in a run configuration's binhacks, leaves the game
as it is. In the port's own code: `CreateWindowExA` takes thcrap's window
title (`tsa_CreateWindowExA`: the game title and build from th16.js),
`CreateFontA` and `CreateFontIndirectA` apply `font` and `fontrules`
(`textdisp.cpp`), `TextOutA` the layout (below).

### Files and formats (stack.cpp, msg.cpp, anm.cpp)

thcrap_tsa's patch hooks for TH16: `s*.msg` (dialogue, `MSG_TH14`),
`e*.msg` (endings, `END_TH10`) and `*.anm`, each with `<file>.jdiff`.
The .msg patcher (th06_msg.cpp) replaces the lines of each text box from
the jdiff (`"<entry>": {"<time>_<index>": {"lines": [...]}}`), inserts
extra lines, drops missing ones, re-encrypts them as the game expects, and
moves speech bubbles that would leave the screen (by the lines' widths in
the dialogue font, `ruby_offset`'s `font_dialog`). The file grows by at
most the jdiff's size, as in thcrap. The .anm patcher (anm.cpp) draws
each PNG of the stack over the texture of the entry it is named after
(`th16/title/title_logo.png`, or thtk's `title_logo@title@3.png`), sprite
by sprite with thcrap's blitting rules (blend over opaque pixels, else
overwrite; empty replacement areas are skipped), in the texture's format
(A8R8G8B8, R5G6B5, A4R4G4B4, A8), and applies the jdiff's header changes
(`sprites` rectangles, `entries` names and blitting modes, script
instruction deletions and changes; parameter changes take plain hex
bytes). Any other file a patch has (help_01.png, for example) replaces the
archive's.

### Strings (strings.cpp, tools/thcrap_strings.py)

thcrap translates hardcoded strings by address: `stringlocs.js` maps
addresses in the original th16.exe to ids (`"Rx9290c":
"th10_ascii_stage_1"`), `stringdefs.js` ids to translations, and
`strings_lookup` compares the pointer the game passes. The port's strings
are its own literals, so it looks them up by content:
`th16_strings.inc` lists every string literal of `src/` with its address
in the original executable (the decompilation matches, so each literal is
there byte for byte), and a string translates when its text is the text
at a stringlocs address. The build makes the table from the sources and
`orig/th16.exe` (CMake cache variable `TH16_ORIG_EXE`) when both are there
(`tools/thcrap_strings.py`, about 0.2 s), else it uses the copy in
`src/thcrap/th16_strings.inc`, written by the same script. All 49
addresses of base_tsa's TH16 stringlocs are in it. Where thcrap looks
strings up, so does the port: the sprintf hacks (the format, and every
`%s` argument, as `strings_va_lookup`), `TextOutA` (layout.cpp looks up
every string it draws), `CreateFontA` (face names) and the window title.
The translating printf takes MSVC's `%ld` as a 32-bit int and cuts a
translation that does not fit at a whole UTF-8 character.

### Text (text.cpp)

thcrap_tsa's layout.cpp: a string is split into runs and commands
`<cmds$text$width>`: `r`/`c`/`l` align the text in a tab (the width of the
third parameter, the whole bitmap when it is empty, or a tab stop `t`
defined earlier), `s` skips it, `b`/`i`/`u` draw it bold, italic or
underlined. TL notes (after U+0014 or U+0012) are cut off, not shown. Text
widths in the game's fonts (thcrap's `GetTextExtentForFontID`, through
`tsa_font_block`: the game's `g_text_font_0` to `g_text_font_7`) give the
speech bubble's width (`th15_textbox_size`: half the width minus 28, at
least 0, where the game counts bytes), the right alignment of spell names
(`spell_align`) and the ruby offset: a ruby line
`"|\tbefore\t,\tbase\t,ruby"` gets its offset from the widths of the text
before the annotated part and of the annotated part in the dialogue font
and of the ruby in its font (+4 for TH16's sprite shift), passed to
`TextOutA` through thcrap's dummy x (32767, doubled by draw_text), and the
string pointer moves so that the game's own two `strchr(',')` calls find
the ruby text. Fonts: the run configuration's `font` replaces every face
the game asks for (script_latin: Touhou Biolinum, from its own
THBiolinum.otf, which the port registers with fontconfig like every
patch's `fonts`), and `fontrules` change matching LOGFONTs
(script_latin: the 15-pixel bold fonts become 21-pixel, weight 100,
non-antialiased). See "Text" under "Platform layer" for UTF-8 and the
metrics.

### Tests

`th16_thcrap_test` (ctest, no game data) writes a small thcrap folder and
checks JSON5, the stack and game configuration, the string table and the
translating printf, spells, the music room, file replacement, the .msg
patcher, layout markup, ruby offsets, bubble widths and the window title,
with GDI and the game's fonts as stand-ins (10 pixels per byte).

With the owner's stack (en.js: base_tsa, base_tasofro, script_latin,
western_name_order, lang_en, and three local patches for other games),
checked headless with the OpenGL renderer under Xvfb (clang64): the title
screen (English logo), Player Data (spell card list with the translated
format, layout-aligned columns and ASCII digits), the replay list (season
names translated in the ASCII font: "Border"), the music room (themes.js
titles, musiccmt.js comments, the centred spoiler warning), the help
manual (lang_en's help_01.png, through the new PNG decoding), stage 1 on
Easy to the boss dialogue (English lines, two-line boxes sized to the
text, `<i$...>` in italics, Eternity Larva's English name card), and an
Extra stage replay of the owner's: the midboss dialogue, Mai and Satono's
name card and the spell card name 'Drum Dance "Powerful Cheers"'
right-aligned. To reach stage 1's boss headless, the test script holds Z,
sweeps left and right, bombs from 75 s on and presses Z again every few
seconds (which takes the continues), then taps Z every 2.5 s through the
dialogue (holding it would skip the boxes).

### Not done

- Ruby (furigana) lines are only checked by the unit test: they occur from
  stage 2 on, which the test runs do not reach.
- The endings (e01.msg to e08.msg with END_TH10, ending images) and the
  result screen's and spell practice's names with real records are not
  checked on screen (the code is the in-game spell lookup's).
- TL notes are dropped rather than shown (th16's lang_en has none).
- thcrap features TH16's stack does not use: BGM modding (`*.pos`,
  `*bgm*.fmt`), spell comments, thcrap's full binary hack code syntax in
  ANM script changes, patch updates, `dat_dump`/`patched_files_dump`.

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
`-fsanitize=thread`). The game uses the mixer through the platform layer;
the headless runs use SDL's dummy driver, where the BGM thread takes its
commands and stages start.

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
  (`tests/th16dat.py` to extract, then read the entry headers). The help
  manual's pages, though, are PNG files in th16.dat (help_01.png to
  help_09.png, loaded with `file_read_all` and
  `AnmManager::reload_texture`): with libpng (`TH16_PNG`, part of the
  thcrap build) those three functions decode PNG, otherwise they log the
  file's first bytes and fail (the manual's pages stay empty). The rest of D3DX is complete: format conversion between all
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

Run the normal build headless as in "Platform layer", Testing (Xvfb with
Mesa, or SDL's offscreen driver on NVIDIA through EGL; for Mesa through
EGL point `__EGL_VENDOR_LIBRARY_FILENAMES` at glvnd's `50_mesa.json`).
Renderer environment variables (any build): `TH16_GL_DUMP_DIR` (write
the back buffer as `frame_NNNNNN.png` there), `TH16_GL_DUMP_EVERY` (every
N presents, default 60), `TH16_GL_DUMP_FROM` (first frame to dump),
`TH16_GL_EXIT_AFTER` (exit after N presents), `TH16_GL_TRACE_FRAME` (log
the render target changes, clears and draws with their state for that
frame), `TH16_GL_PACE=0` (no 60 Hz timer: run as fast as possible),
`TH16_GL_VSYNC=0`.

Checked (clang 32- and 64-bit, NVIDIA through EGL and Mesa llvmpipe), at
640x480 and 1280x960: the loading screen, the title screen, difficulty,
character and season selection, stage 1 (3D forest with fog, enemies,
items, HUD, the spring season release, the player's invincibility blink),
the pause menu (its blurred copy of the playfield goes through
D3DXLoadSurfaceFromSurface from a render target), the in-game screenshot
(P: back buffer LockRect, written as a BMP by the game's thread), GDI text
(menus, the music room, dialogue) and BGM through dsound_sdl.cpp on SDL's
dummy audio driver. (Before the platform layer existed this was checked
with a test-only shim, port/tests/platform_shim.cpp, since removed.)

## Game data in src/stub/

`src/harness/` holds no game code: its stand-in callers only shape MSVC's
output, and the port leaves it out. `src/stub/` is one file, `Opaque.cpp`,
which the matching build compiles without /GL so that LTCG cannot see into
it: the game globals `g_zero_vec2` and `g_stage_table` (with the
original's contents) and two empty sink functions the harness calls. The
port compiles it by name (port/CMakeLists.txt); the sinks are dead code
there. Every other data table (the ANM callback tables, `g_effect_table`,
`g_spell_difficulty`, `g_bullet_types`, `g_pad_mapping`, `g_game_speed`,
`g_Globals`' starting difficulty) is defined with its contents in its
module in `src/`, so the port has no copies of game data of its own (it
used to, in `port/src/game_tables.cpp`, while main had them zero-filled).

## Keeping the matching build

- Edits to game files are either inside `#ifdef TH16_PORT` or produce the
  same code for MSVC (types that are typedefs of the same type, sizes
  written as `sizeof` of what they measure, scoping, a declaration split
  from its assignment). After each change:
  `.venv/bin/python scripts/build.py`, `scripts/check_unchanged.py` (all
  1214 functions unchanged) and `scripts/quickdiff.py | grep -c MATCH`,
  which must equal main's count (864 for both after merging main 550e004,
  with identical quickdiff output). `check_unchanged` always reports
  `.rdata` as different: the linker writes a new timestamp and PDB id each
  time, even for identical builds. Build the baseline (main's tree) in a
  directory whose path has the same length as the one being checked: the
  PDB path is in `.rdata`, a longer one moves the data after it, and the
  five atexit thunks at the end of `.text` (`??__Fg_Supervisor` and
  others) then compare differently in quickdiff although nothing changed.
  (After the merges, main's tree and this branch were built as
  `build/main-tree` and `build/merge-tree` of the worktree.)
- Edited game files (expect merge conflicts with main's renames on these
  lines): decomp.h, types.h, ZunAsm.h, AnmManager.h, Bomb.h, Scorefile.h,
  SoundManager.h, Player.h, Player.cpp, PlayerShot.cpp, AnmDraw.cpp,
  AnmManagerVms.cpp, BombMain.cpp, BulletManager.cpp, GameThread.cpp,
  PauseMenu.cpp, Stage.cpp; for thcrap (all inside `#ifdef TH16_PORT`, see
  "thcrap"): FileSystem.cpp, AnmText.cpp, AsciiManager.cpp, Gui.cpp,
  Spellcard.cpp, EnemyEcl.cpp, MainMenuStates.cpp, PauseMenu.cpp,
  TextHelper.cpp, GameThread.cpp.
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
  the sht hit callbacks' `enemy_pos` and `enemy_size` (the `ShtHitFunc` and
  `DamageSourceHitFunc` typedefs, Player.h:382-385, PlayerShot.cpp,
  Player.cpp:965 call; sht_on_hit_laser casts them back to `Float3 *`),
  `AnmManager::last_texture_matrix_sprite` (a sprite pointer used as a
  cache key, AnmManager.h:423, AnmDraw.cpp:599-602), the
  `BombInf::compute_damage` call (Player.cpp:897), the null `AnmId`
  returns in AnmManagerVms.cpp:970,1017.
- `Supervisor`'s first field holds the HINSTANCE; it was `u8[4]`, which
  overwrote `d3d` on 64-bit and crashed at shutdown. Main now declares it
  as `HINSTANCE instance`.
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
- `CSound::m_desc` (SoundManager.h:221) was `u8[0x24]`; DSUtil.cpp:137,212
  copy a whole `DSBUFFERDESC` (0x28 bytes on 64-bit) into it, clobbering
  `m_manager`. Now `u8[0x20 + sizeof(void *)]`.
- `BgmStream` (SoundManager.h:118), a hand-laid-out view of
  `CStreamingSound`: packed to 4 like `CSound` and its gap before
  `refilling` widened on 64-bit.
- Reimu's bomb orbs (BombMain.cpp:321) were allocated as 0x6c0 bytes; they
  hold an `EnemyInf *` each and need 0x700 on 64-bit. Now (in main too)
  `sizeof(BombReimuAOrbs)`; BombMain.cpp:26 checks the 32-bit size.

### Open

- `sum_rect_damage(i32 a, i32 b, i32 c, i32 d, i32 e, i32 f)` of the
  laser classes (ExpHP: method_1c; Laser.h:96,204,276,398,463;
  Laser.cpp:1430-1435, 1517-1522, 1591-1596; LaserBeam.cpp:22) takes
  pointers in a, b and f and casts them back (the 9
  `-Wint-to-pointer-cast` warnings). Nothing calls it; if something does,
  make those three `iptr`.
- Function pointer types that do not match the function (also in the
  original, harmless on x86-64 and ARM64): `SoundManager::thread_init` and
  `thread_load_sound_files` return void but are started as
  `LPTHREAD_START_ROUTINE` (WinMain.cpp:402, Supervisor.cpp:1002; only the
  exit code is garbage). (The effect table's and the serialize table's
  mismatches are gone: main declares those callbacks with the table types.)
- `#pragma pack(4)` structs hold 8-byte pointers at 4-aligned offsets
  (Supervisor, GameWindow, CSound, PauseMenu, Spellcard, Scorefile,
  RpyInfo): fine for plain loads and stores on x86-64 and ARM64, wrong for
  atomics.
- AnmVm snapshots (AnmManagerVms.cpp:788-886, and
  `anm_gather_effect_on_serialize` in AnmVmCallbacks.cpp) copy
  pointer-holding `AnmVm` records by `sizeof`. Consistent within one
  build; they stay in memory (not in replays), so nothing to do.

### Not 64-bit, but noticed

- Fog.cpp:33-34 allocate one byte too few
  (`sizeof(AnmVm *) * FOG_STRIP_COUNT - 1`, the same for the ids): the
  original's bug, in both builds.
- MainMenu.cpp:791 (`Scorefile::unlock_all`) writes `clears[5]` of an
  `i32[5]` (into the next field, as in the original); GCC needs
  `-fno-aggressive-loop-optimizations`.
- GameWindow.cpp:415 formats a 4 KiB `save_dir` into a 256-byte path
  buffer. The game only sees the short virtual save path
  (`C:\AppData\ShanghaiAlice\th16\`, see "Files"), whatever the host
  folder is, so it cannot overflow.
- `delete` on `new[]` memory (DSUtil.cpp SAFE_DELETE, SupervisorSetup.cpp:170).
