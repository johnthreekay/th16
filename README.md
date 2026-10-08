# th16-decomp

A matching decompilation of 東方天空璋 ～ Hidden Star in Four Seasons (TH16) v1.00a:
C++ source written to compile, with ZUN's own compiler and flags, back to the
machine code of `th16.exe`, checked function by function. Every function is
decompiled; 1007 of 1207 compile to identical bytes so far.

This repository does not include the game's executable or its data files
(`th16.dat`, music, the manual); you need your own copy of the game. The
source does reproduce what matching requires: the initial contents of the
executable's data tables and its string literals. `th16.exe` must have
SHA-256
`c11776019f083978e66027e7394dafb1fb9543afca986f28049a49417e341929`.

## Status

- 1007 of the 1207 functions reccmp compares (83%) are byte-identical, and 29
  more differ only in instruction scheduling; reccmp skips the 7 annotated
  CRT library functions. All 26 vtables match.
- The other 171 compile to functionally equivalent code. Each carries a
  one-line `// TODO:` saying what still differs, mostly register allocation
  and stack frame alignment. Those are whole-program decisions, so fixing
  one function can unmatch another, and some may never match;
  [docs/findings.md](docs/findings.md) has the patterns behind them.
- reccmp's summary line, "1222 / 1222 implemented, 96.80% accuracy", is the
  average instruction similarity per entry, not a share of matching
  functions. It also counts 15 SIMD constants, which all match (96.76% over
  the functions alone).
- Every annotated global holds the original's data (`scripts/check_data.py`:
  201 of 202 match; `g_Supervisor` differs only in defaults its initializer
  clears at startup), and none is a field of another annotated object.
- The source is readable: script opcodes (ECL, ANM, MSG, STD), game modes,
  sound effects, flags and file formats are named enums; structs and
  functions carry doc comments; ZUN's few inline assembly helpers sit behind
  documented macros in `src/ZunAsm.h`. `scripts/check_unchanged.py`
  confirmed that the readability pass changed no function's generated code.
- A portable build that runs on Linux (SDL2 and OpenGL, built with clang or
  gcc, 32- or 64-bit) is on this branch; see "Portable build" below. It
  needs your own game data files, and can apply a thcrap patch stack such
  as the English translation.

## Portable build

The same game sources also build with Clang or GCC for Linux (macOS is
untested, see below), 64-bit or 32-bit, with a platform layer in `port/`
in place of Windows and DirectX: the window, input and audio over SDL2,
Direct3D 9 over OpenGL 3.3, GDI text over FreeType and fontconfig. It
can also apply a thcrap patch stack, such as the English translation, see
"thcrap patches" below. The MSVC matching build does not see any of it
(`#ifdef TH16_PORT`). Details, design and test notes:
[`port/NOTES.md`](port/NOTES.md).

### Dependencies

- CMake 3.16 or later, Ninja (or make), Clang or GCC with C++17.
- SDL2 (or sdl2-compat), FreeType, fontconfig and pkg-config. On Arch:
  `sdl2-compat freetype2 fontconfig pkgconf`; on Debian or Ubuntu:
  `libsdl2-dev libfreetype-dev libfontconfig-dev pkg-config`.
