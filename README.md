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
  gcc, 32- or 64-bit) lives on the `port` branch; see its README and
  `port/NOTES.md`. It needs your own game data files.

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
- `port/NOTES.md` on the `port` branch: the portable build's design and
  test notes.

## License

The project's own work (the scripts, the portable build's platform layer,
the names, comments and documentation) is dedicated to the public domain
under [CC0 1.0](LICENSE). The game itself, including the data tables and
strings the source reproduces from `th16.exe`, remains the property of its
author, ZUN (Team Shanghai Alice), and `src/DSUtil.cpp` reproduces ZUN's
adaptation of Microsoft's DirectX SDK sample code; CC0 cannot waive rights
the contributors do not hold.

## Credits

Names and structure layouts follow ExpHP's
[th-re-data](https://github.com/exphp-share/th-re-data) where possible. The
[TH06 decomp](https://github.com/happyhavoc/th06) is a useful reference for
how ZUN's engine looked in its first Windows incarnation.
