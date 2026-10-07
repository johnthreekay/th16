# Workflow

How the matching build works and the rules for working on it. Setup is in
the [README](../README.md); what decides code generation, function by
function, is in [findings.md](findings.md).

## Building and comparing

```sh
.venv/bin/python scripts/build.py      # src/ -> build/th16.exe + .pdb + .map
.venv/bin/python scripts/compare.py    # per-function match report (reccmp)
.venv/bin/python scripts/compare.py -v 0x401300   # diff one function
.venv/bin/python scripts/quickdiff.py 0x401300    # rough diff in seconds
.venv/bin/python scripts/check_data.py            # globals against the original
.venv/bin/python scripts/check_toolchain.py       # toolchain sanity check
```

`quickdiff.py` skips reccmp's PDB parsing, which takes about 25 seconds under
Wine, so it is the tool for trying source variants quickly. Data addresses
compare as symbol+offset when the original's address of the symbol is known
(`// GLOBAL:` annotations, `build/lib.csv`), so wrong offsets into global
objects show up; unknown data addresses and call targets still count as
equal, so confirm with `compare.py`. Large structs should carry
`static_assert(offsetof(...))` checks for their known offsets.

Edits meant to leave code alone (renames, comments, enums) are checked
against a baseline of every function's quickdiff result:

```sh
.venv/bin/python scripts/check_unchanged.py --save   # after a build, before the edit
.venv/bin/python scripts/check_unchanged.py          # after the edit and a rebuild
```

Its section comparison is only informative: `.rdata` differs after every
relink, and a change to `.rdata`'s size shows the five atexit thunks as
changed (see "Known tooling gaps").

## Annotations

Each decompiled function carries an annotation with its address in the
original, in [reccmp](https://github.com/isledecomp/reccmp)'s format:

```cpp
// FUNCTION: TH16 0x401300
int UpdateFuncRegistry::register_on_tick(UpdateFunc *f, int priority)
```

Globals referenced by decompiled code get `// GLOBAL: TH16 0x...` so the
comparison can line up their addresses. Compiler-generated functions use
`// SYNTHETIC: TH16 0x...` with the name on the following comment line, e.g.
``// UpdateFuncRegistry::`scalar deleting destructor'``.

### Compiler-generated and CRT functions

Name-based annotations: the marker, then a comment line naming the function.

- Scalar deleting destructors: `// SYNTHETIC: TH16 0x...` then
  ``// X::`scalar deleting destructor'``.
- Dynamic initializers (0x401000-0x401250 in the original) and the atexit
  destructors they register (0x48ac10-0x48acd0): name them by symbol, e.g.
  `// SYNTHETIC: TH16 0x401110` then `// ??__Eg_arcfiles@@YAXXZ`, and
  `// ??__Fg_arcfiles@@YAXXZ` for the destructor (see Arcfile.cpp). The PDB
  knows these static functions only by that string; compare.py treats it as
  their symbol. Matching them means defining the global, with its
  constructor and destructor, the way ZUN did.
- Implicit constructors/destructors: `// X::X` or `// X::~X`, or the symbol.
- UCRT functions defined inline in the headers with C linkage:
  `// LIBRARY: TH16 0x4090d0 SYMBOL` then `// _sprintf` (src/CrtInline.cpp).
- The code after the CRT up to 0x48a3c0 and the small functions there are
  exception unwind funclets generated from the functions they belong to;
  they need no source of their own.

## Whole-program optimization

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
  it compare by name. Large functions are found by masked byte patterns;
  small ones by unique matches inside the library's address range, by which
  function their calls lead to, or through the calls of functions already
  found. Globals they reference (`__security_cookie`) are named the same way.
- `scripts/compare.py` runs reccmp with two fixes for VS2017 binaries: it
  accepts the newer C++ EH FuncInfo magic (`0x19930522`), and it moves each
  EH handler thunk's start back over the `/GS` cookie check that precedes
  `mov eax, FuncInfo; jmp __CxxFrameHandler3`. Without them every function
  with a C++ EH frame shows a spurious difference.

## Placeholders and stand-in callers

While the decompilation was partial, three directories held code that is
not ZUN's, to give decompiled functions the surroundings they had in the
original: `src/stub/` (placeholder bodies compiled **without** `/GL`, so
calls to them stayed opaque), `src/placeholder/` (`/GL` stand-ins for
callees whose shape LTCG had to see) and `src/harness/` (stand-in callers).
With every function decompiled, what is left is:

- `src/stub/Opaque.cpp`, compiled **without** `/GL`: globals LTCG must
  not see (`g_zero_vec2`, which it would fold into constant zeros, and
  `g_stage_table`, whose contents change code elsewhere; see "Data tables")
  and two sinks the harness uses to make an address escape or a frame
  8-byte aligned.
- `src/harness/`, compiled **with** `/GL`: stand-in callers for functions
  whose shape still depends on calls our build does not reproduce (constant
  arguments LTCG must not fold, globals whose address the original takes
  elsewhere, frames the original keeps aligned). Each one says which
  original call site it stands for.
- `DECOMP_NOINLINE` (`src/decomp.h`) marks functions the original keeps out of
  line where ours would inline them.
- `HARNESS_CALLED` marks functions kept alive by their callers (real or
  harness) instead of `/INCLUDE`. That lets LTCG see every caller and pick
  the same custom calling convention it did in the original, including
  conventions no keyword can request (`this` in `ecx` with a float in
  `xmm1`). Prefer it over spelling out `LTCG_FASTCALL`/`LTCG_VECTORCALL`,
  which only cover the simpler cases.

## Data tables

Every annotated global holds the original's initial contents, so the
matching build is a complete program. `scripts/check_data.py` checks it:

```sh
.venv/bin/python scripts/check_data.py         # globals that differ
.venv/bin/python scripts/check_data.py -v      # every global
.venv/bin/python scripts/check_data.py g_foo   # one global, all details
```

For each `// GLOBAL:` it compares our bytes (address from build/th16.map,
size from the PDB) with the original's at the annotated address. Differing
dwords still count as equal when both are pointers to corresponding things:
functions through build/functions.txt, annotated globals and vtables (also
found through RTTI names), build/lib.csv symbols, string literals with the
same text, and unnamed library data with equal contents (dinput8's object
format arrays). Anything else is reported. The one global that still
differs is `g_Supervisor`: our build folds the Config constructor's stores
into static data, which the dynamic initializer's memset then clears (see
the TODO there); the original's is all zero.

