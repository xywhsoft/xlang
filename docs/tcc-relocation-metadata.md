# Relocation metadata and the numbered section directory

Status: implemented and focused-qualified on x86-64 Windows/Linux. Complete
compiler rebuilds and the actual nested-language-compiler caller remain pending.
Keep the current hard index checks and the failing r25 acceptance evidence.

## Required behavior

Ordinary, nonallocated relocation records that no symbol addresses should be
link metadata attached to their target section. A final-link state need not
assign them a code/data symbol section index. TCC already has a private-section
container and target `Section.reloc` links; use those existing ownership paths.

This preserves the existing code/data/literal collection precision, constant
values, relocation addends, symbol identity, explicit exports, debug information,
unwind tables, and actual output formats. It must not merge unrelated code/data,
drop a compiler embedding mode, raise the limit without index support, or hide
overflow by truncation.

Relocatable object producers keep ordinary relocation sections numbered for
serialization. Allocated dynamic relocation tables stay numbered. Imported
metadata with a defining symbol must also stay numbered, so aliases and section
symbols retain their identities. Other targets initially keep their existing
representation; the first qualified scope is x86-64 Windows and Linux, excluding
Mach-O.

## Implementation audit

1. Introduce a single internal iterator/accessor that yields existing numbered
   relocation sections and a numbered target's private relocation records exactly
   once. Do not create a duplicate metadata owner or scan unrelated private hashes.
2. Source emission may place eligible final-link relocation records privately;
   OBJ output keeps its wire representation. Preserve append offsets between
   compilation units and all local/global symbol-index rewrites.
3. Before publishing imported sections, validate names/targets, read symbol
   definitions, and mark any relocation section addressed by a symbol. Compute
   the indexed-directory budget using that classification, including packed PE
   `.pdata` rules. A genuine overflow must fail before publishing contributions.
4. Keep private relocation lookup/merging correct for repeated common input
   sections and for independent function/data contributions. Preserve the target
   backward link and target index; never map a metadata definition to UNDEF.
5. Audit symbol sorting, GOT/PLT generation/fill, relocation execution, GC/archive
   queries, and debug/unwind handling. Existing target-based GC traversal already
   follows `Section.reloc`; private metadata is not an independently live payload.
6. ELF dynamic output can turn ordinary records into allocated runtime relocation
   tables. Materialize those required tables in the numbered directory at a
   checked boundary before layout/serialization. Handle failure normally and do
   not change iterator membership during the pass that classifies them.
7. When output sorting renumbers targets, update private relocation target
   indices as well. PE output must still emit/register real sorted unwind data
   and image fixups. Both public and private containers must free their own
   records once, including failed loads and output failures.

The existing ELF 16-bit index representation remains a genuine bound for
addressable/serialized indexed sections. This work is not a claim of general
`SHN_XINDEX` support. It removes unneeded final-link metadata numbering while
preserving the true bound.

## Qualification required before integration

- Before/fixed controls with actual large objects and the captured nested parent
  manifest; count indexed payload separately from ordinary relocation metadata.
- Source and imported-object JIT, PE/ELF EXE and shared output, actual GNU-linked
  OBJ clients, repeated contributions, and referenced-metadata preservation.
- Existing root/GC, CIE/FDE, PE `.pdata`, COPY/GOT/PLT, COFF/TLS, debug-line, and
  input-budget tests. Update budget expectations to the real representation;
  retain a genuinely overflowing input and unchanged hard-limit tests.
- Rejected input/output and teardown checks; a normal malformed/oversized CPU
  object is sufficient, without network/security testing.
- Complete O2 compiler/TCC/XRT/VFS rebuilds, matrix/image and native/nested compiler
  EXE/shared acceptance on both platforms, then matched size measurements.

Record cold compile/link allocations, scans, metadata storage, and any changed
output bytes. Runtime GUI/drawing calls must not acquire a new wrapper or
bookkeeping path from this internal compiler correction.

## Focused implementation qualification, 2026-11-12

Ordinary final-link metadata now uses the existing private container. The shared
accessor covers symbol rewrites, relocation execution, GOT creation and fill.
ELF output classifies runtime tables, checks the required directory capacity,
then transfers each required table's sole ownership to the numbered container.
Output sorting also parks discarded public sections in the teardown container;
those records are deliberately excluded from private metadata lookup/promotion.
No `TCCState` or `Section` fields were added. The input classification byte fits
existing `SectionMergeInfo` padding: Windows remains 16 bytes and Linux 24 bytes.

The focused O0/O2 results, frozen under
`F:/xlang-validation/xge-full-20261110/`, include:

- Windows/Linux source and object JIT, repeated common contributions, genuine
  GNU object clients, metadata addressed by a symbol in a later input, allocated
  and nonordinary metadata flags, exact contribution offsets, checked promotion
  budget refusal, and real OS-loader shared-library execution.
