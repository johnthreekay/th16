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
| Linker | 14.10.25017, full `/LTCG`, no `/DEBUG` | POGO debug entry with `LTCG` signature; no ILTCG, CodeView or VC_FEATURE entries |
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
```

Each decompiled function carries an annotation with its address in the
original, in [reccmp](https://github.com/isledecomp/reccmp)'s format:

```cpp
// FUNCTION: TH16 0x401300
int UpdateFuncRegistry::register_on_tick(UpdateFunc *f, int priority)
```

Globals referenced by decompiled code get `// GLOBAL: TH16 0x...` so the
comparison can line up their addresses.

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
  it compare by name.

### Things learned so far

- `UpdateFunc`'s flag bits are `volatile`. Without it MSVC merges the
  constructor's stores with later ones; the original keeps every store
  around the bit updates, as MSVC does for volatile accesses.

## Credits

Names and structure layouts follow ExpHP's
[th-re-data](https://github.com/exphp-share/th-re-data) where possible. The
[TH06 decomp](https://github.com/happyhavoc/th06) is a useful reference for
how ZUN's engine looked in its first Windows incarnation.