It also checks that no global, at its size in our PDB, runs past the next
annotated address in the original. Equal bytes do not show that: the
window fields once annotated as separate globals, and two tables with an
extra trailing NULL that was the next table's first entry, all compared
equal. Such a global is really part of another object; a value written
through one name is lost to code reading the other.

Write tables as readable initializers: functions by name, strings as
literals (`scripts/cstring.py` gives byte-exact Shift-JIS escapes), enums by
name. Tables in `.rdata` in the original are `const` (the ANM callback
tables, `g_spell_difficulty`); `.data` ones are not. Giving a table its
contents can change code elsewhere even when nothing reads it differently:
with `g_stage_table` filled in in /GL code, LTCG ordered the operands of
two AnmVm::write_sprite_corners multiplications differently, so it lives
in Opaque.cpp. Check with check_unchanged.py after each table.

## Known tooling gaps

- Template members cannot be annotated: build.py's name parsing does not
  handle `Interp<Float3>`-style names. Static functions defined in a .cpp
  can be (build.py falls back to the static symbol of that object file), but
  static functions defined in a header, such as the per-file `sincosmul`
  copies, cannot, and neither can anything annotated inside a header: move
  an out-of-line copy into a .cpp instead.
- The `// GLOBAL:` parser does not take a constructor call with
  arguments (`ScreenEffect g_screen_effect(...);`); write it as copy
  initialization (`= ScreenEffect(...)`), which compiles the same.
- build.py's incremental build does not record headers that files in
  src/harness/ (and the other subdirectories) include as `"../X.h"`: only
  headers reached through other headers end up in the `.dep` list. After
  changing such a header, delete `build/obj/src/harness/*.dep` and
  `build/sym/src/harness/*.dep` (or all `.dep` files), or the link can
  fail on a stale object or quietly use one.
- Comments must go above `// FUNCTION:`, not between it and the signature:
  reccmp then loses the function and build.py may misread the declaration.
  The same holds for `// GLOBAL:`: with a comment line in between, reccmp
  names the variable after the comment and every use shows as a difference.
- quickdiff misreports jump thunks and tail jumps; check those with
  compare.py. It reads on until a `ret`, so for the atexit destructors at
  the end of our .text (0x48ac40 to 0x48acd0) it disassembles the start of
  .rdata, the import address table, whose entries point into .rdata: any
  change to .rdata's size shows those five as changed in
  check_unchanged.py although their code is the same.
- Overloads are told apart by the object file that defines them and then
  by parameter types (typedefs mapped through `TYPEDEFS` in build.py; add
  new ones there if an overload is reported as ambiguous).
- build.py reads `template <> __declspec(noinline) X::f` as a function
  named `__declspec`; use DECOMP_NOINLINE. An explicit specialization of an
  in-class template member also needs a user in its own .cpp to be emitted.
- `// FUNCTION:` only finds C++ symbols; WinMain (C linkage) is annotated
  by its linker symbol: `// SYNTHETIC: TH16 0x459830 SYMBOL`, then
  `// _WinMain@16`.
