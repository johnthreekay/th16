# th16-decomp

Matching decomp of TH16 1.00a. See README.md for the toolchain evidence and workflow.

## Commands

- Build: `.venv/bin/python scripts/build.py` (about 3 s; Wine runs MSVC 19.10.25017)
- Compare: `.venv/bin/python scripts/compare.py`, one function: `-v 0x<orig addr>`
- Toolchain sanity check: `.venv/bin/python scripts/check_toolchain.py`

## Decompiling a function

1. Read the original: `orig/th16.exe` (Ghidra, or `objdump -d -M intel`).
   Names and struct layouts: ExpHP's th-re-data (`data/th16.v1.00a/*.json`).
2. Write C++ in `src/`, preceded by `// FUNCTION: TH16 0x<addr>`. Globals it
   touches get `// GLOBAL: TH16 0x<addr>` on their definition.
3. Build and compare until reccmp reports 100%.

## Gotchas

- Callee pops stack args (`ret N`) but callers never set `ecx`: write it as a
  member function that ignores `this` (LTCG removes it). A `static` member
  gives `ret`, and `__stdcall`/cdecl statics get rewritten to register args.
- Args in odd registers (`ebx`, ...): LTCG custom convention; match together
  with the callers, since `/INCLUDE` keeps the standard convention.
- Redundant stores kept around bitfield updates point to `volatile` fields.
- Never pipe cl/link output from Python: `cl /Zi` leaves `mspdbsrv.exe`
  running, which holds the pipe open. `toolchain.run` writes to a file.
- reccmp must run through `scripts/compare.py` so its `cvdump.exe` uses the
  project Wine prefix instead of `~/.wine`.
- Never commit anything from `orig/`, `prefix/` or `build/`.
