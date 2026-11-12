# Relocation metadata and the numbered section directory

Status: implementation plan based on the actual r25 nested-compiler failure.
The source has not yet gained this representation. Keep the current hard index
checks and the failing full-language-compiler acceptance case.

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
