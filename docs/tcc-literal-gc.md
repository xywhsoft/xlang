# TinyCC readonly literal collection

This repairs the common compiler/linker mechanism behind the unused XGE readonly
data diagnosed in `xge3-unused-readonly-data.md`. It adds no XGE-specific compiler
rule or API wrapper. The focused CPU qualification is complete; full O2 CLI,
embedded compiler, and matched XGE qualification are still in progress. No final
XGE size saving is claimed by this preflight.

## Representation and linking

`-fdata-sections` gives each compiler-created string or floating constant an
independent readonly section. Explicit user section attributes and unevaluated
expressions preserve their existing behavior. Without data sections, direct
compilation keeps the shared literal pool.

An object literal is one complete ELF `SHF_MERGE` record. It deliberately omits
`SHF_STRINGS`: a foreign linker must not independently merge substrings separated
by embedded NULs. Record size, type alignment, wide strings, numeric bits, and
interior pointers remain meaningful. Objects from different translation units
stay independent even when their internal literal section ordinals coincide and
the final consumer did not request data sections.

The existing relocation graph determines reachability. After collection, final
JIT/PE/ELF output packs surviving eligible literal records into the common
readonly pool. All main-table symbols in moved sections receive the aligned
pool offset and new section index; symbol indices and relocation addends stay
unchanged. This bounds final section-header overhead. Relocatable object output
is neither collected nor packed. Records with outgoing relocations or additional
section flags are left alone.

Packing also applies without GC when consuming already split literal objects;
in that mode it retains every literal. Explicit sections keep their own identity
and alignment. These changes do not add constant pooling to unrelated inputs or
alter foreign dynamic-symbol section identities.

## Costs and cleanup

During compilation, each independent literal has section/symbol metadata and a
name. During final packing, retained bytes are copied once, one temporary offset
and movement ledger is allocated lazily, and one main-symbol-table pass adjusts
locations. The existing cleanup registry protects that ledger when used within
a parser-protected error boundary. An otherwise empty registry is released after
packing, because a final link may never be followed by another parse cycle.
A caller's pending cleanup entries are preserved.

The ordinary unsplit path creates no ledger. No runtime API dispatch, ownership
record, language allocation, or scan is added to the generated program. Dead
source is still parsed/compiled and input section buffers remain owned by the
compiler until normal teardown; lower peak compiler memory is not promised.
This does not change TinyCC's default process-fatal allocation policy or promise
that a partially failed final link can be resumed in place.

## Qualification

The small immutable evidence index is
`dev/release-materials/provenance/tcc-literal-gc-preflight-20261112.json`.
Raw freezes and logs remain outside Git under
`F:/xlang-validation/xge-full-20261110/`.

- Windows/Linux fixtures compiled by GCC at O0 and O2 pass 366 C checks. They
  cover live/dead strings and floats, embedded NULs, wide strings, signed zero,
  interior offsets, explicit 32-byte alignment, repeated object section names,
  JIT execution, and final section compaction with 192 live literals.
- All actual compaction allocation prefixes are failed in the existing
  parser-protected allocator path; error delivery, pending cleanup preservation,
  and zero retained allocator blocks after teardown are checked.
- Linux additionally links the real emitted objects with GNU ld and executes
  both O0/O2 clients. Whole-record flags and sizes are independently inspected.
- Existing GC/root/unwind/debug checks pass 532 C checks across both platforms;
  Windows `.pdata` checks pass 274; Linux COPY relocation and section-budget
  checks pass 102. The combined focused total is 1274 C checks, separate from
  command and external-link checks.

Before controls reproduce three expected literal-retention failures per O0/O2
run on both platforms. A later teardown control reproduces one retained-registry
failure; it passes after the cleanup correction. An earlier fixture inspected
metadata after public relocation had intentionally released it; that harness was
corrected to inspect before relocation and then execute the public JIT path.
Those failed control/harness logs are preserved and do not count as acceptance.

Run the narrow gate with a new output directory:

```text
python -B tools/check_tcc_literal_gc.py --root <repository> --out <new-external-directory>
```

Use `python -X utf8 -B` on Windows and the native GCC toolchain on each platform.
The fixture includes frozen production libtcc; it is not a replacement for full
compiler and nested-compiler delivery tests. Emitted PE files are inspected, not
executed by this gate. Graphics, network, and whole-engine performance are outside
its scope.