- An OpenGL 3.3 driver (Mesa's software llvmpipe works too).
- A Japanese font for the game's text: Noto Sans Mono CJK JP and Noto Serif
  CJK JP are preferred (Arch `noto-fonts-cjk`, Debian `fonts-noto-cjk`);
  `TH16_FONT_GOTHIC` and `TH16_FONT_MINCHO` pick others.
- For 32-bit builds (`-DTH16_M32=ON`): the 32-bit multilib (gcc-multilib,
  lib32 glibc and libstdc++) and 32-bit SDL2, FreeType and fontconfig
  (Arch `lib32-sdl2-compat lib32-freetype2 lib32-fontconfig`).
- Optional, for thcrap patches: jansson and libpng (Arch `jansson libpng`,
  Debian `libjansson-dev libpng-dev`). The CMake option `TH16_THCRAP` is on
  when pkg-config finds both, and configuring prints "thcrap support:
  on/off". There is rarely a 32-bit jansson, so 32-bit builds usually
  have it off.

### Building

```
cmake -S port -B build-port/clang64 -G Ninja -DCMAKE_CXX_COMPILER=clang++
cmake --build build-port/clang64
(cd build-port/clang64 && ctest)
```

The other configurations are built the same way:

```
cmake -S port -B build-port/gcc64   -G Ninja -DCMAKE_CXX_COMPILER=g++
cmake -S port -B build-port/clang32 -G Ninja -DCMAKE_CXX_COMPILER=clang++ -DTH16_M32=ON
cmake -S port -B build-port/gcc32   -G Ninja -DCMAKE_CXX_COMPILER=g++ -DTH16_M32=ON
```

`-DTH16_NULL_RENDERER=ON` builds a Direct3D that draws nothing, for running
without a display. `-DTH16_THCRAP=OFF` leaves out the thcrap support. All
output stays under `build-port/` (ignored by git).

### Running

The executable needs the game's data files, `th16.dat` and `thbgm.dat`,
from your own copy of TH16 v1.00a (`th16.exe` itself is not used):

```
build-port/clang64/th16 ~/"Touhou Project/(TH16) Touhou Tenkuushou ~ Hidden Star in Four Seasons"
```

- Game folder: the argument (or `--game-dir DIR`), else `$TH16_DATA_DIR`,
  else the current directory if it holds `th16.dat`, else the executable's
  directory. The port never writes there.
- Save folder (`th16.cfg`, `scoreth16.dat`, `replay/`, `snapshot/`,
  `log.txt`): `--save-dir DIR`, else `$TH16_SAVE_DIR`, else
  `~/.local/share/th16-port` (`$XDG_DATA_HOME/th16-port`; on macOS
  `~/Library/Application Support/th16-port`). The configuration, score file
  and replays of a game folder are read until the port saves its own.
- The window size and full screen choice come from `th16.cfg`, as on
  Windows; Alt+Enter switches between window and (desktop-sized) full
  screen.
- Without a display, `SDL_VIDEODRIVER=offscreen` (or `xvfb-run`) and
  `SDL_AUDIODRIVER=dummy` run it headless; `port/NOTES.md` ("Platform
  layer", Testing) has a scripted run that reaches stage 1.

### thcrap patches

A build with thcrap support applies the patch stack of an existing thcrap
installation, as thcrap does on Windows: install thcrap (on Windows, or
under Wine), pick the patches with its configuration tool, and the port
reads that folder (only reads it; it does not download or update
patches). With the English patch, the dialogue, endings, menus and other
images, spell card names, the music room, the manual and the game's fonts
are in English.

```
build-port/clang64/th16 --thcrap ~/.local/share/thcrap --thcrap-config en ~/"Touhou Project/..."
```

- thcrap folder: `--thcrap DIR`, else `$TH16_THCRAP_DIR`, else
  `$XDG_DATA_HOME/thcrap` or `~/.local/share/thcrap` if it exists.
- Run configuration (the patch stack): `--thcrap-config NAME` (a file in
  the folder's `config/`, with or without `.js`, or any path), else
  `$TH16_THCRAP_CONFIG`, else the newest run configuration in `config/`.
- `--no-thcrap` (or `TH16_THCRAP=0`) runs the game unpatched. So does a
  stack without patches for th16.
- The log (stderr) lists the stack and every file it patches.

The stack's own switches apply as with thcrap: a run configuration can
turn off a binary hack, for example base_tsa's Stage 5 spell practice fix
(`"binhacks": {"fix_satono_1": {"ignore": true}}`). Not supported: TL
notes (dropped), BGM mods and patch updates; see `port/NOTES.md`,
"thcrap".

### Controls

The game's own: arrow keys (or the numeric keypad) move, Z shoots and
confirms, X bombs and cancels, Shift focuses, C is the season release
(and skips dialogue, as does Ctrl), Escape pauses, P or Home takes a
screenshot (a BMP in `snapshot/`). The first game controller SDL finds is
also read, laid out as DirectInput reports an Xbox pad: the left stick and
the d-pad move, and the buttons follow `th16.cfg`'s mapping (by default
A shoots, B bombs, X focuses, Y releases the season, RB pauses).

### Known gaps

- The audio mixer has only been checked with SDL's dummy and disk drivers
  in tests (bit-exact BGM streaming), not by listening on a real device.
- D3DX math uses the documented formulas, which may differ from the real
  D3DX9 in the last bit; replays recorded on Windows are not checked for
  compatibility.
- The resolution dialog shown at startup on Windows has no UI here: the
  saved configuration is used as it is.
- No exclusive full screen modes (full screen is desktop-sized), no
  high-DPI windows, no controller hot-plugging through DirectInput (only
  through the winmm fallback).
- See `port/NOTES.md` ("Not done", "64-bit problems") for the rest.

### macOS

Not built or run yet. The build files handle macOS's libiconv and SDL's
paths, and the renderer asks for a forward-compatible OpenGL 3.3 core
context, which macOS provides; fonts may need `TH16_FONT_GOTHIC` and
`TH16_FONT_MINCHO`. On Apple Silicon the x87 `fsincos` helpers fall back
to `sinl`/`cosl`, which can differ from the original in the last bit.

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

All of it is downloaded from Microsoft by `scripts/setup.py`. The MSVC
packages are the original VS2017 15.0 payloads, which Microsoft still serves
at the URLs listed in the 15.0 (26228.4) layout catalog. That catalog
survives in an archived 2017 offline layout; only its URLs and hashes are
used, and every file is pinned by SHA-256 in `scripts/toolchain.json`. The
Windows tools run under Wine in a private prefix (`prefix/wine`).

## Setup (Linux)

Requires Wine, `cabextract`, Python 3 and [uv](https://github.com/astral-sh/uv).

```sh
uv venv && uv pip install -r requirements.txt
.venv/bin/python scripts/setup.py --game-dir "/path/to/your/TH16 install"
.venv/bin/python scripts/check_toolchain.py
```

`setup.py` downloads about 800 MB into `prefix/downloads`, unpacks the
toolchain into `prefix/` and copies `th16.exe` into `orig/`. Neither
directory is committed.

## Building and comparing

```sh
.venv/bin/python scripts/build.py                 # src/ -> build/th16.exe + .pdb + .map
.venv/bin/python scripts/compare.py               # per-function match report (reccmp)
.venv/bin/python scripts/compare.py -v 0x401300   # diff one function
.venv/bin/python scripts/quickdiff.py 0x401300    # rough diff in seconds
```

Each function carries its address in the original as a
[reccmp](https://github.com/isledecomp/reccmp) annotation
(`// FUNCTION: TH16 0x401300`), and so does each global the code touches.

## Documentation

- [docs/workflow.md](docs/workflow.md): how the matching build works.
  Covers the tools, annotations, what whole-program optimization means for
  the source, the stand-in callers, data tables and known tooling gaps.
- [docs/findings.md](docs/findings.md): what decides MSVC's output here.
  Source shapes, whole-program effects and the differences still open, as
  found while matching.
- [AGENTS.md](AGENTS.md): the short version for coding agents and
  contributors. Commands, matching gotchas and rules for parallel work.
- [port/NOTES.md](port/NOTES.md): the portable build's design and test
  notes.

## License

The project's own work (the scripts, the portable build's platform layer,
the names, comments and documentation) is dedicated to the public domain
under [CC0 1.0](LICENSE). The game itself, including the data tables and
strings the source reproduces from `th16.exe`, remains the property of its
author, ZUN (Team Shanghai Alice), and `src/DSUtil.cpp` reproduces ZUN's
adaptation of Microsoft's DirectX SDK sample code; CC0 cannot waive rights
the contributors do not hold. The portable build's thcrap support adapts
code from [thcrap](https://github.com/thpatch/thcrap), which is public
domain (Unlicense).

## Credits

Names and structure layouts follow ExpHP's
[th-re-data](https://github.com/exphp-share/th-re-data) where possible. The
[TH06 decomp](https://github.com/happyhavoc/th06) is a useful reference for
how ZUN's engine looked in its first Windows incarnation.
