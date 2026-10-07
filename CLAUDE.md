# th16-decomp

Matching decomp of TH16 1.00a. See README.md for the toolchain evidence and workflow.

## Commands

- Build: `.venv/bin/python scripts/build.py` (about 3 s; Wine runs MSVC 19.10.25017)
- Compare: `.venv/bin/python scripts/compare.py`, one function: `-v 0x<orig addr>`
- Fast iteration: `.venv/bin/python scripts/quickdiff.py 0x<orig addr>` (seconds; globals
  compare as symbol+offset where the original address is known, other addresses
  count as equal, so confirm with compare.py)
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
- Args in registers (ecx/edx, floats in xmm, `ebx`, ...): LTCG custom
  convention, which only happens when LTCG sees every caller. Mark the
  definition HARNESS_CALLED and call it from src/harness/ (see ZunAngle).
- A class with a vtable must use the RTTI name from the binary.
- Redundant stores kept, or flag updates not merged: something blocks MSVC's
  dead store elimination. Plain-int flags with `&=`/`|=` (not bitfields),
  then `volatile` (see create_func). Exception: field assignments that
  compile to xor/and/xor are real bitfields (EnemyFlagsLow). Check every function that inlines the
  same struct code before settling on a struct-level change.
- Our build inlines a callee the original calls: mark it `DECOMP_NOINLINE`.
- Callee not decompiled yet: placeholder in `src/stub/` (built without /GL,
  so LTCG treats it as opaque). Function shaped by an undecompiled caller:
  recreate the call site in `src/harness/`.
- Register allocation mismatch with identical instructions: reorder loads,
  swap loop forms (for/while/goto), split or merge variables. The TH06
  decomp (happyhavoc/th06) shows ZUN's habits, including switch case order.
- Never pipe cl/link output from Python: `cl /Zi` leaves `mspdbsrv.exe`
  running, which holds the pipe open. `toolchain.run` writes to a file.
- reccmp must run through `scripts/compare.py` so its `cvdump.exe` uses the
  project Wine prefix instead of `~/.wine`.
- Never commit anything from `orig/`, `prefix/` or `build/`.

## Working in parallel (agent worktrees)

Several agents may work at once, each in its own git worktree and branch,
each owning one address range. In a fresh worktree run
`python3 scripts/worktree_setup.py` first (links prefix/, orig/, .venv/),
then build. List your functions with
`TH_RE_DATA=~/.cache/claude-builds/th16-ref/th-re-data .venv/bin/python scripts/list_functions.py <lo> <hi>`.

To keep branches mergeable:
- New code goes in files named after the class or module (`src/Bullet.cpp`).
- Placeholders for callees from other ranges go in `src/stub/<unit>.cpp`
  (opaque) or `src/placeholder/<unit>.cpp` (visible to LTCG), stand-in
  callers in `src/harness/<unit>.cpp` (one file per unit).
- Before merging, check for clashes: every `// (FUNCTION|STUB|GLOBAL|
  SYNTHETIC|VTABLE): TH16 0x...` address must appear once across src/.
- Shared headers (Supervisor.h, CriticalSections.h, decomp.h, types.h, ...):
  only add. Never reorder, rename or delete existing items; to add a struct
  field, split the padding array around it in place.
- Do not change scripts/ or build flags; describe tooling problems instead.
- A function that will not match after a reasonable number of attempts
  stays in (functionally correct, still annotated) with a one-line
  `// TODO:` comment saying what differs; move on.
- Commit often. No em dashes in comments or commit messages.
