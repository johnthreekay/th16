# Portable build (Linux/macOS, GCC/Clang)

The game sources in `src/` compile and link with GCC and Clang, 32-bit
(`-m32`) and 64-bit, against stand-in Windows/DirectX headers. The platform
layer is implemented except for the renderer: files, threads, the window
and its messages, keyboard and controller input, GDI text (see "Platform
layer") and audio (DirectSound over SDL2, see "Audio"). Direct3D is still
the stub whose CreateDevice fails, so the default build stops at the
device error (a message box); with `-DTH16_NULL_RENDERER=ON` (a Direct3D
that draws nothing) the game boots with real files, threads, input and
text to its title menu and plays from there, in all four builds. The
matching MSVC build is unaffected (see "Keeping the matching build").

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

`-DTH16_NULL_RENDERER=ON` builds `src/d3d9_null.cpp` in place of every other
`src/d3d9_*.cpp` and `src/d3dx9_tex*.cpp`: a Direct3D 9 that succeeds at
everything and draws nothing (textures and surfaces are plain memory, so
locks and the text upload work; Present waits for the next 1/60 s when
the game asks for vsync). It is for running the game without a renderer.

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
  - `d3d9_stubs.cpp`: stub classes for IDirect3D9, IDirect3DDevice9,
    IDirect3DTexture9, IDirect3DSurface9, IDirect3DVertexBuffer9 with every
    method. Direct3DCreate9 returns the stub IDirect3D9, whose CreateDevice
    fails.
  - `d3d9_null.cpp`: the null Direct3D 9 (`TH16_NULL_RENDERER`).
  - `d3dx9_math.cpp`: complete (matrix, vector, projection functions).
  - `d3dx9_tex.cpp`: texture creation and surface loading: stubs.
  - `dsound_sdl.cpp`, `dsound_sdl.h`: DirectSound 8 as a software mixer on
    one SDL audio device, complete for what the game uses (see "Audio").
  - `game_tables.cpp`: the game globals the matching build defines in
    `src/stub/` (see "Game data in src/stub/").
  - `layout_checks.cpp`: compile-time layout checks that stay on in the
    64-bit build (`TH16_PORT_CHECK`, defined in port_prelude.h).
- `tests/`: the audio tests (see "Audio"), the platform test
  (`platform_test.cpp`) and `th16dat.py`, a Python copy of the game's
  th16.dat reader that lists and extracts files for tests.

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
  compatibility.
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
th16 [--game-dir DIR] [--save-dir DIR] [DIR]
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
neither). Time: one steady clock for QueryPerformanceCounter (1 ns units),
timeGetTime and GetTickCount.

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
are antialiased (FreeType grayscale coverage blended over the pixel, like
GDI's standard smoothing) in the COLORREF text colour; `SetBkMode(OPAQUE)`
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

### For the renderer and the sound code (port_platform.h)

- `port_sdl_window(hwnd)`: the SDL window behind the game's HWND (NULL
  before the window exists or without a display). The window exists by
  the time the game calls `IDirect3D9::CreateDevice` (whose hFocusWindow
  is that HWND), and lives until after the device is released.
- `port_set_window_flags(flags)`: the SDL_WINDOW_* flags the window gets
  (default `SDL_WINDOW_OPENGL`). WinMain calls `Direct3DCreate9` before
  it creates the window (also after a restart), so the renderer calls this
  there, with any `SDL_GL_*` attributes that have to precede the window.
  If creation fails with the flags, the window is made without them.
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
ways, Japanese text. SDL's HIDAPI controller probing (libusb) can take a
second or more at startup, much longer under gdb; `SDL_JOYSTICK_HIDAPI=0`
skips it for tests.

### Not done

- The renderer (Direct3D over OpenGL; `d3d9_stubs.cpp`, `d3dx9_tex.cpp`).
- Controller hot-plugging through DirectInput (only the winmm fallback
  sees a controller connected after startup), force feedback (unused).
- Exclusive full screen modes (full screen is desktop-sized); high-DPI
  windows (a renderer can ask for `SDL_WINDOW_ALLOW_HIGHDPI`).
- Named events and mutexes are process-local (only the single-instance
  check crosses processes, through the lock file).
- Not built or run on macOS yet (iconv, fontconfig and SDL paths are
  handled; the fonts may need `TH16_FONT_*`).

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
  buffer. The game only sees the short virtual save path
  (`C:\AppData\ShanghaiAlice\th16\`, see "Files"), whatever the host
  folder is, so it cannot overflow.
- `delete` on `new[]` memory (DSUtil.cpp SAFE_DELETE, SupervisorSetup.cpp:166).
