# TinyCC readonly literal collection

This repairs the common compiler/linker mechanism behind the unused XGE readonly
data diagnosed in `xge3-unused-readonly-data.md`. It adds no XGE-specific compiler
rule or API wrapper. Focused CPU qualification includes bounded code/data-owned
literal units. Full O2 CLI, embedded compiler, and matched XGE qualification are
still in progress. No final XGE size saving is claimed by this preflight.

## Representation and linking

`-fdata-sections` gives compiler-created strings and floating constants readonly
collection units. Ordinary literals in one function share its literal unit;
each named static/global initializer has its own unit, scoped independently of
an enclosing function. This follows the existing code/data collection granularity:
retaining an initialized object already follows all its relocation edges.
Literals without a current code/data owner receive an individual unit.

The owner selects storage during compilation; the existing relocation graph
determines retention during linking. There is no reverse edge that keeps a
function alive merely because a literal remains reachable. In particular, a
retained local static initializer does not pin unrelated ordinary literals from
its discarded function. Explicit user section attributes and unevaluated
expressions preserve their existing behavior. Without data sections, direct
compilation keeps the shared literal pool.

An object literal unit is one complete ELF `SHF_MERGE` record. It deliberately omits
`SHF_STRINGS`: a foreign linker must not independently merge substrings separated
by embedded NULs. Record size, type alignment, wide strings, numeric bits, and
interior pointers remain meaningful. Mixed units finish with their required
alignment padding. Objects from different translation units
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

During compilation, code/data literal units have section metadata and names;
individual literal symbols still describe their exact offsets. The parser uses
two private pointers to select/restore the current unit; no public or internal
`TCCState`/`Section` layout is enlarged by owner selection. During final packing,
retained bytes are copied once, one temporary offset
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

The initial immutable evidence index is
`dev/release-materials/provenance/tcc-literal-gc-preflight-20261112.json`.
The successor with code/data-owner units is
`dev/release-materials/provenance/tcc-literal-owner-preflight-20261112.json`.
Raw freezes and logs remain outside Git under
`F:/xlang-validation/xge-full-20261110/`.

- Current Windows/Linux fixtures compiled by GCC at O0 and O2 pass 450 C checks. They
  cover live/dead strings and floats, embedded NULs, wide strings, signed zero,
  interior offsets, explicit 32-byte alignment, repeated object section names,
  JIT execution, final section compaction with 192 live literals, and code/data
  owner separation. A function and an incomplete-array initializer each contain
  35,000 distinct varying literals, retain bounded section counts, and execute
  the expected checksums. Exact literal storage sizes reject duplicate sizing-pass
  output. Both contexts are checked independently.
- All actual compaction allocation prefixes are failed in the existing
  parser-protected allocator path; error delivery, pending cleanup preservation,
  and zero retained allocator blocks after teardown are checked.
- Linux additionally links the real emitted objects with GNU ld and executes
  both O0/O2 clients. Whole-record flags and sizes are independently inspected.
- Existing GC/root/unwind/debug checks pass 532 C checks across both platforms;
  Windows `.pdata` checks pass 274; Linux COPY relocation and section-budget
  checks pass 102. The combined current focused total is 1358 C checks, separate from
  command and external-link checks.

Before controls reproduce three expected literal-retention failures per O0/O2
run on both platforms. A later teardown control reproduces one retained-registry
failure; it passes after the cleanup correction. An earlier fixture inspected
metadata after public relocation had intentionally released it; that harness was
corrected to inspect before relocation and then execute the public JIT path.
Those failed control/harness logs are preserved and do not count as acceptance.

The first complete Windows CLI built successfully and passed matrix JIT/dynamic
execution, but its static link correctly refused an unrepresentable directory:
39,238 existing sections plus a 32,962-section input budget. The real XRT object
had expanded from 20,715 to 32,963 headers when every literal received a section.
This is a product integration limit of that candidate, not a test harness failure.
Code/data-owner units reduce that same real native-object preflight to 24,141
headers. The unchanged hard section-index guard remains in force. A separate
82-check O0/O2 budget gate admits this actual object on top of 25,000 sections.
The object preflight does not substitute for pending full static CLI/embedded tests.

Run the narrow gate with a new output directory:

```text
python -B tools/check_tcc_literal_gc.py --root <repository> --out <new-external-directory>
```

Use `python -X utf8 -B` on Windows and the native GCC toolchain on each platform.
The fixture includes frozen production libtcc; it is not a replacement for full
compiler and nested-compiler delivery tests. Emitted PE files are inspected, not
executed by this gate. Graphics, network, and whole-engine performance are outside
its scope.