- Full literal/GC/debug/unwind, PE record-level exception, ELF COPY, indexed
  budget and unchanged hard-limit regression gates.
- Real GCC COFF archives, native callback/aggregate ABI and COMDAT/lifecycle,
  emitted PE execution, IAT ordering, static TLS process/thread callbacks, Linux
  PIC/GOT execution with a genuine conflicting GOT definition still refused,
  and non-mutating archive selection.

The captured Windows population preflight admits 1423 unique real object files,
including the 9674496-byte XRT object, with zero linker errors: 43299 numbered
sections and 29030 private relocation tables. The before provider still refuses
the same unique population at its real index guard. All object hashes are pinned.
This is **not** a replay of the full language caller: its 1431 captured object
calls include eight repeated objects. Bare replay of those repeated calls emitted
duplicate-definition diagnostics and is retained as a failed harness experiment.
The qualifier reports the eight removed repetitions explicitly. It checks both
the load return code and the internal error count; return code alone is inadequate.
The original complete parent/child acceptance remains mandatory.

The small immutable index is
`dev/release-materials/provenance/tcc-relocation-metadata-preflight-20261112.json`.
Large raw logs and frozen work copies stay external. The report does not qualify
the complete compiler, final SDK/IDE installation, or the whole XGE integration.

Cold linking adds a symbol-definition classification pass before the input
budget check and private-metadata name lookups for common-section merging.
Promotions use existing vectors and a checked ownership transfer; the table
payloads still occupy memory. No cold CPU/peak-memory improvement is claimed.
Runtime drawing/resource calls gain no wrapper from this internal linker change.

## Actual caller errors found after the metadata preflight

A subsequent debugger trace of the real r25 compiler confirms that the eight
repeated object calls also accumulate 18 duplicate-definition errors in its
actual state. The early raw-replay diagnostics therefore exposed a second real
caller defect; they were not merely an artifact of the harness. The original
summary's unique-population preflight remains correctly limited to that scope.
Only the last index-limit message had been visible to the user.

Eight legacy entries in `xlAddEmbeddedXlangRuntimeSources.runtimeSources` now
come exclusively from the canonical resident catalog. The precompiler checks
host-entry and resident source rosters for overlap, including duplicate raw
component rows before producer deduplication can hide them. It rejects the real
old eight-source overlap and five duplicated-layout controls.

`tcc_add_binary` also records the incoming error count. A new non-aborting loader
diagnostic now makes a nominally successful object/archive/library load return
failure. An earlier diagnosed error does not poison a later successful load;
the existing error recovery and public ABI remain intact. O0/O2 tests reproduce
the old incorrect success return on both platforms, require the new failure
return through all three public loading paths, and run a fresh valid JIT object.
They add no production allocation, copying, or runtime drawing work.

Both complete r26/r18 build attempts were superseded before qualification once
this real caller defect was confirmed. Their aborted reports and prior results
remain intact. The successor must rebuild the loader, host API, fixed objects,
VFS and embedded compiler together. Focused evidence is indexed in
`dev/release-materials/provenance/tcc-embedded-object-errors-20261112.json`;
complete nested caller qualification is still pending.

## Complete Windows combined cohort

The r27 O2 build now rebuilds TinyCC, XRT, fixed objects, both VFS catalogs,
the compiler/runtime DLLs and CLI together. Its 13495 frozen input files include
the private-metadata implementation, binary error propagation and corrected host
source rosters. The four complete Windows gates pass: matrix, image, literal
children through CLI/EXE/shared embedded TCC, and actual source-free parent/child
language compiler EXE/shared execution. The last case had failed in r25 and is
now executed successfully rather than replaced by a unique-object preflight.

With the same image SDK and matrix-only source, r25 and r27 produce identical
file and machine-code sizes. This internal representation/correctness work does
not add consumer code to that sample. The build and measured producer sizes are
pinned in `dev/release-materials/provenance/tcc-relocation-windows-20261112.json`.
Linux r19 combined qualification and the full XGE/XUI/Document delivery remain
open; this Windows cohort is not the final SDK/IDE installation.

## Complete Linux combined cohort

The successor Linux r19 O2 build and all four combined gates now pass too,
including the real source-free parent/child language compiler in executable and
shared-library callers. With the same image SDK and matrix-only source, its
consumer file, code and allocated read-only data sizes match r17 exactly.
Producer sizes are 47,224 bytes for `xl`, 16,484,880 for `libxl.so` and
22,489,904 for `libxlc.so`. Precompiled build inputs were preserved outside the
temporary Linux build directory before qualification. The immutable successor
index is `dev/release-materials/provenance/tcc-relocation-linux-20261112.json`;
the earlier Windows report retains its historical Linux-pending statement.
Both combined compiler cohorts are qualified for these gates. Full ecosystem
coverage, pending ownership contracts and final matched SDK/IDE delivery remain
open; neither cohort has been installed into the normal release runtime.
